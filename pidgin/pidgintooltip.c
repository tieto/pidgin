/**
 * @file pidgintooltip.c Pidgin Tooltip API
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 */

#include "internal.h"
#include "prefs.h"
#include "pidgin.h"
#include "pidgintooltip.h"
#include "debug.h"

static gboolean enable_tooltips;
static int tooltip_delay = -1;

struct
{
	GtkWidget *widget;
	int timeout;
	GdkRectangle tip_rect;
	GtkWidget *tipwindow;
	PidginTooltipPaint paint_tooltip;
} pidgin_tooltip;

typedef struct
{
	GtkWidget *widget;
	gpointer userdata;
	PidginTooltipPaint paint_tooltip;
	union {
		struct {
			PidginTooltipCreateForTree create_tooltip;
			GtkTreePath *path;
		} treeview;
		struct {
			PidginTooltipCreate create_tooltip;
		} widget;
	} common;
} PidginTooltipData;

static void
initialize_tooltip_delay()
{
#if GTK_CHECK_VERSION(2,14,0)
	GtkSettings *settings;
#endif

	if (tooltip_delay != -1)
		return;

#if GTK_CHECK_VERSION(2,14,0)
	settings = gtk_settings_get_default();

	g_object_get(settings, "gtk-enable-tooltips", &enable_tooltips, NULL);
	g_object_get(settings, "gtk-tooltip-timeout", &tooltip_delay, NULL);
#else
	tooltip_delay = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/tooltip_delay");
	enable_tooltips = (tooltip_delay != 0);
#endif
}

static void
destroy_tooltip_data(PidginTooltipData *data)
{
	gtk_tree_path_free(data->common.treeview.path);
	pidgin_tooltip_destroy();
	g_free(data);
}

void pidgin_tooltip_destroy()
{
	if (pidgin_tooltip.timeout > 0) {
		g_source_remove(pidgin_tooltip.timeout);
		pidgin_tooltip.timeout = 0;
	}
	if (pidgin_tooltip.tipwindow) {
		gtk_widget_destroy(pidgin_tooltip.tipwindow);
		pidgin_tooltip.tipwindow = NULL;
	}
}

static gboolean
pidgin_tooltip_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	if (pidgin_tooltip.paint_tooltip) {
		gtk_paint_flat_box(widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
				NULL, widget, "tooltip", 0, 0, -1, -1);
		pidgin_tooltip.paint_tooltip(widget, data);
	}
	return FALSE;
}

static GtkWidget*
setup_tooltip_window(void)
{
	const char *name;
	GtkWidget *tipwindow;

	tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
	name = gtk_window_get_title(GTK_WINDOW(pidgin_tooltip.widget));
#if GTK_CHECK_VERSION(2,10,0)
	gtk_window_set_type_hint(GTK_WINDOW(tipwindow), GDK_WINDOW_TYPE_HINT_TOOLTIP);
#endif
	gtk_widget_set_app_paintable(tipwindow, TRUE);
	gtk_window_set_title(GTK_WINDOW(tipwindow), name ? name : _("Pidgin Tooltip"));
	gtk_window_set_resizable(GTK_WINDOW(tipwindow), FALSE);
	gtk_widget_set_name(tipwindow, "gtk-tooltips");
	gtk_widget_ensure_style(tipwindow);
	gtk_widget_realize(tipwindow);
	return tipwindow;
}

static void
setup_tooltip_window_position(gpointer data, int w, int h)
{
	int sig;
	int scr_w, scr_h, x, y;
#if GTK_CHECK_VERSION(2,2,0)
	int mon_num;
	GdkScreen *screen = NULL;
#endif
	GdkRectangle mon_size;
	GtkWidget *tipwindow = pidgin_tooltip.tipwindow;

#if GTK_CHECK_VERSION(2,2,0)
	gdk_display_get_pointer(gdk_display_get_default(), &screen, &x, &y, NULL);
	mon_num = gdk_screen_get_monitor_at_point(screen, x, y);
	gdk_screen_get_monitor_geometry(screen, mon_num, &mon_size);

	scr_w = mon_size.width + mon_size.x;
	scr_h = mon_size.height + mon_size.y;
#else
	scr_w = gdk_screen_width();
	scr_h = gdk_screen_height();
	gdk_window_get_pointer(NULL, &x, &y, NULL);
	mon_size.x = 0;
	mon_size.y = 0;
#endif

#if GTK_CHECK_VERSION(2,2,0)
	if (w > mon_size.width)
		w = mon_size.width - 10;

	if (h > mon_size.height)
		h = mon_size.height - 10;
#endif
	x -= ((w >> 1) + 4);

	if ((y + h + 4) > scr_h)
		y = y - h - 5;
	else
		y = y + 6;

	if (y < mon_size.y)
		y = mon_size.y;

	if (y != mon_size.y) {
		if ((x + w) > scr_w)
			x -= (x + w + 5) - scr_w;
		else if (x < mon_size.x)
			x = mon_size.x;
	} else {
		x -= (w / 2 + 10);
		if (x < mon_size.x)
			x = mon_size.x;
	}

	gtk_widget_set_size_request(tipwindow, w, h);
	gtk_window_move(GTK_WINDOW(tipwindow), x, y);
	gtk_widget_show(tipwindow);

	g_signal_connect(G_OBJECT(tipwindow), "expose_event",
			G_CALLBACK(pidgin_tooltip_expose_event), data);

	/* Hide the tooltip when the widget is destroyed */
	sig = g_signal_connect(G_OBJECT(pidgin_tooltip.widget), "destroy", G_CALLBACK(pidgin_tooltip_destroy), NULL);
	g_signal_connect_swapped(G_OBJECT(tipwindow), "destroy", G_CALLBACK(g_source_remove), GINT_TO_POINTER(sig));
}

void pidgin_tooltip_show(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_tooltip, PidginTooltipPaint paint_tooltip)
{
	GtkWidget *tipwindow;
	int w, h;

	pidgin_tooltip_destroy();

	pidgin_tooltip.widget = gtk_widget_get_toplevel(widget);
	pidgin_tooltip.tipwindow = tipwindow = setup_tooltip_window();
	pidgin_tooltip.paint_tooltip = paint_tooltip;

	if (!create_tooltip(tipwindow, userdata, &w, &h)) {
		pidgin_tooltip_destroy();
		return;
	}
	setup_tooltip_window_position(userdata, w, h);
}

static void
reset_data_treepath(PidginTooltipData *data)
{
	gtk_tree_path_free(data->common.treeview.path);
	data->common.treeview.path = NULL;
}

static void
pidgin_tooltip_draw(PidginTooltipData *data)
{
	GtkWidget *tipwindow;
	int w, h;

	pidgin_tooltip_destroy();

	pidgin_tooltip.widget = gtk_widget_get_toplevel(data->widget);
	pidgin_tooltip.tipwindow = tipwindow = setup_tooltip_window();
	pidgin_tooltip.paint_tooltip = data->paint_tooltip;

	if (!data->common.widget.create_tooltip(tipwindow, data->userdata, &w, &h)) {
		if (tipwindow == pidgin_tooltip.tipwindow)
			pidgin_tooltip_destroy();
		return;
	}

	setup_tooltip_window_position(data->userdata, w, h);
}

static void
pidgin_tooltip_draw_tree(PidginTooltipData *data)
{
	GtkWidget *tipwindow;
	GtkTreePath *path = NULL;
	int w, h;

	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(data->widget),
				pidgin_tooltip.tip_rect.x,
				pidgin_tooltip.tip_rect.y + (pidgin_tooltip.tip_rect.height/2),
				&path, NULL, NULL, NULL)) {
		pidgin_tooltip_destroy();
		return;
	}

	if (data->common.treeview.path) {
		if (gtk_tree_path_compare(data->common.treeview.path, path) == 0) {
			gtk_tree_path_free(path);
			return;
		}
		gtk_tree_path_free(data->common.treeview.path);
		data->common.treeview.path = NULL;
	}

	pidgin_tooltip_destroy();

	pidgin_tooltip.widget = gtk_widget_get_toplevel(data->widget);
	pidgin_tooltip.tipwindow = tipwindow = setup_tooltip_window();
	pidgin_tooltip.paint_tooltip = data->paint_tooltip;

	if (!data->common.treeview.create_tooltip(tipwindow, path, data->userdata, &w, &h)) {
		if (tipwindow == pidgin_tooltip.tipwindow)
			pidgin_tooltip_destroy();
		gtk_tree_path_free(path);
		return;
	}

	setup_tooltip_window_position(data->userdata, w, h);

	data->common.treeview.path = path;
	g_signal_connect_swapped(G_OBJECT(pidgin_tooltip.tipwindow), "destroy",
			G_CALLBACK(reset_data_treepath), data);
}

static gboolean
pidgin_tooltip_timeout(gpointer data)
{
	PidginTooltipData *tdata = data;
	pidgin_tooltip.timeout = 0;
	if (GTK_IS_TREE_VIEW(tdata->widget))
		pidgin_tooltip_draw_tree(data);
	else
		pidgin_tooltip_draw(data);
	return FALSE;
}

static gboolean
row_motion_cb(GtkWidget *tv, GdkEventMotion *event, gpointer userdata)
{
	GtkTreePath *path;

	if (event->window != gtk_tree_view_get_bin_window(GTK_TREE_VIEW(tv)))
		return FALSE;    /* The cursor is probably on the TreeView's header. */

	initialize_tooltip_delay();
	if (!enable_tooltips)
		return FALSE;

	if (pidgin_tooltip.timeout) {
		if ((event->y >= pidgin_tooltip.tip_rect.y) && ((event->y - pidgin_tooltip.tip_rect.height) <= pidgin_tooltip.tip_rect.y))
			return FALSE;
		/* We've left the cell.  Remove the timeout and create a new one below */
		pidgin_tooltip_destroy();
	}

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL);

	if (path == NULL) {
		pidgin_tooltip_destroy();
		return FALSE;
	}

	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &pidgin_tooltip.tip_rect);
	gtk_tree_path_free(path);

	pidgin_tooltip.timeout = g_timeout_add(tooltip_delay, (GSourceFunc)pidgin_tooltip_timeout, userdata);

	return FALSE;
}

static gboolean
widget_leave_cb(GtkWidget *tv, GdkEvent *event, gpointer userdata)
{
	pidgin_tooltip_destroy();
	return FALSE;
}

gboolean pidgin_tooltip_setup_for_treeview(GtkWidget *tree, gpointer userdata,
		PidginTooltipCreateForTree create_tooltip, PidginTooltipPaint paint_tooltip)
{
	PidginTooltipData *tdata = g_new0(PidginTooltipData, 1);
	tdata->widget = tree;
	tdata->userdata = userdata;
	tdata->common.treeview.create_tooltip = create_tooltip;
	tdata->paint_tooltip = paint_tooltip;

	g_signal_connect(G_OBJECT(tree), "motion-notify-event", G_CALLBACK(row_motion_cb), tdata);
	g_signal_connect(G_OBJECT(tree), "leave-notify-event", G_CALLBACK(widget_leave_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(tree), "destroy", G_CALLBACK(destroy_tooltip_data), tdata);
	return TRUE;
}

static gboolean
widget_motion_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	initialize_tooltip_delay();

	pidgin_tooltip_destroy();
	if (!enable_tooltips)
		return FALSE;

	pidgin_tooltip.timeout = g_timeout_add(tooltip_delay, (GSourceFunc)pidgin_tooltip_timeout, data);
	return FALSE;
}

gboolean pidgin_tooltip_setup_for_widget(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_tooltip, PidginTooltipPaint paint_tooltip)
{
	PidginTooltipData *wdata = g_new0(PidginTooltipData, 1);
	wdata->widget = widget;
	wdata->userdata = userdata;
	wdata->common.widget.create_tooltip = create_tooltip;
	wdata->paint_tooltip = paint_tooltip;

	g_signal_connect(G_OBJECT(widget), "motion-notify-event", G_CALLBACK(widget_motion_cb), wdata);
	g_signal_connect(G_OBJECT(widget), "leave-notify-event", G_CALLBACK(widget_leave_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(widget), "destroy", G_CALLBACK(g_free), wdata);
	return TRUE;
}

