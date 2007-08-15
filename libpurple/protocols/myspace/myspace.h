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
#include <errno.h>/* for EAGAIN */
#include <stdarg.h>
#include <math.h>

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
/* Send third-party client version? (Recognized by us and Miranda's plugin) */
/*#define MSIM_SEND_CLIENT_VERSION              */

/* Debugging options */
/*#define MSIM_DEBUG_MSG                */
/* Low-level and rarely needed */
/*#define MSIM_DEBUG_PARSE             */
/*#define MSIM_DEBUG_LOGIN_CHALLENGE*/
/*#define MSIM_DEBUG_RXBUF            */

/* Define to cause init_plugin() to run some tests and print
 * the results to the Purple debug log, then exit. Useful to 
 * run with 'pidgin -d' to see the output. Don't define if
 * you want to actually use the plugin! */
/*#define MSIM_SELF_TEST            */

/* Use the attention API for zaps? */
/* Can't have until >=2.2.0, since is a new API. */
/*#define MSIM_USE_ATTENTION_API 	 */

/* Constants */

/* Maximum length of a password that is acceptable. This is the limit
 * on the official client (build 679) and on the 'new password' field at
 * http://settings.myspace.com/index.cfm?fuseaction=user.changepassword
 * (though curiously, not on the 'current password' field). */

/* Not defined; instead have the client reject the password, until libpurple
 * supports specifying a length limit on the protocol's password. */
/* #define MSIM_MAX_PASSWORD_LENGTH    10	*/

/* Build version of MySpaceIM to report to servers (1.0.xxx.0) */
#define MSIM_CLIENT_VERSION         697

/* Language codes from http://www.microsoft.com/globaldev/reference/oslocversion.mspx */
#define MSIM_LANGUAGE_ID_ENGLISH    1033
#define MSIM_LANGUAGE_NAME_ENGLISH  "ENGLISH"

/* msimprpl version string of this plugin */
#define MSIM_PRPL_VERSION_STRING    "0.14"

/* Default server */
#define MSIM_SERVER                 "im.myspace.akadns.net"
#define MSIM_PORT                   1863        /* TODO: alternate ports and automatic */

/* Time between keepalives (seconds) - if no data within this time, is dead. */
#define MSIM_KEEPALIVE_INTERVAL     (3 * 60)

/* Time to check if alive (milliseconds) */
#define MSIM_KEEPALIVE_INTERVAL_CHECK   (30 * 1000)

/* Time to check for new mail (milliseconds) */
#define MSIM_MAIL_INTERVAL_CHECK    (60 * 1000) 


/* Constants */
#define HASH_SIZE                   0x14        /**< Size of SHA-1 hash for login */
#define NONCE_SIZE                  0x20        /**< Half of decoded 'nc' field */
#define MSIM_READ_BUF_SIZE          (15 * 1024) /**< Receive buffer size */
#define MSIM_FINAL_STRING           "\\final\\" /**< Message end marker */

/* Messages */
#define MSIM_BM_INSTANT             1
#define MSIM_BM_STATUS              100
#define MSIM_BM_ACTION              121
#define MSIM_BM_MEDIA               122
#define MSIM_BM_PROFILE             124
#define MSIM_BM_UNOFFICIAL_CLIENT   200

/* Authentication algorithm for login2 */
#define MSIM_AUTH_ALGORITHM         196610

/* Recognized challenge length */
#define MSIM_AUTH_CHALLENGE_LENGTH  0x40

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
#define MSIM_LOGIN_IP_LIST_LEN         25

/* Indexes into status string (0|1|2|3|..., but 0 always empty) */
#define MSIM_STATUS_ORDINAL_EMPTY       0
#define MSIM_STATUS_ORDINAL_UNKNOWNs    1
#define MSIM_STATUS_ORDINAL_ONLINE      2
#define MSIM_STATUS_ORDINAL_UNKNOWNss   3
#define MSIM_STATUS_ORDINAL_HEADLINE    4
#define MSIM_STATUS_ORDINAL_UNKNOWNls   5
#define MSIM_STATUS_ORDINAL_UNKNOWN     6
#define MSIM_STATUS_ORDINAL_UNKNOWN1    7
#define MSIM_STATUS_ORDINAL_UNKNOWNp    8
#define MSIM_STATUS_ORDINAL_UNKNOWN2    9

/* Status codes - states a buddy (or you!) can be in. */
#define MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN    0
#define MSIM_STATUS_CODE_ONLINE               1
#define MSIM_STATUS_CODE_IDLE                 2
#define MSIM_STATUS_CODE_AWAY                 5

/* Text formatting bits for <f s=#> */
#define MSIM_TEXT_BOLD                  1
#define MSIM_TEXT_ITALIC                2   
#define MSIM_TEXT_UNDERLINE             4

/* Default baseline size of purple's fonts, in points. What is size 3 in points. 
 * _font_scale specifies scaling factor relative to this point size. Note this 
 * is only the default; it is configurable in account options. */
#define MSIM_BASE_FONT_POINT_SIZE       8

/* Default display's DPI. 96 is common but it can differ. Also configurable
 * in account options. */
#define MSIM_DEFAULT_DPI                96


/* Random number in every MsimSession, to ensure it is valid. */
#define MSIM_SESSION_STRUCT_MAGIC       0xe4a6752b

/* Inbox status bitfield values for MsimSession.inbox_status */
#define MSIM_INBOX_MAIL                 (1 << 0)
#define MSIM_INBOX_BLOG_COMMENT         (1 << 1)
#define MSIM_INBOX_PROFILE_COMMENT      (1 << 2)
#define MSIM_INBOX_FRIEND_REQUEST       (1 << 3)
#define MSIM_INBOX_PICTURE_COMMENT      (1 << 4)

/* Everything needed to keep track of a session (proto_data field in PurpleConnection) */
typedef struct _MsimSession
{
	guint magic;                        /**< MSIM_SESSION_STRUCT_MAGIC */
	PurpleAccount *account;
	PurpleConnection *gc;
	guint sesskey;                      /**< Session key from server */
	guint userid;                       /**< This user's numeric user ID */
	gchar *username;                    /**< This user's unique username */
	gint fd;                            /**< File descriptor to/from server */

	/* TODO: Remove. */
	GHashTable *user_lookup_cb;         /**< Username -> userid lookup callback */
	GHashTable *user_lookup_cb_data;    /**< Username -> userid lookup callback data */

	GHashTable *server_info;            /**< Parameters from server */

	gchar *rxbuf;                       /**< Receive buffer */
	guint rxoff;                        /**< Receive buffer offset */
	guint next_rid;                     /**< Next request/response ID */
	time_t last_comm;                   /**< Time received last communication */
	guint inbox_status;                 /**< Bit field of inbox notifications */
} MsimSession;

/* Check if an MsimSession is valid */
#define MSIM_SESSION_VALID(s) (session != NULL && session->magic == MSIM_SESSION_STRUCT_MAGIC)

/* Hold ephemeral information about buddies, for proto_data of PurpleBuddy. */
/* GHashTable? */
typedef struct _MsimUser
{
	PurpleBuddy *buddy;
	guint client_cv;
	gchar *client_info;
	guint age;
	gchar *gender;
	gchar *location;
	guint total_friends;
	gchar *headline;
	gchar *display_name;
	/* Note: uid is in &buddy->node (set_blist_node_int), since it never changes */
	gchar *username;
	gchar *band_name, *song_name;
	gchar *image_url;
} MsimUser;


#ifdef MSIM_USE_ATTENTION_API
#define MsimAttentionType PurpleAttentionType
#else
/* Different kinds of attention alerts. Not yet in libpurple, so define 
 * our own structure here. */
typedef struct _MsimAttentionType MsimAttentionType;

/** A type of "attention" message (zap, nudge, buzz, etc. depending on the
 * protocol) that can be sent and received. */
struct _MsimAttentionType {
	PurpleStoredImage *icon;
	const gchar *description;		/**< Shown before sending. */
	const gchar *incoming_description;	/**< Shown when sent. */
	const gchar *outgoing_description;	/**< Shown when received. */
};
#endif

gchar *str_replace(const gchar *str, const gchar *old, const gchar *new);

/* Callback function pointer type for when a user's information is received, 
 * initiated from a user lookup. */
typedef void (*MSIM_USER_LOOKUP_CB)(MsimSession *session, MsimMessage *userinfo, gpointer data);

/* Functions */
gboolean msim_load(PurplePlugin *plugin);
GList *msim_status_types(PurpleAccount *acct);

GList *msim_attention_types(PurpleAccount *acct);
gboolean msim_send_attention(PurpleConnection *gc, gchar *username, guint code);

GList *msim_blist_node_menu(PurpleBlistNode *node);

const gchar *msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy);

gboolean msim_send_raw(MsimSession *session, const gchar *msg);

void msim_login(PurpleAccount *acct);

int msim_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
PurpleMessageFlags flags);

typedef void (*MSIM_XMLNODE_CONVERT)(MsimSession *, xmlnode *, gchar **, gchar **);

unsigned int msim_send_typing(PurpleConnection *gc, const gchar *name, PurpleTypingState state);
void msim_get_info(PurpleConnection *gc, const gchar *name);

void msim_set_status(PurpleAccount *account, PurpleStatus *status);
void msim_set_idle(PurpleConnection *gc, int time);

void msim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void msim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);

gboolean msim_offline_message(const PurpleBuddy *buddy);

MsimSession *msim_session_new(PurpleAccount *acct);
void msim_session_destroy(MsimSession *session);

void msim_close(PurpleConnection *gc);

char *msim_status_text(PurpleBuddy *buddy);
void msim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full);
GList *msim_actions(PurplePlugin *plugin, gpointer context);

#ifdef MSIM_SELF_TEST
void msim_test_all(void) __attribute__((__noreturn__));
int msim_test_msg(void);
int msim_test_escaping(void);
#endif

void init_plugin(PurplePlugin *plugin);

#endif /* !_MYSPACE_MYSPACE_H */
