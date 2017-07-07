/*
 * Screen Capture - a plugin that allows taking screenshots and sending them
 * to your buddies as inline images.
 *
 * Copyright (C) 2014, Tomasz Wasilczyk <twasilczyk@pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"

#include <gdk/gdkkeysyms.h>

#include "debug.h"
#include "glibcompat.h"
#include "version.h"

#include "gtk3compat.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"
#include "gtkwebviewtoolbar.h"
#include "pidginstock.h"

#define SCRNCAP_SHOOTING_TIMEOUT 500
#define SCRNCAP_DEFAULT_COLOR "#FFFF00000000"

enum
{
	SCRNCAP_RESPONSE_COLOR
};

static gboolean is_shooting = FALSE;
static guint shooting_timeout = 0;
static GtkWidget *current_window = NULL;

static gint crop_origin_x, crop_origin_y;
static gboolean crop_active;
static gint crop_x, crop_y, crop_w, crop_h;

static gint draw_origin_x, draw_origin_y;
static gboolean draw_active;

static GdkRGBA brush_color = {1, 0, 0, 1};
static gint line_width = 2;

/******************************************************************************
 * libpidgin helper functions
 ******************************************************************************/

static inline void
scrncap_conv_set_data(PidginConversation *gtkconv, const gchar *key,
	gpointer value)
{
	g_return_if_fail(gtkconv != NULL);

	g_object_set_data(G_OBJECT(gtkconv->tab_cont), key, value);
}

static inline gpointer
scrncap_conv_get_data(PidginConversation *gtkconv, const gchar *key)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);

	return g_object_get_data(G_OBJECT(gtkconv->tab_cont), key);
}

/******************************************************************************
 * GdkPixbuf helper functions
 ******************************************************************************/

static GdkPixbuf *
scrncap_perform_screenshot(void)
{
	GdkWindow *root;
	gint orig_x, orig_y;

	root = gdk_get_default_root_window();
	gdk_window_get_origin(root, &orig_x, &orig_y);

	return gdk_pixbuf_get_from_window(root, 0, 0,
		gdk_window_get_width(root), gdk_window_get_height(root));
}

static void
scrncap_pixbuf_darken(GdkPixbuf *pixbuf)
{
	guchar *pixels;
	int i, y, width, height, row_width, n_channels, rowstride, pad;

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	n_channels = gdk_pixbuf_get_n_channels(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);

	row_width = width * n_channels;
	pad = rowstride - row_width;
	g_return_if_fail(pad >= 0);

	for (y = 0; y < height; y++) {
		for (i = 0; i < row_width; i++, pixels++)
			*pixels /= 2;
		pixels += pad;
	}
}

static gboolean
scrncap_pixbuf_to_image_cb(const gchar *buf, gsize count, GError **error,
	gpointer data)
{
	PurpleImage *image = *(PurpleImage **)data;

	image = purple_image_new_from_data(buf, count);

	return TRUE;
}

static PurpleImage *
scrncap_pixbuf_to_image(GdkPixbuf *pixbuf)
{
	PurpleImage *image = NULL;
	GError *error = NULL;

	gdk_pixbuf_save_to_callback(pixbuf, scrncap_pixbuf_to_image_cb, &image,
		"png", &error, NULL);

	if (error != NULL) {
		purple_debug_error("screencap", "Failed saving an image: %s",
			error->message);
		g_error_free(error);
		g_object_unref(image);
		return NULL;
	}

	if (purple_image_get_extension(image) == NULL) {
		purple_debug_error("screencap", "Invalid image format");
		g_object_unref(image);
		return NULL;
	}

	return image;
}

/******************************************************************************
 * Draw window
 ******************************************************************************/

static gboolean
scrncap_drawing_area_btnpress(GtkWidget *draw_area, GdkEventButton *event,
	gpointer _unused)
{
	if (draw_active)
		return TRUE;

	draw_origin_x = event->x;
	draw_origin_y = event->y;
	draw_active = TRUE;

	return TRUE;
}

static gboolean
scrncap_drawing_area_btnrelease(GtkWidget *draw_area, GdkEvent *event,
	gpointer _unused)
{
	if (!draw_active)
		return TRUE;

	draw_active = FALSE;

	return TRUE;
}

static gboolean
scrncap_drawing_area_motion(GtkWidget *draw_area, GdkEventButton *event,
	gpointer _cr)
{
	cairo_t *cr = _cr;
	int x, y;
	int redraw_x, redraw_y, redraw_w, redraw_h;

	x = event->x;
	y = event->y;

	if (!draw_active) {
		draw_origin_x = x;
		draw_origin_y = y;
		draw_active = TRUE;
		return FALSE;
	}

	cairo_move_to(cr, draw_origin_x, draw_origin_y);
	cairo_line_to(cr, x, y);
	cairo_set_line_width(cr, line_width);
	cairo_stroke(cr);

	redraw_x = MIN(draw_origin_x, x) - line_width - 1;
	redraw_y = MIN(draw_origin_y, y) - line_width - 1;
	redraw_w = MAX(draw_origin_x, x) - redraw_x + line_width + 1;
	redraw_h = MAX(draw_origin_y, y) - redraw_y + line_width + 1;

	draw_origin_x = x;
	draw_origin_y = y;

	gtk_widget_queue_draw_area(draw_area,
		redraw_x, redraw_y, redraw_w, redraw_h);

	return FALSE;
}

static gboolean
scrncap_drawing_area_enter(GtkWidget *widget, GdkEvent *event,
	GdkCursor *draw_cursor)
{
	GdkWindow *gdkwindow;

	gdkwindow = gtk_widget_get_window(GTK_WIDGET(widget));
	gdk_window_set_cursor(gdkwindow, draw_cursor);

	return FALSE;
}

static gboolean
scrncap_drawing_area_leave(GtkWidget *widget, GdkEvent *event,
	GdkCursor *draw_cursor)
{
	GdkWindow *gdkwindow;

	gdkwindow = gtk_widget_get_window(GTK_WIDGET(widget));
	gdk_window_set_cursor(gdkwindow, NULL);

	return FALSE;
}

static void
scrncap_draw_window_close(GtkWidget *window, gpointer _unused)
{
	if (current_window != window)
		return;

	is_shooting = FALSE;
	current_window = NULL;
}

static gboolean
scrncap_draw_window_paint(GtkWidget *widget, cairo_t *cr, gpointer _surface)
{
	cairo_surface_t *surface = _surface;

	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);

	return FALSE;
}

static void
scrncap_draw_window_response(GtkDialog *draw_window, gint response_id,
	gpointer _webview)
{
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);
	GdkPixbuf *result = NULL;
	PurpleImage *image;
	const gchar *fname_prefix;
	gchar *fname;
	static guint fname_no = 0;

	if (response_id == SCRNCAP_RESPONSE_COLOR)
		return;

	if (response_id == GTK_RESPONSE_OK) {
		cairo_surface_t *surface = g_object_get_data(
			G_OBJECT(draw_window), "surface");
		result = gdk_pixbuf_get_from_surface(surface, 0, 0,
			cairo_image_surface_get_width(surface),
			cairo_image_surface_get_height(surface));
	}

	gtk_widget_destroy(GTK_WIDGET(draw_window));

	if (result == NULL)
		return;

	image = scrncap_pixbuf_to_image(result);

	/* translators: this is the file name prefix,
	 * keep it lowercase and pure ASCII.
	 * Please avoid "_" character, use "-" instead. */
	fname_prefix = _("screenshot-");
	fname = g_strdup_printf("%s%u", fname_prefix, ++fname_no);
	purple_image_set_friendly_filename(image, fname);
	g_free(fname);

	pidgin_webview_insert_image(webview, image);
	g_object_unref(image);
}

static void
scrncap_draw_color_selected(GtkColorButton *button, cairo_t *cr)
{
	gchar *color_str;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &brush_color);
	gdk_cairo_set_source_rgba(cr, &brush_color);

	color_str = gdk_rgba_to_string(&brush_color);
	purple_prefs_set_string("/plugins/gtk/screencap/brush_color", color_str);
	g_free(color_str);
}

static void
scrncap_draw_window(PidginWebView *webview, GdkPixbuf *screen)
{
	GtkDialog *draw_window;
	GtkWidget *drawing_area, *box;
	GtkWidget *scroll_area;
	GtkWidget *color_button;
	int width, height;
	cairo_t *cr;
	cairo_surface_t *surface;
	GdkDisplay *display;
	GdkCursor *draw_cursor;

	is_shooting = TRUE;

	current_window = pidgin_create_dialog(
		_("Insert screenshot"), 0, "insert-screenshot", TRUE);
	draw_window = GTK_DIALOG(current_window);
	gtk_widget_set_size_request(GTK_WIDGET(draw_window), 400, 300);
	gtk_window_set_position(GTK_WINDOW(draw_window), GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(draw_window), "destroy",
		G_CALLBACK(scrncap_draw_window_close), NULL);

	display = gtk_widget_get_display(current_window);
	draw_cursor = gdk_cursor_new_for_display(display, GDK_PENCIL);
	g_object_set_data_full(G_OBJECT(draw_window), "draw-cursor",
		draw_cursor, g_object_unref);

	width = gdk_pixbuf_get_width(screen);
	height = gdk_pixbuf_get_height(screen);

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
	cr = cairo_create(surface);
	g_signal_connect_swapped(G_OBJECT(draw_window), "destroy",
		G_CALLBACK(cairo_destroy), cr);
	g_object_set_data_full(G_OBJECT(draw_window), "surface",
		surface, (GDestroyNotify)cairo_surface_destroy);

	gdk_cairo_set_source_pixbuf(cr, screen, 0, 0);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	g_object_unref(screen);

	drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(drawing_area, width, height);
	g_signal_connect(G_OBJECT(drawing_area), "draw",
		G_CALLBACK(scrncap_draw_window_paint), surface);
	gtk_widget_add_events(drawing_area, GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK |
		GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(drawing_area), "button-press-event",
		G_CALLBACK(scrncap_drawing_area_btnpress), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "button-release-event",
		G_CALLBACK(scrncap_drawing_area_btnrelease), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event",
		G_CALLBACK(scrncap_drawing_area_motion), cr);
	g_signal_connect(G_OBJECT(drawing_area), "enter-notify-event",
		G_CALLBACK(scrncap_drawing_area_enter), draw_cursor);
	g_signal_connect(G_OBJECT(drawing_area), "leave-notify-event",
		G_CALLBACK(scrncap_drawing_area_leave), draw_cursor);

#if GTK_CHECK_VERSION(3,14,0)
	box = drawing_area;
	g_object_set(drawing_area,
		"halign", GTK_ALIGN_CENTER,
		"valign", GTK_ALIGN_CENTER,
		NULL);
#else
	box = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_container_add(GTK_CONTAINER(box), drawing_area);
#endif
	scroll_area = pidgin_make_scrollable(box,
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC,
		GTK_SHADOW_NONE, -1, -1);
	g_object_set(G_OBJECT(scroll_area), "expand", TRUE, NULL);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(
		GTK_DIALOG(draw_window))), scroll_area);

	color_button = gtk_color_button_new();
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_button),
		&brush_color);
	g_signal_connect(G_OBJECT(color_button), "color-set",
		G_CALLBACK(scrncap_draw_color_selected), cr);
	scrncap_draw_color_selected(GTK_COLOR_BUTTON(color_button), cr);

	gtk_dialog_add_action_widget(draw_window, color_button,
		SCRNCAP_RESPONSE_COLOR);
	gtk_dialog_add_button(draw_window, GTK_STOCK_ADD, GTK_RESPONSE_OK);
	gtk_dialog_add_button(draw_window, GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL);
	gtk_dialog_set_default_response(draw_window, GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(draw_window), "response",
		G_CALLBACK(scrncap_draw_window_response), webview);

	gtk_widget_show_all(GTK_WIDGET(draw_window));
}

/******************************************************************************
 * Crop window
 ******************************************************************************/

static void
scrncap_crop_window_close(GtkWidget *window, gpointer _unused)
{
	if (current_window != window)
		return;

	is_shooting = FALSE;
	current_window = NULL;
}

static gboolean
scrncap_crop_window_keypress(GtkWidget *crop_window, GdkEventKey *event,
	gpointer _webview)
{
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);
	guint key = event->keyval;

	if (key == GDK_KEY_Escape) {
		gtk_widget_destroy(crop_window);
		return TRUE;
	}
	if (key == GDK_KEY_Return) {
		GdkPixbuf *screenshot, *subscreen, *result;

		screenshot = g_object_get_data(G_OBJECT(crop_window),
			"screenshot");
		subscreen = gdk_pixbuf_new_subpixbuf(screenshot,
			crop_x, crop_y, crop_w, crop_h);
		result = gdk_pixbuf_copy(subscreen);
		g_object_unref(subscreen);

		gtk_widget_destroy(crop_window);

		scrncap_draw_window(webview, result);

		return TRUE;
	}

	return FALSE;
}

static gboolean
scrncap_crop_window_focusout(GtkWidget *window, GdkEventFocus *event,
	gpointer _unused)
{
	gtk_widget_destroy(window);
	return FALSE;
}

static gboolean
scrncap_crop_window_btnpress(GtkWidget *window, GdkEventButton *event,
	gpointer _unused)
{
	GtkWidget *hint_box;
	GtkImage *selection;
	GtkFixed *cont;

	g_return_val_if_fail(!crop_active, TRUE);

	hint_box = g_object_get_data(G_OBJECT(window), "hint-box");
	if (hint_box) {
		gtk_widget_destroy(hint_box);
		g_object_set_data(G_OBJECT(window), "hint-box", NULL);
	}

	selection = g_object_get_data(G_OBJECT(window), "selection");
	cont = g_object_get_data(G_OBJECT(window), "cont");

	gtk_fixed_move(cont, GTK_WIDGET(selection), -10, -10);
	gtk_image_set_from_pixbuf(selection, NULL);
	gtk_widget_show(GTK_WIDGET(selection));

	crop_origin_x = event->x_root;
	crop_origin_y = event->y_root;
	crop_active = TRUE;

	return TRUE;
}

static gboolean
scrncap_crop_window_btnrelease(GtkWidget *window, GdkEvent *event,
	gpointer _unused)
{
	crop_active = FALSE;

	return TRUE;
}

static gboolean
scrncap_crop_window_motion(GtkWidget *window, GdkEventButton *event,
	gpointer _unused)
{
	GtkFixed *cont;
	GtkImage *selection;
	GdkPixbuf *crop, *screenshot;

	g_return_val_if_fail(crop_active, FALSE);

	selection = g_object_get_data(G_OBJECT(window), "selection");
	cont = g_object_get_data(G_OBJECT(window), "cont");

	crop_x = MIN(crop_origin_x, event->x_root);
	crop_y = MIN(crop_origin_y, event->y_root);
	crop_w = abs(crop_origin_x - event->x_root);
	crop_h = abs(crop_origin_y - event->y_root);
	crop_w = MAX(crop_w, 1);
	crop_h = MAX(crop_h, 1);

	gtk_fixed_move(cont, GTK_WIDGET(selection), crop_x, crop_y);

	screenshot = g_object_get_data(G_OBJECT(window), "screenshot");
	crop = gdk_pixbuf_new_subpixbuf(screenshot,
		crop_x, crop_y, crop_w, crop_h);
	gtk_image_set_from_pixbuf(GTK_IMAGE(selection), crop);
	g_object_unref(crop);

	return FALSE;
}

static void
scrncap_crop_window_realize(GtkWidget *crop_window, gpointer _unused)
{
	GdkWindow *gdkwindow;
	GdkDisplay *display;
	GdkCursor *cursor;

	gdkwindow = gtk_widget_get_window(GTK_WIDGET(crop_window));
	display = gdk_window_get_display(gdkwindow);

	gdk_window_set_events(gdkwindow, gdk_window_get_events(gdkwindow) |
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_BUTTON_MOTION_MASK);

	cursor = gdk_cursor_new_for_display(display, GDK_CROSSHAIR);
	gdk_window_set_cursor(gdkwindow, cursor);
	g_object_unref(cursor);
}

static gboolean
scrncap_do_screenshot_cb(gpointer _webview)
{
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);
	GtkWindow *crop_window;
	GdkPixbuf *screenshot, *screenshot_d;
	int width, height;
	GtkFixed *cont;
	GtkImage *image, *selection;
	GtkWidget *hint;
	gchar *hint_msg;
	GtkRequisition hint_size;
	GtkWidget *hint_box;

	shooting_timeout = 0;
	crop_active = FALSE;

	(void)webview;

	screenshot = scrncap_perform_screenshot();
	g_return_val_if_fail(screenshot != NULL, G_SOURCE_REMOVE);
	width = gdk_pixbuf_get_width(screenshot);
	height = gdk_pixbuf_get_height(screenshot);

	crop_x = crop_y = 0;
	crop_w = width;
	crop_h = height;

	current_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	crop_window = GTK_WINDOW(current_window);
	gtk_window_set_decorated(crop_window, FALSE);
	gtk_window_set_resizable(crop_window, FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(crop_window), width, height);
	gtk_window_fullscreen(crop_window);
	gtk_window_set_keep_above(crop_window, TRUE);

	g_signal_connect(G_OBJECT(crop_window), "realize",
		G_CALLBACK(scrncap_crop_window_realize), NULL);
	g_signal_connect(G_OBJECT(crop_window), "destroy",
		G_CALLBACK(scrncap_crop_window_close), NULL);
	g_signal_connect(G_OBJECT(crop_window), "key-press-event",
		G_CALLBACK(scrncap_crop_window_keypress), webview);
	g_signal_connect(G_OBJECT(crop_window), "focus-out-event",
		G_CALLBACK(scrncap_crop_window_focusout), NULL);
	g_signal_connect(G_OBJECT(crop_window), "button-press-event",
		G_CALLBACK(scrncap_crop_window_btnpress), NULL);
	g_signal_connect(G_OBJECT(crop_window), "button-release-event",
		G_CALLBACK(scrncap_crop_window_btnrelease), NULL);
	g_signal_connect(G_OBJECT(crop_window), "motion-notify-event",
		G_CALLBACK(scrncap_crop_window_motion), NULL);
	g_object_set_data_full(G_OBJECT(crop_window), "screenshot",
		screenshot, g_object_unref);

	cont = GTK_FIXED(gtk_fixed_new());
	g_object_set_data(G_OBJECT(crop_window), "cont", cont);
	gtk_container_add(GTK_CONTAINER(crop_window), GTK_WIDGET(cont));

	screenshot_d = gdk_pixbuf_copy(screenshot);
	scrncap_pixbuf_darken(screenshot_d);
	image = GTK_IMAGE(gtk_image_new_from_pixbuf(screenshot_d));
	g_object_unref(screenshot_d);
	gtk_fixed_put(cont, GTK_WIDGET(image), 0, 0);

	selection = GTK_IMAGE(gtk_image_new_from_pixbuf(NULL));
	gtk_fixed_put(cont, GTK_WIDGET(selection), -10, -10);
	g_object_set_data(G_OBJECT(crop_window), "selection", selection);

	hint = gtk_label_new(NULL);
	hint_msg = g_strdup_printf("<span size='x-large'>%s</span>",
		_("Select the region to send and press Enter button to confirm "
		"or press Escape button to cancel"));
	gtk_label_set_markup(GTK_LABEL(hint), hint_msg);
	g_free(hint_msg);
#if GTK_CHECK_VERSION(3,12,0)
	gtk_widget_set_margin_start(hint, 10);
	gtk_widget_set_margin_end(hint, 10);
#else
	gtk_widget_set_margin_left(hint, 10);
	gtk_widget_set_margin_right(hint, 10);
#endif
	gtk_widget_set_margin_top(hint, 7);
	gtk_widget_set_margin_bottom(hint, 7);
	hint_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(hint_box), hint);
	gtk_widget_get_preferred_size(hint, NULL, &hint_size);
	gtk_fixed_put(cont, hint_box,
		width / 2 - hint_size.width / 2 - 10,
		height / 2 - hint_size.height / 2 - 7);
	g_object_set_data(G_OBJECT(crop_window), "hint-box", hint_box);

	gtk_widget_show_all(GTK_WIDGET(crop_window));
	gtk_widget_hide(GTK_WIDGET(selection));

	return G_SOURCE_REMOVE;
}

static void
scrncap_do_screenshot(GtkAction *action, PidginWebView *webview)
{
	if (current_window) {
		gtk_window_present(GTK_WINDOW(current_window));
		return;
	}
	if (is_shooting)
		return;
	is_shooting = TRUE;

	shooting_timeout = g_timeout_add(SCRNCAP_SHOOTING_TIMEOUT,
		scrncap_do_screenshot_cb, webview);
}

/******************************************************************************
 * PidginConversation setup
 ******************************************************************************/

static void
scrncap_convwin_switch(GtkNotebook *notebook, GtkWidget *page, gint page_num,
	gpointer _win)
{
	PidginConvWindow *win = _win;
	PidginConversation *gtkconv;
	PidginWebView *webview;
	gboolean images_supported;
	GtkAction *action;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	if (gtkconv == NULL)
		return;

	webview = PIDGIN_WEBVIEW(gtkconv->entry);
	action = g_object_get_data(G_OBJECT(win->menu->menubar),
		"insert-screenshot-action");

	g_return_if_fail(action != NULL);

	images_supported = pidgin_webview_get_format_functions(webview) &
		PIDGIN_WEBVIEW_IMAGE;

	gtk_action_set_sensitive(action, images_supported);
}

static void
scrncap_convwin_menu_cb(GtkAction *action, PidginConvWindow *win)
{
	PidginConversation *gtkconv;
	PidginWebView *webview;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	webview = PIDGIN_WEBVIEW(gtkconv->entry);

	scrncap_do_screenshot(action, webview);
}

static void
scrncap_convwin_init(PidginConvWindow *win)
{
	PidginConvWindowMenu *menu = win->menu;
	GtkAction *action;
	GtkWidget *conv_submenu, *conv_insert_image;
	GtkWidget *scrncap_btn_menu;
	gint pos = -1, i;
	GList *children, *it;

	action = g_object_get_data(G_OBJECT(menu->menubar),
		"insert-screenshot-action");
	if (action != NULL)
		return;

	action = gtk_action_new("InsertScreenshot", _("Insert Screens_hot..."),
		NULL, PIDGIN_STOCK_TOOLBAR_INSERT_SCREENSHOT);
	gtk_action_set_is_important(action, TRUE);
	g_object_set_data_full(G_OBJECT(menu->menubar),
		"insert-screenshot-action", action, g_object_unref);
	g_signal_connect(G_OBJECT(action), "activate",
		G_CALLBACK(scrncap_convwin_menu_cb), win);

	conv_insert_image = gtk_ui_manager_get_widget(menu->ui,
		"/Conversation/ConversationMenu/InsertImage");
	g_return_if_fail(conv_insert_image != NULL);
	conv_submenu = gtk_widget_get_parent(conv_insert_image);

	pos = -1;
	children = gtk_container_get_children(GTK_CONTAINER(conv_submenu));
	for (it = children, i = 0; it; it = g_list_next(it), i++) {
		if (it->data == conv_insert_image) {
			pos = i + 1;
			break;
		}
	}
	g_list_free(children);
	g_warn_if_fail(pos >= 0);

	scrncap_btn_menu = gtk_action_create_menu_item(action);
	g_object_set_data(G_OBJECT(menu->menubar), "insert-screenshot-btn",
		scrncap_btn_menu);
	gtk_menu_shell_insert(GTK_MENU_SHELL(conv_submenu),
		GTK_WIDGET(scrncap_btn_menu), pos);
	gtk_widget_show(GTK_WIDGET(scrncap_btn_menu));

	g_signal_connect_after(G_OBJECT(win->notebook), "switch-page",
		G_CALLBACK(scrncap_convwin_switch), win);
	scrncap_convwin_switch(GTK_NOTEBOOK(win->notebook), NULL, 0, win);
}

static void
scrncap_convwin_uninit(PidginConvWindow *win)
{
	PidginConvWindowMenu *menu = win->menu;
	GtkWidget *btn;

	btn = g_object_get_data(G_OBJECT(menu->menubar),
		"insert-screenshot-btn");
	if (btn)
		gtk_widget_destroy(btn);

	g_object_set_data(G_OBJECT(menu->menubar),
		"insert-screenshot-btn", NULL);
	g_object_set_data(G_OBJECT(menu->menubar),
		"insert-screenshot-action", NULL);

	g_signal_handlers_disconnect_matched(win->notebook, G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, scrncap_convwin_switch, NULL);
}

static void
scrncap_conversation_update(PidginWebView *webview,
	PidginWebViewButtons buttons, gpointer _action)
{
	GtkAction *action = GTK_ACTION(_action);

	gtk_action_set_sensitive(action, buttons & PIDGIN_WEBVIEW_IMAGE);
}

static void
scrncap_conversation_init(PidginConversation *gtkconv)
{
	PidginWebView *webview;
	PidginWebViewToolbar *toolbar;
	GtkAction *action;
	GtkToolItem *scrncap_btn_wide;
	GtkWidget *scrncap_btn_lean;
	gint pos = -1, i;
	GtkToolbar *wide_view, *lean_view;
	GtkMenu *wide_menu = NULL;
	GList *wide_children, *it;

	if (scrncap_conv_get_data(gtkconv, "scrncap-btn-wide") != NULL)
		return;

	webview = PIDGIN_WEBVIEW(gtkconv->entry);
	toolbar = PIDGIN_WEBVIEWTOOLBAR(pidgin_webview_get_toolbar(webview));
	g_return_if_fail(toolbar != NULL);
	wide_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_wide_view(toolbar));
	g_return_if_fail(wide_view != NULL);
	lean_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_lean_view(toolbar));
	g_return_if_fail(lean_view != NULL);

	action = gtk_action_new("InsertScreenshot", _("_Screenshot"),
		_("Insert screenshot"), PIDGIN_STOCK_TOOLBAR_INSERT_SCREENSHOT);
	gtk_action_set_is_important(action, TRUE);
	g_signal_connect(G_OBJECT(action), "activate",
		G_CALLBACK(scrncap_do_screenshot), webview);

	scrncap_btn_wide = GTK_TOOL_ITEM(gtk_action_create_tool_item(action));
	scrncap_conv_set_data(gtkconv, "scrncap-btn-wide", scrncap_btn_wide);
	for (i = 0; i < gtk_toolbar_get_n_items(wide_view); i++) {
		GtkToolItem *ref_item = gtk_toolbar_get_nth_item(wide_view, i);
		GtkAction *action;

		action = g_object_get_data(G_OBJECT(ref_item), "action");
		if (action == NULL)
			continue;

		if (g_strcmp0(gtk_action_get_name(action), "InsertImage") == 0) {
			pos = i + 1;
			break;
		}
	}
	gtk_toolbar_insert(wide_view, scrncap_btn_wide, pos);
	gtk_widget_show(GTK_WIDGET(scrncap_btn_wide));

	for (i = 0; i < gtk_toolbar_get_n_items(lean_view); i++) {
		GtkToolItem *ref_item = gtk_toolbar_get_nth_item(lean_view, i);
		const gchar *menu_name;

		menu_name = g_object_get_data(G_OBJECT(ref_item), "menu-name");
		if (g_strcmp0(menu_name, "insert") == 0) {
			wide_menu = g_object_get_data(G_OBJECT(ref_item), "menu");
			break;
		}
	}
	g_return_if_fail(wide_menu);

	pos = -1;
	wide_children = gtk_container_get_children(GTK_CONTAINER(wide_menu));
	for (it = wide_children, i = 0; it; it = g_list_next(it), i++) {
		GtkWidget *child = it->data;
		GtkAction *action;

		action = g_object_get_data(G_OBJECT(child), "action");
		if (action == NULL)
			continue;

		if (g_strcmp0(gtk_action_get_name(action), "InsertImage") == 0) {
			pos = i + 1;
			break;
		}
	}
	g_list_free(wide_children);
	if (pos < 0) {
		g_warn_if_fail(pos >= 0);
		pos = 0;
	}

	g_signal_connect_object(G_OBJECT(webview), "allowed-formats-updated",
		G_CALLBACK(scrncap_conversation_update), action, 0);
	scrncap_conversation_update(webview,
		pidgin_webview_get_format_functions(webview), action);

	scrncap_btn_lean = gtk_action_create_menu_item(action);
	scrncap_conv_set_data(gtkconv, "scrncap-btn-lean", scrncap_btn_lean);
	gtk_menu_shell_insert(GTK_MENU_SHELL(wide_menu),
		GTK_WIDGET(scrncap_btn_lean), pos);
	gtk_widget_show(GTK_WIDGET(scrncap_btn_lean));
}

static void
scrncap_conversation_uninit(PidginConversation *gtkconv)
{
	GtkWidget *scrncap_btn_wide, *scrncap_btn_lean;

	scrncap_btn_wide = scrncap_conv_get_data(gtkconv, "scrncap-btn-wide");
	if (scrncap_btn_wide == NULL)
		return;

	scrncap_btn_lean = scrncap_conv_get_data(gtkconv, "scrncap-btn-lean");

	gtk_widget_destroy(scrncap_btn_wide);
	if (scrncap_btn_lean)
		gtk_widget_destroy(scrncap_btn_lean);

	scrncap_conv_set_data(gtkconv, "scrncap-btn-wide", NULL);
	scrncap_conv_set_data(gtkconv, "scrncap-btn-lean", NULL);
}

/******************************************************************************
 * Plugin setup
 ******************************************************************************/

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Tomasz Wasilczyk <twasilczyk@pidgin.im>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           "gtk-screencap",
		"name",         N_("Screen Capture"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Utility"),
		"summary",      N_("Send screenshots to your buddies."),
		"description",  N_("Adds an option to send a screenshot as an inline "
		                   "image. It works only with protocols that supports "
		                   "inline images."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	GList *it;
	const gchar *color_str;

	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/screencap");
	purple_prefs_add_string("/plugins/gtk/screencap/brush_color",
		SCRNCAP_DEFAULT_COLOR);

	color_str = purple_prefs_get_string("/plugins/gtk/screencap/brush_color");
	if (color_str && color_str[0])
		gdk_rgba_parse(&brush_color, color_str);

	purple_signal_connect(pidgin_conversations_get_handle(),
		"conversation-displayed", plugin,
		PURPLE_CALLBACK(scrncap_conversation_init), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(),
		"conversation-window-created", plugin,
		PURPLE_CALLBACK(scrncap_convwin_init), NULL);

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;
		scrncap_conversation_init(PIDGIN_CONVERSATION(conv));
	}

	it = pidgin_conv_windows_get_list();
	for (; it; it = g_list_next(it)) {
		PidginConvWindow *win = it->data;
		scrncap_convwin_init(win);
	}

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	GList *it;

	if (shooting_timeout > 0)
		g_source_remove(shooting_timeout);
	if (current_window != NULL)
		gtk_widget_destroy(GTK_WIDGET(current_window));

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;
		scrncap_conversation_uninit(PIDGIN_CONVERSATION(conv));
	}

	it = pidgin_conv_windows_get_list();
	for (; it; it = g_list_next(it)) {
		PidginConvWindow *win = it->data;
		scrncap_convwin_uninit(win);
	}

	return TRUE;
}

PURPLE_PLUGIN_INIT(screencap, plugin_query, plugin_load, plugin_unload);
