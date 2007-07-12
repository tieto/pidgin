/* MySpaceIM Protocol Plugin, header file
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

#ifndef _MYSPACE_MYSPACE_H
#define _MYSPACE_MYSPACE_H

/* Other includes */
#include <string.h>
#include <errno.h>	/* for EAGAIN */
#include <stdarg.h>

#include <glib.h>

#ifdef _WIN32
#include "win32dep.h"
#else
/* For recv() and send(); needed to match Win32 */
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include "internal.h"

#include "notify.h"
#include "plugin.h"
#include "accountopt.h"
#include "version.h"
#include "cipher.h"     /* for SHA-1 */
#include "util.h"       /* for base64 */
#include "debug.h"      /* for purple_debug_info */
#include "xmlnode.h"

/* MySpaceIM includes */
#include "message.h"

/* Conditional compilation options */

/* Debugging options */
#define MSIM_DEBUG_MSG					
/* Low-level and rarely needed */
/*#define MSIM_DEBUG_PARSE 				*/
/*#define MSIM_DEBUG_LOGIN_CHALLENGE	*/
/*#define MSIM_DEBUG_RXBUF				*/

/* Define to cause init_plugin() to run some tests and print
 * the results to the Purple debug log, then exit. Useful to 
 * run with 'pidgin -d' to see the output. Don't define if
 * you want to actually use the plugin! */
/*#define MSIM_SELF_TEST				*/

/* Constants */

/* Maximum length of a password that is acceptable. This is the limit
 * on the official client (build 679) and on the 'new password' field at
 * http://settings.myspace.com/index.cfm?fuseaction=user.changepassword
 * (though curiously, not on the 'current password' field). */
#define MSIM_MAX_PASSWORD_LENGTH 	10

/* Build version of MySpaceIM to report to servers (1.0.xxx.0) */
#define MSIM_CLIENT_VERSION     673

/* Default server */
#define MSIM_SERVER         "im.myspace.akadns.net"
#define MSIM_PORT           1863        /* TODO: alternate ports and automatic */

/* Time between keepalives (seconds) - if no data within this time, is dead. */
#define MSIM_KEEPALIVE_INTERVAL         (3 * 60)

/* Time to check if alive (milliseconds) */
#define MSIM_KEEPALIVE_INTERVAL_CHECK   (30 * 1000)

/* Constants */
#define HASH_SIZE           0x14        /**< Size of SHA-1 hash for login */
#define NONCE_SIZE          0x20        /**< Half of decoded 'nc' field */
#define MSIM_READ_BUF_SIZE  (5 * 1024)  /**< Receive buffer size */
#define MSIM_FINAL_STRING   "\\final\\" /**< Message end marker */

/* Messages */
#define MSIM_BM_INSTANT     1
#define MSIM_BM_STATUS      100
#define MSIM_BM_ACTION      121
/* #define MSIM_BM_UNKNOWN1    122 */

/* Authentication algorithm for login2 */
#define MSIM_AUTH_ALGORITHM	196610

/* Recognized challenge length */
#define MSIM_AUTH_CHALLENGE_LENGTH      0x40

/* TODO: obtain IPs of network interfaces from user's machine, instead of
 * hardcoding these values below (used in msim_compute_login_response). 
 * This is not immediately
 * important because you can still connect and perform basic
 * functions of the protocol. There is also a high chance that the addreses
 * are RFC1918 private, so the servers couldn't do anything with them
 * anyways except make note of that fact. Probably important for any
 * kind of direct connection, or file transfer functionality.
 */

#define MSIM_LOGIN_IP_LIST  "\x00\x00\x00\x00\x05\x7f\x00\x00\x01\x00\x00\x00\x00\x0a\x00\x00\x40\xc0\xa8\x58\x01\xc0\xa8\x3c\x01"
#define MSIM_LOGIN_IP_LIST_LEN	25

/* Indexes into status string (0|1|2|3|..., but 0 always empty) */
#define MSIM_STATUS_ORDINAL_EMPTY		0
#define MSIM_STATUS_ORDINAL_UNKNOWNs	1
#define MSIM_STATUS_ORDINAL_ONLINE		2
#define MSIM_STATUS_ORDINAL_UNKNOWNss	3
#define MSIM_STATUS_ORDINAL_HEADLINE	4
#define MSIM_STATUS_ORDINAL_UNKNOWNls	5
#define MSIM_STATUS_ORDINAL_UNKNOWN		6
#define MSIM_STATUS_ORDINAL_UNKNOWN1	7
#define MSIM_STATUS_ORDINAL_UNKNOWNp	8
#define MSIM_STATUS_ORDINAL_UNKNOWN2	9

/* Status codes - states a buddy (or you!) can be in. */
#define MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN		0
#define MSIM_STATUS_CODE_ONLINE			1
#define MSIM_STATUS_CODE_IDLE           2
#define MSIM_STATUS_CODE_AWAY			5

/* Text formatting bits for <f s=#> */
#define MSIM_TEXT_BOLD                  1
#define MSIM_TEXT_ITALIC                2   
#define MSIM_TEXT_UNDERLINE             4

/* Random number in every MsimSession, to ensure it is valid. */
#define MSIM_SESSION_STRUCT_MAGIC       0xe4a6752b

/* Everything needed to keep track of a session. */
typedef struct _MsimSession
{
    guint magic;                        /**< MSIM_SESSION_STRUCT_MAGIC */
    PurpleAccount *account;
    PurpleConnection *gc;
    guint sesskey;                      /**< Session key from server */
    guint userid;                       /**< This user's numeric user ID */
	gchar *username;					/**< This user's unique username */
    gint fd;                            /**< File descriptor to/from server */

	/* TODO: Remove. */
    GHashTable *user_lookup_cb;         /**< Username -> userid lookup callback */
    GHashTable *user_lookup_cb_data;    /**< Username -> userid lookup callback data */

    gchar *rxbuf;                       /**< Receive buffer */
    guint rxoff;                        /**< Receive buffer offset */
	guint next_rid;						/**< Next request/response ID */
    time_t last_comm;                   /**< Time received last communication */
} MsimSession;

/* Check if an MsimSession is valid */
#define MSIM_SESSION_VALID(s) (session != NULL && \
		session->magic == MSIM_SESSION_STRUCT_MAGIC)

/* Callback function pointer type for when a user's information is received, 
 * initiated from a user lookup. */
typedef void (*MSIM_USER_LOOKUP_CB)(MsimSession *session, MsimMessage *userinfo,
          gpointer data);

/* Functions */
gboolean msim_load(PurplePlugin *plugin);
GList *msim_status_types(PurpleAccount *acct);
const gchar *msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy);

/* TODO: move these three functions to message.c/h */
gchar *msim_unescape_or_escape(gchar *msg, gboolean escape);
gchar *msim_unescape(const gchar *msg);
gchar *msim_escape(const gchar *msg);
gchar *str_replace(const gchar *str, const gchar *old, const gchar *new);

void print_hash_item(gpointer key, gpointer value, gpointer user_data);
gboolean msim_send_raw(MsimSession *session, const gchar *msg);
int msim_send_really_raw(PurpleConnection *gc, const char *buf,int total_bytes);

void msim_login(PurpleAccount *acct);
gboolean msim_login_challenge(MsimSession *session, MsimMessage *msg);
const gchar *msim_compute_login_response(const gchar nonce[2 * NONCE_SIZE],
		        const gchar *email, const gchar *password, guint *response_len);


int msim_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
	PurpleMessageFlags flags);
gboolean msim_send_bm(MsimSession *session, const gchar *who, const gchar *text, int type);
void msim_send_im_cb(MsimSession *session, MsimMessage *userinfo, gpointer data);

void msim_unrecognized(MsimSession *session, MsimMessage *msg, gchar *note);


typedef void (*MSIM_XMLNODE_CONVERT)(xmlnode *, gchar **, gchar **);
void msim_markup_tag_to_html(xmlnode *root, gchar **begin, gchar **end);
void html_tag_to_msim_markup(xmlnode *root, gchar **begin, gchar **end);
gchar *msim_convert_xml(const gchar *raw, MSIM_XMLNODE_CONVERT f);

/* High-level msim markup <=> html conversion functions. */
gchar *msim_markup_to_html(const gchar *raw);
gchar *html_to_msim_markup(const gchar *raw);

int msim_incoming_im(MsimSession *session, MsimMessage *msg);
int msim_incoming_action(MsimSession *session, MsimMessage *msg);

unsigned int msim_send_typing(PurpleConnection *gc, const gchar *name, PurpleTypingState state);
void msim_get_info_cb(MsimSession *session, MsimMessage *userinfo, gpointer data);
void msim_get_info(PurpleConnection *gc, const gchar *name);

void msim_set_status(PurpleAccount *account, PurpleStatus *status);
void msim_set_idle(PurpleConnection *gc, int time);
void msim_set_status_code(MsimSession *session, guint code, gchar *statstring);

void msim_store_buddy_info_each(gpointer key, gpointer value, gpointer user_data);
gboolean msim_store_buddy_info(MsimSession *session, MsimMessage *msg);
gboolean msim_process_reply(MsimSession *session, MsimMessage *msg);

gboolean msim_preprocess_incoming(MsimSession *session, MsimMessage *msg);

gboolean msim_check_alive(gpointer data);
gboolean msim_we_are_logged_on(MsimSession *session, MsimMessage *msg);

gboolean msim_process(MsimSession *session, MsimMessage *msg);

MsimMessage *msim_do_postprocessing(MsimMessage *msg, const gchar *uid_field_name, const gchar *uid_before, guint uid);
void msim_postprocess_outgoing_cb(MsimSession *session, MsimMessage *userinfo, gpointer data);
gboolean msim_postprocess_outgoing(MsimSession *session, MsimMessage *msg, const gchar *username, 
	const gchar *uid_field_name, const gchar *uid_before);


gboolean msim_error(MsimSession *session, MsimMessage *msg);
gboolean msim_status(MsimSession *session, MsimMessage *msg);

void msim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void msim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);

gboolean msim_offline_message(const PurpleBuddy *buddy);

void msim_input_cb(gpointer gc_uncasted, gint source, 
		PurpleInputCondition cond);

guint msim_new_reply_callback(MsimSession *session, MSIM_USER_LOOKUP_CB cb, gpointer data);

void msim_connect_cb(gpointer data, gint source, 
		const gchar *error_message);

MsimSession *msim_session_new(PurpleAccount *acct);
void msim_session_destroy(MsimSession *session);

void msim_close(PurpleConnection *gc);

gboolean msim_is_userid(const gchar *user);
gboolean msim_is_email(const gchar *user);

void msim_lookup_user(MsimSession *session, const gchar *user, 
		MSIM_USER_LOOKUP_CB cb, gpointer data);

char *msim_status_text(PurpleBuddy *buddy);
void msim_tooltip_text(PurpleBuddy *buddy, 
		PurpleNotifyUserInfo *user_info, gboolean full);

#ifdef MSIM_SELF_TEST
void msim_test_all(void) __attribute__((__noreturn__));
int msim_test_xml(void);
int msim_test_msg(void);
int msim_test_escaping(void);
#endif

void init_plugin(PurplePlugin *plugin);

#endif /* !_MYSPACE_MYSPACE_H */
