/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#ifndef _FINCHLOG_H_
#define _FINCHLOG_H_
/**
 * SECTION:gntlog
 * @section_id: finch-gntlog
 * @short_description: <filename>gntlog.h</filename>
 * @title: Log Viewer
 */

#include "log.h"
#include "account.h"
#include "gntwidget.h"

typedef struct _FinchLogViewer FinchLogViewer;

/**
 * FinchLogViewer:
 * @logs: The list of logs viewed in this viewer
 * @window: The viewer's window
 * @tree:   The tree representing said treestore
 * @text:   The text to display said logs
 * @entry:  The search entry, in which search terms are entered
 * @flags:  The most recently used log flags
 * @search: The string currently being searched for
 *
 * A GNT Log Viewer.  You can look at logs with it.
 */
struct _FinchLogViewer {
	GList *logs;

	GntWidget	*window;
	GntWidget	*tree;
	GntWidget	*text;
	GntWidget	*entry;
	GntWidget	*label;
	PurpleLogReadFlags flags;
	char		*search;
};



void finch_log_show(PurpleLogType type, const char *username, PurpleAccount *account);
void finch_log_show_contact(PurpleContact *contact);

void finch_syslog_show(void);

/**************************************************************************/
/* GNT Log Subsystem                                                      */
/**************************************************************************/

/**
 * finch_log_init:
 *
 * Initializes the GNT log subsystem.
 */
void finch_log_init(void);

/**
 * finch_log_get_handle:
 *
 * Returns the GNT log subsystem handle.
 *
 * Returns: The GNT log subsystem handle.
 */
void *finch_log_get_handle(void);

/**
 * finch_log_uninit:
 *
 * Uninitializes the GNT log subsystem.
 */
void finch_log_uninit(void);

#endif
