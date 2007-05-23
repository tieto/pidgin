/* MySpaceIM Protocol Plugin
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@homing.pidgin.im>
 *
 * Based on Purple's "C Plugin HOWTO" hello world example.
 *
 * Code also drawn from mockprpl:
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

#include "myspace.h"
#include "message.h"

/** 
 * Load the plugin.
 */
gboolean msim_load(PurplePlugin *plugin)
{
#ifdef MSIM_USE_PURPLE_RC4
	/* If compiled to use RC4 from libpurple, check if it is really there. */
	if (!purple_ciphers_find_cipher("rc4"))
	{
		purple_debug_error("msim", "compiled with MSIM_USE_PURPLE_RC4 but rc4 not in libpurple - not loading MySpaceIM plugin!\n");
		purple_notify_error(plugin, _("Missing Cipher"), 
				_("The RC4 cipher could not be found"),
				_("Recompile without MSIM_USE_PURPLE_RC4, or upgrade "
					"to a libpurple with RC4 support (>= 2.0.1). MySpaceIM "
					"plugin will not be loaded."));
		return FALSE;
	}
#endif
	return TRUE;
}

/**
 * Get possible user status types. Based on mockprpl.
 *
 * @return GList of status types.
 */
GList *msim_status_types(PurpleAccount *acct)
{
    GList *types;
    PurpleStatusType *type;

    purple_debug_info("myspace", "returning status types for %s: %s, %s, %s\n",
                  acct->username,
                  MSIM_STATUS_ONLINE, MSIM_STATUS_AWAY, MSIM_STATUS_OFFLINE, MSIM_STATUS_INVISIBLE);


    types = NULL;

	/* TODO: Clean up - I don't like all this repetition */
    type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, MSIM_STATUS_ONLINE,
                              MSIM_STATUS_ONLINE, TRUE);
    purple_status_type_add_attr(type, "message", _("Online"),
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_AWAY, MSIM_STATUS_AWAY,
                              MSIM_STATUS_AWAY, TRUE);
    purple_status_type_add_attr(type, "message", _("Away"),
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_OFFLINE, MSIM_STATUS_OFFLINE,
                              MSIM_STATUS_OFFLINE, TRUE);
    purple_status_type_add_attr(type, "message", _("Offline"),
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    type = purple_status_type_new(PURPLE_STATUS_INVISIBLE, MSIM_STATUS_INVISIBLE,
                              MSIM_STATUS_INVISIBLE, TRUE);
    purple_status_type_add_attr(type, "message", _("Invisible"),
                            purple_value_new(PURPLE_TYPE_STRING));
    types = g_list_append(types, type);

    return types;
}

/**
 * Return the icon name for a buddy and account.
 *
 * @param acct The account to find the icon for, or NULL for protocol icon.
 * @param buddy The buddy to find the icon for, or NULL for the account icon.
 *
 * @return The base icon name string.
 */
const gchar *msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
    /* Use a MySpace icon submitted by hbons submitted one at
     * http://developer.pidgin.im/wiki/MySpaceIM. */
    return "myspace";
}

/**
 * Unescape a protocol message.
 *
 * @return The unescaped message. Caller must g_free().
 */
gchar *msim_unescape(const gchar *msg)
{
	/* TODO: make more elegant, refactor with msim_escape */
	gchar *tmp, *ret;
	
	tmp = str_replace(msg, "/1", "/");
	ret = str_replace(tmp, "/2", "\\");
	g_free(tmp);
	return ret;
}

/**
 * Escape a protocol message.
 *
 * @return The escaped message. Caller must g_free().
 */
gchar *msim_escape(const gchar *msg)
{
	/* TODO: make more elegant, refactor with msim_unescape */
	gchar *tmp, *ret;
	
	tmp = str_replace(msg, "/", "/1");
	ret = str_replace(tmp, "\\", "/2");
	g_free(tmp);

	return ret;
}

/**
 * Replace 'old' with 'new' in 'str'.
 *
 * @param str The original string.
 * @param old The substring of 'str' to replace.
 * @param new The replacement for 'old' within 'str'.
 *
 * @return A _new_ string, based on 'str', with 'old' replaced
 * 		by 'new'. Must be g_free()'d by caller.
 *
 * This string replace method is based on
 * http://mail.gnome.org/archives/gtk-app-devel-list/2000-July/msg00201.html
 *
 */
gchar *str_replace(const gchar* str, const gchar *old, const gchar *new)
{
	char **items;
	char *ret;

	items = g_strsplit(str, old, -1);
	ret = g_strjoinv(new, items);
	g_free(items);
	return ret;
}



/** 
 * Parse a MySpaceIM protocol message into a hash table. 
 *
 * @param msg The message string to parse, will be g_free()'d.
 *
 * @return Hash table of message. Caller should destroy with
 *              g_hash_table_destroy() when done.
 */
GHashTable* msim_parse(gchar* msg)
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
#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "tok=<%s>, i%2=%d\n", token, i % 2);
#endif
        if (i % 2)
        {
			/* Odd-numbered ordinal is a value */
		
			/* Note: returns a new string */	
            value = msim_unescape(token);

            /* Check if key already exists */
            if (g_hash_table_lookup(table, key) == NULL)
            {
#ifdef MSIM_DEBUG_PARSE
                purple_debug_info("msim", "insert: |%s|=|%s|\n", key, value);
#endif
				/* Insert - strdup 'key' because it will be g_strfreev'd (as 'tokens'),
				 * but do not strdup 'value' because it was newly allocated by
				 * msim_unescape(). 
				 */
                g_hash_table_insert(table, g_strdup(key), value);
            } else {
                /* TODO: Some dictionaries have multiple values for the same
                 * key. Should append to a GList to handle this case. */
                purple_debug_info("msim", "msim_parse: key %s already exists, "
                "not overwriting or replacing; ignoring new value %s\n", key,
                value);
            }
        } else {
			/* Even numbered index is a key name */
            key = token;
        }
    }
    g_strfreev(tokens);

    /* Can free now since all data was copied to hash key/values */
    g_free(msg);

    return table;
}

/**
 * Parse a \x1c-separated "dictionary" of key=value pairs into a hash table.
 *
 * @param body_str The text of the dictionary to parse. Often the
 *                  value for the 'body' field.
 *
 * @return Hash table of the keys and values. Must g_hash_table_destroy() when done.
 */
GHashTable *msim_parse_body(const gchar *body_str)
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

#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "-- %s: %s\n", key, value);
#endif

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



#ifdef MSIM_DEBUG_MSG
void print_hash_item(gpointer key, gpointer value, gpointer user_data)
{
    purple_debug_info("msim", "%s=%s\n", (char*)key, (char*)value);
}
#endif

/** 
 * Send raw data to the server.
 *
 * @param session 
 * @param msg The raw data to send.
 *
 * @return TRUE if succeeded, FALSE if not.
 *
 */
gboolean msim_send_raw(MsimSession *session, const gchar *msg)
{
	int total_bytes_sent, total_bytes;
    
	purple_debug_info("msim", "msim_send_raw: writing <%s>\n", msg);

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);


	/* Loop until all data is sent, or a failure occurs. */
	total_bytes_sent = 0;
	total_bytes = strlen(msg);
	do
	{
		int bytes_sent;

		bytes_sent = send(session->fd, msg + total_bytes_sent, 
			total_bytes - total_bytes_sent, 0);

		if (bytes_sent < 0)
		{
			purple_debug_info("msim", "msim_send_raw(%s): send() failed: %s\n",
					msg, g_strerror(errno));
			return FALSE;
		}
		total_bytes_sent += bytes_sent;

	} while(total_bytes_sent < total_bytes);
	return TRUE;
}

/** 
 * Start logging in to the MSIM servers.
 * 
 * @param acct Account information to use to login.
 */
void msim_login(PurpleAccount *acct)
{
    PurpleConnection *gc;
    const char *host;
    int port;

    g_return_if_fail(acct != NULL);

    purple_debug_info("myspace", "logging in %s\n", acct->username);

    gc = purple_account_get_connection(acct);
    gc->proto_data = msim_session_new(acct);

    /* 1. connect to server */
    purple_connection_update_progress(gc, _("Connecting"),
                                  0,   /* which connection step this is */
                                  4);  /* total number of steps */

    host = purple_account_get_string(acct, "server", MSIM_SERVER);
    port = purple_account_get_int(acct, "port", MSIM_PORT);

    /* From purple.sf.net/api:
     * """Note that this function name can be misleading--although it is called 
     * "proxy connect," it is used for establishing any outgoing TCP connection, 
     * whether through a proxy or not.""" */

    /* Calls msim_connect_cb when connected. */
    if (purple_proxy_connect(gc, acct, host, port, msim_connect_cb, gc) == NULL)
    {
        /* TODO: try other ports if in auto mode, then save
         * working port and try that first next time. */
        purple_connection_error(gc, _("Couldn't create socket"));
        return;
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
int msim_login_challenge(MsimSession *session, GHashTable *table) 
{
    PurpleAccount *account;
    gchar *nc_str;
    guchar *nc;
    gchar *response;
    gsize nc_len;
	guint response_len;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    nc_str = g_hash_table_lookup(table, "nc");

    account = session->account;
    //assert(account);

    purple_connection_update_progress(session->gc, _("Reading challenge"), 1, 4);

    purple_debug_info("msim", "nc=<%s>\n", nc_str);

    nc = (guchar*)purple_base64_decode(nc_str, &nc_len);
    purple_debug_info("msim", "base64 decoded to %d bytes\n", nc_len);
    if (nc_len != 0x40)
    {
        purple_debug_info("msim", "bad nc length: %x != 0x40\n", nc_len);
        purple_connection_error(session->gc, _("Unexpected challenge length from server"));
        return 0;
    }

    purple_connection_update_progress(session->gc, _("Logging in"), 2, 4);

    response = msim_compute_login_response(nc, account->username, account->password, &response_len);

    g_free(nc);

	msim_send(session, 
			"login2", MSIM_TYPE_INTEGER, MSIM_AUTH_ALGORITHM,
			"username", MSIM_TYPE_STRING, g_strdup(account->username),
			/* GString and gchar * response will be freed in msim_msg_free() in msim_send(). */
			"response", MSIM_TYPE_BINARY, g_string_new_len(response, response_len),
			"clientver", MSIM_TYPE_INTEGER, MSIM_CLIENT_VERSION,
			"reconn", MSIM_TYPE_INTEGER, 0,
			"status", MSIM_TYPE_INTEGER, 100,
			"id", MSIM_TYPE_INTEGER, 1,
			NULL);

    return 0;
}

#ifndef MSIM_USE_PURPLE_RC4
/* No RC4 in this version of libpurple, so bring our own. */

/* 
   Unix SMB/CIFS implementation.

   a partial implementation of RC4 designed for use in the 
   SMB authentication protocol

   Copyright (C) Andrew Tridgell 1998

   $Id: crypt-rc4.c 12116 2004-09-27 23:29:22Z guy $
   
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

   
   Modified by Jeff Connelly for MySpaceIM Gaim plugin.
*/

#include <glib.h>
#include <string.h>

/* Perform RC4 on a block of data using specified key.  "data" is a pointer
   to the block to be processed.  Output is written to same memory as input,
   so caller may need to make a copy before calling this function, since
   the input will be overwritten.  
   
   Taken from Samba source code.  Modified to allow us to maintain state
   between calls to crypt_rc4.
*/

void crypt_rc4_init(rc4_state_struct *rc4_state, 
		    const unsigned char *key, int key_len)
{
  int ind;
  unsigned char j = 0;
  unsigned char *s_box;

  memset(rc4_state, 0, sizeof(rc4_state_struct));
  s_box = rc4_state->s_box;
  
  for (ind = 0; ind < 256; ind++)
  {
    s_box[ind] = (unsigned char)ind;
  }

  for( ind = 0; ind < 256; ind++)
  {
     unsigned char tc;

     j += (s_box[ind] + key[ind%key_len]);

     tc = s_box[ind];
     s_box[ind] = s_box[j];
     s_box[j] = tc;
  }

}

void crypt_rc4(rc4_state_struct *rc4_state, unsigned char *data, int data_len)
{
  unsigned char *s_box;
  unsigned char index_i;
  unsigned char index_j;
  int ind;

  /* retrieve current state from the state struct (so we can resume where
     we left off) */
  index_i = rc4_state->index_i;
  index_j = rc4_state->index_j;
  s_box = rc4_state->s_box;

  for( ind = 0; ind < data_len; ind++)
  {
    unsigned char tc;
    unsigned char t;

    index_i++;
    index_j += s_box[index_i];

    tc = s_box[index_i];
    s_box[index_i] = s_box[index_j];
    s_box[index_j] = tc;

    t = s_box[index_i] + s_box[index_j];
    data[ind] = data[ind] ^ s_box[t];
  }

  /* Store the updated state */
  rc4_state->index_i = index_i;
  rc4_state->index_j = index_j;
}

#endif /* !MSIM_USE_PURPLE_RC4 */


/**
 * Compute the base64'd login challenge response based on username, password, nonce, and IPs.
 *
 * @param nonce The base64 encoded nonce ('nc') field from the server.
 * @param email User's email address (used as login name).
 * @param password User's cleartext password.
 * @param response_len Will be written with response length.
 *
 * @return Binary login challenge response, ready to send to the server. Must be g_free()'d
 *         when finished.
 */
gchar* msim_compute_login_response(guchar nonce[2*NONCE_SIZE], 
		gchar* email, gchar* password, guint* response_len)
{
    PurpleCipherContext *key_context;
    PurpleCipher *sha1;
#ifdef MSIM_USE_PURPLE_RC4
	PurpleCipherContext *rc4;
#else
	rc4_state_struct rc4;
#endif

    guchar hash_pw[HASH_SIZE];
    guchar key[HASH_SIZE];
    gchar* password_utf16le;
    guchar* data;
	guchar* data_out;
	size_t data_len, data_out_len;
	gsize conv_bytes_read, conv_bytes_written;
	GError* conv_error;
#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
	int i;
#endif

    //memset(nonce, 0, NONCE_SIZE);
    //memset(nonce + NONCE_SIZE, 1, NONCE_SIZE);

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

    /* Compute password hash */ 
    purple_cipher_digest_region("sha1", (guchar*)password_utf16le, 
			conv_bytes_written, sizeof(hash_pw), hash_pw, NULL);
	g_free(password_utf16le);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "pwhash = ");
    for (i = 0; i < sizeof(hash_pw); i++)
        purple_debug_info("msim", "%.2x ", hash_pw[i]);
    purple_debug_info("msim", "\n");
#endif

    /* key = sha1(sha1(pw) + nonce2) */
    sha1 = purple_ciphers_find_cipher("sha1");
    key_context = purple_cipher_context_new(sha1, NULL);
    purple_cipher_context_append(key_context, hash_pw, HASH_SIZE);
    purple_cipher_context_append(key_context, nonce + NONCE_SIZE, NONCE_SIZE);
    purple_cipher_context_digest(key_context, sizeof(key), key, NULL);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "key = ");
    for (i = 0; i < sizeof(key); i++)
    {
        purple_debug_info("msim", "%.2x ", key[i]);
    }
    purple_debug_info("msim", "\n");
#endif

#ifdef MSIM_USE_PURPLE_RC4
	rc4 = purple_cipher_context_new_by_name("rc4", NULL);

    /* Note: 'key' variable is 0x14 bytes (from SHA-1 hash), 
     * but only first 0x10 used for the RC4 key. */
	purple_cipher_context_set_option(rc4, "key_len", (gpointer)0x10);
	purple_cipher_context_set_key(rc4, key);
#endif

    /* TODO: obtain IPs of network interfaces. This is not immediately
     * important because you can still connect and perform basic
     * functions of the protocol. There is also a high chance that the addreses
     * are RFC1918 private, so the servers couldn't do anything with them
     * anyways except make note of that fact. Probably important for any
     * kind of direct connection, or file transfer functionality.
     */
    /* rc4 encrypt:
     * nonce1+email+IP list */
    data_len = NONCE_SIZE + strlen(email) 
		/* TODO: change to length of IP list */
		+ 25;
    data = g_new0(guchar, data_len);
    memcpy(data, nonce, NONCE_SIZE);
    memcpy(data + NONCE_SIZE, email, strlen(email));
    memcpy(data + NONCE_SIZE + strlen(email),
            /* TODO: IP addresses of network interfaces */
            "\x00\x00\x00\x00\x05\x7f\x00\x00\x01\x00\x00\x00\x00\x0a\x00\x00\x40\xc0\xa8\x58\x01\xc0\xa8\x3c\x01", 25);

#ifdef MSIM_USE_PURPLE_RC4
	data_out = g_new0(guchar, data_len);

    purple_cipher_context_encrypt(rc4, (const guchar*)data, 
			data_len, data_out, &data_out_len);
	purple_cipher_context_destroy(rc4);
#else
	/* Use our own RC4 code */
	purple_debug_info("msim", "Using non-purple RC4 cipher code in this version\n");
	crypt_rc4_init(&rc4, key, 0x10);
	crypt_rc4(&rc4, data, data_len);
	data_out_len = data_len;
	data_out = data;
#endif

	g_assert(data_out_len == data_len);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "response=<%s>\n", data_out);
#endif

	*response_len = data_out_len;

    return (gchar*)data_out;
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
int msim_send_im(PurpleConnection *gc, const char *who,
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
int msim_send_im_by_userid(MsimSession *session, const gchar *userid, const gchar *message, PurpleMessageFlags flags)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(userid != NULL, 0);
    g_return_val_if_fail(msim_is_userid(userid) == TRUE, 0);
    g_return_val_if_fail(message != NULL, 0);

	msim_send(session, 
			"bm", MSIM_TYPE_INTEGER, MSIM_BM_INSTANT,
			"sesskey", MSIM_TYPE_STRING, g_strdup(session->sesskey),
			"t", MSIM_TYPE_STRING, g_strdup(userid),
			"cv", MSIM_TYPE_INTEGER, MSIM_CLIENT_VERSION,
			"msg", MSIM_TYPE_STRING, g_strdup(message),
			NULL);	

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
void msim_send_im_by_userid_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
{
    send_im_cb_struct *s;
    gchar *userid;
    GHashTable *body;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

    body = msim_parse_body(g_hash_table_lookup(userinfo, "body"));
	g_return_if_fail(body != NULL);

    userid = g_hash_table_lookup(body, "UserID");

    s = (send_im_cb_struct*)data;
    msim_send_im_by_userid(session, userid, s->message, s->flags);

    g_hash_table_destroy(body);
    g_hash_table_destroy(userinfo);
    g_free(s->message);
    g_free(s->who);
} 

/**
 * Callback to handle incoming messages, after resolving userid.
 *
 * @param session 
 * @param userinfo Message from server on user's info, containing UserName.
 * @param data A gchar* of the incoming instant message's text.
 */
void msim_incoming_im_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
{
    gchar *msg;
    gchar *username;
    GHashTable *body;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

    body = msim_parse_body(g_hash_table_lookup(userinfo, "body"));
	g_return_if_fail(body != NULL);

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
int msim_incoming_im(MsimSession *session, GHashTable *table)
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


/**
 * Process a message. 
 *
 * @param gc Connection.
 * @param table Any message from the server.
 *
 * @return The return value of the function used to process the message, or -1 if
 * called with invalid parameters.
 */
int msim_process(PurpleConnection *gc, GHashTable *table)
{
    MsimSession *session;

    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(table != NULL, -1);

    session = (MsimSession*)gc->proto_data;

#ifdef MSIM_DEBUG_MSG
    purple_debug_info("msim", "-------- message -------------\n");
    g_hash_table_foreach(table, print_hash_item, NULL);
    purple_debug_info("msim", "------------------------------\n");
#endif

    if (g_hash_table_lookup(table, "nc"))
    {
        return msim_login_challenge(session, table);
    } else if (g_hash_table_lookup(table, "sesskey")) {
        purple_debug_info("msim", "SESSKEY=<%s>\n", (gchar*)g_hash_table_lookup(table, "sesskey"));

        purple_connection_update_progress(gc, _("Connected"), 3, 4);

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
		/* TODO: dump unknown msgs to file, so user can send them to me
		 * if they wish, to help add support for new messages (inspired
		 * by Alexandr Shutko, who maintains OSCAR protocol documentation). */
        purple_debug_info("msim", "msim_process: unhandled message\n");
        return 0;
    }
}
/**
 * Process a message reply from the server.
 *
 * @param session 
 * @param table Message reply from server.
 *
 * @return 0, since the 'table' field is no longer needed.
 */
int msim_process_reply(MsimSession *session, GHashTable *table)
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
int msim_error(MsimSession *session, GHashTable *table)
{
    gchar *err, *errmsg, *full_errmsg;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    err = g_hash_table_lookup(table, "err");
    errmsg = g_hash_table_lookup(table, "errmsg");

    full_errmsg = g_strdup_printf(_("Protocol error, code %s: %s"), err, errmsg);

    purple_debug_info("msim", "msim_error: %s\n", full_errmsg);

    /* TODO: check 'fatal' and die if asked to.
     * TODO: do something with the error # (localization of errmsg?)  */
    purple_notify_error(session->account, g_strdup(_("MySpaceIM Error")), 
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

#if 0
/* Not sure about this */
void msim_status_now(gchar *who, gpointer data)
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
void msim_status_cb(MsimSession *session, GHashTable *userinfo, gpointer data)
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
	g_return_if_fail(body != NULL);

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
int msim_status(MsimSession *session, GHashTable *table)
{
    gchar *status_str;
    gchar *userid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);
    g_return_val_if_fail(table != NULL, 0);

    status_str = g_hash_table_lookup(table, "msg");
    if (!status_str)
    {
        purple_debug_info("msim", "msim_status: bm is status but no status msg\n");
        return 0;
    }

    userid = g_hash_table_lookup(table, "f");
    if (!userid)
    {
        purple_debug_info("msim", "msim_status: bm is status but no f field\n");
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
 * Callback when input available.
 *
 * @param gc_uncasted A PurpleConnection pointer.
 * @param source File descriptor.
 * @param cond PURPLE_INPUT_READ
 *
 * Reads the input, and dispatches calls msim_process to handle it.
 */
void msim_input_cb(gpointer gc_uncasted, gint source, PurpleInputCondition cond)
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
    g_return_if_fail(cond == PURPLE_INPUT_READ);

    /* Only can handle so much data at once... 
     * If this happens, try recompiling with a higher MSIM_READ_BUF_SIZE.
     * Should be large enough to hold the largest protocol message.
     */
    if (session->rxoff == MSIM_READ_BUF_SIZE)
    {
        purple_debug_error("msim", "msim_input_cb: %d-byte read buffer full!\n",
                MSIM_READ_BUF_SIZE);
        purple_connection_error(gc, _("Read buffer full"));
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
        purple_connection_error(gc, _("Read error"));
        purple_debug_error("msim", "msim_input_cb: read error, ret=%d, "
			"error=%s, source=%d, fd=%d (%X))\n", 
			n, strerror(errno), source, session->fd, session->fd);
        close(source);
        return;
    } 
    else if (n == 0)
    {
        purple_debug_info("msim", "msim_input_cb: server disconnected\n");
        purple_connection_error(gc, _("Server has disconnected"));
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

#ifdef MSIM_DEBUG_RXBUF
    purple_debug_info("msim", "buf=<%s>\n", session->rxbuf);
#endif

    /* Look for \\final\\ end markers. If found, process message. */
    while((end = strstr(session->rxbuf, MSIM_FINAL_STRING)))
    {
        GHashTable *table;

#ifdef MSIM_DEBUG_RXBUF
        purple_debug_info("msim", "in loop: buf=<%s>\n", session->rxbuf);
#endif
        *end = 0;
        table = msim_parse(g_strdup(session->rxbuf));
        if (!table)
        {
            purple_debug_info("msim", "msim_input_cb: couldn't parse <%s>\n", 
					session->rxbuf);
            purple_connection_error(gc, _("Unparseable message"));
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
void msim_connect_cb(gpointer data, gint source, const gchar *error_message)
{
    PurpleConnection *gc;
    MsimSession *session;

    g_return_if_fail(data != NULL);

    gc = (PurpleConnection*)data;
    session = gc->proto_data;

    if (source < 0)
    {
        purple_connection_error(gc, _("Couldn't connect to host"));
        purple_connection_error(gc, g_strdup_printf(_("Couldn't connect to host: %s (%d)"), 
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
MsimSession *msim_session_new(PurpleAccount *acct)
{
    MsimSession *session;

    g_return_val_if_fail(acct != NULL, NULL);

    session = g_new0(MsimSession, 1);

    session->magic = MSIM_SESSION_STRUCT_MAGIC;
    session->account = acct;
    session->gc = purple_account_get_connection(acct);
    session->fd = -1;
    session->user_lookup_cb = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);  /* do NOT free function pointers! */
    session->user_lookup_cb_data = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, g_free);
    session->user_lookup_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
		   	g_free, (GDestroyNotify)g_hash_table_destroy);
    session->rxoff = 0;
    session->rxbuf = g_new0(gchar, MSIM_READ_BUF_SIZE);
	session->next_rid = 1;
    
    return session;
}

/**
 * Free a session.
 *
 * @param session The session to destroy.
 */
void msim_session_destroy(MsimSession *session)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));

    session->magic = -1;

    g_free(session->rxbuf);
    g_free(session->userid);
    g_free(session->sesskey);

    g_free(session);
}
                 


/** 
 * Close the connection.
 * 
 * @param gc The connection.
 */
void msim_close(PurpleConnection *gc)
{
    g_return_if_fail(gc != NULL);
    
    msim_session_destroy(gc->proto_data);
}


/**
 * Check if a string is a userid (all numeric).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is userid, FALSE if not.
 */
gboolean msim_is_userid(const gchar *user)
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
gboolean msim_is_email(const gchar *user)
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
void msim_lookup_user(MsimSession *session, const gchar *user, MSIM_USER_LOOKUP_CB cb, gpointer data)
{
    gchar *field_name;
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

    rid = session->next_rid;
	++session->next_rid;

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


	msim_send(session,
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_STRING, g_strdup(session->sesskey),
			"cmd", MSIM_TYPE_INTEGER, 1,
			"dsn", MSIM_TYPE_INTEGER, dsn,
			"uid", MSIM_TYPE_STRING, g_strdup(session->userid),
			"lid", MSIM_TYPE_INTEGER, lid,
			"rid", MSIM_TYPE_INTEGER, rid,
			/* TODO: dictionary field type */
			"body", MSIM_TYPE_STRING, g_strdup_printf("%s=%s", field_name, user),
			NULL);
} 


/**
 * Obtain the status text for a buddy.
 *
 * @param buddy The buddy to obtain status text for.
 *
 * @return Status text, or NULL if error.
 *
 */
char *msim_status_text(PurpleBuddy *buddy)
{
    MsimSession *session;
    GHashTable *userinfo;
    gchar *display_name;

    g_return_val_if_fail(buddy != NULL, NULL);

    session = (MsimSession*)buddy->account->gc->proto_data;
    g_return_val_if_fail(MSIM_SESSION_VALID(session), NULL);
    g_return_val_if_fail(session->user_lookup_cache != NULL, NULL);

    userinfo = g_hash_table_lookup(session->user_lookup_cache, buddy->name);
    if (!userinfo)
    {
        return g_strdup("");
    }

    display_name = g_hash_table_lookup(userinfo, "DisplayName");
    g_return_val_if_fail(display_name != NULL, NULL);

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
void msim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
    g_return_if_fail(buddy != NULL);
    g_return_if_fail(user_info != NULL);

    if (PURPLE_BUDDY_IS_ONLINE(buddy))
    {
        MsimSession *session;
        GHashTable *userinfo;

        session = (MsimSession*)buddy->account->gc->proto_data;

        g_return_if_fail(MSIM_SESSION_VALID(session));
        g_return_if_fail(session->user_lookup_cache);

        userinfo = g_hash_table_lookup(session->user_lookup_cache, buddy->name);

        g_return_if_fail(userinfo != NULL);

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
PurplePluginProtocolInfo prpl_info =
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
    NULL,              /* roomlist_room_serialize */
	NULL,			   /* _purple_reserved1 */
	NULL,			   /* _purple_reserved2 */
	NULL,			   /* _purple_reserved3 */
	NULL 			   /* _purple_reserved4 */
};



/** Based on MSN's plugin info comments. */
PurplePluginInfo info =
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

    msim_load,                                        /**< load           */
    NULL,                                             /**< unload         */
    NULL,                                             /**< destroy        */
    NULL,                                             /**< ui_info        */
    &prpl_info,                                       /**< extra_info     */
    NULL,                                             /**< prefs_info     */

    /* msim_actions */
    NULL,

	NULL,											  /**< reserved1      */
	NULL,											  /**< reserved2      */
	NULL,											  /**< reserved3      */
	NULL 											  /**< reserved4      */
};

#include "message.h"

void init_plugin(PurplePlugin *plugin) 
{
	PurpleAccountOption *option;
#ifdef _TEST_MSIM_MSG
	MsimMessage *msg = msim_msg_new();
	msg = msim_msg_append(msg, "bx", MSIM_TYPE_BINARY, g_string_new_len(g_strdup("XXX"), 3));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v1"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_INTEGER, 42);
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v43"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v52/xxx\\yyy"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v7"));
	purple_debug_info("msim", "msg=%s\n", msim_msg_debug_string(msg));
	purple_debug_info("msim", "msg=%s\n", msim_msg_pack(msg));
	msim_msg_free(msg);
	exit(0);
#endif

	/* TODO: default to automatically try different ports. Make the user be
	 * able to set the first port to try (like LastConnectedPort in Windows client).  */
	option = purple_account_option_string_new(_("Connect server"), "server", MSIM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Connect port"), "port", MSIM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(myspace, init_plugin, info);
