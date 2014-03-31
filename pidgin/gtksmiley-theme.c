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

#include "debug.h"

#include <glib/gstdio.h>

#define PIDGIN_SMILEY_THEME_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_SMILEY_THEME, \
	PidginSmileyThemePrivate))

#define PIDGIN_SMILEY_THEME_MAX_INDEX_SIZE 102400

typedef struct
{
	gchar *path;
} PidginSmileyThemePrivate;

static GObjectClass *parent_class;

static gchar **probe_dirs;
static GList *smiley_themes = NULL;

/*******************************************************************************
 * Theme loading
 ******************************************************************************/

static void
pidgin_smiley_theme_load(const gchar *theme_path)
{
	PidginSmileyTheme *theme;
	PidginSmileyThemePrivate *priv;
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

	g_free(index_path);

	purple_debug_fatal("tomo", "loading not implemented");
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

	//TODO: remove it
	pidgin_smiley_theme_probe();
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

	return NULL;
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
