/*
 *					MXit Protocol libPurple Plugin
 *
 *					-- MXit Forms & Commands --
 *
 *				Andrew Victor	<libpurple@mxit.com>
 *
 *			(C) Copyright 2009	MXit Lifestyle (Pty) Ltd.
 *				<http://www.mxitlifestyle.com>
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


#include "internal.h"
#include <glib/gprintf.h>

#include "purple.h"

#include "protocol.h"
#include "mxit.h"
#include "markup.h"
#include "formcmds.h"

#undef MXIT_DEBUG_COMMANDS

/*
 * the MXit Command identifiers
 */
typedef enum
{
	MXIT_CMD_UNKNOWN = 0,		/* Unknown command */
	MXIT_CMD_CLEAR,				/* Clear (clear) */
	MXIT_CMD_SENDSMS,			/* Send SMS (sendsms) */
	MXIT_CMD_REPLY,				/* Reply (reply) */
	MXIT_CMD_PLATREQ,			/* Platform Request (platreq) */
	MXIT_CMD_SELECTCONTACT,		/* Select Contact (selc) */
	MXIT_CMD_IMAGE				/* Inline image (img) */
} MXitCommandType;


/*
 * object for an inline image request with an URL
 */
struct ii_url_request
{
	struct RXMsgData*	mx;
	char*				url;
};


/*------------------------------------------------------------------------
 * Callback function invoked when an inline image request to a web site completes.
 *
 *  @param url_data
 *  @param user_data		The Markup message object
 *  @param url_text			The data returned from the WAP site
 *  @param len				The length of the data returned
 *  @param error_message	Descriptive error message
 */
static void mxit_cb_ii_returned(PurpleUtilFetchUrlData* url_data, gpointer user_data, const gchar* url_text, gsize len, const gchar* error_message)
{
	struct ii_url_request*	iireq		= (struct ii_url_request*) user_data;
	char*					ii_data;
	int*					intptr		= NULL;
	int						id;

#ifdef	MXIT_DEBUG_COMMANDS
	purple_debug_info(MXIT_PLUGIN_ID, "Inline Image returned from %s\n", iireq->url);
#endif

	if (!url_text) {
		/* no reply from the WAP site */
		purple_debug_error(MXIT_PLUGIN_ID, "Error downloading Inline Image from %s.\n", iireq->url);
		goto done;
	}

	/* lets first see if we dont have the inline image already in cache */
	if (g_hash_table_lookup(iireq->mx->session->iimages, iireq->url)) {
		/* inline image found in the cache, so we just ignore this reply */
		goto done;
	}

	/* make a copy of the data */
	ii_data = g_malloc(len);
	memcpy(ii_data, (const char*) url_text, len);

	/* we now have the inline image, store it in the imagestore */
	id = purple_imgstore_add_with_id(ii_data, len, NULL);

	/* map the inline image id to purple image id */
	intptr = g_malloc(sizeof(int));
	*intptr = id;
	g_hash_table_insert(iireq->mx->session->iimages, iireq->url, intptr);

	iireq->mx->flags |= PURPLE_MESSAGE_IMAGES;

done:
	iireq->mx->img_count--;
	if ((iireq->mx->img_count == 0) && (iireq->mx->converted)) {
		/*
		 * this was the last outstanding emoticon for this message,
		 * so we can now display it to the user.
		 */
		mxit_show_message(iireq->mx);
	}

	g_free(iireq);
}


/*------------------------------------------------------------------------
 * Return the command identifier of this MXit Command.
 *
 *  @param cmd			The MXit command <key,value> map
 *  @return				The MXit command identifier
 */
static MXitCommandType command_type(GHashTable* hash)
{
	char* op;
	char* type;

	op = g_hash_table_lookup(hash, "op");
	if (op) {
		if ( strcmp(op, "cmd") == 0 ) {
			type = g_hash_table_lookup(hash, "type");
			if (type == NULL)								/* no command provided */
				return MXIT_CMD_UNKNOWN;
			else if (strcmp(type, "clear") == 0)			/* clear */
				return MXIT_CMD_CLEAR;
			else if (strcmp(type, "sendsms") == 0)			/* send an SMS */
				return MXIT_CMD_SENDSMS;
			else if (strcmp(type, "reply") == 0)			/* list of options */
				return MXIT_CMD_REPLY;
			else if (strcmp(type, "platreq") == 0)			/* platform request */
				return MXIT_CMD_PLATREQ;
			else if (strcmp(type, "selc") == 0)				/* select contact */
				return MXIT_CMD_SELECTCONTACT;
		}
		else if (strcmp(op, "img") == 0)
				return MXIT_CMD_IMAGE;
	}

	return MXIT_CMD_UNKNOWN;
}


/*------------------------------------------------------------------------
 * Tokenize a MXit Command string into a <key,value> map.
 *
 *  @param cmd			The MXit command string
 *  @return				The <key,value> hash-map, or NULL on error.
 */
static GHashTable* command_tokenize(char* cmd)
{
	GHashTable* hash	= NULL;
	gchar**		parts;
	gchar*		part;
	int			i		= 0;

#ifdef MXIT_DEBUG_COMMANDS
	purple_debug_info(MXIT_PLUGIN_ID, "command: '%s'\n", cmd);
#endif

	/* explode the command into parts */
	parts = g_strsplit(cmd, "|", 0);

	hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	/* now break part into a key & value */
	while ((part = parts[i]) != NULL) {
		char* value;

		value = strchr(parts[i], '=');		/* find start of value */
		if (value != NULL) {
			*value = '\0';
			value++;
		}

#ifdef MXIT_DEBUG_COMMANDS
		purple_debug_info(MXIT_PLUGIN_ID, "  key='%s' value='%s'\n", parts[i], value);
#endif

		g_hash_table_insert(hash, g_strdup(parts[i]), g_strdup(value));

		i++;
	}

	g_strfreev(parts);

	return hash;
}


/*------------------------------------------------------------------------
 * Process a Clear MXit command.
 * [::op=cmd|type=clear|clearmsgscreen=true|auto=true|id=12345:]
 *
 *  @param session			The MXit session object
 *  @param from				The sender of the message.
 */
static void command_clear(struct MXitSession* session, const char* from, GHashTable* hash)
{
	PurpleConversation *conv;
	char* clearmsgscreen;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, from, session->acc);
	if (conv == NULL) {
		purple_debug_error(MXIT_PLUGIN_ID, _( "Conversation with '%s' not found\n" ), from);
		return;
	}

	clearmsgscreen = g_hash_table_lookup(hash, "clearmsgscreen");
	if ( (clearmsgscreen) && (strcmp(clearmsgscreen, "true") == 0) ) {
		/* this is a command to clear the chat screen */
		purple_debug_info(MXIT_PLUGIN_ID, "Clear the screen\n");

		purple_conversation_clear_message_history(conv);			// TODO: This doesn't actually clear the screen.
	}
}


/*------------------------------------------------------------------------
 * Process a Reply MXit command.
 *
 *  @param mx			The received message data object
 *  @param hash			The MXit command <key,value> map
 */
static void command_reply(struct RXMsgData* mx, GHashTable* hash)
{
	char* replymsg;
	char* selmsg;

	selmsg = g_hash_table_lookup(hash, "selmsg");			/* find the selection message */
	replymsg = g_hash_table_lookup(hash, "replymsg");		/* find the reply message */
	if ((selmsg) && (replymsg)) {
		gchar*	seltext = g_markup_escape_text(purple_url_decode(selmsg), -1);

		mxit_add_html_link( mx, purple_url_decode(replymsg), seltext );

		g_free(seltext);
	}
}


/*------------------------------------------------------------------------
 * Process a PlatformRequest MXit command.
 *
 *  @param hash			The MXit command <key,value> map
 *  @param msg			The message to display (as generated so far)
 */
static void command_platformreq(GHashTable* hash, GString* msg)
{
	gchar*	text	= NULL;
	char*	selmsg;
	char*	dest;

	selmsg = g_hash_table_lookup(hash, "selmsg");			/* find the selection message */
	if (selmsg) {
		text = g_markup_escape_text(purple_url_decode(selmsg), -1);
	}

	dest = g_hash_table_lookup(hash, "dest");				/* find the destination */
	if (dest) {
		g_string_append_printf(msg, "<a href=\"%s\">%s</a>", purple_url_decode(dest), (text) ? text : _( "Download" ));		/* add link to display message */
	}

	if (text)
		g_free(text);
}


/*------------------------------------------------------------------------
 * Process an inline image MXit command.
 *
 *  @param mx			The received message data object
 *  @param hash			The MXit command <key,value> map
 *  @param msg			The message to display (as generated so far)
 */
static void command_image(struct RXMsgData* mx, GHashTable* hash, GString* msg)
{
	const char*	img;
	const char*	reply;
	guchar*		rawimg;
	char		link[256];
	gsize		rawimglen;
	int			imgid;

	img = g_hash_table_lookup(hash, "dat");
	if (img) {
		rawimg = purple_base64_decode(img, &rawimglen);
		//purple_util_write_data_to_file_absolute("/tmp/mxitinline.png", (char*) rawimg, rawimglen);
		imgid = purple_imgstore_add_with_id(rawimg, rawimglen, NULL);
		g_snprintf(link, sizeof(link), "<img id=\"%i\">", imgid);
		g_string_append_printf(msg, "%s", link);
		mx->flags |= PURPLE_MESSAGE_IMAGES;
	}
	else {
		img = g_hash_table_lookup(hash, "src");
		if (img) {
			struct ii_url_request*	iireq;

			iireq = g_new0(struct ii_url_request,1);
			iireq->url = g_strdup(purple_url_decode(img));
			iireq->mx = mx;

			g_string_append_printf(msg, "%s%s>", MXIT_II_TAG, iireq->url);
			mx->got_img = TRUE;

			/* lets first see if we dont have the inline image already in cache */
			if (g_hash_table_lookup(mx->session->iimages, iireq->url)) {
				/* inline image found in the cache, so we do not have to request it from the web */
				g_free(iireq);
			}
			else {
				/* send the request for the inline image */
				purple_debug_info(MXIT_PLUGIN_ID, "sending request for inline image '%s'\n", iireq->url);

				/* request the image (reference: "libpurple/util.h") */
				purple_util_fetch_url_request(iireq->url, TRUE, NULL, TRUE, NULL, FALSE, mxit_cb_ii_returned, iireq);
				mx->img_count++;
			}
		}
	}

	/* if this is a clickable image, show a click link */
	reply = g_hash_table_lookup(hash, "replymsg");
	if (reply) {
		g_string_append_printf(msg, "\n");
		mxit_add_html_link(mx, reply, _( "click here" ));
	}
}


/*------------------------------------------------------------------------
 * Process a received MXit Command message.
 *
 *  @param mx				The received message data object
 *  @param message			The message text
 *  @return					The length of the command
 */
int mxit_parse_command(struct RXMsgData* mx, char* message)
{
	GHashTable* hash	= NULL;
	char*		start;
	char*		end;

	/* ensure that this is really a command */
	if ( ( message[0] != ':' ) || ( message[1] != ':' ) ) {
		/* this is not a command */
		return 0;
	}

	start = message + 2;
	end = strstr(start, ":");
	if (end) {
		/* end of a command found */
		*end = '\0';		/* terminate command string */

		hash = command_tokenize(start);			/* break into <key,value> pairs */
		if (hash) {
			MXitCommandType type = command_type(hash);

			switch (type) {
				case MXIT_CMD_CLEAR :
					command_clear(mx->session, mx->from, hash);
					break;
				case MXIT_CMD_REPLY :
					command_reply(mx, hash);
					break;
				case MXIT_CMD_PLATREQ :
					command_platformreq(hash, mx->msg);
					break;
				case MXIT_CMD_IMAGE :
					command_image(mx, hash, mx->msg);
					break;
				default :
					/* command unknown, or not currently supported */
					break;
			}
			g_hash_table_destroy(hash);
		}
		*end = ':';

		return end - message;
	}
	else {
		return 0;
	}
}
