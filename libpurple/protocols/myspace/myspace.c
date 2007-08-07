/* MySpaceIM Protocol Plugin
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@soc.pidgin.im>
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


/* Loosely based on Miranda plugin by Scott Ellis, formatting.cpp, 
 * https://server.scottellis.com.au/websvn/filedetails.php?repname=Miranda+Plugins&path=%2FMySpace%2Fformatting.cpp&rev=0&sc=0 */

/* The names in in emoticon_names (for <i n=whatever>) map to corresponding 
 * entries in emoticon_symbols (for the ASCII representation of the emoticon).
 */
static const char *emoticon_names[] = {
    "bigsmile", "growl", "growl", "mad", "scared", "scared", "tongue", "tongue",
    "devil", "devil", "happy", "happy", "happi", 
    "messed", "sidefrown", "upset", 
    "frazzled", "heart", "heart", "nerd", "sinister", "wink", "winc",
    "geek", "laugh", "laugh", "oops", "smirk", "worried", "worried",
    "googles", "mohawk", "pirate", "straight", "kiss",
    NULL};

/* Multiple emoticon symbols in Pidgin can map to one name. List the
 * canonical form, as inserted by the "Smile!" dialog, first. For example,
 * :) comes before :-), because although both are recognized as 'happy',
 * the first is inserted by the smiley button. 
 *
 * Note that symbols are case-sensitive in Pidgin -- :-X is not :-x. */
static const char *emoticon_symbols[] = {
    ":D", ">:o", ">:O", ":-[", "=-O", "=-o", ":P",  ":p",
    "O:-)", "o:-)", ":)", ":-)", ":-)",
    "8-)", ":-$", ":-$", 
    ":-/", ";-)", ";)", "8-)" /*:)*/, ":-D", ";-)", ";-)",
    ":-X", ":-D", ":-d", ":'(", "8-)", ":-(", ":(",
    "8-)", ":-X", ":-)", ":-!", ":-*",
    NULL};


/* Internal functions */
static void msim_send_zap(PurpleBlistNode *node, gpointer zap_num_ptr);

#ifdef MSIM_DEBUG_MSG
static void print_hash_item(gpointer key, gpointer value, gpointer user_data);
#endif

static int msim_send_really_raw(PurpleConnection *gc, const char *buf,
        int total_bytes);
static gboolean msim_login_challenge(MsimSession *session, MsimMessage *msg);
static const gchar *msim_compute_login_response(
        const gchar nonce[2 * NONCE_SIZE], const gchar *email, 
        const gchar *password, guint *response_len);
static gboolean msim_send_bm(MsimSession *session, const gchar *who, 
        const gchar *text, int type);

static guint msim_point_to_purple_size(MsimSession *session, guint point);
static guint msim_purple_size_to_point(MsimSession *session, guint size);
static guint msim_height_to_point(MsimSession *session, guint height);
static guint msim_point_to_height(MsimSession *session, guint point);

static void msim_unrecognized(MsimSession *session, MsimMessage *msg, gchar *note);

static void msim_markup_tag_to_html(MsimSession *, xmlnode *root, 
        gchar **begin, gchar **end);
static void html_tag_to_msim_markup(MsimSession *, xmlnode *root, 
        gchar **begin, gchar **end);
static gchar *msim_convert_xml(MsimSession *, const gchar *raw, 
        MSIM_XMLNODE_CONVERT f);
static gchar *msim_convert_smileys_to_markup(gchar *before);

/* High-level msim markup <=> html conversion functions. */
static gchar *msim_markup_to_html(MsimSession *, const gchar *raw);
static gchar *html_to_msim_markup(MsimSession *, const gchar *raw);

static gboolean msim_incoming_bm_record_cv(MsimSession *session, 
        MsimMessage *msg);
static gboolean msim_incoming_bm(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_status(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_im(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_zap(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_action(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_media(MsimSession *session, MsimMessage *msg);
static gboolean msim_incoming_unofficial_client(MsimSession *session, 
        MsimMessage *msg);

#ifdef MSIM_SEND_CLIENT_VERSION
static gboolean msim_send_unofficial_client(MsimSession *session, 
        gchar *username);
#endif

static void msim_get_info_cb(MsimSession *session, MsimMessage *userinfo, gpointer data);

static void msim_set_status_code(MsimSession *session, guint code, 
        gchar *statstring);

static void msim_store_buddy_info_each(gpointer key, gpointer value, 
        gpointer user_data);
static gboolean msim_store_buddy_info(MsimSession *session, MsimMessage *msg);
static gboolean msim_process_server_info(MsimSession *session, 
        MsimMessage *msg);
static gboolean msim_web_challenge(MsimSession *session, MsimMessage *msg); 
static gboolean msim_process_reply(MsimSession *session, MsimMessage *msg);

static gboolean msim_preprocess_incoming(MsimSession *session,MsimMessage *msg);

#ifdef MSIM_USE_KEEPALIVE
static gboolean msim_check_alive(gpointer data);
#endif

static gboolean msim_we_are_logged_on(MsimSession *session, MsimMessage *msg);

static gboolean msim_process(MsimSession *session, MsimMessage *msg);

static MsimMessage *msim_do_postprocessing(MsimMessage *msg, 
        const gchar *uid_field_name, const gchar *uid_before, guint uid);
static void msim_postprocess_outgoing_cb(MsimSession *session, 
        MsimMessage *userinfo, gpointer data);
static gboolean msim_postprocess_outgoing(MsimSession *session, 
        MsimMessage *msg, const gchar *username, const gchar *uid_field_name, 
        const gchar *uid_before); 

static gboolean msim_error(MsimSession *session, MsimMessage *msg);

static void msim_check_inbox_cb(MsimSession *session, MsimMessage *userinfo, 
        gpointer data);
static gboolean msim_check_inbox(gpointer data);

static void msim_input_cb(gpointer gc_uncasted, gint source, 
        PurpleInputCondition cond);

static guint msim_new_reply_callback(MsimSession *session, 
        MSIM_USER_LOOKUP_CB cb, gpointer data);

static void msim_connect_cb(gpointer data, gint source, 
		const gchar *error_message);

static gboolean msim_is_userid(const gchar *user);
static gboolean msim_is_email(const gchar *user);

static void msim_lookup_user(MsimSession *session, const gchar *user, 
		MSIM_USER_LOOKUP_CB cb, gpointer data);

#ifndef round
double round(double round);

/* round is part of C99, but sometimes is unavailable before then.
 * Based on http://forums.belution.com/en/cpp/000/050/13.shtml
 */
double round(double value)
{
    if (value < 0)
        return -(floor(-value + 0.5));
    else
        return   floor( value + 0.5);
}
#endif

/** 
 * Load the plugin.
 */
gboolean 
msim_load(PurplePlugin *plugin)
{
	/* If compiled to use RC4 from libpurple, check if it is really there. */
	if (!purple_ciphers_find_cipher("rc4"))
	{
		purple_debug_error("msim", "rc4 not in libpurple, but it is required - not loading MySpaceIM plugin!\n");
		purple_notify_error(plugin, _("Missing Cipher"), 
				_("The RC4 cipher could not be found"),
				_("Upgrade "
					"to a libpurple with RC4 support (>= 2.0.1). MySpaceIM "
					"plugin will not be loaded."));
		return FALSE;
	}
	return TRUE;
}

/**
 * Get possible user status types. Based on mockprpl.
 *
 * @return GList of status types.
 */
GList *
msim_status_types(PurpleAccount *acct)
{
    GList *types;
    PurpleStatusType *status;

    purple_debug_info("myspace", "returning status types\n");

    types = NULL;

    /* Statuses are almost all the same. Define a macro to reduce code repetition. */
#define _MSIM_ADD_NEW_STATUS(prim) status =                         \
        purple_status_type_new_with_attrs(                          \
        prim,   /* PurpleStatusPrimitive */                         \
        NULL,   /* id - use default */                              \
        NULL,   /* name - use default */                            \
        TRUE,   /* savable */                                       \
        TRUE,   /* user_settable */                                 \
        FALSE,  /* not independent */                               \
                                                                    \
        /* Attributes - each status can have a message. */          \
        "message",                                                  \
        _("Message"),                                               \
        purple_value_new(PURPLE_TYPE_STRING),                       \
        NULL);                                                      \
                                                                    \
                                                                    \
        types = g_list_append(types, status)
        

    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_AWAY);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_OFFLINE);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_INVISIBLE);


    return types;
}

/** Zap someone. Callback from msim_blist_node_menu zap menu. */
static void
msim_send_zap(PurpleBlistNode *node, gpointer zap_num_ptr)
{
    PurpleBuddy *buddy;
    PurpleConnection *gc;
    MsimSession *session;
    gchar *username, *zap_string, *zap_text;
    guint zap;
    const gchar *zap_gerund[10];

    if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
    {
        /* Only know about buddies for now. */
        return;
    }

    zap_gerund[0] = _("Zapping");
    zap_gerund[1] = _("Whacking");
    zap_gerund[2] = _("Torching");
    zap_gerund[3] = _("Smooching");
    zap_gerund[4] = _("Hugging");
    zap_gerund[5] = _("Bslapping");
    zap_gerund[6] = _("Goosing");
    zap_gerund[7] = _("Hi-fiving");
    zap_gerund[8] = _("Punking");
    zap_gerund[9] = _("Raspberry'ing");
 
    g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

    buddy = (PurpleBuddy *)node;
    gc = purple_account_get_connection(buddy->account);
    g_return_if_fail(gc != NULL);

    session = (MsimSession *)(gc->proto_data);
    g_return_if_fail(session != NULL);

    username = buddy->name;
    g_return_if_fail(username != NULL);

    zap = GPOINTER_TO_INT(zap_num_ptr);
    zap_string = g_strdup_printf("!!!ZAP_SEND!!!=RTE_BTN_ZAPS_%d", zap);
    zap_text = g_strdup_printf("*** %s! ***", zap_gerund[zap]);

    serv_got_im(session->gc, username, zap_text, 
			PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_SYSTEM, time(NULL));

	if (!msim_send_bm(session, username, zap_string, MSIM_BM_ACTION))
    {
        purple_debug_info("msim_send_zap", "msim_send_bm failed: zapping %s with %s",
                username, zap_string);
    }

    g_free(zap_string);
    g_free(zap_text);
    return;
}


/** Return menu, if any, for a buddy list node. */
GList *
msim_blist_node_menu(PurpleBlistNode *node)
{
    GList *menu, *zap_menu;
    PurpleMenuAction *act;
    const gchar *zap_names[10];
    guint i;

    if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
    {
        /* Only know about buddies for now. */
        return NULL;
    }

    /* Names from official client. */
    zap_names[0] = _("zap");
    zap_names[1] = _("whack");
    zap_names[2] = _("torch");
    zap_names[3] = _("smooch");
    zap_names[4] = _("hug");
    zap_names[5] = _("bslap");
    zap_names[6] = _("goose");
    zap_names[7] = _("hi-five");
    zap_names[8] = _("punk'd");
    zap_names[9] = _("raspberry");
 
    menu = zap_menu = NULL;

    for (i = 0; i < sizeof(zap_names) / sizeof(zap_names[0]); ++i)
    {
        act = purple_menu_action_new(zap_names[i], PURPLE_CALLBACK(msim_send_zap),
                GUINT_TO_POINTER(i), NULL);
        zap_menu = g_list_append(zap_menu, act);
    }

    act = purple_menu_action_new(_("Zap"), NULL, NULL, zap_menu);
    menu = g_list_append(menu, act);

    return menu;
}

/**
 * Return the icon name for a buddy and account.
 *
 * @param acct The account to find the icon for, or NULL for protocol icon.
 * @param buddy The buddy to find the icon for, or NULL for the account icon.
 *
 * @return The base icon name string.
 */
const gchar *
msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
    /* Use a MySpace icon submitted by hbons at
     * http://developer.pidgin.im/wiki/MySpaceIM. */
    return "myspace";
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
gchar *
str_replace(const gchar *str, const gchar *old, const gchar *new)
{
	gchar **items;
	gchar *ret;

	items = g_strsplit(str, old, -1);
	ret = g_strjoinv(new, items);
	g_free(items);
	return ret;
}

#ifdef MSIM_DEBUG_MSG
static void 
print_hash_item(gpointer key, gpointer value, gpointer user_data)
{
    purple_debug_info("msim", "%s=%s\n",
            key ? (gchar *)key : "(NULL)", 
            value ? (gchar *)value : "(NULL)");
}
#endif

/** 
 * Send raw data (given as a NUL-terminated string) to the server.
 *
 * @param session 
 * @param msg The raw data to send, in a NUL-terminated string.
 *
 * @return TRUE if succeeded, FALSE if not.
 *
 */
gboolean 
msim_send_raw(MsimSession *session, const gchar *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
	
    purple_debug_info("msim", "msim_send_raw: writing <%s>\n", msg);

	return msim_send_really_raw(session->gc, msg, strlen(msg)) ==
		strlen(msg);
}

/** Send raw data to the server, possibly with embedded NULs. 
 *
 * Used in prpl_info struct, so that plugins can have the most possible
 * control of what is sent over the connection. Inside this prpl, 
 * msim_send_raw() is used, since it sends NUL-terminated strings (easier).
 *
 * @param gc PurpleConnection
 * @param buf Buffer to send
 * @param total_bytes Size of buffer to send
 *
 * @return Bytes successfully sent, or -1 on error.
 */
static int 
msim_send_really_raw(PurpleConnection *gc, const char *buf, int total_bytes)
{
	int total_bytes_sent;
    MsimSession *session;

    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(buf != NULL, -1);
    g_return_val_if_fail(total_bytes >= 0, -1);

    session = (MsimSession *)(gc->proto_data);

    g_return_val_if_fail(MSIM_SESSION_VALID(session), -1);
	
	/* Loop until all data is sent, or a failure occurs. */
	total_bytes_sent = 0;
	do
	{
		int bytes_sent;

		bytes_sent = send(session->fd, buf + total_bytes_sent, 
                total_bytes - total_bytes_sent, 0);

		if (bytes_sent < 0)
		{
			purple_debug_info("msim", "msim_send_raw(%s): send() failed: %s\n",
					buf, g_strerror(errno));
			return total_bytes_sent;
		}
		total_bytes_sent += bytes_sent;

	} while(total_bytes_sent < total_bytes);

	return total_bytes_sent;
}


/** 
 * Start logging in to the MSIM servers.
 * 
 * @param acct Account information to use to login.
 */
void 
msim_login(PurpleAccount *acct)
{
    PurpleConnection *gc;
    const gchar *host;
    int port;

    g_return_if_fail(acct != NULL);
    g_return_if_fail(acct->username != NULL);

    purple_debug_info("msim", "logging in %s\n", acct->username);

    gc = purple_account_get_connection(acct);
    gc->proto_data = msim_session_new(acct);
    gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_URLDESC;

    /* Passwords are limited in length. */
	if (strlen(acct->password) > MSIM_MAX_PASSWORD_LENGTH)
	{
		gchar *str;

		str = g_strdup_printf(
				_("Sorry, passwords over %d characters in length (yours is "
				"%d) are not supported by the MySpaceIM plugin."), 
				MSIM_MAX_PASSWORD_LENGTH,
				(int)strlen(acct->password));

		/* Notify an error message also, because this is important! */
		purple_notify_error(acct, g_strdup(_("MySpaceIM Error")), str, NULL);

        purple_connection_error(gc, str);
		
		g_free(str);
	}

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
static gboolean 
msim_login_challenge(MsimSession *session, MsimMessage *msg) 
{
    PurpleAccount *account;
    const gchar *response;
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

    if (nc_len != MSIM_AUTH_CHALLENGE_LENGTH)
    {
        purple_debug_info("msim", "bad nc length: %x != 0x%x\n", nc_len, MSIM_AUTH_CHALLENGE_LENGTH);
        purple_connection_error(session->gc, _("Unexpected challenge length from server"));
        return FALSE;
    }

    purple_connection_update_progress(session->gc, _("Logging in"), 2, 4);

    response_len = 0;
    response = msim_compute_login_response(nc, account->username, account->password, &response_len);

    g_free(nc);

	return msim_send(session, 
			"login2", MSIM_TYPE_INTEGER, MSIM_AUTH_ALGORITHM,
			/* This is actually user's email address. */
			"username", MSIM_TYPE_STRING, g_strdup(account->username),
			/* GString and gchar * response will be freed in msim_msg_free() in msim_send(). */
			"response", MSIM_TYPE_BINARY, g_string_new_len(response, response_len),
			"clientver", MSIM_TYPE_INTEGER, MSIM_CLIENT_VERSION,
            "langid", MSIM_TYPE_INTEGER, MSIM_LANGUAGE_ID_ENGLISH,
            "imlang", MSIM_TYPE_STRING, g_strdup(MSIM_LANGUAGE_NAME_ENGLISH),
			"reconn", MSIM_TYPE_INTEGER, 0,
			"status", MSIM_TYPE_INTEGER, 100,
			"id", MSIM_TYPE_INTEGER, 1,
			NULL);
}

/**
 * Compute the base64'd login challenge response based on username, password, nonce, and IPs.
 *
 * @param nonce The base64 encoded nonce ('nc') field from the server.
 * @param email User's email address (used as login name).
 * @param password User's cleartext password.
 * @param response_len Will be written with response length.
 *
 * @return Binary login challenge response, ready to send to the server. 
 * Must be g_free()'d when finished. NULL if error.
 */
static const gchar *
msim_compute_login_response(const gchar nonce[2 * NONCE_SIZE], 
		const gchar *email, const gchar *password, guint *response_len)
{
    PurpleCipherContext *key_context;
    PurpleCipher *sha1;
	PurpleCipherContext *rc4;

    guchar hash_pw[HASH_SIZE];
    guchar key[HASH_SIZE];
    gchar *password_utf16le, *password_ascii_lc;
    guchar *data;
	guchar *data_out;
	size_t data_len, data_out_len;
	gsize conv_bytes_read, conv_bytes_written;
	GError *conv_error;
#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
	int i;
#endif

    g_return_val_if_fail(nonce != NULL, NULL);
    g_return_val_if_fail(email != NULL, NULL);
    g_return_val_if_fail(password != NULL, NULL);
    g_return_val_if_fail(response_len != NULL, NULL);

    /* Convert password to lowercase (required for passwords containing
     * uppercase characters). MySpace passwords are lowercase,
     * see ticket #2066. */
    password_ascii_lc = g_strdup(password);
    g_strdown(password_ascii_lc);

    /* Convert ASCII password to UTF16 little endian */
    purple_debug_info("msim", "converting password to UTF-16LE\n");
	conv_error = NULL;
	password_utf16le = g_convert(password_ascii_lc, -1, "UTF-16LE", "UTF-8", 
			&conv_bytes_read, &conv_bytes_written, &conv_error);
    g_free(password_ascii_lc);

	g_return_val_if_fail(conv_bytes_read == strlen(password), NULL);

	if (conv_error != NULL)
	{
		purple_debug_error("msim", 
				"g_convert password UTF8->UTF16LE failed: %s",
				conv_error->message);
		g_error_free(conv_error);
        return NULL;
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

	rc4 = purple_cipher_context_new_by_name("rc4", NULL);

    /* Note: 'key' variable is 0x14 bytes (from SHA-1 hash), 
     * but only first 0x10 used for the RC4 key. */
	purple_cipher_context_set_option(rc4, "key_len", (gpointer)0x10);
	purple_cipher_context_set_key(rc4, key);

    /* TODO: obtain IPs of network interfaces */

    /* rc4 encrypt:
     * nonce1+email+IP list */

    data_len = NONCE_SIZE + strlen(email) + MSIM_LOGIN_IP_LIST_LEN;
    data = g_new0(guchar, data_len);
    memcpy(data, nonce, NONCE_SIZE);
    memcpy(data + NONCE_SIZE, email, strlen(email));
    memcpy(data + NONCE_SIZE + strlen(email), MSIM_LOGIN_IP_LIST, MSIM_LOGIN_IP_LIST_LEN);

	data_out = g_new0(guchar, data_len);

    purple_cipher_context_encrypt(rc4, (const guchar *)data, 
			data_len, data_out, &data_out_len);
	purple_cipher_context_destroy(rc4);

	g_assert(data_out_len == data_len);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "response=<%s>\n", data_out);
#endif

	*response_len = data_out_len;

    return (const gchar *)data_out;
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
int 
msim_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
		PurpleMessageFlags flags)
{
    MsimSession *session;
    gchar *message_msim;
    int rc;
    
    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(who != NULL, -1);
    g_return_val_if_fail(message != NULL, -1);

	/* 'flags' has many options, not used here. */

	session = (MsimSession *)gc->proto_data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), -1);

    message_msim = html_to_msim_markup(session, message);

	if (msim_send_bm(session, who, message_msim, MSIM_BM_INSTANT))
	{
		/* Return 1 to have Purple show this IM as being sent, 0 to not. I always
		 * return 1 even if the message could not be sent, since I don't know if
		 * it has failed yet--because the IM is only sent after the userid is
		 * retrieved from the server (which happens after this function returns).
		 */
        /* TODO: maybe if message is delayed, don't echo to conv window,
         * but do echo it to conv window manually once it is actually
         * sent? Would be complicated. */
		rc = 1;
	} else {
		rc = -1;
	}

    g_free(message_msim);

    /*
     * In MySpace, you login with your email address, but don't talk to other
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

    return rc;
}

/** Send a buddy message of a given type.
 *
 * @param session
 * @param who Username to send message to.
 * @param text Message text to send. Not freed; will be copied.
 * @param type A MSIM_BM_* constant.
 *
 * @return TRUE if success, FALSE if fail.
 *
 * Buddy messages ('bm') include instant messages, action messages, status messages, etc.
 *
 */
static gboolean 
msim_send_bm(MsimSession *session, const gchar *who, const gchar *text, 
		int type)
{
	gboolean rc;
	MsimMessage *msg;
    const gchar *from_username;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(who != NULL, FALSE);
    g_return_val_if_fail(text != NULL, FALSE);
   
	from_username = session->account->username;

    g_return_val_if_fail(from_username != NULL, FALSE);

    purple_debug_info("msim", "sending %d message from %s to %s: %s\n",
                  type, from_username, who, text);

	msg = msim_msg_new(TRUE,
			"bm", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(type),
			"sesskey", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(session->sesskey),
			/* 't' will be inserted here */
			"cv", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(MSIM_CLIENT_VERSION),
			"msg", MSIM_TYPE_STRING, g_strdup(text),
			NULL);

	rc = msim_postprocess_outgoing(session, msg, who, "t", "cv");

	msim_msg_free(msg);

	return rc;
}

/* Indexes of this array + 1 map HTML font size to scale of normal font size. *
 * Based on _point_sizes from libpurple/gtkimhtml.c 
 *                                 1    2  3    4     5      6       7 */
static gdouble _font_scale[] = { .85, .95, 1, 1.2, 1.44, 1.728, 2.0736 };

#define MAX_FONT_SIZE                   7       /* Purple maximum font size */
#define POINTS_PER_INCH                 72      /* How many pt's in an inch */

/** Convert typographical font point size to HTML font size. 
 * Based on libpurple/gtkimhtml.c */
static guint
msim_point_to_purple_size(MsimSession *session, guint point)
{
    guint size, this_point, base;
    gdouble scale;
    
    base = purple_account_get_int(session->account, "base_font_size", MSIM_BASE_FONT_POINT_SIZE);
   
    for (size = 0; size < sizeof(_font_scale) / sizeof(_font_scale[0]); ++size)
    {
        scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];
        this_point = (guint)round(scale * base);

        if (this_point >= point)
        {
            purple_debug_info("msim", "msim_point_to_purple_size: %d pt -> size=%d\n",
                    point, size);
            return size;
        }
    }

    /* No HTML font size was this big; return largest possible. */
    return this_point;
}

/** Convert HTML font size to point size. */
static guint
msim_purple_size_to_point(MsimSession *session, guint size)
{
    gdouble scale;
    guint point;
    guint base;

    scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];

    base = purple_account_get_int(session->account, "base_font_size", MSIM_BASE_FONT_POINT_SIZE);

    point = (guint)round(scale * base);

    purple_debug_info("msim", "msim_purple_size_to_point: size=%d -> %d pt\n",
                    size, point);

    return point;
}

/** Convert a msim markup font pixel height to the more usual point size, for incoming messages. */
static guint 
msim_height_to_point(MsimSession *session, guint height)
{
    guint dpi;

    dpi = purple_account_get_int(session->account, "port", MSIM_DEFAULT_DPI);

    return (guint)round((POINTS_PER_INCH * 1. / dpi) * height);

	/* See also: libpurple/protocols/bonjour/jabber.c
	 * _font_size_ichat_to_purple */
}

/** Convert point size to msim pixel height font size specification, for outgoing messages. */
static guint
msim_point_to_height(MsimSession *session, guint point)
{
    guint dpi;

    dpi = purple_account_get_int(session->account, "port", MSIM_DEFAULT_DPI);

    return (guint)round((dpi * 1. / POINTS_PER_INCH) * point);
}

/** Convert the msim markup <f> (font) tag into HTML. */
static void 
msim_markup_f_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *face, *height_str, *decor_str;
	GString *gs_end, *gs_begin;
	guint decor, height;

	face = xmlnode_get_attrib(root, "f");	
	height_str = xmlnode_get_attrib(root, "h");
	decor_str = xmlnode_get_attrib(root, "s");

	if (height_str)
		height = atol(height_str);
	else
		height = 12;

	if (decor_str)
		decor = atol(decor_str);
	else
		decor = 0;

	gs_begin = g_string_new("");
	/* TODO: get font size working */
	if (height && !face)
		g_string_printf(gs_begin, "<font size='%d'>", 
                msim_point_to_purple_size(session, msim_height_to_point(session, height)));
    else if (height && face)
		g_string_printf(gs_begin, "<font face='%s' size='%d'>", face,  
                msim_point_to_purple_size(session, msim_height_to_point(session, height)));
    else
        g_string_printf(gs_begin, "<font>");

	/* No support for font-size CSS? */
	/* g_string_printf(gs_begin, "<span style='font-family: %s; font-size: %dpt'>", face, 
			msim_height_to_point(height)); */

	gs_end = g_string_new("</font>");

	if (decor & MSIM_TEXT_BOLD)
	{
		g_string_append(gs_begin, "<b>");
		g_string_prepend(gs_end, "</b>");
	}

	if (decor & MSIM_TEXT_ITALIC)
	{
		g_string_append(gs_begin, "<i>");
		g_string_append(gs_end, "</i>");	
	}

	if (decor & MSIM_TEXT_UNDERLINE)
	{
		g_string_append(gs_begin, "<u>");
		g_string_append(gs_end, "</u>");	
	}


	*begin = gs_begin->str;
	*end = gs_end->str;
}

/** Convert a msim markup color to a color suitable for libpurple.
  *
  * @param msim Either a color name, or an rgb(x,y,z) code.
  *
  * @return A new string, either a color name or #rrggbb code. Must g_free(). 
  */
static char *
msim_color_to_purple(const char *msim)
{
	guint red, green, blue;

	if (!msim)
		return g_strdup("black");

	if (sscanf(msim, "rgb(%d,%d,%d)", &red, &green, &blue) != 3)
	{
		/* Color name. */
		return g_strdup(msim);
	}
	/* TODO: rgba (alpha). */

	return g_strdup_printf("#%.2x%.2x%.2x", red, green, blue);
}	

/** Convert the msim markup <p> (paragraph) tag into HTML. */
static void 
msim_markup_p_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
    /* Just pass through unchanged. 
	 *
	 * Note: attributes currently aren't passed, if there are any. */
	*begin = g_strdup("<p>");
	*end = g_strdup("</p>");
}

/** Convert the msim markup <c> tag (text color) into HTML. TODO: Test */
static void 
msim_markup_c_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color)
	{
		purple_debug_info("msim", "msim_markup_c_to_html: <c> tag w/o v attr");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

	purple_color = msim_color_to_purple(color);

	*begin = g_strdup_printf("<font color='%s'>", purple_color); 

	g_free(purple_color);

	/* *begin = g_strdup_printf("<span style='color: %s'>", color); */
	*end = g_strdup("</font>");
}

/** Convert the msim markup <b> tag (background color) into HTML. TODO: Test */
static void 
msim_markup_b_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color)
	{
		*begin = g_strdup("");
		*end = g_strdup("");
		purple_debug_info("msim", "msim_markup_b_to_html: <b> w/o v attr");
		/* TODO: log as unrecognized. */
		return;
	}

	purple_color = msim_color_to_purple(color);

	/* TODO: find out how to set background color. */
	*begin = g_strdup_printf("<span style='background-color: %s'>", 
			purple_color);
	g_free(purple_color);

	*end = g_strdup("</p>");
}

/** Convert the msim markup <i> tag (emoticon image) into HTML. */
static void 
msim_markup_i_to_html(MsimSession *session, xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *name;
    guint i;
       
	name = xmlnode_get_attrib(root, "n");
	if (!name)
	{
		purple_debug_info("msim", "msim_markup_i_to_html: <i> w/o n");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

    for (i = 0; emoticon_names[i] != NULL; ++i)
    {
        if (!strcmp(name, emoticon_names[i]))
        {
            *begin = g_strdup(emoticon_symbols[i]);
            *end = g_strdup("");
            return;
        }
    }

    *begin = g_strdup(name);
    *end = g_strdup("");
}

/** Convert an individual msim markup tag to HTML. */
static void 
msim_markup_tag_to_html(MsimSession *session, xmlnode *root, gchar **begin, 
        gchar **end)
{
	if (!strcmp(root->name, "f"))
	{
		msim_markup_f_to_html(session, root, begin, end);
	} else if (!strcmp(root->name, "p")) {
		msim_markup_p_to_html(session, root, begin, end);
	} else if (!strcmp(root->name, "c")) {
		msim_markup_c_to_html(session, root, begin, end);
	} else if (!strcmp(root->name, "b")) {
		msim_markup_b_to_html(session, root, begin, end);
	} else if (!strcmp(root->name, "i")) {
		msim_markup_i_to_html(session, root, begin, end);
	} else {
		purple_debug_info("msim", "msim_markup_tag_to_html: "
				"unknown tag name=%s, ignoring", 
                (root && root->name) ? root->name : "(NULL)");
		*begin = g_strdup("");
		*end = g_strdup("");
	}
}

/** Convert an individual HTML tag to msim markup. */
static void 
html_tag_to_msim_markup(MsimSession *session, xmlnode *root, gchar **begin, 
        gchar **end)
{
    /* TODO: Coalesce nested tags into one <f> tag!
     * Currently, the 's' value will be overwritten when b/i/u is nested
     * within another one, and only the inner-most formatting will be 
     * applied to the text. */
    if (!strcmp(root->name, "root"))
    {
        *begin = g_strdup("");
        *end = g_strdup("");
    } else if (!strcmp(root->name, "b")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_BOLD);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "i")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_ITALIC);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "u")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_UNDERLINE);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "font")) {
        const gchar *size;
        const gchar *face;

        size = xmlnode_get_attrib(root, "size");
        face = xmlnode_get_attrib(root, "face");

        if (face && size)
        {
            *begin = g_strdup_printf("<f f='%s' h='%d'>", face, 
                    msim_point_to_height(session,
                        msim_purple_size_to_point(session, atoi(size))));
        } else if (face) {
            *begin = g_strdup_printf("<f f='%s'>", face);
        } else if (size) {
            *begin = g_strdup_printf("<f h='%d'>", 
                     msim_point_to_height(session,
                         msim_purple_size_to_point(session, atoi(size))));
        } else {
            *begin = g_strdup("<f>");
        }

        *end = g_strdup("</f>");

        /* TODO: color (bg uses <body>), emoticons */
    } else {
        *begin = g_strdup_printf("[%s]", root->name);
        *end = g_strdup_printf("[/%s]", root->name);
    }
}

/** Convert an xmlnode of msim markup or HTML to an HTML string or msim markup.
 *
 * @param f Function to convert tags.
 *
 * @return An HTML string. Caller frees.
 */
static gchar *
msim_convert_xmlnode(MsimSession *session, xmlnode *root, MSIM_XMLNODE_CONVERT f)
{
	xmlnode *node;
	gchar *begin, *inner, *end;
    GString *final;

	if (!root || !root->name)
		return g_strdup("");

	purple_debug_info("msim", "msim_convert_xmlnode: got root=%s\n",
			root->name);

	begin = inner = end = NULL;

    final = g_string_new("");

    f(session, root, &begin, &end);
    
    g_string_append(final, begin);

	/* Loop over all child nodes. */
 	for (node = root->child; node != NULL; node = node->next)
	{
		switch (node->type)
		{
		case XMLNODE_TYPE_ATTRIB:
			/* Attributes handled above. */
			break;

		case XMLNODE_TYPE_TAG:
			/* A tag or tag with attributes. Recursively descend. */
			inner = msim_convert_xmlnode(session, node, f);
            g_return_val_if_fail(inner != NULL, NULL);

			purple_debug_info("msim", " ** node name=%s\n", 
                    (node && node->name) ? node->name : "(NULL)");
			break;
	
		case XMLNODE_TYPE_DATA:	
			/* Literal text. */
			inner = g_new0(char, node->data_sz + 1);
			strncpy(inner, node->data, node->data_sz);
			inner[node->data_sz] = 0;

			purple_debug_info("msim", " ** node data=%s\n", 
                    inner ? inner : "(NULL)");
			break;
			
		default:
			purple_debug_info("msim",
					"msim_convert_xmlnode: strange node\n");
			inner = g_strdup("");
		}

        if (inner)
            g_string_append(final, inner);
    }

    /* TODO: Note that msim counts each piece of text enclosed by <f> as
     * a paragraph and will display each on its own line. You actually have
     * to _nest_ <f> tags to intersperse different text in one paragraph!
     * Comment out this line below to see. */
    g_string_append(final, end);

	purple_debug_info("msim", "msim_markup_xmlnode_to_gtkhtml: RETURNING %s\n",
			(final && final->str) ? final->str : "(NULL)");

	return final->str;
}

/** Convert XML to something based on MSIM_XMLNODE_CONVERT. */
static gchar *
msim_convert_xml(MsimSession *session, const gchar *raw, MSIM_XMLNODE_CONVERT f)
{
	xmlnode *root;
	gchar *str;
    gchar *enclosed_raw;

    g_return_val_if_fail(raw != NULL, NULL);

    /* Enclose text in one root tag, to try to make it valid XML for parsing. */
    enclosed_raw = g_strconcat("<root>", raw, "</root>", NULL);

	root = xmlnode_from_str(enclosed_raw, -1);

	if (!root)
	{
		purple_debug_info("msim", "msim_markup_to_html: couldn't parse "
				"%s as XML, returning raw: %s\n", enclosed_raw, raw);
        /* TODO: msim_unrecognized */
        g_free(enclosed_raw);
		return g_strdup(raw);
	}

    g_free(enclosed_raw);

	str = msim_convert_xmlnode(session, root, f);
    g_return_val_if_fail(str != NULL, NULL);
	purple_debug_info("msim", "msim_markup_to_html: returning %s\n", str);

	xmlnode_free(root);

	return str;
}

/** Convert plaintext smileys to <i> markup tags.
 *
 * @param before Original text with ASCII smileys. Will be freed.
 * @return A new string with <i> tags, if applicable. Must be g_free()'d.
 */
static gchar *
msim_convert_smileys_to_markup(gchar *before)
{
    gchar *old, *new, *replacement;
    guint i;

    old = before;
    new = NULL;

    for (i = 0; emoticon_symbols[i] != NULL; ++i)
    {

        replacement = g_strdup_printf("<i n=\"%s\"/>", emoticon_names[i]);

        purple_debug_info("msim", "msim_convert_smileys_to_markup: %s->%s\n",
                emoticon_symbols[i] ? emoticon_symbols[i] : "(NULL)", 
                replacement ? replacement : "(NULL)");
        new = str_replace(old, emoticon_symbols[i], replacement);
        
        g_free(replacement);
        g_free(old);

        old = new;
    }

    return new;
}
    

/** High-level function to convert MySpaceIM markup to Purple (HTML) markup. 
 *
 * @return Purple markup string, must be g_free()'d. */
static gchar *
msim_markup_to_html(MsimSession *session, const gchar *raw)
{
    return msim_convert_xml(session, raw, 
            (MSIM_XMLNODE_CONVERT)(msim_markup_tag_to_html));
}

/** High-level function to convert Purple (HTML) to MySpaceIM markup.
 *
 * @return HTML markup string, must be g_free()'d. */
static gchar *
html_to_msim_markup(MsimSession *session, const gchar *raw)
{
    gchar *markup;

    markup = msim_convert_xml(session, raw,
            (MSIM_XMLNODE_CONVERT)(html_tag_to_msim_markup));
    
    if (purple_account_get_bool(session->account, "emoticons", TRUE))
    {
        /* Frees markup and allocates a new one. */
        markup = msim_convert_smileys_to_markup(markup);
    }

    return markup;
}

/** Record the client version in the buddy list, from an incoming message. */
static gboolean
msim_incoming_bm_record_cv(MsimSession *session, MsimMessage *msg)
{
    gchar *username, *cv;
    gboolean ret;
    PurpleBuddy *buddy;

    username = msim_msg_get_string(msg, "_username");
    cv = msim_msg_get_string(msg, "cv");

    g_return_val_if_fail(username != NULL, FALSE);
    g_return_val_if_fail(cv != NULL, FALSE);

    buddy = purple_find_buddy(session->account, username);

    if (buddy)
    {
        purple_blist_node_set_int(&buddy->node, "client_cv", atol(cv));
        ret = TRUE;
    } else {
        ret = FALSE;
    }

    g_free(username);
    g_free(cv);

    return ret;
}

/** Handle an incoming buddy message. */
static gboolean
msim_incoming_bm(MsimSession *session, MsimMessage *msg)
{
    guint bm;
   
    bm = msim_msg_get_integer(msg, "bm");

    msim_incoming_bm_record_cv(session, msg);

    switch (bm)
    {
        case MSIM_BM_STATUS:
            return msim_incoming_status(session, msg);
        case MSIM_BM_INSTANT:
            return msim_incoming_im(session, msg);
        case MSIM_BM_ACTION:
            return msim_incoming_action(session, msg);
        case MSIM_BM_MEDIA:
            return msim_incoming_media(session, msg);
        case MSIM_BM_UNOFFICIAL_CLIENT:
            return msim_incoming_unofficial_client(session, msg);
        default:
            /* Not really an IM, but show it for informational 
             * purposes during development. */
            return msim_incoming_im(session, msg);
    }
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
static gboolean 
msim_incoming_im(MsimSession *session, MsimMessage *msg)
{
    gchar *username, *msg_msim_markup, *msg_purple_markup;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    username = msim_msg_get_string(msg, "_username");
    g_return_val_if_fail(username != NULL, FALSE);

	msg_msim_markup = msim_msg_get_string(msg, "msg");
    g_return_val_if_fail(msg_msim_markup != NULL, FALSE);

	msg_purple_markup = msim_markup_to_html(session, msg_msim_markup);
	g_free(msg_msim_markup);

    serv_got_im(session->gc, username, msg_purple_markup, 
			PURPLE_MESSAGE_RECV, time(NULL));

	g_free(username);
	g_free(msg_purple_markup);

	return TRUE;
}

/**
 * Process unrecognized information.
 *
 * @param session
 * @param msg An MsimMessage that was unrecognized, or NULL.
 * @param note Information on what was unrecognized, or NULL.
 */
static void 
msim_unrecognized(MsimSession *session, MsimMessage *msg, gchar *note)
{
	/* TODO: Some more context, outwardly equivalent to a backtrace, 
     * for helping figure out what this msg is for. What was going on?
     * But not too much information so that a user
	 * posting this dump reveals confidential information.
	 */

	/* TODO: dump unknown msgs to file, so user can send them to me
	 * if they wish, to help add support for new messages (inspired
	 * by Alexandr Shutko, who maintains OSCAR protocol documentation). */

	purple_debug_info("msim", "Unrecognized data on account for %s\n", 
            session->account->username ? session->account->username
            : "(NULL)");
	if (note)
	{
		purple_debug_info("msim", "(Note: %s)\n", note);
	}

    if (msg)
    {
        msim_msg_dump("Unrecognized message dump: %s\n", msg);
    }
}

/** Process an incoming zap. */
static gboolean
msim_incoming_zap(MsimSession *session, MsimMessage *msg)
{
    gchar *msg_text, *username, *zap_text;
    gint zap;
    const gchar *zap_past_tense[10];

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

    zap_text = g_strdup_printf(_("*** You have been %s! ***"), zap_past_tense[zap]);

    serv_got_im(session->gc, username, zap_text, 
			PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_SYSTEM, time(NULL));

    g_free(zap_text);
    g_free(msg_text);
    g_free(username);

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
 */
static gboolean 
msim_incoming_action(MsimSession *session, MsimMessage *msg)
{
	gchar *msg_text, *username;
	gboolean rc;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

	msg_text = msim_msg_get_string(msg, "msg");
    g_return_val_if_fail(msg_text != NULL, FALSE);

	username = msim_msg_get_string(msg, "_username");
    g_return_val_if_fail(username != NULL, FALSE);

	purple_debug_info("msim", "msim_incoming_action: action <%s> from <%d>\n", 
            msg_text, username);

	if (strcmp(msg_text, "%typing%") == 0)
	{
		/* TODO: find out if msim repeatedly sends typing messages, so we can 
         * give it a timeout. Right now, there does seem to be an inordinately 
         * amount of time between typing stopped-typing notifications. */
		serv_got_typing(session->gc, username, 0, PURPLE_TYPING);
		rc = TRUE;
	} else if (strcmp(msg_text, "%stoptyping%") == 0) {
		serv_got_typing_stopped(session->gc, username);
		rc = TRUE;
    } else if (strstr(msg_text, "!!!ZAP_SEND!!!=RTE_BTN_ZAPS_")) {
        rc = msim_incoming_zap(session, msg);
	} else {
		msim_unrecognized(session, msg, 
                "got to msim_incoming_action but unrecognized value for 'msg'");
		rc = FALSE;
	}

	g_free(msg_text);
	g_free(username);

	return rc;
}

/* Process an incoming media (buddy icon) message. */
static gboolean
msim_incoming_media(MsimSession *session, MsimMessage *msg)
{
    gchar *username, *text;

    username = msim_msg_get_string(msg, "_username");
    text = msim_msg_get_string(msg, "msg");

    g_return_val_if_fail(username != NULL, FALSE);
    g_return_val_if_fail(text != NULL, FALSE);

    purple_debug_info("msim", "msim_incoming_media: from %s, got msg=%s\n", username, text);

    /* Media messages are sent when the user opens a window to someone.
     * Tell libpurple they started typing and stopped typing, to inform the Psychic
     * Mode plugin so it too can open a window to the user. */
    serv_got_typing(session->gc, username, 0, PURPLE_TYPING);
    serv_got_typing_stopped(session->gc, username);

    g_free(username);

    return TRUE;
}

/* Process an incoming "unofficial client" message. The plugin for
 * Miranda IM sends this message with the plugin information. */
static gboolean
msim_incoming_unofficial_client(MsimSession *session, MsimMessage *msg)
{
    PurpleBuddy *buddy;
    gchar *username, *client_info;

    username = msim_msg_get_string(msg, "_username");
    client_info = msim_msg_get_string(msg, "msg");

    g_return_val_if_fail(username != NULL, FALSE);
    g_return_val_if_fail(client_info != NULL, FALSE);

    purple_debug_info("msim", "msim_incoming_unofficial_client: %s is using client %s\n",
        username, client_info);

	buddy = purple_find_buddy(session->account, username);
    
    g_return_val_if_fail(buddy != NULL, FALSE);

    purple_blist_node_remove_setting(&buddy->node, "client");
    purple_blist_node_set_string(&buddy->node, "client", client_info);


    g_free(username);
    /* Do not free client_info - the blist now owns it. */

    return TRUE;
}


#ifdef MSIM_SEND_CLIENT_VERSION
/** Send our client version to another unofficial client that understands it. */
static gboolean
msim_send_unofficial_client(MsimSession *session, gchar *username)
{
    gchar *our_info;
    gboolean ret;

    our_info = g_strdup_printf("Libpurple %d.%d.%d - msimprpl %s", 
            PURPLE_MAJOR_VERSION,
            PURPLE_MINOR_VERSION,
            PURPLE_MICRO_VERSION,
            MSIM_PRPL_VERSION_STRING);

	ret = msim_send_bm(session, username, our_info, MSIM_BM_UNOFFICIAL_CLIENT);

    return ret;
}
#endif

/** 
 * Handle when our user starts or stops typing to another user.
 *
 * @param gc
 * @param name The buddy name to which our user is typing to
 * @param state PURPLE_TYPING, PURPLE_TYPED, PURPLE_NOT_TYPING
 *
 * @return 0
 */
unsigned int 
msim_send_typing(PurpleConnection *gc, const gchar *name, 
        PurpleTypingState state)
{
	const gchar *typing_str;
	MsimSession *session;

    g_return_val_if_fail(gc != NULL, 0);
    g_return_val_if_fail(name != NULL, 0);

	session = (MsimSession *)gc->proto_data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);

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
	msim_send_bm(session, name, typing_str, MSIM_BM_ACTION);
	return 0;
}

/** Callback for msim_get_info(), for when user info is received. */
static void 
msim_get_info_cb(MsimSession *session, MsimMessage *user_info_msg, 
        gpointer data)
{
	GHashTable *body;
	gchar *body_str;
	MsimMessage *msg;
	gchar *user;
	PurpleNotifyUserInfo *user_info;
	PurpleBuddy *buddy;
	const gchar *str, *str2;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	/* Get user{name,id} from msim_get_info, passed as an MsimMessage for 
	   orthogonality. */
	msg = (MsimMessage *)data;
    g_return_if_fail(msg != NULL);

	user = msim_msg_get_string(msg, "user");
	if (!user)
	{
		purple_debug_info("msim", "msim_get_info_cb: no 'user' in msg");
		return;
	}

	msim_msg_free(msg);
	purple_debug_info("msim", "msim_get_info_cb: got for user: %s\n", user);

	body_str = msim_msg_get_string(user_info_msg, "body");
    g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_free(body_str);

	buddy = purple_find_buddy(session->account, user);
	/* Note: don't assume buddy is non-NULL; will be if lookup random user 
     * not on blist. */

	user_info = purple_notify_user_info_new();

	/* Identification */
	purple_notify_user_info_add_pair(user_info, _("User"), user);

	/* note: g_hash_table_lookup does not create a new string! */
	str = g_hash_table_lookup(body, "UserID");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("User ID"), 
				g_strdup(str));

	/* a/s/l...the vitals */	
	str = g_hash_table_lookup(body, "Age");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Age"), g_strdup(str));

	str = g_hash_table_lookup(body, "Gender");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Gender"), g_strdup(str));

	str = g_hash_table_lookup(body, "Location");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Location"), 
				g_strdup(str));

	/* Other information */

	if (buddy)
	{
        /* Headline comes from buddy status messages */
		str = purple_blist_node_get_string(&buddy->node, "Headline");
		if (str)
			purple_notify_user_info_add_pair(user_info, "Headline", str);
	}


	str = g_hash_table_lookup(body, "BandName");
	str2 = g_hash_table_lookup(body, "SongName");
	if (str || str2)
	{
		purple_notify_user_info_add_pair(user_info, _("Song"), 
			g_strdup_printf("%s - %s",
				str ? str : "Unknown Artist",
				str2 ? str2 : "Unknown Song"));
	}


	/* Total friends only available if looked up by uid, not username. */
	str = g_hash_table_lookup(body, "TotalFriends");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Total Friends"), 
			g_strdup(str));

    if (buddy)
    {
        gint cv;

        str = purple_blist_node_get_string(&buddy->node, "client");
        cv = purple_blist_node_get_int(&buddy->node, "client_cv");

        if (str)
        {
            purple_notify_user_info_add_pair(user_info, _("Client Version"),
                    g_strdup_printf("%s (build %d)", str, cv));
        }
    }

	purple_notify_userinfo(session->gc, user, user_info, NULL, NULL);
	purple_debug_info("msim", "msim_get_info_cb: username=%s\n", user);
	//purple_notify_user_info_destroy(user_info);
	/* Do not free username, since it will be used by user_info. */

	g_hash_table_destroy(body);
}

/** Retrieve a user's profile. */
void 
msim_get_info(PurpleConnection *gc, const gchar *user)
{
	PurpleBuddy *buddy;
	MsimSession *session;
	guint uid;
	gchar *user_to_lookup;
	MsimMessage *user_msg;

    g_return_if_fail(gc != NULL);
    g_return_if_fail(user != NULL);

	session = (MsimSession *)gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	/* Obtain uid of buddy. */
	buddy = purple_find_buddy(session->account, user);
	if (buddy)
	{
		uid = purple_blist_node_get_int(&buddy->node, "UserID");
		if (!uid)
		{
			PurpleNotifyUserInfo *user_info;

			user_info = purple_notify_user_info_new();
			purple_notify_user_info_add_pair(user_info, NULL,
					_("This buddy appears to not have a userid stored in the buddy list, can't look up. Is the user really on the buddy list?"));

			purple_notify_userinfo(session->gc, user, user_info, NULL, NULL);
			purple_notify_user_info_destroy(user_info);
			return;
		}

		user_to_lookup = g_strdup_printf("%d", uid);
	} else {

		/* Looking up buddy not on blist. Lookup by whatever user entered. */
		user_to_lookup = g_strdup(user);
	}

	/* Pass the username to msim_get_info_cb(), because since we lookup
	 * by userid, the userinfo message will only contain the uid (not 
	 * the username).
	 */
	user_msg = msim_msg_new(TRUE, 
			"user", MSIM_TYPE_STRING, g_strdup(user),
			NULL);
	purple_debug_info("msim", "msim_get_info, setting up lookup, user=%s\n", user);

	msim_lookup_user(session, user_to_lookup, msim_get_info_cb, user_msg);

	g_free(user_to_lookup); 
}

/** Set your status - callback for when user manually sets it.  */
void
msim_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleStatusType *type;
	MsimSession *session;
    guint status_code;
    const gchar *statstring;

	session = (MsimSession *)account->gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	type = purple_status_get_type(status);

	switch (purple_status_type_get_primitive(type))
	{
		case PURPLE_STATUS_AVAILABLE:
            purple_debug_info("msim", "msim_set_status: available (%d->%d)\n", PURPLE_STATUS_AVAILABLE,
                    MSIM_STATUS_CODE_ONLINE);
			status_code = MSIM_STATUS_CODE_ONLINE;
			break;

		case PURPLE_STATUS_INVISIBLE:
            purple_debug_info("msim", "msim_set_status: invisible (%d->%d)\n", PURPLE_STATUS_INVISIBLE,
                    MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN);
			status_code = MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN;
			break;

		case PURPLE_STATUS_AWAY:
            purple_debug_info("msim", "msim_set_status: away (%d->%d)\n", PURPLE_STATUS_AWAY,
                    MSIM_STATUS_CODE_AWAY);
			status_code = MSIM_STATUS_CODE_AWAY;
			break;

		default:
			purple_debug_info("msim", "msim_set_status: unknown "
					"status interpreting as online");
			status_code = MSIM_STATUS_CODE_ONLINE;
			break;
	}

    statstring = purple_status_get_attr_string(status, "message");

    if (!statstring)
        statstring = g_strdup("");

    msim_set_status_code(session, status_code, g_strdup(statstring));
}

/** Go idle. */
void
msim_set_idle(PurpleConnection *gc, int time)
{
    MsimSession *session;

    g_return_if_fail(gc != NULL);

    session = (MsimSession *)gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

    if (time == 0)
    {
        /* Going back from idle. In msim, idle is mutually exclusive 
         * from the other states (you can only be away or idle, but not
         * both, for example), so by going non-idle I go online.
         */
        /* TODO: find out how to keep old status string? */
        msim_set_status_code(session, MSIM_STATUS_CODE_ONLINE, g_strdup(""));
    } else {
        /* msim doesn't support idle time, so just go idle */
        msim_set_status_code(session, MSIM_STATUS_CODE_IDLE, g_strdup(""));
    }
}

/** Set status using an MSIM_STATUS_CODE_* value.
 * @param status_code An MSIM_STATUS_CODE_* value.
 * @param statstring Status string, must be a dynamic string (will be freed by msim_send).
 */
static void 
msim_set_status_code(MsimSession *session, guint status_code, gchar *statstring)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(statstring != NULL);

    purple_debug_info("msim", "msim_set_status_code: going to set status to code=%d,str=%s\n",
            status_code, statstring);

	if (!msim_send(session,
			"status", MSIM_TYPE_INTEGER, status_code,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"statstring", MSIM_TYPE_STRING, statstring, 
			"locstring", MSIM_TYPE_STRING, g_strdup(""),
            NULL))
	{
		purple_debug_info("msim", "msim_set_status: failed to set status");
	}

}

/** After a uid is resolved to username, tag it with the username and submit for processing. 
 * 
 * @param session
 * @param userinfo Response messsage to resolving request.
 * @param data MsimMessage *, the message to attach information to. 
 */
static void 
msim_incoming_resolved(MsimSession *session, MsimMessage *userinfo, 
		gpointer data)
{
	gchar *body_str;
	GHashTable *body;
	gchar *username;
	MsimMessage *msg;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

	body_str = msim_msg_get_string(userinfo, "body");
	g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_return_if_fail(body != NULL);
	g_free(body_str);

	username = g_hash_table_lookup(body, "UserName");
	g_return_if_fail(username != NULL);


	msg = (MsimMessage *)data;
    g_return_if_fail(msg != NULL);

    /* TODO: more elegant solution than below. attach whole message? */
	/* Special elements name beginning with '_', we'll use internally within the
	 * program (did not come directly from the wire). */
	msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));
  
    /* TODO: attach more useful information, like ImageURL */

	msim_process(session, msg);

	/* TODO: Free copy cloned from  msim_preprocess_incoming(). */
	//XXX msim_msg_free(msg);
	g_hash_table_destroy(body);
}

#if 0
/* Lookup a username by userid, from buddy list. 
 *
 * @param wanted_uid
 *
 * @return Username of wanted_uid, if on blist, or NULL. Static string. 
 *
 * XXX WARNING: UNKNOWN MEMORY CORRUPTION HERE! 
 */
static const gchar *
msim_uid2username_from_blist(MsimSession *session, guint wanted_uid)
{
	GSList *buddies, *cur;

	buddies = purple_find_buddies(session->account, NULL); 

	if (!buddies)
	{
		purple_debug_info("msim", "msim_uid2username_from_blist: no buddies?");
		return NULL;
	}

	for (cur = buddies; cur != NULL; cur = g_slist_next(cur))
	{
		PurpleBuddy *buddy;
		//PurpleBlistNode *node;
		guint uid;
		const gchar *name;


		/* See finch/gnthistory.c */
		buddy = cur->data;
		//node  = cur->data;

		uid = purple_blist_node_get_int(&buddy->node, "UserID");
		//uid = purple_blist_node_get_int(node, "UserID");

		/* name = buddy->name; */								/* crash */
		/* name = PURPLE_BLIST_NODE_NAME(&buddy->node);  */		/* crash */

		/* XXX Is this right? Memory corruption here somehow. Happens only
		 * when return one of these values. */
		name = purple_buddy_get_name(buddy); 					/* crash */
		//name = purple_buddy_get_name((PurpleBuddy *)node); 	/* crash */
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
			gchar *ret;

			ret = g_strdup(name);

			g_slist_free(buddies);

			return ret;
		}
	}

	g_slist_free(buddies);
	return NULL;
}
#endif

/** Preprocess incoming messages, resolving as needed, calling msim_process() when ready to process.
 *
 * @param session
 * @param msg MsimMessage *, freed by caller.
 */
static gboolean 
msim_preprocess_incoming(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

	if (msim_msg_get(msg, "bm") && msim_msg_get(msg, "f"))
	{
		guint uid;
		const gchar *username;

		/* 'f' = userid message is from, in buddy messages */
		uid = msim_msg_get_integer(msg, "f");

		/* TODO: Make caching work. Currently it is commented out because
		 * it crashes for unknown reasons, memory realloc error. */
#if 0
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
			gchar *from;

			/* Send lookup request. */
			/* XXX: where is msim_msg_get_string() freed? make _strdup and _nonstrdup. */
			purple_debug_info("msim", "msim_incoming: sending lookup, setting up callback\n");
			from = msim_msg_get_string(msg, "f");
			msim_lookup_user(session, from, msim_incoming_resolved, msim_msg_clone(msg)); 
			g_free(from);

			/* indeterminate */
			return TRUE;
		}
	} else {
		/* Nothing to resolve - send directly to processing. */
		return msim_process(session, msg);
	}
}

#ifdef MSIM_USE_KEEPALIVE
/** Check if the connection is still alive, based on last communication. */
static gboolean
msim_check_alive(gpointer data)
{
    MsimSession *session;
    time_t delta;
    gchar *errmsg;

    session = (MsimSession *)data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);

    delta = time(NULL) - session->last_comm;
    //purple_debug_info("msim", "msim_check_alive: delta=%d\n", delta);
    if (delta >= MSIM_KEEPALIVE_INTERVAL)
    {
        errmsg = g_strdup_printf(_("Connection to server lost (no data received within %d seconds)"), (int)delta);

        purple_debug_info("msim", "msim_check_alive: %s > interval of %d, presumed dead\n",
                errmsg, MSIM_KEEPALIVE_INTERVAL);
        purple_connection_error(session->gc, errmsg);

        purple_notify_error(session->gc, NULL, errmsg, NULL);

        g_free(errmsg);

        return FALSE;
    }

    return TRUE;
}
#endif

/** Handle mail reply checks. */
static void
msim_check_inbox_cb(MsimSession *session, MsimMessage *reply, gpointer data)
{
    GHashTable *body;
    gchar *body_str;
    GString *notification;
    guint old_inbox_status;
    guint i, n;
    const gchar *froms[5], *tos[5], *urls[5], *subjects[5];

    /* Three parallel arrays for each new inbox message type. */
    static const gchar *inbox_keys[] = 
    { 
        "Mail", 
        "BlogComment", 
        "ProfileComment", 
        "FriendRequest", 
        "PictureComment" 
    };

    static const guint inbox_bits[] = 
    { 
        MSIM_INBOX_MAIL, 
        MSIM_INBOX_BLOG_COMMENT,
        MSIM_INBOX_PROFILE_COMMENT,
        MSIM_INBOX_FRIEND_REQUEST,
        MSIM_INBOX_PICTURE_COMMENT
    };

    static const gchar *inbox_urls[] =
    {
        "http://messaging.myspace.com/index.cfm?fuseaction=mail.inbox",
        "http://blog.myspace.com/index.cfm?fuseaction=blog",
        "http://home.myspace.com/index.cfm?fuseaction=user",
        "http://messaging.myspace.com/index.cfm?fuseaction=mail.friendRequests",
        "http://home.myspace.com/index.cfm?fuseaction=user"
    };

    static const gchar *inbox_text[5];

    /* Can't write _()'d strings in array initializers. Workaround. */
    inbox_text[0] = _("New mail messages");
    inbox_text[1] = _("New blog comments");
    inbox_text[2] = _("New profile comments");
    inbox_text[3] = _("New friend requests!");
    inbox_text[4] = _("New picture comments");

    g_return_if_fail(reply != NULL);

    msim_msg_dump("msim_check_inbox_cb: reply=%s\n", reply);

    body_str = msim_msg_get_string(reply, "body");
    g_return_if_fail(body_str != NULL);

    body = msim_parse_body(body_str);
    g_free(body_str);

    notification = g_string_new("");

    old_inbox_status = session->inbox_status;

    n = 0;

    for (i = 0; i < sizeof(inbox_keys) / sizeof(inbox_keys[0]); ++i)
    {
        const gchar *key;
        guint bit;
        
        key = inbox_keys[i];
        bit = inbox_bits[i];

        if (g_hash_table_lookup(body, key))
        {
            /* Notify only on when _changes_ from no mail -> has mail
             * (edge triggered) */
            if (!(session->inbox_status & bit))
            {
                purple_debug_info("msim", "msim_check_inbox_cb: got %s, at %d\n",
                        key ? key : "(NULL)", n);

                subjects[n] = inbox_text[i];
                froms[n] = _("MySpace");
                tos[n] = session->username;
                /* TODO: append token, web challenge, so automatically logs in.
                 * Would also need to free strings because they won't be static
                 */
                urls[n] = inbox_urls[i];

                ++n;
            } else {
                purple_debug_info("msim",
                        "msim_check_inbox_cb: already notified of %s\n",
                        key ? key : "(NULL)");
            }

            session->inbox_status |= bit;
        }
    }

    if (n)
    {
        purple_debug_info("msim",
                "msim_check_inbox_cb: notifying of %d\n", n);

        /* TODO: free strings with callback _if_ change to dynamic (w/ token) */
        purple_notify_emails(session->gc,       /* handle */
                n,                              /* count */
                TRUE,                           /* detailed */
                subjects, froms, tos, urls, 
                NULL,       /* PurpleNotifyCloseCallback cb */
                NULL);      /* gpointer user_data */

    }

    g_hash_table_destroy(body);
}

/* Send request to check if there is new mail. */
static gboolean
msim_check_inbox(gpointer data)
{
    MsimSession *session;

    session = (MsimSession *)data;

    purple_debug_info("msim", "msim_check_inbox: checking mail\n");
    g_return_val_if_fail(msim_send(session, 
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_GET,
			"dsn", MSIM_TYPE_INTEGER, MG_CHECK_MAIL_DSN,
			"lid", MSIM_TYPE_INTEGER, MG_CHECK_MAIL_LID,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"rid", MSIM_TYPE_INTEGER, 
                msim_new_reply_callback(session, msim_check_inbox_cb, NULL),
			"body", MSIM_TYPE_STRING, g_strdup(""),
			NULL), TRUE);

    /* Always return true, so that we keep checking for mail. */
    return TRUE;
}

/** Called when the session key arrives. */
static gboolean
msim_we_are_logged_on(MsimSession *session, MsimMessage *msg)
{
    MsimMessage *body;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    purple_connection_update_progress(session->gc, _("Connected"), 3, 4);
    purple_connection_set_state(session->gc, PURPLE_CONNECTED);

    session->sesskey = msim_msg_get_integer(msg, "sesskey");
    purple_debug_info("msim", "SESSKEY=<%d>\n", session->sesskey);

    /* What is proof? Used to be uid, but now is 52 base64'd bytes... */

    /* Comes with: proof,profileid,userid,uniquenick -- all same values
     * some of the time, but can vary. This is our own user ID. */
    session->userid = msim_msg_get_integer(msg, "userid");

    /* Not sure what profileid is used for. */
    if (msim_msg_get_integer(msg, "profileid") != session->userid)
    {
        msim_unrecognized(session, msg, 
                "Profile ID didn't match user ID, don't know why");
    }

    /* We now know are our own username, only after we're logged in..
     * which is weird, but happens because you login with your email
     * address and not username. Will be freed in msim_session_destroy(). */
    session->username = msim_msg_get_string(msg, "uniquenick");

    if (msim_msg_get_integer(msg, "uniquenick") == session->userid)
    {
        purple_debug_info("msim_we_are_logged_on", "TODO: pick username");
    }

    body = msim_msg_new(TRUE,
            "UserID", MSIM_TYPE_INTEGER, session->userid,
            NULL);

    /* Request IM info about ourself. */
    msim_send(session,
            "persist", MSIM_TYPE_STRING, g_strdup("persist"),
            "sesskey", MSIM_TYPE_INTEGER, session->sesskey,
            "dsn", MSIM_TYPE_INTEGER, MG_OWN_MYSPACE_INFO_DSN,
            "uid", MSIM_TYPE_INTEGER, session->userid,
            "lid", MSIM_TYPE_INTEGER, MG_OWN_MYSPACE_INFO_LID,
            "rid", MSIM_TYPE_INTEGER, session->next_rid++,
            "body", MSIM_TYPE_DICTIONARY, body,
            NULL);

    /* Request MySpace info about ourself. */
    msim_send(session,
            "persist", MSIM_TYPE_STRING, g_strdup("persist"),
            "sesskey", MSIM_TYPE_INTEGER, session->sesskey,
            "dsn", MSIM_TYPE_INTEGER, MG_OWN_IM_INFO_DSN,
            "uid", MSIM_TYPE_INTEGER, session->userid,
            "lid", MSIM_TYPE_INTEGER, MG_OWN_IM_INFO_LID,
            "rid", MSIM_TYPE_INTEGER, session->next_rid++,
            "body", MSIM_TYPE_STRING, g_strdup(""),
            NULL);

    /* TODO: set options (persist cmd=514,dsn=1,lid=10) */
    /* TODO: set blocklist */

    /* Notify servers of our current status. */
    purple_debug_info("msim", "msim_we_are_logged_on: notifying servers of status\n");
    msim_set_status(session->account,
            purple_account_get_active_status(session->account));

    /* TODO: setinfo */
    /*
    body = msim_msg_new(TRUE,
        "TotalFriends", MSIM_TYPE_INTEGER, 666,
        NULL);
    msim_send(session,
            "setinfo", MSIM_TYPE_BOOLEAN, TRUE,
            "sesskey", MSIM_TYPE_INTEGER, session->sesskey,
            "info", MSIM_TYPE_DICTIONARY, body,
            NULL);
            */

    /* Disable due to problems with timeouts. TODO: fix. */
#ifdef MSIM_USE_KEEPALIVE
    purple_timeout_add(MSIM_KEEPALIVE_INTERVAL_CHECK, 
            (GSourceFunc)msim_check_alive, session);
#endif

    purple_timeout_add(MSIM_MAIL_INTERVAL_CHECK, 
            (GSourceFunc)msim_check_inbox, session);

    msim_check_inbox(session);

    return TRUE;
}

/**
 * Process a message. 
 *
 * @param session
 * @param msg A message from the server, ready for processing (possibly with resolved username information attached). Caller frees.
 *
 * @return TRUE if successful. FALSE if processing failed.
 */
static gboolean 
msim_process(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(session != NULL, FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

#ifdef MSIM_DEBUG_MSG
	{
		msim_msg_dump("ready to process: %s\n", msg);
	}
#endif

    if (msim_msg_get_integer(msg, "lc") == 1)
    {
        return msim_login_challenge(session, msg);
    } else if (msim_msg_get_integer(msg, "lc") == 2) {
        return msim_we_are_logged_on(session, msg);
    } else if (msim_msg_get(msg, "bm"))  {
        return msim_incoming_bm(session, msg);
    } else if (msim_msg_get(msg, "rid")) {
        return msim_process_reply(session, msg);
    } else if (msim_msg_get(msg, "error")) {
        return msim_error(session, msg);
    } else if (msim_msg_get(msg, "ka")) {
        return TRUE;
    } else {
		msim_unrecognized(session, msg, "in msim_process");
        return FALSE;
    }
}

/** Store an field of information about a buddy. */
static void 
msim_store_buddy_info_each(gpointer key, gpointer value, gpointer user_data)
{
	PurpleBuddy *buddy;
	gchar *key_str, *value_str;

	buddy = (PurpleBuddy *)user_data;
	key_str = (gchar *)key;
	value_str = (gchar *)value;

	if (strcmp(key_str, "UserID") == 0 ||
			strcmp(key_str, "Age") == 0 ||
			strcmp(key_str, "TotalFriends") == 0)
	{
		/* Certain fields get set as integers, instead of strings, for
		 * convenience. May not be the best way to do it, but having at least
		 * UserID as an integer is convenient...until it overflows! */
		purple_blist_node_set_int(&buddy->node, key_str, atol(value_str));
	} else {
		purple_blist_node_set_string(&buddy->node, key_str, value_str);
	}
}

/** Save buddy information to the buddy list from a user info reply message.
 *
 * @param session
 * @param msg The user information reply, with any amount of information.
 *
 * The information is saved to the buddy's blist node, which ends up in blist.xml.
 */
static gboolean 
msim_store_buddy_info(MsimSession *session, MsimMessage *msg)
{
	GHashTable *body;
	gchar *username, *body_str, *uid;
	PurpleBuddy *buddy;
	guint rid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
 
	rid = msim_msg_get_integer(msg, "rid");
	
	g_return_val_if_fail(rid != 0, FALSE);

	body_str = msim_msg_get_string(msg, "body");
	g_return_val_if_fail(body_str != NULL, FALSE);
	body = msim_parse_body(body_str);
	g_free(body_str);

	/* TODO: implement a better hash-like interface, and use it. */
	username = g_hash_table_lookup(body, "UserName");

	if (!username)
	{
		purple_debug_info("msim", 
			"msim_process_reply: not caching body, no UserName\n");
        g_hash_table_destroy(body);
		return FALSE;
	}

	uid = g_hash_table_lookup(body, "UserID");
    if (!uid)
    {
        g_hash_table_destroy(body);
        g_return_val_if_fail(uid, FALSE);
    }

	purple_debug_info("msim", "associating uid %s with username %s\n", uid, username);

	buddy = purple_find_buddy(session->account, username);
	if (buddy)
	{
		g_hash_table_foreach(body, msim_store_buddy_info_each, buddy);
	}

    if (msim_msg_get_integer(msg, "dsn") == MG_OWN_IM_INFO_DSN &&
        msim_msg_get_integer(msg, "lid") == MG_OWN_IM_INFO_LID)
    {
        /* TODO: do something with our own IM info, if we need it for some
         * specific purpose. Otherwise it is available on the buddy list,
         * if the user has themselves as their own buddy. */
    } else if (msim_msg_get_integer(msg, "dsn") == MG_OWN_MYSPACE_INFO_DSN &&
            msim_msg_get_integer(msg, "lid") == MG_OWN_MYSPACE_INFO_LID) {
        /* TODO: same as above, but for MySpace info. */
    }

    g_hash_table_destroy(body);

	return TRUE;
}

/** Process the initial server information from the server. */
static gboolean
msim_process_server_info(MsimSession *session, MsimMessage *msg)
{
    gchar *body_str;
    GHashTable *body;

    body_str = msim_msg_get_string(msg, "body");
    g_return_val_if_fail(body_str != NULL, FALSE);
    body = msim_parse_body(body_str);
    g_free(body_str);
    g_return_val_if_fail(body != NULL, FALSE);
 
    /* Example body:
AdUnitRefreshInterval=10.
AlertPollInterval=360.
AllowChatRoomEmoticonSharing=False.
ChatRoomUserIDs=78744676;163733130;1300326231;123521495;142663391.
CurClientVersion=673.
EnableIMBrowse=True.
EnableIMStuffAvatars=False.
EnableIMStuffZaps=False.
MaxAddAllFriends=100.
MaxContacts=1000.
MinClientVersion=594.
MySpaceIM_ENGLISH=78744676.
MySpaceNowTimer=720.
PersistenceDataTimeout=900.
UseWebChallenge=1.
WebTicketGoHome=False

    Anything useful? TODO: use what is useful, and use it.
*/
    purple_debug_info("msim_process_server_info",
            "maximum contacts: %s\n", 
            g_hash_table_lookup(body, "MaxContacts") ? 
            g_hash_table_lookup(body, "MaxContacts") : "(NULL)");

    session->server_info = body;
    /* session->server_info freed in msim_session_destroy */

    return TRUE;
}

/** Process a web challenge, used to login to the web site. */
static gboolean
msim_web_challenge(MsimSession *session, MsimMessage *msg)
{
    /* TODO: web challenge, store token */
    return FALSE;
}

/**
 * Process a persistance message reply from the server.
 *
 * @param session 
 * @param msg Message reply from server.
 *
 * @return TRUE if successful.
 *
 * msim_lookup_user sets callback for here 
 */
static gboolean 
msim_process_reply(MsimSession *session, MsimMessage *msg)
{
    MSIM_USER_LOOKUP_CB cb;
    gpointer data;
    guint rid, cmd, dsn, lid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    msim_store_buddy_info(session, msg);		

    rid = msim_msg_get_integer(msg, "rid");
    cmd = msim_msg_get_integer(msg, "cmd");
    dsn = msim_msg_get_integer(msg, "dsn");
    lid = msim_msg_get_integer(msg, "lid");

    /* Unsolicited messages */
    if (cmd == (MSIM_CMD_BIT_REPLY | MSIM_CMD_GET))
    {
        if (dsn == MG_SERVER_INFO_DSN && lid == MG_SERVER_INFO_LID)
        {
            return msim_process_server_info(session, msg);
        } else if (dsn == MG_WEB_CHALLENGE_DSN && lid == MG_WEB_CHALLENGE_LID) {
            return msim_web_challenge(session, msg);
        }
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
static gboolean 
msim_error(MsimSession *session, MsimMessage *msg)
{
    gchar *errmsg, *full_errmsg;
	guint err;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    err = msim_msg_get_integer(msg, "err");
    errmsg = msim_msg_get_string(msg, "errmsg");

    full_errmsg = g_strdup_printf(_("Protocol error, code %d: %s"), err, 
			errmsg ? errmsg : "no 'errmsg' given");

	g_free(errmsg);

    purple_debug_info("msim", "msim_error: %s\n", full_errmsg);

    purple_notify_error(session->account, g_strdup(_("MySpaceIM Error")), 
            full_errmsg, NULL);

	/* Destroy session if fatal. */
    if (msim_msg_get(msg, "fatal"))
    {
        purple_debug_info("msim", "fatal error, closing\n");
        purple_connection_error(session->gc, full_errmsg);
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
static gboolean 
msim_incoming_status(MsimSession *session, MsimMessage *msg)
{
    PurpleBuddyList *blist;
    PurpleBuddy *buddy;
    //PurpleStatus *status;
    //gchar **status_array;
    GList *list;
    gchar *status_headline;
    //gchar *status_str;
    //gint i;
    gint status_code, purple_status_code;
    gchar *username;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

	msim_msg_dump("msim_status msg=%s\n", msg);

	/* Helpfully looked up by msim_incoming_resolve() for us. */
    username = msim_msg_get_string(msg, "_username");
    g_return_val_if_fail(username != NULL, FALSE);

    {
        gchar *ss;

        ss = msim_msg_get_string(msg, "msg");
        purple_debug_info("msim", 
                "msim_status: updating status for <%s> to <%s>\n",
                username, ss ? ss : "(NULL)");
        g_free(ss);
    }

    /* Example fields: 
	 *  |s|0|ss|Offline 
	 *  |s|1|ss|:-)|ls||ip|0|p|0 
	 */
    list = msim_msg_get_list(msg, "msg");

    status_code = atoi(g_list_nth_data(list, MSIM_STATUS_ORDINAL_ONLINE));
	purple_debug_info("msim", "msim_status: %s's status code = %d\n", username, status_code);
    status_headline = g_list_nth_data(list, MSIM_STATUS_ORDINAL_HEADLINE);

    blist = purple_get_blist();

    /* Add buddy if not found */
    buddy = purple_find_buddy(session->account, username);
    if (!buddy)
    {
        purple_debug_info("msim", 
				"msim_status: making new buddy for %s\n", username);
        buddy = purple_buddy_new(session->account, username, NULL);

        purple_blist_add_buddy(buddy, NULL, NULL, NULL);

		/* All buddies on list should have 'uid' integer associated with them. */
		purple_blist_node_set_int(&buddy->node, "UserID", msim_msg_get_integer(msg, "f"));
		
		msim_store_buddy_info(session, msg);
    } else {
        purple_debug_info("msim", "msim_status: found buddy %s\n", username);
    }

	purple_blist_node_set_string(&buddy->node, "Headline", status_headline);
  
    /* Set user status */	
    switch (status_code)
	{
		case MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN: 
			purple_status_code = PURPLE_STATUS_OFFLINE;	
			break;

		case MSIM_STATUS_CODE_ONLINE: 
			purple_status_code = PURPLE_STATUS_AVAILABLE;
			break;

		case MSIM_STATUS_CODE_AWAY:
			purple_status_code = PURPLE_STATUS_AWAY;
			break;

        case MSIM_STATUS_CODE_IDLE:
            /* will be handled below */
            purple_status_code = -1;
            break;

		default:
				purple_debug_info("msim", "msim_status for %s, unknown status code %d, treating as available\n",
						username, status_code);
				purple_status_code = PURPLE_STATUS_AVAILABLE;
	}

    purple_prpl_got_user_status(session->account, username, purple_primitive_get_id_from_type(purple_status_code), NULL);

    if (status_code == MSIM_STATUS_CODE_IDLE)
    {
        purple_debug_info("msim", "msim_status: got idle: %s\n", username);
        purple_prpl_got_user_idle(session->account, username, TRUE, time(NULL));
    } else {
        /* All other statuses indicate going back to non-idle. */
        purple_prpl_got_user_idle(session->account, username, FALSE, time(NULL));
    }

#ifdef MSIM_SEND_CLIENT_VERSION
    if (status_code == MSIM_STATUS_CODE_ONLINE)
    {
        /* Secretly whisper to unofficial clients our own version as they come online */
        msim_send_unofficial_client(session, username);
    }
#endif

	g_free(username);
    msim_msg_list_free(list);

    return TRUE;
}

/** Add a buddy to user's buddy list. */
void 
msim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *msg;
    MsimMessage	*msg_persist;
    MsimMessage *body;

	session = (MsimSession *)gc->proto_data;
	purple_debug_info("msim", "msim_add_buddy: want to add %s to %s\n", 
            buddy->name, (group && group->name) ? group->name : "(no group)");

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
	
	/* TODO: if addbuddy fails ('error' message is returned), delete added buddy from
	 * buddy list since Purple adds it locally. */

    body = msim_msg_new(TRUE,
            "ContactID", MSIM_TYPE_STRING, g_strdup("<uid>"),
            "GroupName", MSIM_TYPE_STRING, g_strdup(group->name),
            "Position", MSIM_TYPE_INTEGER, 1000,
            "Visibility", MSIM_TYPE_INTEGER, 1,
            "NickName", MSIM_TYPE_STRING, g_strdup(""),
            "NameSelect", MSIM_TYPE_INTEGER, 0,
            NULL);

	/* TODO: Update blocklist. */

#if 0
	msg_persist = msim_msg_new(TRUE,
		"persist", MSIM_TYPE_INTEGER, 1,
		"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
		"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_PUT,
		"dsn", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_DSN,
		"lid", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_LID,
		/* TODO: Use msim_new_reply_callback to get rid. */
		"rid", MSIM_TYPE_INTEGER, session->next_rid++,
		"body", MSIM_TYPE_DICTIONARY, body,
        NULL);

	if (!msim_postprocess_outgoing(session, msg_persist, buddy->name, "body", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to add buddy"), _("persist command failed"));
		msim_msg_free(msg_persist);
		return;
	}
	msim_msg_free(msg_persist);
#endif

}

/** Perform actual postprocessing on a message, adding userid as specified.
 *
 * @param msg The message to postprocess.
 * @param uid_before Name of field where to insert new field before, or NULL for end.
 * @param uid_field_name Name of field to add uid to.
 * @param uid The userid to insert.
 *
 * If the field named by uid_field_name already exists, then its string contents will
 * be used for the field, except "<uid>" will be replaced by the userid.
 *
 * If the field named by uid_field_name does not exist, it will be added before the
 * field named by uid_before, as an integer, with the userid.
 *
 * Does not handle sending, or scheduling userid lookup. For that, see msim_postprocess_outgoing().
 */ 
static MsimMessage *
msim_do_postprocessing(MsimMessage *msg, const gchar *uid_before, 
		const gchar *uid_field_name, guint uid)
{	
	msim_msg_dump("msim_do_postprocessing msg: %s\n", msg);

	/* First, check - if the field already exists, replace <uid> within it */
	if (msim_msg_get(msg, uid_field_name))
	{
		MsimMessageElement *elem;
		gchar *fmt_string;
		gchar *uid_str, *new_str;

        /* Warning: this is a delicate, but safe, operation */

		elem = msim_msg_get(msg, uid_field_name);

        /* Get the packed element, flattening it. This allows <uid> to be
         * replaced within nested data structures, since the replacement is done
         * on the linear, packed data, not on a complicated data structure.
         *
         * For example, if the field was originally a dictionary or a list, you 
         * would have to iterate over all the items in it to see what needs to
         * be replaced. But by packing it first, the <uid> marker is easily replaced
         * just by a string replacement.
         */
        fmt_string = msim_msg_pack_element_data(elem);

		uid_str = g_strdup_printf("%d", uid);
		new_str = str_replace(fmt_string, "<uid>", uid_str);
		g_free(uid_str);
		g_free(fmt_string);

        /* Free the old element data */
        msim_msg_free_element_data(elem->data);

        /* Replace it with our new data */
        elem->data = new_str;
        elem->type = MSIM_TYPE_RAW;

	} else {
		/* Otherwise, insert new field into outgoing message. */
		msg = msim_msg_insert_before(msg, uid_before, uid_field_name, MSIM_TYPE_INTEGER, GUINT_TO_POINTER(uid));
	}

    msim_msg_dump("msim_postprocess_outgoing_cb: postprocessed msg=%s\n", msg);

	return msg;
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
*/
static void 
msim_postprocess_outgoing_cb(MsimSession *session, MsimMessage *userinfo, 
        gpointer data)
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

	msg = msim_do_postprocessing(msg, uid_before, uid_field_name, atol(uid));

	/* Send */
	if (!msim_msg_send(session, msg))
	{
		msim_msg_dump("msim_postprocess_outgoing_cb: sending failed for message: %s\n", msg);
	}


	/* Free field names AFTER sending message, because MsimMessage does NOT copy
	 * field names - instead, treats them as static strings (which they usually are).
	 */
	g_free(uid_field_name);
	g_free(uid_before);

    g_hash_table_destroy(body);

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
 * @return TRUE if successful.
 */
gboolean 
msim_postprocess_outgoing(MsimSession *session, MsimMessage *msg, 
		const gchar *username, const gchar *uid_field_name, 
		const gchar *uid_before)
{
    PurpleBuddy *buddy;
	guint uid;
	gboolean rc;

    g_return_val_if_fail(msg != NULL, FALSE);

	/* Store information for msim_postprocess_outgoing_cb(). */
	msim_msg_dump("msim_postprocess_outgoing: msg before=%s\n", msg);
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
			uid = purple_blist_node_get_int(&buddy->node, "UserID");
		} else {
			uid = 0;
		}

		if (!buddy || !uid)
		{
			/* Don't have uid offhand - need to ask for it, and wait until hear back before sending. */
			purple_debug_info("msim", ">>> msim_postprocess_outgoing: couldn't find username %s in blist\n",
					username ? username : "(NULL)");
			msim_msg_dump("msim_postprocess_outgoing - scheduling lookup, msg=%s\n", msg);
			/* TODO: where is cloned message freed? Should be in _cb. */
			msim_lookup_user(session, username, msim_postprocess_outgoing_cb, msim_msg_clone(msg));
			return TRUE;		/* not sure of status yet - haven't sent! */
		}
	}
	
	/* Already have uid, postprocess and send msg immediately. */
	purple_debug_info("msim", "msim_postprocess_outgoing: found username %s has uid %d\n",
			username ? username : "(NULL)", uid);

	msg = msim_do_postprocessing(msg, uid_before, uid_field_name, uid);

	msim_msg_dump("msim_postprocess_outgoing: msg after (uid immediate)=%s\n", msg);
	
	rc = msim_msg_send(session, msg);

	//msim_msg_free(msg);

	return rc;
}

/** Remove a buddy from the user's buddy list. */
void 
msim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *delbuddy_msg;
	MsimMessage *persist_msg;
	MsimMessage *blocklist_msg;
    GList *blocklist_updates;

	session = (MsimSession *)gc->proto_data;

	delbuddy_msg = msim_msg_new(TRUE,
				"delbuddy", MSIM_TYPE_BOOLEAN, TRUE,
				"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
				/* 'delprofileid' with uid will be inserted here. */
				NULL);

	if (!msim_postprocess_outgoing(session, delbuddy_msg, buddy->name, "delprofileid", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("'delbuddy' command failed"));
        msim_msg_free(delbuddy_msg);
		return;
	}
    msim_msg_free(delbuddy_msg);

	persist_msg = msim_msg_new(TRUE, 
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_DELETE,
			"dsn", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_DSN,
			"lid", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_LID,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"rid", MSIM_TYPE_INTEGER, session->next_rid++,
			/* <uid> will be replaced by postprocessing */
			"body", MSIM_TYPE_STRING, g_strdup("ContactID=<uid>"),
			NULL);

	if (!msim_postprocess_outgoing(session, persist_msg, buddy->name, "body", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("persist command failed"));	
        msim_msg_free(persist_msg);
		return;
	}
    msim_msg_free(persist_msg);

    blocklist_updates = NULL;
    blocklist_updates = g_list_prepend(blocklist_updates, "a-");
    blocklist_updates = g_list_prepend(blocklist_updates, "<uid>");
    blocklist_updates = g_list_prepend(blocklist_updates, "b-");
    blocklist_updates = g_list_prepend(blocklist_updates, "<uid>");
    blocklist_updates = g_list_reverse(blocklist_updates);

	blocklist_msg = msim_msg_new(TRUE,
			"blocklist", MSIM_TYPE_BOOLEAN, TRUE,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			/* TODO: MsimMessage lists. Currently <uid> isn't replaced in lists. */
			//"idlist", MSIM_TYPE_STRING, g_strdup("a-|<uid>|b-|<uid>"),
            "idlist", MSIM_TYPE_LIST, blocklist_updates,
			NULL);

	if (!msim_postprocess_outgoing(session, blocklist_msg, buddy->name, "idlist", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("blocklist command failed"));
        msim_msg_free(blocklist_msg);
		return;
	}
    msim_msg_free(blocklist_msg);
}

/** Return whether the buddy can be messaged while offline.
 *
 * The protocol supports offline messages in just the same way as online
 * messages.
 */
gboolean 
msim_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
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
static void 
msim_input_cb(gpointer gc_uncasted, gint source, PurpleInputCondition cond)
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
	g_return_if_fail(MSIM_SESSION_VALID(session));

    /* Mark down that we got data, so don't timeout. */
    session->last_comm = time(NULL);

    /* Only can handle so much data at once... 
     * If this happens, try recompiling with a higher MSIM_READ_BUF_SIZE.
     * Should be large enough to hold the largest protocol message.
     */
    if (session->rxoff >= MSIM_READ_BUF_SIZE)
    {
        purple_debug_error("msim", 
                "msim_input_cb: %d-byte read buffer full! rxoff=%d\n",
                MSIM_READ_BUF_SIZE, session->rxoff);
        purple_connection_error(gc, _("Read buffer full"));
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
        purple_debug_error("msim", "msim_input_cb: read error, ret=%d, "
			"error=%s, source=%d, fd=%d (%X))\n", 
			n, strerror(errno), source, session->fd, session->fd);
        purple_connection_error(gc, _("Read error"));
        return;
    } 
    else if (n == 0)
    {
        purple_debug_info("msim", "msim_input_cb: server disconnected\n");
        purple_connection_error(gc, _("Server has disconnected"));
        return;
    }

    if (n + session->rxoff >= MSIM_READ_BUF_SIZE)
    {
        purple_debug_info("msim_input_cb", "received %d bytes, pushing rxoff to %d, over buffer size of %d\n",
                n, n + session->rxoff, MSIM_READ_BUF_SIZE);
        /* TODO: g_realloc like msn, yahoo, irc, jabber? */
        purple_connection_error(gc, _("Read buffer full"));
    }

    /* Null terminate */
    purple_debug_info("msim", "msim_input_cb: going to null terminate "
            "at n=%d\n", n);
    session->rxbuf[session->rxoff + n] = 0;

#ifdef MSIM_CHECK_EMBEDDED_NULLS
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
#endif

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
            purple_debug_info("msim", "msim_input_cb: couldn't parse rxbuf\n");
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
 * @return The request/reply ID, used to link replies with requests, or -1.
 *         Put the rid in your request, 'rid' field.
 *
 * TODO: Make more generic and more specific:
 * 1) MSIM_USER_LOOKUP_CB - make it for PERSIST_REPLY, not just user lookup
 * 2) data - make it an MsimMessage?
 */
static guint 
msim_new_reply_callback(MsimSession *session, MSIM_USER_LOOKUP_CB cb, 
		gpointer data)
{
	guint rid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), -1);

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
static void 
msim_connect_cb(gpointer data, gint source, const gchar *error_message)
{
    PurpleConnection *gc;
    MsimSession *session;

    g_return_if_fail(data != NULL);

    gc = (PurpleConnection *)data;
    session = (MsimSession *)gc->proto_data;

    if (source < 0)
    {
        purple_connection_error(gc, _("Couldn't connect to host"));
        purple_connection_error(gc, g_strdup_printf(
					_("Couldn't connect to host: %s (%d)"), 
                    error_message ? error_message : "no message given", 
					source));
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
MsimSession *
msim_session_new(PurpleAccount *acct)
{
    MsimSession *session;

    g_return_val_if_fail(acct != NULL, NULL);

    session = g_new0(MsimSession, 1);

    session->magic = MSIM_SESSION_STRUCT_MAGIC;
    session->account = acct;
    session->gc = purple_account_get_connection(acct);
	session->sesskey = 0;
	session->userid = 0;
	session->username = NULL;
    session->fd = -1;

	/* TODO: Remove. */
    session->user_lookup_cb = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);  /* do NOT free function pointers! (values) */
    session->user_lookup_cb_data = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);/* TODO: we don't know what the values are,
											 they could be integers inside gpointers
											 or strings, so I don't freed them.
											 Figure this out, once free cache. */

    /* Created in msim_process_server_info() */
    session->server_info = NULL;

    session->rxoff = 0;
    session->rxbuf = g_new0(gchar, MSIM_READ_BUF_SIZE);
	session->next_rid = 1;
    session->last_comm = time(NULL);
    session->inbox_status = 0;
    
    return session;
}

/**
 * Free a session.
 *
 * @param session The session to destroy.
 */
void 
msim_session_destroy(MsimSession *session)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));
	
    session->magic = -1;

    g_free(session->rxbuf);
	g_free(session->username);

	/* TODO: Remove. */
	g_hash_table_destroy(session->user_lookup_cb);
	g_hash_table_destroy(session->user_lookup_cb_data);

    if (session->server_info)
        g_hash_table_destroy(session->server_info);
	
    g_free(session);
}
                 
/** 
 * Close the connection.
 * 
 * @param gc The connection.
 */
void 
msim_close(PurpleConnection *gc)
{
	MsimSession *session;

	if (gc == NULL)
		return;

	session = (MsimSession *)gc->proto_data;
	if (session == NULL)
		return;

	gc->proto_data = NULL;

	if (!MSIM_SESSION_VALID(session))
		return;

    if (session->gc->inpa)
		purple_input_remove(session->gc->inpa);

    msim_session_destroy(session);
}


/**
 * Check if a string is a userid (all numeric).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is userid, FALSE if not.
 */
static gboolean 
msim_is_userid(const gchar *user)
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
static gboolean 
msim_is_email(const gchar *user)
{
    g_return_val_if_fail(user != NULL, FALSE);

    return strchr(user, '@') != NULL;
}


/**
 * Asynchronously lookup user information, calling callback when receive result.
 *
 * @param session
 * @param user The user id, email address, or username. Not freed.
 * @param cb Callback, called with user information when available.
 * @param data An arbitray data pointer passed to the callback.
 */
/* TODO: change to not use callbacks */
static void 
msim_lookup_user(MsimSession *session, const gchar *user, 
		MSIM_USER_LOOKUP_CB cb, gpointer data)
{
    MsimMessage *body;
    gchar *field_name;
    guint rid, cmd, dsn, lid;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(user != NULL);
    g_return_if_fail(cb != NULL);

    purple_debug_info("msim", "msim_lookup_userid: "
			"asynchronously looking up <%s>\n", user);

	msim_msg_dump("msim_lookup_user: data=%s\n", (MsimMessage *)data);

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

    body = msim_msg_new(TRUE,
            field_name, MSIM_TYPE_STRING, g_strdup(user),
            NULL);

	g_return_if_fail(msim_send(session,
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, 1,
			"dsn", MSIM_TYPE_INTEGER, dsn,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"lid", MSIM_TYPE_INTEGER, lid,
			"rid", MSIM_TYPE_INTEGER, rid,
			"body", MSIM_TYPE_DICTIONARY, body,
			NULL));
} 


/**
 * Obtain the status text for a buddy.
 *
 * @param buddy The buddy to obtain status text for.
 *
 * @return Status text, or NULL if error. Caller g_free()'s.
 *
 */
char *
msim_status_text(PurpleBuddy *buddy)
{
    MsimSession *session;
	const gchar *display_name, *headline;

    g_return_val_if_fail(buddy != NULL, NULL);

    session = (MsimSession *)buddy->account->gc->proto_data;
    g_return_val_if_fail(MSIM_SESSION_VALID(session), NULL);

	display_name = headline = NULL;

	/* Retrieve display name and/or headline, depending on user preference. */
    if (purple_account_get_bool(session->account, "show_display_name", TRUE))
	{
		display_name = purple_blist_node_get_string(&buddy->node, "DisplayName");
	} 

    if (purple_account_get_bool(session->account, "show_headline", FALSE))
	{
		headline = purple_blist_node_get_string(&buddy->node, "Headline");
	}

	/* Return appropriate combination of display name and/or headline, or neither. */

	if (display_name && headline)
		return g_strconcat(display_name, " ", headline, NULL);

	if (display_name)
		return g_strdup(display_name);

	if (headline)
		return g_strdup(headline);

	return NULL;
}

/**
 * Obtain the tooltip text for a buddy.
 *
 * @param buddy Buddy to obtain tooltip text on.
 * @param user_info Variable modified to have the tooltip text.
 * @param full TRUE if should obtain full tooltip text.
 *
 */
void 
msim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, 
		gboolean full)
{
	const gchar *str, *str2;
	gint n;

    g_return_if_fail(buddy != NULL);
    g_return_if_fail(user_info != NULL);

    if (PURPLE_BUDDY_IS_ONLINE(buddy))
    {
        MsimSession *session;

        session = (MsimSession *)buddy->account->gc->proto_data;

        g_return_if_fail(MSIM_SESSION_VALID(session));

        /* TODO: if (full), do something different */
		
		/* Useful to identify the account the tooltip refers to. 
		 *  Other prpls show this. */
		str = purple_blist_node_get_string(&buddy->node, "UserName"); 
		if (str)
			purple_notify_user_info_add_pair(user_info, _("User Name"), str);

		/* a/s/l...the vitals */	
		n = purple_blist_node_get_int(&buddy->node, "Age");
		if (n)
			purple_notify_user_info_add_pair(user_info, _("Age"),
					g_strdup_printf("%d", n));

		str = purple_blist_node_get_string(&buddy->node, "Gender");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Gender"), str);

		str = purple_blist_node_get_string(&buddy->node, "Location");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Location"), str);

		/* Other information */
 		str = purple_blist_node_get_string(&buddy->node, "Headline");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Headline"), str);

		str = purple_blist_node_get_string(&buddy->node, "BandName");
		str2 = purple_blist_node_get_string(&buddy->node, "SongName");
		if (str || str2)
			purple_notify_user_info_add_pair(user_info, _("Song"), 
                g_strdup_printf("%s - %s",
					str ? str : _("Unknown Artist"),
					str2 ? str2 : _("Unknown Song")));

		n = purple_blist_node_get_int(&buddy->node, "TotalFriends");
		if (n)
			purple_notify_user_info_add_pair(user_info, _("Total Friends"),
				g_strdup_printf("%d", n));

    }
}

/** Actions menu for account. */
GList *
msim_actions(PurplePlugin *plugin, gpointer context)
{
    PurpleConnection *gc;
    GList *menu;
    //PurplePluginAction *act;

    gc = (PurpleConnection *)context;

    menu = NULL;

#if 0
    /* TODO: find out how */
    act = purple_plugin_action_new(_("Find people..."), msim_);
    menu = g_list_append(menu, act);

    act = purple_plugin_action_new(_("Import friends..."), NULL);
    menu = g_list_append(menu, act);

    act = purple_plugin_action_new(_("Change IM name..."), NULL);
    menu = g_list_append(menu, act);
#endif

    return menu;
}

/** Callbacks called by Purple, to access this plugin. */
PurplePluginProtocolInfo prpl_info =
{
	/* options */
      OPT_PROTO_USE_POINTSIZE		/* specify font size in sane point size */
	| OPT_PROTO_MAIL_CHECK,

	/* | OPT_PROTO_IM_IMAGE - TODO: direct images. */	
    NULL,              /* user_splits */
    NULL,              /* protocol_options */
    NO_BUDDY_ICONS,    /* icon_spec - TODO: eventually should add this */
    msim_list_icon,    /* list_icon */
    NULL,              /* list_emblems */
    msim_status_text,  /* status_text */
    msim_tooltip_text, /* tooltip_text */
    msim_status_types, /* status_types */
    msim_blist_node_menu,  /* blist_node_menu */
    NULL,              /* chat_info */
    NULL,              /* chat_info_defaults */
    msim_login,        /* login */
    msim_close,        /* close */
    msim_send_im,      /* send_im */
    NULL,              /* set_info */
    msim_send_typing,  /* send_typing */
	msim_get_info, 	   /* get_info */
    msim_set_status,   /* set_status */
    msim_set_idle,     /* set_idle */
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
    msim_offline_message, /* offline_message */
    NULL,              /* whiteboard_prpl_ops */
    msim_send_really_raw,     /* send_raw */
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
    MSIM_PRPL_VERSION_STRING,                         /**< version        */
                                                      /**  summary        */
    "MySpaceIM Protocol Plugin",
                                                      /**  description    */
    "MySpaceIM Protocol Plugin",
    "Jeff Connelly <jeff2@soc.pidgin.im>",         /**< author         */
    "http://developer.pidgin.im/wiki/MySpaceIM/",     /**< homepage       */

    msim_load,                                        /**< load           */
    NULL,                                             /**< unload         */
    NULL,                                             /**< destroy        */
    NULL,                                             /**< ui_info        */
    &prpl_info,                                       /**< extra_info     */
    NULL,                                             /**< prefs_info     */
    msim_actions,                                     /**< msim_actions   */
	NULL,											  /**< reserved1      */
	NULL,											  /**< reserved2      */
	NULL,											  /**< reserved3      */
	NULL 											  /**< reserved4      */
};


#ifdef MSIM_SELF_TEST
/** Test functions.
 * Used to test or try out the internal workings of msimprpl. If you're reading
 * this code for the first time, these functions can be instructive in how
 * msimprpl is architected.
 */
void 
msim_test_all(void) 
{
	guint failures;


	failures = 0;
	failures += msim_test_msg();
	failures += msim_test_escaping();

	if (failures)
	{
		purple_debug_info("msim", "msim_test_all HAD FAILURES: %d\n", failures);
	}
	else
	{
		purple_debug_info("msim", "msim_test_all - all tests passed!\n");
	}
	exit(0);
}

/** Test MsimMessage for basic functionality. */
int 
msim_test_msg(void)
{
	MsimMessage *msg, *msg_cloned, *msg2;
    GList *list;
	gchar *packed, *packed_expected, *packed_cloned;
	guint failures;

	failures = 0;

	purple_debug_info("msim", "\n\nTesting MsimMessage\n");
	msg = msim_msg_new(FALSE);		/* Create a new, empty message. */

	/* Append some new elements. */
	msg = msim_msg_append(msg, "bx", MSIM_TYPE_BINARY, g_string_new_len(g_strdup("XXX"), 3));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v1"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(42));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v43"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v52/xxx\\yyy"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v7"));
	msim_msg_dump("msg debug str=%s\n", msg);
	packed = msim_msg_pack(msg);

	purple_debug_info("msim", "msg packed=%s\n", packed);

	packed_expected = "\\bx\\WFhY\\k1\\v1\\k1\\42\\k1"
		"\\v43\\k1\\v52/1xxx/2yyy\\k1\\v7\\final\\";

	if (0 != strcmp(packed, packed_expected))
	{
		purple_debug_info("msim", "!!!(%d), msim_msg_pack not what expected: %s != %s\n",
				++failures, packed, packed_expected);
	}


	msg_cloned = msim_msg_clone(msg);
	packed_cloned = msim_msg_pack(msg_cloned);

	purple_debug_info("msim", "msg cloned=%s\n", packed_cloned);
	if (0 != strcmp(packed, packed_cloned))
	{
		purple_debug_info("msim", "!!!(%d), msim_msg_pack on cloned message not equal to original: %s != %s\n",
				++failures, packed_cloned, packed);
	}

	g_free(packed);
	g_free(packed_cloned);
	msim_msg_free(msg_cloned);
	msim_msg_free(msg);

    /* Try some of the more advanced functionality */
    list = NULL;

    list = g_list_prepend(list, "item3");
    list = g_list_prepend(list, "item2");
    list = g_list_prepend(list, "item1");
    list = g_list_prepend(list, "item0");

    msg = msim_msg_new(FALSE);
    msg = msim_msg_append(msg, "string", MSIM_TYPE_STRING, g_strdup("string value"));
    msg = msim_msg_append(msg, "raw", MSIM_TYPE_RAW, g_strdup("raw value"));
    msg = msim_msg_append(msg, "integer", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(3140));
    msg = msim_msg_append(msg, "boolean", MSIM_TYPE_BOOLEAN, GUINT_TO_POINTER(FALSE));
    msg = msim_msg_append(msg, "list", MSIM_TYPE_LIST, list);

    msim_msg_dump("msg with list=%s\n", msg);
    purple_debug_info("msim", "msg with list packed=%s\n", msim_msg_pack(msg));

    msg2 = msim_msg_new(FALSE);
    msg2 = msim_msg_append(msg2, "outer", MSIM_TYPE_STRING, g_strdup("outer value"));
    msg2 = msim_msg_append(msg2, "body", MSIM_TYPE_DICTIONARY, msg);
    msim_msg_dump("msg with dict=%s\n", msg2);      /* msg2 now 'owns' msg */
    purple_debug_info("msim", "msg with dict packed=%s\n", msim_msg_pack(msg2));

    msim_msg_free(msg2);

	return failures;
}

/** Test protocol-level escaping/unescaping. */
int 
msim_test_escaping(void)
{
	guint failures;
	gchar *raw, *escaped, *unescaped, *expected;

	failures = 0;

	purple_debug_info("msim", "\n\nTesting escaping\n");

	raw = "hello/world\\hello/world";

	escaped = msim_escape(raw);
	purple_debug_info("msim", "msim_test_escaping: raw=%s, escaped=%s\n", raw, escaped);
	expected = "hello/1world/2hello/1world";
	if (0 != strcmp(escaped, expected))
	{
		purple_debug_info("msim", "!!!(%d), msim_escape failed: %s != %s\n",
				++failures, escaped, expected);
	}


	unescaped = msim_unescape(escaped);
	g_free(escaped);
	purple_debug_info("msim", "msim_test_escaping: unescaped=%s\n", unescaped);
	if (0 != strcmp(raw, unescaped))
	{
		purple_debug_info("msim", "!!!(%d), msim_unescape failed: %s != %s\n",
				++failures, raw, unescaped);
	}	

	return failures;
}
#endif

/** Initialize plugin. */
void 
init_plugin(PurplePlugin *plugin) 
{
	PurpleAccountOption *option;
#ifdef MSIM_SELF_TEST
	msim_test_all();
#endif /* MSIM_SELF_TEST */

	/* TODO: default to automatically try different ports. Make the user be
	 * able to set the first port to try (like LastConnectedPort in Windows client).  */
	option = purple_account_option_string_new(_("Connect server"), "server", MSIM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Connect port"), "port", MSIM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show display name in status text"), "show_display_name", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show headline in status text"), "show_headline", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_bool_new(_("Send emoticons"), "emoticons", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

#ifdef MSIM_USER_REALLY_CARES_ABOUT_PRECISE_FONT_SIZES
    option = purple_account_option_int_new(_("Screen resolution (dots per inch)"), "dpi", MSIM_DEFAULT_DPI);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_int_new(_("Base font size (points)"), "base_font_size", MSIM_BASE_FONT_POINT_SIZE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif
}

PURPLE_INIT_PLUGIN(myspace, init_plugin, info);
