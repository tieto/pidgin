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

#ifndef _PIDGINTHEMES_H_
#define _PIDGINTHEMES_H_
/**
 * SECTION:gtkthemes
 * @section_id: pidgin-gtkthemes
 * @short_description: <filename>gtkthemes.h</filename>
 * @title: Smiley Theme API
 */

struct PidginSmileyList {
	char *sml;
	GSList *smileys;
	GHashTable *files; /* map from smiley shortcut to filename */
	struct PidginSmileyList *next;
};

struct PidginSmileyThemeREMOVEIT {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;

	struct PidginSmileyList *list;
};

extern struct PidginSmileyTheme *current_smiley_theme;

G_BEGIN_DECLS

void pidgin_themes_init(void);

gboolean pidgin_themes_smileys_disabled(void);

void pidgin_themes_smiley_themeize(GtkWidget *);

void pidgin_themes_smiley_themeize_custom(GtkWidget *);

void pidgin_themes_smiley_theme_probe(void);

void pidgin_themes_load_smiley_theme(const char *file, gboolean load);

void pidgin_themes_remove_smiley_theme(const char *file);

GSList *pidgin_themes_get_proto_smileys(const char *id);

G_END_DECLS

#endif /* _PIDGINTHEMES_H_ */
