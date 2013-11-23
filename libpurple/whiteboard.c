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
#include "glibcompat.h"
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

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_STATE,
	PROP_ACCOUNT,
	PROP_WHO,
	PROP_DRAW_LIST,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

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

	g_object_notify_by_pspec(G_OBJECT(wb), properties[PROP_STATE]);
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

	g_object_notify_by_pspec(G_OBJECT(wb), properties[PROP_DRAW_LIST]);
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
	g_return_if_fail(PURPLE_IS_WHITEBOARD(wb));

	wb->ui_data = ui_data;
}

gpointer purple_whiteboard_get_ui_data(const PurpleWhiteboard *wb)
{
	g_return_val_if_fail(PURPLE_IS_WHITEBOARD(wb), NULL);

	return wb->ui_data;
}

/******************************************************************************
 * GObject code
 *****************************************************************************/
/* Set method for GObject properties */
static void
purple_whiteboard_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(obj);
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	switch (param_id) {
		case PROP_STATE:
			purple_whiteboard_set_state(wb, g_value_get_int(value));
			break;
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_WHO:
			priv->who = g_strdup(g_value_get_string(value));
			break;
		case PROP_DRAW_LIST:
			purple_whiteboard_set_draw_list(wb, g_value_get_pointer(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_whiteboard_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(obj);

	switch (param_id) {
		case PROP_STATE:
			g_value_set_int(value, purple_whiteboard_get_state(wb));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_whiteboard_get_account(wb));
			break;
		case PROP_WHO:
			g_value_set_string(value, purple_whiteboard_get_who(wb));
			break;
		case PROP_DRAW_LIST:
			g_value_set_pointer(value, purple_whiteboard_get_draw_list(wb));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_whiteboard_constructed(GObject *object)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(object);
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);
	PurplePluginProtocolInfo *prpl_info;

	parent_class->constructed(object);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(
				purple_account_get_connection(priv->account)));
	purple_whiteboard_set_prpl_ops(wb, prpl_info->whiteboard_prpl_ops);

	/* Start up protocol specifics */
	if(priv->prpl_ops && priv->prpl_ops->start)
		priv->prpl_ops->start(wb);

	wbList = g_list_append(wbList, wb);
}

/* GObject finalize function */
static void
purple_whiteboard_finalize(GObject *object)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(object);
	PurpleWhiteboardPrivate *priv = PURPLE_WHITEBOARD_GET_PRIVATE(wb);

	if(wb->ui_data)
	{
		/* Destroy frontend */
		if(whiteboard_ui_ops && whiteboard_ui_ops->destroy)
			whiteboard_ui_ops->destroy(wb);
	}

	/* Do protocol specific session ending procedures */
	if(priv->prpl_ops && priv->prpl_ops->end)
		priv->prpl_ops->end(wb);

	wbList = g_list_remove(wbList, wb);

	g_free(priv->who);

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_whiteboard_class_init(PurpleWhiteboardClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_whiteboard_finalize;
	obj_class->constructed = purple_whiteboard_constructed;

	/* Setup properties */
	obj_class->get_property = purple_whiteboard_get_property;
	obj_class->set_property = purple_whiteboard_set_property;

	properties[PROP_STATE] = g_param_spec_int("state", "State",
				"State of the whiteboard.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(obj_class, PROP_STATE,
				properties[PROP_STATE]);

	properties[PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The whiteboard's account.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(obj_class, PROP_ACCOUNT,
				properties[PROP_ACCOUNT]);

	properties[PROP_WHO] = g_param_spec_string("who", "Who",
				"Who you're drawing with.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(obj_class, PROP_WHO,
				properties[PROP_WHO]);

	properties[PROP_DRAW_LIST] = g_param_spec_pointer("draw-list", "Draw list",
				"A list of points to draw to the buddy.",
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(obj_class, PROP_DRAW_LIST,
				properties[PROP_DRAW_LIST]);

	g_type_class_add_private(klass, sizeof(PurpleWhiteboardPrivate));
}

GType
purple_whiteboard_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleWhiteboardClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_whiteboard_class_init,
			NULL,
			NULL,
			sizeof(PurpleWhiteboard),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleWhiteboard",
				&info, 0);
	}

	return type;
}

PurpleWhiteboard *purple_whiteboard_new(PurpleAccount *account, const char *who, int state)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(who != NULL, NULL);

	return g_object_new(PURPLE_TYPE_WHITEBOARD,
		"account", account,
		"who",     who,
		"state",   state,
		NULL
	);
}
