/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#ifndef _GGP_STATUS_H
#define _GGP_STATUS_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_status_session_data ggp_status_session_data;

void ggp_status_setup(PurpleConnection *gc);
void ggp_status_cleanup(PurpleConnection *gc);

GList * ggp_status_types(PurpleAccount *account);
int ggp_status_from_purplestatus(PurpleStatus *status, gchar **message);
const gchar * ggp_status_to_purplestatus(int status);
const gchar * ggp_status_get_name(const gchar *purple_status);

// own status

void ggp_status_set_initial(PurpleConnection *gc, struct gg_login_params *glp);

gboolean ggp_status_set(PurpleAccount *account, int status, const gchar* msg);
void ggp_status_set_purplestatus(PurpleAccount *account, PurpleStatus *status);
void ggp_status_set_disconnected(PurpleAccount *account);
void ggp_status_fake_to_self(PurpleConnection *gc);

gboolean ggp_status_get_status_broadcasting(PurpleConnection *gc);
void ggp_status_set_status_broadcasting(PurpleConnection *gc,
	gboolean broadcasting);
void ggp_status_broadcasting_dialog(PurpleConnection *gc);

// buddy status

void ggp_status_got_others(PurpleConnection *gc, struct gg_event *ev);
char * ggp_status_buddy_text(PurpleBuddy *buddy);

#endif /* _GGP_STATUS_H */
