/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02111-1301, USA.
 */

/*
 * GtkTicker Copyright 2000 Syd Logan
 */

#include "gtkticker.h"
#include <gtk/gtk.h>

static void gtk_ticker_compute_offsets (GtkTicker    *ticker);
static void gtk_ticker_class_init    (GtkTickerClass    *klass);
static void gtk_ticker_init          (GtkTicker         *ticker);
static void gtk_ticker_map           (GtkWidget        *widget);
static void gtk_ticker_realize       (GtkWidget        *widget);
static void gtk_ticker_size_request  (GtkWidget        *widget,
		GtkRequisition   *requisition);
static void gtk_ticker_size_allocate (GtkWidget        *widget,
		GtkAllocation    *allocation);
static void gtk_ticker_add_real      (GtkContainer     *container,
		GtkWidget        *widget);
static void gtk_ticker_remove_real   (GtkContainer     *container,
		GtkWidget        *widget);
static void gtk_ticker_forall        (GtkContainer     *container,
		gboolean	       include_internals,
		GtkCallback       callback,
		gpointer          callback_data);
static GType gtk_ticker_child_type   (GtkContainer     *container);


static GtkContainerClass *parent_class = NULL;


GType gtk_ticker_get_type (void)
{
	static GType ticker_type = 0;

	ticker_type = g_type_from_name("GtkTicker");

	if (!ticker_type)
	{
		static const GTypeInfo ticker_info =
		{
			sizeof(GtkTickerClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_ticker_class_init,
			NULL,
			NULL,
			sizeof(GtkTicker),
			0,
			(GInstanceInitFunc) gtk_ticker_init,
			NULL
		};

		ticker_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkTicker",
				&ticker_info, 0);
	}

	/* kludge to re-initialise the class if it's already registered */
	else if (parent_class == NULL) {
		gtk_ticker_class_init((GtkTickerClass *)g_type_class_peek(ticker_type));
	}

	return ticker_type;
}

static void gtk_ticker_finalize(GObject *object) {
	gtk_ticker_stop_scroll(GTK_TICKER(object));

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gtk_ticker_class_init (GtkTickerClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	gobject_class = (GObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	container_class = (GtkContainerClass*) class;

	parent_class = g_type_class_ref (GTK_TYPE_CONTAINER);

	gobject_class->finalize = gtk_ticker_finalize;

	widget_class->map = gtk_ticker_map;
	widget_class->realize = gtk_ticker_realize;
	/*widget_class->size_request = gtk_ticker_size_request;*/
	widget_class->size_allocate = gtk_ticker_size_allocate;

	container_class->add = gtk_ticker_add_real;
	container_class->remove = gtk_ticker_remove_real;
	container_class->forall = gtk_ticker_forall;
	container_class->child_type = gtk_ticker_child_type;
}

static GType gtk_ticker_child_type (GtkContainer *container)
{
	return GTK_TYPE_WIDGET;
}

static void gtk_ticker_init (GtkTicker *ticker)
{
	gtk_widget_set_has_window (GTK_WIDGET (ticker), TRUE);

	ticker->interval = (guint) 200;
	ticker->scootch = (guint) 2;
	ticker->children = NULL;
	ticker->timer = 0;
	ticker->dirty = TRUE;
}

GtkWidget* gtk_ticker_new (void)
{
	return GTK_WIDGET(g_object_new(GTK_TYPE_TICKER, NULL));
}

static void gtk_ticker_put (GtkTicker *ticker, GtkWidget *widget)
{
	GtkTickerChild *child_info;

	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));
	g_return_if_fail (widget != NULL);

	child_info = g_new(GtkTickerChild, 1);
	child_info->widget = widget;
	child_info->x = 0;

	gtk_widget_set_parent(widget, GTK_WIDGET (ticker));

	ticker->children = g_list_append (ticker->children, child_info);

	if (gtk_widget_get_realized (GTK_WIDGET (ticker)))
		gtk_widget_realize (widget);

	if (gtk_widget_get_visible (GTK_WIDGET (ticker)) &&
		gtk_widget_get_visible (widget))
	{
		if (gtk_widget_get_mapped (GTK_WIDGET (ticker)))
			gtk_widget_map (widget);

		gtk_widget_queue_resize (GTK_WIDGET (ticker));
	}
}

void gtk_ticker_set_interval (GtkTicker *ticker, gint interval)
{
	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));

	if ( interval < 0 )
		interval = 200;
	ticker->interval = interval;
}

guint gtk_ticker_get_interval (GtkTicker *ticker)
{
	g_return_val_if_fail (ticker != NULL, -1);
	g_return_val_if_fail (GTK_IS_TICKER (ticker), -1);

	return ticker->interval;
}

void gtk_ticker_set_scootch (GtkTicker *ticker, gint scootch)
{
	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));

	if (scootch <= 0)
		scootch = 2;
	ticker->scootch = scootch;
	ticker->dirty = TRUE;
}

guint gtk_ticker_get_scootch (GtkTicker *ticker )
{
	g_return_val_if_fail (ticker != NULL, -1);
	g_return_val_if_fail (GTK_IS_TICKER (ticker), -1);

	return ticker->scootch;
}

void gtk_ticker_set_spacing (GtkTicker *ticker, gint spacing )
{
	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));

	if ( spacing < 0 )
		spacing = 0;
	ticker->spacing = spacing;
	ticker->dirty = TRUE;
}

static int ticker_timeout(gpointer data)
{
	GtkTicker *ticker = (GtkTicker *) data;

	if (gtk_widget_get_visible (GTK_WIDGET (ticker)))
		gtk_widget_queue_resize (GTK_WIDGET (ticker));

	return( TRUE );
}

void gtk_ticker_start_scroll(GtkTicker *ticker)
{
	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));
	if ( ticker->timer != 0 )
		return;
	ticker->timer = g_timeout_add(ticker->interval, ticker_timeout, ticker);
}

void gtk_ticker_stop_scroll(GtkTicker *ticker)
{
	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER (ticker));
	if ( ticker->timer == 0 )
		return;
	g_source_remove(ticker->timer);
	ticker->timer = 0;
}

guint gtk_ticker_get_spacing (GtkTicker *ticker )
{
	g_return_val_if_fail (ticker != NULL, -1);
	g_return_val_if_fail (GTK_IS_TICKER (ticker), -1);

	return ticker->spacing;
}

static void gtk_ticker_map (GtkWidget *widget)
{
	GtkTicker *ticker;
	GtkTickerChild *child;
	GList *children;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TICKER (widget));

	gtk_widget_set_mapped (widget, TRUE);
	ticker = GTK_TICKER (widget);

	children = ticker->children;
	while (children)
	{
		child = children->data;
		children = children->next;

		if (gtk_widget_get_visible (child->widget) &&
				!gtk_widget_get_mapped (child->widget))
			gtk_widget_map (child->widget);
	}

	gdk_window_show (gtk_widget_get_window (widget));
}

static void gtk_ticker_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkWindow *window;
	GtkStyle *style;
	GtkAllocation allocation;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TICKER (widget));

	gtk_widget_set_realized (widget, TRUE);

	attributes.window_type = GDK_WINDOW_CHILD;
	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	/*attributes.colormap = gtk_widget_get_colormap (widget);*/
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL /*| GDK_WA_COLORMAP*/;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
			&attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	style = gtk_widget_get_style (widget);
	style = gtk_style_attach (style, window);
	gtk_widget_set_style (widget, style);
	gtk_style_set_background (style, window, GTK_STATE_NORMAL);
}

static void gtk_ticker_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkTicker *ticker;
	GtkTickerChild *child;
	GList *children;
	GtkRequisition child_requisition;
	guint border_width;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TICKER (widget));
	g_return_if_fail (requisition != NULL);

	ticker = GTK_TICKER (widget);
	requisition->width = 0;
	requisition->height = 0;

	children = ticker->children;
	while (children)
	{
		child = children->data;
		children = children->next;

		if (gtk_widget_get_visible (child->widget))
		{
			gtk_widget_size_request (child->widget, &child_requisition);

			requisition->height = MAX (requisition->height,
					child_requisition.height);
			requisition->width += child_requisition.width + ticker->spacing;
		}
	}
	if ( requisition->width > ticker->spacing )
		requisition->width -= ticker->spacing;

	border_width = gtk_container_get_border_width (GTK_CONTAINER (ticker));
	requisition->height += border_width * 2;
	requisition->width += border_width * 2;
}

static void gtk_ticker_compute_offsets (GtkTicker *ticker)
{
	GtkAllocation allocation;
	GtkTickerChild *child;
	GtkRequisition child_requisition;
	GList *children;
	guint16 border_width;

	g_return_if_fail (ticker != NULL);
	g_return_if_fail (GTK_IS_TICKER(ticker));

	border_width = gtk_container_get_border_width (GTK_CONTAINER (ticker));

	gtk_widget_get_allocation (GTK_WIDGET (ticker), &allocation);
	ticker->width = allocation.width;
	ticker->total = 0;
	children = ticker->children;
	while (children) {
		child = children->data;

		child->x = 0;
		if (gtk_widget_get_visible (child->widget)) {
			gtk_widget_get_child_requisition (child->widget, &child_requisition);
			child->offset = ticker->total;
			ticker->total +=
				child_requisition.width + border_width + ticker->spacing;
		}
		children = children->next;
	}
	ticker->dirty = FALSE;
}

static void gtk_ticker_size_allocate (GtkWidget *widget,
		GtkAllocation *allocation)
{
	GtkTicker *ticker;
	GtkAllocation a;
	GtkTickerChild *child;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	GList *children;
	guint16 border_width;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_TICKER(widget));
	g_return_if_fail (allocation != NULL);

	ticker = GTK_TICKER (widget);

	gtk_widget_get_allocation (GTK_WIDGET (ticker), &a);
	if ( a.width != ticker->width )
		ticker->dirty = TRUE;

	if ( ticker->dirty == TRUE ) {
		gtk_ticker_compute_offsets( ticker );
	}

	gtk_widget_set_allocation (widget, allocation);
	if (gtk_widget_get_realized (widget))
		gdk_window_move_resize (gtk_widget_get_window (widget),
				allocation->x,
				allocation->y,
				allocation->width,
				allocation->height);

	border_width = gtk_container_get_border_width (GTK_CONTAINER (ticker));

	children = ticker->children;
	while (children)
	{
		child = children->data;
		child->x -= ticker->scootch;

		if (gtk_widget_get_visible (child->widget)) {
			gtk_widget_get_child_requisition (child->widget, &child_requisition);
			child_allocation.width = child_requisition.width;
			child_allocation.x = child->offset + border_width + child->x;
			if ( ( child_allocation.x + child_allocation.width ) < allocation->x  ) {
				if ( ticker->total >=  allocation->width ) {
					child->x += allocation->x + allocation->width + ( ticker->total - ( allocation->x + allocation->width ) );
				}
				else {
					child->x += allocation->x + allocation->width;
				}
			}
			child_allocation.y = border_width;
			child_allocation.height = child_requisition.height;
			gtk_widget_size_allocate (child->widget, &child_allocation);
		}
		children = children->next;
	}
}

void gtk_ticker_add(GtkTicker *ticker, GtkWidget *widget)
{
	gtk_ticker_add_real( GTK_CONTAINER( ticker ), widget );
	ticker->dirty = TRUE;
}

void gtk_ticker_remove(GtkTicker *ticker, GtkWidget *widget)
{
	gtk_ticker_remove_real( GTK_CONTAINER( ticker ), widget );
	ticker->dirty = TRUE;
}

static void gtk_ticker_add_real(GtkContainer *container, GtkWidget *widget)
{
	g_return_if_fail (container != NULL);
	g_return_if_fail (GTK_IS_TICKER (container));
	g_return_if_fail (widget != NULL);

	gtk_ticker_put(GTK_TICKER (container), widget);
}

static void gtk_ticker_remove_real(GtkContainer *container, GtkWidget *widget)
{
	GtkTicker *ticker;
	GtkTickerChild *child;
	GList *children;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GTK_IS_TICKER (container));
	g_return_if_fail (widget != NULL);

	ticker = GTK_TICKER (container);

	children = ticker->children;
	while (children)
	{
		child = children->data;

		if (child->widget == widget)
		{
			gboolean was_visible = gtk_widget_get_visible (widget);

			gtk_widget_unparent (widget);

			ticker->children = g_list_remove_link (ticker->children, children);
			g_list_free (children);
			g_free (child);

			if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
				gtk_widget_queue_resize (GTK_WIDGET (container));

			break;
		}

		children = children->next;
	}
}

static void gtk_ticker_forall (GtkContainer *container,
		gboolean	include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
	GtkTicker *ticker;
	GtkTickerChild *child;
	GList *children;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GTK_IS_TICKER (container));
	g_return_if_fail (callback != NULL);

	ticker = GTK_TICKER (container);

	children = ticker->children;
	while (children)
	{
		child = children->data;
		children = children->next;

		(* callback) (child->widget, callback_data);
	}
}

