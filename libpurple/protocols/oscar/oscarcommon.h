/* gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 *
 */

/* oscarcommon.h contains prototypes for the prpl functions used by libaim.c
 * and libicq.c
 */

#include "accountopt.h"
#include "internal.h"
#include "prpl.h"
#include "version.h"
#include "notify.h"

#define OSCAR_DEFAULT_LOGIN_SERVER "login.oscar.aol.com"
#define OSCAR_DEFAULT_LOGIN_PORT 5190
#ifndef _WIN32
#define OSCAR_DEFAULT_CUSTOM_ENCODING "ISO-8859-1"
#else
#define OSCAR_DEFAULT_CUSTOM_ENCODING oscar_get_locale_charset()
#endif
#define OSCAR_DEFAULT_AUTHORIZATION TRUE
#define OSCAR_DEFAULT_HIDE_IP TRUE
#define OSCAR_DEFAULT_WEB_AWARE FALSE
#define OSCAR_DEFAULT_ALWAYS_USE_RV_PROXY FALSE

#ifdef _WIN32
const char *oscar_get_locale_charset(void);
#endif
const char *oscar_list_icon_icq(GaimAccount *a, GaimBuddy *b);
const char *oscar_list_icon_aim(GaimAccount *a, GaimBuddy *b);
const char* oscar_list_emblem(GaimBuddy *b);
char *oscar_status_text(GaimBuddy *b);
void oscar_tooltip_text(GaimBuddy *b, GaimNotifyUserInfo *user_info, gboolean full);
GList *oscar_status_types(GaimAccount *account);
GList *oscar_blist_node_menu(GaimBlistNode *node);
GList *oscar_chat_info(GaimConnection *gc);
GHashTable *oscar_chat_info_defaults(GaimConnection *gc, const char *chat_name);
void oscar_login(GaimAccount *account);
void oscar_close(GaimConnection *gc);
int oscar_send_im(GaimConnection *gc, const char *name, const char *message, GaimMessageFlags imflags);
void oscar_set_info(GaimConnection *gc, const char *rawinfo);
unsigned int oscar_send_typing(GaimConnection *gc, const char *name, GaimTypingState state);
void oscar_get_info(GaimConnection *gc, const char *name);
void oscar_set_status(GaimAccount *account, GaimStatus *status);
void oscar_set_idle(GaimConnection *gc, int time);
void oscar_change_passwd(GaimConnection *gc, const char *old, const char *new);
void oscar_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group);
void oscar_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group);
void oscar_add_permit(GaimConnection *gc, const char *who);
void oscar_add_deny(GaimConnection *gc, const char *who);
void oscar_rem_permit(GaimConnection *gc, const char *who);
void oscar_rem_deny(GaimConnection *gc, const char *who);
void oscar_set_permit_deny(GaimConnection *gc);
void oscar_join_chat(GaimConnection *gc, GHashTable *data);
char *oscar_get_chat_name(GHashTable *data);
void oscar_chat_invite(GaimConnection *gc, int id, const char *message, const char *name);
void oscar_chat_leave(GaimConnection *gc, int id);
int oscar_send_chat(GaimConnection *gc, int id, const char *message, GaimMessageFlags flags);
void oscar_keepalive(GaimConnection *gc);
void oscar_alias_buddy(GaimConnection *gc, const char *name, const char *alias);
void oscar_move_buddy(GaimConnection *gc, const char *name, const char *old_group, const char *new_group);
void oscar_rename_group(GaimConnection *gc, const char *old_name, GaimGroup *group, GList *moved_buddies);
void oscar_convo_closed(GaimConnection *gc, const char *who);
const char *oscar_normalize(const GaimAccount *account, const char *str);
void oscar_set_icon(GaimConnection *gc, const char *iconfile);
gboolean oscar_can_receive_file(GaimConnection *gc, const char *who);
void oscar_send_file(GaimConnection *gc, const char *who, const char *file);
GaimXfer *oscar_new_xfer(GaimConnection *gc, const char *who);
gboolean oscar_offline_message(const GaimBuddy *buddy);
GList *oscar_actions(GaimPlugin *plugin, gpointer context);
