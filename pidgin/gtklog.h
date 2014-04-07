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
/**
 * SECTION:gtklog
 * @section_id: pidgin-gtklog
 * @short_description: <filename>gtklog.h</filename>
 * @title: Log Viewer
 * @see_also: <link linkend="chapter-signals-gtklog">Log signals</link>
 */

#include "pidgin.h"
#include "log.h"

#include "account.h"

typedef struct _PidginLogViewer PidginLogViewer;

/**
 * PidginLogViewer:
 * @logs:      The list of logs viewed in this viewer
 * @window:    The viewer's window
 * @treestore: The treestore containing said logs
 * @treeview:  The treeview representing said treestore
 * @web_view:  The webkit web view to display said logs
 * @entry:     The search entry, in which search terms are entered
 * @flags:     The most recently used log flags
 * @search:    The string currently being searched for
 * @label:     The label at the top of the log viewer
 *
 * A GTK+ Log Viewer.  You can look at logs with it.
 */
struct _PidginLogViewer {
	GList *logs;

	GtkWidget        *window;
	GtkTreeStore     *treestore;
	GtkWidget        *treeview;
	GtkWidget        *web_view;
	GtkWidget        *entry;

	PurpleLogReadFlags flags;
	char             *search;
	GtkLabel         *label;
};


G_BEGIN_DECLS

void pidgin_log_show(PurpleLogType type, const char *buddyname, PurpleAccount *account);
void pidgin_log_show_contact(PurpleContact *contact);

void pidgin_syslog_show(void);

/**************************************************************************/
/* GTK+ Log Subsystem                                                     */
/**************************************************************************/

/**
 * pidgin_log_init:
 *
 * Initializes the GTK+ log subsystem.
 */
void pidgin_log_init(void);

/**
 * pidgin_log_get_handle:
 *
 * Returns the GTK+ log subsystem handle.
 *
 * Returns: The GTK+ log subsystem handle.
 */
void *pidgin_log_get_handle(void);

/**
 * pidgin_log_uninit:
 *
 * Uninitializes the GTK+ log subsystem.
 */
void pidgin_log_uninit(void);

G_END_DECLS

#endif
