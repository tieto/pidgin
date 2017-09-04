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
#define _PIDGIN_GTKUTILS_C_

#include "internal.h"
#include "glibcompat.h"
#include "pidgin.h"

#ifdef _WIN32
#  undef small
#  include <shellapi.h>
#endif /*_WIN32*/

#include <gdk/gdkkeysyms.h>

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "protocol.h"
#include "request.h"
#include "signals.h"
#include "sound.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkprefs.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "pidginstock.h"
#include "gtkrequest.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "gtkwebviewtoolbar.h"
#include "pidgin/minidialog.h"

#include "gtk3compat.h"


/******************************************************************************
 * Enums
 *****************************************************************************/

enum {
	AOP_ICON_COLUMN,
	AOP_NAME_COLUMN,
	AOP_DATA_COLUMN,
	AOP_COLUMN_COUNT
};

enum {
	DND_FILE_TRANSFER,
	DND_IM_IMAGE,
	DND_BUDDY_ICON
};

enum {
	COMPLETION_DISPLAYED_COLUMN,  /* displayed completion value */
	COMPLETION_BUDDY_COLUMN,      /* buddy name */
	COMPLETION_NORMALIZED_COLUMN, /* UTF-8 normalized & casefolded buddy name */
	COMPLETION_COMPARISON_COLUMN, /* UTF-8 normalized & casefolded value for comparison */
	COMPLETION_ACCOUNT_COLUMN,    /* account */
	COMPLETION_COLUMN_COUNT
};

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	GtkTreeModel *model;
	gint default_item;
} AopMenu;

typedef struct {
	char *filename;
	PurpleAccount *account;
	char *who;
} _DndData;

typedef struct
{
	GtkWidget *entry;
	GtkWidget *accountopt;

	PidginFilterBuddyCompletionEntryFunc filter_func;
	gpointer filter_func_user_data;

	GtkListStore *store;
} PidginCompletionData;

struct _icon_chooser {
	GtkWidget *icon_filesel;
	GtkWidget *icon_preview;
	GtkWidget *icon_text;

	void (*callback)(const char*,gpointer);
	gpointer data;
};

struct _old_button_clicked_cb_data
{
	PidginUtilMiniDialogCallback cb;
	gpointer data;
};

/******************************************************************************
 * Globals
 *****************************************************************************/

static guint accels_save_timer = 0;
static GSList *registered_url_handlers = NULL;
static GSList *minidialogs = NULL;

/******************************************************************************
 * Code
 *****************************************************************************/

static gboolean
url_clicked_idle_cb(gpointer data)
{
	purple_notify_uri(NULL, data);
	g_free(data);
	return FALSE;
}

static gboolean
url_clicked_cb(PidginWebView *unused, const char *uri)
{
	g_idle_add(url_clicked_idle_cb, g_strdup(uri));
	return TRUE;
}

void
pidgin_setup_webview(GtkWidget *webview)
{
	g_return_if_fail(webview != NULL);
	g_return_if_fail(PIDGIN_IS_WEBVIEW(webview));

#ifdef _WIN32
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font")) {
		WebKitWebSettings *settings = webkit_web_settings_new();
		g_object_set(G_OBJECT(settings), "default-font-size",
		             purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/font_size"),
		             NULL);
		g_object_set(G_OBJECT(settings), "default-font-family",
		             purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font"),
		             NULL);

		webkit_web_view_set_settings(WEBKIT_WEB_VIEW(webview), settings);
		g_object_unref(settings);
	}
#endif
}

static
void pidgin_window_init(GtkWindow *wnd, const char *title, guint border_width, const char *role, gboolean resizable)
{
	if (title)
		gtk_window_set_title(wnd, title);
#ifdef _WIN32
	else
		gtk_window_set_title(wnd, PIDGIN_ALERT_TITLE);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(wnd), border_width);
	if (role)
		gtk_window_set_role(wnd, role);
	gtk_window_set_resizable(wnd, resizable);
}

GtkWidget *
pidgin_create_window(const char *title, guint border_width, const char *role, gboolean resizable)
{
	GtkWindow *wnd = NULL;

	wnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	pidgin_window_init(wnd, title, border_width, role, resizable);

	return GTK_WIDGET(wnd);
}

GtkWidget *
pidgin_create_small_button(GtkWidget *image)
{
	GtkWidget *button;

	button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	/* don't allow focus on the close button */
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);

	gtk_widget_show(image);

	gtk_container_add(GTK_CONTAINER(button), image);

	return button;
}

GtkWidget *
pidgin_create_dialog(const char *title, guint border_width, const char *role, gboolean resizable)
{
	GtkWindow *wnd = NULL;

	wnd = GTK_WINDOW(gtk_dialog_new());
	pidgin_window_init(wnd, title, border_width, role, resizable);

	return GTK_WIDGET(wnd);
}

GtkWidget *
pidgin_create_video_widget(void)
{
	GtkWidget *video = NULL;
	GdkRGBA color = {0.0, 0.0, 0.0, 1.0};

	video = gtk_drawing_area_new();
	gtk_widget_override_background_color(video, GTK_STATE_FLAG_NORMAL, &color);

	/* In order to enable client shadow decorations, GtkDialog from GTK+ 3.0
	 * uses ARGB visual which by default gets inherited by its child widgets.
	 * XVideo adaptors on the other hand often support just depth 24 and
	 * rendering video through xvimagesink onto a widget inside a GtkDialog
	 * then results in no visible output.
	 *
	 * This ensures the default system visual of the drawing area doesn't get
	 * overridden by the widget's parent.
	 */
	gtk_widget_set_visual(video,
	                gdk_screen_get_system_visual(gtk_widget_get_screen(video)));

	return video;
}

GtkWidget *
pidgin_dialog_get_vbox_with_properties(GtkDialog *dialog, gboolean homogeneous, gint spacing)
{
	GtkBox *vbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_box_set_homogeneous(vbox, homogeneous);
	gtk_box_set_spacing(vbox, spacing);
	return GTK_WIDGET(vbox);
}

GtkWidget *pidgin_dialog_get_vbox(GtkDialog *dialog)
{
	return gtk_dialog_get_content_area(GTK_DIALOG(dialog));
}

GtkWidget *pidgin_dialog_get_action_area(GtkDialog *dialog)
{
	return gtk_dialog_get_action_area(GTK_DIALOG(dialog));
}

GtkWidget *pidgin_dialog_add_button(GtkDialog *dialog, const char *label,
		GCallback callback, gpointer callbackdata)
{
	GtkWidget *button = gtk_button_new_from_stock(label);
	GtkWidget *bbox = pidgin_dialog_get_action_area(dialog);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	if (callback)
		g_signal_connect(G_OBJECT(button), "clicked", callback, callbackdata);
	gtk_widget_show(button);
	return button;
}

GtkWidget *
pidgin_create_webview(gboolean editable, GtkWidget **webview_ret, GtkWidget **sw_ret)
{
	GtkWidget *frame;
	GtkWidget *webview;
	GtkWidget *sep;
	GtkWidget *sw;
	GtkWidget *toolbar = NULL;
	GtkWidget *vbox;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	if (editable) {
		toolbar = pidgin_webviewtoolbar_new();
		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
		gtk_widget_show(toolbar);

		sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
		g_signal_connect_swapped(G_OBJECT(toolbar), "show", G_CALLBACK(gtk_widget_show), sep);
		g_signal_connect_swapped(G_OBJECT(toolbar), "hide", G_CALLBACK(gtk_widget_hide), sep);
		gtk_widget_show(sep);
	}

	webview = pidgin_webview_new(editable);
	if (editable && purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck"))
		pidgin_webview_set_spellcheck(PIDGIN_WEBVIEW(webview), TRUE);
	gtk_widget_show(webview);

	if (editable) {
		pidgin_webviewtoolbar_attach(PIDGIN_WEBVIEWTOOLBAR(toolbar), webview);
		pidgin_webview_set_toolbar(PIDGIN_WEBVIEW(webview), toolbar);
	}
	pidgin_setup_webview(webview);

	sw = pidgin_make_scrollable(webview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_NONE, -1, -1);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	pidgin_webview_set_vadjustment(PIDGIN_WEBVIEW(webview),
			gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)));

	if (webview_ret != NULL)
		*webview_ret = webview;

	if (sw_ret != NULL)
		*sw_ret = sw;

	return frame;
}

void
pidgin_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
									  (*text != '\0'));
}

void
pidgin_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity;

	if (to_toggle == NULL)
		return;

	sensitivity = gtk_widget_get_sensitive(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
pidgin_toggle_sensitive_array(GtkWidget *w, GPtrArray *data)
{
	gboolean sensitivity;
	gpointer element;
	guint i;

	for (i=0; i < data->len; i++) {
		element = g_ptr_array_index(data,i);
		if (element == NULL)
			continue;

		sensitivity = gtk_widget_get_sensitive(element);

		gtk_widget_set_sensitive(element, !sensitivity);
	}
}

void
pidgin_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle)
{
	if (to_toggle == NULL)
		return;

	if (gtk_widget_get_visible(to_toggle))
		gtk_widget_hide(to_toggle);
	else
		gtk_widget_show(to_toggle);
}

GtkWidget *pidgin_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	return menuitem;
}

GtkWidget *pidgin_new_check_item(GtkWidget *menu, const char *str,
		GCallback cb, gpointer data, gboolean checked)
{
	GtkWidget *menuitem;
	menuitem = gtk_check_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), checked);

	if (cb)
		g_signal_connect(G_OBJECT(menuitem), "activate", cb, data);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
pidgin_pixbuf_toolbar_button_from_stock(const char *icon)
{
	GtkWidget *button, *image, *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	return button;
}

GtkWidget *
pidgin_pixbuf_button_from_stock(const char *text, const char *icon,
							  PidginButtonOrientation style)
{
	GtkWidget *button, *image, *bbox, *ibox, *lbox = NULL;

	button = gtk_button_new();

	if (style == PIDGIN_BUTTON_HORIZONTAL) {
		bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		ibox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		if (text)
			lbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	} else {
		bbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		ibox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		if (text)
			lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	}

	gtk_container_add(GTK_CONTAINER(button), bbox);

	if (icon) {
		gtk_box_pack_start(GTK_BOX(bbox), ibox, TRUE, TRUE, 0);
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, TRUE, 0);
	}

	if (text) {
		GtkLabel *label;

		gtk_box_pack_start(GTK_BOX(bbox), lbox, TRUE, TRUE, 0);
		label = GTK_LABEL(gtk_label_new(NULL));
		gtk_label_set_text_with_mnemonic(label, text);
		gtk_label_set_mnemonic_widget(label, button);
		gtk_box_pack_start(GTK_BOX(lbox), GTK_WIDGET(label),
			FALSE, TRUE, 0);
		pidgin_set_accessible_label(button, label);
	}

	gtk_widget_show_all(bbox);

	return button;
}


GtkWidget *pidgin_new_menu_item(GtkWidget *menu, const char *mnemonic,
                const char *icon, GCallback cb, gpointer data)
{
	GtkWidget *menuitem;
	GtkWidget *box;
	GtkWidget *label;

        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);

        menuitem = gtk_menu_item_new();

	if (cb)
		g_signal_connect(G_OBJECT(menuitem), "activate", cb, data);

        if (icon) {
                GtkWidget *image;
                image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
                gtk_container_add(GTK_CONTAINER(box), image);
        }

        label = gtk_label_new_with_mnemonic(mnemonic);
        gtk_container_add(GTK_CONTAINER(box), label);

        gtk_container_add(GTK_CONTAINER(menuitem), box);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
pidgin_make_frame(GtkWidget *parent, const char *title)
{
	GtkWidget *vbox, *vbox2, *hbox;
	GtkLabel *label;
	char *labeltitle;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	label = GTK_LABEL(gtk_label_new(NULL));

	labeltitle = g_strdup_printf("<span weight=\"bold\">%s</span>", title);
	gtk_label_set_markup(label, labeltitle);
	g_free(labeltitle);

	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(label), FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(label));
	pidgin_set_accessible_label(vbox, label);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = GTK_LABEL(gtk_label_new("    "));
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(label));

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	g_object_set_data(G_OBJECT(vbox2), "main-vbox", vbox);

	return vbox2;
}

static gpointer
aop_option_menu_get_selected(GtkWidget *optmenu)
{
	gpointer data = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail(optmenu != NULL, NULL);

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(optmenu), &iter))
		gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)),
		                   &iter, AOP_DATA_COLUMN, &data, -1);

	return data;
}

static void
aop_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	if (cb != NULL) {
		((void (*)(GtkWidget *, gpointer, gpointer))cb)(optmenu,
			aop_option_menu_get_selected(optmenu),
			g_object_get_data(G_OBJECT(optmenu), "user_data"));
	}
}

static void
aop_option_menu_replace_menu(GtkWidget *optmenu, AopMenu *new_aop_menu)
{
	gtk_combo_box_set_model(GTK_COMBO_BOX(optmenu), new_aop_menu->model);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu), new_aop_menu->default_item);
	g_free(new_aop_menu);
}

static GdkPixbuf *
pidgin_create_icon_from_protocol(PurpleProtocol *protocol, PidginProtocolIconSize size, PurpleAccount *account)
{
	const char *protoname = NULL;
	char *tmp;
	char *filename = NULL;
	GdkPixbuf *pixbuf;

	protoname = purple_protocol_class_list_icon(protocol, account, NULL);
	if (protoname == NULL)
		return NULL;

	/*
	 * Status icons will be themeable too, and then it will look up
	 * protoname from the theme
	 */
	tmp = g_strconcat(protoname, ".png", NULL);

	filename = g_build_filename(PURPLE_DATADIR,
		"pixmaps", "pidgin", "protocols",
		(size == PIDGIN_PROTOCOL_ICON_SMALL) ? "16" :
			((size == PIDGIN_PROTOCOL_ICON_MEDIUM) ? "22" : "48"),
		tmp, NULL);
	g_free(tmp);

	pixbuf = pidgin_pixbuf_new_from_file(filename);
	g_free(filename);

	return pixbuf;
}

static GtkWidget *
aop_option_menu_new(AopMenu *aop_menu, GCallback cb, gpointer user_data)
{
	GtkWidget *optmenu = NULL;
	GtkCellRenderer *cr = NULL;

	optmenu = gtk_combo_box_new();
	gtk_widget_show(optmenu);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(optmenu), cr = gtk_cell_renderer_pixbuf_new(), FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(optmenu), cr, "pixbuf", AOP_ICON_COLUMN);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(optmenu), cr = gtk_cell_renderer_text_new(), TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(optmenu), cr, "text", AOP_NAME_COLUMN);

	aop_option_menu_replace_menu(optmenu, aop_menu);
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	g_signal_connect(G_OBJECT(optmenu), "changed", G_CALLBACK(aop_menu_cb), cb);

	return optmenu;
}

static void
aop_option_menu_select_by_data(GtkWidget *optmenu, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer iter_data;
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu));
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, AOP_DATA_COLUMN, &iter_data, -1);
			if (iter_data == data) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(optmenu), &iter);
				return;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

static AopMenu *
create_protocols_menu(const char *default_proto_id)
{
	AopMenu *aop_menu = NULL;
	PurpleProtocol *protocol;
	GdkPixbuf *pixbuf = NULL;
	GtkTreeIter iter;
	GtkListStore *ls;
	GList *list, *p;
	int i;

	ls = gtk_list_store_new(AOP_COLUMN_COUNT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);

	aop_menu = g_malloc0(sizeof(AopMenu));
	aop_menu->default_item = 0;
	aop_menu->model = GTK_TREE_MODEL(ls);

	list = purple_protocols_get_all();

	for (p = list, i = 0;
		 p != NULL;
		 p = p->next, i++) {

		protocol = PURPLE_PROTOCOL(p->data);

		pixbuf = pidgin_create_icon_from_protocol(protocol, PIDGIN_PROTOCOL_ICON_SMALL, NULL);

		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter,
		                   AOP_ICON_COLUMN, pixbuf,
		                   AOP_NAME_COLUMN, purple_protocol_get_name(protocol),
		                   AOP_DATA_COLUMN, purple_protocol_get_id(protocol),
		                   -1);

		if (pixbuf)
			g_object_unref(pixbuf);

		if (default_proto_id != NULL && purple_strequal(purple_protocol_get_id(protocol), default_proto_id))
			aop_menu->default_item = i;
	}
	g_list_free(list);

	return aop_menu;
}

GtkWidget *
pidgin_protocol_option_menu_new(const char *id, GCallback cb,
								  gpointer user_data)
{
	return aop_option_menu_new(create_protocols_menu(id), cb, user_data);
}

const char *
pidgin_protocol_option_menu_get_selected(GtkWidget *optmenu)
{
	return (const char *)aop_option_menu_get_selected(optmenu);
}

PurpleAccount *
pidgin_account_option_menu_get_selected(GtkWidget *optmenu)
{
	return (PurpleAccount *)aop_option_menu_get_selected(optmenu);
}

static AopMenu *
create_account_menu(PurpleAccount *default_account,
					PurpleFilterAccountFunc filter_func, gboolean show_all)
{
	AopMenu *aop_menu = NULL;
	PurpleAccount *account;
	GdkPixbuf *pixbuf = NULL;
	GList *list;
	GList *p;
	GtkListStore *ls;
	GtkTreeIter iter;
	int i;
	char buf[256];

	if (show_all)
		list = purple_accounts_get_all();
	else
		list = purple_connections_get_all();

	ls = gtk_list_store_new(AOP_COLUMN_COUNT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);

	aop_menu = g_malloc0(sizeof(AopMenu));
	aop_menu->default_item = 0;
	aop_menu->model = GTK_TREE_MODEL(ls);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		if (show_all)
			account = (PurpleAccount *)p->data;
		else {
			PurpleConnection *gc = (PurpleConnection *)p->data;

			account = purple_connection_get_account(gc);
		}

		if (filter_func && !filter_func(account)) {
			i--;
			continue;
		}

		pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);

		if (pixbuf) {
			if (purple_account_is_disconnected(account) && show_all &&
					purple_connections_get_all())
				gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);
		}

		if (purple_account_get_private_alias(account)) {
			g_snprintf(buf, sizeof(buf), "%s (%s) (%s)",
					   purple_account_get_username(account),
					   purple_account_get_private_alias(account),
					   purple_account_get_protocol_name(account));
		} else {
			g_snprintf(buf, sizeof(buf), "%s (%s)",
					   purple_account_get_username(account),
					   purple_account_get_protocol_name(account));
		}

		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, 
		                   AOP_ICON_COLUMN, pixbuf,
		                   AOP_NAME_COLUMN, buf,
		                   AOP_DATA_COLUMN, account,
		                   -1);

		if (pixbuf)
			g_object_unref(pixbuf);

		if (default_account && account == default_account)
			aop_menu->default_item = i;
	}

	return aop_menu;
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	gboolean show_all;
	PurpleAccount *account;
	PurpleFilterAccountFunc filter_func;

	account = (PurpleAccount *)aop_option_menu_get_selected(optmenu);
	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu), "show_all"));
	filter_func = g_object_get_data(G_OBJECT(optmenu), "filter_func");

	aop_option_menu_replace_menu(optmenu, create_account_menu(account, filter_func, show_all));
}

static void
account_menu_sign_on_off_cb(PurpleConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(PurpleAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event,
						  void *user_data)
{
	purple_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

void
pidgin_account_option_menu_set_selected(GtkWidget *optmenu, PurpleAccount *account)
{
	aop_option_menu_select_by_data(optmenu, account);
}

GtkWidget *
pidgin_account_option_menu_new(PurpleAccount *default_account,
								 gboolean show_all, GCallback cb,
								 PurpleFilterAccountFunc filter_func,
								 gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = aop_option_menu_new(create_account_menu(default_account, filter_func, show_all), cb, user_data);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
					 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the purple sign on/off event callbacks. */
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						optmenu, PURPLE_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	purple_signal_connect(purple_connections_get_handle(), "signed-off",
						optmenu, PURPLE_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	purple_signal_connect(purple_accounts_get_handle(), "account-added",
						optmenu, PURPLE_CALLBACK(account_menu_added_removed_cb),
						optmenu);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						optmenu, PURPLE_CALLBACK(account_menu_added_removed_cb),
						optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all", GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "filter_func", filter_func);

	return optmenu;
}

void
pidgin_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
                         GdkModifierType arg2, GClosure *arg3,
                         gpointer data)
{
	purple_debug(PURPLE_DEBUG_MISC, "accels",
	           "accel changed, scheduling save.\n");

	if (!accels_save_timer)
		accels_save_timer = g_timeout_add_seconds(5, pidgin_save_accels,
		                                  NULL);
}

gboolean
pidgin_save_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(purple_user_dir(), G_DIR_SEPARATOR_S,
	                            "accels", NULL);
	purple_debug(PURPLE_DEBUG_MISC, "accels", "saving accels to %s\n", filename);
	gtk_accel_map_save(filename);
	g_free(filename);

	accels_save_timer = 0;
	return FALSE;
}

void
pidgin_load_accels()
{
	char *filename = NULL;

	filename = g_build_filename(purple_user_dir(), G_DIR_SEPARATOR_S,
	                            "accels", NULL);
	gtk_accel_map_load(filename);
	g_free(filename);
}

static void
show_retrieveing_info(PurpleConnection *conn, const char *name)
{
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	purple_notify_user_info_add_pair_plaintext(info, _("Information"), _("Retrieving..."));
	purple_notify_userinfo(conn, name, info, NULL, NULL);
	purple_notify_user_info_destroy(info);
}

void pidgin_retrieve_user_info(PurpleConnection *conn, const char *name)
{
	show_retrieveing_info(conn, name);
	purple_serv_get_info(conn, name);
}

void pidgin_retrieve_user_info_in_chat(PurpleConnection *conn, const char *name, int chat)
{
	char *who = NULL;
	PurpleProtocol *protocol = NULL;

	if (chat < 0) {
		pidgin_retrieve_user_info(conn, name);
		return;
	}

	protocol = purple_connection_get_protocol(conn);
	if (protocol != NULL)
		who = purple_protocol_chat_iface_get_user_real_name(protocol, conn, chat, name);

	pidgin_retrieve_user_info(conn, who ? who : name);
	g_free(who);
}

gboolean
pidgin_parse_x_im_contact(const char *msg, gboolean all_accounts,
							PurpleAccount **ret_account, char **ret_protocol,
							char **ret_username, char **ret_alias)
{
	char *protocol = NULL;
	char *username = NULL;
	char *alias    = NULL;
	char *str;
	char *s;
	gboolean valid;

	g_return_val_if_fail(msg          != NULL, FALSE);
	g_return_val_if_fail(ret_protocol != NULL, FALSE);
	g_return_val_if_fail(ret_username != NULL, FALSE);

	s = str = g_strdup(msg);

	while (*s != '\r' && *s != '\n' && *s != '\0')
	{
		char *key, *value;

		key = s;

		/* Grab the key */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ' ')
			s++;

		if (*s == '\r') s++;

		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		/* Clear past any whitespace */
		while (*s != '\0' && *s == ' ')
			s++;

		/* Now let's grab until the end of the line. */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		if (strchr(key, ':') != NULL)
		{
			if (!g_ascii_strcasecmp(key, "X-IM-Username:"))
				username = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Protocol:"))
				protocol = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Alias:"))
				alias = g_strdup(value);
		}
	}

	if (username != NULL && protocol != NULL)
	{
		valid = TRUE;

		*ret_username = username;
		*ret_protocol = protocol;

		if (ret_alias != NULL)
			*ret_alias = alias;

		/* Check for a compatible account. */
		if (ret_account != NULL)
		{
			GList *list;
			PurpleAccount *account = NULL;
			GList *l;
			const char *protoname;

			if (all_accounts)
				list = purple_accounts_get_all();
			else
				list = purple_connections_get_all();

			for (l = list; l != NULL; l = l->next)
			{
				PurpleConnection *gc;
				PurpleProtocol *proto = NULL;

				if (all_accounts)
				{
					account = (PurpleAccount *)l->data;

					proto = purple_protocols_find(
						purple_account_get_protocol_id(account));

					if (proto == NULL)
					{
						account = NULL;

						continue;
					}
				}
				else
				{
					gc = (PurpleConnection *)l->data;
					account = purple_connection_get_account(gc);

					proto = purple_connection_get_protocol(gc);
				}

				protoname = purple_protocol_class_list_icon(proto, account, NULL);

				if (purple_strequal(protoname, protocol))
					break;

				account = NULL;
			}

			/* Special case for AIM and ICQ */
			if (account == NULL && (purple_strequal(protocol, "aim") ||
									purple_strequal(protocol, "icq")))
			{
				for (l = list; l != NULL; l = l->next)
				{
					PurpleConnection *gc;
					PurpleProtocol *proto = NULL;

					if (all_accounts)
					{
						account = (PurpleAccount *)l->data;

						proto = purple_protocols_find(
							purple_account_get_protocol_id(account));

						if (proto == NULL)
						{
							account = NULL;

							continue;
						}
					}
					else
					{
						gc = (PurpleConnection *)l->data;
						account = purple_connection_get_account(gc);

						proto = purple_connection_get_protocol(gc);
					}

					protoname = purple_protocol_class_list_icon(proto, account, NULL);

					if (purple_strequal(protoname, "aim") || purple_strequal(protoname, "icq"))
						break;

					account = NULL;
				}
			}

			*ret_account = account;
		}
	}
	else
	{
		valid = FALSE;

		g_free(username);
		g_free(protocol);
		g_free(alias);
	}

	g_free(str);

	return valid;
}

void
pidgin_set_accessible_label(GtkWidget *w, GtkLabel *l)
{
	AtkObject *acc;
	const gchar *label_text;
	const gchar *existing_name;

	acc = gtk_widget_get_accessible (w);

	/* If this object has no name, set it's name with the label text */
	existing_name = atk_object_get_name (acc);
	if (!existing_name) {
		label_text = gtk_label_get_text(l);
		if (label_text)
			atk_object_set_name (acc, label_text);
	}

	pidgin_set_accessible_relations(w, l);
}

void
pidgin_set_accessible_relations (GtkWidget *w, GtkLabel *l)
{
	AtkObject *acc, *label;
	AtkObject *rel_obj[1];
	AtkRelationSet *set;
	AtkRelation *relation;

	acc = gtk_widget_get_accessible (w);
	label = gtk_widget_get_accessible(GTK_WIDGET(l));

	/* Make sure mnemonics work */
	gtk_label_set_mnemonic_widget(l, w);

	/* Create the labeled-by relation */
	set = atk_object_ref_relation_set (acc);
	rel_obj[0] = label;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
	g_object_unref(set);

	/* Create the label-for relation */
	set = atk_object_ref_relation_set (label);
	rel_obj[0] = acc;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
	g_object_unref(set);
}

void
pidgin_menu_position_func_helper(GtkMenu *menu,
							gint *x,
							gint *y,
							gboolean *push_in,
							gpointer data)
{
	GtkWidget *widget;
	GtkRequisition requisition;
	GdkScreen *screen;
	GdkRectangle monitor;
	gint monitor_num;
	gint space_left, space_right, space_above, space_below;
	gboolean rtl;

	g_return_if_fail(GTK_IS_MENU(menu));

	widget     = GTK_WIDGET(menu);
	screen     = gtk_widget_get_screen(widget);
	rtl        = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);

	/*
	 * We need the requisition to figure out the right place to
	 * popup the menu. In fact, we always need to ask here, since
	 * if a size_request was queued while we weren't popped up,
	 * the requisition won't have been recomputed yet.
	 */
	gtk_widget_get_preferred_size(widget, NULL, &requisition);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);

	*push_in = FALSE;

	/*
	 * The placement of popup menus horizontally works like this (with
	 * RTL in parentheses)
	 *
	 * - If there is enough room to the right (left) of the mouse cursor,
	 *   position the menu there.
	 *
	 * - Otherwise, if if there is enough room to the left (right) of the
	 *   mouse cursor, position the menu there.
	 *
	 * - Otherwise if the menu is smaller than the monitor, position it
	 *   on the side of the mouse cursor that has the most space available
	 *
	 * - Otherwise (if there is simply not enough room for the menu on the
	 *   monitor), position it as far left (right) as possible.
	 *
	 * Positioning in the vertical direction is similar: first try below
	 * mouse cursor, then above.
	 */
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	space_left = *x - monitor.x;
	space_right = monitor.x + monitor.width - *x - 1;
	space_above = *y - monitor.y;
	space_below = monitor.y + monitor.height - *y - 1;

	/* position horizontally */

	if (requisition.width <= space_left ||
	    requisition.width <= space_right)
	{
		if ((rtl  && requisition.width <= space_left) ||
		    (!rtl && requisition.width >  space_right))
		{
			/* position left */
			*x = *x - requisition.width + 1;
		}

		/* x is clamped on-screen further down */
	}
	else if (requisition.width <= monitor.width)
	{
		/* the menu is too big to fit on either side of the mouse
		 * cursor, but smaller than the monitor. Position it on
		 * the side that has the most space
		 */
		if (space_left > space_right)
		{
			/* left justify */
			*x = monitor.x;
		}
		else
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
	}
	else /* menu is simply too big for the monitor */
	{
		if (rtl)
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
		else
		{
			/* left justify */
			*x = monitor.x;
		}
	}

	/* Position vertically. The algorithm is the same as above, but
	 * simpler because we don't have to take RTL into account.
	 */

	if (requisition.height <= space_above ||
	    requisition.height <= space_below)
	{
		if (requisition.height > space_below) {
			*y = *y - requisition.height + 1;
		}

		*y = CLAMP (*y, monitor.y,
			   monitor.y + monitor.height - requisition.height);
	}
	else if (requisition.height > space_below &&
	         requisition.height > space_above)
	{
		if (space_below >= space_above)
			*y = monitor.y + monitor.height - requisition.height;
		else
			*y = monitor.y;
	}
	else
	{
		*y = monitor.y;
	}
}


void
pidgin_treeview_popup_menu_position_func(GtkMenu *menu,
										   gint *x,
										   gint *y,
										   gboolean *push_in,
										   gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkTreeView *tv = GTK_TREE_VIEW(data);
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GdkRectangle rect;

	gdk_window_get_origin (gtk_widget_get_window(widget), x, y);
	gtk_tree_view_get_cursor (tv, &path, &col);
	gtk_tree_view_get_cell_area (tv, path, col, &rect);

	*x += rect.x+rect.width;
	*y += rect.y + rect.height;
	pidgin_menu_position_func_helper(menu, x, y, push_in, data);
}

static void dnd_image_ok_callback(_DndData *data, int choice)
{
	const gchar *shortname;
	gchar *filedata;
	size_t size;
	GStatBuf st;
	GError *err = NULL;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PurpleBuddy *buddy;
	PurpleContact *contact;
	PurpleImage *img;

	switch (choice) {
	case DND_BUDDY_ICON:
		if (g_stat(data->filename, &st)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"),
						data->filename, g_strerror(errno));
			purple_notify_error(NULL, NULL,
				_("Failed to load image"), str, NULL);
			g_free(str);

			break;
		}

		buddy = purple_blist_find_buddy(data->account, data->who);
		if (!buddy) {
			purple_debug_info("custom-icon", "You can only set custom icons for people on your buddylist.\n");
			break;
		}
		contact = purple_buddy_get_contact(buddy);
		purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, data->filename);
		break;
	case DND_FILE_TRANSFER:
		purple_serv_send_file(purple_account_get_connection(data->account), data->who, data->filename);
		break;
	case DND_IM_IMAGE:
		conv = PURPLE_CONVERSATION(purple_im_conversation_new(data->account, data->who));
		gtkconv = PIDGIN_CONVERSATION(conv);

		if (!g_file_get_contents(data->filename, &filedata, &size,
					 &err)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"), data->filename, err->message);
			purple_notify_error(NULL, NULL,
				_("Failed to load image"), str, NULL);

			g_error_free(err);
			g_free(str);

			break;
		}
		shortname = strrchr(data->filename, G_DIR_SEPARATOR);
		shortname = shortname ? shortname + 1 : data->filename;
		img = purple_image_new_from_data(filedata, size);
		purple_image_set_friendly_filename(img, shortname);

		pidgin_webview_insert_image(PIDGIN_WEBVIEW(gtkconv->entry), img);
		g_object_unref(img);

		break;
	}
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_image_cancel_callback(_DndData *data, int choice)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_set_icon_ok_cb(_DndData *data)
{
	dnd_image_ok_callback(data, DND_BUDDY_ICON);
}

static void dnd_set_icon_cancel_cb(_DndData *data)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void
pidgin_dnd_file_send_image(PurpleAccount *account, const gchar *who,
		const gchar *filename)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;
	_DndData *data = g_malloc(sizeof(_DndData));
	gboolean ft = FALSE, im = FALSE;

	data->who = g_strdup(who);
	data->filename = g_strdup(filename);
	data->account = account;

	if (gc)
		protocol = purple_connection_get_protocol(gc);

	if (!(purple_connection_get_flags(gc) & PURPLE_CONNECTION_FLAG_NO_IMAGES))
		im = TRUE;

	if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, XFER_IFACE, can_receive))
		ft = purple_protocol_xfer_iface_can_receive(protocol, gc, who);
	else if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, XFER_IFACE, send))
		ft = TRUE;

	if (im && ft) {
		purple_request_choice(NULL, NULL,
				_("You have dragged an image"),
				_("You can send this image as a file "
					"transfer, embed it into this message, "
					"or use it as the buddy icon for this user."),
				(gpointer)DND_FILE_TRANSFER, _("OK"),
				(GCallback)dnd_image_ok_callback, _("Cancel"),
				(GCallback)dnd_image_cancel_callback,
				purple_request_cpar_from_account(account), data,
				_("Set as buddy icon"), DND_BUDDY_ICON,
				_("Send image file"), DND_FILE_TRANSFER,
				_("Insert in message"), DND_IM_IMAGE,
				NULL);
	} else if (!(im || ft)) {
		purple_request_yes_no(NULL, NULL, _("You have dragged an image"),
				_("Would you like to set it as the buddy icon for this user?"),
				PURPLE_DEFAULT_ACTION_NONE,
				purple_request_cpar_from_account(account),
				data, (GCallback)dnd_set_icon_ok_cb, (GCallback)dnd_set_icon_cancel_cb);
	} else {
		purple_request_choice(NULL, NULL,
				_("You have dragged an image"),
				(ft ? _("You can send this image as a file transfer, or use it as the buddy icon for this user.") :
				 _("You can insert this image into this message, or use it as the buddy icon for this user")),
				GINT_TO_POINTER(ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
				_("OK"), (GCallback)dnd_image_ok_callback,
				_("Cancel"), (GCallback)dnd_image_cancel_callback,
				purple_request_cpar_from_account(account),
				data,
				_("Set as buddy icon"), DND_BUDDY_ICON,
				(ft ? _("Send image file") : _("Insert in message")), (ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
				NULL);
	}

}

#ifndef _WIN32
static void
pidgin_dnd_file_send_desktop(PurpleAccount *account, const gchar *who,
		const gchar *filename)
{
	gchar *name;
	gchar *type;
	gchar *url;
	GKeyFile *desktop_file;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	GError *error = NULL;

	desktop_file = g_key_file_new();

	if (!g_key_file_load_from_file(desktop_file, filename, G_KEY_FILE_NONE, &error)) {
		if (error) {
			purple_debug_warning("D&D", "Failed to load %s: %s\n",
					filename, error->message);
			g_error_free(error);
		}
		return;
	}

	name = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_NAME, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Name from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}

	type = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_TYPE, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Type from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}

	url = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_URL, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Type from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}


	/* If any of this is null, do nothing. */
	if (!name || !type || url) {
		g_free(type);
		g_free(name);
		g_free(url);

		return;
	}

	/* I don't know if we really want to do anything here.  Most of
	 * the desktop item types are crap like "MIME Type" (I have no
	 * clue how that would be a desktop item) and "Comment"...
	 * nothing we can really send.  The only logical one is
	 * "Application," but do we really want to send a binary and
	 * nothing else? Probably not.  I'll just give an error and
	 * return. */
	/* The original patch sent the icon used by the launcher.  That's probably wrong */
	if (purple_strequal(type, "Link")) {
		purple_notify_error(NULL, NULL, _("Cannot send launcher"),
				_("You dragged a desktop launcher. Most "
					"likely you wanted to send the target "
					"of this launcher instead of this "
					"launcher itself."), NULL);

	} else {

		conv = PURPLE_CONVERSATION(purple_im_conversation_new(account, who));
		gtkconv =  PIDGIN_CONVERSATION(conv);
		pidgin_webview_insert_link(PIDGIN_WEBVIEW(gtkconv->entry),
				url, name);
	}

	g_free(type);
	g_free(name);
	g_free(url);
}
#endif /* _WIN32 */

void
pidgin_dnd_file_manage(GtkSelectionData *sd, PurpleAccount *account, const char *who)
{
	GdkPixbuf *pb;
	GList *files = purple_uri_list_extract_filenames((const gchar *) gtk_selection_data_get_data(sd));
	PurpleConnection *gc = purple_account_get_connection(account);
	gchar *filename = NULL;
	gchar *basename = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(who != NULL);

	for ( ; files; files = g_list_delete_link(files, files)) {
		g_free(filename);
		g_free(basename);

		filename = files->data;
		basename = g_path_get_basename(filename);

		/* XXX - Make ft API support creating a transfer with more than one file */
		if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
			continue;
		}

		/* XXX - make ft api suupport sending a directory */
		/* Are we dealing with a directory? */
		if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
			char *str, *str2;

			str = g_strdup_printf(_("Cannot send folder %s."), basename);
			str2 = g_strdup_printf(_("%s cannot transfer a folder. You will need to send the files within individually."), PIDGIN_NAME);

			purple_notify_error(NULL, NULL, str, str2,
				purple_request_cpar_from_connection(gc));

			g_free(str);
			g_free(str2);
			continue;
		}

		/* Are we dealing with an image? */
		pb = pidgin_pixbuf_new_from_file(filename);
		if (pb) {
			pidgin_dnd_file_send_image(account, who, filename);

			g_object_unref(G_OBJECT(pb));

			continue;
		}

#ifndef _WIN32
		/* Are we trying to send a .desktop file? */
		else if (purple_str_has_suffix(basename, ".desktop")) {
			pidgin_dnd_file_send_desktop(account, who, filename);

			continue;
		}
#endif /* _WIN32 */

		/* Everything is fine, let's send */
		purple_serv_send_file(gc, who, filename);
	}

	g_free(filename);
	g_free(basename);
}

void pidgin_buddy_icon_get_scale_size(GdkPixbuf *buf, PurpleBuddyIconSpec *spec, PurpleBuddyIconScaleFlags rules, int *width, int *height)
{
	*width = gdk_pixbuf_get_width(buf);
	*height = gdk_pixbuf_get_height(buf);

	if ((spec == NULL) || !(spec->scale_rules & rules))
		return;

	purple_buddy_icon_spec_get_scaled_size(spec, width, height);

	/* and now for some arbitrary sanity checks */
	if(*width > 100)
		*width = 100;
	if(*height > 100)
		*height = 100;
}

GdkPixbuf * pidgin_create_status_icon(PurpleStatusPrimitive prim, GtkWidget *w, const char *size)
{
	GtkIconSize icon_size = gtk_icon_size_from_name(size);
	GdkPixbuf *pixbuf = NULL;
	const char *stock = pidgin_stock_id_from_status_primitive(prim);

	pixbuf = gtk_widget_render_icon (w, stock ? stock : PIDGIN_STOCK_STATUS_AVAILABLE,
			icon_size, "GtkWidget");
	return pixbuf;
}

static const char *
stock_id_from_status_primitive_idle(PurpleStatusPrimitive prim, gboolean idle)
{
	const char *stock = NULL;
	switch (prim) {
		case PURPLE_STATUS_UNSET:
			stock = NULL;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			stock = idle ? PIDGIN_STOCK_STATUS_BUSY_I : PIDGIN_STOCK_STATUS_BUSY;
			break;
		case PURPLE_STATUS_AWAY:
			stock = idle ? PIDGIN_STOCK_STATUS_AWAY_I : PIDGIN_STOCK_STATUS_AWAY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			stock = idle ? PIDGIN_STOCK_STATUS_XA_I : PIDGIN_STOCK_STATUS_XA;
			break;
		case PURPLE_STATUS_INVISIBLE:
			stock = PIDGIN_STOCK_STATUS_INVISIBLE;
			break;
		case PURPLE_STATUS_OFFLINE:
			stock = idle ? PIDGIN_STOCK_STATUS_OFFLINE_I : PIDGIN_STOCK_STATUS_OFFLINE;
			break;
		default:
			stock = idle ? PIDGIN_STOCK_STATUS_AVAILABLE_I : PIDGIN_STOCK_STATUS_AVAILABLE;
			break;
	}
	return stock;
}

const char *
pidgin_stock_id_from_status_primitive(PurpleStatusPrimitive prim)
{
	return stock_id_from_status_primitive_idle(prim, FALSE);
}

const char *
pidgin_stock_id_from_presence(PurplePresence *presence)
{
	PurpleStatus *status;
	PurpleStatusType *type;
	PurpleStatusPrimitive prim;
	gboolean idle;

	g_return_val_if_fail(presence, NULL);

	status = purple_presence_get_active_status(presence);
	type = purple_status_get_status_type(status);
	prim = purple_status_type_get_primitive(type);

	idle = purple_presence_is_idle(presence);

	return stock_id_from_status_primitive_idle(prim, idle);
}

GdkPixbuf *
pidgin_create_protocol_icon(PurpleAccount *account, PidginProtocolIconSize size)
{
	PurpleProtocol *protocol;

	g_return_val_if_fail(account != NULL, NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));
	if (protocol == NULL)
		return NULL;
	return pidgin_create_icon_from_protocol(protocol, size, account);
}

static void
menu_action_cb(GtkMenuItem *item, gpointer object)
{
	gpointer data;
	void (*callback)(gpointer, gpointer);

	callback = g_object_get_data(G_OBJECT(item), "purplecallback");
	data = g_object_get_data(G_OBJECT(item), "purplecallbackdata");

	if (callback)
		callback(object, data);
}

GtkWidget *
pidgin_append_menu_action(GtkWidget *menu, PurpleMenuAction *act,
                            gpointer object)
{
	GtkWidget *menuitem;
	GList *list;
	const gchar *stock_id;
	GtkWidget *icon_image = NULL;

	if (act == NULL) {
		return pidgin_separator(menu);
	}

	stock_id = purple_menu_action_get_stock_icon(act);
	if (stock_id) {
		icon_image = gtk_image_new_from_stock(stock_id,
			gtk_icon_size_from_name(
				PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	}

	if (icon_image) {
		menuitem = gtk_image_menu_item_new_with_mnemonic(
			purple_menu_action_get_label(act));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
			icon_image);
	} else {
		menuitem = gtk_menu_item_new_with_mnemonic(
			purple_menu_action_get_label(act));
	}

	list = purple_menu_action_get_children(act);

	if (list == NULL) {
		PurpleCallback callback;

		callback = purple_menu_action_get_callback(act);

		if (callback != NULL) {
			g_object_set_data(G_OBJECT(menuitem),
							  "purplecallback",
							  callback);
			g_object_set_data(G_OBJECT(menuitem),
							  "purplecallbackdata",
							  purple_menu_action_get_data(act));
			g_signal_connect(G_OBJECT(menuitem), "activate",
							 G_CALLBACK(menu_action_cb),
							 object);
		} else {
			gtk_widget_set_sensitive(menuitem, FALSE);
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	} else {
		GList *l = NULL;
		GtkWidget *submenu = NULL;
		GtkAccelGroup *group;

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

		group = gtk_menu_get_accel_group(GTK_MENU(menu));
		if (group) {
			char *path = g_strdup_printf("%s/%s",
				gtk_menu_item_get_accel_path(GTK_MENU_ITEM(menuitem)),
				purple_menu_action_get_label(act));
			gtk_menu_set_accel_path(GTK_MENU(submenu), path);
			g_free(path);
			gtk_menu_set_accel_group(GTK_MENU(submenu), group);
		}

		for (l = list; l; l = l->next) {
			PurpleMenuAction *act = (PurpleMenuAction *)l->data;

			pidgin_append_menu_action(submenu, act, object);
		}
		g_list_free(list);
		purple_menu_action_set_children(act, NULL);
	}
	purple_menu_action_free(act);
	return menuitem;
}

static gboolean buddyname_completion_match_func(GtkEntryCompletion *completion,
		const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	GtkTreeModel *model;
	GValue val1;
	GValue val2;
	const char *tmp;

	model = gtk_entry_completion_get_model(completion);

	val1.g_type = 0;
	gtk_tree_model_get_value(model, iter, COMPLETION_NORMALIZED_COLUMN, &val1);
	tmp = g_value_get_string(&val1);
	if (tmp != NULL && purple_str_has_prefix(tmp, key))
	{
		g_value_unset(&val1);
		return TRUE;
	}
	g_value_unset(&val1);

	val2.g_type = 0;
	gtk_tree_model_get_value(model, iter, COMPLETION_COMPARISON_COLUMN, &val2);
	tmp = g_value_get_string(&val2);
	if (tmp != NULL && purple_str_has_prefix(tmp, key))
	{
		g_value_unset(&val2);
		return TRUE;
	}
	g_value_unset(&val2);

	return FALSE;
}

static gboolean buddyname_completion_match_selected_cb(GtkEntryCompletion *completion,
		GtkTreeModel *model, GtkTreeIter *iter, PidginCompletionData *data)
{
	GValue val;
	GtkWidget *optmenu = data->accountopt;
	PurpleAccount *account;

	val.g_type = 0;
	gtk_tree_model_get_value(model, iter, COMPLETION_BUDDY_COLUMN, &val);
	gtk_entry_set_text(GTK_ENTRY(data->entry), g_value_get_string(&val));
	g_value_unset(&val);

	gtk_tree_model_get_value(model, iter, COMPLETION_ACCOUNT_COLUMN, &val);
	account = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (account == NULL)
		return TRUE;

	if (optmenu != NULL)
		aop_option_menu_select_by_data(optmenu, account);

	return TRUE;
}

static void
add_buddyname_autocomplete_entry(GtkListStore *store, const char *buddy_alias, const char *contact_alias,
								  const PurpleAccount *account, const char *buddyname)
{
	GtkTreeIter iter;
	gboolean completion_added = FALSE;
	gchar *normalized_buddyname;
	gchar *tmp;

	tmp = g_utf8_normalize(buddyname, -1, G_NORMALIZE_DEFAULT);
	normalized_buddyname = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the name and buddy alias match. */
	if (buddy_alias && !purple_strequal(buddy_alias, buddyname)) {
		char *completion_entry = g_strdup_printf("%s \"%s\"", buddyname, buddy_alias);
		char *tmp2 = g_utf8_normalize(buddy_alias, -1, G_NORMALIZE_DEFAULT);

		tmp = g_utf8_casefold(tmp2, -1);
		g_free(tmp2);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				COMPLETION_DISPLAYED_COLUMN, completion_entry,
				COMPLETION_BUDDY_COLUMN, buddyname,
				COMPLETION_NORMALIZED_COLUMN, normalized_buddyname,
				COMPLETION_COMPARISON_COLUMN, tmp,
				COMPLETION_ACCOUNT_COLUMN, account,
				-1);
		g_free(completion_entry);
		g_free(tmp);
		completion_added = TRUE;
	}

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the name and contact alias match. */
	if (contact_alias && !purple_strequal(contact_alias, buddyname)) {
		/* We don't want duplicates when the contact and buddy alias match. */
		if (!purple_strequal(contact_alias, buddy_alias)) {
			char *completion_entry = g_strdup_printf("%s \"%s\"",
							buddyname, contact_alias);
			char *tmp2 = g_utf8_normalize(contact_alias, -1, G_NORMALIZE_DEFAULT);

			tmp = g_utf8_casefold(tmp2, -1);
			g_free(tmp2);

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
					COMPLETION_DISPLAYED_COLUMN, completion_entry,
					COMPLETION_BUDDY_COLUMN, buddyname,
					COMPLETION_NORMALIZED_COLUMN, normalized_buddyname,
					COMPLETION_COMPARISON_COLUMN, tmp,
					COMPLETION_ACCOUNT_COLUMN, account,
					-1);
			g_free(completion_entry);
			g_free(tmp);
			completion_added = TRUE;
		}
	}

	if (completion_added == FALSE) {
		/* Add the buddy's name. */
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				COMPLETION_DISPLAYED_COLUMN, buddyname,
				COMPLETION_BUDDY_COLUMN, buddyname,
				COMPLETION_NORMALIZED_COLUMN, normalized_buddyname,
				COMPLETION_COMPARISON_COLUMN, NULL,
				COMPLETION_ACCOUNT_COLUMN, account,
				-1);
	}

	g_free(normalized_buddyname);
}

static void get_log_set_name(PurpleLogSet *set, gpointer value, PidginCompletionData *data)
{
	PidginFilterBuddyCompletionEntryFunc filter_func = data->filter_func;
	gpointer user_data = data->filter_func_user_data;

	/* 1. Don't show buddies because we will have gotten them already.
	 * 2. The boxes that use this autocomplete code handle only IMs. */
	if (!set->buddy && set->type == PURPLE_LOG_IM) {
		PidginBuddyCompletionEntry entry;
		entry.is_buddy = FALSE;
		entry.entry.logged_buddy = set;

		if (filter_func(&entry, user_data)) {
			add_buddyname_autocomplete_entry(data->store,
												NULL, NULL, set->account, set->name);
		}
	}
}

static void
add_completion_list(PidginCompletionData *data)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	PidginFilterBuddyCompletionEntryFunc filter_func = data->filter_func;
	gpointer user_data = data->filter_func_user_data;
	GHashTable *sets;
	gchar *alias;

	gtk_list_store_clear(data->store);

	for (gnode = purple_blist_get_buddy_list()->root; gnode != NULL; gnode = gnode->next)
	{
		if (!PURPLE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
		{
			if (!PURPLE_IS_CONTACT(cnode))
				continue;

			g_object_get(cnode, "alias", &alias, NULL);

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
			{
				PidginBuddyCompletionEntry entry;
				entry.is_buddy = TRUE;
				entry.entry.buddy = (PurpleBuddy *) bnode;

				if (filter_func(&entry, user_data)) {
					add_buddyname_autocomplete_entry(data->store,
														alias,
														purple_buddy_get_contact_alias(entry.entry.buddy),
														purple_buddy_get_account(entry.entry.buddy),
														purple_buddy_get_name(entry.entry.buddy)
													 );
				}
			}

			g_free(alias);
		}
	}

	sets = purple_log_get_log_sets();
	g_hash_table_foreach(sets, (GHFunc)get_log_set_name, data);
	g_hash_table_destroy(sets);

}

static void
buddyname_autocomplete_destroyed_cb(GtkWidget *widget, gpointer data)
{
	g_free(data);
	purple_signals_disconnect_by_handle(widget);
}

static void
repopulate_autocomplete(gpointer something, gpointer data)
{
	add_completion_list(data);
}

void
pidgin_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *accountopt, PidginFilterBuddyCompletionEntryFunc filter_func, gpointer user_data)
{
	PidginCompletionData *data;

	/*
	 * Store the displayed completion value, the buddy name, the UTF-8
	 * normalized & casefolded buddy name, the UTF-8 normalized &
	 * casefolded value for comparison, and the account.
	 */
	GtkListStore *store;

	GtkEntryCompletion *completion;

	data = g_new0(PidginCompletionData, 1);
	store = gtk_list_store_new(COMPLETION_COLUMN_COUNT, G_TYPE_STRING,
	                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	                           G_TYPE_POINTER);

	data->entry = entry;
	data->accountopt = accountopt;
	if (filter_func == NULL) {
		data->filter_func = pidgin_screenname_autocomplete_default_filter;
		data->filter_func_user_data = NULL;
	} else {
		data->filter_func = filter_func;
		data->filter_func_user_data = user_data;
	}
	data->store = store;

	add_completion_list(data);

	/* Sort the completion list by buddy name */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
	                                     COMPLETION_BUDDY_COLUMN,
	                                     GTK_SORT_ASCENDING);

	completion = gtk_entry_completion_new();
	gtk_entry_completion_set_match_func(completion, buddyname_completion_match_func, NULL, NULL);

	g_signal_connect(G_OBJECT(completion), "match-selected",
		G_CALLBACK(buddyname_completion_match_selected_cb), data);

	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	g_object_unref(completion);

	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_entry_completion_set_text_column(completion, COMPLETION_DISPLAYED_COLUMN);

	purple_signal_connect(purple_connections_get_handle(), "signed-on", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);

	purple_signal_connect(purple_accounts_get_handle(), "account-added", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);

	g_signal_connect(G_OBJECT(entry), "destroy", G_CALLBACK(buddyname_autocomplete_destroyed_cb), data);
}

gboolean
pidgin_screenname_autocomplete_default_filter(const PidginBuddyCompletionEntry *completion_entry, gpointer all_accounts) {
	gboolean all = GPOINTER_TO_INT(all_accounts);

	if (completion_entry->is_buddy) {
		return all || purple_account_is_connected(purple_buddy_get_account(completion_entry->entry.buddy));
	} else {
		return all || (completion_entry->entry.logged_buddy->account != NULL && purple_account_is_connected(completion_entry->entry.logged_buddy->account));
	}
}

void pidgin_set_cursor(GtkWidget *widget, GdkCursorType cursor_type)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	g_return_if_fail(widget != NULL);
	if (gtk_widget_get_window(widget) == NULL)
		return;

	display = gtk_widget_get_display(widget);
	cursor = gdk_cursor_new_for_display(display, cursor_type);
	gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);

	g_object_unref(cursor);

	gdk_display_flush(gdk_window_get_display(gtk_widget_get_window(widget)));
}

void pidgin_clear_cursor(GtkWidget *widget)
{
	g_return_if_fail(widget != NULL);
	if (gtk_widget_get_window(widget) == NULL)
		return;

	gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
}

static void
icon_filesel_choose_cb(GtkWidget *widget, gint response, struct _icon_chooser *dialog)
{
	char *filename, *current_folder;

	if (response != GTK_RESPONSE_ACCEPT) {
		if (response == GTK_RESPONSE_CANCEL) {
			gtk_widget_destroy(dialog->icon_filesel);
		}
		dialog->icon_filesel = NULL;
		if (dialog->callback)
			dialog->callback(NULL, dialog->data);
		g_free(dialog);
		return;
	}

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->icon_filesel));
	current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel));
	if (current_folder != NULL) {
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", current_folder);
		g_free(current_folder);
	}


	if (dialog->callback)
		dialog->callback(filename, dialog->data);
	gtk_widget_destroy(dialog->icon_filesel);
	g_free(filename);
	g_free(dialog);
}


static void
icon_preview_change_cb(GtkFileChooser *widget, struct _icon_chooser *dialog)
{
	GdkPixbuf *pixbuf;
	int height, width;
	char *basename, *markup, *size;
	GStatBuf st;
	char *filename;

	filename = gtk_file_chooser_get_preview_filename(
					GTK_FILE_CHOOSER(dialog->icon_filesel));

	if (!filename || g_stat(filename, &st) || !(pixbuf = pidgin_pixbuf_new_from_file_at_size(filename, 128, 128)))
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), NULL);
		gtk_label_set_markup(GTK_LABEL(dialog->icon_text), "");
		g_free(filename);
		return;
	}

	gdk_pixbuf_get_file_info(filename, &width, &height);
	basename = g_path_get_basename(filename);
	size = purple_str_size_to_units(st.st_size);
	markup = g_strdup_printf(_("<b>File:</b> %s\n"
							   "<b>File size:</b> %s\n"
							   "<b>Image size:</b> %dx%d"),
							 basename, size, width, height);

	gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), pixbuf);
	gtk_label_set_markup(GTK_LABEL(dialog->icon_text), markup);

	g_object_unref(G_OBJECT(pixbuf));
	g_free(filename);
	g_free(basename);
	g_free(size);
	g_free(markup);
}


GtkWidget *pidgin_buddy_icon_chooser_new(GtkWindow *parent, void(*callback)(const char *, gpointer), gpointer data) {
	struct _icon_chooser *dialog = g_new0(struct _icon_chooser, 1);

	GtkWidget *vbox;
	const char *current_folder;

	dialog->callback = callback;
	dialog->data = data;

	current_folder = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder");

	dialog->icon_filesel = gtk_file_chooser_dialog_new(_("Buddy Icon"),
							   parent,
							   GTK_FILE_CHOOSER_ACTION_OPEN,
							   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							   NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog->icon_filesel), GTK_RESPONSE_ACCEPT);
	if ((current_folder != NULL) && (*current_folder != '\0'))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel),
						    current_folder);

	dialog->icon_preview = gtk_image_new();
	dialog->icon_text = gtk_label_new(NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_set_size_request(GTK_WIDGET(vbox), -1, 50);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(dialog->icon_preview), TRUE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(dialog->icon_text), FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog->icon_filesel), vbox);
	gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(dialog->icon_filesel), TRUE);
	gtk_file_chooser_set_use_preview_label(GTK_FILE_CHOOSER(dialog->icon_filesel), FALSE);

	g_signal_connect(G_OBJECT(dialog->icon_filesel), "update-preview",
					 G_CALLBACK(icon_preview_change_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "response",
					 G_CALLBACK(icon_filesel_choose_cb), dialog);
	icon_preview_change_cb(NULL, dialog);

#ifdef _WIN32
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "show",
		G_CALLBACK(winpidgin_ensure_onscreen), dialog->icon_filesel);
#endif

	return dialog->icon_filesel;
}

/*
 * str_array_match:
 *
 * Returns: %TRUE if any string from array @a exists in array @b.
 */
static gboolean
str_array_match(char **a, char **b)
{
	int i, j;

	if (!a || !b)
		return FALSE;
	for (i = 0; a[i] != NULL; i++)
		for (j = 0; b[j] != NULL; j++)
			if (!g_ascii_strcasecmp(a[i], b[j]))
				return TRUE;
	return FALSE;
}

gpointer
pidgin_convert_buddy_icon(PurpleProtocol *protocol, const char *path, size_t *len)
{
	PurpleBuddyIconSpec *spec;
	int orig_width, orig_height, new_width, new_height;
	GdkPixbufFormat *format;
	char **pixbuf_formats;
	char **protocol_formats;
	GError *error = NULL;
	gchar *contents;
	gsize length;
	GdkPixbuf *pixbuf, *original;
	float scale_factor;
	int i;
	gchar *tmp;

	spec = purple_protocol_get_icon_spec(protocol);
	g_return_val_if_fail(spec->format != NULL, NULL);

	format = gdk_pixbuf_get_file_info(path, &orig_width, &orig_height);
	if (format == NULL) {
		purple_debug_warning("buddyicon", "Could not get file info of %s\n", path);
		return NULL;
	}

	pixbuf_formats = gdk_pixbuf_format_get_extensions(format);
	protocol_formats = g_strsplit(spec->format, ",", 0);

	if (str_array_match(pixbuf_formats, protocol_formats) && /* This is an acceptable format AND */
		 (!(spec->scale_rules & PURPLE_ICON_SCALE_SEND) || /* The protocol doesn't scale before it sends OR */
		  (spec->min_width <= orig_width && spec->max_width >= orig_width &&
		   spec->min_height <= orig_height && spec->max_height >= orig_height))) /* The icon is the correct size */
	{
		g_strfreev(pixbuf_formats);

		if (!g_file_get_contents(path, &contents, &length, &error)) {
			purple_debug_warning("buddyicon", "Could not get file contents "
					"of %s: %s\n", path, error->message);
			g_strfreev(protocol_formats);
			return NULL;
		}

		if (spec->max_filesize == 0 || length < spec->max_filesize) {
			/* The supplied image fits the file size, dimensions and type
			   constraints.  Great!  Return it without making any changes. */
			if (len)
				*len = length;
			g_strfreev(protocol_formats);
			return contents;
		}

		/* The image was too big.  Fall-through and try scaling it down. */
		g_free(contents);
	} else {
		g_strfreev(pixbuf_formats);
	}

	/* The original image wasn't compatible.  Scale it or convert file type. */
	pixbuf = gdk_pixbuf_new_from_file(path, &error);
	if (error) {
		purple_debug_warning("buddyicon", "Could not open icon '%s' for "
				"conversion: %s\n", path, error->message);
		g_error_free(error);
		g_strfreev(protocol_formats);
		return NULL;
	}
	original = g_object_ref(G_OBJECT(pixbuf));

	new_width = orig_width;
	new_height = orig_height;

	/* Make sure the image is the correct dimensions */
	if (spec->scale_rules & PURPLE_ICON_SCALE_SEND &&
		(orig_width < spec->min_width || orig_width > spec->max_width ||
		 orig_height < spec->min_height || orig_height > spec->max_height))
	{
		purple_buddy_icon_spec_get_scaled_size(spec, &new_width, &new_height);

		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = gdk_pixbuf_scale_simple(original, new_width, new_height, GDK_INTERP_HYPER);
	}

	scale_factor = 1;
	do {
		for (i = 0; protocol_formats[i]; i++) {
			int quality = 100;
			do {
				const char *key = NULL;
				const char *value = NULL;
				gchar tmp_buf[4];

				purple_debug_info("buddyicon", "Converting buddy icon to %s\n", protocol_formats[i]);

				if (purple_strequal(protocol_formats[i], "png")) {
					key = "compression";
					value = "9";
				} else if (purple_strequal(protocol_formats[i], "jpeg")) {
					sprintf(tmp_buf, "%u", quality);
					key = "quality";
					value = tmp_buf;
				}

				if (!gdk_pixbuf_save_to_buffer(pixbuf, &contents, &length,
						protocol_formats[i], &error, key, value, NULL))
				{
					/* The NULL checking of error is necessary due to this bug:
					 * http://bugzilla.gnome.org/show_bug.cgi?id=405539 */
					purple_debug_warning("buddyicon",
							"Could not convert to %s: %s\n", protocol_formats[i],
							(error && error->message) ? error->message : "Unknown error");
					g_error_free(error);
					error = NULL;

					/* We couldn't convert to this image type.  Try the next
					   image type. */
					break;
				}

				if (spec->max_filesize == 0 || length <= spec->max_filesize) {
					/* We were able to save the image as this image type and
					   have it be within the size constraints.  Great!  Return
					   the image. */
					purple_debug_info("buddyicon", "Converted image from "
							"%dx%d to %dx%d, format=%s, quality=%u, "
							"filesize=%" G_GSIZE_FORMAT "\n",
							orig_width, orig_height, new_width, new_height,
							protocol_formats[i], quality, length);
					if (len)
						*len = length;
					g_strfreev(protocol_formats);
					g_object_unref(G_OBJECT(pixbuf));
					g_object_unref(G_OBJECT(original));
					return contents;
				}

				g_free(contents);

				if (!purple_strequal(protocol_formats[i], "jpeg")) {
					/* File size was too big and we can't lower the quality,
					   so skip to the next image type. */
					break;
				}

				/* File size was too big, but we're dealing with jpeg so try
				   lowering the quality. */
				quality -= 5;
			} while (quality >= 70);
		}

		/* We couldn't save the image in any format that was below the max
		   file size.  Maybe we can reduce the image dimensions? */
		scale_factor *= 0.8;
		new_width = orig_width * scale_factor;
		new_height = orig_height * scale_factor;
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = gdk_pixbuf_scale_simple(original, new_width, new_height, GDK_INTERP_HYPER);
	} while ((new_width > 10 || new_height > 10) && new_width > spec->min_width && new_height > spec->min_height);
	g_strfreev(protocol_formats);
	g_object_unref(G_OBJECT(pixbuf));
	g_object_unref(G_OBJECT(original));

	tmp = g_strdup_printf(_("The file '%s' is too large for %s.  Please try a smaller image.\n"),
			path, purple_protocol_get_name(protocol));
	purple_notify_error(NULL, _("Icon Error"), _("Could not set icon"), tmp, NULL);
	g_free(tmp);

	return NULL;
}

char *pidgin_make_pretty_arrows(const char *str)
{
	char *ret;
	char **split = g_strsplit(str, "->", -1);
	ret = g_strjoinv("\342\207\250", split);
	g_strfreev(split);

	split = g_strsplit(ret, "<-", -1);
	g_free(ret);
	ret = g_strjoinv("\342\207\246", split);
	g_strfreev(split);

	return ret;
}

void pidgin_set_urgent(GtkWindow *window, gboolean urgent)
{
#if defined _WIN32
	winpidgin_window_flash(window, urgent);
#else
	gtk_window_set_urgency_hint(window, urgent);
#endif
}

static void *
pidgin_utils_get_handle(void)
{
	static int handle;

	return &handle;
}

static void connection_signed_off_cb(PurpleConnection *gc)
{
	GSList *list, *l_next;
	for (list = minidialogs; list; list = l_next) {
		l_next = list->next;
		if (g_object_get_data(G_OBJECT(list->data), "gc") == gc) {
				gtk_widget_destroy(GTK_WIDGET(list->data));
		}
	}
}

static void alert_killed_cb(GtkWidget *widget)
{
	minidialogs = g_slist_remove(minidialogs, widget);
}

static void
old_mini_dialog_button_clicked_cb(PidginMiniDialog *mini_dialog,
                                  GtkButton *button,
                                  gpointer user_data)
{
	struct _old_button_clicked_cb_data *data = user_data;
	data->cb(data->data, button);
}

static void
old_mini_dialog_destroy_cb(GtkWidget *dialog,
                           GList *cb_datas)
{
	while (cb_datas != NULL)
	{
		g_free(cb_datas->data);
		cb_datas = g_list_delete_link(cb_datas, cb_datas);
	}
}

static void
mini_dialog_init(PidginMiniDialog *mini_dialog, PurpleConnection *gc, void *user_data, va_list args)
{
	const char *button_text;
	GList *cb_datas = NULL;
	static gboolean first_call = TRUE;

	if (first_call) {
		first_call = FALSE;
		purple_signal_connect(purple_connections_get_handle(), "signed-off",
		                      pidgin_utils_get_handle(),
		                      PURPLE_CALLBACK(connection_signed_off_cb), NULL);
	}

	g_object_set_data(G_OBJECT(mini_dialog), "gc" ,gc);
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		G_CALLBACK(alert_killed_cb), NULL);

	while ((button_text = va_arg(args, char*))) {
		struct _old_button_clicked_cb_data *data = NULL;
		PidginMiniDialogCallback wrapper_cb = NULL;
		PidginUtilMiniDialogCallback callback =
			va_arg(args, PidginUtilMiniDialogCallback);

		if (callback != NULL) {
			data = g_new0(struct _old_button_clicked_cb_data, 1);
			data->cb = callback;
			data->data = user_data;
			wrapper_cb = old_mini_dialog_button_clicked_cb;
		}
		pidgin_mini_dialog_add_button(mini_dialog, button_text,
			wrapper_cb, data);
		cb_datas = g_list_append(cb_datas, data);
	}

	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		G_CALLBACK(old_mini_dialog_destroy_cb), cb_datas);
}

#define INIT_AND_RETURN_MINI_DIALOG(mini_dialog) \
	va_list args; \
	va_start(args, user_data); \
	mini_dialog_init(mini_dialog, gc, user_data, args); \
	va_end(args); \
	return GTK_WIDGET(mini_dialog);

GtkWidget *
pidgin_make_mini_dialog(PurpleConnection *gc,
                        const char *icon_name,
                        const char *primary,
                        const char *secondary,
                        void *user_data,
                        ...)
{
	PidginMiniDialog *mini_dialog = pidgin_mini_dialog_new(primary, secondary, icon_name);
	INIT_AND_RETURN_MINI_DIALOG(mini_dialog);
}

GtkWidget *
pidgin_make_mini_dialog_with_custom_icon(PurpleConnection *gc,
					GdkPixbuf *custom_icon,
					const char *primary,
					const char *secondary,
					void *user_data,
					...)
{
	PidginMiniDialog *mini_dialog = pidgin_mini_dialog_new_with_custom_icon(primary, secondary, custom_icon);
	INIT_AND_RETURN_MINI_DIALOG(mini_dialog);
}

/*
 * "This is so dead sexy."
 * "Two thumbs up."
 * "Best movie of the year."
 *
 * This is the function that handles CTRL+F searching in the buddy list.
 * It finds the top-most buddy/group/chat/whatever containing the
 * entered string.
 *
 * It's somewhat ineffecient, because we strip all the HTML from the
 * "name" column of the buddy list (because the GtkTreeModel does not
 * contain the screen name in a non-markedup format).  But the alternative
 * is to add an extra column to the GtkTreeModel.  And this function is
 * used rarely, so it shouldn't matter TOO much.
 */
gboolean pidgin_tree_view_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data)
{
	gchar *enteredstring;
	gchar *tmp;
	gchar *withmarkup;
	gchar *nomarkup;
	gchar *normalized;
	gboolean result;
	size_t i;
	size_t len;
	PangoLogAttr *log_attrs;
	gchar *word;

	if (g_ascii_strcasecmp(key, "Global Thermonuclear War") == 0)
	{
		purple_notify_info(NULL, "WOPR", "Wouldn't you prefer a nice "
			"game of chess?", NULL, NULL);
		return FALSE;
	}

	gtk_tree_model_get(model, iter, column, &withmarkup, -1);
	if (withmarkup == NULL)   /* This is probably a separator */
		return TRUE;

	tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
	enteredstring = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	nomarkup = purple_markup_strip_html(withmarkup);
	tmp = g_utf8_normalize(nomarkup, -1, G_NORMALIZE_DEFAULT);
	g_free(nomarkup);
	normalized = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	if (purple_str_has_prefix(normalized, enteredstring))
	{
		g_free(withmarkup);
		g_free(enteredstring);
		g_free(normalized);
		return FALSE;
	}


	/* Use Pango to separate by words. */
	len = g_utf8_strlen(normalized, -1);
	log_attrs = g_new(PangoLogAttr, len + 1);

	pango_get_log_attrs(normalized, strlen(normalized), -1, NULL, log_attrs, len + 1);

	word = normalized;
	result = TRUE;
	for (i = 0; i < (len - 1) ; i++)
	{
		if (log_attrs[i].is_word_start &&
		    purple_str_has_prefix(word, enteredstring))
		{
			result = FALSE;
			break;
		}
		word = g_utf8_next_char(word);
	}
	g_free(log_attrs);

/* The non-Pango version. */
#if 0
	word = normalized;
	result = TRUE;
	while (word[0] != '\0')
	{
		gunichar c = g_utf8_get_char(word);
		if (!g_unichar_isalnum(c))
		{
			word = g_utf8_find_next_char(word, NULL);
			if (purple_str_has_prefix(word, enteredstring))
			{
				result = FALSE;
				break;
			}
		}
		else
			word = g_utf8_find_next_char(word, NULL);
	}
#endif

	g_free(withmarkup);
	g_free(enteredstring);
	g_free(normalized);

	return result;
}


gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf) {
	int height, rowstride, i;
	unsigned char *pixels;
	unsigned char *row;

	if (!gdk_pixbuf_get_has_alpha(pixbuf))
		return TRUE;

	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	row = pixels;
	for (i = 3; i < rowstride; i+=4) {
		if (row[i] < 0xfe)
			return FALSE;
	}

	for (i = 1; i < height - 1; i++) {
		row = pixels + (i * rowstride);
		if (row[3] < 0xfe || row[rowstride - 1] < 0xfe) {
			return FALSE;
	    }
	}

	row = pixels + ((height - 1) * rowstride);
	for (i = 3; i < rowstride; i += 4) {
		if (row[i] < 0xfe)
			return FALSE;
	}

	return TRUE;
}

void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf) {
	int width, height, rowstride;
	guchar *pixels;
	if (!gdk_pixbuf_get_has_alpha(pixbuf))
		return;
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	if (width < 6 || height < 6)
		return;
	/* Top left */
	pixels[3] = 0;
	pixels[7] = 0x80;
	pixels[11] = 0xC0;
	pixels[rowstride + 3] = 0x80;
	pixels[rowstride * 2 + 3] = 0xC0;

	/* Top right */
	pixels[width * 4 - 1] = 0;
	pixels[width * 4 - 5] = 0x80;
	pixels[width * 4 - 9] = 0xC0;
	pixels[rowstride + (width * 4) - 1] = 0x80;
	pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

	/* Bottom left */
	pixels[(height - 1) * rowstride + 3] = 0;
	pixels[(height - 1) * rowstride + 7] = 0x80;
	pixels[(height - 1) * rowstride + 11] = 0xC0;
	pixels[(height - 2) * rowstride + 3] = 0x80;
	pixels[(height - 3) * rowstride + 3] = 0xC0;

	/* Bottom right */
	pixels[height * rowstride - 1] = 0;
	pixels[(height - 1) * rowstride - 1] = 0x80;
	pixels[(height - 2) * rowstride - 1] = 0xC0;
	pixels[height * rowstride - 5] = 0x80;
	pixels[height * rowstride - 9] = 0xC0;
}

const char *pidgin_get_dim_grey_string(GtkWidget *widget) {
	static char dim_grey_string[8] = "";
	GtkStyleContext *context;
	GdkRGBA fg, bg;

	if (!widget)
		return "dim grey";

	context = gtk_widget_get_style_context(widget);
	if (!context)
		return "dim grey";

	gtk_style_context_get_color(context, gtk_style_context_get_state(context),
	                            &fg);
	gtk_style_context_get_background_color(context,
	                                       gtk_style_context_get_state(context),
	                                       &bg);
	snprintf(dim_grey_string, sizeof(dim_grey_string), "#%02x%02x%02x",
		 (unsigned int)((fg.red + bg.red) * 0.5 * 255),
		 (unsigned int)((fg.green + bg.green) * 0.5 * 255),
		 (unsigned int)((fg.blue + bg.blue) * 0.5 * 255));
	return dim_grey_string;
}

static void
combo_box_changed_cb(GtkComboBoxText *combo_box, GtkEntry *entry)
{
	char *text = gtk_combo_box_text_get_active_text(combo_box);
	gtk_entry_set_text(entry, text ? text : "");
	g_free(text);
}

static gboolean
entry_key_pressed_cb(GtkWidget *entry, GdkEventKey *key, GtkComboBoxText *combo)
{
	if (key->keyval == GDK_KEY_Down || key->keyval == GDK_KEY_Up) {
		gtk_combo_box_popup(GTK_COMBO_BOX(combo));
		return TRUE;
	}
	return FALSE;
}

GtkWidget *
pidgin_text_combo_box_entry_new(const char *default_item, GList *items)
{
	GtkComboBoxText *ret = NULL;
	GtkWidget *the_entry = NULL;

	ret = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new_with_entry());
	the_entry = gtk_bin_get_child(GTK_BIN(ret));

	if (default_item)
		gtk_entry_set_text(GTK_ENTRY(the_entry), default_item);

	for (; items != NULL ; items = items->next) {
		char *text = items->data;
		if (text && *text)
			gtk_combo_box_text_append_text(ret, text);
	}

	g_signal_connect(G_OBJECT(ret), "changed", (GCallback)combo_box_changed_cb, the_entry);
	g_signal_connect_after(G_OBJECT(the_entry), "key-press-event", G_CALLBACK(entry_key_pressed_cb), ret);

	return GTK_WIDGET(ret);
}

const char *pidgin_text_combo_box_entry_get_text(GtkWidget *widget)
{
	return gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((widget)))));
}

void pidgin_text_combo_box_entry_set_text(GtkWidget *widget, const char *text)
{
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((widget)))), (text));
}

GtkWidget *
pidgin_add_widget_to_vbox(GtkBox *vbox, const char *widget_label, GtkSizeGroup *sg, GtkWidget *widget, gboolean expand, GtkWidget **p_label)
{
	GtkWidget *hbox;
	GtkWidget *label = NULL;

	if (widget_label) {
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_widget_show(hbox);
		gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(widget_label);
		gtk_widget_show(label);
		if (sg) {
			gtk_label_set_xalign(GTK_LABEL(label), 0);
			gtk_size_group_add_widget(sg, label);
		}
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	} else {
		hbox = GTK_WIDGET(vbox);
	}

	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, expand, TRUE, 0);
	if (label) {
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
		pidgin_set_accessible_label(widget, GTK_LABEL(label));
	}

	if (p_label)
		(*p_label) = label;
	return hbox;
}

gboolean pidgin_auto_parent_window(GtkWidget *widget)
{
#if 0
	/* This looks at the most recent window that received focus, and makes
	 * that the parent window. */
#ifndef _WIN32
	static GdkAtom _WindowTime = GDK_NONE;
	static GdkAtom _Cardinal = GDK_NONE;
	GList *windows = NULL;
	GtkWidget *parent = NULL;
	time_t window_time = 0;

	windows = gtk_window_list_toplevels();

	if (_WindowTime == GDK_NONE) {
		_WindowTime = gdk_x11_xatom_to_atom(gdk_x11_get_xatom_by_name("_NET_WM_USER_TIME"));
	}
	if (_Cardinal == GDK_NONE) {
		_Cardinal = gdk_atom_intern("CARDINAL", FALSE);
	}

	while (windows) {
		GtkWidget *window = windows->data;
		guchar *data = NULL;
		int al = 0;
		time_t value;

		windows = g_list_delete_link(windows, windows);

		if (window == widget ||
				!gtk_widget_get_visible(window))
			continue;

		if (!gdk_property_get(window->window, _WindowTime, _Cardinal, 0, sizeof(time_t), FALSE,
				NULL, NULL, &al, &data))
			continue;
		value = *(time_t *)data;
		if (window_time < value) {
			window_time = value;
			parent = window;
		}
		g_free(data);
	}
	if (windows)
		g_list_free(windows);
	if (parent) {
		if (!gtk_get_current_event() && gtk_window_has_toplevel_focus(GTK_WINDOW(parent))) {
			/* The window is in focus, and the new window was not triggered by a keypress/click
			 * event. So do not set it transient, to avoid focus stealing and all that.
			 */
			return FALSE;
		}
		gtk_window_set_transient_for(GTK_WINDOW(widget), GTK_WINDOW(parent));
		return TRUE;
	}
	return FALSE;
#endif
#else
	/* This finds the currently active window and makes that the parent window. */
	GList *windows = NULL;
	GtkWindow *parent = NULL;
	GdkEvent *event = gtk_get_current_event();
	GdkWindow *menu = NULL;
	gpointer parent_from;
	PurpleNotifyType notify_type;

	parent_from = g_object_get_data(G_OBJECT(widget), "pidgin-parent-from");
	if (purple_request_is_valid_ui_handle(parent_from, NULL)) {
		
		gtk_window_set_transient_for(GTK_WINDOW(widget),
			gtk_window_get_transient_for(
				pidgin_request_get_dialog_window(parent_from)));
		return TRUE;
	}
	if (purple_notify_is_valid_ui_handle(parent_from, &notify_type) &&
		notify_type == PURPLE_NOTIFY_MESSAGE)
	{
		gtk_window_set_transient_for(GTK_WINDOW(widget),
			gtk_window_get_transient_for(GTK_WINDOW(parent_from)));
		return TRUE;
	}

	if (event == NULL)
		/* The window was not triggered by a user action. */
		return FALSE;

	/* We need to special case events from a popup menu. */
	if (event->type == GDK_BUTTON_RELEASE) {
		/* XXX: Neither of the following works:
			menu = event->button.window;
			menu = gdk_window_get_parent(event->button.window);
			menu = gdk_window_get_toplevel(event->button.window);
		*/
	} else if (event->type == GDK_KEY_PRESS)
		menu = event->key.window;

	windows = gtk_window_list_toplevels();
	while (windows) {
		GtkWindow *window = GTK_WINDOW(windows->data);
		windows = g_list_delete_link(windows, windows);

		if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(window),
			"pidgin-window-is-closing")))
		{
			parent = gtk_window_get_transient_for(window);
			break;
		}

		if (GTK_WIDGET(window) == widget ||
				!gtk_widget_get_visible(GTK_WIDGET(window))) {
			continue;
		}

		if (gtk_window_has_toplevel_focus(window) ||
				(menu && menu == gtk_widget_get_window(GTK_WIDGET(window)))) {
			parent = window;
			break;
		}
	}
	if (windows)
		g_list_free(windows);
	if (parent) {
		gtk_window_set_transient_for(GTK_WINDOW(widget), parent);
		return TRUE;
	}
	return FALSE;
#endif
}

static GObject *pidgin_pixbuf_from_data_helper(const guchar *buf, gsize count, gboolean animated)
{
	GObject *pixbuf;
	GdkPixbufLoader *loader;
	GError *error = NULL;

	loader = gdk_pixbuf_loader_new();

	if (!gdk_pixbuf_loader_write(loader, buf, count, &error) || error) {
		purple_debug_warning("gtkutils", "gdk_pixbuf_loader_write() "
				"failed with size=%" G_GSIZE_FORMAT ": %s\n", count,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		g_object_unref(G_OBJECT(loader));
		return NULL;
	}

	if (!gdk_pixbuf_loader_close(loader, &error) || error) {
		purple_debug_warning("gtkutils", "gdk_pixbuf_loader_close() "
				"failed for image of size %" G_GSIZE_FORMAT ": %s\n", count,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		g_object_unref(G_OBJECT(loader));
		return NULL;
	}

	if (animated)
		pixbuf = G_OBJECT(gdk_pixbuf_loader_get_animation(loader));
	else
		pixbuf = G_OBJECT(gdk_pixbuf_loader_get_pixbuf(loader));
	if (!pixbuf) {
		purple_debug_warning("gtkutils", "%s() returned NULL for image "
				"of size %" G_GSIZE_FORMAT "\n",
				animated ? "gdk_pixbuf_loader_get_animation"
					: "gdk_pixbuf_loader_get_pixbuf", count);
		g_object_unref(G_OBJECT(loader));
		return NULL;
	}

	g_object_ref(pixbuf);
	g_object_unref(G_OBJECT(loader));

	return pixbuf;
}

GdkPixbuf *pidgin_pixbuf_from_data(const guchar *buf, gsize count)
{
	return GDK_PIXBUF(pidgin_pixbuf_from_data_helper(buf, count, FALSE));
}

GdkPixbufAnimation *pidgin_pixbuf_anim_from_data(const guchar *buf, gsize count)
{
	return GDK_PIXBUF_ANIMATION(pidgin_pixbuf_from_data_helper(buf, count, TRUE));
}

GdkPixbuf *
pidgin_pixbuf_from_image(PurpleImage *image)
{
	return pidgin_pixbuf_from_data(purple_image_get_data(image),
		purple_image_get_data_size(image));
}

GdkPixbuf *pidgin_pixbuf_new_from_file(const gchar *filename)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	g_return_val_if_fail(filename != NULL, NULL);
	g_return_val_if_fail(filename[0] != '\0', NULL);

	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (!pixbuf || error) {
		purple_debug_warning("gtkutils", "gdk_pixbuf_new_from_file() "
				"returned %s for file %s: %s\n",
				pixbuf ? "something" : "nothing",
				filename,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		if (pixbuf)
			g_object_unref(G_OBJECT(pixbuf));
		return NULL;
	}

	return pixbuf;
}

GdkPixbuf *pidgin_pixbuf_new_from_file_at_size(const char *filename, int width, int height)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	g_return_val_if_fail(filename != NULL, NULL);
	g_return_val_if_fail(filename[0] != '\0', NULL);

	pixbuf = gdk_pixbuf_new_from_file_at_size(filename,
			width, height, &error);
	if (!pixbuf || error) {
		purple_debug_warning("gtkutils", "gdk_pixbuf_new_from_file_at_size() "
				"returned %s for file %s: %s\n",
				pixbuf ? "something" : "nothing",
				filename,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		if (pixbuf)
			g_object_unref(G_OBJECT(pixbuf));
		return NULL;
	}

	return pixbuf;
}

GdkPixbuf *pidgin_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, gboolean preserve_aspect_ratio)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	g_return_val_if_fail(filename != NULL, NULL);
	g_return_val_if_fail(filename[0] != '\0', NULL);

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
			width, height, preserve_aspect_ratio, &error);
	if (!pixbuf || error) {
		purple_debug_warning("gtkutils", "gdk_pixbuf_new_from_file_at_scale() "
				"returned %s for file %s: %s\n",
				pixbuf ? "something" : "nothing",
				filename,
				error ? error->message : "(no error message)");
		if (error)
			g_error_free(error);
		if (pixbuf)
			g_object_unref(G_OBJECT(pixbuf));
		return NULL;
	}

	return pixbuf;
}

GdkPixbuf *
pidgin_pixbuf_scale_down(GdkPixbuf *src, guint max_width, guint max_height,
	GdkInterpType interp_type, gboolean preserve_ratio)
{
	guint cur_w, cur_h;
	GdkPixbuf *dst;

	g_return_val_if_fail(src != NULL, NULL);

	if (max_width == 0 || max_height == 0) {
		g_object_unref(src);
		g_return_val_if_reached(NULL);
	}

	cur_w = gdk_pixbuf_get_width(src);
	cur_h = gdk_pixbuf_get_height(src);

	if (cur_w <= max_width && cur_h <= max_height)
		return src;

	/* cur_ratio = cur_w / cur_h
	 * max_ratio = max_w / max_h
	 */

	if (!preserve_ratio) {
		cur_w = MIN(cur_w, max_width);
		cur_h = MIN(cur_h, max_height);
	} else if ((guint64)cur_w * max_height > (guint64)max_width * cur_h) {
		/* cur_w / cur_h > max_width / max_height */
		cur_h = (guint64)max_width * cur_h / cur_w;
		cur_w = max_width;
	} else {
		cur_w = (guint64)max_height * cur_w / cur_h;
		cur_h = max_height;
	}

	if (cur_w <= 0)
		cur_w = 1;
	if (cur_h <= 0)
		cur_h = 1;

	dst = gdk_pixbuf_scale_simple(src, cur_w, cur_h, interp_type);
	g_object_unref(src);

	return dst;
}

static void
url_copy(GtkWidget *w, gchar *url)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
}

static gboolean
link_context_menu(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	char *url;

	url = webkit_dom_html_anchor_element_get_href(link);

	/* Open Link */
	img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Open Link"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_swapped(G_OBJECT(item), "activate",
	                         G_CALLBACK(pidgin_webview_activate_anchor), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Copy Link Location */
	img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Link Location"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	/* The signal owns url now: */
	g_signal_connect_data(G_OBJECT(item), "activate", G_CALLBACK(url_copy),
	                      (gpointer)url, (GClosureNotify)g_free, 0);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

static gboolean
copy_email_address(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	char *text;
	char *address;
#define MAILTOSIZE  (sizeof("mailto:") - 1)

	text = webkit_dom_html_anchor_element_get_href(link);
	if (!text || strlen(text) <= MAILTOSIZE) {
		g_free(text);
		return FALSE;
	}
	address = text + MAILTOSIZE;

	/* Copy Email Address */
	img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Email Address"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_data(G_OBJECT(item), "activate", G_CALLBACK(url_copy),
	                      g_strdup(address), (GClosureNotify)g_free, 0);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	g_free(text);

	return TRUE;
}

/*
 * open_file:
 * @filename: The path to a file. Specifically this is the link target
 *        from a link in an IM window with the leading "file://" removed.
 */
static void
open_file(PidginWebView *webview, const char *filename)
{
	/* Copied from gtkft.c:open_button_cb */
#ifdef _WIN32
	/* If using Win32... */
	int code;
	/* Escape URI by replacing double-quote with 2 double-quotes. */
	gchar *escaped = purple_strreplace(filename, "\"", "\"\"");
	gchar *param = g_strconcat("/select,\"", escaped, "\"", NULL);
	wchar_t *wc_param = g_utf8_to_utf16(param, -1, NULL, NULL, NULL);

	/* TODO: Better to use SHOpenFolderAndSelectItems()? */
	code = (int)ShellExecuteW(NULL, L"OPEN", L"explorer.exe", wc_param, NULL, SW_NORMAL);

	g_free(wc_param);
	g_free(param);
	g_free(escaped);

	if (code == SE_ERR_ASSOCINCOMPLETE || code == SE_ERR_NOASSOC)
	{
		purple_notify_error(webview, NULL,
				_("There is no application configured to open this type of file."),
				NULL, NULL);
	}
	else if (code < 32)
	{
		purple_notify_error(webview, NULL,
				_("An error occurred while opening the file."), NULL, NULL);
		purple_debug_warning("gtkutils", "filename: %s; code: %d\n",
				filename, code);
	}
#else
	char *command = NULL;
	char *tmp = NULL;
	GError *error = NULL;

	if (purple_running_gnome())
	{
		char *escaped = g_shell_quote(filename);
		command = g_strdup_printf("gnome-open %s", escaped);
		g_free(escaped);
	}
	else if (purple_running_kde())
	{
		char *escaped = g_shell_quote(filename);

		if (purple_str_has_suffix(filename, ".desktop"))
			command = g_strdup_printf("kfmclient openURL %s 'text/plain'", escaped);
		else
			command = g_strdup_printf("kfmclient openURL %s", escaped);
		g_free(escaped);
	}
	else
	{
		purple_notify_uri(NULL, filename);
		return;
	}

	if (purple_program_is_valid(command))
	{
		gint exit_status;
		if (!g_spawn_command_line_sync(command, NULL, NULL, &exit_status, &error))
		{
			tmp = g_strdup_printf(_("Error launching %s: %s"),
							filename, error->message);
			purple_notify_error(webview, NULL, _("Unable to open file."), tmp, NULL);
			g_free(tmp);
			g_error_free(error);
		}
		if (exit_status != 0)
		{
			char *primary = g_strdup_printf(_("Error running %s"), command);
			char *secondary = g_strdup_printf(_("Process returned error code %d"),
									exit_status);
			purple_notify_error(webview, NULL, primary, secondary, NULL);
			g_free(tmp);
		}
	}
#endif
}

#define FILELINKSIZE  (sizeof("file://") - 1)
static gboolean
file_clicked_cb(PidginWebView *webview, const char *uri)
{
	/* Strip "file://" from the URI. */
	open_file(webview, uri + FILELINKSIZE);
	return TRUE;
}

static gboolean
open_containing_cb(PidginWebView *webview, const char *uri)
{
	char *dir = g_path_get_dirname(uri + FILELINKSIZE);
	open_file(webview, dir);
	g_free(dir);
	return TRUE;
}

static gboolean
file_context_menu(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	char *url;

	url = webkit_dom_html_anchor_element_get_href(link);

	/* Open File */
	img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Open File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_swapped(G_OBJECT(item), "activate",
	                         G_CALLBACK(pidgin_webview_activate_anchor), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Open Containing Directory */
	img = gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("Open _Containing Directory"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	/* The signal owns url now: */
	g_signal_connect_data(G_OBJECT(item), "activate",
	                      G_CALLBACK(open_containing_cb), (gpointer)url,
	                      (GClosureNotify)g_free, 0);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

#define AUDIOLINKSIZE  (sizeof("audio://") - 1)
static gboolean
audio_clicked_cb(PidginWebView *webview, const char *uri)
{
	PidginConversation *conv = g_object_get_data(G_OBJECT(webview), "gtkconv");
	if (!conv) /* no playback in debug window */
		return TRUE;
	purple_sound_play_file(uri + AUDIOLINKSIZE, NULL);
	return TRUE;
}

static void
savefile_write_cb(gpointer user_data, char *file)
{
	char *temp_file = user_data;
	gchar *contents;
	gsize length;
	GError *error = NULL;

	if (!g_file_get_contents(temp_file, &contents, &length, &error)) {
		purple_debug_error("gtkutils", "Unable to read contents of %s: %s\n",
		                   temp_file, error->message);
		g_error_free(error);
		g_free(temp_file);
		return;
	}
	g_free(temp_file);

	if (!purple_util_write_data_to_file_absolute(file, contents, length)) {
		purple_debug_error("gtkutils", "Unable to write contents to %s\n",
		                   file);
	}
}

static gboolean
save_file_cb(GtkWidget *item, const char *url)
{
	PidginConversation *gtkconv = g_object_get_data(G_OBJECT(item), "gtkconv");
	PurpleConversation *conv;
	if (!gtkconv)
		return TRUE;
	conv = gtkconv->active_conv;
	purple_request_file(conv, _("Save File"), NULL, TRUE,
		G_CALLBACK(savefile_write_cb), G_CALLBACK(g_free),
		purple_request_cpar_from_conversation(conv),
		(gpointer)g_strdup(url));
	return TRUE;
}

static gboolean
audio_context_menu(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	char *url;
	PidginConversation *conv = g_object_get_data(G_OBJECT(webview), "gtkconv");
	if (!conv) /* No menu in debug window */
		return TRUE;

	url = webkit_dom_html_anchor_element_get_href(link);

	/* Play Sound */
	img = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Play Sound"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_swapped(G_OBJECT(item), "activate",
	                         G_CALLBACK(pidgin_webview_activate_anchor), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Save File */
	img = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Save File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_data(G_OBJECT(item), "activate",
	                      G_CALLBACK(save_file_cb), g_strdup(url+AUDIOLINKSIZE),
	                      (GClosureNotify)g_free, 0);
	g_object_set_data(G_OBJECT(item), "gtkconv", conv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	g_free(url);

	return TRUE;
}

/* XXX: The following two functions are for demonstration purposes only! */
static gboolean
open_dialog(PidginWebView *webview, const char *url)
{
	const char *str;

	if (!url || strlen(url) < sizeof("open://")) {
		return FALSE;
	}

	str = url + sizeof("open://") - 1;

	if (purple_strequal(str, "accounts"))
		pidgin_accounts_window_show();
	else if (purple_strequal(str, "prefs"))
		pidgin_prefs_show();
	else
		return FALSE;
	return TRUE;
}

static gboolean
dummy(PidginWebView *webview, WebKitDOMHTMLAnchorElement *link, GtkWidget *menu)
{
	return TRUE;
}

#ifdef _WIN32
static void
winpidgin_register_win32_url_handlers(void)
{
	int idx = 0;
	LONG ret = ERROR_SUCCESS;

	do {
		DWORD nameSize = 256;
		wchar_t start[256];
		ret = RegEnumKeyExW(HKEY_CLASSES_ROOT, idx++, start, &nameSize,
							NULL, NULL, NULL, NULL);
		if (ret == ERROR_SUCCESS) {
			HKEY reg_key = NULL;
			ret = RegOpenKeyExW(HKEY_CLASSES_ROOT, start, 0, KEY_READ, &reg_key);
			if (ret == ERROR_SUCCESS) {
				ret = RegQueryValueExW(reg_key, L"URL Protocol", NULL, NULL, NULL, NULL);
				if (ret == ERROR_SUCCESS) {
					gchar *utf8 = g_utf16_to_utf8(start, -1, NULL, NULL, NULL);
					gchar *protocol = g_strdup_printf("%s:", utf8);
					g_free(utf8);
					registered_url_handlers = g_slist_prepend(registered_url_handlers, protocol);
					/* We still pass everything to the "http" "open" handler for security reasons */
					pidgin_webview_class_register_protocol(protocol, url_clicked_cb, link_context_menu);
				}
				RegCloseKey(reg_key);
			}
			ret = ERROR_SUCCESS;
		}
	} while (ret == ERROR_SUCCESS);

	if (ret != ERROR_NO_MORE_ITEMS)
		purple_debug_error("winpidgin", "Error iterating HKEY_CLASSES_ROOT subkeys: %ld\n",
						   ret);
}
#endif

GtkWidget *
pidgin_make_scrollable(GtkWidget *child, GtkPolicyType hscrollbar_policy, GtkPolicyType vscrollbar_policy, GtkShadowType shadow_type, int width, int height)
{
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);

	if (G_LIKELY(sw)) {
		gtk_widget_show(sw);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), hscrollbar_policy, vscrollbar_policy);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), shadow_type);
		if (width != -1 || height != -1)
			gtk_widget_set_size_request(sw, width, height);
		if (child) {
#if GTK_CHECK_VERSION(3,8,0)
			gtk_container_add(GTK_CONTAINER(sw), child);
#else
			if (GTK_IS_SCROLLABLE(child))
				gtk_container_add(GTK_CONTAINER(sw), child);
			else
				gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), child);
#endif /* GTK_CHECK_VERSION(3,8,0) */
		}
		return sw;
	}

	return child;
}

void pidgin_utils_init(void)
{
	pidgin_webview_class_register_protocol("http://", url_clicked_cb, link_context_menu);
	pidgin_webview_class_register_protocol("https://", url_clicked_cb, link_context_menu);
	pidgin_webview_class_register_protocol("ftp://", url_clicked_cb, link_context_menu);
	pidgin_webview_class_register_protocol("gopher://", url_clicked_cb, link_context_menu);
	pidgin_webview_class_register_protocol("mailto:", url_clicked_cb, copy_email_address);

	pidgin_webview_class_register_protocol("file://", file_clicked_cb, file_context_menu);
	pidgin_webview_class_register_protocol("audio://", audio_clicked_cb, audio_context_menu);

	/* Example custom URL handler. */
	pidgin_webview_class_register_protocol("open://", open_dialog, dummy);

#ifdef _WIN32
	winpidgin_register_win32_url_handlers();
#endif

}

void pidgin_utils_uninit(void)
{
	pidgin_webview_class_register_protocol("open://", NULL, NULL);

	/* If we have GNOME handlers registered, unregister them. */
	if (registered_url_handlers)
	{
		GSList *l;
		for (l = registered_url_handlers; l; l = l->next)
		{
			pidgin_webview_class_register_protocol((char *)l->data, NULL, NULL);
			g_free(l->data);
		}
		g_slist_free(registered_url_handlers);
		registered_url_handlers = NULL;
		return;
	}

	pidgin_webview_class_register_protocol("audio://", NULL, NULL);
	pidgin_webview_class_register_protocol("file://", NULL, NULL);

	pidgin_webview_class_register_protocol("http://", NULL, NULL);
	pidgin_webview_class_register_protocol("https://", NULL, NULL);
	pidgin_webview_class_register_protocol("ftp://", NULL, NULL);
	pidgin_webview_class_register_protocol("mailto:", NULL, NULL);
	pidgin_webview_class_register_protocol("gopher://", NULL, NULL);
}
