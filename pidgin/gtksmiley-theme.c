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

#include "gtkutils.h"

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

	GdkPixbuf *icon_pixbuf;

	GHashTable *smiley_lists_map;
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
pidgin_smiley_theme_index_parse(const gchar *theme_path, gboolean load_contents)
{
	PidginSmileyThemeIndex *index;
	PidginSmileyThemeIndexProtocol *proto = NULL;
	gchar *index_path;
	FILE *file;
	int line_no = 0;
	gboolean inv_frm = FALSE;

	index_path = g_build_filename(theme_path, "theme", NULL);
	file = g_fopen(index_path, "r");
	if (!file) {
		purple_debug_error("gtksmiley-theme",
			"Failed to open index file %s", index_path);
		g_free(index_path);
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
		smiley->shortcuts = g_list_reverse(smiley->shortcuts);
	}

	fclose(file);

	if (inv_frm) {
		purple_debug_error("gtksmiley-theme", "%s:%" G_GSIZE_FORMAT
			" invalid format", index_path, line_no);
		pidgin_smiley_theme_index_free(index);
		index = NULL;
	}

	g_free(index_path);
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

	/* it's not super-efficient, but we don't expect huge amount of
	 * installed themes */
	for (it = smiley_themes; it; it = g_list_next(it)) {
		PidginSmileyThemePrivate *priv =
			PIDGIN_SMILEY_THEME_GET_PRIVATE(it->data);

		/* theme is already loaded */
		if (g_strcmp0(priv->path, theme_path) == 0)
			return;
	}

	theme = g_object_new(PIDGIN_TYPE_SMILEY_THEME, NULL);
	priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	priv->path = g_strdup(theme_path);

	index = pidgin_smiley_theme_index_parse(theme_path, FALSE);

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
		smiley_themes = g_list_delete_link(smiley_themes, it);
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

const gchar *
pidgin_smiley_theme_get_name(PidginSmileyTheme *theme)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

const gchar *
pidgin_smiley_theme_get_description(PidginSmileyTheme *theme)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->desc;
}

GdkPixbuf *
pidgin_smiley_theme_get_icon(PidginSmileyTheme *theme)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->icon == NULL)
		return NULL;

	if (!priv->icon_pixbuf) {
		gchar *icon_path = g_build_filename(
			priv->path, priv->icon, NULL);
		priv->icon_pixbuf = pidgin_pixbuf_new_from_file(icon_path);
		g_free(icon_path);
	}

	return priv->icon_pixbuf;
}

const gchar *
pidgin_smiley_theme_get_author(PidginSmileyTheme *theme)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->author;
}

PurpleSmileyList *
pidgin_smiley_theme_for_conv(PurpleConversation *conv)
{
	PurpleAccount *acc = NULL;
	PurpleSmileyTheme *theme;
	const gchar *proto_name = NULL;

	theme = purple_smiley_theme_get_current();
	if (theme == NULL)
		return NULL;

	if (conv)
		acc = purple_conversation_get_account(conv);
	if (acc)
		proto_name = purple_account_get_protocol_name(acc);

	return purple_smiley_theme_get_smileys(theme, (gpointer)proto_name);
}

static void
pidgin_smiley_theme_activate_impl(PurpleSmileyTheme *theme)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);
	PidginSmileyThemeIndex *index;
	GHashTable *smap;
	GList *it, *it2, *it3;

	g_return_if_fail(priv != NULL);

	if (priv->smiley_lists_map)
		return;

	priv->smiley_lists_map = smap = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, g_object_unref);

	index = pidgin_smiley_theme_index_parse(priv->path, TRUE);

	for (it = index->protocols; it; it = g_list_next(it)) {
		PidginSmileyThemeIndexProtocol *proto_idx = it->data;
		PurpleSmileyList *proto_smileys;

		proto_smileys = g_hash_table_lookup(smap, proto_idx->name);
		if (!proto_smileys) {
			proto_smileys = purple_smiley_list_new();
			g_hash_table_insert(smap,
				g_strdup(proto_idx->name), proto_smileys);
		}

		for (it2 = proto_idx->smileys; it2; it2 = g_list_next(it2)) {
			PidginSmileyThemeIndexSmiley *smiley_idx = it2->data;
			gchar *smiley_path;

			smiley_path = g_build_filename(
				priv->path, smiley_idx->file, NULL);
			if (!g_file_test(smiley_path, G_FILE_TEST_EXISTS)) {
				purple_debug_warning("gtksmiley-theme",
					"Smiley %s is missing", smiley_path);
				continue;
			}

			for (it3 = smiley_idx->shortcuts; it3;
				it3 = g_list_next(it3))
			{
				PurpleSmiley *smiley;
				gchar *shortcut = it3->data;

				smiley = purple_smiley_new(
					shortcut, smiley_path);
				g_object_set_data(G_OBJECT(smiley),
					"pidgin-smiley-hidden",
					GINT_TO_POINTER(smiley_idx->hidden));
				purple_smiley_list_add(proto_smileys, smiley);
				g_object_unref(smiley);
			}
		}
	}

	pidgin_smiley_theme_index_free(index);
}

static PurpleSmileyList *
pidgin_smiley_theme_get_smileys_impl(PurpleSmileyTheme *theme, gpointer ui_data)
{
	PidginSmileyThemePrivate *priv = PIDGIN_SMILEY_THEME_GET_PRIVATE(theme);
	PurpleSmileyList *smileys = NULL;

	pidgin_smiley_theme_activate_impl(theme);

	if (ui_data)
		smileys = g_hash_table_lookup(priv->smiley_lists_map, ui_data);
	if (smileys != NULL)
		return smileys;

	return g_hash_table_lookup(priv->smiley_lists_map, "default");
}

void
pidgin_smiley_theme_init(void)
{
	GList *it;
	const gchar *user_smileys_dir;
	const gchar *theme_name;

	probe_dirs = g_new0(gchar*, 3);
	probe_dirs[0] = g_build_filename(
		DATADIR, "pixmaps", "pidgin", "emotes", NULL);
	user_smileys_dir = probe_dirs[1] = g_build_filename(
		purple_user_dir(), "smileys", NULL);

	if (!g_file_test(user_smileys_dir, G_FILE_TEST_IS_DIR))
		g_mkdir(user_smileys_dir, S_IRUSR | S_IWUSR | S_IXUSR);

	/* setting theme by name (copy-paste from gtkprefs) */
	pidgin_smiley_theme_probe();
	theme_name = purple_prefs_get_string(
		PIDGIN_PREFS_ROOT "/smileys/theme");
	for (it = smiley_themes; it; it = g_list_next(it)) {
		PidginSmileyTheme *theme = it->data;

		if (g_strcmp0(pidgin_smiley_theme_get_name(theme), theme_name))
			continue;

		purple_smiley_theme_set_current(PURPLE_SMILEY_THEME(theme));
	}
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
	g_free(priv->name);
	g_free(priv->desc);
	g_free(priv->icon);
	g_free(priv->author);
	if (priv->icon_pixbuf)
		g_object_unref(priv->icon_pixbuf);
	if (priv->smiley_lists_map)
		g_hash_table_destroy(priv->smiley_lists_map);

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
	pst_class->activate = pidgin_smiley_theme_activate_impl;
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
