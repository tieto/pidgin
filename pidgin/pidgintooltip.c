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
	PidginTooltipCreateForTree create_tooltip;
	PidginTooltipPaint paint_tooltip;
	GtkTreePath *path;
} PidginTooltipData;

static void
destroy_tooltip_data(PidginTooltipData *data)
{
	gtk_tree_path_free(data->path);
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

static void
pidgin_tooltip_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	if (pidgin_tooltip.paint_tooltip)
		pidgin_tooltip.paint_tooltip(widget, event, data);
}

static void
setup_tooltip_window(gpointer data, int w, int h)
{
	const char *name;
	int sig;
	int scr_w, scr_h, x, y;
#if GTK_CHECK_VERSION(2,2,0)
	int mon_num;
	GdkScreen *screen = NULL;
#endif
	GdkRectangle mon_size;
	GtkWidget *tipwindow = pidgin_tooltip.tipwindow;

	name = gtk_window_get_title(GTK_WINDOW(pidgin_tooltip.widget));
	gtk_widget_set_app_paintable(tipwindow, TRUE);
	gtk_window_set_title(GTK_WINDOW(tipwindow), name ? name : _("Pidgin Tooltip"));
	gtk_window_set_resizable(GTK_WINDOW(tipwindow), FALSE);
	gtk_widget_set_name(tipwindow, "gtk-tooltips");
	g_signal_connect(G_OBJECT(tipwindow), "expose_event",
			G_CALLBACK(pidgin_tooltip_expose_event), data);

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
	pidgin_tooltip.tipwindow = tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_type_hint(GTK_WINDOW(tipwindow), GDK_WINDOW_TYPE_HINT_TOOLTIP);
	pidgin_tooltip.widget = gtk_widget_get_toplevel(widget);
	pidgin_tooltip.paint_tooltip = paint_tooltip;
	gtk_widget_ensure_style(tipwindow);

	if (!create_tooltip(tipwindow, userdata, &w, &h)) {
		pidgin_tooltip_destroy();
		return;
	}
	setup_tooltip_window(userdata, w, h);
}

static void
reset_data_treepath(PidginTooltipData *data)
{
	gtk_tree_path_free(data->path);
	data->path = NULL;
}

static void
pidgin_tooltip_draw(PidginTooltipData *data)
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

	if (data->path) {
		if (gtk_tree_path_compare(data->path, path) == 0) {
			gtk_tree_path_free(path);
			return;
		}
		gtk_tree_path_free(data->path);
		data->path = NULL;
	}

	pidgin_tooltip_destroy();
	pidgin_tooltip.tipwindow = tipwindow = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_type_hint(GTK_WINDOW(tipwindow), GDK_WINDOW_TYPE_HINT_TOOLTIP);
	pidgin_tooltip.widget = gtk_widget_get_toplevel(data->widget);
	pidgin_tooltip.paint_tooltip = data->paint_tooltip;
	gtk_widget_ensure_style(tipwindow);

	if (!data->create_tooltip(tipwindow, path, data->userdata, &w, &h)) {
		pidgin_tooltip_destroy();
		gtk_tree_path_free(path);
		return;
	}

	data->path = path;
	setup_tooltip_window(data->userdata, w, h);
	g_signal_connect_swapped(G_OBJECT(tipwindow), "destroy",
			G_CALLBACK(reset_data_treepath), data);
}

static gboolean
pidgin_tooltip_timeout(gpointer data)
{
	pidgin_tooltip.timeout = 0;
	pidgin_tooltip_draw(data);
	return FALSE;
}

static gboolean
row_motion_cb(GtkWidget *tv, GdkEventMotion *event, gpointer userdata)
{
	GtkTreePath *path;
	int delay;

	/* XXX: probably use something more generic? */
	delay = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/tooltip_delay");
	if (delay == 0)
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

	if (path)
		gtk_tree_path_free(path);
	pidgin_tooltip.timeout = g_timeout_add(delay, (GSourceFunc)pidgin_tooltip_timeout, userdata);

	return FALSE;
}

static gboolean
row_leave_cb(GtkWidget *tv, GdkEvent *event, gpointer userdata)
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
	tdata->create_tooltip = create_tooltip;
	tdata->paint_tooltip = paint_tooltip;

	g_signal_connect(G_OBJECT(tree), "motion-notify-event", G_CALLBACK(row_motion_cb), tdata);
	g_signal_connect(G_OBJECT(tree), "leave-notify-event", G_CALLBACK(row_leave_cb), NULL);
	g_signal_connect_swapped(G_OBJECT(tree), "destroy", G_CALLBACK(destroy_tooltip_data), tdata);
	return TRUE;
}

