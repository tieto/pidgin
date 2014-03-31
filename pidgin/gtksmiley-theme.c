/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "gtksmiley-theme.h"

#include "internal.h"
#include "glibcompat.h"

#include "debug.h"

#include <glib/gstdio.h>

#define PIDGIN_SMILEY_THEME_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_SMILEY_THEME, \
	PidginSmileyThemePrivate))

#define PIDGIN_SMILEY_THEME_MAX_LINES 1024
#define PIDGIN_SMILEY_THEME_MAX_TOKENS 1024

typedef struct
{
	gchar *path;

	gchar *name;
	gchar *desc;
	gchar *icon;
	gchar *author;
} PidginSmileyThemePrivate;

static GObjectClass *parent_class;

static gchar **probe_dirs;
static GList *smiley_themes = NULL;

typedef struct
{
	gchar *name;
	gchar *desc;
	gchar *icon;
	gchar *author;

	GList *protocols;
} PidginSmileyThemeIndex;

typedef struct
{
	gchar *name;
	GList *smileys;
} PidginSmileyThemeIndexProtocol;

typedef struct
{
	gchar *file;
	gboolean hidden;
	GList *shortcuts;
} PidginSmileyThemeIndexSmiley;

/*******************************************************************************
 * Theme index parsing
 ******************************************************************************/

static void
pidgin_smiley_theme_index_free(PidginSmileyThemeIndex *index)
{
	GList *it, *it2;

	g_return_if_fail(index != NULL);

	g_free(index->name);
	g_free(index->desc);
	g_free(index->icon);
	g_free(index->author);

	for (it = index->protocols; it; it = g_list_next(it)) {
		PidginSmileyThemeIndexProtocol *proto = it->data;

		g_free(proto->name);
		for (it2 = proto->smileys; it2; it2 = g_list_next(it2)) {
			PidginSmileyThemeIndexSmiley *smiley = it2->data;

			g_free(smiley->file);
			g_list_free_full(smiley->shortcuts, g_free);
			g_free(smiley);
		}
		g_list_free(proto->smileys);
		g_free(proto);
	}
	g_list_free(index->protocols);
}

static PidginSmileyThemeIndex *
pidgin_smiley_theme_index_parse(const gchar *index_path, gboolean load_contents)
{
	PidginSmileyThemeIndex *index;
	PidginSmileyThemeIndexProtocol *proto = NULL;
	FILE *file;
	int line_no = 0;
	gboolean inv_frm = FALSE;

	file = g_fopen(index_path, "r");
	if (!file) {
		purple_debug_error("gtksmiley-theme",
			"Failed to open index file %s", index_path);
		return NULL;
	}

	index = g_new0(PidginSmileyThemeIndex, 1);

	while (!feof(file)) {
		PidginSmileyThemeIndexSmiley *smiley;
		gchar buff[1024];
		gchar *line, *eqchr;
		gchar **split;
		int i;

		if (++line_no > PIDGIN_SMILEY_THEME_MAX_LINES) {
			purple_debug_warning("gtksmiley-theme", "file too big");
			break;
		}

		if (!fgets(buff, sizeof(buff), file))
			break;

		/* strip comments */
		if (buff[0] == '#')
			continue;

		g_strstrip(buff);
		if (buff[0] == '\0')
			continue;

		if (!g_utf8_validate(buff, -1, NULL)) {
			purple_debug_error("gtksmiley-theme", "%s:%"
				G_GSIZE_FORMAT " is invalid UTF-8",
				index_path, line_no);
			continue;
		}

		line = buff;

		if (line[0] == '[') {
			gchar *end;

			if (!load_contents)
				break;
			line++;
			end = strchr(line, ']');
			if (!end) {
				inv_frm = TRUE;
				break;
			}

			proto = g_new0(PidginSmileyThemeIndexProtocol, 1);
			proto->name = g_strndup(line, end - line);

			index->protocols =
				g_list_prepend(index->protocols, proto);

			continue;
		}

		if ((eqchr = strchr(line, '='))) {
			*eqchr = '\0';
			if (g_ascii_strcasecmp(line, "name") == 0) {
				g_free(index->name);
				index->name = g_strdup(eqchr + 1);
				g_strchug(index->name);
				continue;
			} else if (g_ascii_strcasecmp(line, "description") == 0) {
				g_free(index->desc);
				index->desc = g_strdup(eqchr + 1);
				g_strchug(index->desc);
				continue;
			} else if (g_ascii_strcasecmp(line, "icon") == 0) {
				g_free(index->icon);
				index->icon = g_strdup(eqchr + 1);
				g_strchug(index->icon);
				continue;
			} else if (g_ascii_strcasecmp(line, "author") == 0) {
				g_free(index->author);
				index->author = g_strdup(eqchr + 1);
				g_strchug(index->author);
				continue;
			}
			*eqchr = '=';
		}

		/* parsing section content */

		if (proto == NULL) {
			inv_frm = FALSE;
			break;
		}

		smiley = g_new0(PidginSmileyThemeIndexSmiley, 1);
		proto->smileys = g_list_prepend(proto->smileys, smiley);

		smiley->hidden = FALSE;
		if (line[0] == '!') {
			smiley->hidden = TRUE;
			line++;
		}

		split = g_strsplit_set(line, " \t",
			PIDGIN_SMILEY_THEME_MAX_TOKENS);
		for (i = 0; split[i]; i++) {
			gchar *token = split[i];

			if (token[0] == '\0')
				continue;
			if (i == PIDGIN_SMILEY_THEME_MAX_TOKENS - 1)
				break;

			if (!smiley->file) {
				smiley->file = g_strdup(token);
				continue;
			}

			smiley->shortcuts = g_list_prepend(smiley->shortcuts,
				g_strdup(token));
		}
		g_strfreev(split);
	}

	fclose(file);

	if (inv_frm) {
		purple_debug_error("gtksmiley-theme", "%s:%" G_GSIZE_FORMAT
			" invalid format", index_path, line_no);
		pidgin_smiley_theme_index_free(index);
		index = NULL;
	}

	return index;
}

/*******************************************************************************
 * Theme loading
 ******************************************************************************/

static void
pidgin_smiley_theme_load(const gchar *theme_path)
{
	PidginSmileyTheme *theme;
	PidginSmileyThemePrivate *priv;
	PidginSmileyThemeIndex *index;
	GList *it;
	gchar *index_path;

	/* it's not super-efficient, but we don't expect huge amount of
	 * installed themes */
	for (it = smiley_themes; it; it = g_list_next(it)) {
		PidginSmileyThemePrivate *priv =
			PIDGIN_SMILEY_THEME_GET_PRIVATE(it->data);

		/* theme is already loaded */
		if (g_strcmp0(priv->path, theme_path) == 0)
			return;
	}

	index_path = g_build_filename(theme_path, "theme", NULL);

	theme = g_object_new(PIDGIN_TYPE_SMILEY_THEME, NULL);
	priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	priv->path = g_strdup(theme_path);

	index = pidgin_smiley_theme_index_parse(index_path, FALSE);

	g_free(index_path);

	if (!index->name || index->name[0] == '\0') {
		purple_debug_warning("gtksmiley-theme",
			"incomplete theme %s", theme_path);
		pidgin_smiley_theme_index_free(index);
		g_object_unref(theme);
		return;
	}

	priv->name = g_strdup(index->name);
	if (index->desc && index->desc[0])
		priv->desc = g_strdup(index->desc);
	if (index->icon && index->icon[0])
		priv->icon = g_strdup(index->icon);
	if (index->author && index->author[0])
		priv->author = g_strdup(index->author);

	pidgin_smiley_theme_index_free(index);

	smiley_themes = g_list_append(smiley_themes, theme);
}

static void
pidgin_smiley_theme_probe(void)
{
	GList *it, *next;
	int i;

	/* remove non-existing themes */
	for (it = smiley_themes; it; it = next) {
		PidginSmileyTheme *theme = it->data;
		PidginSmileyThemePrivate *priv =
			PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

		next = g_list_next(it);

		if (g_file_test(priv->path, G_FILE_TEST_EXISTS))
			continue;
		smiley_themes = g_list_remove_link(smiley_themes, it);
		g_object_unref(theme);
	}

	/* scan for themes */
	for (i = 0; probe_dirs[i]; i++) {
		GDir *dir = g_dir_open(probe_dirs[i], 0, NULL);
		const gchar *theme_dir_name;

		if (!dir)
			continue;

		while ((theme_dir_name = g_dir_read_name(dir))) {
			gchar *theme_path;

			/* Ignore Pidgin 2.x.y "none" theme. */
			if (g_strcmp0(theme_dir_name, "none") == 0)
				continue;

			theme_path = g_build_filename(
				probe_dirs[i], theme_dir_name, NULL);

			if (g_file_test(theme_path, G_FILE_TEST_IS_DIR))
				pidgin_smiley_theme_load(theme_path);

			g_free(theme_path);
		}

		g_dir_close(dir);
	}
}


/*******************************************************************************
 * API implementation
 ******************************************************************************/

static PurpleSmileyList *
pidgin_smiley_theme_get_smileys_impl(PurpleSmileyTheme *theme, gpointer ui_data)
{
	return NULL;
}

void
pidgin_smiley_theme_init(void)
{
	const gchar *user_smileys_dir;

	probe_dirs = g_new0(gchar*, 3);
	probe_dirs[0] = g_build_filename(
		DATADIR, "pixmaps", "pidgin", "emotes", NULL);
	user_smileys_dir = probe_dirs[1] = g_build_filename(
		purple_user_dir(), "smileys", NULL);

	if (!g_file_test(user_smileys_dir, G_FILE_TEST_IS_DIR))
		g_mkdir(user_smileys_dir, S_IRUSR | S_IWUSR | S_IXUSR);

	/* TODO: load configured theme */
}

void
pidgin_smiley_theme_uninit(void)
{
	g_strfreev(probe_dirs);
}

GList *
pidgin_smiley_theme_get_all(void)
{
	pidgin_smiley_theme_probe();

	return smiley_themes;
}

/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
pidgin_smiley_theme_finalize(GObject *obj)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(obj);

	g_free(priv->path);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
pidgin_smiley_theme_class_init(PidginSmileyThemeClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);
	PurpleSmileyThemeClass *pst_class = PURPLE_SMILEY_THEME_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PidginSmileyThemePrivate));

	gobj_class->finalize = pidgin_smiley_theme_finalize;

	pst_class->get_smileys = pidgin_smiley_theme_get_smileys_impl;
}

GType
pidgin_smiley_theme_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PidginSmileyThemeClass),
			.class_init = (GClassInitFunc)pidgin_smiley_theme_class_init,
			.instance_size = sizeof(PidginSmileyTheme),
		};

		type = g_type_register_static(PURPLE_TYPE_SMILEY_THEME,
			"PidginSmileyTheme", &info, 0);
	}

	return type;
}
