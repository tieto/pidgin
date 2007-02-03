/**
 * @defgroup gtkui GTK+ User Interface
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
#ifndef _PIDGINDIALOGS_H_
#define _PIDGINDIALOGS_H_

#include "gtkgaim.h"

#include "account.h"
#include "conversation.h"

/* Functions in gtkdialogs.c (these should actually stay in this file) */
void pidgindialogs_destroy_all(void);
void pidgindialogs_about(void);
void pidgindialogs_im(void);
void pidgindialogs_im_with_user(GaimAccount *, const char *);
void pidgindialogs_info(void);
void pidgindialogs_log(void);
void pidgindialogs_alias_contact(GaimContact *);
void pidgindialogs_alias_buddy(GaimBuddy *);
void pidgindialogs_alias_chat(GaimChat *);

void pidgindialogs_remove_buddy(GaimBuddy *);
void pidgindialogs_remove_group(GaimGroup *);
void pidgindialogs_remove_chat(GaimChat *);
void pidgindialogs_remove_contact(GaimContact *);
void pidgindialogs_merge_groups(GaimGroup *, const char *);

/* Everything after this should probably be moved elsewhere */

/**
 * Our UI's identifier.
 */
#define PIDGIN_DIALOG(x)	x = gtk_window_new(GTK_WINDOW_TOPLEVEL); \
			gtk_window_set_type_hint(GTK_WINDOW(x), GDK_WINDOW_TYPE_HINT_DIALOG)
#define PIDGIN_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

#endif /* _PIDGINDIALOGS_H_ */
