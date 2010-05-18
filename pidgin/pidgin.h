/**
 * @file pidgin.h UI definitions and includes
 * @ingroup pidgin
 */

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
/* #warning ***pidgin*** */
#ifndef _PIDGIN_H_
#define _PIDGIN_H_

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
#endif

#ifdef _WIN32
# include "gtkwin32dep.h"
#endif

/**
 * Our UI's identifier.
 */
/* leave this as gtk-gaim until we have a decent way to migrate UI-prefs */
#define PIDGIN_UI "gtk-gaim"

/* change this only when we have a sane upgrade path for old prefs */
#define PIDGIN_PREFS_ROOT "/pidgin"

/* Translators may want to transliterate the name.
 It is not to be translated. */
#define PIDGIN_NAME _("Pidgin")

#ifndef _WIN32
# define PIDGIN_ALERT_TITLE ""
#else
# define PIDGIN_ALERT_TITLE PIDGIN_NAME
#endif

/*
 * Spacings between components, as defined by the
 * GNOME Human Interface Guidelines.
 */
#define PIDGIN_HIG_CAT_SPACE     18
#define PIDGIN_HIG_BORDER        12
#define PIDGIN_HIG_BOX_SPACE      6

#if !GTK_CHECK_VERSION(2,16,0) || !defined(PIDGIN_DISABLE_DEPRECATED)
/*
 * Older versions of GNOME defaulted to using an asterisk as the invisible
 * character.  But this is ugly and we want to use something nicer.
 *
 * The default invisible character was changed in GNOME revision 21446
 * (GTK+ 2.16) from an asterisk to the first available character out of
 * 0x25cf, 0x2022, 0x2731, 0x273a.  See GNOME bugs 83935 and 307304 for
 * discussion leading up to the change.
 *
 * Here's the change:
 * http://svn.gnome.org/viewvc/gtk%2B?view=revision&revision=21446
 *
 */
#define PIDGIN_INVISIBLE_CHAR (gunichar)0x25cf
#endif /* Less than GTK+ 2.16 */

#endif /* _PIDGIN_H_ */

