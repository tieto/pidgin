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

#include <string.h>

#include "whiteboard.h"
#include "prpl.h"

/******************************************************************************
 * Globals
 *****************************************************************************/
static GaimWhiteboardUiOps *whiteboard_ui_ops = NULL;
/* static GaimWhiteboardPrplOps *whiteboard_prpl_ops = NULL; */

static GList *wbList = NULL;

/*static gboolean auto_accept = TRUE; */

/******************************************************************************
 * API
 *****************************************************************************/
void gaim_whiteboard_set_ui_ops(GaimWhiteboardUiOps *ops)
{
	whiteboard_ui_ops = ops;
}

void gaim_whiteboard_set_prpl_ops(GaimWhiteboard *wb, GaimWhiteboardPrplOps *ops)
{
	wb->prpl_ops = ops;
}

GaimWhiteboard *gaim_whiteboard_create(GaimAccount *account, const char *who, int state)
{
	GaimPluginProtocolInfo *prpl_info;
	GaimWhiteboard *wb = g_new0(GaimWhiteboard, 1);

	wb->account = account;
	wb->state   = state;
	wb->who     = g_strdup(who);

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);
	gaim_whiteboard_set_prpl_ops(wb, prpl_info->whiteboard_prpl_ops);

	/* Start up protocol specifics */
	if(wb->prpl_ops && wb->prpl_ops->start)
		wb->prpl_ops->start(wb);

	wbList = g_list_append(wbList, wb);

	return wb;
}

void gaim_whiteboard_destroy(GaimWhiteboard *wb)
{
	g_return_if_fail(wb != NULL);

	if(wb->ui_data)
	{
		/* Destroy frontend */
		if(whiteboard_ui_ops && whiteboard_ui_ops->destroy)
			whiteboard_ui_ops->destroy(wb);
	}

	/* Do protocol specific session ending procedures */
	if(wb->prpl_ops && wb->prpl_ops->end)
		wb->prpl_ops->end(wb);

	g_free(wb->who);
	wbList = g_list_remove(wbList, wb);
	g_free(wb);
}

void gaim_whiteboard_start(GaimWhiteboard *wb)
{
	/* Create frontend for whiteboard */
	if(whiteboard_ui_ops && whiteboard_ui_ops->create)
		whiteboard_ui_ops->create(wb);
}

/* Looks through the list of whiteboard sessions for one that is between
 * usernames 'me' and 'who'.  Returns a pointer to a matching whiteboard
 * session; if none match, it returns NULL.
 */
GaimWhiteboard *gaim_whiteboard_get_session(const GaimAccount *account, const char *who)
{
	GaimWhiteboard *wb;

	GList *l = wbList;

	/* Look for a whiteboard session between the local user and the remote user
	 */
	while(l != NULL)
	{
		wb = l->data;

		if(wb->account == account && !strcmp(wb->who, who))
			return wb;

		l = l->next;
	}

	return NULL;
}

void gaim_whiteboard_draw_list_destroy(GList *draw_list)
{
	g_list_free(draw_list);
}

gboolean gaim_whiteboard_get_dimensions(const GaimWhiteboard *wb, int *width, int *height)
{
	GaimWhiteboardPrplOps *prpl_ops = wb->prpl_ops;

	if (prpl_ops && prpl_ops->get_dimensions)
	{
		prpl_ops->get_dimensions(wb, width, height);
		return TRUE;
	}

	return FALSE;
}

void gaim_whiteboard_set_dimensions(GaimWhiteboard *wb, int width, int height)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->set_dimensions)
		whiteboard_ui_ops->set_dimensions(wb, width, height);
}

void gaim_whiteboard_send_draw_list(GaimWhiteboard *wb, GList *list)
{
	GaimWhiteboardPrplOps *prpl_ops = wb->prpl_ops;

	if (prpl_ops && prpl_ops->send_draw_list)
		prpl_ops->send_draw_list(wb, list);
}

void gaim_whiteboard_draw_point(GaimWhiteboard *wb, int x, int y, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_point)
		whiteboard_ui_ops->draw_point(wb, x, y, color, size);
}

void gaim_whiteboard_draw_line(GaimWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_line)
		whiteboard_ui_ops->draw_line(wb, x1, y1, x2, y2, color, size);
}

void gaim_whiteboard_clear(GaimWhiteboard *wb)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->clear)
		whiteboard_ui_ops->clear(wb);
}

void gaim_whiteboard_send_clear(GaimWhiteboard *wb)
{
	GaimWhiteboardPrplOps *prpl_ops = wb->prpl_ops;

	if (prpl_ops && prpl_ops->clear)
		prpl_ops->clear(wb);
}

void gaim_whiteboard_send_brush(GaimWhiteboard *wb, int size, int color)
{
	GaimWhiteboardPrplOps *prpl_ops = wb->prpl_ops;

	if (prpl_ops && prpl_ops->set_brush)
		prpl_ops->set_brush(wb, size, color);
}

gboolean gaim_whiteboard_get_brush(const GaimWhiteboard *wb, int *size, int *color)
{
	GaimWhiteboardPrplOps *prpl_ops = wb->prpl_ops;

	if (prpl_ops && prpl_ops->get_brush)
	{
		prpl_ops->get_brush(wb, size, color);
		return TRUE;
	}
	return FALSE;
}

void gaim_whiteboard_set_brush(GaimWhiteboard *wb, int size, int color)
{
	if (whiteboard_ui_ops && whiteboard_ui_ops->set_brush)
		whiteboard_ui_ops->set_brush(wb, size, color);
}

