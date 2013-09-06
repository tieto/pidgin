/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"
#include "whiteboard.h"
#include "prpl.h"

#define PURPLE_WHITEBOARD_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_WHITEBOARD, PurpleWhiteboardPrivate))

/** @copydoc _PurpleWhiteboardPrivate */
typedef struct _PurpleWhiteboardPrivate  PurpleWhiteboardPrivate;

/** Private data for a whiteboard */
struct _PurpleWhiteboardPrivate
{
	int state;                      /**< State of whiteboard session          */

	PurpleAccount *account;         /**< Account associated with this session */
	char *who;                      /**< Name of the remote user              */

	void *proto_data;               /**< Protocol specific data
	                                     TODO Remove this, and use
	                                          protocol-specific subclasses    */
	PurpleWhiteboardPrplOps *prpl_ops; /**< Protocol-plugin operations        */

	GList *draw_list;               /**< List of drawing elements/deltas to
	                                     send                                 */
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static PurpleWhiteboardUiOps *whiteboard_ui_ops = NULL;
/* static PurpleWhiteboardPrplOps *whiteboard_prpl_ops = NULL; */

static GList *wbList = NULL;

/*static gboolean auto_accept = TRUE; */

/******************************************************************************
 * API
 *****************************************************************************/
void purple_whiteboard_set_ui_ops(PurpleWhiteboardUiOps *ops)
{
	whiteboard_ui_ops = ops;
}

void purple_whiteboard_set_prpl_ops(PurpleWhiteboard *wb, PurpleWhiteboardPrplOps *ops)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_if_fail(priv != NULL);

	priv->prpl_ops = ops;
}

PurpleWhiteboard *purple_whiteboard_create(PurpleAccount *account, const char *who, int state)
{
	PurpleWhiteboardPrivate *priv;
	PurplePluginProtocolInfo *prpl_info;
	PurpleWhiteboard *wb;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(who     != NULL, NULL);

	wb = g_new0(PurpleWhiteboard, 1);
	priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	priv->account = account;
	priv->state   = state;
	priv->who     = g_strdup(who);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(
				purple_account_get_connection(account)));
	purple_whiteboard_set_prpl_ops(wb, prpl_info->whiteboard_prpl_ops);

	/* Start up protocol specifics */
	if(priv->prpl_ops && priv->prpl_ops->start)
		priv->prpl_ops->start(wb);

	wbList = g_list_append(wbList, wb);

	return wb;
}

void purple_whiteboard_destroy(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_if_fail(priv != NULL);

	if(wb->ui_data)
	{
		/* Destroy frontend */
		if(whiteboard_ui_ops && whiteboard_ui_ops->destroy)
			whiteboard_ui_ops->destroy(wb);
	}

	/* Do protocol specific session ending procedures */
	if(priv->prpl_ops && priv->prpl_ops->end)
		priv->prpl_ops->end(wb);

	g_free(priv->who);
	wbList = g_list_remove(wbList, wb);
	g_free(wb);
}

PurpleAccount *purple_whiteboard_get_account(const PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

const char *purple_whiteboard_get_who(const PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;	
}

void purple_whiteboard_set_state(PurpleWhiteboard *wb, int state)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_if_fail(priv != NULL);

	priv->state = state;
}

int purple_whiteboard_get_state(const PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->state;
}

void purple_whiteboard_start(PurpleWhiteboard *wb)
{
	/* Create frontend for whiteboard */
	if(whiteboard_ui_ops && whiteboard_ui_ops->create)
		whiteboard_ui_ops->create(wb);
}

/* Looks through the list of whiteboard sessions for one that is between
 * usernames 'me' and 'who'.  Returns a pointer to a matching whiteboard
 * session; if none match, it returns NULL.
 */
PurpleWhiteboard *purple_whiteboard_get_session(const PurpleAccount *account, const char *who)
{
	PurpleWhiteboard *wb;
	PurpleWhiteboardPrivate *priv;

	GList *l = wbList;

	/* Look for a whiteboard session between the local user and the remote user
	 */
	while(l != NULL)
	{
		wb = l->data;
		priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

		if(priv->account == account && purple_strequal(priv->who, who))
			return wb;

		l = l->next;
	}

	return NULL;
}

void purple_whiteboard_draw_list_destroy(GList *draw_list)
{
	g_list_free(draw_list);
}

gboolean purple_whiteboard_get_dimensions(const PurpleWhiteboard *wb, int *width, int *height)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurpleWhiteboardPrplOps *prpl_ops;

	g_return_val_if_fail(priv != NULL, FALSE);

	prpl_ops = priv->prpl_ops;

	if (prpl_ops && prpl_ops->get_dimensions)
	{
		prpl_ops->get_dimensions(wb, width, height);
		return TRUE;
	}

	return FALSE;
}

void purple_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->set_dimensions)
		whiteboard_ui_ops->set_dimensions(wb, width, height);
}

void purple_whiteboard_send_draw_list(PurpleWhiteboard *wb, GList *list)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurpleWhiteboardPrplOps *prpl_ops;

	g_return_if_fail(priv != NULL);

	prpl_ops = priv->prpl_ops;

	if (prpl_ops && prpl_ops->send_draw_list)
		prpl_ops->send_draw_list(wb, list);
}

void purple_whiteboard_draw_point(PurpleWhiteboard *wb, int x, int y, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_point)
		whiteboard_ui_ops->draw_point(wb, x, y, color, size);
}

void purple_whiteboard_draw_line(PurpleWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_line)
		whiteboard_ui_ops->draw_line(wb, x1, y1, x2, y2, color, size);
}

void purple_whiteboard_clear(PurpleWhiteboard *wb)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->clear)
		whiteboard_ui_ops->clear(wb);
}

void purple_whiteboard_send_clear(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurpleWhiteboardPrplOps *prpl_ops;

	g_return_if_fail(priv != NULL);

	prpl_ops = priv->prpl_ops;

	if (prpl_ops && prpl_ops->clear)
		prpl_ops->clear(wb);
}

void purple_whiteboard_send_brush(PurpleWhiteboard *wb, int size, int color)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurpleWhiteboardPrplOps *prpl_ops;

	g_return_if_fail(priv != NULL);

	prpl_ops = priv->prpl_ops;

	if (prpl_ops && prpl_ops->set_brush)
		prpl_ops->set_brush(wb, size, color);
}

gboolean purple_whiteboard_get_brush(const PurpleWhiteboard *wb, int *size, int *color)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurpleWhiteboardPrplOps *prpl_ops;

	g_return_val_if_fail(priv != NULL, FALSE);

	prpl_ops = priv->prpl_ops;

	if (prpl_ops && prpl_ops->get_brush)
	{
		prpl_ops->get_brush(wb, size, color);
		return TRUE;
	}
	return FALSE;
}

void purple_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color)
{
	if (whiteboard_ui_ops && whiteboard_ui_ops->set_brush)
		whiteboard_ui_ops->set_brush(wb, size, color);
}

GList *purple_whiteboard_get_draw_list(const PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->draw_list;
}

void purple_whiteboard_set_draw_list(PurpleWhiteboard *wb, GList* draw_list)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_if_fail(priv != NULL);

	priv->draw_list = draw_list;
}

void purple_whiteboard_set_protocol_data(PurpleWhiteboard *wb, gpointer proto_data)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_if_fail(priv != NULL);

	priv->proto_data = proto_data;
}

gpointer purple_whiteboard_get_protocol_data(const PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void purple_whiteboard_set_ui_data(PurpleWhiteboard *wb, gpointer ui_data)
{
	g_return_if_fail(wb != NULL);

	wb->ui_data = ui_data;
}

gpointer purple_whiteboard_get_ui_data(const PurpleWhiteboard *wb)
{
	g_return_val_if_fail(wb != NULL, NULL);

	return wb->ui_data;
}
