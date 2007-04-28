/* MySpaceIM Protocol Plugin
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <myspaceim@xyzzy.cjb.net>
 *
 * Based on Purple's "C Plugin HOWTO" hello world example.
 *
 * Code also drawn from myspace:
 *  http://snarfed.org/space/purple+mock+protocol+plugin
 *  Copyright (C) 2004-2007, Ryan Barrett <mockprpl@ryanb.org>
 *
 * and some constructs also based on existing Purple plugins, which are:
 *   Copyright (C) 2003, Robbert Haarman <purple@inglorion.net>
 *   Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
 *   Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 *   Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#define PURPLE_PLUGIN

#include <string.h>
#include <errno.h>	/* for EAGAIN */

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
#include "version.h"
#include "cipher.h"     /* for SHA-1 */
#include "util.h"       /* for base64 */
#include "debug.h"      /* for purple_debug_info */

#define MSIM_STATUS_ONLINE      "online"
#define MSIM_STATUS_AWAY     	"away"
#define MSIM_STATUS_OFFLINE   	"offline"
#define MSIM_STATUS_INVISIBLE 	"invisible"

/* Build version of MySpaceIM to report to servers */
#define MSIM_CLIENT_VERSION     673

#define MSIM_SERVER         "im.myspace.akadns.net"
//#define MSIM_SERVER         "localhost"
#define MSIM_PORT           1863        /* TODO: alternate ports and automatic */

/* Constants */
#define HASH_SIZE           0x14        /**< Size of SHA-1 hash for login */
#define NONCE_HALF_SIZE     0x20        /**< Half of decoded 'nc' field */
#define MSIM_READ_BUF_SIZE  5*1024      /**< Receive buffer size */
#define MSIM_FINAL_STRING   "\\final\\" /**< Message end marker */

/* Messages */
#define MSIM_BM_INSTANT     1
#define MSIM_BM_STATUS      100
#define MSIM_BM_ACTION      121
/*#define MSIM_BM_UNKNOWN1    122*/

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
} MsimSession;

#define MSIM_SESSION_VALID(s) (session != NULL && session->magic == MSIM_SESSION_STRUCT_MAGIC)

/* Callback for when a user's information is received, initiated from a user lookup. */
typedef void (*MSIM_USER_LOOKUP_CB)(MsimSession *session, GHashTable *userinfo, gpointer data);

/* Passed to MSIM_USER_LOOKUP_CB for msim_send_im_cb - called when
 * user information is available, ready to send a message. */
typedef struct _send_im_cb_struct
{
    gchar *who;
    gchar *message;
    PurpleMessageFlags flags;
} send_im_cb_struct;


/* TODO: .h file */
static void msim_lookup_user(MsimSession *session, const gchar *user, MSIM_USER_LOOKUP_CB cb, gpointer data);
static inline gboolean msim_is_userid(const gchar *user);
static void msim_session_destroy(MsimSession *session);

static void init_plugin(PurplePlugin *plugin) 
{
    purple_notify_message(plugin, PURPLE_NOTIFY_MSG_INFO, "Hello World!",
                        "This is the Hello World! plugin :)", NULL, NULL, NULL);
}

/**
 * Get possible user status types. Based on mockprpl.
 *
 * @return GList of status types.
 */
static GList *msim_status_types(PurpleAccount *acct)
{
    GList *types;
    PurpleStatusType *type;

    purple_debug_info("myspace", "returning status types for %s: %s, %s, %s\n",
                  acct->username,
                  MSIM_STATUS_ONLINE, MSIM_STATUS_AWAY, MSIM_STATUS_OFFLINE, MSIM_STATUS_INVISIBLE);


    types = NULL;

    type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, MSIM_STATUS_ONLINE,
                              MSIM_STATUS_ONLINE, TRUE);
    purple_status_type_add_attr(type, "message", "Online",
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_AWAY, MSIM_STATUS_AWAY,
                              MSIM_STATUS_AWAY, TRUE);
    purple_status_type_add_attr(type, "message", "Away",
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_OFFLINE, MSIM_STATUS_OFFLINE,
                              MSIM_STATUS_OFFLINE, TRUE);
    purple_status_type_add_attr(type, "message", "Offline",
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_INVISIBLE, MSIM_STATUS_INVISIBLE,
                              MSIM_STATUS_INVISIBLE, TRUE);
    purple_status_type_add_attr(type, "message", "Invisible",
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    return types;
}

/** 
 * Parse a MySpaceIM protocol message into a hash table. 
 *
 * @param msg The message string to parse, will be g_free()'d.
 *
 * @return Hash table of message. Caller should destroy with
 *              g_hash_table_destroy() when done.
 */
static GHashTable* msim_parse(gchar* msg)
{
    GHashTable *table;
    gchar *token;
    gchar **tokens;
    gchar *key;
    gchar *value;
    int i;

    g_return_val_if_fail(msg != NULL, NULL);

    purple_debug_info("msim", "msim_parse: got <%s>\n", msg);

    key = NULL;

    /* All messages begin with a \ */
    if (msg[0] != '\\' || msg[1] == 0)
    {
        purple_debug_info("msim", "msim_parse: incomplete/bad msg, "
                "missing initial backslash: <%s>\n", msg);
        /* XXX: Should we try to recover, and read to first backslash? */

        g_free(msg);
        return NULL;
    }

    /* Create table of strings, set to call g_free on destroy. */
    table = g_hash_table_new_full((GHashFunc)g_str_hash, 
            (GEqualFunc)g_str_equal, g_free, g_free);

    for (tokens = g_strsplit(msg + 1, "\\", 0), i = 0; 
            (token = tokens[i]);
            i++)
    {
        //printf("tok=<%s>, i%2=%d\n", token, i % 2);
        if (i % 2)
        {
            value = token;

            /* Check if key already exists */
            if (g_hash_table_lookup(table, key) == NULL)
            {
                //printf("insert: |%s|=|%s|\n", key, value);
                g_hash_table_insert(table, g_strdup(key), g_strdup(value));
            } else {
                /* TODO: Some dictionaries have multiple values for the same
                 * key. Should append to a GList to handle this case. */
                purple_debug_info("msim", "msim_parse: key %s already exists, "
                "not overwriting or replacing; ignoring new value %s\n", key,
                value);
            }
        } else {
            key = token;
        }
    }
    g_strfreev(tokens);

    /* Can free now since all data was copied to hash key/values */
    g_free(msg);

    return table;
}

/**
 * Compute the base64'd login challenge response based on username, password, nonce, and IPs.
 *
 * @param nonce The base64 encoded nonce ('nc') field from the server.
 * @param email User's email address (used as login name).
 * @param password User's cleartext password.
 *
 * @return Encoded login challenge response, ready to send to the server. Must be g_free()'d
 *         when finished.
 */
static gchar* msim_compute_login_response(guchar nonce[2*NONCE_HALF_SIZE], 
		gchar* email, gchar* password)
{
    PurpleCipherContext *key_context;
    PurpleCipher *sha1;
	PurpleCipherContext *rc4;
    guchar hash_pw[HASH_SIZE];
    guchar key[HASH_SIZE];
    gchar* password_utf16le;
    guchar* data;
	guchar* data_out;
    gchar* response;
	size_t data_len, data_out_len;
	gsize conv_bytes_read, conv_bytes_written;
	GError* conv_error;

    //memset(nonce, 0, NONCE_HALF_SIZE);
    //memset(nonce + NONCE_HALF_SIZE, 1, NONCE_HALF_SIZE);

    /* Convert ASCII password to UTF16 little endian */
    purple_debug_info("msim", "converting password to UTF-16LE\n");
	conv_error = NULL;
	password_utf16le = g_convert(password, -1, "UTF-16LE", "UTF-8", 
			&conv_bytes_read, &conv_bytes_written, &conv_error);
	g_assert(conv_bytes_read == strlen(password));
	if (conv_error != NULL)
	{
		purple_debug_error("msim", 
				"g_convert password UTF8->UTF16LE failed: %s",
				conv_error->message);
		g_error_free(conv_error);
	}

#if 0
    password_utf16le = g_new0(gchar, strlen(password) * 2);
    for (i = 0; i < strlen(password) * 2; i += 2)
    {
        password_utf16le[i] = password[i / 2];
        password_utf16le[i + 1] = 0;
    }
#endif
  
    /* Compute password hash */ 
    purple_cipher_digest_region("sha1", (guchar*)password_utf16le, 
			conv_bytes_written, sizeof(hash_pw), hash_pw, NULL);
	g_free(password_utf16le);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    printf("pwhash = ");
    for (i = 0; i < sizeof(hash_pw); i++)
        printf("%.2x ", hash_pw[i]);
    printf("\n");
#endif

    /* key = sha1(sha1(pw) + nonce2) */
    sha1 = purple_ciphers_find_cipher("sha1");
    key_context = purple_cipher_context_new(sha1, NULL);
    purple_cipher_context_append(key_context, hash_pw, HASH_SIZE);
    purple_cipher_context_append(key_context, nonce + NONCE_HALF_SIZE, NONCE_HALF_SIZE);
    purple_cipher_context_digest(key_context, sizeof(key), key, NULL);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    printf("key = ");
    for (i = 0; i < sizeof(key); i++)
    {
        printf("%.2x ", key[i]);
    }
    printf("\n");
#endif

	rc4 = purple_cipher_context_new_by_name("rc4", NULL);

    /* Note: 'key' variable is 0x14 bytes (from SHA-1 hash), 
     * but only first 0x10 used for the RC4 key. */
	purple_cipher_context_set_option(rc4, "key_len", (gpointer)0x10);
	purple_cipher_context_set_key(rc4, key);

    /* TODO: obtain IPs of network interfaces. This is not immediately
     * important because you can still connect and perform basic
     * functions of the protocol. There is also a high chance that the addreses
     * are RFC1918 private, so the servers couldn't do anything with them
     * anyways except make note of that fact. Probably important for any
     * kind of direct connection, or file transfer functionality.
     */
    /* rc4 encrypt:
     * nonce1+email+IP list */
    data_len = NONCE_HALF_SIZE + strlen(email) + 25;
    data = g_new0(guchar, data_len);
    memcpy(data, nonce, NONCE_HALF_SIZE);
    memcpy(data + NONCE_HALF_SIZE, email, strlen(email));
    memcpy(data + NONCE_HALF_SIZE + strlen(email),
            /* IP addresses of network interfaces */
            "\x00\x00\x00\x00\x05\x7f\x00\x00\x01\x00\x00\x00\x00\x0a\x00\x00\x40\xc0\xa8\x58\x01\xc0\xa8\x3c\x01", 25);

	data_out = g_new0(guchar, data_len);
    purple_cipher_context_encrypt(rc4, (const guchar*)data, 
			data_len, data_out, &data_out_len);
	g_assert(data_out_len == data_len);
	purple_cipher_context_destroy(rc4);

    response = purple_base64_encode(data_out, data_out_len);
	g_free(data_out);
#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    printf("response=<%s>\n", response);
#endif

    return response;
}

static void print_hash_item(gpointer key, gpointer value, gpointer user_data)
{
    printf("%s=%s\n", (char*)key, (char*)value);
}

/** 
 * Send an arbitrary protocol message.
 *
 * @param session 
 * @param msg The textual, encoded message to send.
 *
 * Note: this does not send instant messages. For that, see msim_send_im.
 */
static void msim_send(MsimSession *session, const gchar *msg)
{
    int ret;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(msg != NULL);
   
    purple_debug_info("msim", "msim_send: writing <%s>\n", msg);

    ret = send(session->fd, msg, strlen(msg), 0);

    if (ret != strlen(msg))
    {
        purple_debug_info("msim", 
				"msim_send(%s): strlen=%d, but only wrote %s\n",
                msg, strlen(msg), ret);
        /* TODO: better error */
    }
}

/**
 * Process a login challenge, sending a response. 
 *
 * @param session 
 * @param table Hash table of login challenge message.
 *
 * @return 0, since the 'table' parameter is no longer needed.
 */
static int msim_login_challenge(MsimSession *session, GHashTable *table) 
{
    PurpleAccount *account;
    gchar *nc_str;
    guchar *nc;
    gchar *response_str;
    gsize nc_len;
    gchar *buf;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    nc_str = g_hash_table_lookup(table, "nc");

    account = session->account;
    //assert(account);

    purple_connection_update_progress(session->gc, "Reading challenge", 1, 4);

    purple_debug_info("msim", "nc=<%s>\n", nc_str);

    nc = (guchar*)purple_base64_decode(nc_str, &nc_len);
    purple_debug_info("msim", "base64 decoded to %d bytes\n", nc_len);
    if (nc_len != 0x40)
    {
        purple_debug_info("msim", "bad nc length: %x != 0x40\n", nc_len);
        purple_connection_error(session->gc, "Unexpected challenge length from server");
        return 0;
    }

    purple_connection_update_progress(session->gc, "Logging in", 2, 4);

    printf("going to compute login response\n");
    //response_str = msim_compute_login_response(nc_str, "testuser", "testpw"); //session->gc->account->username, session->gc->account->password);
    response_str = msim_compute_login_response(nc, account->username, account->password);
    printf("got back login response\n");

    g_free(nc);

    /* Reply */
    buf = g_strdup_printf("\\login2\\%d\\username\\%s\\response\\%s\\clientver\\%d\\reconn\\%d\\status\\%d\\id\\1\\final\\",
            196610, account->username, response_str, MSIM_CLIENT_VERSION, 0, 100);

    g_free(response_str);
    
    purple_debug_info("msim", "response=<%s>\n", buf);

    msim_send(session, buf);

    g_free(buf);

    return 0;
}

/**
 * Parse a \x1c-separated "dictionary" of key=value pairs into a hash table.
 *
 * @param body_str The text of the dictionary to parse. Often the
 *                  value for the 'body' field.
 *
 * @return Hash table of the keys and values. Must g_hash_table_destroy() when done.
 */
static GHashTable *msim_parse_body(const gchar *body_str)
{
    GHashTable *table;
    gchar *item;
    gchar **items;
    gchar **elements;
    guint i;

    g_return_val_if_fail(body_str != NULL, NULL);

    table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
 
    for (items = g_strsplit(body_str, "\x1c", 0), i = 0; 
        (item = items[i]);
        i++)
    {
        gchar *key, *value;

        //printf("TOK=<%s>\n", token);
        elements = g_strsplit(item, "=", 2);

        key = elements[0];
        if (!key)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null key\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

        value = elements[1];
        if (!value)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null value\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

        //printf("-- %s: %s\n", key, value);

        /* XXX: This overwrites duplicates. */
        /* TODO: make the GHashTable values be GList's, and append to the list if 
         * there is already a value of the same key name. This is important for
         * the WebChallenge message. */
        g_hash_table_insert(table, g_strdup(key), g_strdup(value));
        
        g_strfreev(elements);
    }

    g_strfreev(items);

    return table;
}

/**
 * Immediately send an IM to a user, by their numeric user ID.
 *
 * @param session 
 * @param userid ASCII numeric userid.
 * @param message Text of message to send.
 * @param flags Purple instant message flags.
 *
 * @return 0, since the 'table' parameter is no longer needed.
 *
 */
static int msim_send_im_by_userid(MsimSession *session, const gchar *userid, const gchar *message, PurpleMessageFlags flags)
{
    gchar *msg_string;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(userid != NULL, 0);
    g_return_val_if_fail(msim_is_userid(userid) == TRUE, 0);
    g_return_val_if_fail(message != NULL, 0);

    /* TODO: constants for bm types */
    msg_string = g_strdup_printf("\\bm\\121\\sesskey\\%s\\t\\%s\\cv\\%d\\msg\\%s\\final\\",
            session->sesskey, userid, MSIM_CLIENT_VERSION, message);

    purple_debug_info("msim", "going to write: %s\n", msg_string);

    msim_send(session, msg_string);

    /* TODO: notify Purple that we sent the IM. */

    /* Not needed since sending messages to yourself is allowed by MSIM! */
    /*if (strcmp(from_username, who) == 0)
        serv_got_im(gc, from_username, message, PURPLE_MESSAGE_RECV, time(NULL));
        */

    return 0;
}


/**
 * Callback called when ready to send an IM by userid (the userid has been looked up).
 * Calls msim_send_im_by_userid. 
 *
 * @param session 
 * @param userinfo User info message from server containing a 'body' field
 *                 with a 'UserID' key. This is where the user ID is taken from.
 * @param data A send_im_cb_struct* of information on the IM to send.
 *
 */
static void msim_send_im_by_userid_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
{
    send_im_cb_struct *s;
    gchar *userid;
    GHashTable *body;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

    body = msim_parse_body(g_hash_table_lookup(userinfo, "body"));
    g_assert(body);

    userid = g_hash_table_lookup(body, "UserID");

    s = (send_im_cb_struct*)data;
    msim_send_im_by_userid(session, userid, s->message, s->flags);

    g_hash_table_destroy(body);
    g_hash_table_destroy(userinfo);
    g_free(s->message);
    g_free(s->who);
} 

/**
 * Process a message reply from the server.
 *
 * @param session 
 * @param table Message reply from server.
 *
 * @return 0, since the 'table' field is no longer needed.
 */
static int msim_process_reply(MsimSession *session, GHashTable *table)
{
    gchar *rid_str;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);
    
    rid_str = g_hash_table_lookup(table, "rid");

    if (rid_str)  /* msim_lookup_user sets callback for here */
    {
        MSIM_USER_LOOKUP_CB cb;
        gpointer data;
        guint rid;

        GHashTable *body;
        gchar *username;

        rid = atol(rid_str);

        /* Cache the user info. Currently, the GHashTable of user info in
         * this cache never expires so is never freed. TODO: expire and destroy
         * 
         * Some information never changes (username->userid map), some does.
         * TODO: Cache what doesn't change only
         */
        body = msim_parse_body(g_hash_table_lookup(table, "body"));
        username = g_hash_table_lookup(body, "UserName");
        if (username)
        {
            g_hash_table_insert(session->user_lookup_cache, g_strdup(username), body);
        } else {
            purple_debug_info("msim", 
					"msim_process_reply: not caching <%s>, no UserName\n",
                    g_hash_table_lookup(table, "body"));
        } 

        /* If a callback is registered for this userid lookup, call it. */

        cb = g_hash_table_lookup(session->user_lookup_cb, GUINT_TO_POINTER(rid));
        data = g_hash_table_lookup(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));

        if (cb)
        {
            purple_debug_info("msim", 
					"msim_process_body: calling callback now\n");
            cb(session, table, data);
            g_hash_table_remove(session->user_lookup_cb, GUINT_TO_POINTER(rid));
            g_hash_table_remove(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));

            /* Return 1 to tell caller of msim_process (msim_input_cb) to
             * not destroy 'table'; allow 'cb' to hang on to it and destroy
             * it when it wants. */
            return 1;
        } else {
            purple_debug_info("msim", 
					"msim_process_body: no callback for rid %d\n", rid);
        }
    }
    return 0;
}

/**
 * Handle an error from the server.
 *
 * @param session 
 * @param table The message.
 *
 * @return 0, since 'table' can be freed.
 */
static int msim_error(MsimSession *session, GHashTable *table)
{
    gchar *err, *errmsg, *full_errmsg;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    err = g_hash_table_lookup(table, "err");
    errmsg = g_hash_table_lookup(table, "errmsg");

    full_errmsg = g_strdup_printf("Protocol error, code %s: %s", err, errmsg);

    purple_debug_info("msim", "msim_error: %s\n", full_errmsg);

    /* TODO: check 'fatal' and die if asked to.
     * TODO: do something with the error # (localization of errmsg?)  */
    purple_notify_error(session->account, g_strdup("MySpaceIM Error"), 
            full_errmsg, NULL);

    if (g_hash_table_lookup(table, "fatal"))
    {
        purple_debug_info("msim", "fatal error, destroy session\n");
        purple_connection_error(session->gc, full_errmsg);
        close(session->fd);
        //msim_session_destroy(session);
    }

    return 0;
}

/**
 * Callback to handle incoming messages, after resolving userid.
 *
 * @param session 
 * @param userinfo Message from server on user's info, containing UserName.
 * @param data A gchar* of the incoming instant message's text.
 */
static void msim_incoming_im_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
{
    gchar *msg;
    gchar *username;
    GHashTable *body;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

    body = msim_parse_body(g_hash_table_lookup(userinfo, "body"));
    g_assert(body != NULL);

    username = g_hash_table_lookup(body, "UserName");

    msg = (gchar*)data;
    serv_got_im(session->gc, username, msg, PURPLE_MESSAGE_RECV, time(NULL));

    g_hash_table_destroy(body);
    g_hash_table_destroy(userinfo);
}

/**
 * Handle an incoming message.
 *
 * @param session The session
 * @param table Message from the server, containing 'f' (userid from) and 'msg'.
 *
 * @return 0, since table can be freed.
 */
static int msim_incoming_im(MsimSession *session, GHashTable *table)
{
    gchar *userid;
    gchar *msg;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);


    userid = g_hash_table_lookup(table, "f");
    msg = g_hash_table_lookup(table, "msg");
    
    purple_debug_info("msim", 
			"msim_incoming_im: got msg <%s> from <%s>, resolving username\n",
            msg, userid);

    msim_lookup_user(session, userid, msim_incoming_im_cb, g_strdup(msg));

    return 0;
}

#if 0
/* Not sure about this */
static void msim_status_now(gchar *who, gpointer data)
{
    printf("msim_status_now: %s\n", who);
}
#endif

/** 
 * Callback to update incoming status messages, after looked up username.
 *
 * @param session 
 * @param userinfo Looked up user information from server.
 * @param data gchar* status string.
 *
 */
static void msim_status_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
{
    PurpleBuddyList *blist;
    PurpleBuddy *buddy;
    PurplePresence *presence;
    GHashTable *body;
    //PurpleStatus *status;
    gchar **status_array;
    GList *list;
    gchar *status_text, *status_code;
    gchar *status_str;
    gint i;
    gchar *username;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

    status_str = (gchar*)data;

    body = msim_parse_body(g_hash_table_lookup(userinfo, "body"));
    g_assert(body);

    username = g_hash_table_lookup(body, "UserName");
    /* Note: DisplayName doesn't seem to be resolvable. It could be displayed on
     * the buddy list, if the UserID was stored along with it. */

    if (!username)
    {
        purple_debug_info("msim", "msim_status_cb: no username?!\n");
        return;
    }

    purple_debug_info("msim", 
			"msim_status_cb: updating status for <%s> to <%s>\n", 
			username, status_str);

    /* TODO: generic functions to split into a GList */
    status_array = g_strsplit(status_str, "|", 0);
    for (list = NULL, i = 0;
            status_array[i];
            i++)
    {
        list = g_list_append(list, status_array[i]);
    }

    /* Example fields: |s|0|ss|Offline */
    status_code = g_list_nth_data(list, 2);
    status_text = g_list_nth_data(list, 4);

    blist = purple_get_blist();

    /* Add buddy if not found */
    buddy = purple_find_buddy(session->account, username);
    if (!buddy)
    {
        /* TODO: purple aliases, userids and usernames */
        purple_debug_info("msim", 
				"msim_status: making new buddy for %s\n", username);
        buddy = purple_buddy_new(session->account, username, NULL);

        /* TODO: sometimes (when click on it), buddy list disappears. Fix. */
        purple_blist_add_buddy(buddy, NULL, NULL, NULL);
    } else {
        purple_debug_info("msim", "msim_status: found buddy %s\n", username);
    }

    /* For now, always set status to online. 
     * TODO: make status reflect reality
     * TODO: show headline */
    presence = purple_presence_new_for_buddy(buddy);
    purple_presence_set_status_active(presence, MSIM_STATUS_ONLINE, TRUE);

    g_strfreev(status_array);
    g_list_free(list);
    g_hash_table_destroy(body);
    g_hash_table_destroy(userinfo);
    /* Do not free status_str - it will be freed by g_hash_table_destroy on session->userid_lookup_cb_data */
}

/**
 * Process incoming status messages.
 *
 * @param session
 * @param table Status update message.
 *
 * @return 0, since 'table' can be freed.
 */
static int msim_status(MsimSession *session, GHashTable *table)
{
    gchar *status_str;
    gchar *userid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    status_str = g_hash_table_lookup(table, "msg");
    if (!status_str)
    {
        purple_debug_info("msim", "msim_status: bm=100 but no status msg\n");
        return 0;
    }

    userid = g_hash_table_lookup(table, "f");
    if (!userid)
    {
        purple_debug_info("msim", "msim_status: bm=100 but no f field\n");
        return 0;
    }
   
    /* TODO: if buddies were identified on buddy list by uid, wouldn't have to lookup 
     * before updating the status! Much more efficient. */
    purple_debug_info("msim", 
			"msim_status: got status msg <%s> for <%s>, scheduling lookup\n",
            status_str, userid);
    
    /* Actually update status once obtain username */
    msim_lookup_user(session, userid, msim_status_cb, g_strdup(status_str));

    return 0;
}


/**
 * Process a message. 
 *
 * @param gc Connection.
 * @param table Any message from the server.
 *
 * @return The return value of the function used to process the message, or -1 if
 * called with invalid parameters.
 */
static int msim_process(PurpleConnection *gc, GHashTable *table)
{
    MsimSession *session;

    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(table != NULL, -1);

    session = (MsimSession*)gc->proto_data;

    printf("-------- message -------------\n");
    g_hash_table_foreach(table, print_hash_item, NULL);
    printf("------------------------------\n");

    if (g_hash_table_lookup(table, "nc"))
    {
        return msim_login_challenge(session, table);
    } else if (g_hash_table_lookup(table, "sesskey")) {
        printf("SESSKEY=<%s>\n", (gchar*)g_hash_table_lookup(table, "sesskey"));

        purple_connection_update_progress(gc, "Connected", 3, 4);

        session->sesskey = g_strdup(g_hash_table_lookup(table, "sesskey"));

        /* Comes with: proof,profileid,userid,uniquenick -- all same values
         * (at least for me). */
        session->userid = g_strdup(g_hash_table_lookup(table, "userid"));

        purple_connection_set_state(gc, PURPLE_CONNECTED);

        return 0;
    } else if (g_hash_table_lookup(table, "bm"))  {
        guint bm;
       
        bm = atoi(g_hash_table_lookup(table, "bm"));
        switch (bm)
        {
            case MSIM_BM_STATUS:
                return msim_status(session, table);
            case MSIM_BM_INSTANT:
                return msim_incoming_im(session, table);
            default:
                /* Not really an IM, but show it for informational 
                 * purposes during development. */
                return msim_incoming_im(session, table);
        }

        if (bm == MSIM_BM_STATUS)
        {
            return msim_status(session, table);
        } else { /* else if strcmp(bm, "1") == 0)  */
            return msim_incoming_im(session, table);
        }
    } else if (g_hash_table_lookup(table, "rid")) {
        return msim_process_reply(session, table);
    } else if (g_hash_table_lookup(table, "error")) {
        return msim_error(session, table);
    } else if (g_hash_table_lookup(table, "ka")) {
        purple_debug_info("msim", "msim_process: got keep alive\n");
        return 0;
    } else {
        printf("<<unhandled>>\n");
        return 0;
    }
}

/**
 * Callback when input available.
 *
 * @param gc_uncasted A PurpleConnection pointer.
 * @param source File descriptor.
 * @param cond PURPLE_INPUT_READ
 *
 * Reads the input, and dispatches calls msim_process to handle it.
 */
static void msim_input_cb(gpointer gc_uncasted, gint source, PurpleInputCondition cond)
{
    PurpleConnection *gc;
    PurpleAccount *account;
    MsimSession *session;
    gchar *end;
    int n;

    g_return_if_fail(gc_uncasted != NULL);
    g_return_if_fail(source >= 0);  /* Note: 0 is a valid fd */

    gc = (PurpleConnection*)(gc_uncasted);
    account = purple_connection_get_account(gc);
    session = gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    
    g_assert(cond == PURPLE_INPUT_READ);

    /* Only can handle so much data at once... 
     * If this happens, try recompiling with a higher MSIM_READ_BUF_SIZE.
     * Should be large enough to hold the largest protocol message.
     */
    if (session->rxoff == MSIM_READ_BUF_SIZE)
    {
        purple_debug_error("msim", "msim_input_cb: %d-byte read buffer full!\n",
                MSIM_READ_BUF_SIZE);
        purple_connection_error(gc, "Read buffer full");
        /* TODO: fix 100% CPU after closing */
        close(source);
        return;
    }

    purple_debug_info("msim", "buffer at %d (max %d), reading up to %d\n",
            session->rxoff, MSIM_READ_BUF_SIZE, 
			MSIM_READ_BUF_SIZE - session->rxoff);

    /* Read into buffer. On Win32, need recv() not read(). session->fd also holds
     * the file descriptor, but it sometimes differs from the 'source' parameter.
     */
    n = recv(session->fd, session->rxbuf + session->rxoff, MSIM_READ_BUF_SIZE - session->rxoff, 0);

    if (n < 0 && errno == EAGAIN)
    {
        return;
    }
    else if (n < 0)
    {
        purple_connection_error(gc, "Read error");
        purple_debug_error("msim", "msim_input_cb: read error, ret=%d, "
			"error=%s, source=%d, fd=%d (%X))\n", 
			n, strerror(errno), source, session->fd, session->fd);
        close(source);
        return;
    } 
    else if (n == 0)
    {
        purple_debug_info("msim", "msim_input_cb: server disconnected\n");
        purple_connection_error(gc, "Server has disconnected");
        return;
    }

    /* Null terminate */
    session->rxbuf[session->rxoff + n] = 0;

    /* Check for embedded NULs. I don't handle them, and they shouldn't occur. */
    if (strlen(session->rxbuf + session->rxoff) != n)
    {
        /* Occurs after login, but it is not a null byte. */
        purple_debug_info("msim", "msim_input_cb: strlen=%d, but read %d bytes"
                "--null byte encountered?\n", 
				strlen(session->rxbuf + session->rxoff), n);
        //purple_connection_error(gc, "Invalid message - null byte on input");
        return;
    }

    session->rxoff += n;
    purple_debug_info("msim", "msim_input_cb: read=%d\n", n);

    //printf("buf=<%s>\n", session->rxbuf);

    /* Look for \\final\\ end markers. If found, process message. */
    while((end = strstr(session->rxbuf, MSIM_FINAL_STRING)))
    {
        GHashTable *table;

        //printf("in loop: buf=<%s>\n", session->rxbuf);
        *end = 0;
        table = msim_parse(g_strdup(session->rxbuf));
        if (!table)
        {
            purple_debug_info("msim", "msim_input_cb: couldn't parse <%s>\n", 
					session->rxbuf);
            purple_connection_error(gc, "Unparseable message");
        }
        else
        {
            /* Process message. Returns 0 to free */
            if (msim_process(gc, table) == 0)
                g_hash_table_destroy(table);
        }

        /* Move remaining part of buffer to beginning. */
        session->rxoff -= strlen(session->rxbuf) + strlen(MSIM_FINAL_STRING);
        memmove(session->rxbuf, end + strlen(MSIM_FINAL_STRING), 
                MSIM_READ_BUF_SIZE - (end + strlen(MSIM_FINAL_STRING) - session->rxbuf));

        /* Clear end of buffer */
        //memset(end, 0, MSIM_READ_BUF_SIZE - (end - session->rxbuf));
    }
}

/**
 * Callback when connected. Sets up input handlers.
 *
 * @param data A PurpleConnection pointer.
 * @param source File descriptor.
 * @param error_message
 */
static void msim_connect_cb(gpointer data, gint source, const gchar *error_message)
{
    PurpleConnection *gc;
    MsimSession *session;

    g_return_if_fail(data != NULL);

    gc = (PurpleConnection*)data;
    session = gc->proto_data;

    if (source < 0)
    {
        purple_connection_error(gc, "Couldn't connect to host");
        purple_connection_error(gc, g_strdup_printf("Couldn't connect to host: %s (%d)", 
                    error_message, source));
        return;
    }

    session->fd = source; 

    gc->inpa = purple_input_add(source, PURPLE_INPUT_READ, msim_input_cb, gc);
}

/* Session methods */

/**
 * Create a new MSIM session.
 *
 * @param acct The account to create the session from.
 *
 * @return Pointer to a new session. Free with msim_session_destroy.
 */
static MsimSession *msim_session_new(PurpleAccount *acct)
{
    MsimSession *session;

    g_return_val_if_fail(acct != NULL, NULL);

    session = g_new0(MsimSession, 1);

    session->magic = MSIM_SESSION_STRUCT_MAGIC;
    session->account = acct;
    session->gc = purple_account_get_connection(acct);
    session->fd = -1;
    session->user_lookup_cb = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);  /* do NOT free function pointers! */
    session->user_lookup_cb_data = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    session->user_lookup_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);
    session->rxoff = 0;
    session->rxbuf = g_new0(gchar, MSIM_READ_BUF_SIZE);
    
    return session;
}

/**
 * Free a session.
 *
 * @param session The session to destroy.
 */
static void msim_session_destroy(MsimSession *session)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));

    session->magic = -1;

    g_free(session->rxbuf);
    g_free(session->userid);
    g_free(session->sesskey);

    g_free(session);
}

/** 
 * Start logging in to the MSIM servers.
 * 
 * @param acct Account information to use to login.
 */
static void msim_login(PurpleAccount *acct)
{
    PurpleConnection *gc;
    const char *host;
    int port;

    g_return_if_fail(acct != NULL);

    purple_debug_info("myspace", "logging in %s\n", acct->username);

    gc = purple_account_get_connection(acct);
    gc->proto_data = msim_session_new(acct);

    /* 1. connect to server */
    purple_connection_update_progress(gc, "Connecting",
                                  0,   /* which connection step this is */
                                  4);  /* total number of steps */

    /* TODO: GUI option to be user-modifiable. */
    host = purple_account_get_string(acct, "server", MSIM_SERVER);
    port = purple_account_get_int(acct, "port", MSIM_PORT);
    /* TODO: connect */
    /* From purple.sf.net/api:
     * """Note that this function name can be misleading--although it is called 
     * "proxy connect," it is used for establishing any outgoing TCP connection, 
     * whether through a proxy or not.""" */

    /* Calls msim_connect_cb when connected. */
    if (purple_proxy_connect(gc, acct, host, port, msim_connect_cb, gc) == NULL)
    {
        /* TODO: try other ports if in auto mode, then save
         * working port and try that first next time. */
        purple_connection_error(gc, "Couldn't create socket");
        return;
    }

}                 


/** 
 * Close the connection.
 * 
 * @param gc The connection.
 */
static void msim_close(PurpleConnection *gc)
{
    g_return_if_fail(gc != NULL);
    
    msim_session_destroy(gc->proto_data);
}

/**
 * Return the icon name for a buddy and account.
 *
 * @param acct The account to find the icon for.
 * @param buddy The buddy to find the icon for, or NULL for the accoun icon.
 *
 * @return The base icon name string.
 */
static const gchar *msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
    /* TODO: use a MySpace icon. hbons submitted one to 
     * http://developer.pidgin.im/wiki/MySpaceIM  - tried placing in
     * C:\cygwin\home\Jeff\purple-2.0.0beta6\gtk\pixmaps\status\default 
     * and returning "myspace" but icon shows up blank.
     */
    if (acct == NULL)
    {
        purple_debug_info("msim", "msim_list_icon: acct == NULL!\n");
        //exit(-2);
    }
      return "meanwhile";
}

/**
 * Check if a string is a userid (all numeric).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is userid, FALSE if not.
 */
static inline gboolean msim_is_userid(const gchar *user)
{
    g_return_val_if_fail(user != NULL, FALSE);

    return strspn(user, "0123456789") == strlen(user);
}

/**
 * Check if a string is an email address (contains an @).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is an email, FALSE if not.
 *
 * This function is not intended to be used as a generic
 * means of validating email addresses, but to distinguish
 * between a user represented by an email address from
 * other forms of identification.
 */ 
static inline gboolean msim_is_email(const gchar *user)
{
    g_return_val_if_fail(user != NULL, FALSE);

    return strchr(user, '@') != NULL;
}


/**
 * Asynchronously lookup user information, calling callback when receive result.
 *
 * @param session
 * @param user The user id, email address, or username.
 * @param cb Callback, called with user information when available.
 * @param data An arbitray data pointer passed to the callback.
 */
static void msim_lookup_user(MsimSession *session, const gchar *user, MSIM_USER_LOOKUP_CB cb, gpointer data)
{
    gchar *field_name;
    gchar *msg_string;
    guint rid, cmd, dsn, lid;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(user != NULL);
    g_return_if_fail(cb != NULL);

    purple_debug_info("msim", "msim_lookup_userid", 
			"asynchronously looking up <%s>\n", user);

    /* TODO: check if this user's info was cached and fresh; if so return immediately */
#if 0
    /* If already know userid, then call callback immediately */
    cached_userid = g_hash_table_lookup(session->userid_cache, who);
    if (cached_userid && !by_userid)
    {
        cb(cached_userid, NULL, NULL, data);
        return;
    }
#endif

    rid = rand(); //om();

    /* Setup callback. Response will be associated with request using 'rid'. */
    g_hash_table_insert(session->user_lookup_cb, GUINT_TO_POINTER(rid), cb);
    g_hash_table_insert(session->user_lookup_cb_data, GUINT_TO_POINTER(rid), data);

    /* Send request */

    cmd = 1;

    if (msim_is_userid(user))
    {
        /* TODO: document cmd,dsn,lid */
        field_name = "UserID";
        dsn = 4;
        lid = 3;
    } else if (msim_is_email(user)) {
        field_name = "Email";
        dsn = 5;
        lid = 7;
    } else {
        field_name = "UserName";
        dsn = 5;
        lid = 7;
    }

    msg_string = g_strdup_printf("\\persist\\1\\sesskey\\%s\\cmd\\1\\dsn\\%d\\uid\\%s\\lid\\%d\\rid\\%d\\body\\%s=%s\\final\\",
            session->sesskey, dsn, session->userid, lid, rid, field_name, user);

    msim_send(session, msg_string);
} 

/**
 * Schedule an IM to be sent once the user ID is looked up. 
 *
 * @param gc Connection.
 * @param who A user id, email, or username to send the message to.
 * @param message Instant message text to send.
 * @param flags Flags.
 *
 * @return 1 in all cases, even if the message delivery is destined to fail.
 *
 * Allows sending to a user by username, email address, or userid. If
 * a username or email address is given, the userid must be looked up.
 * This function does that by calling msim_lookup_user(), setting up
 * a msim_send_im_by_userid_cb() callback function called when the userid
 * response is received from the server. 
 *
 * The callback function calls msim_send_im_by_userid() to send the actual
 * instant message. If a userid is specified directly, this function is called
 * immediately here.
 */
static int msim_send_im(PurpleConnection *gc, const char *who,
                            const char *message, PurpleMessageFlags flags)
{
    MsimSession *session;
    const char *from_username = gc->account->username;
    send_im_cb_struct *cbinfo;

    g_return_val_if_fail(gc != NULL, 0);
    g_return_val_if_fail(who != NULL, 0);
    g_return_val_if_fail(message != NULL, 0);

    purple_debug_info("msim", "sending message from %s to %s: %s\n",
                  from_username, who, message);

    session = gc->proto_data;

    /* If numeric ID, can send message immediately without userid lookup */
    if (msim_is_userid(who))
    {
        purple_debug_info("msim", 
				"msim_send_im: numeric 'who' detected, sending asap\n");
        msim_send_im_by_userid(session, who, message, flags);
        return 1;
    }

    /* Otherwise, add callback to IM when userid of destination is available */

    /* Setup a callback for when the userid is available */
    cbinfo = g_new0(send_im_cb_struct, 1);
    cbinfo->who = g_strdup(who);
    cbinfo->message = g_strdup(message);
    cbinfo->flags = flags;

    /* Send the request to lookup the userid */
    msim_lookup_user(session, who, msim_send_im_by_userid_cb, cbinfo); 

    /* msim_send_im_by_userid_cb will now be called once userid is looked up */

    /* Return 1 to have Purple show this IM as being sent, 0 to not. I always
     * return 1 even if the message could not be sent, since I don't know if
     * it has failed yet--because the IM is only sent after the userid is
     * retrieved from the server (which happens after this function returns).
     *
     * TODO: In MySpace, you login with your email address, but don't talk to other
     * users using their email address. So there is currently an asymmetry in the 
     * IM windows when using this plugin:
     *
     * you@example.com: hello
     * some_other_user: what's going on?
     * you@example.com: just coding a prpl
     *
     * TODO: Make the sent IM's appear as from the user's username, instead of
     * their email address. Purple uses the login (in MSIM, the email)--change this.
     */
    return 1;
}

/**
 * Obtain the status text for a buddy.
 *
 * @param buddy The buddy to obtain status text for.
 *
 * @return Status text.
 *
 * Currently returns the display name. 
 */
static char *msim_status_text(PurpleBuddy *buddy)
{
    MsimSession *session;
    GHashTable *userinfo;
    gchar *display_name;

    g_return_val_if_fail(buddy != NULL, NULL);

    session = (MsimSession*)buddy->account->gc->proto_data;
    g_assert(MSIM_SESSION_VALID(session));
    g_assert(session->user_lookup_cache != NULL);

    userinfo = g_hash_table_lookup(session->user_lookup_cache, buddy->name);
    if (!userinfo)
    {
        return g_strdup("");
    }

    display_name = g_hash_table_lookup(userinfo, "DisplayName");
    g_assert(display_name != NULL);

    return g_strdup(display_name);
}

/**
 * Obtain the tooltip text for a buddy.
 *
 * @param buddy Buddy to obtain tooltip text on.
 * @param user_info Variable modified to have the tooltip text.
 * @param full TRUE if should obtain full tooltip text.
 *
 */
static void msim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
    g_return_if_fail(buddy != NULL);
    g_return_if_fail(user_info != NULL);

    if (PURPLE_BUDDY_IS_ONLINE(buddy))
    {
        MsimSession *session;
        GHashTable *userinfo;

        session = (MsimSession*)buddy->account->gc->proto_data;

        g_assert(MSIM_SESSION_VALID(session));
        g_assert(session->user_lookup_cache);

        userinfo = g_hash_table_lookup(session->user_lookup_cache, buddy->name);

        g_assert(userinfo != NULL);

        // TODO: if (full), do something different
        purple_notify_user_info_add_pair(user_info, "User ID", g_hash_table_lookup(userinfo, "UserID"));
        purple_notify_user_info_add_pair(user_info, "Display Name", g_hash_table_lookup(userinfo, "DisplayName"));
        purple_notify_user_info_add_pair(user_info, "User Name", g_hash_table_lookup(userinfo, "UserName"));
        purple_notify_user_info_add_pair(user_info, "Total Friends", g_hash_table_lookup(userinfo, "TotalFriends"));
        purple_notify_user_info_add_pair(user_info, "Song", 
                g_strdup_printf("%s - %s",
                    (gchar*)g_hash_table_lookup(userinfo, "BandName"),
                    (gchar*)g_hash_table_lookup(userinfo, "SongName")));
    }
}

/** Callbacks called by Purple, to access this plugin. */
static PurplePluginProtocolInfo prpl_info =
{
    OPT_PROTO_MAIL_CHECK,/* options - TODO: myspace will notify of mail */
    NULL,              /* user_splits */
    NULL,              /* protocol_options */
    NO_BUDDY_ICONS,    /* icon_spec - TODO: eventually should add this */
    msim_list_icon,    /* list_icon */
    NULL,              /* list_emblems */
    msim_status_text,  /* status_text */
    msim_tooltip_text, /* tooltip_text */
    msim_status_types, /* status_types */
    NULL,              /* blist_node_menu */
    NULL,              /* chat_info */
    NULL,              /* chat_info_defaults */
    msim_login,        /* login */
    msim_close,        /* close */
    msim_send_im,      /* send_im */
    NULL,              /* set_info */
    NULL,              /* send_typing */
    NULL,              /* get_info */
    NULL,              /* set_away */
    NULL,              /* set_idle */
    NULL,              /* change_passwd */
    NULL,              /* add_buddy */
    NULL,              /* add_buddies */
    NULL,              /* remove_buddy */
    NULL,              /* remove_buddies */
    NULL,              /* add_permit */
    NULL,              /* add_deny */
    NULL,              /* rem_permit */
    NULL,              /* rem_deny */
    NULL,              /* set_permit_deny */
    NULL,              /* join_chat */
    NULL,              /* reject chat invite */
    NULL,              /* get_chat_name */
    NULL,              /* chat_invite */
    NULL,              /* chat_leave */
    NULL,              /* chat_whisper */
    NULL,              /* chat_send */
    NULL,              /* keepalive */
    NULL,              /* register_user */
    NULL,              /* get_cb_info */
    NULL,              /* get_cb_away */
    NULL,              /* alias_buddy */
    NULL,              /* group_buddy */
    NULL,              /* rename_group */
    NULL,              /* buddy_free */
    NULL,              /* convo_closed */
    NULL,              /* normalize */
    NULL,              /* set_buddy_icon */
    NULL,              /* remove_group */
    NULL,              /* get_cb_real_name */
    NULL,              /* set_chat_topic */
    NULL,              /* find_blist_chat */
    NULL,              /* roomlist_get_list */
    NULL,              /* roomlist_cancel */
    NULL,              /* roomlist_expand_category */
    NULL,              /* can_receive_file */
    NULL,              /* send_file */
    NULL,              /* new_xfer */
    NULL,              /* offline_message */
    NULL,              /* whiteboard_prpl_ops */
    NULL,              /* send_raw */
    NULL               /* roomlist_room_serialize */
};



/** Based on MSN's plugin info comments. */
static PurplePluginInfo info =
{
    PURPLE_PLUGIN_MAGIC,                                
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
    NULL,                                             /**< ui_requirement */
    0,                                                /**< flags          */
    NULL,                                             /**< dependencies   */
    PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

    "prpl-myspace",                                   /**< id             */
    "MySpaceIM",                                      /**< name           */
    "0.4",                                            /**< version        */
                                                      /**  summary        */
    "MySpaceIM Protocol Plugin",
                                                      /**  description    */
    "MySpaceIM Protocol Plugin",
    "Jeff Connelly <myspaceim@xyzzy.cjb.net>",        /**< author         */
    "http://developer.pidgin.im/wiki/MySpaceIM/",     /**< homepage       */

    NULL,                                             /**< load           */
    NULL,                                             /**< unload         */
    NULL,                                             /**< destroy        */
    NULL,                                             /**< ui_info        */
    &prpl_info,                                       /**< extra_info     */
    NULL,                                             /**< prefs_info     */

    /* msim_actions */
    NULL
};


PURPLE_INIT_PLUGIN(myspace, init_plugin, info);
