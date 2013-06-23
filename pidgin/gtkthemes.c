/*
 * Themes for Pidgin
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
 *
 */
#include "internal.h"
#include "pidgin.h"

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtksmiley.h"
#include "gtkthemes.h"
#include "gtkwebview.h"

GSList *smiley_themes = NULL;
struct smiley_theme *current_smiley_theme;

static void pidgin_themes_destroy_smiley_theme_smileys(struct smiley_theme *theme);

gboolean pidgin_themes_smileys_disabled()
{
	if (!current_smiley_theme)
		return 1;

	return strcmp(current_smiley_theme->name, "none") == 0;
}

static void
pidgin_themes_destroy_smiley_theme(struct smiley_theme *theme)
{
	pidgin_themes_destroy_smiley_theme_smileys(theme);

	g_free(theme->name);
	g_free(theme->desc);
	g_free(theme->author);
	g_free(theme->icon);
	g_free(theme->path);
	g_free(theme);
}

static void pidgin_themes_remove_theme_dir(const char *theme_dir_name)
{
	GString *str = NULL;
	const char *file_name = NULL;
	GDir *theme_dir = NULL;

	if ((theme_dir = g_dir_open(theme_dir_name, 0, NULL)) != NULL) {
		if ((str = g_string_new(theme_dir_name)) != NULL) {
			while ((file_name = g_dir_read_name(theme_dir)) != NULL) {
				g_string_printf(str, "%s%s%s", theme_dir_name, G_DIR_SEPARATOR_S, file_name);
				g_unlink(str->str);
			}
			g_string_free(str, TRUE);
		}
		g_dir_close(theme_dir);
		g_rmdir(theme_dir_name);
	}
}

void pidgin_themes_remove_smiley_theme(const char *file)
{
	char *theme_dir = NULL, *last_slash = NULL;
	g_return_if_fail(NULL != file);

	if (!g_file_test(file, G_FILE_TEST_EXISTS)) return;
	if ((theme_dir = g_strdup(file)) == NULL) return ;

	if ((last_slash = g_strrstr(theme_dir, G_DIR_SEPARATOR_S)) != NULL) {
		GSList *iter = NULL;
		struct smiley_theme *theme = NULL, *new_theme = NULL;

		*last_slash = 0;

		/* Delete files on disk */
		pidgin_themes_remove_theme_dir(theme_dir);

		/* Find theme in themes list and remove it */
		for (iter = smiley_themes ; iter ; iter = iter->next) {
			theme = ((struct smiley_theme *)(iter->data));
			if (!strcmp(theme->path, file))
				break ;
		}
		if (iter) {
			if (theme == current_smiley_theme) {
				new_theme = ((struct smiley_theme *)(NULL == iter->next ? (smiley_themes == iter ? NULL : smiley_themes->data) : iter->next->data));
				if (new_theme)
					purple_prefs_set_string(PIDGIN_PREFS_ROOT "/smileys/theme", new_theme->name);
				else
					current_smiley_theme = NULL;
			}
			smiley_themes = g_slist_delete_link(smiley_themes, iter);

			/* Destroy theme structure */
			pidgin_themes_destroy_smiley_theme(theme);
		}
	}

	g_free(theme_dir);
}

static void _pidgin_themes_smiley_themeize(GtkWidget *webview, gboolean custom)
{
	struct smiley_list *list;
	if (!current_smiley_theme)
		return;

	gtk_webview_remove_smileys(GTK_WEBVIEW(webview));
	list = current_smiley_theme->list;
	while (list) {
		char *sml = !strcmp(list->sml, "default") ? NULL : list->sml;
		GSList *icons = list->smileys;
		while (icons) {
			gtk_webview_associate_smiley(GTK_WEBVIEW(webview), sml, icons->data);
			icons = icons->next;
		}

		if (custom == TRUE) {
			icons = pidgin_smileys_get_all();

			while (icons) {
				gtk_webview_associate_smiley(GTK_WEBVIEW(webview), sml, icons->data);
				icons = icons->next;
			}
		}

		list = list->next;
	}
}

void
pidgin_themes_smiley_themeize(GtkWidget *webview)
{
	_pidgin_themes_smiley_themeize(webview, FALSE);
}

void
pidgin_themes_smiley_themeize_custom(GtkWidget *webview)
{
	_pidgin_themes_smiley_themeize(webview, TRUE);
}

static void
pidgin_themes_destroy_smiley_theme_smileys(struct smiley_theme *theme)
{
	struct smiley_list *wer;

	for (wer = theme->list; wer != NULL; wer = theme->list) {
		while (wer->smileys) {
			GtkWebViewSmiley *uio = wer->smileys->data;
			gtk_webview_smiley_destroy(uio);
			wer->smileys = g_slist_delete_link(wer->smileys, wer->smileys);
		}
		theme->list = wer->next;
		g_free(wer->sml);
		g_free(wer);
	}
	theme->list = NULL;
}

static void
pidgin_smiley_themes_remove_non_existing(void)
{
	static struct smiley_theme *theme = NULL;
	GSList *iter = NULL;

	if (!smiley_themes) return ;

	for (iter = smiley_themes ; iter ; iter = iter->next) {
		theme = ((struct smiley_theme *)(iter->data));
		if (!g_file_test(theme->path, G_FILE_TEST_EXISTS)) {
			if (theme == current_smiley_theme)
				current_smiley_theme = ((struct smiley_theme *)(NULL == iter->next ? NULL : iter->next->data));
			pidgin_themes_destroy_smiley_theme(theme);
			iter->data = NULL;
		}
	}
	/* Remove all elements whose data is NULL */
	smiley_themes = g_slist_remove_all(smiley_themes, NULL);

	if (!current_smiley_theme && smiley_themes) {
		struct smiley_theme *smile = g_slist_last(smiley_themes)->data;
		pidgin_themes_load_smiley_theme(smile->path, TRUE);
	}
}

void pidgin_themes_load_smiley_theme(const char *file, gboolean load)
{
	FILE *f = g_fopen(file, "r");
	char buf[256];
	char *i;
	struct smiley_theme *theme=NULL;
	struct smiley_list *list = NULL;
	GSList *lst = smiley_themes;
	char *dirname;

	if (!f)
		return;

	while (lst) {
		struct smiley_theme *thm = lst->data;
		if (!strcmp(thm->path, file)) {
			theme = thm;
			break;
		}
		lst = lst->next;
	}

	if (theme != NULL && theme == current_smiley_theme) {
		/* Don't reload the theme if it is already loaded */
		fclose(f);
		return;
	}

	if (theme == NULL) {
		theme = g_new0(struct smiley_theme, 1);
		theme->path = g_strdup(file);
		smiley_themes = g_slist_prepend(smiley_themes, theme);
	}

	dirname = g_path_get_dirname(file);

	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f)) {
			break;
		}

		if (buf[0] == '#' || buf[0] == '\0')
			continue;
		else {
			int len = strlen(buf);
			while (len && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
				buf[--len] = '\0';
			if (len == 0)
				continue;
		}

		i = buf;
		while (isspace(*i))
			i++;

		if (*i == '[' && strchr(i, ']') && load) {
			struct smiley_list *child = g_new0(struct smiley_list, 1);
			child->sml = g_strndup(i+1, strchr(i, ']') - i - 1);
			child->files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

			if (theme->list)
				list->next = child;
			else
				theme->list = child;
			/* Reverse the Smiley list since it was built in reverse order for efficiency reasons */
			if (list != NULL)
				list->smileys = g_slist_reverse(list->smileys);
			list = child;
		} else if (!g_ascii_strncasecmp(i, "Name=", strlen("Name="))) {
			g_free(theme->name);
			theme->name = g_strdup(i + strlen("Name="));
		} else if (!g_ascii_strncasecmp(i, "Description=", strlen("Description="))) {
			g_free(theme->desc);
			theme->desc = g_strdup(i + strlen("Description="));
		} else if (!g_ascii_strncasecmp(i, "Icon=", strlen("Icon="))) {
			g_free(theme->icon);
			theme->icon = g_build_filename(dirname, i + strlen("Icon="), NULL);
		} else if (!g_ascii_strncasecmp(i, "Author=", strlen("Author="))) {
			g_free(theme->author);
			theme->author = g_strdup(i + strlen("Author="));
		} else if (load && list) {
			gboolean hidden = FALSE;
			char *sfile = NULL;

			if (*i == '!' && *(i + 1) == ' ') {
				hidden = TRUE;
				i = i + 2;
			}
			while  (*i) {
				char l[64];
				int li = 0;
				while (*i && !isspace(*i) && li < sizeof(l) - 1) {
					if (*i == '\\' && *(i+1) != '\0')
						i++;
					l[li++] = *(i++);
				}
				l[li] = 0;
				if (!sfile) {
					sfile = g_build_filename(dirname, l, NULL);
				} else {
					GtkWebViewSmiley *smiley = gtk_webview_smiley_create(sfile, l, hidden, 0);
					list->smileys = g_slist_prepend(list->smileys, smiley);
					g_hash_table_insert (list->files, g_strdup(l), g_strdup(sfile));
				}
				while (isspace(*i))
					i++;

			}


			g_free(sfile);
		}
	}

	/* Reverse the Smiley list since it was built in reverse order for efficiency reasons */
	if (list != NULL)
		list->smileys = g_slist_reverse(list->smileys);

	g_free(dirname);
	fclose(f);

	if (!theme->name || !theme->desc || !theme->author) {
		purple_debug_error("gtkthemes", "Invalid file format, not loading smiley theme from '%s'\n", file);

		smiley_themes = g_slist_remove(smiley_themes, theme);
		pidgin_themes_destroy_smiley_theme(theme);
		return;
	}

	if (load) {
		GList *cnv;

		if (current_smiley_theme)
			pidgin_themes_destroy_smiley_theme_smileys(current_smiley_theme);
		current_smiley_theme = theme;

		for (cnv = purple_conversations_get(); cnv != NULL; cnv = cnv->next) {
			PurpleConversation *conv = cnv->data;

			if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
				/* We want to see our custom smileys on our entry if we write the shortcut */
				pidgin_themes_smiley_themeize_custom(PIDGIN_CONVERSATION(conv)->entry);
			}
		}
	}
}

void pidgin_themes_smiley_theme_probe()
{
	GDir *dir;
	const gchar *file;
	gchar *path, *test_path;
	int l;
	char* probedirs[3];

	pidgin_smiley_themes_remove_non_existing();

	probedirs[0] = g_build_filename(DATADIR, "pixmaps", "pidgin", "emotes", NULL);
	probedirs[1] = g_build_filename(purple_user_dir(), "smileys", NULL);
	probedirs[2] = 0;
	for (l=0; probedirs[l]; l++) {
		dir = g_dir_open(probedirs[l], 0, NULL);
		if (dir) {
			while ((file = g_dir_read_name(dir))) {
				test_path = g_build_filename(probedirs[l], file, NULL);
				if (g_file_test(test_path, G_FILE_TEST_IS_DIR)) {
					path = g_build_filename(probedirs[l], file, "theme", NULL);

					/* Here we check to see that the theme has proper syntax.
					 * We set the second argument to FALSE so that it doesn't load
					 * the theme yet.
					 */
					pidgin_themes_load_smiley_theme(path, FALSE);
					g_free(path);
				}
				g_free(test_path);
			}
			g_dir_close(dir);
		} else if (l == 1) {
			g_mkdir(probedirs[l], S_IRUSR | S_IWUSR | S_IXUSR);
		}
		g_free(probedirs[l]);
	}

	if (!current_smiley_theme && smiley_themes) {
		struct smiley_theme *smile = smiley_themes->data;
		pidgin_themes_load_smiley_theme(smile->path, TRUE);
	}
}

GSList *pidgin_themes_get_proto_smileys(const char *id) {
	PurplePlugin *proto;
	struct smiley_list *list, *def;

	if ((current_smiley_theme == NULL) || (current_smiley_theme->list == NULL))
		return NULL;

	def = list = current_smiley_theme->list;

	if (id == NULL)
		return def->smileys;

	proto = purple_find_prpl(id);

	while (list) {
		if (!strcmp(list->sml, "default"))
			def = list;
		else if (proto && !strcmp(proto->info->name, list->sml))
			break;

		list = list->next;
	}

	return list ? list->smileys : def->smileys;
}

void pidgin_themes_init()
{
	GSList *l;
	const char *current_theme =
		purple_prefs_get_string(PIDGIN_PREFS_ROOT "/smileys/theme");

	pidgin_themes_smiley_theme_probe();

	for (l = smiley_themes; l; l = l->next) {
		struct smiley_theme *smile = l->data;
		if (smile->name && strcmp(current_theme, smile->name) == 0) {
			pidgin_themes_load_smiley_theme(smile->path, TRUE);
			break;
		}
	}

	/* If we still don't have a smiley theme, choose the first one */
	if (!current_smiley_theme && smiley_themes) {
		struct smiley_theme *smile = smiley_themes->data;
		pidgin_themes_load_smiley_theme(smile->path, TRUE);
	}
}
