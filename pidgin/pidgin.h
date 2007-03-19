/**
 * @file pidgin.h UI definitions and includes
 * @ingroup gtkui
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* #warning ***pidgin*** */
#ifndef _PIDGIN_H_
#define _PIDGIN_H_

#ifndef _WIN32
# include <gdk/gdkx.h>
#endif

#include <gtk/gtk.h>

#ifdef _WIN32
# include "gtkwin32dep.h"
#endif

/**
 * Our UI's identifier.
 */
#define PIDGIN_UI "pidgin"

#ifndef _WIN32
# define PIDGIN_ALERT_TITLE ""
#else
# define PIDGIN_ALERT_TITLE PIDGIN_NAME
#endif

/*
 * This is backwards-compatibility code for older versions of GTK+ (< 2.4.x)
 * It defines the new wrap behavior (unknown in earlier versions)
 * as the old (slightly buggy) wrap behavior.
 * It also includes our back-ported GtkExpander
 */
/** @cond */
#if (!GTK_CHECK_VERSION(2,4,0))
# define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
# include "gtkexpander.h"
#endif
/** @endcond */

/*
 * We include the sources for GtkComboBox and GtkCellView because
 * they don't exist in older versions of GTK+, and we use them
 * in a few places.
 */
#if !GTK_CHECK_VERSION(2,6,0)
#   include "gtkcellview.h"
#   include "gtkcellviewmenuitem.h"
#   include "gaimcombobox.h"
#   if !GTK_CHECK_VERSION(2,4,0)
#       include "gtkcelllayout.h"
#   endif /* Less than GTK+ 2.4 */
#endif /* Less than GTK+ 2.6 */

/*
 * Spacings between components, as defined by the
 * GNOME Human Interface Guidelines.
 */
#define PIDGIN_HIG_CAT_SPACE     18
#define PIDGIN_HIG_BORDER        12
#define PIDGIN_HIG_BOX_SPACE      6

/*
 * See GNOME bug #307304 for some discussion about the invisible
 * character.  0x25cf is a good choice, too.
 */
#define PIDGIN_INVISIBLE_CHAR (gunichar)0x2022

#endif /* _PIDGIN_H_ */

