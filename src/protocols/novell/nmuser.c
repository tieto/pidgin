/*
 * nmuser.c
 *
 * Copyright © 2004 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK OF NOVELL, INC. NO PART OF THIS WORK MAY BE
 * USED, PRACTICED, PERFORMED, COPIED, DISTRIBUTED, REVISED, MODIFIED,
 * TRANSLATED, ABRIDGED, CONDENSED, EXPANDED, COLLECTED, COMPILED, LINKED,
 * RECAST, TRANSFORMED OR ADAPTED WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL,
 * INC. ANY USE OR EXPLOITATION OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 *
 * AS BETWEEN [GAIM] AND NOVELL, NOVELL GRANTS [GAIM] THE RIGHT TO REPUBLISH
 * THIS WORK UNDER THE GPL (GNU GENERAL PUBLIC LICENSE) WITH ALL RIGHTS AND
 * LICENSES THEREUNDER.  IF YOU HAVE RECEIVED THIS WORK DIRECTLY OR INDIRECTLY
 * FROM [GAIM] AS PART OF SUCH A REPUBLICATION, YOU HAVE ALL RIGHTS AND LICENSES
 * GRANTED BY [GAIM] UNDER THE GPL.  IN CONNECTION WITH SUCH A REPUBLICATION, IF
 * ANYTHING IN THIS NOTICE CONFLICTS WITH THE TERMS OF THE GPL, SUCH TERMS
 * PREVAIL.
 *
 */

#include <glib.h>
#include <string.h>
#include "nmfield.h"
#include "nmuser.h"
#include "nmconn.h"
#include "nmcontact.h"
#include "nmuserrecord.h"
#include "util.h"

/* This is the template that we wrap outgoing messages in, since the other
 * GW Messenger clients expect messages to be in RTF.
 */
#define RTF_TEMPLATE 	"{\\rtf1\\fbidis\\ansi\\ansicpg1252\\deff0\\deflang1033"\
                        "{\\fonttbl{\\f0\\fswiss\\fprq2\\fcharset0 "\
                        "Microsoft Sans Serif;}}\n{\\colortbl ;\\red0"\
                        "\\green0\\blue0;}\n\\viewkind4\\uc1\\pard\\ltrpar"\
                        "\\li50\\ri50\\cf1\\f0\\fs20 %s\\par\n}"

static NMERR_T nm_process_response(NMUser * user);

static void _update_contact_list(NMUser * user, NMField * fields);

/**
 * See header for comments on on "public" functions
 */

NMUser *
nm_initialize_user(const char *name, const char *server_addr,
				   int port, gpointer data, nm_event_cb event_callback)
{
	NMUser *user;
	if (name == NULL || server_addr == NULL || event_callback == NULL)
		return NULL;

	user = g_new0(NMUser, 1);

	user->conn = g_new0(NMConn, 1);

	user->contacts =
		g_hash_table_new_full(g_str_hash, nm_utf8_str_equal,
							  g_free, (GDestroyNotify) nm_release_contact);

	user->user_records =
		g_hash_table_new_full(g_str_hash, nm_utf8_str_equal, g_free,
							  (GDestroyNotify) nm_release_user_record);

	user->display_id_to_dn = g_hash_table_new_full(g_str_hash, nm_utf8_str_equal,
												   g_free, g_free);

	user->name = g_strdup(name);
	user->conn->addr = g_strdup(server_addr);
	user->conn->port = port;
	user->evt_callback = event_callback;
	user->client_data = data;

	return user;
}


void
nm_deinitialize_user(NMUser * user)
{
	NMConn *conn = user->conn;

	g_free(conn->addr);
	g_free(conn);

	if (user->contacts) {
		g_hash_table_destroy(user->contacts);
	}

	if (user->user_records) {
		g_hash_table_destroy(user->user_records);
	}

	if (user->display_id_to_dn) {
		g_hash_table_destroy(user->display_id_to_dn);
	}

	if (user->name) {
		g_free(user->name);
	}

	if (user->user_record) {
		nm_release_user_record(user->user_record);
	}

	nm_conference_list_free(user);
	nm_destroy_contact_list(user);

	g_free(user);
}

NMERR_T
nm_send_login(NMUser * user, const char *pwd, const char *my_addr,
			  const char *user_agent, nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL || pwd == NULL || user_agent == NULL) {
		return NMERR_BAD_PARM;
	}

	fields = nm_add_field(fields, NM_A_SZ_USERID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup(user->name), NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_SZ_CREDENTIALS, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup(pwd), NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_SZ_USER_AGENT, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup(user_agent), NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_UD_BUILD, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) NM_PROTOCOL_VERSION,
						  NMFIELD_TYPE_UDWORD);
	if (my_addr) {
		fields = nm_add_field(fields, NM_A_IP_ADDRESS, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(my_addr), NMFIELD_TYPE_UTF8);
	}

	/* Send the login */
	rc = nm_send_request(user->conn, "login", fields, &req);
	if (rc == NM_OK && req != NULL) {
		nm_request_set_callback(req, callback);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields) {
		nm_free_fields(&fields);
	}

	if (req) {
		nm_release_request(req);
	}

	return rc;
}

NMERR_T
nm_send_set_status(NMUser * user, int status, const char *text,
				   const char *auto_resp, nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL)
		return NMERR_BAD_PARM;

	/* Add the status */
	fields = nm_add_field(fields, NM_A_SZ_STATUS, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup_printf("%d", status),
						  NMFIELD_TYPE_UTF8);

	/* Add the status text and auto reply text if there is any */
	if (text) {
		fields = nm_add_field(fields, NM_A_SZ_STATUS_TEXT,
							  0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(text), NMFIELD_TYPE_UTF8);
	}

	if (auto_resp) {
		fields = nm_add_field(fields, NM_A_SZ_MESSAGE_BODY, 0,
							  NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(auto_resp), NMFIELD_TYPE_UTF8);
	}

	rc = nm_send_request(user->conn, "setstatus", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields) {
		nm_free_fields(&fields);
	}

	if (req) {
		nm_release_request(req);
	}

	return rc;
}

NMERR_T
nm_send_get_details(NMUser * user, const char *name,
					nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL || name == NULL)
		return NMERR_BAD_PARM;

	/* Add in DN or display id */
	if (strstr("=", name)) {
		fields = nm_add_field(fields, NM_A_SZ_DN, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(name), NMFIELD_TYPE_DN);
	} else {

		const char *dn = nm_lookup_dn(user, name);

		if (dn) {
			fields = nm_add_field(fields, NM_A_SZ_DN, 0, NMFIELD_METHOD_VALID, 0,
								  (guint32) g_strdup(name), NMFIELD_TYPE_DN);
		} else {
			fields =
				nm_add_field(fields, NM_A_SZ_USERID, 0, NMFIELD_METHOD_VALID, 0,
							 (guint32) g_strdup(name), NMFIELD_TYPE_UTF8);
		}

	}

	rc = nm_send_request(user->conn, "getdetails", fields, &req);
	if (rc == NM_OK) {
		nm_request_set_callback(req, callback);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_create_conference(NMUser * user, NMConference * conference,
						  nm_response_cb callback, gpointer message)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMField *tmp = NULL;
	NMField *field = NULL;
	NMRequest *req = NULL;
	int count, i;

	if (user == NULL || conference == NULL)
		return NMERR_BAD_PARM;

	/* Add in a blank guid */
	tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
					   (guint32) g_strdup(BLANK_GUID), NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_FA_CONVERSATION, 0,
						  NMFIELD_METHOD_VALID, 0, (guint32) tmp,
						  NMFIELD_TYPE_ARRAY);
	tmp = NULL;

	/* Add participants in */
	count = nm_conference_get_participant_count(conference);
	for (i = 0; i < count; i++) {
		NMUserRecord *user_record = nm_conference_get_participant(conference, i);

		if (user_record) {
			fields = nm_add_field(fields, NM_A_SZ_DN,
								  0, NMFIELD_METHOD_VALID, 0,
								  (guint32)
								  g_strdup(nm_user_record_get_dn(user_record)),
								  NMFIELD_TYPE_DN);
		}
	}

	/* Add our user in */
	field = nm_locate_field(NM_A_SZ_DN, user->fields);
	if (field) {
		fields = nm_add_field(fields, NM_A_SZ_DN,
							  0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup((char *) field->value),
							  NMFIELD_TYPE_DN);
	}

	rc = nm_send_request(user->conn, "createconf", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, conference);
		nm_request_set_user_define(req, message);
		nm_conn_add_request_item(user->conn, req);
	}

	if (req)
		nm_release_request(req);

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

NMERR_T
nm_send_leave_conference(NMUser * user, NMConference * conference,
						 nm_response_cb callback, gpointer message)
{

	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMField *tmp = NULL;
	NMRequest *req = NULL;

	if (user == NULL || conference == NULL)
		return NMERR_BAD_PARM;

	/* Add in the conference guid */
	tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
					   (guint32) g_strdup(nm_conference_get_guid(conference)),
					   NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_FA_CONVERSATION, 0,
						  NMFIELD_METHOD_VALID, 0, (guint32) tmp,
						  NMFIELD_TYPE_ARRAY);
	tmp = NULL;

	/* Send the request to the server */
	rc = nm_send_request(user->conn, "leaveconf", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, conference);
		nm_request_set_user_define(req, message);
		nm_conn_add_request_item(user->conn, req);
	}

	if (req)
		nm_release_request(req);

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

NMERR_T
nm_send_join_conference(NMUser * user, NMConference * conference,
						nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL, *tmp = NULL;
	NMRequest *req = NULL;

	if (user == NULL || conference == NULL)
		return NMERR_BAD_PARM;

	/* Add in the conference guid */
	tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
					   (guint32) g_strdup(nm_conference_get_guid(conference)),
					   NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_FA_CONVERSATION, 0,
						  NMFIELD_METHOD_VALID, 0, (guint32) tmp,
						  NMFIELD_TYPE_ARRAY);
	tmp = NULL;

	/* Send the request to the server */
	rc = nm_send_request(user->conn, "joinconf", fields, &req);

	/* Set up the request object so that we know what to do
	 * when we get a response
	 */
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, conference);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (req)
		nm_release_request(req);

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

NMERR_T
nm_send_reject_conference(NMUser * user, NMConference * conference,
						  nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMField *tmp = NULL;
	NMRequest *req = NULL;

	if (user == NULL || conference == NULL)
		return NMERR_BAD_PARM;

	/* Add in the conference guid */
	tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
					   (guint32) g_strdup(nm_conference_get_guid(conference)),
					   NMFIELD_TYPE_UTF8);

	fields = nm_add_field(fields, NM_A_FA_CONVERSATION, 0,
						  NMFIELD_METHOD_VALID, 0, (guint32) tmp,
						  NMFIELD_TYPE_ARRAY);
	tmp = NULL;

	/* Send the request to the server */
	rc = nm_send_request(user->conn, "rejectconf", fields, &req);

	/* Set up the request object so that we know what to do
	 * when we get a response
	 */
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, conference);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (req)
		nm_release_request(req);

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

NMERR_T
nm_send_message(NMUser * user, NMMessage * message, nm_response_cb callback)
{
	NMERR_T rc = NM_OK;
	const char *text;
	NMField *fields = NULL, *tmp = NULL;
	NMRequest *req = NULL;
	NMConference *conf;
	NMUserRecord *user_record;
	int count, i;

	if (user == NULL || message == NULL) {
		return NMERR_BAD_PARM;
	}

	conf = nm_message_get_conference(message);
	if (!nm_conference_is_instantiated(conf)) {
		rc = NMERR_CONFERENCE_NOT_INSTANTIATED;
	} else {

		tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) g_strdup(nm_conference_get_guid(conf)),
						   NMFIELD_TYPE_UTF8);

		fields =
			nm_add_field(fields, NM_A_FA_CONVERSATION, 0, NMFIELD_METHOD_VALID, 0,
						 (guint32) tmp, NMFIELD_TYPE_ARRAY);
		tmp = NULL;

		/* Add RTF and plain text versions of the message */
		text = nm_message_get_text(message);

		tmp = nm_add_field(tmp, NM_A_SZ_MESSAGE_BODY, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) g_strdup_printf(RTF_TEMPLATE, text),
						   NMFIELD_TYPE_UTF8);

		tmp = nm_add_field(tmp, NM_A_UD_MESSAGE_TYPE, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) 0, NMFIELD_TYPE_UDWORD);

		tmp = nm_add_field(tmp, NM_A_SZ_MESSAGE_TEXT, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) g_strdup(text), NMFIELD_TYPE_UTF8);

		fields = nm_add_field(fields, NM_A_FA_MESSAGE, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) tmp, NMFIELD_TYPE_ARRAY);
		tmp = NULL;

		/* Add participants */
		count = nm_conference_get_participant_count(conf);
		for (i = 0; i < count; i++) {
			user_record = nm_conference_get_participant(conf, i);
			if (user_record) {
				fields =
					nm_add_field(fields, NM_A_SZ_DN, 0, NMFIELD_METHOD_VALID, 0,
								 (guint32)
								 g_strdup(nm_user_record_get_dn(user_record)),
								 NMFIELD_TYPE_DN);
			}
		}

		/* Send the request */
		rc = nm_send_request(user->conn, "sendmessage", fields, &req);
		if (rc == NM_OK && req) {
			nm_request_set_callback(req, callback);
			nm_conn_add_request_item(user->conn, req);
		}
	}

	if (fields) {
		nm_free_fields(&fields);
	}

	if (req) {
		nm_release_request(req);
	}

	return rc;
}

NMERR_T
nm_send_typing(NMUser * user, NMConference * conf,
			   gboolean typing, nm_response_cb callback)
{
	NMERR_T rc = NM_OK;
	char *str = NULL;
	NMField *fields = NULL, *tmp = NULL;
	NMRequest *req = NULL;

	if (user == NULL || conf == NULL) {
		return NMERR_BAD_PARM;
	}

	if (!nm_conference_is_instantiated(conf)) {
		rc = NMERR_CONFERENCE_NOT_INSTANTIATED;
	} else {
		/* Add the conference GUID */
		tmp = nm_add_field(tmp, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) g_strdup(nm_conference_get_guid(conf)),
						   NMFIELD_TYPE_UTF8);

		/* Add typing type */
		str = g_strdup_printf("%d",
							  (typing ? NMEVT_USER_TYPING :
							   NMEVT_USER_NOT_TYPING));

		tmp = nm_add_field(tmp, NM_A_SZ_TYPE, 0, NMFIELD_METHOD_VALID, 0,
						   (guint32) str, NMFIELD_TYPE_UTF8);

		fields =
			nm_add_field(fields, NM_A_FA_CONVERSATION, 0, NMFIELD_METHOD_VALID, 0,
						 (guint32) tmp, NMFIELD_TYPE_ARRAY);
		tmp = NULL;

		rc = nm_send_request(user->conn, "sendtyping", fields, &req);
		if (rc == NM_OK && req) {
			nm_request_set_callback(req, callback);
			nm_conn_add_request_item(user->conn, req);
		}
	}

	if (req)
		nm_release_request(req);

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

NMERR_T
nm_send_create_contact(NMUser * user, NMFolder * folder,
					   NMContact * contact, nm_response_cb callback,
					   gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;
	const char *name = NULL;
	const char *display_name = NULL;

	if (user == NULL || folder == NULL || contact == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Add parent ID */
	fields = nm_add_field(fields, NM_A_SZ_PARENT_ID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup_printf("%d",
													nm_folder_get_id(folder)),
						  NMFIELD_TYPE_UTF8);

	/* Check to see if userid is current user and return an error? */

	/* Check to see if contact already exists and return an error? */

	/* Add userid or dn */
	name = nm_contact_get_dn(contact);
	if (name == NULL)
		return NMERR_BAD_PARM;

	if (strstr("=", name)) {
		fields = nm_add_field(fields, NM_A_SZ_DN, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(name), NMFIELD_TYPE_DN);
	} else {
		fields = nm_add_field(fields, NM_A_SZ_USERID, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(name), NMFIELD_TYPE_UTF8);
	}

	/* Add display name */
	display_name = nm_contact_get_display_name(contact);
	if (display_name)
		fields = nm_add_field(fields, NM_A_SZ_DISPLAY_NAME, 0, NMFIELD_METHOD_VALID, 0,
							  (guint32) g_strdup(display_name), NMFIELD_TYPE_UTF8);

	/* Dispatch the request */
	rc = nm_send_request(user->conn, "createcontact", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, contact);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_remove_contact(NMUser * user, NMFolder * folder,
					   NMContact * contact, nm_response_cb callback,
					   gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL || folder == NULL || contact == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Add parent id */
	fields = nm_add_field(fields, NM_A_SZ_PARENT_ID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup_printf("%d",
													nm_folder_get_id(folder)),
						  NMFIELD_TYPE_UTF8);

	/* Add object id */
	fields = nm_add_field(fields, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup_printf("%d",
													nm_contact_get_id(contact)),
						  NMFIELD_TYPE_UTF8);

	/* Dispatch the request */
	rc = nm_send_request(user->conn, "deletecontact", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, contact);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_create_folder(NMUser * user, const char *name,
					  nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL || name == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Add parent ID */
	fields = nm_add_field(fields, NM_A_SZ_PARENT_ID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup("0"), NMFIELD_TYPE_UTF8);

	/* Add name of the folder to add */
	fields =
		nm_add_field(fields, NM_A_SZ_DISPLAY_NAME, 0, NMFIELD_METHOD_VALID, 0,
					 (guint32) g_strdup(name), NMFIELD_TYPE_UTF8);

	/* Add sequence, for now just put it at the bottom */
	fields =
		nm_add_field(fields, NM_A_SZ_SEQUENCE_NUMBER, 0, NMFIELD_METHOD_VALID, 0,
					 (guint32) g_strdup("-1"), NMFIELD_TYPE_UTF8);

	/* Dispatch the request */
	rc = nm_send_request(user->conn, "createfolder", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, g_strdup(name));
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_remove_folder(NMUser * user, NMFolder * folder,
					  nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;

	if (user == NULL || folder == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Add the object id */
	fields = nm_add_field(fields, NM_A_SZ_OBJECT_ID, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup_printf("%d", nm_folder_get_id(folder)),
						  NMFIELD_TYPE_UTF8);

	/* Dispatch the request */
	rc = nm_send_request(user->conn, "deletecontact", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, folder);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_get_status(NMUser * user, NMUserRecord * user_record,
				   nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMRequest *req = NULL;
	const char *dn;

	if (user == NULL || user_record == NULL)
		return NMERR_BAD_PARM;

	/* Add DN to field list */
	dn = nm_user_record_get_dn(user_record);
	if (dn == NULL)
		return (NMERR_T) -1;

	fields = nm_add_field(fields, NM_A_SZ_DN, 0, NMFIELD_METHOD_VALID, 0,
						  (guint32) g_strdup(dn), NMFIELD_TYPE_UTF8);

	/* Dispatch the request */
	rc = nm_send_request(user->conn, "getstatus", fields, &req);
	if (rc == NM_OK && req) {
		nm_request_set_callback(req, callback);
		nm_request_set_data(req, user_record);
		nm_request_set_user_define(req, data);
		nm_conn_add_request_item(user->conn, req);
	}

	if (fields)
		nm_free_fields(&fields);

	if (req)
		nm_release_request(req);

	return rc;
}

NMERR_T
nm_send_rename_contact(NMUser * user, NMContact * contact,
					   const char *new_name, nm_response_cb callback,
					   gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *field = NULL, *fields = NULL, *list = NULL;
	NMRequest *req = NULL;

	if (user == NULL || contact == NULL || new_name == NULL)
		return NMERR_BAD_PARM;

	/* Create field list for current contact */
	field = nm_contact_to_fields(contact);
	if (field) {

		fields =
			nm_add_field(fields, NM_A_FA_CONTACT, 0, NMFIELD_METHOD_DELETE, 0,
						 (guint32) field, NMFIELD_TYPE_ARRAY);
		field = NULL;

		/* Update the contacts display name locally */
		nm_contact_set_display_name(contact, new_name);

		/* Create field list for updated contact */
		field = nm_contact_to_fields(contact);
		if (field) {
			fields =
				nm_add_field(fields, NM_A_FA_CONTACT, 0, NMFIELD_METHOD_ADD, 0,
							 (guint32) field, NMFIELD_TYPE_ARRAY);
			field = NULL;

			/* Package it up */
			list =
				nm_add_field(list, NM_A_FA_CONTACT_LIST, 0, NMFIELD_METHOD_VALID,
							 0, (guint32) fields, NMFIELD_TYPE_ARRAY);
			fields = NULL;

			rc = nm_send_request(user->conn, "updateitem", list, &req);
			if (rc == NM_OK && req) {
				nm_request_set_callback(req, callback);
				nm_request_set_data(req, contact);
				nm_request_set_user_define(req, data);
				nm_conn_add_request_item(user->conn, req);
			}
		}
	}

	if (list)
		nm_free_fields(&list);

	return rc;
}

NMERR_T
nm_send_rename_folder(NMUser * user, NMFolder * folder, const char *new_name,
					  nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *field = NULL, *fields = NULL, *list = NULL;
	NMRequest *req = NULL;

	if (user == NULL || folder == NULL || new_name == NULL)
		return NMERR_BAD_PARM;

	/* Make sure folder does not already exist!? */
	if (nm_find_folder(user, new_name))
		return NMERR_FOLDER_EXISTS;

	/* Create field list for current folder */
	field = nm_folder_to_fields(folder);
	if (field) {

		fields = nm_add_field(fields, NM_A_FA_FOLDER, 0, NMFIELD_METHOD_DELETE, 0,
							  (guint32) field, NMFIELD_TYPE_ARRAY);
		field = NULL;

		/* Update the folders display name locally */
		nm_folder_set_name(folder, new_name);

		/* Create field list for updated folder */
		field = nm_folder_to_fields(folder);
		if (field) {
			fields =
				nm_add_field(fields, NM_A_FA_FOLDER, 0, NMFIELD_METHOD_ADD, 0,
							 (guint32) field, NMFIELD_TYPE_ARRAY);
			field = NULL;

			/* Package it up */
			list =
				nm_add_field(list, NM_A_FA_CONTACT_LIST, 0, NMFIELD_METHOD_VALID,
							 0, (guint32) fields, NMFIELD_TYPE_ARRAY);
			fields = NULL;

			rc = nm_send_request(user->conn, "updateitem", list, &req);
			if (rc == NM_OK && req) {
				nm_request_set_callback(req, callback);
				nm_request_set_data(req, folder);
				nm_request_set_user_define(req, data);
				nm_conn_add_request_item(user->conn, req);
			}
		}
	}

	if (list)
		nm_free_fields(&list);

	return rc;
}

NMERR_T
nm_send_move_contact(NMUser * user, NMContact * contact, NMFolder * folder,
					 nm_response_cb callback, gpointer data)
{
	NMERR_T rc = NM_OK;
	NMField *field = NULL, *fields = NULL, *list = NULL;
	NMRequest *req = NULL;

	if (user == NULL || contact == NULL || folder == NULL)
		return NMERR_BAD_PARM;

	/* Create field list for the contact */
	field = nm_contact_to_fields(contact);
	if (field) {

		fields =
			nm_add_field(fields, NM_A_FA_CONTACT, 0, NMFIELD_METHOD_DELETE, 0,
						 (guint32) field, NMFIELD_TYPE_ARRAY);
		field = NULL;

		/* Wrap the contact up and add it to the request field list */
		list =
			nm_add_field(list, NM_A_FA_CONTACT_LIST, 0, NMFIELD_METHOD_VALID, 0,
						 (guint32) fields, NMFIELD_TYPE_ARRAY);
		fields = NULL;

		/* Add sequence number */
		list =
			nm_add_field(list, NM_A_SZ_SEQUENCE_NUMBER, 0, NMFIELD_METHOD_VALID,
						 0, (guint32) g_strdup("-1"), NMFIELD_TYPE_UTF8);

		/* Add parent ID */
		list = nm_add_field(list, NM_A_SZ_PARENT_ID, 0, NMFIELD_METHOD_VALID, 0,
							(guint32) g_strdup_printf("%d",
													  nm_folder_get_id(folder)),
							NMFIELD_TYPE_UTF8);

		/* Dispatch the request */
		rc = nm_send_request(user->conn, "movecontact", list, &req);
		if (rc == NM_OK && req) {
			nm_request_set_callback(req, callback);
			nm_request_set_data(req, contact);
			nm_request_set_user_define(req, data);
			nm_conn_add_request_item(user->conn, req);

		}
	}

	if (list)
		nm_free_fields(&list);

	return rc;
}


NMERR_T
nm_process_new_data(NMUser * user)
{
	NMConn *conn;
	NMERR_T rc = NM_OK;
	int ret;
	guint32 val;

	if (user == NULL)
		return NMERR_BAD_PARM;

	conn = user->conn;

	/* Check to see if this is an event or a response */
	ret = nm_tcp_read(conn, (char *) &val, sizeof(val));
	if (ret == sizeof(val)) {

		if (strncmp((char *) &val, "HTTP", strlen("HTTP")) == 0)
			rc = nm_process_response(user);
		else
			rc = nm_process_event(user, GUINT32_FROM_LE(val));

	} else {
		rc = NMERR_PROTOCOL;
	}

	return rc;
}

NMConference *
nm_find_conversation(NMUser * user, const char *who)
{
	NMConference *conference = NULL;
	NMConference *tmp;
	GSList *cnode;

	if (user && user->conferences) {
		for (cnode = user->conferences; cnode; cnode = cnode->next) {
			tmp = cnode->data;
			if (nm_conference_get_participant_count(tmp) == 1) {
				NMUserRecord *ur = nm_conference_get_participant(tmp, 0);

				if (ur) {
					if (nm_utf8_str_equal(nm_user_record_get_dn(ur), who)) {
						conference = tmp;
						break;
					}
				}
			}
		}
	}

	return conference;
}

void
nm_conference_list_add(NMUser * user, NMConference * conf)
{
	if (user == NULL || conf == NULL)
		return;

	nm_conference_add_ref(conf);
	user->conferences = g_slist_append(user->conferences, conf);
}

void
nm_conference_list_remove(NMUser * user, NMConference * conf)
{
	if (user == NULL || conf == NULL)
		return;

	if (g_slist_find(user->conferences, conf)) {
		user->conferences = g_slist_remove(user->conferences, conf);
		nm_release_conference(conf);
	}
}

void
nm_conference_list_free(NMUser * user)
{
	GSList *cnode;
	NMConference *conference;

	if (user == NULL)
		return;

	if (user->conferences) {
		for (cnode = user->conferences; cnode; cnode = cnode->next) {
			conference = cnode->data;
			cnode->data = NULL;
			nm_release_conference(conference);
		}

		g_slist_free(user->conferences);
		user->conferences = NULL;
	}
}

NMConference *
nm_conference_list_find(NMUser * user, const char *guid)
{
	GSList *cnode;
	NMConference *conference = NULL, *tmp;

	if (user == NULL || guid == NULL)
		return NULL;

	if (user->conferences) {
		for (cnode = user->conferences; cnode; cnode = cnode->next) {
			tmp = cnode->data;
			if (nm_are_guids_equal(nm_conference_get_guid(tmp), guid)) {
				conference = tmp;
				break;
			}
		}
	}

	return conference;
}

gboolean
nm_are_guids_equal(const char *guid1, const char *guid2)
{
	if (guid1 == NULL || guid2 == NULL)
		return FALSE;

	return (strncmp(guid1, guid2, CONF_GUID_END) == 0);
}

void
nm_user_add_contact(NMUser * user, NMContact * contact)
{
	if (user == NULL || contact == NULL)
		return;

	nm_contact_add_ref(contact);

	g_hash_table_insert(user->contacts,
						g_utf8_strdown(nm_contact_get_dn(contact), -1), contact);
}

void
nm_user_add_user_record(NMUser * user, NMUserRecord * user_record)
{
	nm_user_record_add_ref(user_record);

	g_hash_table_insert(user->user_records,
						g_utf8_strdown(nm_user_record_get_dn(user_record), -1),
						user_record);

	g_hash_table_insert(user->display_id_to_dn,
						g_utf8_strdown(nm_user_record_get_display_id(user_record),
									   -1),
						g_utf8_strdown(nm_user_record_get_dn(user_record), -1));

}

nm_event_cb
nm_user_get_event_callback(NMUser * user)
{
	if (user == NULL)
		return NULL;

	return user->evt_callback;
}

NMConn *
nm_user_get_conn(NMUser * user)
{
	if (user == NULL)
		return NULL;

	return user->conn;
}

NMERR_T
nm_create_contact_list(NMUser * user)
{
	NMERR_T rc = NM_OK;
	NMField *locate = NULL;

	if (user == NULL || user->fields == NULL) {
		return NMERR_BAD_PARM;
	}

	/* Create the root folder */
	user->root_folder = nm_create_folder("");

	/* Find the contact list in the login fields */
	locate = nm_locate_field(NM_A_FA_CONTACT_LIST, user->fields);
	if (locate != NULL) {

		/* Add the folders and then the contacts */
		nm_folder_add_contacts_and_folders(user, user->root_folder,
										   (NMField *) (locate->value));

	}

	return rc;
}

void
nm_destroy_contact_list(NMUser * user)
{
	if (user == NULL)
		return;

	if (user->root_folder) {
		nm_release_folder(user->root_folder);
		user->root_folder = NULL;
	}
}

NMFolder *
nm_get_root_folder(NMUser * user)
{
	if (user == NULL)
		return NULL;

	if (user->root_folder == NULL)
		nm_create_contact_list(user);

	return user->root_folder;
}

NMContact *
nm_find_contact(NMUser * user, const char *name)
{
	char *str;
	const char *dn = NULL;
	NMContact *contact = NULL;

	if (user == NULL || name == NULL)
		return NULL;

	str = g_utf8_strdown(name, -1);
	if (strstr(str, "=")) {
		dn = str;
	} else {
		/* Assume that we have a display id instead of a dn */
		dn = (const char *) g_hash_table_lookup(user->display_id_to_dn, str);
	}

	/* Find contact object in reference table */
	if (dn) {
		contact = (NMContact *) g_hash_table_lookup(user->contacts, dn);
	}

	g_free(str);
	return contact;
}

GList *
nm_find_contacts(NMUser * user, const char *dn)
{
	guint32 i, cnt;
	NMFolder *folder;
	NMContact *contact;
	GList *contacts = NULL;

	if (user == NULL || dn == NULL)
		return NULL;

	/* Check for contact at the root */
	contact = nm_folder_find_contact(user->root_folder, dn);
	if (contact) {
		contacts = g_list_append(contacts, contact);
		contact = NULL;
	}

	/* Check for contact in each subfolder */
	cnt = nm_folder_get_subfolder_count(user->root_folder);
	for (i = 0; i < cnt; i++) {
		folder = nm_folder_get_subfolder(user->root_folder, i);
		contact = nm_folder_find_contact(folder, dn);
		if (contact) {
			contacts = g_list_append(contacts, contact);
			contact = NULL;
		}
	}

	return contacts;
}

NMUserRecord *
nm_find_user_record(NMUser * user, const char *name)
{
	char *str = NULL;
	const char *dn = NULL;
	NMUserRecord *user_record = NULL;

	if (user == NULL || name == NULL)
		return NULL;

	str = g_utf8_strdown(name, -1);
	if (strstr(str, "=")) {
		dn = str;
	} else {
		/* Assume that we have a display id instead of a dn */
		dn = (const char *) g_hash_table_lookup(user->display_id_to_dn, str);
	}

	/* Find user record in reference table */
	if (dn) {
		user_record =
			(NMUserRecord *) g_hash_table_lookup(user->user_records, dn);
	}

	g_free(str);
	return user_record;
}

const char *
nm_lookup_dn(NMUser * user, const char *display_id)
{
	const char *dn;
	char *lower;

	if (user == NULL || display_id == NULL)
		return NULL;

	lower = g_utf8_strdown(display_id, -1);
	dn = g_hash_table_lookup(user->display_id_to_dn, lower);
	g_free(lower);

	return dn;
}

NMFolder *
nm_find_folder(NMUser * user, const char *name)
{
	NMFolder *folder = NULL, *temp;
	int i, num_folders;
	const char *tname = NULL;

	if (user == NULL || name == NULL)
		return NULL;

	if (*name == '\0')
		return user->root_folder;

	num_folders = nm_folder_get_subfolder_count(user->root_folder);
	for (i = 0; i < num_folders; i++) {
		temp = nm_folder_get_subfolder(user->root_folder, i);
		tname = nm_folder_get_name(temp);

		if (tname && (strcmp(tname, name) == 0)) {
			folder = temp;
			break;
		}
	}

	return folder;
}

NMFolder *
nm_find_folder_by_id(NMUser * user, int object_id)
{
	NMFolder *folder = NULL, *temp;
	int i, num_folders;

	if (user == NULL)
		return NULL;

	if (object_id == 0)
		return user->root_folder;

	num_folders = nm_folder_get_subfolder_count(user->root_folder);
	for (i = 0; i < num_folders; i++) {
		temp = nm_folder_get_subfolder(user->root_folder, i);
		if (nm_folder_get_id(temp) == object_id) {
			folder = temp;
			break;
		}
	}

	return folder;
}

static void
_handle_multiple_get_details_joinconf_cb(NMUser * user, NMERR_T ret_code,
										 gpointer resp_data, gpointer user_data)
{
	NMRequest *request = user_data;
	NMUserRecord *user_record = resp_data;
	NMConference *conference;
	GSList *list, *node;

	if (user == NULL || resp_data == NULL || user_data == NULL)
		return;

	conference = nm_request_get_data(request);
	list = nm_request_get_user_define(request);

	if (ret_code == 0 && conference && list) {

		/* Add the user to the conference */
		nm_conference_add_participant(conference, user_record);

		/* Find the user in the list and remove it */
		for (node = list; node; node = node->next) {
			if (nm_utf8_str_equal(nm_user_record_get_dn(user_record),
								  (const char *) node->data)) {
				list = g_slist_remove(list, node->data);
				nm_request_set_user_define(request, list);
				g_free(node->data);
				break;
			}
		}

		/* Time to callback? */
		if (g_slist_length(list) == 0) {
			nm_response_cb cb = nm_request_get_callback(request);

			if (cb) {
				cb(user, 0, conference, conference);
			}
			g_slist_free(list);
			nm_release_request(request);
		}
	}
}

NMERR_T
nm_call_handler(NMUser * user, NMRequest * request, NMField * fields)
{
	NMERR_T rc = NM_OK, ret_code = NM_OK;
	NMConference *conf = NULL;
	NMUserRecord *user_record = NULL;
	NMField *locate = NULL;
	NMField *field = NULL;
	const char *cmd;
	nm_response_cb cb;
	gboolean done = TRUE;

	if (user == NULL || request == NULL || fields == NULL)
		return NMERR_BAD_PARM;

	/* Get the return code */
	field = nm_locate_field(NM_A_SZ_RESULT_CODE, fields);
	if (field) {
		ret_code = atoi((char *) field->value);
	} else {
		ret_code = NMERR_PROTOCOL;
	}

	cmd = nm_request_get_cmd(request);
	if (ret_code == NM_OK && cmd != NULL) {

		if (strcmp("login", cmd) == 0) {

			user->user_record = nm_create_user_record_from_fields(fields);

			/* Save the users fields */
			user->fields = nm_copy_field_array(fields);

		} else if (strcmp("setstatus", cmd) == 0) {

			/* Nothing to do */

		} else if (strcmp("createconf", cmd) == 0) {

			conf = (NMConference *) nm_request_get_data(request);

			/* get the convo guid */
			locate = nm_locate_field(NM_A_FA_CONVERSATION, fields);
			if (locate) {
				field =
					nm_locate_field(NM_A_SZ_OBJECT_ID, (NMField *) fields->value);
				if (field) {
					nm_conference_set_guid(conf, (char *) field->value);
				}
			}

			nm_conference_list_add(user, conf);

		} else if (strcmp("leaveconf", cmd) == 0) {

			conf = (NMConference *) nm_request_get_data(request);
			nm_conference_list_remove(user, conf);

		} else if (strcmp("joinconf", cmd) == 0) {
			GSList *list = NULL, *node;

			conf = nm_request_get_data(request);

			locate = nm_locate_field(NM_A_FA_CONTACT_LIST, fields);
			if (locate && locate->value != 0) {

				field = (NMField *) locate->value;
				while ((field = nm_locate_field(NM_A_SZ_DN, field))) {
					if (field && field->value != 0) {

						if (nm_utf8_str_equal
							(nm_user_record_get_dn(user->user_record),
							 (const char *) field->value)) {
							field++;
							continue;
						}

						user_record =
							nm_find_user_record(user,
												(const char *) field->value);
						if (user_record == NULL) {
							list =
								g_slist_append(list,
											   g_strdup((char *) field->value));
						} else {
							nm_conference_add_participant(conf, user_record);
						}
					}
					field++;
				}

				if (list != NULL) {

					done = FALSE;
					nm_request_add_ref(request);
					nm_request_set_user_define(request, list);
					for (node = list; node; node = node->next) {

						nm_send_get_details(user, (const char *) node->data,
											_handle_multiple_get_details_joinconf_cb,
											request);
					}
				}
			}

		} else if (strcmp("getdetails", cmd) == 0) {

			locate = nm_locate_field(NM_A_FA_RESULTS, fields);
			if (locate && locate->value != 0) {

				user_record = nm_create_user_record_from_fields(locate);
				if (user_record) {
					NMUserRecord *tmp;

					tmp =
						nm_find_user_record(user,
											nm_user_record_get_dn(user_record));
					if (tmp) {

						/* Update the existing user record */
						nm_user_record_copy(tmp, user_record);
						nm_release_user_record(user_record);
						user_record = tmp;

					} else {

						nm_user_add_user_record(user, user_record);
						nm_release_user_record(user_record);

					}

					/* Response data is new user record */
					nm_request_set_data(request, (gpointer) user_record);
				}

			}

		} else if (strcmp("createfolder", cmd) == 0) {

			_update_contact_list(user, fields);

		} else if (strcmp("createcontact", cmd) == 0) {

			_update_contact_list(user, fields);

			locate =
				nm_locate_field(NM_A_SZ_OBJECT_ID, (NMField *) fields->value);
			if (locate) {

				NMContact *new_contact =
					nm_folder_find_item_by_object_id(user->root_folder,
													 atoi((char *) locate->
														  value));

				if (new_contact) {

					/* Add the contact to our cache */
					nm_user_add_contact(user, new_contact);

					/* Set the contact as the response data */
					nm_request_set_data(request, (gpointer) new_contact);

				}

			}

		} else if (strcmp("deletecontact", cmd) == 0) {

			_update_contact_list(user, fields);

		} else if (strcmp("movecontact", cmd) == 0) {

			_update_contact_list(user, fields);

		} else if (strcmp("getstatus", cmd) == 0) {

			locate = nm_locate_field(NM_A_SZ_STATUS, fields);
			if (locate) {
				nm_user_record_set_status((NMUserRecord *)
										  nm_request_get_data(request),
										  atoi((char *) locate->value), NULL);
			}

		} else if (strcmp("updateitem", cmd) == 0) {

			/* Nothing extra to do here */

		} else {

			/* Nothing to do, just print debug message  */
			gaim_debug(GAIM_DEBUG_INFO, "novell",
					   "nm_call_handler(): Unknown request command, %s\n", cmd);

		}
	}

	if (done && (cb = nm_request_get_callback(request))) {

		cb(user, ret_code, nm_request_get_data(request),
		   nm_request_get_user_define(request));

	}

	return rc;
}

static NMERR_T
nm_process_response(NMUser * user)
{
	NMERR_T rc = NM_OK;
	NMField *fields = NULL;
	NMField *field = NULL;
	NMConn *conn = user->conn;
	NMRequest *req = NULL;

	rc = nm_read_header(conn);
	if (rc == NM_OK) {
		rc = nm_read_fields(conn, -1, &fields);
	}

	if (rc == NM_OK) {
		field = nm_locate_field(NM_A_SZ_TRANSACTION_ID, fields);
		if (field != NULL && field->value != 0) {
			req = nm_conn_find_request(conn, atoi((char *) field->value));
			if (req != NULL) {
				rc = nm_call_handler(user, req, fields);
			}
		}
	}

	if (fields)
		nm_free_fields(&fields);

	return rc;
}

/*
 * Some utility functions...haven't figured out where
 * they belong yet.
 */
gint
nm_utf8_strcasecmp(gconstpointer str1, gconstpointer str2)
{
	gint rv;
	char *str1_down = g_utf8_strdown(str1, -1);
	char *str2_down = g_utf8_strdown(str2, -1);

	rv = g_utf8_collate(str1_down, str2_down);

	g_free(str1_down);
	g_free(str2_down);

	return rv;
}

gboolean
nm_utf8_str_equal(gconstpointer str1, gconstpointer str2)
{
	return (nm_utf8_strcasecmp(str1, str2) == 0);
}

char *
nm_typed_to_dotted(const char *typed)
{
	unsigned i = 0, j = 0;
	char *dotted;

	if (typed == NULL)
		return NULL;

	dotted = g_new0(char, strlen(typed));

	do {

		/* replace comma with a dot */
		if (j != 0) {
			dotted[j] = '.';
			j++;
		}

		/* skip the type */
		while (typed[i] != '\0' && typed[i] != '=')
			i++;

		/* verify that we aren't running off the end */
		if (typed[i] == '\0') {
			dotted[j] = '\0';
			break;
		}

		i++;

		/* copy the object name to context */
		while (typed[i] != '\0' && typed[i] != ',') {
			dotted[j] = typed[i];
			j++;
			i++;
		}

	} while (typed[i] != '\0');

	return dotted;
}

static void
_update_contact_list(NMUser * user, NMField * fields)
{
	NMField *list, *cursor, *locate;
	gint objid1;
	NMContact *contact;
	NMFolder *folder;
	gpointer item;

	if (user == NULL || fields == NULL)
		return;

	/* Is it wrapped in a RESULTS array? */
	if (strcmp(fields->tag, NM_A_FA_RESULTS) == 0) {
		list = (NMField *) fields->value;
	} else {
		list = fields;
	}

	/* Update the cached contact list */
	cursor = (NMField *) list->value;
	while (cursor->tag != NULL) {
		if ((g_ascii_strcasecmp(cursor->tag, NM_A_FA_CONTACT) == 0) ||
			(g_ascii_strcasecmp(cursor->tag, NM_A_FA_FOLDER) == 0)) {

			locate =
				nm_locate_field(NM_A_SZ_OBJECT_ID, (NMField *) cursor->value);
			if (locate != NULL && locate->value != 0) {
				objid1 = atoi((char *) locate->value);
				item =
					nm_folder_find_item_by_object_id(user->root_folder, objid1);
				if (item != NULL) {
					if (cursor->method == NMFIELD_METHOD_ADD) {
						if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_CONTACT) == 0) {
							contact = (NMContact *) item;
							nm_contact_update_list_properties(contact, cursor);
						} else if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_FOLDER)
								   == 0) {
							folder = (NMFolder *) item;
							nm_folder_update_list_properties(folder, cursor);
						}
					} else if (cursor->method == NMFIELD_METHOD_DELETE) {
						if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_CONTACT) == 0) {
							contact = (NMContact *) item;
							folder =
								nm_find_folder_by_id(user,
													 nm_contact_get_parent_id
													 (contact));
							if (folder) {
								nm_folder_remove_contact(folder, contact);
							}
						} else if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_FOLDER)
								   == 0) {
							/* TODO: write nm_folder_remove_folder */
							/* ignoring for now, should not be a big deal */
/*								folder = (NMFolder *) item;*/
/*								nm_folder_remove_folder(user->root_folder, folder);*/
						}
					}
				} else {

					if (cursor->method == NMFIELD_METHOD_ADD) {

						/* Not found,  so we need to add it */
						if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_CONTACT) == 0) {

							const char *dn = NULL;

							locate =
								nm_locate_field(NM_A_SZ_DN,
												(NMField *) cursor->value);
							if (locate != NULL && locate->value != 0) {
								dn = (const char *) locate->value;
								if (dn != NULL) {
									contact =
										nm_create_contact_from_fields(cursor);
									if (contact) {
										nm_folder_add_contact_to_list(user->
																	  root_folder,
																	  contact);
										nm_release_contact(contact);
									}
								}
							}
						} else if (g_ascii_strcasecmp(cursor->tag, NM_A_FA_FOLDER)
								   == 0) {
							folder = nm_create_folder_from_fields(cursor);
							nm_folder_add_folder_to_list(user->root_folder,
														 folder);
							nm_release_folder(folder);
						}
					}
				}
			}
		}
		cursor++;
	}
}
