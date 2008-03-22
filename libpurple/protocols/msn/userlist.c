/**
 * @file userlist.c MSN user list support
 *
 * purple
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
#include "msn.h"
#include "userlist.h"

const char *lists[] = { "FL", "AL", "BL", "RL" };

typedef struct
{
	PurpleConnection *gc;
	char *who;
	char *friendly;

} MsnPermitAdd;

/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
msn_accept_add_cb(gpointer data)
{
	MsnPermitAdd *pa = data;

	purple_debug_misc("MSN Userlist", "Accepted the new buddy: %s\n", pa->who);

	if (PURPLE_CONNECTION_IS_VALID(pa->gc))
	{
		MsnSession *session = pa->gc->proto_data;
		MsnUserList *userlist = session->userlist;
		
		msn_userlist_add_buddy_to_list(userlist, pa->who, MSN_LIST_AL);

		msn_del_contact_from_list(session->contact, NULL, pa->who, MSN_LIST_PL);
	}

	g_free(pa->who);
	g_free(pa->friendly);
	g_free(pa);
}

static void
msn_cancel_add_cb(gpointer data)
{
	MsnPermitAdd *pa = data;

	purple_debug_misc("MSN Userlist", "Denied the new buddy: %s\n", pa->who);

	if (PURPLE_CONNECTION_IS_VALID(pa->gc))
	{
		MsnSession *session = pa->gc->proto_data;
		MsnUserList *userlist = session->userlist;
		MsnCallbackState *state = msn_callback_state_new(session);

		msn_callback_state_set_action(state, MSN_DENIED_BUDDY);

		msn_userlist_add_buddy_to_list(userlist, pa->who, MSN_LIST_BL);
		msn_del_contact_from_list(session->contact, state, pa->who, MSN_LIST_PL);
	}

	g_free(pa->who);
	g_free(pa->friendly);
	g_free(pa);
}

static void
got_new_entry(PurpleConnection *gc, const char *passport, const char *friendly)
{
	PurpleAccount *acct;
	MsnPermitAdd *pa;

	pa = g_new0(MsnPermitAdd, 1);
	pa->who = g_strdup(passport);
	pa->friendly = g_strdup(friendly);
	pa->gc = gc;

	acct = purple_connection_get_account(gc);
	purple_account_request_authorization(acct, passport, NULL, friendly, NULL,
										 purple_find_buddy(acct, passport) != NULL,
										 msn_accept_add_cb, msn_cancel_add_cb, pa);

}

/**************************************************************************
 * Utility functions
 **************************************************************************/

gboolean
msn_userlist_user_is_in_group(MsnUser *user, const char * group_id)
{
	if (user == NULL)
		return FALSE;

	if (group_id == NULL)
		return FALSE;

	if (g_list_find_custom(user->group_ids, group_id, (GCompareFunc)strcmp))
		return TRUE;

	return FALSE;
}

gboolean
msn_userlist_user_is_in_list(MsnUser *user, MsnListId list_id)
{
	int list_op;

	if (user == NULL)
		return FALSE;
	
	list_op = 1 << list_id;

	if (user->list_op & list_op)
		return TRUE;
	else
		return FALSE;
}

#if 0
static const char*
get_store_name(MsnUser *user)
{
	const char *store_name;

	g_return_val_if_fail(user != NULL, NULL);

	store_name = msn_user_get_store_name(user);

	if (store_name != NULL)
		store_name = purple_url_encode(store_name);
	else
		store_name = msn_user_get_passport(user);

	/* this might be a bit of a hack, but it should prevent notification server
	 * disconnections for people who have buddies with insane friendly names
	 * who added you to their buddy list from being disconnected. Stu. */
	/* Shx: What? Isn't the store_name obtained from the server, and hence it's
	 * below the BUDDY_ALIAS_MAXLEN ? */
	/* Stu: yeah, that's why it's a bit of a hack, as you pointed out, we're
	 * probably decoding the incoming store_name wrong, or something. bleh. */

	if (strlen(store_name) > BUDDY_ALIAS_MAXLEN)
		store_name = msn_user_get_passport(user);

	return store_name;
}
#endif

/**************************************************************************
 * Server functions
 **************************************************************************/

MsnListId
msn_get_list_id(const char *list)
{
	if (list[0] == 'F')
		return MSN_LIST_FL;
	else if (list[0] == 'A')
		return MSN_LIST_AL;
	else if (list[0] == 'B')
		return MSN_LIST_BL;
	else if (list[0] == 'R')
		return MSN_LIST_RL;

	return -1;
}

/* this function msn_got_add_user isn't called anywhere */
void
msn_got_add_user(MsnSession *session, MsnUser *user,
				 MsnListId list_id, const char * group_id)
{
	PurpleAccount *account;
	const char *passport;
	const char *friendly;

	purple_debug_info("MSNP14","got add user...\n");
	account = session->account;

	passport = msn_user_get_passport(user);
	friendly = msn_user_get_friendly_name(user);

	if (list_id == MSN_LIST_FL)
	{
		PurpleConnection *gc;

		gc = purple_account_get_connection(account);

		serv_got_alias(gc, passport, friendly);

		if (group_id != NULL)
		{
			msn_user_add_group_id(user, group_id);
		}
		else
		{
			/* session->sync->fl_users_count++; */
		}
	}
	else if (list_id == MSN_LIST_AL)
	{
		purple_privacy_permit_add(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_BL)
	{
		purple_privacy_deny_add(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_RL)
	{
		PurpleConversation *convo;

		purple_debug_info("msn",
						"%s has added you to his or her buddy list.\n",
						passport);

 		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, passport, account);
 		if (convo) {
 			PurpleBuddy *buddy;
 			char *msg;
 
 			buddy = purple_find_buddy(account, passport);
 			msg = g_strdup_printf(
 				_("%s has added you to his or her buddy list."),
 				buddy ? purple_buddy_get_contact_alias(buddy) : passport);
 			purple_conv_im_write(PURPLE_CONV_IM(convo), passport, msg,
 				PURPLE_MESSAGE_SYSTEM, time(NULL));
 			g_free(msg);
 		}
 
		if (!(user->list_op & (MSN_LIST_AL_OP | MSN_LIST_BL_OP)))
		{
			/*
			 * TODO: The friendly name was NULL for me when I
			 *       looked at this.  Maybe we should use the store
			 *       name instead? --KingAnt
			 */
//			got_new_entry(gc, passport, friendly);
		}
	}

	user->list_op |= (1 << list_id);
	/* purple_user_add_list_id (user, list_id); */
}

void
msn_got_rem_user(MsnSession *session, MsnUser *user,
				 MsnListId list_id, const char * group_id)
{
	PurpleAccount *account;
	const char *passport;

	account = session->account;

	passport = msn_user_get_passport(user);

	if (list_id == MSN_LIST_FL)
	{
		/* TODO: When is the user totally removed? */
		if (group_id != NULL)
		{
			msn_user_remove_group_id(user, group_id);
			return;
		}
		else
		{
			/* session->sync->fl_users_count--; */
		}
	}
	else if (list_id == MSN_LIST_AL)
	{
		purple_privacy_permit_remove(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_BL)
	{
		purple_privacy_deny_remove(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_RL)
	{
		PurpleConversation *convo;

		purple_debug_info("msn",
						"%s has removed you from his or her buddy list.\n",
						passport);

		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, passport, account);
		if (convo) {
			PurpleBuddy *buddy;
			char *msg;

			buddy = purple_find_buddy(account, passport);
			msg = g_strdup_printf(
				_("%s has removed you from his or her buddy list."),
				buddy ? purple_buddy_get_contact_alias(buddy) : passport);
			purple_conv_im_write(PURPLE_CONV_IM(convo), passport, msg,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(msg);
		}
	}

	user->list_op &= ~(1 << list_id);
	/* purple_user_remove_list_id (user, list_id); */

	if (user->list_op == 0)
	{
		purple_debug_info("msn", "Buddy '%s' shall be deleted?.\n",
						passport);
	}
}

void
msn_got_lst_user(MsnSession *session, MsnUser *user,
				 int list_op, GSList *group_ids)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	const char *passport;
	const char *store;

	account = session->account;
	gc = purple_account_get_connection(account);

	passport = msn_user_get_passport(user);
	store = msn_user_get_store_name(user);
	
	msn_user_set_op(user, list_op);

	if (list_op & MSN_LIST_FL_OP)
	{
		GSList *c;
		for (c = group_ids; c != NULL; c = g_slist_next(c))
		{
			char *group_id;
			group_id = c->data;
			msn_user_add_group_id(user, group_id);
		}

		/* FIXME: It might be a real alias */
		/* Umm, what? This might fix bug #1385130 */
		serv_got_alias(gc, passport, store);
	}

	if (list_op & MSN_LIST_AL_OP)
	{
		/* These are users who are allowed to see our status. */
		purple_privacy_deny_remove(account, passport, TRUE);
		purple_privacy_permit_add(account, passport, TRUE);
	}

	if (list_op & MSN_LIST_BL_OP)
	{
		/* These are users who are not allowed to see our status. */
		purple_privacy_permit_remove(account, passport, TRUE);
		purple_privacy_deny_add(account, passport, TRUE);
	}

	if (list_op & MSN_LIST_RL_OP)
	{
		/* These are users who have us on their buddy list. */
		/*
		 * TODO: What is store name set to when this happens?
		 *       For one of my accounts "something@hotmail.com"
		 *       the store name was "something."  Maybe we
		 *       should use the friendly name, instead? --KingAnt
		 */

		if (!(list_op & (MSN_LIST_AL_OP | MSN_LIST_BL_OP)))
		{
//			got_new_entry(gc, passport, store);
		}
	}

	if (list_op & MSN_LIST_PL_OP)
	{
		got_new_entry(gc, passport, store);
	}
}

/**************************************************************************
 * UserList functions
 **************************************************************************/

MsnUserList*
msn_userlist_new(MsnSession *session)
{
	MsnUserList *userlist;

	userlist = g_new0(MsnUserList, 1);

	userlist->session = session;
	userlist->buddy_icon_requests = g_queue_new();
	
	/* buddy_icon_window is the number of allowed simultaneous buddy icon requests.
	 * XXX With smarter rate limiting code, we could allow more at once... 5 was the limit set when
	 * we weren't retrieiving any more than 5 per MSN session. */
	userlist->buddy_icon_window = 1;

	return userlist;
}

void
msn_userlist_destroy(MsnUserList *userlist)
{
	GList *l;

	/*destroy userlist*/
	for (l = userlist->users; l != NULL; l = l->next)
	{
		msn_user_destroy(l->data);
	}
	g_list_free(userlist->users);

	/*destroy group list*/
	for (l = userlist->groups; l != NULL; l = l->next)
	{
		msn_group_destroy(l->data);
	}
	g_list_free(userlist->groups);

	g_queue_free(userlist->buddy_icon_requests);

	if (userlist->buddy_icon_request_timer)
		purple_timeout_remove(userlist->buddy_icon_request_timer);

	g_free(userlist);
}

MsnUser *
msn_userlist_find_add_user(MsnUserList *userlist,const char *passport,const char *userName)
{
	MsnUser *user;

	user = msn_userlist_find_user(userlist, passport);
	if (user == NULL)
	{
		user = msn_user_new(userlist, passport, userName);
		msn_userlist_add_user(userlist, user);
	} else {
		msn_user_set_store_name(user, userName);
	}
	return user;
}

void
msn_userlist_add_user(MsnUserList *userlist, MsnUser *user)
{
	userlist->users = g_list_prepend(userlist->users, user);
}

void
msn_userlist_remove_user(MsnUserList *userlist, MsnUser *user)
{
	userlist->users = g_list_remove(userlist->users, user);
}

MsnUser *
msn_userlist_find_user(MsnUserList *userlist, const char *passport)
{
	GList *l;

	g_return_val_if_fail(passport != NULL, NULL);

	for (l = userlist->users; l != NULL; l = l->next)
	{
		MsnUser *user = (MsnUser *)l->data;

//		purple_debug_info("MsnUserList","user passport:%s,passport:%s\n",user->passport,passport);
		g_return_val_if_fail(user->passport != NULL, NULL);

		if (!g_strcasecmp(passport, user->passport)){
//			purple_debug_info("MsnUserList","return:%p\n",user);
			return user;
		}
	}

	return NULL;
}

MsnUser *
msn_userlist_find_user_with_id(MsnUserList *userlist, const char *uid)
{
 	GList *l;

	g_return_val_if_fail(uid != NULL, NULL);

	for (l = userlist->users; l != NULL; l = l->next) {
		MsnUser *user = (MsnUser *)l->data;

		if (user->uid == NULL) {
			continue;
		}

		if ( !g_strcasecmp(uid, user->uid) ) {
			return user;
		}
	}

	return NULL;
}

MsnUser *
msn_userlist_find_user_with_mobile_phone(MsnUserList *userlist, const char *number)
{
	GList *l;

	g_return_val_if_fail(number != NULL, NULL);

	for (l = userlist->users; l != NULL; l = l->next) {
		MsnUser *user = (MsnUser *)l->data;

		if (user->phone.mobile == NULL) {
			continue;
		}

		if (!g_strcasecmp(number, user->phone.mobile)) {
			return user;
		}
	}

	return NULL;
}

void
msn_userlist_add_group(MsnUserList *userlist, MsnGroup *group)
{
	userlist->groups = g_list_append(userlist->groups, group);
}

void
msn_userlist_remove_group(MsnUserList *userlist, MsnGroup *group)
{
	userlist->groups = g_list_remove(userlist->groups, group);
}

MsnGroup *
msn_userlist_find_group_with_id(MsnUserList *userlist, const char * id)
{
	GList *l;

	g_return_val_if_fail(userlist != NULL, NULL);
	g_return_val_if_fail(id       != NULL, NULL);

	for (l = userlist->groups; l != NULL; l = l->next)
	{
		MsnGroup *group = l->data;

		if (!g_strcasecmp(group->id,id))
			return group;
	}

	return NULL;
}

MsnGroup *
msn_userlist_find_group_with_name(MsnUserList *userlist, const char *name)
{
	GList *l;

	g_return_val_if_fail(userlist != NULL, NULL);
	g_return_val_if_fail(name     != NULL, NULL);

	for (l = userlist->groups; l != NULL; l = l->next)
	{
		MsnGroup *group = l->data;

		if ((group->name != NULL) && !g_strcasecmp(name, group->name))
			return group;
	}

	return NULL;
}

const char *
msn_userlist_find_group_id(MsnUserList *userlist, const char *group_name)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_name(userlist, group_name);

	if (group != NULL)
		return msn_group_get_id(group);
	else
		return NULL;
}

const char *
msn_userlist_find_group_name(MsnUserList *userlist, const char * group_id)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
		return msn_group_get_name(group);
	else
		return NULL;
}

void
msn_userlist_rename_group_id(MsnUserList *userlist, const char * group_id,
							 const char *new_name)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
		msn_group_set_name(group, new_name);
}

void
msn_userlist_remove_group_id(MsnUserList *userlist, const char * group_id)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
	{
		msn_userlist_remove_group(userlist, group);
		msn_group_destroy(group);
	}
}

void
msn_userlist_rem_buddy(MsnUserList *userlist, const char *who)
{
	MsnUser *user = NULL;
	
	g_return_if_fail(userlist != NULL);
	g_return_if_fail(userlist->session != NULL);
	g_return_if_fail(userlist->session->contact != NULL);
	g_return_if_fail(who != NULL);
	
	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_FL);

	/* delete the contact from address book via soap action */
	if (user != NULL) {
		msn_delete_contact(userlist->session->contact, user->uid);
	}
}

void
msn_userlist_rem_buddy_from_list(MsnUserList *userlist, const char *who,
				 MsnListId list_id)
{
	MsnUser *user;
	const gchar *list;
	MsnListOp list_op = 1 << list_id;

	user = msn_userlist_find_user(userlist, who);
	
	g_return_if_fail(user != NULL);
	
	if ( !msn_userlist_user_is_in_list(user, list_id)) {
		list = lists[list_id];
		purple_debug_info("MSN Userlist", "User %s is not in list %s, not removing.\n", who, list);
		return;
	}

	msn_user_unset_op(user, list_op);

	msn_notification_rem_buddy_from_list(userlist->session->notification, list_id, who);
}

/*add buddy*/
void
msn_userlist_add_buddy(MsnUserList *userlist, const char *who, const char *group_name)
{
	MsnUser *user;
	MsnCallbackState *state = NULL;
	const char *group_id = NULL, *new_group_name;
	
	new_group_name = group_name == NULL ? MSN_INDIVIDUALS_GROUP_NAME : group_name;

	
	g_return_if_fail(userlist != NULL);
	g_return_if_fail(userlist->session != NULL);

	
	purple_debug_info("MSN Userlist", "Add user: %s to group: %s\n", who, new_group_name);

	state = msn_callback_state_new(userlist->session);
	msn_callback_state_set_who(state, who);
	msn_callback_state_set_new_group_name(state, new_group_name);

	if (!purple_email_is_valid(who))
	{
		/* only notify the user about problems adding to the friends list
		 * maybe we should do something else for other lists, but it probably
		 * won't cause too many problems if we just ignore it */
		
		char *str = g_strdup_printf(_("Unable to add \"%s\"."), who);
		
		purple_notify_error(NULL, NULL, str,
				  _("The screen name specified is invalid."));
		g_free(str);

		return;
	}

	group_id = msn_userlist_find_group_id(userlist, new_group_name);

	if (group_id == NULL)
	{
		/* Whoa, we must add that group first. */
		purple_debug_info("MSN Userlist", "Adding user %s to a new group, creating group %s first\n", who, new_group_name);
		
		msn_callback_state_set_action(state, MSN_ADD_BUDDY);

		msn_add_group(userlist->session, state, new_group_name);
		return;
	} else {
		msn_callback_state_set_guid(state, group_id);
	}
	
	/* XXX: adding user here may not be correct (should add them in the
 	 * ACK to the ADL command), but for now we need to make sure they exist  
	 * early enough that the ILN command doesn't screw us up */

	user = msn_userlist_find_add_user(userlist, who, who);

	if ( msn_userlist_user_is_in_list(user, MSN_LIST_FL) ) {

		purple_debug_info("MSN Userlist", "User %s already exists\n", who);

		msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_BL);

		if (msn_userlist_user_is_in_group(user, group_id)) {
			purple_debug_info("MSN Userlist", "User %s is already in group %s, returning\n", who, new_group_name);
			return;
		}
	}
			
	purple_debug_info("MSN Userlist", "Adding user: %s to group id: %s\n", who, group_id);

	msn_callback_state_set_action(state, MSN_ADD_BUDDY);

	/* Add contact in the Contact server with a SOAP request and if
	   successful, send ADL with MSN_LIST_AL and MSN_LIST_FL and a FQY */
	msn_add_contact_to_group(userlist->session->contact, state, who, group_id);
}

void
msn_userlist_add_buddy_to_list(MsnUserList *userlist, const char *who, 
							MsnListId list_id)
{
	MsnUser *user = NULL;
	const gchar *list;
	MsnListOp list_op = 1 << list_id;

	g_return_if_fail(userlist != NULL);
	
	user = msn_userlist_find_add_user(userlist, who, who);
	
	/* First we're going to check if it's already there. */
	if (msn_userlist_user_is_in_list(user, list_id))
	{
		list = lists[list_id];
		purple_debug_info("MSN Userlist", "User '%s' is already in list: %s\n", who, list);
		return;
	}
	
	//store_name = (user != NULL) ? get_store_name(user) : who;
	
	//purple_debug_info("MSN Userlist", "store_name = %s\n", store_name);
	
	/* XXX: see XXX above, this should really be done when we get the response from
		the server */
	
	msn_user_set_op(user, list_op);
	
	msn_notification_add_buddy_to_list(userlist->session->notification, list_id, who);
}

gboolean
msn_userlist_add_buddy_to_group(MsnUserList *userlist, const char *who,
				const char *group_name)
{
	MsnUser *user;
	gchar * group_id;
	
	g_return_val_if_fail(userlist != NULL, FALSE);
	g_return_val_if_fail(group_name != NULL, FALSE);
	g_return_val_if_fail(who != NULL, FALSE);

	purple_debug_info("MSN Userlist","Adding buddy with passport %s to group %s\n", who, group_name);

	if ( (group_id = (gchar *)msn_userlist_find_group_id(userlist, group_name)) == NULL) {
		purple_debug_error("MSN Userlist", "Group %s has no guid!\n", group_name);
		return FALSE;
	}

	if ( (user = msn_userlist_find_user(userlist, who)) == NULL) {
		purple_debug_error("MSN Userlist", "User %s not found!", who);
		return FALSE;
	}
	
	msn_user_add_group_id(user, group_id);

	return TRUE;
}


gboolean
msn_userlist_rem_buddy_from_group(MsnUserList *userlist, const char *who,
				const char *group_name)
{
	const gchar * group_id;
	MsnUser *user;

	g_return_val_if_fail(userlist != NULL, FALSE);
	g_return_val_if_fail(group_name != NULL, FALSE);
	g_return_val_if_fail(who != NULL, FALSE);
	
	purple_debug_info("MSN Userlist","Removing buddy with passport %s from group %s\n", who, group_name);

	if ( (group_id = msn_userlist_find_group_id(userlist, group_name)) == NULL) {
		purple_debug_error("MSN Userlist", "Group %s has no guid!\n", group_name);
		return FALSE;
	}

	if ( (user = msn_userlist_find_user(userlist, who)) == NULL) {
		purple_debug_error("MSN Userlist", "User %s not found!", who);
		return FALSE;
	}

	msn_user_remove_group_id(user, group_id);

	return TRUE;
}

void
msn_userlist_move_buddy(MsnUserList *userlist, const char *who,
			const char *old_group_name, const char *new_group_name)
{
	const char *new_group_id;
	MsnCallbackState *state;
	
	g_return_if_fail(userlist != NULL);
	g_return_if_fail(userlist->session != NULL);
	g_return_if_fail(userlist->session->contact != NULL);

	state = msn_callback_state_new(userlist->session);
	msn_callback_state_set_who(state, who);
	msn_callback_state_set_action(state, MSN_MOVE_BUDDY);
	msn_callback_state_set_old_group_name(state, old_group_name);
	msn_callback_state_set_new_group_name(state, new_group_name);

	new_group_id = msn_userlist_find_group_id(userlist, new_group_name);

	if (new_group_id == NULL)
	{		
		msn_add_group(userlist->session, state, new_group_name);
		return;
	}
	
	/* add the contact to the new group, and remove it from the old one in
	 * the callback
	*/
	msn_add_contact_to_group(userlist->session->contact, state, who, new_group_id);
}

/*load userlist from the Blist file cache*/
void
msn_userlist_load(MsnSession *session)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleConnection *gc = purple_account_get_connection(session->account);
	GSList *l;
	MsnUser * user;

	g_return_if_fail(gc != NULL);

	for (gnode = purple_get_blist()->root; gnode; gnode = gnode->next)
	{
		if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next)
		{
			if (!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for (bnode = cnode->child; bnode; bnode = bnode->next)
			{
				PurpleBuddy *b;
				if (!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				b = (PurpleBuddy *)bnode;
				if (b->account == gc->account)
				{
					user = msn_userlist_find_add_user(session->userlist,
						b->name,NULL);
					b->proto_data = user;
					msn_user_set_op(user, MSN_LIST_FL_OP);
				}
			}
		}
	}
	for (l = session->account->permit; l != NULL; l = l->next)
	{
		user = msn_userlist_find_add_user(session->userlist,
						(char *)l->data,NULL);
		msn_user_set_op(user, MSN_LIST_AL_OP);
	}
	for (l = session->account->deny; l != NULL; l = l->next)
	{
		user = msn_userlist_find_add_user(session->userlist,
						(char *)l->data,NULL);
		msn_user_set_op(user, MSN_LIST_BL_OP);
	}
	
}

