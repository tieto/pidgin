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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "myspace.h"

static void msim_store_user_info_each(const gchar *key_str, gchar *value_str, MsimUser *user);
static gchar *msim_format_now_playing(gchar *band, gchar *song);
static void msim_downloaded_buddy_icon(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text,
		gsize len, const gchar *error_message);

/** Format the "now playing" indicator, showing the artist and song.
 * @return Return a new string (must be g_free()'d), or NULL.
 */
static gchar *
msim_format_now_playing(gchar *band, gchar *song)
{
	if ((band && strlen(band)) || (song && strlen(song))) {
		return g_strdup_printf("%s - %s",
			(band && strlen(band)) ? band : "Unknown Artist",
			(song && strlen(song)) ? song : "Unknown Song");
	} else {
		return NULL;
	}
}
/** Get the MsimUser from a PurpleBuddy, creating it if needed. */
MsimUser *
msim_get_user_from_buddy(PurpleBuddy *buddy)
{
	MsimUser *user;

	if (!buddy) {
		return NULL;
	}

	if (!buddy->proto_data) {
		/* No MsimUser for this buddy; make one. */

		/* TODO: where is this freed? */
		user = g_new0(MsimUser, 1);
		user->buddy = buddy;
		buddy->proto_data = (gpointer)user;
	} 

	user = (MsimUser *)(buddy->proto_data);

	return user;
}

/** Find and return an MsimUser * representing a user on the buddy list, or NULL. */
MsimUser *
msim_find_user(MsimSession *session, const gchar *username)
{
	PurpleBuddy *buddy;
	MsimUser *user;

	buddy = purple_find_buddy(session->account, username);
	if (!buddy) {
		return NULL;
	}

	user = msim_get_user_from_buddy(buddy);

	return user;
}

/** Append user information to a PurpleNotifyUserInfo, given an MsimUser. 
 * Used by msim_tooltip_text() and msim_get_info_cb() to show a user's profile.
 */
void
msim_append_user_info(MsimSession *session, PurpleNotifyUserInfo *user_info, MsimUser *user, gboolean full)
{
	gchar *str;
	guint uid;
	guint cv;

	/* Useful to identify the account the tooltip refers to. 
	 *  Other prpls show this. */
	if (user->username) {
		purple_notify_user_info_add_pair(user_info, _("User"), user->username);
	}

	uid = purple_blist_node_get_int(&user->buddy->node, "UserID");

	if (full) {
		/* TODO: link to username, if available */
		purple_notify_user_info_add_pair(user_info, _("Profile"),
				g_strdup_printf("<a href=\"http://myspace.com/%d\">http://myspace.com/%d</a>",
					uid, uid));
	}


	/* a/s/l...the vitals */
	if (user->age) {
		purple_notify_user_info_add_pair(user_info, _("Age"),
				g_strdup_printf("%d", user->age));
	}

	if (user->gender && strlen(user->gender)) {
		purple_notify_user_info_add_pair(user_info, _("Gender"), user->gender);
	}

	if (user->location && strlen(user->location)) {
		purple_notify_user_info_add_pair(user_info, _("Location"), user->location);
	}

	/* Other information */
	if (user->headline && strlen(user->headline)) {
		purple_notify_user_info_add_pair(user_info, _("Headline"), user->headline);
	}

	str = msim_format_now_playing(user->band_name, user->song_name);
	if (str && strlen(str)) {
		purple_notify_user_info_add_pair(user_info, _("Song"), str);
	}

	/* Note: total friends only available if looked up by uid, not username. */
	if (user->total_friends) {
		purple_notify_user_info_add_pair(user_info, _("Total Friends"),
			g_strdup_printf("%d", user->total_friends));
	}

	if (full) {
		/* Client information */

		str = user->client_info;
		cv = user->client_cv;

		if (str && cv != 0) {
			purple_notify_user_info_add_pair(user_info, _("Client Version"),
					g_strdup_printf("%s (build %d)", str, cv));
		} else if (str) {
			purple_notify_user_info_add_pair(user_info, _("Client Version"),
					g_strdup(str));
		} else if (cv) {
			purple_notify_user_info_add_pair(user_info, _("Client Version"),
					g_strdup_printf("Build %d", cv));
		}
	}
}

/** Store a field of information about a buddy. */
void 
msim_store_user_info_each(const gchar *key_str, gchar *value_str, MsimUser *user)
{
	if (g_str_equal(key_str, "UserID") || g_str_equal(key_str, "ContactID")) {
		/* Save to buddy list, if it exists, for quick cached uid lookup with msim_uid2username_from_blist(). */
		if (user->buddy)
		{
			purple_debug_info("msim", "associating uid %s with username %s\n", key_str, user->buddy->name);
			purple_blist_node_set_int(&user->buddy->node, "UserID", atol(value_str));
		}
		/* Need to store in MsimUser, too? What if not on blist? */
	} else if (g_str_equal(key_str, "Age")) {
		user->age = atol(value_str);
	} else if (g_str_equal(key_str, "Gender")) {
		user->gender = g_strdup(value_str);
	} else if (g_str_equal(key_str, "Location")) {
		user->location = g_strdup(value_str);
	} else if (g_str_equal(key_str, "TotalFriends")) {
		user->total_friends = atol(value_str);
	} else if (g_str_equal(key_str, "DisplayName")) {
		user->display_name = g_strdup(value_str);
	} else if (g_str_equal(key_str, "BandName")) {
		user->band_name = g_strdup(value_str);
	} else if (g_str_equal(key_str, "SongName")) {
		user->song_name = g_strdup(value_str);
	} else if (g_str_equal(key_str, "UserName") || g_str_equal(key_str, "IMName") || g_str_equal(key_str, "NickName")) {
		/* Ignore because PurpleBuddy knows this already */
		;
	} else if (g_str_equal(key_str, "ImageURL") || g_str_equal(key_str, "AvatarURL")) {
		const gchar *previous_url;

		user->image_url = g_strdup(value_str);

		/* Instead of showing 'no photo' picture, show nothing. */
		if (g_str_equal(user->image_url, "http://x.myspace.com/images/no_pic.gif"))
		{
			purple_buddy_icons_set_for_user(user->buddy->account,
				user->buddy->name,
				NULL, 0, NULL);
			return;
		}
	
		/* TODO: use ETag for checksum */
		previous_url = purple_buddy_icons_get_checksum_for_user(user->buddy);

		/* Only download if URL changed */
		if (!previous_url || !g_str_equal(previous_url, user->image_url)) {
			purple_util_fetch_url(user->image_url, TRUE, NULL, TRUE, msim_downloaded_buddy_icon, (gpointer)user);
		}
	} else if (g_str_equal(key_str, "LastImageUpdated")) {
		/* TODO: use somewhere */
		user->last_image_updated = atol(value_str);
	} else if (g_str_equal(key_str, "Headline")) {
		user->headline = g_strdup(value_str);
	} else {
		/* TODO: other fields in MsimUser */
		gchar *msg;

		msg = g_strdup_printf("msim_store_user_info_each: unknown field %s=%s",
				key_str, value_str);

		msim_unrecognized(NULL, NULL, msg);

		g_free(msg);
	}
}

/** Save buddy information to the buddy list from a user info reply message.
 *
 * @param session
 * @param msg The user information reply, with any amount of information.
 * @param user The structure to save to, or NULL to save in PurpleBuddy->proto_data.
 *
 * Variable information is saved to the passed MsimUser structure. Permanent
 * information (UserID) is stored in the blist node of the buddy list (and
 * ends up in blist.xml, persisted to disk) if it exists.
 *
 * If the function has no buddy information, this function
 * is a no-op (and returns FALSE).
 *
 */
gboolean 
msim_store_user_info(MsimSession *session, MsimMessage *msg, MsimUser *user)
{
	gchar *username;
	MsimMessage *body, *body_node;

	g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
	g_return_val_if_fail(msg != NULL, FALSE);

	body = msim_msg_get_dictionary(msg, "body");
	if (!body) {
		return FALSE;
	}

	username = msim_msg_get_string(body, "UserName");

	if (!username) {
		purple_debug_info("msim", 
			"msim_process_reply: not caching body, no UserName\n");
		msim_msg_free(body);
		g_free(username);
		return FALSE;
	}
	
	/* Null user = find and store in PurpleBuddy's proto_data */
	if (!user) {
		user = msim_find_user(session, username);
		if (!user) {
			msim_msg_free(body);
			g_free(username);
			return FALSE;
		}
	}

	/* TODO: make looping over MsimMessage's easier. */
	for (body_node = body; 
		body_node != NULL; 
		body_node = msim_msg_get_next_element_node(body_node))
	{
		const gchar *key_str;
		gchar *value_str;
		MsimMessageElement *elem;

		elem = (MsimMessageElement *)body_node->data;
		key_str = elem->name;

		value_str = msim_msg_get_string_from_element(elem);
		msim_store_user_info_each(key_str, value_str, user);
		g_free(value_str);
	}

	if (msim_msg_get_integer(msg, "dsn") == MG_OWN_IM_INFO_DSN &&
		msim_msg_get_integer(msg, "lid") == MG_OWN_IM_INFO_LID) {
		/* TODO: do something with our own IM info, if we need it for some
		 * specific purpose. Otherwise it is available on the buddy list,
		 * if the user has themselves as their own buddy. 
		 *
		 * However, much of the info is already available in MsimSession,
		 * stored in msim_we_are_logged_on(). */
	} else if (msim_msg_get_integer(msg, "dsn") == MG_OWN_MYSPACE_INFO_DSN &&
			msim_msg_get_integer(msg, "lid") == MG_OWN_MYSPACE_INFO_LID) {
		/* TODO: same as above, but for MySpace info. */
	}

	msim_msg_free(body);

	return TRUE;
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
void 
msim_lookup_user(MsimSession *session, const gchar *user, MSIM_USER_LOOKUP_CB cb, gpointer data)
{
	MsimMessage *body;
	gchar *field_name;
	guint rid, cmd, dsn, lid;

	g_return_if_fail(MSIM_SESSION_VALID(session));
	g_return_if_fail(user != NULL);
	/* Callback can be null to not call anything, just lookup & store information. */
	/*g_return_if_fail(cb != NULL);*/

	purple_debug_info("msim", "msim_lookup_userid: "
			"asynchronously looking up <%s>\n", user);

	msim_msg_dump("msim_lookup_user: data=%s\n", (MsimMessage *)data);

	/* Setup callback. Response will be associated with request using 'rid'. */
	rid = msim_new_reply_callback(session, cb, data);

	/* Send request */

	cmd = MSIM_CMD_GET;

	if (msim_is_userid(user)) {
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

	body = msim_msg_new(
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
 * Check if a string is a userid (all numeric).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is userid, FALSE if not.
 */
gboolean 
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
gboolean 
msim_is_email(const gchar *user)
{
	g_return_val_if_fail(user != NULL, FALSE);

	return strchr(user, '@') != NULL;
}


/** Callback for when a buddy icon finished being downloaded. */
static void
msim_downloaded_buddy_icon(PurpleUtilFetchUrlData *url_data,
		gpointer user_data,
		const gchar *url_text,
		gsize len,
		const gchar *error_message)
{
	MsimUser *user;

	user = (MsimUser *)user_data;

	purple_debug_info("msim_downloaded_buddy_icon",
			"Downloaded %d bytes\n", len);

	if (!url_text) {
		purple_debug_info("msim_downloaded_buddy_icon",
				"failed to download icon for %s",
				user->buddy->name);
		return;
	}

	purple_buddy_icons_set_for_user(user->buddy->account,
			user->buddy->name,
			g_memdup((gchar *)url_text, len), len, 
			/* Use URL itself as buddy icon "checksum" (TODO: ETag) */
			user->image_url);		/* checksum */
}


