/*
 * nmuser.h
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

#ifndef __NM_USER_H__
#define __NM_USER_H__

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

typedef guint32 NMERR_T;
typedef int NMSTATUS_T;

typedef struct _NMUser NMUser;

typedef enum
{
	NMREQUEST_TYPE_LOGIN = 0,
	NMREQUEST_TYPE_LOGOUT,
	NMREQUEST_TYPE_SETSTATUS,
	NMREQUEST_TYPE_GETDETAILS,
	NMREQUEST_TYPE_CREATECONF,
	NMREQUEST_TYPE_SENDMESSAGE,
	NMREQUEST_TYPE_JOINCONF,
	NMREQUEST_TYPE_LEAVECONF,
	NMREQUEST_TYPE_REJECTCONF,
	NMREQUEST_TYPE_SENDTYPING,
	NMREQUEST_TYPE_CREATECONTACT,
	NMREQUEST_TYPE_DELETECONTACT

} NMRequestType;

#include "debug.h"
#include "nmmessage.h"
#include "nmconference.h"
#include "nmcontact.h"
#include "nmuserrecord.h"
#include "nmfield.h"
#include "nmevent.h"

/* Callback typedefs */
typedef void (*nm_response_cb) (NMUser * user, NMERR_T ret_code,
								gpointer resp_data, gpointer user_data);

typedef void (*nm_event_cb) (NMUser * user, NMEvent * event);

#include "nmrequest.h"
#include "nmconn.h"

/* This represents user that we are currently logged in as */
struct _NMUser
{

	char *name;

	NMSTATUS_T status;

	/* A copy of the login response fields */
	NMField *fields;

	/* The user record for this user */
	NMUserRecord *user_record;

	/* Our connection information */
	NMConn *conn;

	/* Our public IP address */
	char *address;

	/* This is the contact list */
	NMFolder *root_folder;

	/* All contacts that we know about hashed by dn */
	GHashTable *contacts;

	/* All user records hashed by dn */
	GHashTable *user_records;

	/* DN lookup */
	GHashTable *display_id_to_dn;

	/* One on one conversations indexed by recipient's dn */
	GSList *conferences;

	guint32 conference_count;

	/* Called when we receive an event */
	nm_event_cb evt_callback;

	/* Pending requests. If we need to go to the server to more info
	 * before processing a request we will queue it up and process when
	 * we get a response
	 */
	GSList *pending_requests;

	/* Pending events. Same as above except for events. */
	GSList *pending_events;

	/* Generic pointer to data needed by the client
	 * using the API
	 */
	gpointer client_data;

};


#define	NM_STATUS_UNKNOWN			0
#define	NM_STATUS_OFFLINE			1
#define NM_STATUS_AVAILABLE			2
#define	NM_STATUS_BUSY				3
#define	NM_STATUS_AWAY				4
#define	NM_STATUS_AWAY_IDLE			5
#define	NM_STATUS_INVALID			6

#define NMERR_BASE							0x2000L
#define NM_OK								0L
#define NMERR_BAD_PARM						(NMERR_BASE + 0x0001)
#define NMERR_TCP_WRITE						(NMERR_BASE + 0x0002)
#define NMERR_TCP_READ						(NMERR_BASE + 0x0003)
#define NMERR_PROTOCOL						(NMERR_BASE + 0x0004)
#define NMERR_SSL_REDIRECT					(NMERR_BASE + 0x0005)
#define NMERR_CONFERENCE_NOT_FOUND 			(NMERR_BASE + 0x0006)
#define NMERR_CONFERENCE_NOT_INSTANTIATED 	(NMERR_BASE + 0x0007)
#define NMERR_FOLDER_EXISTS					(NMERR_BASE + 0x0008)


/**
 *	Initialize the user that we are going to login to the system as.
 *
 *	@param	name			The userid of the user
 *	@param	server			IP Address of server
 *	@param	port			Port to connect to on the server
 *  @param 	data			Client data to associate with the user
 *	@param	event_callback	Function to call when we receive an event
 *
 *	@return The initialized user object. Must be freed by calling
 *			nm_deinitialize_user
 */
NMUser *nm_initialize_user(const char *name, const char *server, int port,
						   gpointer data, nm_event_cb event_callback);


/**
 *	Free up resources associated with the user object.
 *
 *  @param	user	The user to deinitialize
 */
void nm_deinitialize_user(NMUser * user);

/**
 *	Send a login request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The User to login -- must be initialized
 *	@param	pwd			The password of the user
 *  @param  my_addr		The address of the client machine
 *  @param	user_agent	String describing the client (eg. "Gaim/0.76 (Linux; 2.4.20)")
 *	@param	callback	Function to call when we get the response from the server
 *  @param 	data		User defined data to be passed to the callback function
 *
 *
 *	@return	NM_OK if login is sent successfully, error otherwise.
 */
NMERR_T nm_send_login(NMUser * user, const char *pwd, const char *my_addr,
					  const char *user_agent, nm_response_cb callback,
					  gpointer data);

/**
 *	Send a set status request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	dn			The DN of the user (if known, or NULL if not known)
 *	@param	address		IP Address of server
 *	@param	callback	Function to call when we get the response from the server
 *
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_set_status(NMUser * user, int status, const char *text,
						   const char *auto_resp, nm_response_cb callback,
						   gpointer data);

/**
 *	Send a create conference to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	conference	Conference to create
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_create_conference(NMUser * user, NMConference * conference,
								  nm_response_cb callback, gpointer data);

/**
 *	Tell server we have left the conference.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	conference	Conference the user is leaving
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_leave_conference(NMUser * user, NMConference * conference,
								 nm_response_cb callback, gpointer data);

/**
 *	Send a join conference request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	conference	Conference the user is joining
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_join_conference(NMUser * user, NMConference * conference,
								nm_response_cb callback, gpointer data);

/**
 *	Send a conference reject request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	conference	Conference the user is rejecting
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_reject_conference(NMUser * user, NMConference * conference,
								  nm_response_cb callback, gpointer data);

/**
 *	Get details for a user from the server.
 *
 *	The response data sent to the callback will be an NMUserRecord which should be
 *  freed with nm_release_user_record
 *
 *  @param	user		The logged in User
 *	@param	name		Userid or DN of the user to look up
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_get_details(NMUser * user, const char *name,
							nm_response_cb callback, gpointer data);

/**
 *	Get details for multiple users from the server.
 *
 *	The response data to the callback will be a list of NMUserRecord, which should be
 *	freed (each user record should be released with nm_release_user_record and the
 *  list should be freed)
 *
 *  @param	user		The logged in User
 *	@param	name		Userid or DN of the user to look up
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data to be passed to the callback function
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_multiple_get_details(NMUser *user, GSList *names,
									 nm_response_cb callback, gpointer data);

/**
 *	Send a message.
 *
 *  The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	message		The message to send.
 *	@param	callback	Function to call when we get the response from the server
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_message(NMUser * user, NMMessage * message,
						nm_response_cb callback);

/**
 *	Sends a typing event to the server.
 *
 *  The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	conf		The conference that corresponds to the typing event
 *	@param	typing		TRUE if the user is typing
 *						FALSE if the user has stopped typing
 *	@param	callback	Function to call when we get the response from the server
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_typing(NMUser * user, NMConference * conf,
					   gboolean typing, nm_response_cb callback);

/**
 *	Send a create contact request to the server.
 *
 *  The given folder should already exist on the server. If not,
 *  the call will fail.
 *
 *  The response data sent to the callback will be a NMContact which should
 *  be released with nm_release_contact
 *
 *  @param	user		The logged in User
 *	@param	folder		The folder that the contact should be created in
 *	@param	contact		The contact to add
 *	@param	callback	Function to call when we get the response from the server
 *	@param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_create_contact(NMUser * user, NMFolder * folder,
							   NMContact * contact, nm_response_cb callback,
							   gpointer data);

/**
 *	Send a remove contact request to the server.
 *
 *  The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	folder		The folder to remove contact from
 *	@param	contact		The contact to remove
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_remove_contact(NMUser * user, NMFolder * folder,
							   NMContact * contact, nm_response_cb callback,
							   gpointer data);

/**
 *	Send a create folder request to the server.
 *
 *  The response data sent to the callback will be a NMFolder which should be
 *  released with nm_release_folder
 *
 *  @param	user		The logged in User
 *	@param	name		The name of the folder to create
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_create_folder(NMUser * user, const char *name,
							  nm_response_cb callback, gpointer data);

/**
 *	Send a delete folder request to the server.
 *
 *  The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	folder		The name of the folder to remove
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_remove_folder(NMUser * user, NMFolder * folder,
							  nm_response_cb callback, gpointer data);

/**
 *	Send a rename contact request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	contact		The contact to rename
 *  @param	new_name	The new display name for the contact
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_rename_contact(NMUser * user, NMContact * contact,
							   const char *new_name, nm_response_cb callback,
							   gpointer data);

/**
 *	Send a rename folder request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	folder		The folder to rename
 *  @param	new_name	The new name of the folder
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_rename_folder(NMUser * user, NMFolder * folder,
							  const char *new_name, nm_response_cb callback,
							  gpointer data);

/**
 *	Send a move contact request to the server.
 *
 *	The response data sent to the callback will be NULL.
 *
 *  @param	user		The logged in User
 *	@param	contact		The contact to move
 *  @param	folder	    The folder to move the contact to
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_move_contact(NMUser * user, NMContact * contact,
							 NMFolder * folder, nm_response_cb callback,
							 gpointer data);

/**
 *	Send a get status request to the server.
 *
 * 	The response data sent to the callback will be a NMUserRecord.
 *
 *  @param	user		The logged in User
 *	@param	contact		The contact to move
 *  @param	folder	    The folder to move the contact to
 *	@param	callback	Function to call when we get the response from the server
 *  @param	data		User defined data
 *
 *	@return	NM_OK if successfully sent, error otherwise
 */
NMERR_T nm_send_get_status(NMUser * user, NMUserRecord * user_record,
						   nm_response_cb callback, gpointer data);

/**
 *	Reads a response/event from the server and processes it.
 *
 *  @param	user	The logged in User
 */
NMERR_T nm_process_new_data(NMUser * user);

/**
 *	Return the root folder of the contact list
 *
 *  @param	user	The logged in User
 *
 *	@return	Root folder
 */
NMFolder *nm_get_root_folder(NMUser * user);

/**
 *	Create the contact list based on the login fields
 *
 *  @param	user	The logged in User
 *
 */
NMERR_T nm_create_contact_list(NMUser * user);

void nm_destroy_contact_list(NMUser * user);

void nm_user_add_contact(NMUser * user, NMContact * contact);

void nm_user_add_user_record(NMUser * user, NMUserRecord * user_record);

NMContact *nm_find_contact(NMUser * user, const char *dn);

GList *nm_find_contacts(NMUser * user, const char *dn);

NMUserRecord *nm_find_user_record(NMUser * user, const char *dn);

NMFolder *nm_find_folder(NMUser * user, const char *name);

NMFolder *nm_find_folder_by_id(NMUser * user, int object_id);

NMConference *nm_find_conversation(NMUser * user, const char *who);

void nm_conference_list_add(NMUser * user, NMConference * conf);

void nm_conference_list_remove(NMUser * user, NMConference * conf);

void nm_conference_list_free(NMUser * user);

NMConference *nm_conference_list_find(NMUser * user, const char *guid);

const char *nm_lookup_dn(NMUser * user, const char *display_id);

nm_event_cb nm_user_get_event_callback(NMUser * user);

NMConn *nm_user_get_conn(NMUser * user);

/** Some utility functions **/

/**
 * Check to see if the conference GUIDs are equivalent.
 *
 * @param guid1	First guid to compare
 * @param guid2 Second guid to compare
 *
 * @return 		TRUE if conference GUIDs are equivalent, FALSE otherwise.
 *
 */
gboolean nm_are_guids_equal(const char *guid1, const char *guid2);


/**
 * Case insensitive compare for utf8 strings
 *
 * @param guid1	First string to compare
 * @param guid2 Second string to compare
 *
 * @return 		-1 if str1 < str2, 0 if str1 = str2, 1 if str1 > str2
 *
 */
gint nm_utf8_strcasecmp(gconstpointer str1, gconstpointer str2);

/**
 * Compare UTF8 strings for equality only (case insensitive)
 *
 * @param guid1	First string to compare
 * @param guid2 Second string to compare
 *
 * @return 		TRUE if strings are equal, FALSE otherwise
 *
 */
gboolean nm_utf8_str_equal(gconstpointer str1, gconstpointer str2);

/**
 * Convert a fully typed LDAP DN to dotted, untype notation
 * e.g. cn=mike,o=novell -> mike.novell
 *
 * @param typed	Fully typed dn
 *
 * @return 		Dotted equivalent of typed (must be freed).
 *
 */
char *nm_typed_to_dotted(const char *typed);

#endif
