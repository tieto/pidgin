/* MySpaceIM Protocol Plugin, headr file
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@homing.pidgin.im>
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


/* Conditional compilation options */

/* Debugging options */
#define MSIM_DEBUG_MSG					
/* Low-level and rarely needed */
/*#define MSIM_DEBUG_PARSE 				*/
/*#define MSIM_DEBUG_LOGIN_CHALLENGE	*/
/*#define MSIM_DEBUG_RXBUF				*/

/* RC4 didn't make it into Libpurple 2.0.0's cipher suite, so we have
 * to use our own RC4 code (from Samba) by not defining this. */
/*#define MSIM_USE_PURPLE_RC4			*/

/* TODO: when RC4 makes it into libpurple, use the PURPLE_VERSION_CHECK 
 * macro to conditionally compile. And then later, get rid of our own
 * RC4 code and only support libpurple with RC4. */

/* Constants */

/* Statuses */
#define MSIM_STATUS_ONLINE      "online"
#define MSIM_STATUS_AWAY        "away"
#define MSIM_STATUS_OFFLINE     "offline"
#define MSIM_STATUS_INVISIBLE   "invisible"

/* Build version of MySpaceIM to report to servers (1.0.xxx.0) */
#define MSIM_CLIENT_VERSION     673

/* Server */
#define MSIM_SERVER         "im.myspace.akadns.net"
//#define MSIM_SERVER         "localhost"
#define MSIM_PORT           1863        /* TODO: alternate ports and automatic */

/* Constants */
#define HASH_SIZE           0x14        /**< Size of SHA-1 hash for login */
#define NONCE_SIZE          0x20        /**< Half of decoded 'nc' field */
#define MSIM_READ_BUF_SIZE  5*1024      /**< Receive buffer size */
#define MSIM_FINAL_STRING   "\\final\\" /**< Message end marker */

/* Messages */
#define MSIM_BM_INSTANT     1
#define MSIM_BM_STATUS      100
#define MSIM_BM_ACTION      121
/*#define MSIM_BM_UNKNOWN1    122*/

/* Authentication algorithm for login2 */
#define MSIM_AUTH_ALGORITHM	196610

/* Random number in every MsimSession, to ensure it is valid. */
#define MSIM_SESSION_STRUCT_MAGIC       0xe4a6752b

/* Everything needed to keep track of a session. */
typedef struct _MsimSession
{
    guint magic;                        /**< MSIM_SESSION_STRUCT_MAGIC */
    PurpleAccount *account;
    PurpleConnection *gc;
    gchar *sesskey;                     /**< Session key text string from server */
    gchar *userid;                      /**< This user's numeric user ID */
    gint fd;                            /**< File descriptor to/from server */

    GHashTable *user_lookup_cb;         /**< Username -> userid lookup callback */
    GHashTable *user_lookup_cb_data;    /**< Username -> userid lookup callback data */
    GHashTable *user_lookup_cache;      /**< Cached information on users */

    gchar *rxbuf;                       /**< Receive buffer */
    guint rxoff;                        /**< Receive buffer offset */
	guint next_rid;						/**< Next request/response ID */
} MsimSession;

/* Check if an MsimSession is valid */
#define MSIM_SESSION_VALID(s) (session != NULL && \
		session->magic == MSIM_SESSION_STRUCT_MAGIC)

/* Callback function pointer type for when a user's information is received, 
 * initiated from a user lookup. */
typedef void (*MSIM_USER_LOOKUP_CB)(MsimSession *session, GHashTable *userinfo,
	   gpointer data);

/* Passed to MSIM_USER_LOOKUP_CB for msim_send_im_cb - called when
 * user information is available, ready to send a message. */
typedef struct _send_im_cb_struct
{
    gchar *who;
    gchar *message;
    PurpleMessageFlags flags;
} send_im_cb_struct;


/* Functions */
gboolean msim_load(PurplePlugin *plugin);
GList *msim_status_types(PurpleAccount *acct);
const gchar *msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy);

/* TODO: move these three functions to message.c/h */
gchar *msim_unescape(const gchar *msg);
gchar *msim_escape(const gchar *msg);
gchar *str_replace(const gchar* str, const gchar *old, const gchar *new);

GHashTable *msim_parse(gchar* msg);
GHashTable* msim_parse_body(const gchar *body_str);

void print_hash_item(gpointer key, gpointer value, gpointer user_data);
gboolean msim_send_raw(MsimSession *session, const gchar *msg);
gchar *msim_pack(GHashTable *table);
gboolean msim_sendh(MsimSession *session, GHashTable *table);
gboolean msim_send(MsimSession *session, ...);

void msim_login(PurpleAccount *acct);
int msim_login_challenge(MsimSession *session, GHashTable *table);
gchar* msim_compute_login_response(guchar nonce[2*NONCE_SIZE],
		        gchar* email, gchar* password, guint *response_len);

int msim_send_im(PurpleConnection *gc, const char *who,
		const char *message, PurpleMessageFlags flags);
int msim_send_im_by_userid(MsimSession *session, const gchar *userid, 
		const gchar *message, PurpleMessageFlags flags);
void msim_send_im_by_userid_cb(MsimSession *session, 
		GHashTable *userinfo, gpointer data);
void msim_incoming_im_cb(MsimSession *session, GHashTable *userinfo, 
		gpointer data);
int msim_incoming_im(MsimSession *session, GHashTable *table);

int msim_process_reply(MsimSession *session, GHashTable *table);
int msim_process(PurpleConnection *gc, GHashTable *table);

int msim_error(MsimSession *session, GHashTable *table);
void msim_status_cb(MsimSession *session, GHashTable *userinfo, 
		gpointer data);
int msim_status(MsimSession *session, GHashTable *table);
void msim_input_cb(gpointer gc_uncasted, gint source, 
		PurpleInputCondition cond);
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

void init_plugin(PurplePlugin *plugin);

#ifndef MSIM_USE_PURPLE_RC4
/* 
   Unix SMB/CIFS implementation.

   a partial implementation of RC4 designed for use in the 
   SMB authentication protocol

   Copyright (C) Andrew Tridgell 1998

   $Id: crypt-rc4.h 12116 2004-09-27 23:29:22Z guy $
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

typedef struct _rc4_state_struct {
  unsigned char s_box[256];
  unsigned char index_i;
  unsigned char index_j;
} rc4_state_struct;

void crypt_rc4_init(rc4_state_struct *rc4_state,
            const unsigned char *key, int key_len);

void crypt_rc4(rc4_state_struct *rc4_state, unsigned char *data, int data_len);
#endif	/* !MSIM_USE_PURPLE_RC4 */

#endif /* !_MYSPACE_MYSPACE_H */
