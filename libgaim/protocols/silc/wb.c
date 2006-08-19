/*

  wb.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2005 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcincludes.h"
#include "silcclient.h"
#include "silcgaim.h"
#include "wb.h"

/*
  SILC Whiteboard packet:

  1 byte	command
  2 bytes	width
  2 bytes	height
  4 bytes	brush color
  2 bytes	brush size
  n bytes	data 

  Data:

  4 bytes	x
  4 bytes	y

  Commands:

  0x01		draw
  0x02		clear

  MIME:

  MIME-Version: 1.0
  Content-Type: application/x-wb
  Content-Transfer-Encoding: binary

*/

#define SILCGAIM_WB_MIME "MIME-Version: 1.0\r\nContent-Type: application/x-wb\r\nContent-Transfer-Encoding: binary\r\n\r\n"
#define SILCGAIM_WB_HEADER strlen(SILCGAIM_WB_MIME) + 11

#define SILCGAIM_WB_WIDTH 500
#define SILCGAIM_WB_HEIGHT 400
#define SILCGAIM_WB_WIDTH_MAX 1024
#define SILCGAIM_WB_HEIGHT_MAX 1024

/* Commands */
typedef enum {
	SILCGAIM_WB_DRAW 	= 0x01,
	SILCGAIM_WB_CLEAR	= 0x02,
} SilcGaimWbCommand;

/* Brush size */
typedef enum {
	SILCGAIM_WB_BRUSH_SMALL = 2,
	SILCGAIM_WB_BRUSH_MEDIUM = 5,
	SILCGAIM_WB_BRUSH_LARGE = 10,
} SilcGaimWbBrushSize;

/* Brush color (XXX Gaim should provide default colors) */
typedef enum {
	SILCGAIM_WB_COLOR_BLACK		= 0,
	SILCGAIM_WB_COLOR_RED		= 13369344,
	SILCGAIM_WB_COLOR_GREEN		= 52224,
	SILCGAIM_WB_COLOR_BLUE		= 204,
	SILCGAIM_WB_COLOR_YELLOW 	= 15658496,
	SILCGAIM_WB_COLOR_ORANGE	= 16737792,
	SILCGAIM_WB_COLOR_CYAN		= 52428,
	SILCGAIM_WB_COLOR_VIOLET	= 5381277,
	SILCGAIM_WB_COLOR_PURPLE	= 13369548,
	SILCGAIM_WB_COLOR_TAN		= 12093547,
	SILCGAIM_WB_COLOR_BROWN		= 5256485,
	SILCGAIM_WB_COLOR_GREY		= 11184810,
	SILCGAIM_WB_COLOR_WHITE		= 16777215,
} SilcGaimWbColor;

typedef struct {
	int type;		/* 0 = buddy, 1 = channel */
	union {
		SilcClientEntry client;
		SilcChannelEntry channel;
	} u;
	int width;
	int height;
	int brush_size;
	int brush_color;
} *SilcGaimWb;

/* Initialize whiteboard */

GaimWhiteboard *silcgaim_wb_init(SilcGaim sg, SilcClientEntry client_entry)
{
        SilcClientConnection conn;
	GaimWhiteboard *wb;
	SilcGaimWb wbs;

	conn = sg->conn;
	wb = gaim_whiteboard_get_session(sg->account, client_entry->nickname);
	if (!wb)
		wb = gaim_whiteboard_create(sg->account, client_entry->nickname, 0);
	if (!wb)
		return NULL;

	if (!wb->proto_data) {
		wbs = silc_calloc(1, sizeof(*wbs));
		if (!wbs)
			return NULL;
		wbs->type = 0;
		wbs->u.client = client_entry;
		wbs->width = SILCGAIM_WB_WIDTH;
		wbs->height = SILCGAIM_WB_HEIGHT;
		wbs->brush_size = SILCGAIM_WB_BRUSH_SMALL;
		wbs->brush_color = SILCGAIM_WB_COLOR_BLACK;
		wb->proto_data = wbs;

		/* Start the whiteboard */
		gaim_whiteboard_start(wb);
		gaim_whiteboard_clear(wb);
	}

	return wb;
}

GaimWhiteboard *silcgaim_wb_init_ch(SilcGaim sg, SilcChannelEntry channel)
{
	GaimWhiteboard *wb;
	SilcGaimWb wbs;

	wb = gaim_whiteboard_get_session(sg->account, channel->channel_name);
	if (!wb)
		wb = gaim_whiteboard_create(sg->account, channel->channel_name, 0);
	if (!wb)
		return NULL;

	if (!wb->proto_data) {
		wbs = silc_calloc(1, sizeof(*wbs));
		if (!wbs)
			return NULL;
		wbs->type = 1;
		wbs->u.channel = channel;
		wbs->width = SILCGAIM_WB_WIDTH;
		wbs->height = SILCGAIM_WB_HEIGHT;
		wbs->brush_size = SILCGAIM_WB_BRUSH_SMALL;
		wbs->brush_color = SILCGAIM_WB_COLOR_BLACK;
		wb->proto_data = wbs;

		/* Start the whiteboard */
		gaim_whiteboard_start(wb);
		gaim_whiteboard_clear(wb);
	}

	return wb;
}

static void
silcgaim_wb_parse(SilcGaimWb wbs, GaimWhiteboard *wb,
		  unsigned char *message, SilcUInt32 message_len)
{
	SilcUInt8 command;
	SilcUInt16 width, height, brush_size;
	SilcUInt32 brush_color, x, y, dx, dy;
	SilcBufferStruct buf;
	int ret;

	/* Parse the packet */
	silc_buffer_set(&buf, message, message_len);
	ret = silc_buffer_unformat(&buf,
				   SILC_STR_UI_CHAR(&command),
				   SILC_STR_UI_SHORT(&width),
				   SILC_STR_UI_SHORT(&height),
				   SILC_STR_UI_INT(&brush_color),
				   SILC_STR_UI_SHORT(&brush_size),
				   SILC_STR_END);
	if (ret < 0)
		return;
	silc_buffer_pull(&buf, ret);

	/* Update whiteboard if its dimensions changed */
	if (width != wbs->width || height != wbs->height)
		silcgaim_wb_set_dimensions(wb, height, width);

	if (command == SILCGAIM_WB_DRAW) {
		/* Parse data and draw it */
		ret = silc_buffer_unformat(&buf,
					   SILC_STR_UI_INT(&dx),
					   SILC_STR_UI_INT(&dy),
					   SILC_STR_END);
		if (ret < 0)
			return;
		silc_buffer_pull(&buf, 8);
		x = dx;
		y = dy;
		while (buf.len > 0) {
			ret = silc_buffer_unformat(&buf,
						   SILC_STR_UI_INT(&dx),
						   SILC_STR_UI_INT(&dy),
						   SILC_STR_END);
			if (ret < 0)
				return;
			silc_buffer_pull(&buf, 8);

			gaim_whiteboard_draw_line(wb, x, y, x + dx, y + dy,
						  brush_color, brush_size);
			x += dx;
			y += dy;
		}
	}

	if (command == SILCGAIM_WB_CLEAR)
		gaim_whiteboard_clear(wb);
}

typedef struct {
  unsigned char *message;
  SilcUInt32 message_len;
  SilcGaim sg;
  SilcClientEntry sender;
  SilcChannelEntry channel;
} *SilcGaimWbRequest;

static void
silcgaim_wb_request_cb(SilcGaimWbRequest req, gint id)
{
	GaimWhiteboard *wb;

        if (id != 1)
                goto out;

	if (!req->channel)
		wb = silcgaim_wb_init(req->sg, req->sender);
	else
		wb = silcgaim_wb_init_ch(req->sg, req->channel);

	silcgaim_wb_parse(wb->proto_data, wb, req->message, req->message_len);

  out:
	silc_free(req->message);
	silc_free(req);
}

static void
silcgaim_wb_request(SilcClient client, const unsigned char *message, 
		    SilcUInt32 message_len, SilcClientEntry sender, 
		    SilcChannelEntry channel)
{
	char tmp[128];
	SilcGaimWbRequest req;
	GaimConnection *gc;
	SilcGaim sg;

	gc = client->application;
        sg = gc->proto_data;

	/* Open whiteboard automatically if requested */
	if (gaim_account_get_bool(sg->account, "open-wb", FALSE)) {
		GaimWhiteboard *wb;

		if (!channel)
			wb = silcgaim_wb_init(sg, sender);
		else
			wb = silcgaim_wb_init_ch(sg, channel);

		silcgaim_wb_parse(wb->proto_data, wb, (unsigned char *)message,
				  message_len);
		return;
	}

	if (!channel) {
		g_snprintf(tmp, sizeof(tmp),
			_("%s sent message to whiteboard. Would you like "
			  "to open the whiteboard?"), sender->nickname);
	} else {
		g_snprintf(tmp, sizeof(tmp),
			_("%s sent message to whiteboard on %s channel. "
			  "Would you like to open the whiteboard?"),
			sender->nickname, channel->channel_name);
	}

	req = silc_calloc(1, sizeof(*req));
	if (!req)
		return;
	req->message = silc_memdup(message, message_len);
	req->message_len = message_len;
	req->sender = sender;
	req->channel = channel;
	req->sg = sg;

	gaim_request_action(gc, _("Whiteboard"), tmp, NULL, 1, req, 2,
			    _("Yes"), G_CALLBACK(silcgaim_wb_request_cb),
			    _("No"), G_CALLBACK(silcgaim_wb_request_cb));
}

/* Process incoming whiteboard message */

void silcgaim_wb_receive(SilcClient client, SilcClientConnection conn,
			 SilcClientEntry sender, SilcMessagePayload payload,
			 SilcMessageFlags flags, const unsigned char *message,
			 SilcUInt32 message_len)
{
	SilcGaim sg;
        GaimConnection *gc;
	GaimWhiteboard *wb;
	SilcGaimWb wbs;

	gc = client->application;
        sg = gc->proto_data;

	wb = gaim_whiteboard_get_session(sg->account, sender->nickname);
	if (!wb) {
		/* Ask user if they want to open the whiteboard */
		silcgaim_wb_request(client, message, message_len,
				    sender, NULL);
		return;
	}

	wbs = wb->proto_data;
	silcgaim_wb_parse(wbs, wb, (unsigned char *)message, message_len);
}

/* Process incoming whiteboard message on channel */

void silcgaim_wb_receive_ch(SilcClient client, SilcClientConnection conn,
			    SilcClientEntry sender, SilcChannelEntry channel,
			    SilcMessagePayload payload,
			    SilcMessageFlags flags,
			    const unsigned char *message,
			    SilcUInt32 message_len)
{
	SilcGaim sg;
        GaimConnection *gc;
	GaimWhiteboard *wb;
	SilcGaimWb wbs;

	gc = client->application;
        sg = gc->proto_data;

	wb = gaim_whiteboard_get_session(sg->account, channel->channel_name);
	if (!wb) {
		/* Ask user if they want to open the whiteboard */
		silcgaim_wb_request(client, message, message_len,
				    sender, channel);
		return;
	}

	wbs = wb->proto_data;
	silcgaim_wb_parse(wbs, wb, (unsigned char *)message, message_len);
}

/* Send whiteboard message */

void silcgaim_wb_send(GaimWhiteboard *wb, GList *draw_list)
{
	SilcGaimWb wbs = wb->proto_data;
	SilcBuffer packet;
	GList *list;
	int len;
        GaimConnection *gc;
        SilcGaim sg;

	g_return_if_fail(draw_list);
	gc = gaim_account_get_connection(wb->account);
	g_return_if_fail(gc);
 	sg = gc->proto_data;
	g_return_if_fail(sg);

	len = SILCGAIM_WB_HEADER;
	for (list = draw_list; list; list = list->next)
		len += 4;

	packet = silc_buffer_alloc_size(len);
	if (!packet)
		return;

	/* Assmeble packet */
	silc_buffer_format(packet,
			   SILC_STR_UI32_STRING(SILCGAIM_WB_MIME),
			   SILC_STR_UI_CHAR(SILCGAIM_WB_DRAW),
			   SILC_STR_UI_SHORT(wbs->width),
			   SILC_STR_UI_SHORT(wbs->height),
			   SILC_STR_UI_INT(wbs->brush_color),
			   SILC_STR_UI_SHORT(wbs->brush_size),
			   SILC_STR_END);
	silc_buffer_pull(packet, SILCGAIM_WB_HEADER);
	for (list = draw_list; list; list = list->next) {
		silc_buffer_format(packet,
				   SILC_STR_UI_INT(GPOINTER_TO_INT(list->data)),
				   SILC_STR_END);
		silc_buffer_pull(packet, 4);
	}

	/* Send the message */
	if (wbs->type == 0) {
		/* Private message */
		silc_client_send_private_message(sg->client, sg->conn, 
						 wbs->u.client, 
						 SILC_MESSAGE_FLAG_DATA,
						 packet->head, len, TRUE);
	} else if (wbs->type == 1) {
		/* Channel message. Channel private keys are not supported. */
		silc_client_send_channel_message(sg->client, sg->conn,
						 wbs->u.channel, NULL,
						 SILC_MESSAGE_FLAG_DATA,
						 packet->head, len, TRUE);
	}

	silc_buffer_free(packet);
}

/* Gaim Whiteboard operations */

void silcgaim_wb_start(GaimWhiteboard *wb)
{
	/* Nothing here.  Everything is in initialization */
}

void silcgaim_wb_end(GaimWhiteboard *wb)
{
	silc_free(wb->proto_data);
	wb->proto_data = NULL;
}

void silcgaim_wb_get_dimensions(GaimWhiteboard *wb, int *width, int *height)
{
	SilcGaimWb wbs = wb->proto_data;
	*width = wbs->width;
	*height = wbs->height;
}

void silcgaim_wb_set_dimensions(GaimWhiteboard *wb, int width, int height)
{
	SilcGaimWb wbs = wb->proto_data;
	wbs->width = width > SILCGAIM_WB_WIDTH_MAX ? SILCGAIM_WB_WIDTH_MAX :
			width;
	wbs->height = height > SILCGAIM_WB_HEIGHT_MAX ? SILCGAIM_WB_HEIGHT_MAX :
			height;

	/* Update whiteboard */
	gaim_whiteboard_set_dimensions(wb, wbs->width, wbs->height);
}

void silcgaim_wb_get_brush(GaimWhiteboard *wb, int *size, int *color)
{
	SilcGaimWb wbs = wb->proto_data;
	*size = wbs->brush_size;
	*color = wbs->brush_color;
}

void silcgaim_wb_set_brush(GaimWhiteboard *wb, int size, int color)
{
	SilcGaimWb wbs = wb->proto_data;
	wbs->brush_size = size;
	wbs->brush_color = color;

	/* Update whiteboard */
	gaim_whiteboard_set_brush(wb, size, color);
}

void silcgaim_wb_clear(GaimWhiteboard *wb)
{
	SilcGaimWb wbs = wb->proto_data;
	SilcBuffer packet;
	int len;
        GaimConnection *gc;
        SilcGaim sg;

	gc = gaim_account_get_connection(wb->account);
	g_return_if_fail(gc);
 	sg = gc->proto_data;
	g_return_if_fail(sg);

	len = SILCGAIM_WB_HEADER;
	packet = silc_buffer_alloc_size(len);
	if (!packet)
		return;

	/* Assmeble packet */
	silc_buffer_format(packet,
			   SILC_STR_UI32_STRING(SILCGAIM_WB_MIME),
			   SILC_STR_UI_CHAR(SILCGAIM_WB_CLEAR),
			   SILC_STR_UI_SHORT(wbs->width),
			   SILC_STR_UI_SHORT(wbs->height),
			   SILC_STR_UI_INT(wbs->brush_color),
			   SILC_STR_UI_SHORT(wbs->brush_size),
			   SILC_STR_END);

	/* Send the message */
	if (wbs->type == 0) {
		/* Private message */
		silc_client_send_private_message(sg->client, sg->conn, 
						 wbs->u.client, 
						 SILC_MESSAGE_FLAG_DATA,
						 packet->head, len, TRUE);
	} else if (wbs->type == 1) {
		/* Channel message */
		silc_client_send_channel_message(sg->client, sg->conn,
						 wbs->u.channel, NULL,
						 SILC_MESSAGE_FLAG_DATA,
						 packet->head, len, TRUE);
	}

	silc_buffer_free(packet);
}
