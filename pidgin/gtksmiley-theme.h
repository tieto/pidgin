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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PIDGIN_SMILEY_THEME_H_
#define _PIDGIN_SMILEY_THEME_H_

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "smiley-theme.h"

typedef struct _PidginSmileyTheme PidginSmileyTheme;
typedef struct _PidginSmileyThemeClass PidginSmileyThemeClass;

#define PIDGIN_TYPE_SMILEY_THEME            (pidgin_smiley_theme_get_type())
#define PIDGIN_SMILEY_THEME(smiley)         (G_TYPE_CHECK_INSTANCE_CAST((smiley), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyTheme))
#define PIDGIN_SMILEY_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyThemeClass))
#define PIDGIN_IS_SMILEY_THEME(smiley)      (G_TYPE_CHECK_INSTANCE_TYPE((smiley), PIDGIN_TYPE_SMILEY_THEME))
#define PIDGIN_IS_SMILEY_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_SMILEY_THEME))
#define PIDGIN_SMILEY_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_SMILEY_THEME, PidginSmileyThemeClass))

/**
 * PidginSmileyTheme:
 *
 * A list of smileys.
 */
struct _PidginSmileyTheme
{
	/*< private >*/
	PurpleSmileyTheme parent;
};

struct _PidginSmileyThemeClass
{
	/*< private >*/
	PurpleSmileyThemeClass parent_class;

	void (*pidgin_reserved1)(void);
	void (*pidgin_reserved2)(void);
	void (*pidgin_reserved3)(void);
	void (*pidgin_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * pidgin_smiley_theme_get_type:
 *
 * Returns: The #GType for a smiley list.
 */
GType
pidgin_smiley_theme_get_type(void);

/* TODO: remove it */
const gchar *
pidgin_smiley_theme_get_path(PidginSmileyTheme *theme);

const gchar *
pidgin_smiley_theme_get_name(PidginSmileyTheme *theme);

const gchar *
pidgin_smiley_theme_get_description(PidginSmileyTheme *theme);

GdkPixbuf *
pidgin_smiley_theme_get_icon(PidginSmileyTheme *theme);

const gchar *
pidgin_smiley_theme_get_author(PidginSmileyTheme *theme);

void
pidgin_smiley_theme_init(void);

void
pidgin_smiley_theme_uninit(void);

GList *
pidgin_smiley_theme_get_all(void);

G_END_DECLS

#endif /* _PIDGIN_SMILEY_THEME_H_ */

