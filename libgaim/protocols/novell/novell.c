/*
 * novell.c
 *
 * Copyright (c) 2004 Novell, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 *
 */

#include "internal.h"
#include "accountopt.h"
#include "debug.h"
#include "prpl.h"
#include "server.h"
#include "nmuser.h"
#include "notify.h"
#include "util.h"
#include "sslconn.h"
#include "request.h"
#include "network.h"
#include "privacy.h"
#include "status.h"
#include "version.h"

#define DEFAULT_PORT			8300
#define NOVELL_CONNECT_STEPS	4
#define NM_ROOT_FOLDER_NAME "GroupWise Messenger"

#define NOVELL_STATUS_TYPE_AVAILABLE "available"
#define NOVELL_STATUS_TYPE_AWAY "away"
#define NOVELL_STATUS_TYPE_BUSY "busy"
#define NOVELL_STATUS_TYPE_OFFLINE "offline"
#define NOVELL_STATUS_TYPE_IDLE "idle"
#define NOVELL_STATUS_TYPE_APPEAR_OFFLINE "appearoffline"

static GaimPlugin *my_protocol = NULL;

static gboolean
_is_disconnect_error(NMERR_T err);

static gboolean
_check_for_disconnect(NMUser * user, NMERR_T err);

static void
_send_message(NMUser * user, NMMessage * message);

static void
_update_buddy_status(NMUser *user, GaimBuddy * buddy, int status, int gmt);

static void
_remove_gaim_buddies(NMUser * user);

static void
_add_contacts_to_gaim_blist(NMUser * user, NMFolder * folder);

static void
_add_gaim_buddies(NMUser * user);

static void
_sync_contact_list(NMUser *user);

static void
_sync_privacy_lists(NMUser *user);

static void
_show_info(GaimConnection * gc, NMUserRecord * user_record);

const char *
_get_conference_name(int id);

/*******************************************************************************
 * Response callbacks
 *******************************************************************************/

/* Handle login response */
static void
_login_resp_cb(NMUser * user, NMERR_T ret_code,
			   gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	const char *alias;
	NMERR_T rc;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);
	if (gc == NULL)
		return;

	if (ret_code == NM_OK) {

		/* Set alias for user if not set (use Full Name) */
		alias = gaim_account_get_alias(user->client_data);
		if (alias == NULL || *alias == '\0') {
			alias = nm_user_record_get_full_name(user->user_record);

			if (alias)
				gaim_account_set_alias(user->client_data, alias);
		}

		/* Tell Gaim that we are connected */
		gaim_connection_set_state(gc, GAIM_CONNECTED);

		_sync_contact_list(user);

		rc = nm_send_set_status(user, NM_STATUS_AVAILABLE, NULL, NULL, NULL,
								NULL);
		_check_for_disconnect(user, rc);

	} else {

		char *err = g_strdup_printf(_("Login failed (%s)."),
					    nm_error_to_string (ret_code));

		/* Don't attempt to auto-reconnect if our password
		 * was invalid.
		 */
		if (ret_code == NMERR_AUTHENTICATION_FAILED ||
			ret_code == NMERR_CREDENTIALS_MISSING ||
			ret_code == NMERR_PASSWORD_INVALID) {
			gc->wants_to_die = TRUE;
		}
		gaim_connection_error(gc, err);
		g_free(err);
	}
}

/* Handle getstatus response*/
static void
_get_status_resp_cb(NMUser * user, NMERR_T ret_code,
					gpointer resp_data, gpointer user_data)
{
	GaimBuddy *buddy;
	GSList *buddies;
	GSList *bnode;
	NMUserRecord *user_record = (NMUserRecord *) resp_data;
	int status;

	if (user == NULL || user_record == NULL)
		return;

	if (ret_code == NM_OK) {

		/* Find all Gaim buddies and update their statuses */
		const char *name = nm_user_record_get_display_id(user_record);

		if (name) {
			buddies = gaim_find_buddies((GaimAccount *) user->client_data, name);
			for (bnode = buddies; bnode; bnode = bnode->next) {
				buddy = (GaimBuddy *) bnode->data;
				if (buddy) {
					status = nm_user_record_get_status(user_record);
					_update_buddy_status(user, buddy, status, time(0));
				}
			}
			g_slist_free(buddies);
		}

	} else {

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_get_status_resp_cb(): rc = 0x%X\n", ret_code);

	}
}

/* Show an error if the rename failed */
static void
_rename_contact_resp_cb(NMUser * user, NMERR_T ret_code,
						gpointer resp_data, gpointer user_data)
{
	if (ret_code != NM_OK) {
		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_rename_contact_resp_cb(): rc = 0x%X\n", ret_code);
	}
}

/* Handle the getdetails response and send the message */
static void
_get_details_resp_send_msg(NMUser * user, NMERR_T ret_code,
						   gpointer resp_data, gpointer user_data)
{
	GaimConversation *gconv;
	GaimConnection *gc;
	NMUserRecord *user_record = NULL;
	NMContact *cntct = NULL;
	NMConference *conf;
	NMMessage *msg = user_data;
	const char *dn = NULL;
	const char *name;

	if (user == NULL || msg == NULL)
		return;

	if (ret_code == NM_OK) {
		user_record = (NMUserRecord *) resp_data;
		if (user_record) {

			/* Set the title for the conversation */
			/* XXX - Should this be GAIM_CONV_TYPE_IM? */
			gconv =	gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY,
														nm_user_record_get_display_id(user_record),
														(GaimAccount *) user->client_data);
			if (gconv) {

				dn = nm_user_record_get_dn(user_record);
				if (dn) {
					cntct = nm_find_contact(user, dn);
				}

				if (cntct) {
					gaim_conversation_set_title(gconv,
												nm_contact_get_display_name(cntct));
				} else {

					/* Not in the contact list, try to user full name */
					name = (char *) nm_user_record_get_full_name(user_record);
					if (name)
						gaim_conversation_set_title(gconv, name);
				}
			}

			/* Add the user record to particpant list */
			conf = nm_message_get_conference(msg);
			if (conf) {
				nm_conference_add_participant(conf, user_record);
				_send_message(user, msg);
			}
		}

	} else {

		gc = gaim_account_get_connection(user->client_data);
		if (gc != NULL) {
			char *err = g_strdup_printf(_("Unable to send message."
										  " Could not get details for user (%s)."),
						    nm_error_to_string (ret_code));

			gaim_notify_error(gc, NULL, err, NULL);
			g_free(err);
		}

		if (msg)
			nm_release_message(msg);
	}
}

/* Set up the new GaimBuddy based on the response from getdetails */
static void
_get_details_resp_setup_buddy(NMUser * user, NMERR_T ret_code,
							  gpointer resp_data, gpointer user_data)
{
	NMUserRecord *user_record;
	NMContact *contact;
	GaimBuddy *buddy;
	const char *alias;
	NMERR_T rc = NM_OK;

	if (user == NULL || resp_data == NULL || user_data == NULL)
		return;

	contact = user_data;

	if (ret_code == NM_OK) {
		user_record = resp_data;

		buddy = nm_contact_get_data(contact);

		nm_contact_set_user_record(contact, user_record);

		/* Set the display id */
		gaim_blist_rename_buddy(buddy,
								nm_user_record_get_display_id(user_record));

		alias = gaim_buddy_get_alias(buddy);
		if (alias == NULL || *alias == '\0' || (strcmp(alias, buddy->name) == 0)) {
			gaim_blist_alias_buddy(buddy,
								   nm_user_record_get_full_name(user_record));

			/* Tell the server about the new display name */
			rc = nm_send_rename_contact(user, contact,
										nm_user_record_get_full_name(user_record),
										NULL, NULL);
			_check_for_disconnect(user, rc);

		}


		/* Get initial status for the buddy */
		rc = nm_send_get_status(user, resp_data, _get_status_resp_cb, NULL);
		_check_for_disconnect(user, rc);

/*		nm_release_contact(contact);*/

	}

	if (contact)
		nm_release_contact(contact);
}

/* Add the new contact into the GaimBuddy list */
static void
_create_contact_resp_cb(NMUser * user, NMERR_T ret_code,
						gpointer resp_data, gpointer user_data)
{
	NMContact *tmp_contact = (NMContact *) user_data;
	NMContact *new_contact = NULL;
	NMFolder *folder = NULL;
	GaimGroup *group;
	GaimBuddy *buddy;
	const char *folder_name = NULL;
	NMERR_T rc = NM_OK;

	if (user == NULL)
		return;

	if (ret_code == NM_OK) {

		new_contact = (NMContact *) resp_data;
		if (new_contact == NULL || tmp_contact == NULL)
			return;

		/* Get the userid and folder name for the new contact */
		folder = nm_find_folder_by_id(user,
									  nm_contact_get_parent_id(new_contact));
		if (folder) {
			folder_name = nm_folder_get_name(folder);
		}

		if (folder_name == NULL || *folder_name == '\0')
			folder_name = NM_ROOT_FOLDER_NAME;

		/* Re-add the buddy now that we got the okay from the server */
		if (folder_name && (group = gaim_find_group(folder_name))) {

			const char *alias = nm_contact_get_display_name(tmp_contact);
			const char *display_id = nm_contact_get_display_id(new_contact);

			if (display_id == NULL)
				display_id = nm_contact_get_dn(new_contact);

			if (alias && strcmp(alias, display_id)) {

				/* The user requested an alias, tell the server about it. */
				rc = nm_send_rename_contact(user, new_contact, alias,
											_rename_contact_resp_cb, NULL);
				_check_for_disconnect(user, rc);

			} else {

				alias = "";

			}

			/* Add it to the gaim buddy list if it is not there */
			buddy = gaim_find_buddy_in_group(user->client_data, display_id, group);
			if (buddy == NULL) {
				buddy = gaim_buddy_new(user->client_data, display_id, alias);
				gaim_blist_add_buddy(buddy, NULL, group, NULL);
			}

			/* Save the new buddy as part of the contact object */
			nm_contact_set_data(new_contact, (gpointer) buddy);

			/* We need details for the user before we can setup the
			 * new Gaim buddy. We always call this because the
			 * 'createcontact' response fields do not always contain
			 * everything that we need.
			 */
			nm_contact_add_ref(new_contact);

			rc = nm_send_get_details(user, nm_contact_get_dn(new_contact),
									 _get_details_resp_setup_buddy, new_contact);
			_check_for_disconnect(user, rc);

		}

	} else {
		GaimConnection *gc = gaim_account_get_connection(user->client_data);
		const char *name = nm_contact_get_dn(tmp_contact);
		char *err;

		err =
			g_strdup_printf(_("Unable to add %s to your buddy list (%s)."),
					name, nm_error_to_string (ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}

	if (tmp_contact)
		nm_release_contact(tmp_contact);
}

/* Show an error if we failed to send the message */
static void
_send_message_resp_cb(NMUser * user, NMERR_T ret_code,
					  gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	char *err = NULL;

	if (user == NULL)
		return;

	if (ret_code != NM_OK) {
		gc = gaim_account_get_connection(user->client_data);

		/* TODO: Improve this! message to who or for what conference? */
		err = g_strdup_printf(_("Unable to send message (%s)."),
				      nm_error_to_string (ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);
	}
}

/* Show an error if the remove failed */
static void
_remove_contact_resp_cb(NMUser * user, NMERR_T ret_code,
						gpointer resp_data, gpointer user_data)
{
	if (ret_code != NM_OK) {
		/* TODO: Display an error? */

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_remove_contact_resp_cb(): rc = 0x%x\n", ret_code);
	}
}

/* Show an error if the remove failed */
static void
_remove_folder_resp_cb(NMUser * user, NMERR_T ret_code,
					   gpointer resp_data, gpointer user_data)
{
	if (ret_code != NM_OK) {
		/* TODO: Display an error? */

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_remove_folder_resp_cb(): rc = 0x%x\n", ret_code);
	}
}

/* Show an error if the move failed */
static void
_move_contact_resp_cb(NMUser * user, NMERR_T ret_code,
					  gpointer resp_data, gpointer user_data)
{
	if (ret_code != NM_OK) {
		/* TODO: Display an error? */

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_move_contact_resp_cb(): rc = 0x%x\n", ret_code);
	}
}

/* Show an error if the rename failed */
static void
_rename_folder_resp_cb(NMUser * user, NMERR_T ret_code,
					   gpointer resp_data, gpointer user_data)
{
	if (ret_code != NM_OK) {
		/* TODO: Display an error? */

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_rename_folder_resp_cb(): rc = 0x%x\n", ret_code);
	}
}

static void
_sendinvite_resp_cb(NMUser *user, NMERR_T ret_code,
					gpointer resp_data, gpointer user_data)
{
	char *err;
	GaimConnection *gc;

	if (user == NULL)
		return;

	if (ret_code != NM_OK) {
		gc = gaim_account_get_connection(user->client_data);
		err = g_strdup_printf(_("Unable to invite user (%s)."), nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "_sendinvite_resp_cb(): rc = 0x%x\n", ret_code);
	}

}

/* If the createconf was successful attempt to send the message,
 * otherwise display an error message to the user.
 */
static void
_createconf_resp_send_msg(NMUser * user, NMERR_T ret_code,
						  gpointer resp_data, gpointer user_data)
{
	NMConference *conf;
	NMMessage *msg = user_data;

	if (user == NULL || msg == NULL)
		return;

	if (ret_code == NM_OK) {
		_send_message(user, msg);
	} else {

		if ((conf = nm_message_get_conference(msg))) {

			GaimConnection *gc = gaim_account_get_connection(user->client_data);
			const char *name = NULL;
			char *err;
			NMUserRecord *ur;

			ur = nm_conference_get_participant(conf, 0);
			if (ur)
				name = nm_user_record_get_userid(ur);

			if (name)
				err = g_strdup_printf(_("Unable to send message to %s."
										" Could not create the conference (%s)."),
						      name,
						      nm_error_to_string (ret_code));
			else
				err = g_strdup_printf(_("Unable to send message."
										" Could not create the conference (%s)."),
						      nm_error_to_string (ret_code));

			gaim_notify_error(gc, NULL, err, NULL);
			g_free(err);
		}

		if (msg)
			nm_release_message(msg);
	}
}

/* Move contact to newly created folder */
static void
_create_folder_resp_move_contact(NMUser * user, NMERR_T ret_code,
								 gpointer resp_data, gpointer user_data)
{
	NMContact *contact = user_data;
	NMFolder *new_folder;
	char *folder_name = resp_data;
	NMERR_T rc = NM_OK;

	if (user == NULL || folder_name == NULL || contact == NULL) {

		if (folder_name)
			g_free(folder_name);

		return;
	}

	if (ret_code == NM_OK || ret_code == NMERR_DUPLICATE_FOLDER) {
		new_folder = nm_find_folder(user, folder_name);
		if (new_folder) {

			/* Tell the server to move the contact to the new folder */
/*			rc = nm_send_move_contact(user, contact, new_folder,
			_move_contact_resp_cb, NULL); */

			rc = nm_send_create_contact(user, new_folder, contact,
										NULL, NULL);

			_check_for_disconnect(user, rc);

		}
	} else {
		GaimConnection *gc = gaim_account_get_connection(user->client_data);
		char *err = g_strdup_printf(_("Unable to move user %s"
									  " to folder %s in the server side list."
									  " Error while creating folder (%s)."),
					    nm_contact_get_dn(contact),
					    folder_name,
					    nm_error_to_string (ret_code));

		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);
	}

	if (folder_name)
		g_free(folder_name);
}

/* Add contact to newly create folder */
static void
_create_folder_resp_add_contact(NMUser * user, NMERR_T ret_code,
								gpointer resp_data, gpointer user_data)
{
	NMContact *contact = (NMContact *) user_data;
	NMFolder *folder;
	char *folder_name = (char *) resp_data;
	NMERR_T rc = NM_OK;

	if (user == NULL || folder_name == NULL || contact == NULL) {

		if (contact)
			nm_release_contact(contact);

		if (folder_name)
			g_free(folder_name);

		return;
	}

	if (ret_code == NM_OK || ret_code == NMERR_DUPLICATE_FOLDER) {
		folder = nm_find_folder(user, folder_name);
		if (folder) {

			rc = nm_send_create_contact(user, folder, contact,
										_create_contact_resp_cb, contact);
			_check_for_disconnect(user, rc);
		}
	} else {
		GaimConnection *gc = gaim_account_get_connection(user->client_data);
		const char *name = nm_contact_get_dn(contact);
		char *err =
			g_strdup_printf(_("Unable to add %s to your buddy list."
					  " Error creating folder in server side list (%s)."),
					name, nm_error_to_string (ret_code));

		gaim_notify_error(gc, NULL, err, NULL);

		nm_release_contact(contact);
		g_free(err);
	}

	g_free(folder_name);
}

static void
_join_conf_resp_cb(NMUser * user, NMERR_T ret_code,
				   gpointer resp_data, gpointer user_data)
{
	GaimConversation *chat;
	GaimConnection *gc;
	NMUserRecord *ur;
	NMConference *conference = user_data;
	const char *name, *conf_name;
	int i, count;

	if (user == NULL || conference == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);

	if (ret_code == NM_OK) {
		conf_name = _get_conference_name(++user->conference_count);
		chat = serv_got_joined_chat(gc, user->conference_count, conf_name);
		if (chat) {

			nm_conference_set_data(conference, (gpointer) chat);

			count = nm_conference_get_participant_count(conference);
			for (i = 0; i < count; i++) {
				ur = nm_conference_get_participant(conference, i);
				if (ur) {
					name = nm_user_record_get_display_id(ur);
					gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat), name, NULL, 
											GAIM_CBFLAGS_NONE, TRUE);
				}
			}
		}
	}
}

/* Show info returned by getdetails */
static void
_get_details_resp_show_info(NMUser * user, NMERR_T ret_code,
							gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	NMUserRecord *user_record;
	char *name;
	char *err;

	if (user == NULL)
		return;

	name = user_data;

	if (ret_code == NM_OK) {
		user_record = (NMUserRecord *) resp_data;
		if (user_record) {
			_show_info(gaim_account_get_connection(user->client_data),
					   user_record);
		}
	} else {
		gc = gaim_account_get_connection(user->client_data);
		err =
			g_strdup_printf(_("Could not get details for user %s (%s)."),
					name, nm_error_to_string (ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);
	}

	if (name)
		g_free(name);
}

/* Handle get details response add to privacy list */
static void
_get_details_resp_add_privacy_item(NMUser *user, NMERR_T ret_code,
								   gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	NMUserRecord *user_record = resp_data;
	char *err;
	gboolean allowed = GPOINTER_TO_INT(user_data);
	const char *display_id;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);
	display_id = nm_user_record_get_display_id(user_record);

	if (ret_code == NM_OK) {

		if (allowed) {

			if (!g_slist_find_custom(gc->account->permit,
									 display_id, (GCompareFunc)nm_utf8_strcasecmp)) {
				gaim_privacy_permit_add(gc->account, display_id, TRUE);
			}

		} else {

			if (!g_slist_find_custom(gc->account->permit,
									 display_id, (GCompareFunc)nm_utf8_strcasecmp)) {
				gaim_privacy_deny_add(gc->account, display_id, TRUE);
			}
		}

	} else {

		err = g_strdup_printf(_("Unable to add user to privacy list (%s)."),
							  nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}
}

/* Handle response to create privacy item request */
static void
_create_privacy_item_deny_resp_cb(NMUser *user, NMERR_T ret_code,
								  gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	NMUserRecord *user_record;
	char *who = user_data;
	char *err;
	NMERR_T rc = NM_OK;
	const char *display_id = NULL;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);

	if (ret_code == NM_OK) {

		user_record = nm_find_user_record(user, who);
		if (user_record)
			display_id = nm_user_record_get_display_id(user_record);

		if (display_id) {

			if (!g_slist_find_custom(gc->account->deny,
									 display_id, (GCompareFunc)nm_utf8_strcasecmp)) {

				gaim_privacy_deny_add(gc->account, display_id, TRUE);
			}

		} else {
			rc = nm_send_get_details(user, who,
									 _get_details_resp_add_privacy_item,
									 (gpointer)FALSE);
			_check_for_disconnect(user, rc);
		}
	} else {

		err = g_strdup_printf(_("Unable to add %s to deny list (%s)."),
							  who, nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}

	if (who)
		g_free(who);

}

/* Handle response to create privacy item request */
static void
_create_privacy_item_permit_resp_cb(NMUser *user, NMERR_T ret_code,
									gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	NMUserRecord *user_record;
	char *who = user_data;
	char *err;
	NMERR_T rc = NM_OK;
	const char *display_id = NULL;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);

	if (ret_code == NM_OK) {

		user_record = nm_find_user_record(user, who);
		if (user_record)
			display_id = nm_user_record_get_display_id(user_record);

		if (display_id) {

			if (!g_slist_find_custom(gc->account->permit,
									 display_id,
									 (GCompareFunc)nm_utf8_strcasecmp)) {

				gaim_privacy_permit_add(gc->account, display_id, TRUE);
			}

		} else {
			rc = nm_send_get_details(user, who,
									 _get_details_resp_add_privacy_item,
									 (gpointer)TRUE);
			_check_for_disconnect(user, rc);
		}

	} else {

		err = g_strdup_printf(_("Unable to add %s to permit list (%s)."), who,
							  nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}

	if (who)
		g_free(who);
}

static void
_get_details_send_privacy_create(NMUser *user, NMERR_T ret_code,
								 gpointer resp_data, gpointer user_data)
{
	NMERR_T rc = NM_OK;
	GaimConnection *gc;
	NMUserRecord *user_record = resp_data;
	char *err;
	gboolean allowed = GPOINTER_TO_INT(user_data);
	const char *dn, *display_id;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);
	dn = nm_user_record_get_dn(user_record);
	display_id = nm_user_record_get_display_id(user_record);

	if (ret_code == NM_OK) {

		if (allowed) {
			rc = nm_send_create_privacy_item(user, dn, TRUE,
											 _create_privacy_item_permit_resp_cb,
											 g_strdup(display_id));
			_check_for_disconnect(user, rc);

		} else {
			rc = nm_send_create_privacy_item(user, dn, FALSE,
											 _create_privacy_item_deny_resp_cb,
											 g_strdup(display_id));
			_check_for_disconnect(user, rc);
		}

	} else {

		err = g_strdup_printf(_("Unable to add user to privacy list (%s)."),
							  nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}
}

static void
_remove_privacy_item_resp_cb(NMUser *user, NMERR_T ret_code,
									gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	char *who = user_data;
	char *err;

	if (user == NULL)
		return;

	if (ret_code != NM_OK) {

		gc = gaim_account_get_connection(user->client_data);
		err = g_strdup_printf(_("Unable to remove %s from privacy list (%s)."), who,
							  nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);
	}

	if (who)
		g_free(who);
}

static void
_set_privacy_default_resp_cb(NMUser *user, NMERR_T ret_code,
									gpointer resp_data, gpointer user_data)
{
	GaimConnection *gc;
	char *err;

	if (user == NULL)
		return;

	if (ret_code != NM_OK) {

		gc = gaim_account_get_connection(user->client_data);
		err = g_strdup_printf(_("Unable to change server side privacy settings (%s)."),
							  nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}
}

/* Handle get details response add to privacy list */
static void
_get_details_resp_send_invite(NMUser *user, NMERR_T ret_code,
							  gpointer resp_data, gpointer user_data)
{
	NMERR_T rc = NM_OK;
	GaimConnection *gc;
	NMUserRecord *user_record = resp_data;
	char *err;
	GSList *cnode;
	NMConference *conference;
	gpointer chat;
	long id = (long) user_data;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);

	if (ret_code == NM_OK) {

		for (cnode = user->conferences; cnode != NULL; cnode = cnode->next) {
			conference = cnode->data;
			if (conference && (chat = nm_conference_get_data(conference))) {
				if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(chat)) == id) {
					rc = nm_send_conference_invite(user, conference, user_record,
												   NULL, _sendinvite_resp_cb, NULL);
					_check_for_disconnect(user, rc);
					break;
				}
			}
		}

	} else {

		err = g_strdup_printf(_("Unable to invite user (%s)."), nm_error_to_string(ret_code));
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);

	}
}

static void
_createconf_resp_send_invite(NMUser * user, NMERR_T ret_code,
							  gpointer resp_data, gpointer user_data)
{
	NMERR_T rc = NM_OK;
	NMConference *conference = resp_data;
	NMUserRecord *user_record = user_data;
	GaimConnection *gc;
	char *err;

	if (user == NULL)
		return;



	if (ret_code == NM_OK) {
		rc = nm_send_conference_invite(user, conference, user_record,
									   NULL, _sendinvite_resp_cb, NULL);
		_check_for_disconnect(user, rc);
	} else {
		err = g_strdup_printf(_("Unable to create conference (%s)."), nm_error_to_string(ret_code));
		gc = gaim_account_get_connection(user->client_data);
		gaim_notify_error(gc, NULL, err, NULL);
		g_free(err);
	}
}

/*******************************************************************************
 * Helper functions
 ******************************************************************************/

static char *
_user_agent_string()
{

#if !defined(_WIN32)

	const char *sysname = "";
	const char *release = "";
	struct utsname u;

	if (uname(&u) == 0) {
		sysname = u.sysname;
		release = u.release;
	} else {
		sysname = "Linux";
		release = "Unknown";
	}

	return g_strdup_printf("Gaim/%s (%s; %s)", VERSION, sysname, release);

#else

	const char *sysname = "";
	OSVERSIONINFO os_info;
	SYSTEM_INFO sys_info;

	os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&os_info);
	GetSystemInfo(&sys_info);

	if (os_info.dwPlatformId == VER_PLATFORM_WIN32_NT)  {
		switch (os_info.dwMajorVersion) {
			case 3:
			case 4:
				sysname = "Windows NT";
				break;
			case 5:
				switch (os_info.dwMinorVersion) {
					case 0:
						sysname = "Windows 2000";
						break;
					case 1:
						sysname = "Windows XP";
						break;
					case 2:
						sysname = "Windows Server 2003";
						break;
					default:
						sysname = "Windows";
						break;
				}
				break;
			default:
				sysname = "Windows";
				break;
		}

	}	else if (os_info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		switch (os_info.dwMinorVersion) {
			case 0:
				sysname = "Windows 95";
				break;
			case 10:
				sysname = "Windows 98";
				break;
			case 90:
				sysname = "Windows ME";
				break;
			default:
				sysname = "Windows";
				break;
		}
	} else {
		sysname = "Windows";
	}

	return g_strdup_printf("Gaim/%s (%s; %ld.%ld)", VERSION, sysname,
						   os_info.dwMajorVersion, os_info.dwMinorVersion);

#endif


}

static gboolean
_is_disconnect_error(NMERR_T err)
{
	return (err == NMERR_TCP_WRITE ||
			err == NMERR_TCP_READ || err == NMERR_PROTOCOL);
}

static gboolean
_check_for_disconnect(NMUser * user, NMERR_T err)
{
	GaimConnection *gc = gaim_account_get_connection(user->client_data);

	if (_is_disconnect_error(err)) {

		gaim_connection_error(gc, _("Error communicating with server."
									" Closing connection."));
		return TRUE;

	}

	return FALSE;
}

/* Check to see if the conference is instantiated, if so send the message.
 * If not send the create conference -- the response handler for the createconf
 * will call this function again.
 */
static void
_send_message(NMUser * user, NMMessage * message)
{
	NMConference *conf;
	NMERR_T rc = NM_OK;

	conf = nm_message_get_conference(message);
	if (conf) {
		/* We have a conference make sure that the
		   server knows about it already. */
		if (nm_conference_is_instantiated(conf)) {

			/* We have everything that we need...finally! */
			rc = nm_send_message(user, message, _send_message_resp_cb);
			_check_for_disconnect(user, rc);

			nm_release_message(message);

		} else {
			rc = nm_send_create_conference(user, conf, _createconf_resp_send_msg, message);
			_check_for_disconnect(user, rc);
		}
	}
}

/*
 * Update the status of the given buddy in the Gaim buddy list
 */
static void
_update_buddy_status(NMUser *user, GaimBuddy * buddy, int novellstatus, int gmt)
{
	GaimAccount *account;
	const char *status_id;
	const char *text = NULL;
	const char *dn;
	int idle = 0;
	gboolean loggedin = TRUE;

	account = buddy->account;

	switch (novellstatus) {
		case NM_STATUS_AVAILABLE:
			status_id = NOVELL_STATUS_TYPE_AVAILABLE;
			break;
		case NM_STATUS_AWAY:
			status_id = NOVELL_STATUS_TYPE_AWAY;
			break;
		case NM_STATUS_BUSY:
			status_id = NOVELL_STATUS_TYPE_BUSY;
			break;
		case NM_STATUS_OFFLINE:
			status_id = NOVELL_STATUS_TYPE_OFFLINE;
			loggedin = FALSE;
			break;
		case NM_STATUS_AWAY_IDLE:
			status_id = NOVELL_STATUS_TYPE_AWAY;
			idle = gmt;
			break;
		default:
			status_id = NOVELL_STATUS_TYPE_OFFLINE;
			loggedin = FALSE;
			break;
	}

	/* Get status text for the user */
	dn = nm_lookup_dn(user, buddy->name);
	if (dn) {
		NMUserRecord *user_record = nm_find_user_record(user, dn);
		if (user_record) {
			text = nm_user_record_get_status_text(user_record);
		}
	}

	gaim_prpl_got_user_status(account, buddy->name, status_id,
							  "message", text, NULL);
	gaim_prpl_got_user_idle(account, buddy->name,
							(novellstatus == NM_STATUS_AWAY_IDLE), idle);
}

/* Iterate through the cached Gaim buddy list and remove buddies
 * that are not in the server side list.
 */
static void
_remove_gaim_buddies(NMUser *user)
{
	GaimBlistNode *gnode;
	GaimBlistNode *cnode;
	GaimBlistNode *bnode;
	GaimGroup *group;
	GaimBuddy *buddy;
	GaimBuddyList *blist;
	GSList *rem_list = NULL;
	GSList *l;
	NMFolder *folder = NULL;
	const char *gname = NULL;

	if ((blist = gaim_get_blist())) {
		for (gnode = blist->root; gnode; gnode = gnode->next) {
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			group = (GaimGroup *) gnode;
			for (cnode = gnode->child; cnode; cnode = cnode->next) {
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for (bnode = cnode->child; bnode; bnode = bnode->next) {
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					buddy = (GaimBuddy *) bnode;
					if (buddy->account == user->client_data) {
						gname = group->name;
						if (strcmp(group->name, NM_ROOT_FOLDER_NAME) == 0)
							gname = "";
						folder = nm_find_folder(user, gname);
						if (folder == NULL ||
							!nm_folder_find_contact_by_display_id(folder, buddy->name)) {
							rem_list = g_slist_append(rem_list, buddy);
						}
					}
				}
			}
		}

		if (rem_list) {
			for (l = rem_list; l; l = l->next) {
				gaim_blist_remove_buddy(l->data);
			}
			g_slist_free(rem_list);
		}
	}
}

/* Add all of the contacts in the given folder to the Gaim buddy list */
static void
_add_contacts_to_gaim_blist(NMUser * user, NMFolder * folder)
{
	NMUserRecord *user_record = NULL;
	NMContact *contact = NULL;
	GaimBuddy *buddy = NULL;
	GaimGroup *group;
	NMERR_T cnt = 0, i;
	const char *text = NULL;
	const char *name = NULL;
	const char *fname = NULL;
	int status = 0;

	/* If this is the root folder give it a name. Gaim does not have the concept of
	 * a root folder.
	 */
	fname = nm_folder_get_name(folder);
	if (fname == NULL || *fname == '\0') {
		fname = NM_ROOT_FOLDER_NAME;
	}

	/* Does the Gaim group exist already? */
	group = gaim_find_group(fname);
	if (group == NULL) {
		group = gaim_group_new(fname);
		gaim_blist_add_group(group, NULL);
	}

	/* Get each contact for this folder */
	cnt = nm_folder_get_contact_count(folder);
	for (i = 0; i < cnt; i++) {
		contact = nm_folder_get_contact(folder, i);
		if (contact) {

			name = nm_contact_get_display_id(contact);
			if (name) {

				buddy = gaim_find_buddy_in_group(user->client_data, name, group);
				if (buddy == NULL) {
					/* Add it to the gaim buddy list */
					buddy = gaim_buddy_new(user->client_data,
										   name,
										   nm_contact_get_display_name(contact));

					gaim_blist_add_buddy(buddy, NULL, group, NULL);
				}

				/* Set the initial status for the buddy */
				user_record = nm_contact_get_user_record(contact);
				if (user_record) {
					status = nm_user_record_get_status(user_record);
					text = nm_user_record_get_status_text(user_record);
				}
				_update_buddy_status(user, buddy, status, time(0));

				/* Save the new buddy as part of the contact object */
				nm_contact_set_data(contact, (gpointer) buddy);
			}

		} else {
			/* NULL contact. This should not happen, but
			 * let's break out of the loop.
			 */
			break;
		}
	}
}

/* Add all of the server side contacts to the Gaim buddy list. */
static void
_add_gaim_buddies(NMUser * user)
{
	int cnt = 0, i;
	NMFolder *root_folder = NULL;
	NMFolder *folder = NULL;

	root_folder = nm_get_root_folder(user);
	if (root_folder) {

		/* Add sub-folders and contacts to sub-folders...
		 * iterate throught the sub-folders in reverse order
		 * because Gaim adds the folders to the front -- so we
		 * want to add the first folder last
		 */
		cnt = nm_folder_get_subfolder_count(root_folder);
		for (i = cnt-1; i >= 0; i--) {
			folder = nm_folder_get_subfolder(root_folder, i);
			if (folder) {
				_add_contacts_to_gaim_blist(user, folder);
			}
		}

		/* Add contacts for the root folder */
		_add_contacts_to_gaim_blist(user, root_folder);
	}
}

static void
_sync_contact_list(NMUser *user)
{
	/* Remove all buddies from the local list that are
	 * not in the server side list and add all buddies
	 * from the server side list that are not in
	 * the local list
	 */
	_remove_gaim_buddies(user);
	_add_gaim_buddies(user);
	user->clist_synched = TRUE;
}

static void
_sync_privacy_lists(NMUser *user)
{
	GSList *node = NULL, *rem_list = NULL;
	GaimConnection *gc;
	const char *name, *dn;
	NMUserRecord *user_record;

	if (user == NULL)
		return;

	gc = gaim_account_get_connection(user->client_data);
	if (gc == NULL)
		return;

	/* Set the Gaim privacy setting */
	if (user->default_deny) {
		if (user->allow_list == NULL) {
			gc->account->perm_deny = GAIM_PRIVACY_DENY_ALL;
		} else {
			gc->account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
		}
	} else {
		if (user->deny_list == NULL) {
			gc->account->perm_deny = GAIM_PRIVACY_ALLOW_ALL;
		} else {
			gc->account->perm_deny = GAIM_PRIVACY_DENY_USERS;
		}
	}

	/* Add stuff */
	for (node = user->allow_list; node; node = node->next) {
		user_record = nm_find_user_record(user, (char *)node->data);
		if (user_record)
			name = nm_user_record_get_display_id(user_record);
		else
			name =(char *)node->data;

		if (!g_slist_find_custom(gc->account->permit,
								 name, (GCompareFunc)nm_utf8_strcasecmp)) {
			gaim_privacy_permit_add(gc->account, name , TRUE);
		}
	}

	for (node = user->deny_list; node; node = node->next) {
		user_record = nm_find_user_record(user, (char *)node->data);
		if (user_record)
			name = nm_user_record_get_display_id(user_record);
		else
			name =(char *)node->data;

		if (!g_slist_find_custom(gc->account->deny,
								 name, (GCompareFunc)nm_utf8_strcasecmp)) {
			gaim_privacy_deny_add(gc->account, name, TRUE);
		}
	}


	/*  Remove stuff */
	for (node = gc->account->permit; node; node = node->next) {
		dn = nm_lookup_dn(user, (char *)node->data);
		if (dn != NULL &&
			!g_slist_find_custom(user->allow_list,
								 dn, (GCompareFunc)nm_utf8_strcasecmp)) {
			rem_list = g_slist_append(rem_list, node->data);
		}
	}

	if (rem_list) {
		for (node = rem_list; node; node = node->next) {
			gaim_privacy_permit_remove(gc->account, (char *)node->data, TRUE);
		}
		g_free(rem_list);
		rem_list = NULL;
	}

	for (node = gc->account->deny; node; node = node->next) {
		dn = nm_lookup_dn(user, (char *)node->data);
		if (dn != NULL &&
			!g_slist_find_custom(user->deny_list,
								 dn, (GCompareFunc)nm_utf8_strcasecmp)) {
			rem_list = g_slist_append(rem_list, node->data);
		}
	}

	if (rem_list) {
		for (node = rem_list; node; node = node->next) {
			gaim_privacy_deny_remove(gc->account, (char *)node->data, TRUE);
		}
		g_slist_free(rem_list);
	}
}

 /* Map known property tags to user-friendly strings */
static const char *
_map_property_tag(const char *tag)
{
	if (tag == NULL) return NULL;

	if (strcmp(tag, "telephoneNumber") == 0)
		return _("Telephone Number");
	else if (strcmp(tag, "L") == 0)
		return _("Location");
	else if (strcmp(tag, "OU") == 0)
		return _("Department");
	else if (strcmp(tag, "personalTitle") == 0)
		return _("Personal Title");
	else if (strcmp(tag, "Title") == 0)
		return _("Title");
	else if (strcmp(tag, "mailstop") == 0)
		return _("Mailstop");
	else if (strcmp(tag, "Internet EMail Address") == 0)
		return _("E-Mail Address");
	else
		return tag;
}

/* Display a dialog box showing the properties for the given user record */
static void
_show_info(GaimConnection * gc, NMUserRecord * user_record)
{
	GString *info_text;
	int count, i;
	NMProperty *property;
	const char *tag, *value;

	info_text = g_string_new("");

	tag = _("User ID");
	value = nm_user_record_get_userid(user_record);
	if (value) {
		g_string_append_printf(info_text, "<b>%s:</b> %s<br>", tag, value);
	}

/*	tag = _("DN");
	value = nm_user_record_get_dn(user_record);
	if (value) {
	g_string_append_printf(info_text, "<b>%s:</b> %s<br>",
	tag, value);
	}
*/

	tag = _("Full name");
	value = nm_user_record_get_full_name(user_record);
	if (value) {
		g_string_append_printf(info_text, "<b>%s:</b> %s<br>", tag, value);
	}

	count = nm_user_record_get_property_count(user_record);
	for (i = 0; i < count; i++) {
		property = nm_user_record_get_property(user_record, i);
		if (property) {
			tag = _map_property_tag(nm_property_get_tag(property));
			value = nm_property_get_value(property);
			if (tag && value) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br>",
									   tag, value);
			}
			nm_release_property(property);
		}
	}

	gaim_notify_userinfo(gc, nm_user_record_get_userid(user_record), 
						 info_text->str, NULL, NULL);

	g_string_free(info_text, TRUE);
}

/* Send a join conference, the first item in the parms list is the
 * NMUser object and the second item is the conference to join.
 * This callback is passed to gaim_request_action when we ask the
 * user if they want to join the conference.
 */
static void
_join_conference_cb(GSList * parms)
{
	NMUser *user;
	NMConference *conference;
	NMERR_T rc = NM_OK;

	if (parms == NULL || g_slist_length(parms) != 2)
		return;

	user = g_slist_nth_data(parms, 0);
	conference = g_slist_nth_data(parms, 1);

	if (user && conference) {
		rc = nm_send_join_conference(user, conference,
									 _join_conf_resp_cb, conference);
		_check_for_disconnect(user, rc);
	}

	g_slist_free(parms);
}

/* Send a reject conference, the first item in the parms list is the
 * NMUser object and the second item is the conference to reject.
 * This callback is passed to gaim_request_action when we ask the
 * user if they want to joing the conference.
 */
static void
_reject_conference_cb(GSList * parms)
{
	NMUser *user;
	NMConference *conference;
	NMERR_T rc = NM_OK;

	if (parms == NULL || g_slist_length(parms) != 2)
		return;

	user = g_slist_nth_data(parms, 0);
	conference = g_slist_nth_data(parms, 1);

	if (user && conference) {
		rc = nm_send_reject_conference(user, conference, NULL, NULL);
		_check_for_disconnect(user, rc);
	}

	g_slist_free(parms);
}

static void
_initiate_conference_cb(GaimBlistNode *node, gpointer ignored)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	NMUser *user;
	const char *conf_name;
	GaimConversation *chat = NULL;
	NMUserRecord *user_record;
	NMConference *conference;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	user = gc->proto_data;
	if (user == NULL)
		return;

	/* We should already have a userrecord for the buddy */
	user_record = nm_find_user_record(user, buddy->name);
	if (user_record == NULL)
		return;

	conf_name = _get_conference_name(++user->conference_count);
	chat = serv_got_joined_chat(gc, user->conference_count, conf_name);
	if (chat) {

		conference = nm_create_conference(NULL);
		nm_conference_set_data(conference, (gpointer) chat);
		nm_send_create_conference(user, conference, _createconf_resp_send_invite, user_record);
		nm_release_conference(conference);
	}
}

const char *
_get_conference_name(int id)
{
	static char *name = NULL;

	if (name)
		g_free(name);

	name = g_strdup_printf(_("GroupWise Conference %d"), id);

	return name;
}

static void
_show_privacy_locked_error(GaimConnection *gc, NMUser *user)
{
	char *err;

	err = g_strdup_printf(_("Unable to change server side privacy settings (%s)."),
						  nm_error_to_string(NMERR_ADMIN_LOCKED));
	gaim_notify_error(gc, NULL, err, NULL);
	g_free(err);
}

/*******************************************************************************
 * Connect and recv callbacks
 ******************************************************************************/

static void
novell_ssl_connect_error(GaimSslConnection * gsc,
						 GaimSslErrorType error, gpointer data)
{
	GaimConnection *gc;
	NMUser *user;

	gc = data;
	user = gc->proto_data;
	user->conn->ssl_conn->data = NULL;

	gaim_connection_error(gc, _("Unable to make SSL connection to server."));
}

static void
novell_ssl_recv_cb(gpointer data, GaimSslConnection * gsc,
				   GaimInputCondition condition)
{
	GaimConnection *gc = data;
	NMUser *user;
	NMERR_T rc;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	rc = nm_process_new_data(user);
	if (rc != NM_OK) {

		if (_is_disconnect_error(rc)) {

			gaim_connection_error(gc,
								  _("Error communicating with server."
									" Closing connection."));
		} else {
			gaim_debug(GAIM_DEBUG_INFO, "novell",
					   "Error processing event or response (%d).\n", rc);
		}
	}
}

static void
novell_ssl_connected_cb(gpointer data, GaimSslConnection * gsc,
						GaimInputCondition cond)
{
	GaimConnection *gc = data;
	NMUser *user;
	NMConn *conn;
	NMERR_T rc = 0;
	const char *pwd = NULL;
	const char *my_addr = NULL;
	char *ua = NULL;

	if (gc == NULL || gsc == NULL)
		return;

	user = gc->proto_data;
	if ((user == NULL) || (conn = user->conn) == NULL)
		return;

	gaim_connection_update_progress(gc, _("Authenticating..."),
									2, NOVELL_CONNECT_STEPS);

	my_addr = gaim_network_get_my_ip(gsc->fd);
	pwd = gaim_connection_get_password(gc);
	ua = _user_agent_string();

	rc = nm_send_login(user, pwd, my_addr, ua, _login_resp_cb, NULL);
	if (rc == NM_OK) {
		conn->connected = TRUE;
		gaim_ssl_input_add(gsc, novell_ssl_recv_cb, gc);
	} else {
		gaim_connection_error(gc, _("Unable to connect to server."));
	}

	gaim_connection_update_progress(gc, _("Waiting for response..."),
									3, NOVELL_CONNECT_STEPS);

	g_free(ua);
}

/*******************************************************************************
 * Event callback and event handlers
 ******************************************************************************/

static void
_evt_receive_message(NMUser * user, NMEvent * event)
{
	NMUserRecord *user_record = NULL;
	NMContact *contact = NULL;
	GaimConversation *gconv;
	NMConference *conference;
	GaimMessageFlags flags;
	char *text = NULL;

	text = g_markup_escape_text(nm_event_get_text(event), -1);

	conference = nm_event_get_conference(event);
	if (conference) {

		GaimConversation *chat = nm_conference_get_data(conference);

		/* Is this a single person 'conversation' or a conference? */
		if (chat == NULL && nm_conference_get_participant_count(conference) == 1) {

			user_record = nm_find_user_record(user, nm_event_get_source(event));
			if (user_record) {

				flags = 0;
				if (nm_event_get_type(event) == NMEVT_RECEIVE_AUTOREPLY)
					flags |= GAIM_MESSAGE_AUTO_RESP;

				serv_got_im(gaim_account_get_connection(user->client_data),
							nm_user_record_get_display_id(user_record),
							text, flags,
							nm_event_get_gmt(event));

				gconv =	gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM,
					nm_user_record_get_display_id(user_record),
					(GaimAccount *) user->client_data);
				if (gconv) {

					contact = nm_find_contact(user, nm_event_get_source(event));
					if (contact) {

						gaim_conversation_set_title(
							gconv, nm_contact_get_display_name(contact));


					} else {

						const char *name =
							nm_user_record_get_full_name(user_record);

						if (name == NULL)
							name = nm_user_record_get_userid(user_record);

						gaim_conversation_set_title(gconv, name);
					}

				}

			} else {
				/* this should not happen, see the event code.
				 * the event code will get the contact details from
				 * the server if it does not have them before calling
				 * the event callback.
				 */
			}

		} else if (chat) {

			/* get the contact for send if we have one */
			NMContact *contact = nm_find_contact(user,
												 nm_event_get_source(event));

			/* get the user record for the sender */
			user_record = nm_find_user_record(user, nm_event_get_source(event));
			if (user_record) {
				const char *name = nm_contact_get_display_name(contact);

				if (name == NULL) {
					name = nm_user_record_get_full_name(user_record);
					if (name == NULL)
						name = nm_user_record_get_display_id(user_record);
				}

				serv_got_chat_in(gaim_account_get_connection(user->client_data),
								 gaim_conv_chat_get_id(GAIM_CONV_CHAT(chat)),
								 name, 0, text, nm_event_get_gmt(event));
			}
		}
	}

	g_free(text);
}

static void
_evt_conference_left(NMUser * user, NMEvent * event)
{
	GaimConversation *chat;
	NMConference *conference;

	conference = nm_event_get_conference(event);
	if (conference) {
		chat = nm_conference_get_data(conference);
		if (chat) {
			NMUserRecord *ur = nm_find_user_record(user,
												   nm_event_get_source(event));

			if (ur)
				gaim_conv_chat_remove_user(GAIM_CONV_CHAT(chat),
										   nm_user_record_get_display_id(ur),
										   NULL);
		}
	}
}

static void
_evt_conference_invite_notify(NMUser * user, NMEvent * event)
{
	GaimConversation *gconv;
	NMConference *conference;
	NMUserRecord *user_record = NULL;
	char *str = NULL;

	user_record = nm_find_user_record(user, nm_event_get_source(event));
	conference = nm_event_get_conference(event);
	if (user_record && conference) {
		gconv = nm_conference_get_data(conference);
		str = g_strdup_printf(_("%s has been invited to this conversation."),
							  nm_user_record_get_display_id(user_record));
		gaim_conversation_write(gconv, NULL, str,
								GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(str);
	}
}

static void
_evt_conference_invite(NMUser * user, NMEvent * event)
{
	NMUserRecord *ur;
	GaimConnection *gc;
	GSList *parms = NULL;
	const char *title = NULL;
	const char *secondary = NULL;
	const char *name = NULL;
	char *primary = NULL;
	time_t gmt;

	ur = nm_find_user_record(user, nm_event_get_source(event));
	if (ur)
		name = nm_user_record_get_full_name(ur);

	if (name == NULL)
		name = nm_event_get_source(event);

	gmt = nm_event_get_gmt(event);
	title = _("Invitation to Conversation");
	primary = g_strdup_printf(_("Invitation from: %s\n\nSent: %s"),
							  name, gaim_date_format_full(localtime(&gmt)));
	secondary = _("Would you like to join the conversation?");

	/* Set up parms list for the callbacks
	 * We need to send the NMUser object and
	 * the NMConference object to the callbacks
	 */
	parms = NULL;
	parms = g_slist_append(parms, user);
	parms = g_slist_append(parms, nm_event_get_conference(event));

	/* Prompt the user */
	gc = gaim_account_get_connection(user->client_data);
	gaim_request_action(gc, title, primary, secondary,
						GAIM_DEFAULT_ACTION_NONE, parms, 2,
						_("Yes"), G_CALLBACK(_join_conference_cb),
						_("No"), G_CALLBACK(_reject_conference_cb));

	g_free(primary);
}


static void
_evt_conference_joined(NMUser * user, NMEvent * event)
{
	GaimConversation *chat = NULL;
	GaimConnection *gc;
	NMConference *conference = NULL;
	NMUserRecord *ur = NULL;
	const char *name;
	const char *conf_name;

	gc = gaim_account_get_connection(user->client_data);
	if (gc == NULL)
		return;

	conference = nm_event_get_conference(event);
	if (conference) {
		chat = nm_conference_get_data(conference);
		if (nm_conference_get_participant_count(conference) == 2 && chat == NULL) {
			ur = nm_conference_get_participant(conference, 0);
			if (ur) {
				conf_name = _get_conference_name(++user->conference_count);
				chat =
					serv_got_joined_chat(gc, user->conference_count, conf_name);
				if (chat) {

					nm_conference_set_data(conference, (gpointer) chat);

					name = nm_user_record_get_display_id(ur);
					gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat), name, NULL, 
											GAIM_CBFLAGS_NONE, TRUE);

				}
			}
		}

		if (chat != NULL) {
			ur = nm_find_user_record(user, nm_event_get_source(event));
			if (ur) {
				name = nm_user_record_get_display_id(ur);
				if (!gaim_conv_chat_find_user(GAIM_CONV_CHAT(chat), name)) {
					gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat), name, NULL, 
											GAIM_CBFLAGS_NONE, TRUE);
				}
			}
		}
	}
}

static void
_evt_status_change(NMUser * user, NMEvent * event)
{
	GaimBuddy *buddy = NULL;
	GSList *buddies;
	GSList *bnode;
	NMUserRecord *user_record;
	const char *display_id;
	int status;

	user_record = nm_event_get_user_record(event);
	if (user_record) {

		/* Retrieve new status */
		status = nm_user_record_get_status(user_record);

		/* Update status for buddy in all folders */
		display_id = nm_user_record_get_display_id(user_record);
		buddies = gaim_find_buddies(user->client_data, display_id);
		for (bnode = buddies; bnode; bnode = bnode->next) {
			buddy = (GaimBuddy *) bnode->data;
			if (buddy) {
				_update_buddy_status(user, buddy, status, nm_event_get_gmt(event));
			}
		}

		g_slist_free(buddies);

	}
}

static void
_evt_user_disconnect(NMUser * user, NMEvent * event)
{
	GaimConnection *gc;

	gc = gaim_account_get_connection((GaimAccount *) user->client_data);
	if (gc)
	{
		gc->wants_to_die = TRUE; /* we don't want to reconnect in this case */
		gaim_connection_error(gc, _("You have been logged out because you"
									" logged in at another workstation."));
	}
}

static void
_evt_user_typing(NMUser * user, NMEvent * event)
{
	GaimConnection *gc;
	NMUserRecord *user_record = NULL;

	gc = gaim_account_get_connection((GaimAccount *) user->client_data);
	if (gc) {
		user_record = nm_find_user_record(user, nm_event_get_source(event));
		if (user_record) {
			serv_got_typing(gc, nm_user_record_get_display_id(user_record),
							30, GAIM_TYPING);
		}
	}
}

static void
_evt_user_not_typing(NMUser * user, NMEvent * event)
{
	GaimConnection *gc;
	NMUserRecord *user_record;

	gc = gaim_account_get_connection((GaimAccount *) user->client_data);
	if (gc) {
		user_record = nm_find_user_record(user, nm_event_get_source(event));
		if (user_record) {
			serv_got_typing_stopped(gc,
									nm_user_record_get_display_id(user_record));
		}
	}
}

static void
_evt_undeliverable_status(NMUser * user, NMEvent * event)
{
	NMUserRecord *ur;
	GaimConversation *gconv;
	char *str;

	ur = nm_find_user_record(user, nm_event_get_source(event));
	if (ur) {
		/* XXX - Should this be GAIM_CONV_TYPE_IM? */
		gconv =
			gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY,
												nm_user_record_get_display_id(ur),
												user->client_data);
		if (gconv) {
			const char *name = nm_user_record_get_full_name(ur);

			if (name == NULL) {
				name = nm_user_record_get_display_id(ur);
			}
			str = g_strdup_printf(_("%s appears to be offline and did not receive"
									" the message that you just sent."), name);
			gaim_conversation_write(gconv, NULL, str,
									GAIM_MESSAGE_SYSTEM, time(NULL));
			g_free(str);
		}
	}
}

static void
_event_callback(NMUser * user, NMEvent * event)
{
	if (user == NULL || event == NULL)
		return;

	switch (nm_event_get_type(event)) {
		case NMEVT_STATUS_CHANGE:
			_evt_status_change(user, event);
			break;
		case NMEVT_RECEIVE_AUTOREPLY:
		case NMEVT_RECEIVE_MESSAGE:
			_evt_receive_message(user, event);
			break;
		case NMEVT_USER_DISCONNECT:
			_evt_user_disconnect(user, event);
			break;
		case NMEVT_USER_TYPING:
			_evt_user_typing(user, event);
			break;
		case NMEVT_USER_NOT_TYPING:
			_evt_user_not_typing(user, event);
			break;
		case NMEVT_SERVER_DISCONNECT:
			/* Nothing to do? */
			break;
		case NMEVT_INVALID_RECIPIENT:
			break;
		case NMEVT_UNDELIVERABLE_STATUS:
			_evt_undeliverable_status(user, event);
			break;
		case NMEVT_CONFERENCE_INVITE_NOTIFY:
			/* Someone else has been invited to join a
			 * conference that we are currently a part of
			 */
			_evt_conference_invite_notify(user, event);
			break;
		case NMEVT_CONFERENCE_INVITE:
			/* We have been invited to join a conference */
			_evt_conference_invite(user, event);
			break;
		case NMEVT_CONFERENCE_JOINED:
			/* Some one has joined a conference that we
			 * are a part of
			 */
			_evt_conference_joined(user, event);
			break;
		case NMEVT_CONFERENCE_LEFT:
			/* Someone else has left a conference that we
			 * are currently a part of
			 */
			_evt_conference_left(user, event);
			break;
		default:
			gaim_debug(GAIM_DEBUG_INFO, "novell",
					   "_event_callback(): unhandled event, %d\n",
					   nm_event_get_type(event));
			break;
	}
}

/*******************************************************************************
 * Prpl Ops
 ******************************************************************************/

static void
novell_login(GaimAccount * account)
{
	GaimConnection *gc;
	NMUser *user = NULL;
	const char *server;
	const char *name;
	int port;

	if (account == NULL)
		return;

	gc = gaim_account_get_connection(account);
	if (gc == NULL)
		return;

	server = gaim_account_get_string(account, "server", NULL);
	if (server == NULL || *server == '\0') {

		/* TODO: Would be nice to prompt if not set!
		 *  gaim_request_fields(gc, _("Server Address"),...);
		 */

		/* ...but for now just error out with a nice message. */
		gaim_connection_error(gc, _("Unable to connect to server."
									" Please enter the address of the server"
									" you wish to connect to."));
		return;
	}

	port = gaim_account_get_int(account, "port", DEFAULT_PORT);
	name = gaim_account_get_username(account);

	user = nm_initialize_user(name, server, port, account, _event_callback);
	if (user && user->conn) {
		/* save user */
		gc->proto_data = user;

		/* connect to the server */
		gaim_connection_update_progress(gc, _("Connecting"),
										1, NOVELL_CONNECT_STEPS);

		user->conn->use_ssl = TRUE;

		user->conn->ssl_conn = g_new0(NMSSLConn, 1);
		user->conn->ssl_conn->read = (nm_ssl_read_cb) gaim_ssl_read;
		user->conn->ssl_conn->write = (nm_ssl_write_cb) gaim_ssl_write;

		user->conn->ssl_conn->data = gaim_ssl_connect(user->client_data,
													  user->conn->addr, user->conn->port,
													  novell_ssl_connected_cb, novell_ssl_connect_error, gc);
		if (user->conn->ssl_conn->data == NULL) {
			gaim_connection_error(gc, _("Error."
										" SSL support is not installed."));
		}
	}
}

static void
novell_close(GaimConnection * gc)
{
	NMUser *user;
	NMConn *conn;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user) {
		conn = user->conn;
		if (conn && conn->ssl_conn) {
			gaim_ssl_close(user->conn->ssl_conn->data);
		}
		nm_deinitialize_user(user);
	}
	gc->proto_data = NULL;
}

static int
novell_send_im(GaimConnection * gc, const char *name,
			   const char *message_body, GaimMessageFlags flags)
{
	NMUserRecord *user_record = NULL;
	NMConference *conf = NULL;
	NMMessage *message;
	NMUser *user;
	const char *dn = NULL;
	char *plain;
	gboolean done = TRUE, created_conf = FALSE;
	NMERR_T rc = NM_OK;

	if (gc == NULL || name == NULL ||
		message_body == NULL || *message_body == '\0')
		return 0;

	user = gc->proto_data;
	if (user == NULL)
		return 0;

	/* Create a new message */
	plain = gaim_unescape_html(message_body);
	message = nm_create_message(plain);
	g_free(plain);

	/* Need to get the DN for the buddy so we can look up the convo */
	dn = nm_lookup_dn(user, name);

	/* Do we already know about the sender? */
	user_record = nm_find_user_record(user, dn);
	if (user_record) {

		/* Do we already have an instantiated conference? */
		conf = nm_find_conversation(user, dn);
		if (conf == NULL) {

			/* If not, create a blank conference */
			conf = nm_create_conference(NULL);
			created_conf = TRUE;

			nm_conference_add_participant(conf, user_record);
		}

		nm_message_set_conference(message, conf);

		/* Make sure conference is instantiated */
		if (!nm_conference_is_instantiated(conf)) {

			/* It is not, so send the createconf. We will
			 * have to finish sending the message when we
			 * get the response with the new conference guid.
			 */
			rc = nm_send_create_conference(user, conf, _createconf_resp_send_msg, message);
			_check_for_disconnect(user, rc);

			done = FALSE;
		}

	} else {

		/* If we don't have details for the user, then we don't have
		 * a conference yet. So create one and send the getdetails
		 * to the server. We will have to finish sending the message
		 * when we get the response from the server.
		 */
		conf = nm_create_conference(NULL);
		created_conf = TRUE;

		nm_message_set_conference(message, conf);

		rc = nm_send_get_details(user, name, _get_details_resp_send_msg, message);
		_check_for_disconnect(user, rc);

		done = FALSE;
	}

	if (done) {

		/* Did we find everything we needed? */
		rc = nm_send_message(user, message, _send_message_resp_cb);
		_check_for_disconnect(user, rc);

		nm_release_message(message);
	}

	if (created_conf && conf)
		nm_release_conference(conf);

	return 1;
}

static unsigned int
novell_send_typing(GaimConnection * gc, const char *name, GaimTypingState state)
{
	NMConference *conf = NULL;
	NMUser *user;
	const char *dn = NULL;
	NMERR_T rc = NM_OK;

	if (gc == NULL || name == NULL)
		return 0;

	user = gc->proto_data;
	if (user == NULL)
		return 0;

	/* Need to get the DN for the buddy so we can look up the convo */
	dn = nm_lookup_dn(user, name);
	if (dn) {

		/* Now find the conference in our list */
		conf = nm_find_conversation(user, dn);
		if (conf) {

			rc = nm_send_typing(user, conf,
								((state == GAIM_TYPING) ? TRUE : FALSE), NULL);
			_check_for_disconnect(user, rc);

		}

	}

	return 0;
}

static void
novell_convo_closed(GaimConnection * gc, const char *who)
{
	NMUser *user;
	NMConference *conf;
	const char *dn;
	NMERR_T rc = NM_OK;

	if (gc == NULL || who == NULL)
		return;

	user = gc->proto_data;
	if (user && (dn = nm_lookup_dn(user, who))) {
		conf = nm_find_conversation(user, dn);
		if (conf) {
			rc = nm_send_leave_conference(user, conf, NULL, NULL);
			_check_for_disconnect(user, rc);
		}
	}
}

static void
novell_chat_leave(GaimConnection * gc, int id)
{
	NMConference *conference;
	NMUser *user;
	GaimConversation *chat;
	GSList *cnode;
	NMERR_T rc = NM_OK;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	for (cnode = user->conferences; cnode != NULL; cnode = cnode->next) {
		conference = cnode->data;
		if (conference && (chat = nm_conference_get_data(conference))) {
			if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(chat)) == id) {
				rc = nm_send_leave_conference(user, conference, NULL, NULL);
				_check_for_disconnect(user, rc);
				break;
			}
		}
	}

	serv_got_chat_left(gc, id);
}

static void
novell_chat_invite(GaimConnection *gc, int id,
				   const char *message, const char *who)
{
	NMConference *conference;
	NMUser *user;
	GaimConversation *chat;
	GSList *cnode;
	NMERR_T rc = NM_OK;
	NMUserRecord *user_record = NULL;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	user_record = nm_find_user_record(user, who);
	if (user_record == NULL) {
		rc = nm_send_get_details(user, who, _get_details_resp_send_invite, GINT_TO_POINTER(id));
		_check_for_disconnect(user, rc);
		return;
	}

	for (cnode = user->conferences; cnode != NULL; cnode = cnode->next) {
		conference = cnode->data;
		if (conference && (chat = nm_conference_get_data(conference))) {
			if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(chat)) == id) {
				rc = nm_send_conference_invite(user, conference, user_record,
											   message, _sendinvite_resp_cb, NULL);
				_check_for_disconnect(user, rc);
				break;
			}
		}
	}
}

static int
novell_chat_send(GaimConnection * gc, int id, const char *text, GaimMessageFlags flags)
{
	NMConference *conference;
	GaimConversation *chat;
	GSList *cnode;
	NMMessage *message;
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *name;
	char *str, *plain;

	if (gc == NULL || text == NULL)
		return -1;

	user = gc->proto_data;
	if (user == NULL)
		return -1;

	plain = gaim_unescape_html(text);
	message = nm_create_message(plain);
	g_free(plain);

	for (cnode = user->conferences; cnode != NULL; cnode = cnode->next) {
		conference = cnode->data;
		if (conference && (chat = nm_conference_get_data(conference))) {
			if (gaim_conv_chat_get_id(GAIM_CONV_CHAT(chat)) == id) {

				nm_message_set_conference(message, conference);

				/* check to see if the conference is instatiated yet */
				if (!nm_conference_is_instantiated(conference)) {
					nm_message_add_ref(message);
					nm_send_create_conference(user, conference, _createconf_resp_send_msg, message);
				} else {
					rc = nm_send_message(user, message, _send_message_resp_cb);
				}

				nm_release_message(message);

				if (!_check_for_disconnect(user, rc)) {

					/* Use the account alias if it is set */
					name = gaim_account_get_alias(user->client_data);
					if (name == NULL || *name == '\0') {

						/* If there is no account alias, try full name */
						name = nm_user_record_get_full_name(user->user_record);
						if (name == NULL || *name == '\0') {

							/* Fall back to the username that we are signed in with */
							name = gaim_account_get_username(user->client_data);
						}
					}

					serv_got_chat_in(gc, id, name, 0, text, time(NULL));
					return 0;
				} else
					return -1;

			}
		}
	}


	/* The conference was not found, must be closed */
	chat = gaim_find_chat(gc, id);
	if (chat) {
		str = g_strdup_printf(_("This conference has been closed."
								" No more messages can be sent."));
		gaim_conversation_write(chat, NULL, str, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(str);
	}

	if (message)
		nm_release_message(message);

	return -1;
}

static void
novell_add_buddy(GaimConnection * gc, GaimBuddy *buddy, GaimGroup * group)
{
	NMFolder *folder = NULL;
	NMContact *contact;
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *alias, *gname;

	if (gc == NULL || buddy == NULL || group == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user == NULL)
		return;

	/* If we haven't synched the contact list yet, ignore
	 * the add_buddy calls. Server side list is the master.
	 */
	if (!user->clist_synched)
		return;

	contact = nm_create_contact();
	nm_contact_set_dn(contact, buddy->name);

	/* Remove the GaimBuddy (we will add it back after adding it
	 * to the server side list). Save the alias if there is one.
	 */
	alias = gaim_buddy_get_alias(buddy);
	if (alias && strcmp(alias, buddy->name))
		nm_contact_set_display_name(contact, alias);

	gaim_blist_remove_buddy(buddy);
	buddy = NULL;

	if (strcmp(group->name, NM_ROOT_FOLDER_NAME) == 0) {
		gname = "";
	} else {
		gname = group->name;
	}

	folder = nm_find_folder(user, gname);
	if (folder) {

		/* We have everything that we need, so send the createcontact */
		rc = nm_send_create_contact(user, folder, contact,
									_create_contact_resp_cb, contact);

	} else {

		/* Need to create the folder before we can add the contact */
		rc = nm_send_create_folder(user, gname,
								   _create_folder_resp_add_contact, contact);
	}

	_check_for_disconnect(user, rc);

}

static void
novell_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	NMContact *contact;
	NMFolder *folder;
	NMUser *user;
	const char *dn, *gname;
	NMERR_T rc = NM_OK;

	if (gc == NULL || buddy == NULL || group == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user && (dn = nm_lookup_dn(user, buddy->name))) {
		if (strcmp(group->name, NM_ROOT_FOLDER_NAME) == 0) {
			gname = "";
		} else {
			gname = group->name;
		}
		folder = nm_find_folder(user, gname);
		if (folder) {
			contact = nm_folder_find_contact(folder, dn);
			if (contact) {

				/* Remove the buddy from the contact */
				nm_contact_set_data(contact, NULL);

				/* Tell the server to remove the contact */
				rc = nm_send_remove_contact(user, folder, contact,
											_remove_contact_resp_cb, NULL);
				_check_for_disconnect(user, rc);
			}
		}
	}
}

static void
novell_remove_group(GaimConnection * gc, GaimGroup *group)
{
	NMUser *user;
	NMERR_T rc = NM_OK;

	if (gc == NULL || group == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user) {
		NMFolder *folder = nm_find_folder(user, group->name);

		if (folder) {
			rc = nm_send_remove_folder(user, folder,
									   _remove_folder_resp_cb, NULL);
			_check_for_disconnect(user, rc);
		}
	}
}

static void
novell_alias_buddy(GaimConnection * gc, const char *name, const char *alias)
{
	NMContact *contact;
	NMUser *user;
	GList *contacts = NULL;
	GList *cnode = NULL;
	const char *dn = NULL, *fname = NULL;
	NMERR_T rc = NM_OK;

	if (gc == NULL || name == NULL || alias == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user && (dn = nm_lookup_dn(user, name))) {

		/* Alias all of instances of the contact */
		contacts = nm_find_contacts(user, dn);
		for (cnode = contacts; cnode != NULL; cnode = cnode->next) {
			contact = (NMContact *) cnode->data;
			if (contact) {
				GaimGroup *group = NULL;
				GaimBuddy *buddy;
				NMFolder *folder;

				/* Alias the Gaim buddy? */
				folder = nm_find_folder_by_id(user,
											  nm_contact_get_parent_id(contact));
				if (folder) {
					fname = nm_folder_get_name(folder);
					if (*fname == '\0') {
						fname = NM_ROOT_FOLDER_NAME;
					}
					group = gaim_find_group(fname);
				}

				if (group) {
					buddy = gaim_find_buddy_in_group(user->client_data,
													 name, group);
					if (buddy && strcmp(buddy->alias, alias))
						gaim_blist_alias_buddy(buddy, alias);
				}

				/* Tell the server to alias the contact */
				rc = nm_send_rename_contact(user, contact, alias,
											_rename_contact_resp_cb, NULL);
				_check_for_disconnect(user, rc);
			}
		}
		if (contacts)
			g_list_free(contacts);
	}
}

static void
novell_group_buddy(GaimConnection * gc,
				   const char *name, const char *old_group_name,
				   const char *new_group_name)
{
	NMFolder *old_folder;
	NMFolder *new_folder;
	NMContact *contact;
	NMUser *user;
	const char *dn;
	NMERR_T rc = NM_OK;

	if (gc == NULL || name == NULL ||
		old_group_name == NULL || new_group_name == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user && (dn = nm_lookup_dn(user, name))) {

		/* Find the old folder */
		if (strcmp(old_group_name, NM_ROOT_FOLDER_NAME) == 0) {
			old_folder = nm_get_root_folder(user);
			if (nm_folder_find_contact(old_folder, dn) == NULL)
				old_folder = nm_find_folder(user, old_group_name);
		} else {
			old_folder = nm_find_folder(user, old_group_name);
		}

		if (old_folder && (contact = nm_folder_find_contact(old_folder, dn))) {

			/* Find the new folder */
			new_folder = nm_find_folder(user, new_group_name);
			if (new_folder == NULL) {
				if (strcmp(new_group_name, NM_ROOT_FOLDER_NAME) == 0)
					new_folder = nm_get_root_folder(user);
			}

			if (new_folder) {

				/* Tell the server to move the contact to the new folder */
				rc = nm_send_move_contact(user, contact, new_folder,
										  _move_contact_resp_cb, NULL);

			} else {

				nm_contact_add_ref(contact);

				/* Remove the old contact first */
				nm_send_remove_contact(user, old_folder, contact,
									   _remove_contact_resp_cb, NULL);

				/* New folder does not exist yet, so create it  */
				rc = nm_send_create_folder(user, new_group_name,
										   _create_folder_resp_move_contact,
										   contact);
			}

			_check_for_disconnect(user, rc);
		}
	}
}

static void
novell_rename_group(GaimConnection * gc, const char *old_name,
					GaimGroup *group, GList *moved_buddies)
{
	NMERR_T rc = NM_OK;
	NMFolder *folder;
	NMUser *user;

	if (gc == NULL || old_name == NULL || group == NULL || moved_buddies == NULL) {
		return;
	}

	user = gc->proto_data;
	if (user) {
		/* Does new folder exist already? */
		if (nm_find_folder(user, group->name)) {
			/* gaim_blist_rename_group() adds the buddies
			 * to the new group and removes the old group...
			 * so there is nothing more to do here.
			 */
			return;
		}

		if (strcmp(old_name, NM_ROOT_FOLDER_NAME) == 0) {
			/* Can't rename the root folder ... need to revisit this */
			return;
		}

		folder = nm_find_folder(user, old_name);
		if (folder) {
			rc = nm_send_rename_folder(user, folder, group->name,
									   _rename_folder_resp_cb, NULL);
			_check_for_disconnect(user, rc);
		}
	}
}

static void
novell_list_emblems(GaimBuddy * buddy, const char **se, const char **sw, const char **nw, const char **ne)
{
	NMUserRecord *user_record = NULL;
	GaimConnection *gc;
	NMUser *user;
	int status = 0;

	gc = gaim_account_get_connection(buddy->account);

	if (gc == NULL || (user = gc->proto_data) == NULL)
		return;

	user_record = nm_find_user_record(user, buddy->name);

	if (user_record)
		status = nm_user_record_get_status(user_record);

	switch (status) {
		case NM_STATUS_AVAILABLE:
			*se = "";
			break;
		case NM_STATUS_AWAY:
			*se = "away";
			break;
		case NM_STATUS_BUSY:
			*se = "occupied";
			break;
		case NM_STATUS_UNKNOWN:
			*se = "error";
			break;
	}
}

static const char *
novell_list_icon(GaimAccount * account, GaimBuddy * buddy)
{
	return "novell";
}

static void
novell_tooltip_text(GaimBuddy * buddy, GString * str, gboolean full)
{
	NMUserRecord *user_record = NULL;
	GaimConnection *gc;
	NMUser *user;
	int status = 0;
	const char *status_str = NULL;
	const char *text = NULL;

	if (buddy == NULL)
		return; 

	gc = gaim_account_get_connection(buddy->account);
	if (gc == NULL || (user = gc->proto_data) == NULL)
		return;

	if (GAIM_BUDDY_IS_ONLINE(buddy)) {
		user_record = nm_find_user_record(user, buddy->name);
		if (user_record) {
			status = nm_user_record_get_status(user_record);
			text = nm_user_record_get_status_text(user_record);
			/* No custom text, so default it ... */
			switch (status) {
				case NM_STATUS_AVAILABLE:
					status_str = _("Available");
					break;
				case NM_STATUS_AWAY:
					status_str = _("Away");
					break;
				case NM_STATUS_BUSY:
					status_str = _("Busy");
					break;
				case NM_STATUS_AWAY_IDLE:
					status_str = _("Idle");
					break;
				case NM_STATUS_OFFLINE:
					status_str = _("Offline");
					break;
				default:
					status_str = _("Unknown");
					break;
			}

			if (text)
				g_string_append_printf(str, "\n<b>%s:</b> %s"
										    "\n<b>%s:</b> %s",
										    _("Status"), status_str,
										    _("Message"), text);
			else
				g_string_append_printf(str, "\n<b>%s:</b> %s",
										    _("Status"), status_str);
		}
	}
}

static void
novell_set_idle(GaimConnection * gc, int time)
{
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *id = NULL;
	GaimStatus *status = NULL;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	status = gaim_account_get_active_status(gaim_connection_get_account(gc));
	id = gaim_status_get_id(status);

	/* Only go idle if active status is available  */
	if (!strcmp(id, NOVELL_STATUS_TYPE_AVAILABLE)) {
		if (time > 0) {
			rc = nm_send_set_status(user, NM_STATUS_AWAY_IDLE, NULL, NULL, NULL, NULL);
		} else {
			rc = nm_send_set_status(user, NM_STATUS_AVAILABLE, NULL, NULL, NULL, NULL);
		}
	}

	_check_for_disconnect(user, rc);
}

static void
novell_get_info(GaimConnection * gc, const char *name)
{
	NMUserRecord *user_record;
	NMUser *user;
	NMERR_T rc;

	if (gc == NULL || name == NULL)
		return;

	user = (NMUser *) gc->proto_data;
	if (user) {

		user_record = nm_find_user_record(user, name);
		if (user_record) {

			_show_info(gc, user_record);

		} else {

			rc = nm_send_get_details(user, name,
									 _get_details_resp_show_info, g_strdup(name));

			_check_for_disconnect(user, rc);

		}

	}
}

static char *
novell_status_text(GaimBuddy * buddy)
{
	const char *text = NULL;
	const char *dn = NULL;

	if (buddy && buddy->account) {
		GaimConnection *gc = gaim_account_get_connection(buddy->account);

		if (gc && gc->proto_data) {
			NMUser *user = gc->proto_data;

			dn = nm_lookup_dn(user, buddy->name);
			if (dn) {
				NMUserRecord *user_record = nm_find_user_record(user, dn);

				if (user_record) {
					text = nm_user_record_get_status_text(user_record);
					if (text)
						return g_strdup(text);
				}
			}
		}
	}

	return NULL;
}

static GList *
novell_status_types(GaimAccount *account)
{
	GList *status_types = NULL;
	GaimStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AVAILABLE, NOVELL_STATUS_TYPE_AVAILABLE,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"), gaim_value_new(GAIM_TYPE_STRING),
										   NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AWAY, NOVELL_STATUS_TYPE_AWAY,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"), gaim_value_new(GAIM_TYPE_STRING),
										   NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_UNAVAILABLE, NOVELL_STATUS_TYPE_BUSY,
										   _("Busy"), TRUE, TRUE, FALSE,
										   "message", _("Message"), gaim_value_new(GAIM_TYPE_STRING),
										   NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_INVISIBLE, NOVELL_STATUS_TYPE_APPEAR_OFFLINE,
										   NULL, TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	return status_types;
}

static void
novell_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc;
	gboolean connected;
	GaimPresence *presence;
	GaimStatusType *type;
	GaimStatusPrimitive primitive;
	NMUser *user;
	NMSTATUS_T novellstatus = NM_STATUS_AVAILABLE;
	NMERR_T rc = NM_OK;
	const char *msg = NULL;
	char *text = NULL;

	connected = gaim_account_is_connected(account);
	presence = gaim_status_get_presence(status);
	type = gaim_status_get_type(status);
	primitive = gaim_status_type_get_primitive(type);

	/*
	 * We don't have any independent statuses, so we don't need to
	 * do anything when a status is deactivated (because another
	 * status is about to be activated).
	 */
	if (!gaim_status_is_active(status))
		return;

	if (!connected)
		return;

	gc = gaim_account_get_connection(account);
	user = gc->proto_data;
	if (user == NULL)
		return;

	if (primitive == GAIM_STATUS_AVAILABLE) {
		novellstatus = NM_STATUS_AVAILABLE;
	} else if (primitive == GAIM_STATUS_AWAY) {
		novellstatus = NM_STATUS_AWAY;
	} else if (primitive == GAIM_STATUS_UNAVAILABLE) {
		novellstatus = NM_STATUS_BUSY;
	} else if (primitive == GAIM_STATUS_INVISIBLE) {
		novellstatus = NM_STATUS_OFFLINE;
	} else if (gaim_presence_is_idle(presence)) {
		novellstatus = NM_STATUS_AWAY_IDLE;
	} else {
		novellstatus = NM_STATUS_AVAILABLE;
	}

	if (primitive == GAIM_STATUS_AWAY || primitive == GAIM_STATUS_AVAILABLE ||
		primitive == GAIM_STATUS_UNAVAILABLE) {
		msg = gaim_status_get_attr_string(status, "message");
		text = g_strdup(msg);

		if (primitive == GAIM_STATUS_AVAILABLE)
			msg = NULL; /* no auto replies for online status */

		/* Don't want newlines in status text */
		gaim_util_chrreplace(text, '\n', ' ');
	}

	rc = nm_send_set_status(user, novellstatus, text, msg, NULL, NULL);
	_check_for_disconnect(user, rc);

	if (text)
		g_free(text);
}

static void
novell_add_permit(GaimConnection *gc, const char *who)
{
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *name = who;

	if (gc == NULL || who == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	/* Remove first -- we will add it back in when we get
	 * the okay from the server
	 */
	gaim_privacy_permit_remove(gc->account, who, TRUE);

	if (nm_user_is_privacy_locked(user)) {
		_show_privacy_locked_error(gc, user);
		_sync_privacy_lists(user);
		return;
	}

	/* Work around for problem with un-typed, dotted contexts */
	if (strchr(who, '.')) {
		const char *dn = nm_lookup_dn(user, who);
		if (dn == NULL) {
			rc = nm_send_get_details(user, who, _get_details_send_privacy_create,
									 (gpointer)TRUE);
			_check_for_disconnect(user, rc);
			return;
		} else {
			name = dn;
		}
	}

	rc = nm_send_create_privacy_item(user, name, TRUE,
									 _create_privacy_item_permit_resp_cb,
									 g_strdup(who));
	_check_for_disconnect(user, rc);
}

static void
novell_add_deny(GaimConnection *gc, const char *who)
{
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *name = who;

	if (gc == NULL || who == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	/* Remove first -- we will add it back in when we get
	 * the okay from the server
	 */
	gaim_privacy_deny_remove(gc->account, who, TRUE);

	if (nm_user_is_privacy_locked(user)) {
		_show_privacy_locked_error(gc, user);
		_sync_privacy_lists(user);
		return;
	}

	/* Work around for problem with un-typed, dotted contexts */
	if (strchr(who, '.')) {
		const char *dn = nm_lookup_dn(user, who);
		if (dn == NULL) {
			rc = nm_send_get_details(user, who, _get_details_send_privacy_create,
									 (gpointer)FALSE);
			_check_for_disconnect(user, rc);
			return;
		} else {
			name = dn;
		}
	}

	rc = nm_send_create_privacy_item(user, name, FALSE,
									 _create_privacy_item_deny_resp_cb,
									 g_strdup(who));
	_check_for_disconnect(user, rc);
}

static void
novell_rem_permit(GaimConnection *gc, const char *who)
{
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *dn = NULL;

	if (gc == NULL || who == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	if (nm_user_is_privacy_locked(user)) {
		_show_privacy_locked_error(gc, user);
		_sync_privacy_lists(user);
		return;
	}

	dn = nm_lookup_dn(user, who);
	if (dn == NULL)
		dn = who;

	rc = nm_send_remove_privacy_item(user, dn, TRUE,
									 _remove_privacy_item_resp_cb,
									 g_strdup(who));
	_check_for_disconnect(user, rc);
}

static void
novell_rem_deny(GaimConnection *gc, const char *who)
{
	NMUser *user;
	NMERR_T rc = NM_OK;
	const char *dn = NULL;

	if (gc == NULL || who == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	if (nm_user_is_privacy_locked(user)) {
		_show_privacy_locked_error(gc, user);
		_sync_privacy_lists(user);
		return;
	}

	dn = nm_lookup_dn(user, who);
	if (dn == NULL)
		dn = who;

	rc = nm_send_remove_privacy_item(user, dn, FALSE,
									 _remove_privacy_item_resp_cb,
									 g_strdup(who));
	_check_for_disconnect(user, rc);
}

static void
novell_set_permit_deny(GaimConnection *gc)
{
	NMERR_T rc = NM_OK;
	const char *dn, *name = NULL;
	NMUserRecord *user_record = NULL;
	GSList *node = NULL, *copy = NULL;
	NMUser *user;
	int i, j, num_contacts, num_folders;
	NMContact *contact;
	NMFolder *folder = NULL;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	if (user->privacy_synched == FALSE) {
		_sync_privacy_lists(user);
		user->privacy_synched = TRUE;
		return;
	}

	if (nm_user_is_privacy_locked(user)) {
		_show_privacy_locked_error(gc, user);
		_sync_privacy_lists(user);
		return;
	}

	switch (gc->account->perm_deny) {

		case GAIM_PRIVACY_ALLOW_ALL:
			rc = nm_send_set_privacy_default(user, FALSE,
											 _set_privacy_default_resp_cb, NULL);
			_check_for_disconnect(user, rc);

			/* clear server side deny list */
			if (rc == NM_OK) {
				copy = g_slist_copy(user->deny_list);
				for (node = copy; node && node->data; node = node->next) {
					rc = nm_send_remove_privacy_item(user, (const char *)node->data,
													 FALSE, NULL, NULL);
					if (_check_for_disconnect(user, rc))
						break;
				}
				g_slist_free(copy);
				g_slist_free(user->deny_list);
				user->deny_list = NULL;
			}
			break;

		case GAIM_PRIVACY_DENY_ALL:
			rc = nm_send_set_privacy_default(user, TRUE,
											 _set_privacy_default_resp_cb, NULL);
			_check_for_disconnect(user, rc);

			/* clear server side allow list */
			if (rc == NM_OK) {
				copy = g_slist_copy(user->allow_list);
				for (node = copy; node && node->data; node = node->next) {
					rc = nm_send_remove_privacy_item(user, (const char *)node->data,
													 TRUE, NULL, NULL);
					if (_check_for_disconnect(user, rc))
						break;
				}
				g_slist_free(copy);
				g_slist_free(user->allow_list);
				user->allow_list = NULL;
			}
			break;

		case GAIM_PRIVACY_ALLOW_USERS:

			rc = nm_send_set_privacy_default(user, TRUE,
											 _set_privacy_default_resp_cb, NULL);
			_check_for_disconnect(user, rc);

			/* sync allow lists */
			if (rc == NM_OK) {

				for (node = user->allow_list; node; node = node->next) {
					user_record = nm_find_user_record(user, (char *)node->data);
					if (user_record) {
						name = nm_user_record_get_display_id(user_record);

						if (!g_slist_find_custom(gc->account->permit,
												 name, (GCompareFunc)nm_utf8_strcasecmp)) {
							gaim_privacy_permit_add(gc->account, name , TRUE);
						}
					}
				}

				for (node = gc->account->permit; node; node = node->next) {
					name = NULL;
					dn = nm_lookup_dn(user, (char *)node->data);
					if (dn) {
						user_record = nm_find_user_record(user, dn);
						name = nm_user_record_get_display_id(user_record);

						if (!g_slist_find_custom(user->allow_list,
												 dn, (GCompareFunc)nm_utf8_strcasecmp)) {
							rc = nm_send_create_privacy_item(user, dn, TRUE,
															 _create_privacy_item_deny_resp_cb,
															 g_strdup(dn));
						}
					} else {
						gaim_privacy_permit_remove(gc->account, (char *)node->data, TRUE);
					}
				}
			}
			break;

		case GAIM_PRIVACY_DENY_USERS:

			/* set to default allow */
			rc = nm_send_set_privacy_default(user, FALSE,
											 _set_privacy_default_resp_cb, NULL);
			_check_for_disconnect(user, rc);

			/* sync deny lists */
			if (rc == NM_OK) {

				for (node = user->deny_list; node; node = node->next) {
					user_record = nm_find_user_record(user, (char *)node->data);
					if (user_record) {
						name = nm_user_record_get_display_id(user_record);

						if (!g_slist_find_custom(gc->account->deny,
												 name, (GCompareFunc)nm_utf8_strcasecmp)) {
							gaim_privacy_deny_add(gc->account, name , TRUE);
						}
					}
				}

				for (node = gc->account->deny; node; node = node->next) {

					name = NULL;
					dn = nm_lookup_dn(user, (char *)node->data);
					if (dn) {
						user_record = nm_find_user_record(user, dn);
						name = nm_user_record_get_display_id(user_record);

						if (!g_slist_find_custom(user->deny_list,
												 dn, (GCompareFunc)nm_utf8_strcasecmp)) {
							rc = nm_send_create_privacy_item(user, dn, FALSE,
															 _create_privacy_item_deny_resp_cb,
															 g_strdup(name));
						}
					} else {
						gaim_privacy_deny_remove(gc->account, (char *)node->data, TRUE);
					}
				}

			}
			break;

		case GAIM_PRIVACY_ALLOW_BUDDYLIST:

			/* remove users from allow list that are not in buddy list */
			copy = g_slist_copy(user->allow_list);
			for (node = copy; node && node->data; node = node->next) {
				if (!nm_find_contacts(user, node->data)) {
					rc = nm_send_remove_privacy_item(user, (const char *)node->data,
													 TRUE, NULL, NULL);
					if (_check_for_disconnect(user, rc))
						return;
				}
			}
			g_slist_free(copy);

			/* add all buddies to allow list */
			num_contacts = nm_folder_get_contact_count(user->root_folder);
			for (i = 0; i < num_contacts; i++) {
				contact = nm_folder_get_contact(user->root_folder, i);
				dn = nm_contact_get_dn(contact);
				if (dn && !g_slist_find_custom(user->allow_list,
											   dn, (GCompareFunc)nm_utf8_strcasecmp))
				{
					rc = nm_send_create_privacy_item(user, dn, TRUE,
													 _create_privacy_item_deny_resp_cb,
													 g_strdup(dn));
					if (_check_for_disconnect(user, rc))
						return;
				}

			}

			num_folders = nm_folder_get_subfolder_count(user->root_folder);
			for (i = 0; i < num_folders; i++) {
				folder = nm_folder_get_subfolder(user->root_folder, i);
				num_contacts = nm_folder_get_contact_count(folder);
				for (j = 0; j < num_contacts; j++) {
					contact = nm_folder_get_contact(folder, j);
					dn = nm_contact_get_dn(contact);
					if (dn && !g_slist_find_custom(user->allow_list,
												   dn, (GCompareFunc)nm_utf8_strcasecmp))
					{
						rc = nm_send_create_privacy_item(user, dn, TRUE,
														 _create_privacy_item_deny_resp_cb,
														 g_strdup(dn));
						if (_check_for_disconnect(user, rc))
							return;
					}
				}
			}

			/* set to default deny */
			rc = nm_send_set_privacy_default(user, TRUE,
											 _set_privacy_default_resp_cb, NULL);
			if (_check_for_disconnect(user, rc))
				break;

			break;
	}
}

static GList *
novell_blist_node_menu(GaimBlistNode *node)
{
	GList *list = NULL;
	GaimMenuAction *act;

	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		act = gaim_menu_action_new(_("Initiate _Chat"),
		                           GAIM_CALLBACK(_initiate_conference_cb),
		                           NULL, NULL);
		list = g_list_append(list, act);
	}

	return list;
}

static void
novell_keepalive(GaimConnection *gc)
{
	NMUser *user;
	NMERR_T rc = NM_OK;

	if (gc == NULL)
		return;

	user = gc->proto_data;
	if (user == NULL)
		return;

	rc = nm_send_keepalive(user, NULL, NULL);
	_check_for_disconnect(user, rc);
}

static GaimPluginProtocolInfo prpl_info = {
	0,
	NULL,						/* user_splits */
	NULL,						/* protocol_options */
	NO_BUDDY_ICONS,				/* icon_spec */
	novell_list_icon,			/* list_icon */
	novell_list_emblems,		/* list_emblems */
	novell_status_text,			/* status_text */
	novell_tooltip_text,		/* tooltip_text */
	novell_status_types,		/* status_types */
	novell_blist_node_menu,		/* blist_node_menu */
	NULL,						/* chat_info */
	NULL,						/* chat_info_defaults */
	novell_login,				/* login */
	novell_close,				/* close */
	novell_send_im,				/* send_im */
	NULL,						/* set_info */
	novell_send_typing,			/* send_typing */
	novell_get_info,			/* get_info */
	novell_set_status,			/* set_status */
	novell_set_idle,			/* set_idle */
	NULL,						/* change_passwd */
	novell_add_buddy,			/* add_buddy */
	NULL,						/* add_buddies */
	novell_remove_buddy,		/* remove_buddy */
	NULL,						/* remove_buddies */
	novell_add_permit,			/* add_permit */
	novell_add_deny,			/* add_deny */
	novell_rem_permit,			/* rem_permit */
	novell_rem_deny,			/* rem_deny */
	novell_set_permit_deny,		/* set_permit_deny */
	NULL,						/* join_chat */
	NULL,						/* reject_chat */
	NULL,					/* get_chat_name */
	novell_chat_invite,			/* chat_invite */
	novell_chat_leave,			/* chat_leave */
	NULL,						/* chat_whisper */
	novell_chat_send,			/* chat_send */
	novell_keepalive,			/* keepalive */
	NULL,						/* register_user */
	NULL,						/* get_cb_info */
	NULL,						/* get_cb_away */
	novell_alias_buddy,			/* alias_buddy */
	novell_group_buddy,			/* group_buddy */
	novell_rename_group,		/* rename_group */
	NULL,						/* buddy_free */
	novell_convo_closed,		/* convo_closed */
	gaim_normalize_nocase,		/* normalize */
	NULL,						/* set_buddy_icon */
	novell_remove_group,		/* remove_group */
	NULL,						/* get_cb_real_name */
	NULL,						/* set_chat_topic */
	NULL,						/* find_blist_chat */
	NULL,						/* roomlist_get_list */
	NULL,						/* roomlist_cancel */
	NULL,						/* roomlist_expand_category */
	NULL,						/* can_receive_file */
	NULL,						/* send_file */
	NULL,						/* new_xfer */
	NULL,						/* offline_message */
	NULL,						/* whiteboard_prpl_ops */
	NULL,						/* send_raw */
	NULL,						/* roomlist_room_serialize */

};

static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,			/**< type           */
	NULL,					/**< ui_requirement */
	0,					/**< flags          */
	NULL,					/**< dependencies   */
	GAIM_PRIORITY_DEFAULT,			/**< priority       */
	"prpl-novell",				/**< id             */
	"GroupWise",				/**< name           */
	VERSION,				/**< version        */
	/**  summary        */
	N_("Novell GroupWise Messenger Protocol Plugin"),
	/**  description    */
	N_("Novell GroupWise Messenger Protocol Plugin"),
	NULL,					/**< author         */
	GAIM_WEBSITE,				/**< homepage       */

	NULL,					/**< load           */
	NULL,					/**< unload         */
	NULL,					/**< destroy        */

	NULL,					/**< ui_info        */
	&prpl_info,				/**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin * plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server address"), "server", NULL);
	prpl_info.protocol_options =
		g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Server port"), "port", DEFAULT_PORT);
	prpl_info.protocol_options =
		g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(novell, init_plugin, info);
