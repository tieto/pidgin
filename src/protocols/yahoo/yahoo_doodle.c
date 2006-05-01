/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 *
 */

/******************************************************************************
 * INCLUDES
 *****************************************************************************/
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "cipher.h"
#include "cmds.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"

#include "yahoo.h"
#include "yahoo_packet.h"
#include "yahoo_friend.h"
#include "yahoochat.h"
#include "ycht.h"
#include "yahoo_auth.h"
#include "yahoo_filexfer.h"
#include "yahoo_picture.h"

#include "whiteboard.h"
#include "yahoo_doodle.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
const int DefaultColorRGB24[] =
{
	DOODLE_COLOR_RED,
	DOODLE_COLOR_ORANGE,
	DOODLE_COLOR_YELLOW,
	DOODLE_COLOR_GREEN,
	DOODLE_COLOR_CYAN,
	DOODLE_COLOR_BLUE,
	DOODLE_COLOR_VIOLET,
	DOODLE_COLOR_PURPLE,
	DOODLE_COLOR_TAN,
	DOODLE_COLOR_BROWN,
	DOODLE_COLOR_BLACK,
	DOODLE_COLOR_GREY,
	DOODLE_COLOR_WHITE
};

/******************************************************************************
 * Functions
 *****************************************************************************/
GaimCmdRet yahoo_doodle_gaim_cmd_start(GaimConversation *conv, const char *cmd, char **args, char **error, void *data)
{
	GaimAccount *account;
	GaimConnection *gc;
	const gchar *name;

	if(*args && args[0])
		return GAIM_CMD_RET_FAILED;

	account = gaim_conversation_get_account(conv);
	gc = gaim_account_get_connection(account);
	name = gaim_conversation_get_name(conv);
	yahoo_doodle_initiate(gc, name);

	/* Write a local message to this conversation showing that a request for a
	 * Doodle session has been made
	 */
	gaim_conv_im_write(GAIM_CONV_IM(conv), "", _("Sent Doodle request."),
					   GAIM_MESSAGE_NICK | GAIM_MESSAGE_RECV, time(NULL));

	return GAIM_CMD_RET_OK;
}

void yahoo_doodle_initiate(GaimConnection *gc, const char *name)
{
	GaimAccount *account;
	char *to = (char*)name;
	GaimWhiteboard *wb;

	g_return_if_fail(gc);
	g_return_if_fail(name);

	account = gaim_connection_get_account(gc);
	wb = gaim_whiteboard_get_session(account, to);

	if(wb == NULL)
	{
		/* Insert this 'session' in the list.  At this point, it's only a
		 * requested session.
		 */
		gaim_whiteboard_create(account, to, DOODLE_STATE_REQUESTING);
	}

	/* NOTE Perhaps some careful handling of remote assumed established
	 * sessions
	 */

	yahoo_doodle_command_send_request(gc, to);
	yahoo_doodle_command_send_ready(gc, to);

}

void yahoo_doodle_process(GaimConnection *gc, const char *me, const char *from,
						  const char *command, const char *message)
{
	if(!command)
		return;

	/* Now check to see what sort of Doodle message it is */
	switch(atoi(command))
	{
		case DOODLE_CMD_REQUEST:
			yahoo_doodle_command_got_request(gc, from);
			break;

		case DOODLE_CMD_READY:
			yahoo_doodle_command_got_ready(gc, from);
			break;

		case DOODLE_CMD_CLEAR:
			yahoo_doodle_command_got_clear(gc, from);
			break;

		case DOODLE_CMD_DRAW:
			yahoo_doodle_command_got_draw(gc, from, message);
			break;

		case DOODLE_CMD_EXTRA:
			yahoo_doodle_command_got_extra(gc, from, message);
			break;

		case DOODLE_CMD_CONFIRM:
			yahoo_doodle_command_got_confirm(gc, from);
			break;
	}
}

void yahoo_doodle_command_got_request(GaimConnection *gc, const char *from)
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	gaim_debug_info("yahoo", "doodle: Got Request (%s)\n", from);

	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	/* If a session with the remote user doesn't exist */
	if(wb == NULL)
	{
		/* Ask user if they wish to accept the request for a doodle session */
		/* TODO Ask local user to start Doodle session with remote user */
		/* NOTE This if/else statement won't work right--must use dialog
		 * results
		 */

		/* char dialog_message[64];
		g_sprintf(dialog_message, "%s is requesting to start a Doodle session with you.", from);

		gaim_notify_message(NULL, GAIM_NOTIFY_MSG_INFO, "Doodle",
		dialog_message, NULL, NULL, NULL);
		*/

		gaim_whiteboard_create(account, from, DOODLE_STATE_REQUESTED);

		yahoo_doodle_command_send_request(gc, from);
	}

	/* TODO Might be required to clear the canvas of an existing doodle
	 * session at this point
	 */
}

void yahoo_doodle_command_got_ready(GaimConnection *gc, const char *from)
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	gaim_debug_info("yahoo", "doodle: Got Ready (%s)\n", from);

	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	if(wb == NULL)
		return;

	if(wb->state == DOODLE_STATE_REQUESTING)
	{
		gaim_whiteboard_start(wb);

		wb->state = DOODLE_STATE_ESTABLISHED;

		yahoo_doodle_command_send_confirm(gc, from);
	}

	if(wb->state == DOODLE_STATE_ESTABLISHED)
	{
		/* TODO Ask whether to save picture too */
		gaim_whiteboard_clear(wb);
	}

	/* NOTE Not sure about this... I am trying to handle if the remote user
	 * already thinks we're in a session with them (when their chat message
	 * contains the doodle;11 imv key)
	 */
	if(wb->state == DOODLE_STATE_REQUESTED)
	{
		/* gaim_whiteboard_start(wb); */
		yahoo_doodle_command_send_request(gc, from);
	}
}

void yahoo_doodle_command_got_draw(GaimConnection *gc, const char *from, const char *message)
{
	GaimAccount *account;
	GaimWhiteboard *wb;
	char **tokens;
	int i;
	GList *d_list = NULL; /* a local list of drawing info */

	g_return_if_fail(message != NULL);

	gaim_debug_info("yahoo", "doodle: Got Draw (%s)\n", from);
	gaim_debug_info("yahoo", "doodle: Draw message: %s\n", message);

	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	if(wb == NULL)
		return;

	/* TODO Functionalize
	 * Convert drawing packet message to an integer list
	 */

	/* Check to see if the message begans and ends with quotes */
	if((message[0] != '\"') || (message[strlen(message) - 1] != '\"'))
		return;

	/* Ignore the inital quotation mark. */
	message += 1;

	tokens = g_strsplit(message, ",", 0);

	/* Traverse and extract all integers divided by commas */
	for (i = 0; tokens[i] != NULL; i++)
	{
		int last = strlen(tokens[i]) - 1;
		if (tokens[i][last] == '"')
			tokens[i][last] = '\0';

		d_list = g_list_prepend(d_list, GINT_TO_POINTER(atoi(tokens[i])));
	}
	d_list = g_list_reverse(d_list);

	g_strfreev(tokens);

	yahoo_doodle_draw_stroke(wb, d_list);

	/* goodle_doodle_session_set_canvas_as_icon(ds); */

	g_list_free(d_list);
}

void yahoo_doodle_command_got_clear(GaimConnection *gc, const char *from)
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	gaim_debug_info("yahoo", "doodle: Got Clear (%s)\n", from);

	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	if(wb == NULL)
		return;

	if(wb->state == DOODLE_STATE_ESTABLISHED)
	{
		/* TODO Ask user whether to save the image before clearing it */

		gaim_whiteboard_clear(wb);
	}
}

void
yahoo_doodle_command_got_extra(GaimConnection *gc, const char *from, const char *message)
{
	gaim_debug_info("yahoo", "doodle: Got Extra (%s)\n", from);

	/* I do not like these 'extra' features, so I'll only handle them in one
	 * way, which is returning them with the command/packet to turn them off
	 */
	yahoo_doodle_command_send_extra(gc, from, DOODLE_EXTRA_NONE);
}

void yahoo_doodle_command_got_confirm(GaimConnection *gc, const char *from)
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	gaim_debug_info("yahoo", "doodle: Got Confirm (%s)\n", from);

	/* Get the doodle session */
	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	if(wb == NULL)
		return;

	/* TODO Combine the following IF's? */

	/* Check if we requested a doodle session */
	if(wb->state == DOODLE_STATE_REQUESTING)
	{
		wb->state = DOODLE_STATE_ESTABLISHED;

		gaim_whiteboard_start(wb);

		yahoo_doodle_command_send_confirm(gc, from);
	}

	/* Check if we accepted a request for a doodle session */
	if(wb->state == DOODLE_STATE_REQUESTED)
	{
		wb->state = DOODLE_STATE_ESTABLISHED;

		gaim_whiteboard_start(wb);
	}
}

void yahoo_doodle_command_got_shutdown(GaimConnection *gc, const char *from)
{
	GaimAccount *account;
	GaimWhiteboard *wb;

	g_return_if_fail(from != NULL);

	gaim_debug_info("yahoo", "doodle: Got Shutdown (%s)\n", from);

	account = gaim_connection_get_account(gc);

	/* Only handle this if local client requested Doodle session (else local
	 * client would have sent one)
	 */
	wb = gaim_whiteboard_get_session(account, from);

	/* TODO Ask if user wants to save picture before the session is closed */

	/* If this session doesn't exist, don't try and kill it */
	if(wb == NULL)
		return;
	else
	{
		gaim_whiteboard_destroy(wb);

		/* yahoo_doodle_command_send_shutdown(gc, from); */
	}
}

static void yahoo_doodle_command_send_generic(const char *type,
											  GaimConnection *gc,
											  const char *to,
											  const char *message,
											  const char *thirteen,
											  const char *sixtythree,
											  const char *sixtyfour)
{
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	gaim_debug_info("yahoo", "doodle: Sent %s (%s)\n", type, to);

	yd = gc->proto_data;

	/* Make and send an acknowledge (ready) Doodle packet */
	pkt = yahoo_packet_new(YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_str(pkt, 49,  "IMVIRONMENT");
	yahoo_packet_hash_str(pkt, 1,    gaim_account_get_username(gc->account));
	yahoo_packet_hash_str(pkt, 14,   message);
	yahoo_packet_hash_str(pkt, 13,   thirteen);
	yahoo_packet_hash_str(pkt, 5,    to);
	yahoo_packet_hash_str(pkt, 63,   sixtythree ? sixtythree : "doodle;11");
	yahoo_packet_hash_str(pkt, 64,   sixtyfour);
	yahoo_packet_hash_str(pkt, 1002, "1");

	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_doodle_command_send_request(GaimConnection *gc, const char *to)
{
	yahoo_doodle_command_send_generic("Request", gc, to, "1", "1", NULL, "1");
}

void yahoo_doodle_command_send_ready(GaimConnection *gc, const char *to)
{
	yahoo_doodle_command_send_generic("Ready", gc, to, "", "0", NULL, "0");
}

void yahoo_doodle_command_send_draw(GaimConnection *gc, const char *to, const char *message)
{
	yahoo_doodle_command_send_generic("Draw", gc, to, message, "3", NULL, "1");
}

void yahoo_doodle_command_send_clear(GaimConnection *gc, const char *to)
{
	yahoo_doodle_command_send_generic("Clear", gc, to, " ", "2", NULL, "1");
}

void yahoo_doodle_command_send_extra(GaimConnection *gc, const char *to, const char *message)
{
	yahoo_doodle_command_send_generic("Extra", gc, to, message, "4", NULL, "1");
}

void yahoo_doodle_command_send_confirm(GaimConnection *gc, const char *to)
{
	yahoo_doodle_command_send_generic("Confirm", gc, to, "1", "5", NULL, "1");
}

void yahoo_doodle_command_send_shutdown(GaimConnection *gc, const char *to)
{
	yahoo_doodle_command_send_generic("Shutdown", gc, to, "", "0", ";0", "0");
}

void yahoo_doodle_start(GaimWhiteboard *wb)
{
	doodle_session *ds = g_new0(doodle_session, 1);

	/* gaim_debug_debug("yahoo", "doodle: yahoo_doodle_start()\n"); */

	/* Set default brush size and color */
	ds->brush_size  = DOODLE_BRUSH_SMALL;
	ds->brush_color = DOODLE_COLOR_RED;

	wb->proto_data = ds;
}

void yahoo_doodle_end(GaimWhiteboard *wb)
{
	GaimConnection *gc = gaim_account_get_connection(wb->account);

	/* g_debug_debug("yahoo", "doodle: yahoo_doodle_end()\n"); */

	yahoo_doodle_command_send_shutdown(gc, wb->who);

	g_free(wb->proto_data);
}

void yahoo_doodle_get_dimensions(GaimWhiteboard *wb, int *width, int *height)
{
	/* standard Doodle canvases are of one size:  368x256 */
	*width = DOODLE_CANVAS_WIDTH;
	*height = DOODLE_CANVAS_HEIGHT;
}

static char *yahoo_doodle_build_draw_string(doodle_session *ds, GList *draw_list)
{
	GString *message;

	g_return_val_if_fail(draw_list != NULL, NULL);

	message = g_string_new("");
	g_string_printf(message, "\"%d,%d", ds->brush_color, ds->brush_size);

	for(; draw_list != NULL; draw_list = draw_list->next)
	{
		g_string_append_printf(message, ",%d", GPOINTER_TO_INT(draw_list->data));
	}
	g_string_append_c(message, '"');

	return g_string_free(message, FALSE);
}

void yahoo_doodle_send_draw_list(GaimWhiteboard *wb, GList *draw_list)
{
	doodle_session *ds = wb->proto_data;
	char *message;

	g_return_if_fail(draw_list != NULL);

	message = yahoo_doodle_build_draw_string(ds, draw_list);
		yahoo_doodle_command_send_draw(wb->account->gc, wb->who, message);
	g_free(message);
}

void yahoo_doodle_clear(GaimWhiteboard *wb)
{
	yahoo_doodle_command_send_clear(wb->account->gc, wb->who);
}


/* Traverse through the list and draw the points and lines */
void yahoo_doodle_draw_stroke(GaimWhiteboard *wb, GList *draw_list)
{
	int brush_color;
	int brush_size;
	int x;
	int y;

	g_return_if_fail(draw_list != NULL);

	brush_color = GPOINTER_TO_INT(draw_list->data);
	draw_list = draw_list->next;
	g_return_if_fail(draw_list != NULL);

	brush_size = GPOINTER_TO_INT(draw_list->data);
	draw_list = draw_list->next;
	g_return_if_fail(draw_list != NULL);

	x = GPOINTER_TO_INT(draw_list->data);
	draw_list = draw_list->next;
	g_return_if_fail(draw_list != NULL);

	y = GPOINTER_TO_INT(draw_list->data);
	draw_list = draw_list->next;
	g_return_if_fail(draw_list != NULL);

	/*
	gaim_debug_debug("yahoo", "doodle: Drawing: color=%d, size=%d, (%d,%d)\n", brush_color, brush_size, x, y);
	*/

	while(draw_list != NULL && draw_list->next != NULL)
	{
		int dx = GPOINTER_TO_INT(draw_list->data);
		int dy = GPOINTER_TO_INT(draw_list->next->data);

		gaim_whiteboard_draw_line(wb,
								  x, y,
								  x + dx, y + dy,
								  brush_color, brush_size);

		x += dx;
		y += dy;

		draw_list = draw_list->next->next;
	}
}

void yahoo_doodle_get_brush(GaimWhiteboard *wb, int *size, int *color)
{
	doodle_session *ds = (doodle_session *)wb->proto_data;
	*size = ds->brush_size;
	*color = ds->brush_color;
}

void yahoo_doodle_set_brush(GaimWhiteboard *wb, int size, int color)
{
	doodle_session *ds = (doodle_session *)wb->proto_data;
	ds->brush_size = size;
	ds->brush_color = color;

	/* Notify the core about the changes */
	gaim_whiteboard_set_brush(wb, size, color);
}

