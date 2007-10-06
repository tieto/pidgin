/**
 * @file gtklog.h GTK+ Log viewer
 * @ingroup pidgin
 * @see @ref gtklog-signals
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
#ifndef _PIDGINLOG_H_
#define _PIDGINLOG_H_

#include "pidgin.h"
#include "log.h"

#include "account.h"

typedef struct _PidginLogViewer PidginLogViewer;

/**
 * A GTK+ Log Viewer.  You can look at logs with it.
 */
struct _PidginLogViewer {
	GList *logs;                 /**< The list of logs viewed in this viewer   */

	GtkWidget        *window;    /**< The viewer's window                      */
	GtkTreeStore     *treestore; /**< The treestore containing said logs       */
	GtkWidget        *treeview;  /**< The treeview representing said treestore */
	GtkWidget        *imhtml;    /**< The imhtml to display said logs          */
	GtkWidget        *entry;     /**< The search entry, in which search terms
	                              *   are entered                              */
	PurpleLogReadFlags flags;      /**< The most recently used log flags         */
	char             *search;    /**< The string currently being searched for  */
	GtkWidget        *label;     /**< The label at the top of the log viewer   */
};



void pidgin_log_show(PurpleLogType type, const char *screenname, PurpleAccount *account);
void pidgin_log_show_contact(PurpleContact *contact);

void pidgin_syslog_show(void);

/**************************************************************************/
/** @name GTK+ Log Subsystem                                              */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GTK+ log subsystem.
 */
void pidgin_log_init(void);

/**
 * Returns the GTK+ log subsystem handle.
 *
 * @return The GTK+ log subsystem handle.
 */
void *pidgin_log_get_handle(void);

/**
 * Uninitializes the GTK+ log subsystem.
 */
void pidgin_log_uninit(void);

/*@}*/

#endif
