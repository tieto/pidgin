/**
 * @file gntlog.h GNT Log viewer
 * @ingroup finch
 */

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

#include "log.h"
#include "account.h"
#include "gntwidget.h"

typedef struct _FinchLogViewer FinchLogViewer;

/**
 * A GNT Log Viewer.  You can look at logs with it.
 */
struct _FinchLogViewer {
	GList *logs;                 /**< The list of logs viewed in this viewer   */

	GntWidget	*window;    /**< The viewer's window                      */
	GntWidget	*tree;      /**< The tree representing said treestore */
	GntWidget	*text;      /**< The text to display said logs          */
	GntWidget	*entry;     /**< The search entry, in which search terms
	                              *   are entered                              */
	GntWidget	*label;
	PurpleLogReadFlags flags;      /**< The most recently used log flags         */
	char		*search;    /**< The string currently being searched for  */
};



void finch_log_show(PurpleLogType type, const char *username, PurpleAccount *account);
void finch_log_show_contact(PurpleContact *contact);

void finch_syslog_show(void);

/**************************************************************************/
/** @name GNT Log Subsystem                                              */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GNT log subsystem.
 */
void finch_log_init(void);

/**
 * Returns the GNT log subsystem handle.
 *
 * @return The GNT log subsystem handle.
 */
void *finch_log_get_handle(void);

/**
 * Uninitializes the GNT log subsystem.
 */
void finch_log_uninit(void);

/*@}*/

#endif
