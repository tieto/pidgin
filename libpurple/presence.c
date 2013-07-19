/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#include "presence.h"

#define PURPLE_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PRESENCE, PurplePresencePrivate))

/** @copydoc _PurplePresencePrivate */
typedef struct _PurplePresencePrivate  PurplePresencePrivate;

#define PURPLE_ACCOUNT_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_ACCOUNT_PRESENCE, PurpleAccountPresencePrivate))

/** @copydoc _PurpleAccountPresencePrivate */
typedef struct _PurpleAccountPresencePrivate  PurpleAccountPresencePrivate;

#define PURPLE_BUDDY_PRESENCE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BUDDY_PRESENCE, PurpleBuddyPresencePrivate))

/** @copydoc _PurpleBuddyPresencePrivate */
typedef struct _PurpleBuddyPresencePrivate  PurpleBuddyPresencePrivate;

/** Private data for a presence */
struct _PurplePresencePrivate
{
	gboolean idle;
	time_t idle_time;
	time_t login_time;

	GList *statuses;
	GHashTable *status_table;

	PurpleStatus *active_status;
};

/** Private data for an account presence */
struct _PurpleAccountPresencePrivate
{
	PurpleAccount *account;
};

/** Private data for a buddy presence */
struct _PurpleBuddyPresencePrivate
{
	PurpleBuddy *buddy;
};

/**************************************************************************
* PurplePresence API
**************************************************************************/
PurplePresence *
purple_presence_new(PurplePresenceContext context)
{
	PurplePresence *presence;

	g_return_val_if_fail(context != PURPLE_PRESENCE_CONTEXT_UNSET, NULL);

	presence = g_new0(PurplePresence, 1);
	PURPLE_DBUS_REGISTER_POINTER(presence, PurplePresence);

	presence->context = context;

	presence->status_table =
		g_hash_table_new_full(g_str_hash, g_str_equal,
							  g_free, NULL);

	return presence;
}

PurplePresence *
purple_account_presence_new(PurpleAccount *account)
{
	PurplePresence *presence = NULL;
	g_return_val_if_fail(account != NULL, NULL);

	presence = purple_presence_new(PURPLE_PRESENCE_CONTEXT_ACCOUNT);
	presence->u.account = account;
	presence->statuses = purple_prpl_get_statuses(account, presence);

	return presence;
}

PurplePresence *
purple_conversation_presence_new(PurpleConversation *conv)
{
	PurplePresence *presence;

	g_return_val_if_fail(conv != NULL, NULL);

	presence = purple_presence_new(PURPLE_PRESENCE_CONTEXT_CONV);
	presence->u.chat.conv = conv;
	/* presence->statuses = purple_prpl_get_statuses(purple_conversation_get_account(conv), presence); ? */

	return presence;
}

PurplePresence *
purple_buddy_presence_new(PurpleBuddy *buddy)
{
	PurplePresence *presence;
	PurpleAccount *account;

	g_return_val_if_fail(buddy != NULL, NULL);
	account = purple_buddy_get_account(buddy);

	presence = purple_presence_new(PURPLE_PRESENCE_CONTEXT_BUDDY);

	presence->u.buddy.name    = g_strdup(purple_buddy_get_name(buddy));
	presence->u.buddy.account = account;
	presence->statuses = purple_prpl_get_statuses(account, presence);

	presence->u.buddy.buddy = buddy;

	return presence;
}

void
purple_presence_destroy(PurplePresence *presence)
{
	g_return_if_fail(presence != NULL);

	if (purple_presence_get_context(presence) == PURPLE_PRESENCE_CONTEXT_BUDDY)
	{
		g_free(presence->u.buddy.name);
	}
	else if (purple_presence_get_context(presence) == PURPLE_PRESENCE_CONTEXT_CONV)
	{
		g_free(presence->u.chat.user);
	}

	g_list_foreach(presence->statuses, (GFunc)purple_status_destroy, NULL);
	g_list_free(presence->statuses);

	g_hash_table_destroy(presence->status_table);

	PURPLE_DBUS_UNREGISTER_POINTER(presence);
	g_free(presence);
}

void
purple_presence_set_status_active(PurplePresence *presence, const char *status_id,
		gboolean active)
{
	PurpleStatus *status;

	g_return_if_fail(presence  != NULL);
	g_return_if_fail(status_id != NULL);

	status = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);
	/* TODO: Should we do the following? */
	/* g_return_if_fail(active == status->active); */

	if (purple_status_is_exclusive(status))
	{
		if (!active)
		{
			purple_debug_warning("status",
					"Attempted to set a non-independent status "
					"(%s) inactive. Only independent statuses "
					"can be specifically marked inactive.",
					status_id);
			return;
		}
	}

	purple_status_set_active(status, active);
}

void
purple_presence_switch_status(PurplePresence *presence, const char *status_id)
{
	purple_presence_set_status_active(presence, status_id, TRUE);
}

static void
update_buddy_idle(PurpleBuddy *buddy, PurplePresence *presence,
		time_t current_time, gboolean old_idle, gboolean idle)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleAccount *account = purple_buddy_get_account(buddy);

	if (!old_idle && idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became idle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				purple_buddy_get_alias(buddy), current_time, tmp2);
				g_free(tmp2);
			}
		}
	}
	else if (old_idle && !idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became unidle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				purple_buddy_get_alias(buddy), current_time, tmp2);
				g_free(tmp2);
			}
		}
	}

	if (old_idle != idle)
		purple_signal_emit(purple_blist_get_handle(), "buddy-idle-changed", buddy,
		                 old_idle, idle);

	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	/* Should this be done here? It'd perhaps make more sense to
	 * connect to buddy-[un]idle signals and update from there
	 */

	if (ops != NULL && ops->update != NULL)
		ops->update(purple_blist_get_buddy_list(), (PurpleBListNode *)buddy);
}

void
purple_presence_set_idle(PurplePresence *presence, gboolean idle, time_t idle_time)
{
	gboolean old_idle;
	time_t current_time;

	g_return_if_fail(presence != NULL);

	if (presence->idle == idle && presence->idle_time == idle_time)
		return;

	old_idle            = presence->idle;
	presence->idle      = idle;
	presence->idle_time = (idle ? idle_time : 0);

	current_time = time(NULL);

	if (purple_presence_get_context(presence) == PURPLE_PRESENCE_CONTEXT_BUDDY)
	{
		update_buddy_idle(purple_buddy_presence_get_buddy(presence), presence, current_time,
		                  old_idle, idle);
	}
	else if (purple_presence_get_context(presence) == PURPLE_PRESENCE_CONTEXT_ACCOUNT)
	{
		PurpleAccount *account;
		PurpleConnection *gc = NULL;
		PurplePlugin *prpl = NULL;
		PurplePluginProtocolInfo *prpl_info = NULL;

		account = purple_account_presence_get_account(presence);

		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *msg, *tmp;

				if (idle)
					tmp = g_strdup_printf(_("+++ %s became idle"), purple_account_get_username(account));
				else
					tmp = g_strdup_printf(_("+++ %s became unidle"), purple_account_get_username(account));

				msg = g_markup_escape_text(tmp, -1);
				g_free(tmp);
				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				                 purple_account_get_username(account),
				                 (idle ? idle_time : current_time), msg);
				g_free(msg);
			}
		}

		gc = purple_account_get_connection(account);

		if(gc)
			prpl = purple_connection_get_prpl(gc);

		if(PURPLE_CONNECTION_IS_CONNECTED(gc) && prpl != NULL)
			prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if (prpl_info && prpl_info->set_idle)
			prpl_info->set_idle(gc, (idle ? (current_time - idle_time) : 0));
	}
}

void
purple_presence_set_login_time(PurplePresence *presence, time_t login_time)
{
	g_return_if_fail(presence != NULL);

	if (presence->login_time == login_time)
		return;

	presence->login_time = login_time;
}

PurplePresenceContext
purple_presence_get_context(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, PURPLE_PRESENCE_CONTEXT_UNSET);

	return presence->context;
}

PurpleAccount *
purple_account_presence_get_account(const PurplePresence *presence)
{
	PurplePresenceContext context;

	g_return_val_if_fail(presence != NULL, NULL);

	context = purple_presence_get_context(presence);

	g_return_val_if_fail(context == PURPLE_PRESENCE_CONTEXT_ACCOUNT ||
			context == PURPLE_PRESENCE_CONTEXT_BUDDY, NULL);

	return presence->u.account;
}

PurpleConversation *
purple_conversation_presence_get_conversation(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(purple_presence_get_context(presence) ==
			PURPLE_PRESENCE_CONTEXT_CONV, NULL);

	return presence->u.chat.conv;
}

const char *
purple_presence_get_chat_user(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(purple_presence_get_context(presence) ==
			PURPLE_PRESENCE_CONTEXT_CONV, NULL);

	return presence->u.chat.user;
}

PurpleBuddy *
purple_buddy_presence_get_buddy(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);
	g_return_val_if_fail(purple_presence_get_context(presence) ==
			PURPLE_PRESENCE_CONTEXT_BUDDY, NULL);

	return presence->u.buddy.buddy;
}

GList *
purple_presence_get_statuses(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);

	return presence->statuses;
}

PurpleStatus *
purple_presence_get_status(const PurplePresence *presence, const char *status_id)
{
	PurpleStatus *status;
	GList *l = NULL;

	g_return_val_if_fail(presence  != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	/* What's the purpose of this hash table? */
	status = (PurpleStatus *)g_hash_table_lookup(presence->status_table,
						   status_id);

	if (status == NULL) {
		for (l = purple_presence_get_statuses(presence);
			 l != NULL && status == NULL; l = l->next)
		{
			PurpleStatus *temp_status = l->data;

			if (purple_strequal(status_id, purple_status_get_id(temp_status)))
				status = temp_status;
		}

		if (status != NULL)
			g_hash_table_insert(presence->status_table,
								g_strdup(purple_status_get_id(status)), status);
	}

	return status;
}

PurpleStatus *
purple_presence_get_active_status(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, NULL);

	return presence->active_status;
}

gboolean
purple_presence_is_available(const PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	status = purple_presence_get_active_status(presence);

	return ((status != NULL && purple_status_is_available(status)) &&
			!purple_presence_is_idle(presence));
}

gboolean
purple_presence_is_online(const PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence != NULL, FALSE);

	if ((status = purple_presence_get_active_status(presence)) == NULL)
		return FALSE;

	return purple_status_is_online(status);
}

gboolean
purple_presence_is_status_active(const PurplePresence *presence,
		const char *status_id)
{
	PurpleStatus *status;

	g_return_val_if_fail(presence  != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	status = purple_presence_get_status(presence, status_id);

	return (status != NULL && purple_status_is_active(status));
}

gboolean
purple_presence_is_status_primitive_active(const PurplePresence *presence,
		PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(presence  != NULL,              FALSE);
	g_return_val_if_fail(primitive != PURPLE_STATUS_UNSET, FALSE);

	for (l = purple_presence_get_statuses(presence);
	     l != NULL; l = l->next)
	{
		PurpleStatus *temp_status = l->data;
		PurpleStatusType *type = purple_status_get_type(temp_status);

		if (purple_status_type_get_primitive(type) == primitive &&
		    purple_status_is_active(temp_status))
			return TRUE;
	}
	return FALSE;
}

gboolean
purple_presence_is_idle(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, FALSE);

	return purple_presence_is_online(presence) && presence->idle;
}

time_t
purple_presence_get_idle_time(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return presence->idle_time;
}

time_t
purple_presence_get_login_time(const PurplePresence *presence)
{
	g_return_val_if_fail(presence != NULL, 0);

	return purple_presence_is_online(presence) ? presence->login_time : 0;
}

static int
purple_presence_compute_score(const PurplePresence *presence)
{
	GList *l;
	int score = 0;

	for (l = purple_presence_get_statuses(presence); l != NULL; l = l->next) {
		PurpleStatus *status = (PurpleStatus *)l->data;
		PurpleStatusType *type = purple_status_get_type(status);

		if (purple_status_is_active(status)) {
			score += primitive_scores[purple_status_type_get_primitive(type)];
			if (!purple_status_is_online(status)) {
				PurpleBuddy *b = purple_buddy_presence_get_buddy(presence);
				if (b && purple_account_supports_offline_message(purple_buddy_get_account(b), b))
					score += primitive_scores[SCORE_OFFLINE_MESSAGE];
			}
		}
	}
	score += purple_account_get_int(purple_account_presence_get_account(presence), "score", 0);
	if (purple_presence_is_idle(presence))
		score += primitive_scores[SCORE_IDLE];
	return score;
}

gint
purple_presence_compare(const PurplePresence *presence1,
		const PurplePresence *presence2)
{
	time_t idle_time_1, idle_time_2;
	int score1 = 0, score2 = 0;

	if (presence1 == presence2)
		return 0;
	else if (presence1 == NULL)
		return 1;
	else if (presence2 == NULL)
		return -1;

	if (purple_presence_is_online(presence1) &&
			!purple_presence_is_online(presence2))
		return -1;
	else if (purple_presence_is_online(presence2) &&
			!purple_presence_is_online(presence1))
		return 1;

	/* Compute the score of the first set of statuses. */
	score1 = purple_presence_compute_score(presence1);

	/* Compute the score of the second set of statuses. */
	score2 = purple_presence_compute_score(presence2);

	idle_time_1 = time(NULL) - purple_presence_get_idle_time(presence1);
	idle_time_2 = time(NULL) - purple_presence_get_idle_time(presence2);

	if (idle_time_1 > idle_time_2)
		score1 += primitive_scores[SCORE_IDLE_TIME];
	else if (idle_time_1 < idle_time_2)
		score2 += primitive_scores[SCORE_IDLE_TIME];

	if (score1 < score2)
		return 1;
	else if (score1 > score2)
		return -1;

	return 0;
}
