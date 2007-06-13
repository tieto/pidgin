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

#include "message.h"
#include "persist.h"
#include "myspace.h"

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
    PurpleStatusType *status;

    purple_debug_info("myspace", "returning status types\n");

    types = NULL;

	/* TODO: Fix these:
	 *
	 * g_log: purple_presence_get_active_status: assertion `presence != NULL' failed
	 * g_log: purple_status_get_name: assertion `status != NULL' failed
	 * [...]
	 *
	 * and 
	 * g_log: purple_presence_set_status_active: assertion `status != NULL' failed
	 * [...]
	 */
    status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_append(types, status);

    status = purple_status_type_new_full(PURPLE_STATUS_AWAY, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_append(types, status);

    status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_append(types, status);

    status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_append(types, status);

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
gchar *str_replace(const gchar *str, const gchar *old, const gchar *new)
{
	char **items;
	char *ret;

	items = g_strsplit(str, old, -1);
	ret = g_strjoinv(new, items);
	g_free(items);
	return ret;
}




#ifdef MSIM_DEBUG_MSG
void print_hash_item(gpointer key, gpointer value, gpointer user_data)
{
    purple_debug_info("msim", "%s=%s\n", (char *)key, (char *)value);
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
 * @param msg Login challenge message.
 *
 * @return TRUE if successful, FALSE if not
 */
gboolean msim_login_challenge(MsimSession *session, MsimMessage *msg) 
{
    PurpleAccount *account;
    gchar *response;
	guint response_len;
	gchar *nc;
	gsize nc_len;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    g_return_val_if_fail(msim_msg_get_binary(msg, "nc", &nc, &nc_len), FALSE);

    account = session->account;

	g_return_val_if_fail(account != NULL, FALSE);

    purple_connection_update_progress(session->gc, _("Reading challenge"), 1, 4);

    purple_debug_info("msim", "nc is %d bytes, decoded\n", nc_len);

    if (nc_len != 0x40)
    {
        purple_debug_info("msim", "bad nc length: %x != 0x40\n", nc_len);
        purple_connection_error(session->gc, _("Unexpected challenge length from server"));
        return FALSE;
    }

    purple_connection_update_progress(session->gc, _("Logging in"), 2, 4);

    response = msim_compute_login_response(nc, account->username, account->password, &response_len);

    g_free(nc);

	return msim_send(session, 
			"login2", MSIM_TYPE_INTEGER, MSIM_AUTH_ALGORITHM,
			/* This is actually user's email address. */
			"username", MSIM_TYPE_STRING, g_strdup(account->username),
			/* GString and gchar * response will be freed in msim_msg_free() in msim_send(). */
			"response", MSIM_TYPE_BINARY, g_string_new_len(response, response_len),
			"clientver", MSIM_TYPE_INTEGER, MSIM_CLIENT_VERSION,
			"reconn", MSIM_TYPE_INTEGER, 0,
			"status", MSIM_TYPE_INTEGER, 100,
			"id", MSIM_TYPE_INTEGER, 1,
			NULL);
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
gchar *msim_compute_login_response(gchar nonce[2 * NONCE_SIZE], 
		gchar *email, gchar *password, guint *response_len)
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
    gchar *password_utf16le;
    guchar *data;
	guchar *data_out;
	size_t data_len, data_out_len;
	gsize conv_bytes_read, conv_bytes_written;
	GError *conv_error;
#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
	int i;
#endif

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
    purple_cipher_digest_region("sha1", (guchar *)password_utf16le, 
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
    purple_cipher_context_append(key_context, (guchar *)(nonce + NONCE_SIZE), NONCE_SIZE);
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

    purple_cipher_context_encrypt(rc4, (const guchar *)data, 
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

    return (gchar *)data_out;
}

/**
 * Schedule an IM to be sent once the user ID is looked up. 
 *
 * @param gc Connection.
 * @param who A user id, email, or username to send the message to.
 * @param message Instant message text to send.
 * @param flags Flags.
 *
 * @return 1 if successful or postponed, -1 if failed
 *
 * Allows sending to a user by username, email address, or userid. If
 * a username or email address is given, the userid must be looked up.
 * This function does that by calling msim_postprocess_outgoing().
 */
int msim_send_im(PurpleConnection *gc, const char *who,
                            const char *message, PurpleMessageFlags flags)
{
    MsimSession *session;
    
    g_return_val_if_fail(gc != NULL, 0);
    g_return_val_if_fail(who != NULL, 0);
    g_return_val_if_fail(message != NULL, 0);

	/* 'flags' has many options, not used here. */

	session = gc->proto_data;

	/* TODO: const-correctness */
	if (msim_send_bm(session, (char *)who, (char *)message, MSIM_BM_INSTANT))
	{
		/* Return 1 to have Purple show this IM as being sent, 0 to not. I always
		 * return 1 even if the message could not be sent, since I don't know if
		 * it has failed yet--because the IM is only sent after the userid is
		 * retrieved from the server (which happens after this function returns).
		 */
		return 1;
	} else {
		return -1;
	}
    /*
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
}

/** Send a buddy message of a given type.
 *
 * @param session
 * @param who Username to send message to.
 * @param text Message text to send. Not freed; will be copied.
 * @param type A MSIM_BM_* constant.
 *
 * Buddy messages ('bm') include instant messages, action messages, status messages, etc.
 */
gboolean msim_send_bm(MsimSession *session, gchar *who, gchar *text, int type)
{
	gboolean rc;
	MsimMessage *msg;
    const char *from_username = session->account->username;

    purple_debug_info("msim", "sending %d message from %s to %s: %s\n",
                  type, from_username, who, text);

	msg = msim_msg_new(TRUE,
			"bm", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(type),
			"sesskey", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(session->sesskey),
			/* 't' will be inserted here */
			"cv", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(MSIM_CLIENT_VERSION),
			"msg", MSIM_TYPE_STRING, g_strdup(text),
			NULL);

	rc = msim_postprocess_outgoing(session, msg, (char *)who, "t", "cv");

	msim_msg_free(msg);

	return rc;
}

/**
 * Handle an incoming instant message.
 *
 * @param session The session
 * @param msg Message from the server, containing 'f' (userid from) and 'msg'. 
 * 			  Should also contain username in _username from preprocessing.
 *
 * @return TRUE if successful.
 */
gboolean msim_incoming_im(MsimSession *session, MsimMessage *msg)
{
    gchar *username;

    username = msim_msg_get_string(msg, "_username");

    serv_got_im(session->gc, username, msim_msg_get_string(msg, "msg"), PURPLE_MESSAGE_RECV, time(NULL));

	g_free(username);
	/* TODO: Free copy cloned from msim_incoming_im(). */
	//msim_msg_free(msg);

	return TRUE;
}

/**
 * Handle an incoming action message.
 *
 * @param session
 * @param msg
 *
 * @return TRUE if successful.
 *
 * UNTESTED
 */
gboolean msim_incoming_action(MsimSession *session, MsimMessage *msg)
{
	gchar *msg_text, *username;
	gboolean rc;

	msg_text = msim_msg_get_string(msg, "msg");
	username = msim_msg_get_string(msg, "_username");

	purple_debug_info("msim", "msim_incoming_action: action <%s> from <%d>\n", msg_text, username);

	if (strcmp(msg_text, "%typing%") == 0)
	{
		/* TODO: find out if msim repeatedly sends typing messages, so we can give it a timeout. */
		serv_got_typing(session->gc, username, 0, PURPLE_TYPING);
		rc = TRUE;
	} else if (strcmp(msg_text, "%stoptyping%") == 0) {
		serv_got_typing_stopped(session->gc, username);
		rc = TRUE;
	} else {
		/* TODO: make a function, msim_unrecognized(), that logs all unhandled msgs to file. */
		purple_debug_info("msim", "msim_incoming_action: for %s, unknown msg %s\n",
				username, msg_text);
		rc = FALSE;
	}


	g_free(msg_text);
	g_free(username);

	return rc;
}

/** 
 * Handle when our user starts or stops typing to another user.
 *
 * @param gc
 * @param name The buddy name to which our user is typing to
 * @param state PURPLE_TYPING, PURPLE_TYPED, PURPLE_NOT_TYPING
 *
 * NOT CURRENTLY USED OR COMPLETE
 */
unsigned int msim_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state)
{
	char *typing_str;
	MsimSession *session;

	session = (MsimSession *)gc->proto_data;

	switch (state)
	{	
		case PURPLE_TYPING: 
			typing_str = "%typing%"; 
			break;

		case PURPLE_TYPED:
		case PURPLE_NOT_TYPING:
		default:
			typing_str = "%stoptyping%";
			break;
	}

	purple_debug_info("msim", "msim_send_typing(%s): %d (%s)\n", name, state, typing_str);
	msim_send_bm(session, (gchar *)name, typing_str, MSIM_BM_ACTION);
	return 0;
}

/** After a uid is resolved to username, tag it with the username and submit for processing. 
 * 
 * @param session
 * @param userinfo Response messsage to resolving request.
 * @param data MsimMessage *, the message to attach information to. 
 */
static void msim_incoming_resolved(MsimSession *session, MsimMessage *userinfo, gpointer data)
{
	gchar *body_str;
	GHashTable *body;
	gchar *username;
	MsimMessage *msg;

	body_str = msim_msg_get_string(userinfo, "body");
	g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_return_if_fail(body != NULL);
	g_free(body_str);

	username = g_hash_table_lookup(body, "UserName");
	g_return_if_fail(username != NULL);

	msg = (MsimMessage *)data;
	/* Special elements name beginning with '_', we'll use internally within the
	 * program (did not come from the wire). */
	msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));

	msim_process(session, msg);

	/* TODO: Free copy cloned from  msim_preprocess_incoming(). */
	//XXX msim_msg_free(msg);
	g_hash_table_destroy(body);
}

#ifdef _MSIM_UID2USERNAME_WORKS
/* Lookup a username by userid, from buddy list. 
 *
 * @param wanted_uid
 *
 * @return Username of wanted_uid, if on blist, or NULL. Static string. 
 *
 * XXX WARNING: UNKNOWN MEMORY CORRUPTION HERE! 
 */
static const gchar *msim_uid2username_from_blist(MsimSession *session, guint wanted_uid)
{
	GSList *buddies, *buddies_head;

	for (buddies = buddies_head = purple_find_buddies(session->account, NULL); 
			buddies; 
			buddies = g_slist_next(buddies))
	{
		PurpleBuddy *buddy;
		guint uid;
		gchar *name;

		buddy = buddies->data;

		uid = purple_blist_node_get_int(&buddy->node, "uid");

		/* name = buddy->name; */								/* crash */
		/* name = PURPLE_BLIST_NODE_NAME(&buddy->node);  */		/* crash */

		/* XXX Is this right? Memory corruption here somehow. Happens only
		 * when return one of these values. */
		name = purple_buddy_get_name(buddy); 					/* crash */
		/* return name; */										/* crash (with above) */

		/* name = NULL; */										/* no crash */
		/* return NULL; */										/* no crash (with anything) */

		/* crash =
*** glibc detected *** pidgin: realloc(): invalid pointer: 0x0000000000d2aec0 ***
======= Backtrace: =========
/lib/libc.so.6(__libc_realloc+0x323)[0x2b7bfc012e03]
/usr/lib/libglib-2.0.so.0(g_realloc+0x31)[0x2b7bfba79a41]
/usr/lib/libgtk-x11-2.0.so.0(gtk_tree_path_append_index+0x3a)[0x2b7bfa110d5a]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa1287dc]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa128e56]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa128efd]
/usr/lib/libglib-2.0.so.0(g_main_context_dispatch+0x1b4)[0x2b7bfba72c84]
/usr/lib/libglib-2.0.so.0[0x2b7bfba75acd]
/usr/lib/libglib-2.0.so.0(g_main_loop_run+0x1ca)[0x2b7bfba75dda]
/usr/lib/libgtk-x11-2.0.so.0(gtk_main+0xa3)[0x2b7bfa0475f3]
pidgin(main+0x8be)[0x46b45e]
/lib/libc.so.6(__libc_start_main+0xf4)[0x2b7bfbfbf0c4]
pidgin(gtk_widget_grab_focus+0x39)[0x429ab9]

or:
 *** glibc detected *** /usr/local/bin/pidgin: malloc(): memory corruption (fast): 0x0000000000c10076 ***
 (gdb) bt
#0  0x00002b4074ecd47b in raise () from /lib/libc.so.6
#1  0x00002b4074eceda0 in abort () from /lib/libc.so.6
#2  0x00002b4074f0453b in __fsetlocking () from /lib/libc.so.6
#3  0x00002b4074f0c810 in free () from /lib/libc.so.6
#4  0x00002b4074f0d6dd in malloc () from /lib/libc.so.6
#5  0x00002b4074974b5b in g_malloc () from /usr/lib/libglib-2.0.so.0
#6  0x00002b40749868bf in g_strdup () from /usr/lib/libglib-2.0.so.0
#7  0x00002b407810969f in msim_parse (
    raw=0xd2a910 "\\bm\\100\\f\\3656574\\msg\\|s|0|ss|Offline")
	    at message.c:648
#8  0x00002b407810889c in msim_input_cb (gc_uncasted=0xcf92c0, 
    source=<value optimized out>, cond=<value optimized out>) at myspace.c:1478


	Why is it crashing in msim_parse()'s g_strdup()?
*/


		purple_debug_info("msim", "msim_uid2username_from_blist: %s's uid=%d (want %d)\n",
				name, uid, wanted_uid);

		if (uid == wanted_uid)
		{
			g_slist_free(buddies_head);

			return name;
		}
	}

	g_slist_free(buddies_head);
	return NULL;
}

#endif

/** Preprocess incoming messages, resolving as needed, calling msim_process() when ready to process.
 *
 * TODO: if no uid to resolve, process immediately. if uid, check if know username,
 * if so, tag and process immediately, if not, queue, send resolve msg, and process
 * once get username.
 *
 * @param session
 * @param msg MsimMessage *, freed by caller.
 */
gboolean msim_preprocess_incoming(MsimSession *session, MsimMessage *msg)
{
	if (msim_msg_get(msg, "bm") && msim_msg_get(msg, "f"))
	{
		guint uid;
		const gchar *username;

		/* 'f' = userid message is from, in buddy messages */
		uid = msim_msg_get_integer(msg, "f");

		/* TODO: Make caching work. Currently it is commented out because
		 * it crashes for unknown reasons, memory realloc error. */
//#define _MSIM_UID2USERNAME_WORKS
#ifdef _MSIM_UID2USERNAME_WORKS
		username = msim_uid2username_from_blist(session, uid); 
#else
		username = NULL;
#endif

		if (username)
		{
			/* Know username already, use it. */
			purple_debug_info("msim", "msim_preprocess_incoming: tagging with _username=%s\n",
					username);
			msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));
			return msim_process(session, msg);

		} else {
			/* Send lookup request. */
			/* XXX: where is msim_msg_get_string() freed? make _strdup and _nonstrdup. */
			purple_debug_info("msim", "msim_incoming: sending lookup, setting up callback\n");
			msim_lookup_user(session, msim_msg_get_string(msg, "f"), msim_incoming_resolved, msim_msg_clone(msg)); 

			/* indeterminate */
			return TRUE;
		}
	} else {
		/* Nothing to resolve - send directly to processing. */
		return msim_process(session, msg);
	}
}

/**
 * Process a message. 
 *
 * @param session
 * @param msg A message from the server, ready for processing (possibly with resolved username information attached). Caller frees.
 *
 * @return TRUE if successful. FALSE if processing failed.
 */
gboolean msim_process(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(session != NULL, -1);
    g_return_val_if_fail(msg != NULL, -1);

#ifdef MSIM_DEBUG_MSG
	{
		msim_msg_dump("ready to process: %s\n", msg);
	}
#endif

    if (msim_msg_get(msg, "nc"))
    {
        return msim_login_challenge(session, msg);
    } else if (msim_msg_get(msg, "sesskey")) {

        purple_connection_update_progress(session->gc, _("Connected"), 3, 4);

        session->sesskey = msim_msg_get_integer(msg, "sesskey");
        purple_debug_info("msim", "SESSKEY=<%d>\n", session->sesskey);

        /* Comes with: proof,profileid,userid,uniquenick -- all same values
		 * some of the time, but can vary. */
        session->userid = msim_msg_get_integer(msg, "userid");

        purple_connection_set_state(session->gc, PURPLE_CONNECTED);

        return TRUE;
    } else if (msim_msg_get(msg, "bm"))  {
        guint bm;
       
        bm = msim_msg_get_integer(msg, "bm");
        switch (bm)
        {
            case MSIM_BM_STATUS:
                return msim_status(session, msg);
            case MSIM_BM_INSTANT:
                return msim_incoming_im(session, msg);
			case MSIM_BM_ACTION:
				return msim_incoming_action(session, msg);
            default:
                /* Not really an IM, but show it for informational 
                 * purposes during development. */
                return msim_incoming_im(session, msg);
        }
    } else if (msim_msg_get(msg, "rid")) {
        return msim_process_reply(session, msg);
    } else if (msim_msg_get(msg, "error")) {
        return msim_error(session, msg);
    } else if (msim_msg_get(msg, "ka")) {
        purple_debug_info("msim", "msim_process: got keep alive\n");
        return TRUE;
    } else {
		/* TODO: dump unknown msgs to file, so user can send them to me
		 * if they wish, to help add support for new messages (inspired
		 * by Alexandr Shutko, who maintains OSCAR protocol documentation). */
        purple_debug_info("msim", "msim_process: unhandled message\n");
        return FALSE;
    }
}

/**
 * Process a persistance message reply from the server.
 *
 * @param session 
 * @param msg Message reply from server.
 *
 * @return TRUE if successful.
 */
gboolean msim_process_reply(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
    
    if (msim_msg_get(msg, "rid"))  /* msim_lookup_user sets callback for here */
    {
        MSIM_USER_LOOKUP_CB cb;
        gpointer data;
        guint rid;
        GHashTable *body;
        gchar *username, *body_str;

        rid = msim_msg_get_integer(msg, "rid");

        /* Cache the user info. Currently, the GHashTable of user info in
         * this cache never expires so is never freed. TODO: expire and destroy
         * 
         * Some information never changes (username->userid map), some does.
         * TODO: Cache what doesn't change only
         */
		body_str = msim_msg_get_string(msg, "body");
        body = msim_parse_body(body_str);
		g_free(body_str);


		/* TODO: implement a better hash-like interface, and use it. */
        username = g_hash_table_lookup(body, "UserName");

		/* TODO: Save user info reply for msim_tooltip_text. */
		/* TODO: get rid of user_lookup_cache, and find another way to 
		 * pass the relevant information to msim_tooltip_text. */
		/* g_hash_table_insert(session->user_lookup_cache, username, body); */

        if (username)
        {
			PurpleBuddy *buddy;
			gchar *uid;

			uid = g_hash_table_lookup(body, "UserID");
			g_assert(uid);

			/* Associate uid with user on buddy list. This is saved to blist.xml. */

			purple_debug_info("msim", "associating %d with %s\n", uid, username);

			buddy = purple_find_buddy(session->account, username);
			if (buddy)
			{
				purple_blist_node_set_int(&buddy->node, "uid", atoi(uid));
			} 
        } else {
            purple_debug_info("msim", 
					"msim_process_reply: not caching body, no UserName\n");
        } 

        /* If a callback is registered for this userid lookup, call it. */
        cb = g_hash_table_lookup(session->user_lookup_cb, GUINT_TO_POINTER(rid));
        data = g_hash_table_lookup(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));

        if (cb)
        {
            purple_debug_info("msim", 
					"msim_process_body: calling callback now\n");
			/* Clone message, so that the callback 'cb' can use it (needs to free it also). */
            cb(session, msim_msg_clone(msg), data);
            g_hash_table_remove(session->user_lookup_cb, GUINT_TO_POINTER(rid));
            g_hash_table_remove(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));
        } else {
            purple_debug_info("msim", 
					"msim_process_body: no callback for rid %d\n", rid);
        }
    }

	return TRUE;
}

/**
 * Handle an error from the server.
 *
 * @param session 
 * @param msg The message.
 *
 * @return TRUE if successfully reported error.
 */
gboolean msim_error(MsimSession *session, MsimMessage *msg)
{
    gchar *errmsg, *full_errmsg;
	guint err;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    err = msim_msg_get_integer(msg, "err");
    errmsg = msim_msg_get_string(msg, "errmsg");

    full_errmsg = g_strdup_printf(_("Protocol error, code %d: %s"), err, errmsg);

	g_free(errmsg);

    purple_debug_info("msim", "msim_error: %s\n", full_errmsg);

    /* TODO: do something with the error # (localization of errmsg?)  */
    purple_notify_error(session->account, g_strdup(_("MySpaceIM Error")), 
            full_errmsg, NULL);

	/* Destroy session if fatal. */
    if (msim_msg_get(msg, "fatal"))
    {
        purple_debug_info("msim", "fatal error, closing\n");
        purple_connection_error(session->gc, full_errmsg);
        close(session->fd);
		/* Do not call msim_session_destroy(session) - called in msim_close(). */
    }

    return TRUE;
}

/**
 * Process incoming status messages.
 *
 * @param session
 * @param msg Status update message. Caller frees.
 *
 * @return TRUE if successful.
 */
gboolean msim_status(MsimSession *session, MsimMessage *msg)
{
    PurpleBuddyList *blist;
    PurpleBuddy *buddy;
    //PurpleStatus *status;
    gchar **status_array;
    GList *list;
    gchar *status_headline;
    gchar *status_str;
    gint i, status_code, purple_status_code;
    gchar *username;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    status_str = msim_msg_get_string(msg, "msg");
	g_return_val_if_fail(status_str != NULL, FALSE);

	msim_msg_dump("msim_status msg=%s\n", msg);

	/* Helpfully looked up by msim_incoming_resolve() for us. */
    username = msim_msg_get_string(msg, "_username");
    /* Note: DisplayName doesn't seem to be resolvable. It could be displayed on
     * the buddy list, if the UserID was stored along with it. */

	if (username == NULL)
	{
		g_free(status_str);
		g_return_val_if_fail(NULL, FALSE);
	}

    purple_debug_info("msim", 
			"msim_status_cb: updating status for <%s> to <%s>\n", 
			username, status_str);

    /* TODO: generic functions to split into a GList, part of MsimMessage */
    status_array = g_strsplit(status_str, "|", 0);
    for (list = NULL, i = 0;
            status_array[i];
            i++)
    {
		/* Note: this adds the 0th ordinal too, which might not be a value
		 * at all (the 0 in the 0|1|2|3... status fields, but 0 always appears blank).
		 */
        list = g_list_append(list, status_array[i]);
    }

    /* Example fields: 
	 *  |s|0|ss|Offline 
	 *  |s|1|ss|:-)|ls||ip|0|p|0 
	 *
	 * TODO: write list support in MsimMessage, and use it here.
	 */

    status_code = atoi(g_list_nth_data(list, MSIM_STATUS_ORDINAL_ONLINE));
	purple_debug_info("msim", "msim_status_cb: %s's status code = %d\n", username, status_code);
    status_headline = g_list_nth_data(list, MSIM_STATUS_ORDINAL_HEADLINE);

    blist = purple_get_blist();

    /* Add buddy if not found */
    buddy = purple_find_buddy(session->account, username);
    if (!buddy)
    {
        /* TODO: purple aliases, userids and usernames */
        purple_debug_info("msim", 
				"msim_status: making new buddy for %s\n", username);
        buddy = purple_buddy_new(session->account, username, NULL);

        purple_blist_add_buddy(buddy, NULL, NULL, NULL);
		/* All buddies on list should have 'uid' integer associated with them. */
		purple_blist_node_set_int(&buddy->node, "uid", msim_msg_get_integer(msg, "f"));
		purple_debug_info("msim", "UID=%d\n", purple_blist_node_get_int(&buddy->node, "uid"));
    } else {
        purple_debug_info("msim", "msim_status: found buddy %s\n", username);
    }

    /* TODO: show headline */
  
    /* Set user status */	
    switch (status_code)
	{
		case 1: purple_status_code = PURPLE_STATUS_AVAILABLE;
				break;
		case 0: purple_status_code = PURPLE_STATUS_OFFLINE;	
				break;
		default:
				purple_debug_info("msim", "msim_status_cb for %s, unknown status code %d\n",
						username, status_code);
				purple_status_code = PURPLE_STATUS_AVAILABLE;
	}
	purple_prpl_got_user_status(session->account, username, purple_primitive_get_id_from_type(purple_status_code), NULL);

    g_strfreev(status_array);
	g_free(status_str);
	g_free(username);
    g_list_free(list);

    return TRUE;
}

/** Add a buddy to user's buddy list. TODO: make work. Should receive statuses from added buddy. */
void msim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *msg;

	session = (MsimSession *)gc->proto_data;
	purple_debug_info("msim", "msim_add_buddy: want to add %s to %s\n", buddy->name,
			group ? group->name : "(no group)");

	msg = msim_msg_new(TRUE,
			"addbuddy", MSIM_TYPE_BOOLEAN, TRUE,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			/* "newprofileid" will be inserted here with uid. */
			"reason", MSIM_TYPE_STRING, g_strdup(""),
			NULL);

	if (!msim_postprocess_outgoing(session, msg, buddy->name, "newprofileid", "reason"))
	{
		purple_notify_error(NULL, NULL, _("Failed to add buddy"), _("'addbuddy' command failed."));
		msim_msg_free(msg);
		return;
	}
	msim_msg_free(msg);

	/* TODO: update blocklist */

#if 0
	/* TODO */
	if (!msim_send(session,
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_PUT,
			"dsn", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_DSN,
			"lid", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_LID,
			/* TODO: msim_send_persist, to handle all this rid business */
			"rid", MSIM_TYPE_INTEGER, session->next_rid++,
			"body", MSIM_TYPE_STRING,
				g_strdup_printf("ContactID=%s\034"
				"GroupName=%s\034"
				"Position=1000\034"
				"Visibility=1\034"
				"NickName=\034"
				"NameSelect=0",
				buddy->name, group->name),
			NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to add buddy"), _("persist command failed"));
		return;
	}
#endif
}

/** Callback for msim_postprocess_outgoing() to add a userid to a message, and send it (once receiving userid).
 *
 * @param session
 * @param userinfo The user information reply message, containing the user ID
 * @param data The message to postprocess and send.
 *
 * The data message should contain these fields:
 *
 *  _uid_field_name: string, name of field to add with userid from userinfo message
 *  _uid_before: string, name of field before field to insert, or NULL for end
 *
 *
 * If the field named by _uid_field_name already exists, then its string contents will
 * be treated as a format string, containing a "%d", to be replaced by the userid.
 *
 * If the field named by _uid_field_name does not exist, it will be added before the
 * field named by _uid_before, as an integer, with the userid.
 */
void msim_postprocess_outgoing_cb(MsimSession *session, MsimMessage *userinfo, gpointer data)
{
	gchar *body_str;
	GHashTable *body;
	gchar *uid, *uid_field_name, *uid_before;
	MsimMessage *msg;

	msg = (MsimMessage *)data;

	msim_msg_dump("msim_postprocess_outgoing_cb() got msg=%s\n", msg);

	/* Obtain userid from userinfo message. */
	body_str = msim_msg_get_string(userinfo, "body");
	g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_free(body_str);

	uid = g_strdup(g_hash_table_lookup(body, "UserID"));
	g_hash_table_destroy(body);

	uid_field_name = msim_msg_get_string(msg, "_uid_field_name");
	uid_before = msim_msg_get_string(msg, "_uid_before");

	/* First, check - if the field already exists, treat it as a format string. */
	if (msim_msg_get(msg, uid_field_name))
	{
		MsimMessageElement *elem;
		gchar *fmt_string;

		elem = msim_msg_get(msg, uid_field_name);
		g_return_if_fail(elem->type == MSIM_TYPE_STRING);

		/* Get the raw string, not with msim_msg_get_string() since that copies it. 
		 * Want the original string so can free it. */
		fmt_string = (gchar *)(elem->data);

		/* Format string: ....%d... */	
		elem->data = g_strdup_printf(fmt_string, uid);

		g_free(fmt_string);
	} else {
		/* Otherwise, insert new field into outgoing message. */
		msg = msim_msg_insert_before(msg, uid_before, uid_field_name, MSIM_TYPE_STRING, uid);
	}
	
	/* Send */
	if (!msim_msg_send(session, msg))
	{
		purple_debug_info("msim", "msim_postprocess_outgoing_cb: sending failed for message: %s\n", msg);
	}

	/* Free field names AFTER sending message, because MsimMessage does NOT copy
	 * field names - instead, treats them as static strings (which they usually are).
	 */
	g_free(uid_field_name);
	g_free(uid_before);

	//msim_msg_free(msg);
}

/** Postprocess and send a message.
 *
 * @param session
 * @param msg Message to postprocess. Will NOT be freed.
 * @param username Username to resolve. Assumed to be a static string (will not be freed or copied).
 * @param uid_field_name Name of new field to add, containing uid of username. Static string.
 * @param uid_before Name of existing field to insert username field before. Static string.
 *
 * @return Postprocessed message.
 */
gboolean msim_postprocess_outgoing(MsimSession *session, MsimMessage *msg, gchar *username, 
	gchar *uid_field_name, gchar *uid_before)
{
    PurpleBuddy *buddy;
	guint uid;
	gboolean rc;

	/* Store information for msim_postprocess_outgoing_cb(). */
	purple_debug_info("msim", "msim_postprocess_outgoing(u=%s,ufn=%s,ub=%s)\n",
			username, uid_field_name, uid_before);
	msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));
	msg = msim_msg_append(msg, "_uid_field_name", MSIM_TYPE_STRING, g_strdup(uid_field_name));
	msg = msim_msg_append(msg, "_uid_before", MSIM_TYPE_STRING, g_strdup(uid_before));

	/* First, try the most obvious. If numeric userid is given, use that directly. */
    if (msim_is_userid(username))
    {
		uid = atol(username);
    } else {
		/* Next, see if on buddy list and know uid. */
		buddy = purple_find_buddy(session->account, username);
		if (buddy)
		{
			uid = purple_blist_node_get_int(&buddy->node, "uid");
		} else {
			uid = 0;
		}

		if (!buddy || !uid)
		{
			purple_debug_info("msim", ">>> msim_postprocess_outgoing: couldn't find username %s in blist\n",
					username);
			msim_msg_dump("msim_postprocess_outgoing - scheduling lookup, msg=%s\n", msg);
			/* TODO: where is cloned message freed? Should be in _cb. */
			msim_lookup_user(session, username, msim_postprocess_outgoing_cb, msim_msg_clone(msg));
			return TRUE;		/* not sure of status yet - haven't sent! */
		}
	}
	
	/* Already have uid, insert it and send msg immediately. */
	purple_debug_info("msim", "msim_postprocess_outgoing: found username %s has uid %d\n",
			username, uid);

	msg = msim_msg_insert_before(msg, uid_before, uid_field_name, MSIM_TYPE_INTEGER, GUINT_TO_POINTER(uid));

	rc = msim_msg_send(session, msg);

	//msim_msg_free(msg);

	return rc;
}

/** Remove a buddy from the user's buddy list. */
void msim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *msg;
	MsimMessage *persist_msg;

	session = (MsimSession *)gc->proto_data;

	msg = msim_msg_new(FALSE,
				"delbuddy", MSIM_TYPE_BOOLEAN, TRUE,
				"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
				/* 'delprofileid' with uid will be inserted here. */
				NULL);
	/* TODO: free msg */
	if (!msim_postprocess_outgoing(session, msg, buddy->name, "delprofileid", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("'delbuddy' command failed"));
		return;
	}

	persist_msg = msim_msg_new(FALSE, 
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_DELETE,
			"dsn", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_DSN,
			"lid", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_LID,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"rid", MSIM_TYPE_INTEGER, session->next_rid++,
			"body", MSIM_TYPE_STRING, g_strdup("ContactID=%d"),
			NULL);

	/* TODO: free msg */
	if (!msim_postprocess_outgoing(session, msg, buddy->name, "body", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("persist command failed"));	
		return;
	}

	/* TODO: update blocklist */
}



/**
 * Callback when input available.
 *
 * @param gc_uncasted A PurpleConnection pointer.
 * @param source File descriptor.
 * @param cond PURPLE_INPUT_READ
 *
 * Reads the input, and calls msim_preprocess_incoming() to handle it.
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

    gc = (PurpleConnection *)(gc_uncasted);
    account = purple_connection_get_account(gc);
    session = gc->proto_data;

    g_return_if_fail(cond == PURPLE_INPUT_READ);
	/* TODO: fix bug #193, crash when re-login */
	g_return_if_fail(MSIM_SESSION_VALID(session));

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
        MsimMessage *msg;

#ifdef MSIM_DEBUG_RXBUF
        purple_debug_info("msim", "in loop: buf=<%s>\n", session->rxbuf);
#endif
        *end = 0;
        msg = msim_parse(g_strdup(session->rxbuf));
        if (!msg)
        {
            purple_debug_info("msim", "msim_input_cb: couldn't parse <%s>\n", 
					session->rxbuf);
            purple_connection_error(gc, _("Unparseable message"));
        }
        else
        {
            /* Process message and then free it (processing function should
			 * clone message if it wants to keep it afterwards.) */
            if (!msim_preprocess_incoming(session, msg))
			{
				msim_msg_dump("msim_input_cb: preprocessing message failed on msg: %s\n", msg);
			}
			msim_msg_free(msg);
        }

        /* Move remaining part of buffer to beginning. */
        session->rxoff -= strlen(session->rxbuf) + strlen(MSIM_FINAL_STRING);
        memmove(session->rxbuf, end + strlen(MSIM_FINAL_STRING), 
                MSIM_READ_BUF_SIZE - (end + strlen(MSIM_FINAL_STRING) - session->rxbuf));

        /* Clear end of buffer */
        //memset(end, 0, MSIM_READ_BUF_SIZE - (end - session->rxbuf));
    }
}

/* Setup a callback, to be called when a reply is received with the returned rid.
 *
 * @param cb The callback, an MSIM_USER_LOOKUP_CB.
 * @param data Arbitrary user data to be passed to callback (probably an MsimMessage *).
 *
 * @return The request/reply ID, used to link replies with requests. Put the rid in your request.
 *
 * TODO: Make more generic and more specific:
 * 1) MSIM_USER_LOOKUP_CB - make it for PERSIST_REPLY, not just user lookup
 * 2) data - make it an MsimMessage?
 */
guint msim_new_reply_callback(MsimSession *session, MSIM_USER_LOOKUP_CB cb, gpointer data)
{
	guint rid;

	rid = session->next_rid++;

    g_hash_table_insert(session->user_lookup_cb, GUINT_TO_POINTER(rid), cb);
    g_hash_table_insert(session->user_lookup_cb_data, GUINT_TO_POINTER(rid), data);

	return rid;
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

    gc = (PurpleConnection *)data;
    session = (MsimSession *)gc->proto_data;

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

	/* TODO: Remove. */
    session->user_lookup_cb = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);  /* do NOT free function pointers! (values) */
    session->user_lookup_cb_data = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);/* TODO: we don't know what the values are,
											 they could be integers inside gpointers
											 or strings, so I don't freed them.
											 Figure this out, once free cache. */
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

	/* TODO: Remove. */
	g_hash_table_destroy(session->user_lookup_cb);
	g_hash_table_destroy(session->user_lookup_cb_data);
	g_hash_table_destroy(session->user_lookup_cache);
	
    g_free(session);
}
                 


/** 
 * Close the connection.
 * 
 * @param gc The connection.
 */
void msim_close(PurpleConnection *gc)
{
	MsimSession *session;

	purple_debug_info("msim", "msim_close: destroying session\n");

	session = (MsimSession *)gc->proto_data;

    g_return_if_fail(gc != NULL);
	g_return_if_fail(session != NULL);
	g_return_if_fail(MSIM_SESSION_VALID(session));

   
    purple_input_remove(session->fd);
    msim_session_destroy(session);
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
/* TODO: change to not use callbacks */
void msim_lookup_user(MsimSession *session, const gchar *user, MSIM_USER_LOOKUP_CB cb, gpointer data)
{
    gchar *field_name;
    guint rid, cmd, dsn, lid;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(user != NULL);
    g_return_if_fail(cb != NULL);

    purple_debug_info("msim", "msim_lookup_userid: "
			"asynchronously looking up <%s>\n", user);

	msim_msg_dump("msim_lookup_user: data=%s\n", (MsimMessage *)data);

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

    /* Setup callback. Response will be associated with request using 'rid'. */
    rid = msim_new_reply_callback(session, cb, data);

    /* Send request */

    cmd = MSIM_CMD_GET;

    if (msim_is_userid(user))
    {
        field_name = "UserID";
        dsn = MG_MYSPACE_INFO_BY_ID_DSN; 
        lid = MG_MYSPACE_INFO_BY_ID_LID; 
    } else if (msim_is_email(user)) {
        field_name = "Email";
        dsn = MG_MYSPACE_INFO_BY_STRING_DSN;
        lid = MG_MYSPACE_INFO_BY_STRING_LID;
    } else {
        field_name = "UserName";
        dsn = MG_MYSPACE_INFO_BY_STRING_DSN;
        lid = MG_MYSPACE_INFO_BY_STRING_LID;
    }


	g_return_if_fail(msim_send(session,
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, 1,
			"dsn", MSIM_TYPE_INTEGER, dsn,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"lid", MSIM_TYPE_INTEGER, lid,
			"rid", MSIM_TYPE_INTEGER, rid,
			/* TODO: dictionary field type */
			"body", MSIM_TYPE_STRING, g_strdup_printf("%s=%s", field_name, user),
			NULL));
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

    session = (MsimSession *)buddy->account->gc->proto_data;
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

        session = (MsimSession *)buddy->account->gc->proto_data;

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
                    (gchar *)g_hash_table_lookup(userinfo, "BandName"),
                    (gchar *)g_hash_table_lookup(userinfo, "SongName")));
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
    msim_send_typing,  /* send_typing */
    NULL,              /* get_info */
    NULL,              /* set_away */
    NULL,              /* set_idle */
    NULL,              /* change_passwd */
    msim_add_buddy,    /* add_buddy */
    NULL,              /* add_buddies */
    msim_remove_buddy, /* remove_buddy */
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
    PURPLE_PLUGIN_PROTOCOL,                            /**< type           */
    NULL,                                              /**< ui_requirement */
    0,                                                 /**< flags          */
    NULL,                                              /**< dependencies   */
    PURPLE_PRIORITY_DEFAULT,                           /**< priority       */

    "prpl-myspace",                                   /**< id             */
    "MySpaceIM",                                      /**< name           */
    "0.6",                                            /**< version        */
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

void init_plugin(PurplePlugin *plugin) 
{
	PurpleAccountOption *option;
#ifdef  _TEST_MSIM_MSG
	{
		MsimMessage *msg;

		purple_debug_info("msim", "testing MsimMessage\n");
		msg = msim_msg_new(FALSE);
		msg = msim_msg_append(msg, "bx", MSIM_TYPE_BINARY, g_string_new_len(g_strdup("XXX"), 3));
		msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v1"));
		msg = msim_msg_append(msg, "k1", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(42));
		msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v43"));
		msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v52/xxx\\yyy"));
		msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v7"));
		purple_debug_info("msim", "msg debug str=%s\n", msim_msg_debug_string(msg));
		msim_msg_debug_dump("msg debug str=%s\n", msg);
		purple_debug_info("msim", "msg packed=%s\n", msim_msg_pack(msg));
		purple_debug_info("msim", "msg cloned=%s\n", msim_msg_pack(msim_msg_clone(msg)));
		msim_msg_free(msg);
		exit(0);
	}
#endif

	/* TODO: default to automatically try different ports. Make the user be
	 * able to set the first port to try (like LastConnectedPort in Windows client).  */
	option = purple_account_option_string_new(_("Connect server"), "server", MSIM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Connect port"), "port", MSIM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(myspace, init_plugin, info);
