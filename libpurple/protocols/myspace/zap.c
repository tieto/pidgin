/* MySpaceIM Protocol Plugin - zap support
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@soc.pidgin.im>
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

#include "myspace.h"
#include "zap.h"

static gboolean msim_send_zap(MsimSession *session, const gchar *username, guint code);
static void msim_send_zap_from_menu(PurpleBlistNode *node, gpointer zap_num_ptr);


/** Get zap types. */
GList *
msim_attention_types(PurpleAccount *acct)
{
	static GList *types = NULL;
	MsimAttentionType* attn;

	if (!types) {
#define _MSIM_ADD_NEW_ATTENTION(icn, nme, incoming, outgoing)              \
		attn = g_new0(MsimAttentionType, 1);                       \
		attn->icon_name = icn;                                     \
		attn->name = nme;                                          \
		attn->incoming_description = incoming;                     \
		attn->outgoing_description = outgoing;                     \
		types = g_list_append(types, attn);

		/* TODO: icons for each zap */
		_MSIM_ADD_NEW_ATTENTION(NULL, _("zap"), _("zapped"), _("Zapping"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("whack"), _("whacked"), _("Whacking"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("torch"), _("torched"), _("Torching"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("smooch"), _("smooched"), _("Smooching"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("hug"), _("hugged"), _("Hugging"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("bslap"), _("bslapped"), _("Bslapping"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("goose"), _("goosed"), _("Goosing"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("hi-five"), _("hi-fived"), _("Hi-fiving"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("punk"), _("punk'd"), _("Punking"));
		_MSIM_ADD_NEW_ATTENTION(NULL, _("raspberry"), _("raspberried"), _("Raspberry'ing"));
	}

	return types;
}

/** Send a zap */
gboolean
msim_send_attention(PurpleConnection *gc, const gchar *username, guint code)
{
	GList *types;
	MsimSession *session;
	MsimAttentionType *attn;
	PurpleBuddy *buddy;

	session = (MsimSession *)gc->proto_data;

	/* Look for this attention type, by the code index given. */
	types = msim_attention_types(gc->account);
	attn = (MsimAttentionType *)g_list_nth_data(types, code);

	if (!attn) {
		purple_debug_info("msim_send_attention", "got invalid zap code %d\n", code);
		return FALSE;
	}

	buddy = purple_find_buddy(session->account, username);
	if (!buddy) {
		return FALSE;
	}

	/* TODO: make use of the MsimAttentionType we found, instead of
	 * doing it all over in msim_send_zap_from_menu. */
	msim_send_zap_from_menu(&buddy->node, GUINT_TO_POINTER(code));

	return TRUE;
}

/** Send a zap to a user. */
static gboolean
msim_send_zap(MsimSession *session, const gchar *username, guint code)
{
	gchar *zap_string;
#ifndef MSIM_USE_ATTENTION_API
	gchar *zap_description;
#endif
	GList *types;
	MsimAttentionType *attn;
	gboolean rc;

	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(username != NULL, FALSE);

	types = msim_attention_types(session->account);

	attn = g_list_nth_data(types, code);
	if (!attn) {
		return FALSE;
	}


#ifdef MSIM_USE_ATTENTION_API
	serv_got_attention(session->gc, username, attn, FALSE);
#else
	zap_description = g_strdup_printf("*** Attention: %s %s ***", attn->outgoing_description,
			username);

	serv_got_im(session->gc, username, zap_description,
			PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_SYSTEM, time(NULL));

	g_free(zap_description);
#endif

	/* Construct and send the actual zap command. */
	zap_string = g_strdup_printf("!!!ZAP_SEND!!!=RTE_BTN_ZAPS_%d", code);

	if (!msim_send_bm(session, username, zap_string, MSIM_BM_ACTION)) {
		purple_debug_info("msim_send_zap_from_menu", "msim_send_bm failed: zapping %s with %s",
				username, zap_string);
		rc = FALSE;
	} else {
		rc = TRUE;
	}
	
	g_free(zap_string);

	return rc;

}

/** Zap someone. Callback from msim_blist_node_menu zap menu. */
static void
msim_send_zap_from_menu(PurpleBlistNode *node, gpointer zap_num_ptr)
{
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleConnection *gc;
	MsimSession *session;
	guint zap;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		/* Only know about buddies for now. */
		return;
	}

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *)node;

	/* Find the session */
	account = buddy->account;
	gc = purple_account_get_connection(account);
	session = (MsimSession *)gc->proto_data;

	zap = GPOINTER_TO_INT(zap_num_ptr);

	g_return_if_fail(msim_send_zap(session, buddy->name, zap));
}

/** Return menu, if any, for a buddy list node. */
GList *
msim_blist_node_menu(PurpleBlistNode *node)
{
	GList *menu, *zap_menu;
	GList *types;
	PurpleMenuAction *act;
	/* Warning: hardcoded to match that in msim_attention_types. */
	const gchar *zap_names[10];
	guint i;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		/* Only know about buddies for now. */
		return NULL;
	}

	/* Names from official client. */
	types = msim_attention_types(NULL);
	i = 0;
	do
	{
		MsimAttentionType *attn;

		attn = (MsimAttentionType *)types->data;
		zap_names[i] = attn->name;
		++i;
	} while ((types = g_list_next(types)));

	menu = zap_menu = NULL;

	/* TODO: get rid of once is accessible directly in GUI */
	for (i = 0; i < sizeof(zap_names) / sizeof(zap_names[0]); ++i) {
		act = purple_menu_action_new(zap_names[i], PURPLE_CALLBACK(msim_send_zap_from_menu),
				GUINT_TO_POINTER(i), NULL);
		zap_menu = g_list_append(zap_menu, act);
	}

	act = purple_menu_action_new(_("Zap"), NULL, NULL, zap_menu);
	menu = g_list_append(menu, act);

	return menu;
}

/** Process an incoming zap. */
gboolean
msim_incoming_zap(MsimSession *session, MsimMessage *msg)
{
	gchar *msg_text, *username;
	gint zap;
	const gchar *zap_past_tense[10];
#ifdef MSIM_USE_ATTENTION_API
	MsimAttentionType attn;
#else
	gchar *zap_text;
#endif

	zap_past_tense[0] = _("zapped");
	zap_past_tense[1] = _("whacked");
	zap_past_tense[2] = _("torched");
	zap_past_tense[3] = _("smooched");
	zap_past_tense[4] = _("hugged");
	zap_past_tense[5] = _("bslapped");
	zap_past_tense[6] = _("goosed");
	zap_past_tense[7] = _("hi-fived");
	zap_past_tense[8] = _("punk'd");
	zap_past_tense[9] = _("raspberried");

	msg_text = msim_msg_get_string(msg, "msg");
	username = msim_msg_get_string(msg, "_username");

	g_return_val_if_fail(msg_text != NULL, FALSE);
	g_return_val_if_fail(username != NULL, FALSE);

	g_return_val_if_fail(sscanf(msg_text, "!!!ZAP_SEND!!!=RTE_BTN_ZAPS_%d", &zap) == 1, FALSE);

	zap = CLAMP(zap, 0, sizeof(zap_past_tense) / sizeof(zap_past_tense[0]));

	/* TODO:ZAP: use msim_attention_types */
#ifdef MSIM_USE_ATTENTION_API
	attn.incoming_description = zap_past_tense[zap];
	attn.outgoing_description = NULL;
	attn.icon_name = NULL;		/* TODO: icon */

	serv_got_attention(session->gc, username, &attn, TRUE);
#else
	zap_text = g_strdup_printf(_("*** You have been %s! ***"), zap_past_tense[zap]);
	serv_got_im(session->gc, username, zap_text, 
			PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, time(NULL));
	g_free(zap_text);
#endif

	g_free(msg_text);
	g_free(username);

	return TRUE;
}


