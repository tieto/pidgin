/*
 * Themes for Gaim
 *
 * Copyright (C) 2003, Sean Egan <bj91704@binghamton.edu>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "gtkimhtml.h"
#include <stdio.h>

struct smiley_list {
	char *sml;
	GSList *smileys;
	struct smiley_list *next;
};

struct smiley_theme {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;
	
	struct smiley_list *list;
};

GSList *smiley_themes;
static struct smiley_theme *current_smiley_theme;

void smiley_themeize(GtkWidget *imhtml)
{
	struct smiley_list *list;
	if (!current_smiley_theme)
		return;
	
	

	gtk_imhtml_remove_smileys(GTK_IMHTML(imhtml));
	list = current_smiley_theme->list;
	while (list) {
		char *sml = !strcmp(list->sml, "default") ? NULL : list->sml;
		GSList *icons = list->smileys;
		while (icons) {
			gtk_imhtml_associate_smiley(GTK_IMHTML(imhtml), sml, icons->data);
			icons = icons->next;
		}
		list = list->next;
	}
}

struct smiley_theme *load_smiley_theme(char *file, gboolean load)
{
	FILE *f = fopen(file, "r");
	char buf[256];
	char sml[16];
	char *i;
	struct smiley_theme *theme=NULL;
	struct smiley_list *list = NULL;
	GSList *lst = smiley_themes;
	char *dirname;
	
	while (lst) {
		struct smiley_theme *thm = lst->data;
		if (!g_strcasecmp(thm->path, file)) {
			theme = thm;
			break;
		}
		lst = lst->next;
	}
	if (!theme) {
		theme = g_new0(struct smiley_theme, 1);
		theme->path = file;
	}
	if (!f)
		return NULL;
	
	dirname = g_path_get_dirname(file);
	if (load) {
		if (current_smiley_theme) {
			struct smiley_list *wer = current_smiley_theme->list;
			while (wer) {
				char *nds = !strcmp(wer->sml, "default") ? NULL : wer->sml;
				GSList *dfg = wer->smileys;
				while (dfg) {
					GtkIMHtmlSmiley *uio = dfg->data;
					if (uio->icon)
						g_object_unref(uio->icon);
					g_free(uio->file);
					g_free(uio->smile);
					dfg = dfg->next;
				}
				wer = wer->next;
			}
		}
	current_smiley_theme = theme;
	}
	
	
	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f)) {
			g_free(dirname);
			return NULL;
		}
				
		if (buf[0] == '#' || buf[0] == '\0') 
			continue;
		
		i = buf;
		while (isspace(*i))
			i++;
		
		if (*i == '[' && strchr(i, ']') && load) {
			struct smiley_list *child = g_new0(struct smiley_list, 1);
			child->sml = g_strndup(i+1, (int)strchr(i, ']') - (int)i - 1);
			if (theme->list) 
				list->next = child;
			else
				theme->list = child;
			list = child;
		} else if (!g_strncasecmp(i, "Name=", strlen("Name="))) {
			theme->name = g_strdup(i+ strlen("Name="));
		} else if (!g_strncasecmp(i, "Description=", strlen("Description="))) {
			theme->desc = g_strdup(i + strlen("Description="));
		} else if (!g_strncasecmp(i, "Icon=", strlen("Icon="))) {
			theme->icon = g_build_filename(dirname, i + strlen("Icon="), NULL);
		} else if (!g_strncasecmp(i, "Author=", strlen("Author"))) {
			theme->desc = g_strdup(i + strlen("Author"));
		} else if (load && list) {
			gboolean hidden;
			char *file = NULL;
			GtkIMHtmlSmiley *smiley = g_new0(GtkIMHtmlSmiley, 1);
			
			if (*i == '!' && *i + 1 == ' ') {
				hidden = TRUE;
				i = i + 2;
			}
			while  (*i) {
				char l[64];
				int li = 0;
				while (!isspace(*i)) 
					l[li++] = *(i++);
				if (!file) {
					l[li] = 0;
					file = g_build_filename(dirname, l, NULL);
				} else {
					l[li] = 0;
					smiley = g_new0(GtkIMHtmlSmiley, 1);
					smiley->file = file;
					smiley->smile = g_strdup(l);
					list->smileys = g_slist_append(list->smileys, smiley);
				}
				while (isspace(*i))
					i++;
			}
		}
	}
	g_free(dirname);
	return theme;
}

void smiley_theme_probe()
{
	GDir *dir;
	const gchar *file;
	gchar *path;
	struct smiley_theme *smile;
	int l;

	char* probedirs[3];
	probedirs[0] = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", NULL);
	probedirs[1] = g_build_filename(gaim_user_dir(), "smileys", NULL);
	probedirs[2] = 0;
	for (l=0; probedirs[l]; l++) {
		dir = g_dir_open(probedirs[l], 0, NULL);
		if (dir) {
			while ((file = g_dir_read_name(dir))) {
				path = g_build_filename(probedirs[0], file, "theme", NULL);
				
				/* Here we check to see that the theme has proper syntax.
				 * We set the second argument to FALSE so that it doesn't load
				 * the theme yet.
				 */
				if (smile = load_smiley_theme(path, TRUE)) {
					smiley_themes = g_slist_append(smiley_themes, smile);
				}
				g_free(path);
			}
			g_dir_close(dir);
		}
		g_free(probedirs[l]);
	}
	
 
       
}
