/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
 *
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
 */
#include "internal.h"
#include "gtkgaim.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#else
# ifdef small
#  undef small
# endif
#endif /*_WIN32*/

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
# ifdef _WIN32
#  include "wspell.h"
# endif
#endif

#include <gdk/gdkkeysyms.h>

#include "debug.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "signals.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkutils.h"

guint accels_save_timer = 0;

static gboolean
url_clicked_idle_cb(gpointer data)
{
	gaim_notify_uri(NULL, data);
	g_free(data);
	return FALSE;
}

static void
url_clicked_cb(GtkWidget *w, const char *uri)
{
	g_idle_add(url_clicked_idle_cb, g_strdup(uri));
}

GtkIMHtmlFuncs gtkimhtml_cbs = {
	(GtkIMHtmlGetImageFunc)gaim_imgstore_get,
	(GtkIMHtmlGetImageDataFunc)gaim_imgstore_get_data,
	(GtkIMHtmlGetImageSizeFunc)gaim_imgstore_get_size,
	(GtkIMHtmlGetImageFilenameFunc)gaim_imgstore_get_filename,
	gaim_imgstore_ref,
	gaim_imgstore_unref,
};

void
gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	g_signal_connect(G_OBJECT(imhtml), "url_clicked",
					 G_CALLBACK(url_clicked_cb), NULL);

	smiley_themeize(imhtml);

	gtk_imhtml_set_funcs(GTK_IMHTML(imhtml), &gtkimhtml_cbs);
}

void
toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
gaim_gtk_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
									  (*text != '\0'));
}

void
gaim_gtk_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity;

	if (to_toggle == NULL)
		return;

	sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
gtk_toggle_sensitive_array(GtkWidget *w, GPtrArray *data)
{
	gboolean sensitivity;
	gpointer element;
	int i;

	for (i=0; i < data->len; i++) {
		element = g_ptr_array_index(data,i);
		if (element == NULL)
			continue;

		sensitivity = GTK_WIDGET_IS_SENSITIVE(element);

		gtk_widget_set_sensitive(element, !sensitivity);	
	}
}

void
gaim_gtk_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle)
{
	if (to_toggle == NULL)
		return;

	if (GTK_WIDGET_VISIBLE(to_toggle))
		gtk_widget_hide(to_toggle);
	else
		gtk_widget_show(to_toggle);
}

void gaim_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

GtkWidget *gaim_new_item(GtkWidget *menu, const char *str)
{
	GtkWidget *menuitem;
	GtkWidget *label;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	label = gtk_label_new(str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);
/* FIXME: Go back and fix this 
	gtk_widget_add_accelerator(menuitem, "activate", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
*/
	gaim_set_accessible_label (menuitem, label);
	return menuitem;
}

GtkWidget *gaim_new_check_item(GtkWidget *menu, const char *str,
		GtkSignalFunc sf, gpointer data, gboolean checked)
{
	GtkWidget *menuitem;
	menuitem = gtk_check_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), checked);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_pixbuf_toolbar_button_from_stock(const char *icon)
{
	GtkWidget *button, *image, *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	return button;
}

GtkWidget *
gaim_pixbuf_button_from_stock(const char *text, const char *icon,
							  GaimButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox;

	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL) {
		bbox = gtk_hbox_new(FALSE, 0);
		ibox = gtk_hbox_new(FALSE, 0);
		lbox = gtk_hbox_new(FALSE, 0);
	} else {
		bbox = gtk_vbox_new(FALSE, 0);
		ibox = gtk_vbox_new(FALSE, 0);
		lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add(GTK_CONTAINER(button), bbox);

	if (icon) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, TRUE, 0);
	}

	if (text) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, TRUE, 0);
		gaim_set_accessible_label (button, label);
	}

	gtk_widget_show_all(bbox);

	return button;
}


GtkWidget *gaim_new_item_from_stock(GtkWidget *menu, const char *str, const char *icon, GtkSignalFunc sf, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	/*
	GtkWidget *hbox;
	GtkWidget *label;
	*/
	GtkWidget *image;

	if (icon == NULL)
		menuitem = gtk_menu_item_new_with_mnemonic(str);
	else
		menuitem = gtk_image_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	if (icon != NULL) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	}
/* FIXME: this isn't right
	if (mod) {
		label = gtk_label_new(mod);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}
*/
/*
	if (accel_key) {
		gtk_widget_add_accelerator(menuitem, "activate", accel, accel_key,
					   accel_mods, GTK_ACCEL_LOCKED);
	}
*/

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_gtk_make_frame(GtkWidget *parent, const char *title)
{
	GtkWidget *vbox, *label, *hbox;
	char labeltitle[256];

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(NULL);
	g_snprintf(labeltitle, sizeof(labeltitle),
			   "<span weight=\"bold\">%s</span>", title);

	gtk_label_set_markup(GTK_LABEL(label), labeltitle);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gaim_set_accessible_label (vbox, label);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	return vbox;
}

static void
protocol_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	const char *protocol;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	protocol = g_object_get_data(G_OBJECT(item), "protocol");
	user_data = (g_object_get_data(G_OBJECT(optmenu), "user_data"));

	if (cb != NULL)
		((void (*)(GtkWidget *, const char *, gpointer))cb)(item, protocol,
															user_data);
}

GtkWidget *
gaim_gtk_protocol_option_menu_new(const char *id, GCallback cb,
								  gpointer user_data)
{
	GaimPluginProtocolInfo *prpl_info;
	GaimPlugin *plugin;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = gaim_plugins_get_protocols(), i = 0;
		 p != NULL;
		 p = p->next, i++) {

		plugin = (GaimPlugin *)p->data;
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		proto_name = prpl_info->list_icon(NULL, NULL);
		g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
									"default", buf, NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);

		if (pixbuf != NULL) {
			/* Scale and insert the image */
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
											GDK_INTERP_BILINEAR);
			image = gtk_image_new_from_pixbuf(scale);

			g_object_unref(G_OBJECT(pixbuf));
			g_object_unref(G_OBJECT(scale));
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		/* Create the label. */
		label = gtk_label_new(plugin->info->name);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "protocol", plugin->info->id);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (id != NULL && !strcmp(plugin->info->id, id))
			selected_index = i;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);

	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(protocol_menu_cb), cb);

	g_object_unref(sg);

	return optmenu;
}

GaimAccount *
gaim_gtk_account_option_menu_get_selected(GtkWidget *optmenu)
{
	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	GtkWidget *item = gtk_menu_get_active(GTK_MENU(menu));
	return g_object_get_data(G_OBJECT(item), "account");
}

static void
account_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	GaimAccount *account;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	account   = g_object_get_data(G_OBJECT(item),    "account");
	user_data = g_object_get_data(G_OBJECT(optmenu), "user_data");

	if (cb != NULL)
		((void (*)(GtkWidget *, GaimAccount *, gpointer))cb)(item, account,
															 user_data);
}

static void
create_account_menu(GtkWidget *optmenu, GaimAccount *default_account,
					GaimFilterAccountFunc filter_func, gboolean show_all)
{
	GaimAccount *account;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GtkWidget *hbox;
	GtkWidget *label;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *list;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	if (show_all)
		list = gaim_accounts_get_all();
	else
		list = gaim_connections_get_all();

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		if (show_all)
			account = (GaimAccount *)p->data;
		else {
			GaimConnection *gc = (GaimConnection *)p->data;

			account = gaim_connection_get_account(gc);
		}

		if (filter_func && !filter_func(account)) {
			i--;
			continue;
		}

		plugin = gaim_find_prpl(gaim_account_get_protocol_id(account));

		if (plugin != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		if (prpl_info != NULL) {
			proto_name = prpl_info->list_icon(account, NULL);
			g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
			                            "default", buf, NULL);
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (pixbuf != NULL) {
				/* Scale and insert the image */
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
				                                GDK_INTERP_BILINEAR);

				if (!gaim_account_is_connected(account) && show_all &&
						gaim_connections_get_all())
					gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);

				image = gtk_image_new_from_pixbuf(scale);

				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
			}
			else
				image = gtk_image_new();
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		g_snprintf(buf, sizeof(buf), "%s (%s)",
				   gaim_account_get_username(account),
				   gaim_account_get_protocol_name(account));

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "account", account);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (default_account != NULL && account == default_account)
			selected_index = i;
	}

	g_object_unref(sg);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	/* Set the place we should be at. */
	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean show_all;
	GaimAccount *account;
	GaimFilterAccountFunc filter_func;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));
	account = g_object_get_data(G_OBJECT(item), "account");

	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu),
												 "show_all"));

	filter_func = g_object_get_data(G_OBJECT(optmenu),
										   "filter_func");

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	create_account_menu(optmenu, account, filter_func, show_all);
}

static void
account_menu_sign_on_off_cb(GaimConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(GaimAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event,
						  void *user_data)
{
	gaim_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

void
gaim_gtk_account_option_menu_set_selected(GtkWidget *optmenu, GaimAccount *account)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean show_all;
	GaimAccount *curaccount;
	GaimFilterAccountFunc filter_func;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));
	curaccount = g_object_get_data(G_OBJECT(item), "account");

	if (account == curaccount)
		return;

	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu),
												 "show_all"));

	filter_func = g_object_get_data(G_OBJECT(optmenu),
										   "filter_func");

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	create_account_menu(optmenu, account, filter_func, show_all);
}

GtkWidget *
gaim_gtk_account_option_menu_new(GaimAccount *default_account,
								 gboolean show_all, GCallback cb,
								 GaimFilterAccountFunc filter_func,
								 gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
					 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the gaim sign on/off event callbacks. */
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-added",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all", GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "filter_func",
					  filter_func);

	/* Create and set the actual menu. */
	create_account_menu(optmenu, default_account, filter_func, show_all);

	/* And now the last callback. */
	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(account_menu_cb), cb);

	return optmenu;
}

gboolean
gaim_gtk_check_if_dir(const char *path, GtkFileSelection *filesel)
{
	char *dirname;

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != G_DIR_SEPARATOR) {
			dirname = g_strconcat(path, G_DIR_SEPARATOR_S, NULL);
		} else {
			dirname = g_strdup(path);
		}
		gtk_file_selection_set_filename(filesel, dirname);
		g_free(dirname);
		return TRUE;
	}

	return FALSE;
}

char *stylize(const gchar *text, int length)
{
	gchar *buf;
	char *tmp = g_malloc(length);

	buf = g_malloc(length);
	g_snprintf(buf, length, "%s", text);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_formatting")) {
		const char *font;
		GdkColor fg_color, bg_color;

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_bold")) {
			g_snprintf(tmp, length, "<B>%s</B>", buf);
			strcpy(buf, tmp);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_italic")) {
			g_snprintf(tmp, length, "<I>%s</I>", buf);
			strcpy(buf, tmp);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_underline")) {
			g_snprintf(tmp, length, "<U>%s</U>", buf);
			strcpy(buf, tmp);
		}

		font = gaim_prefs_get_string("/gaim/gtk/conversations/font_face");

		if (strcmp(font, "") != 0) {
			g_snprintf(tmp, length, "<FONT FACE=\"%s\">%s</FONT>", font, buf);
			strcpy(buf, tmp);
		}

		g_snprintf(tmp, length, "<FONT SIZE=\"%d\">%s</FONT>",
			gaim_prefs_get_int("/gaim/gtk/conversations/font_size"), buf);
		strcpy(buf, tmp);

		if(strcmp(gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"), "") != 0)
		{
			gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"),
							&fg_color);

			g_snprintf(tmp, length, "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>",
					   fg_color.red/256, fg_color.green/256,
					   fg_color.blue/256, buf);
			strcpy(buf, tmp);
		}

		if(strcmp(gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"), "") != 0)
		{
			gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"),
							&bg_color);

			g_snprintf(tmp, length, "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>",
					   bg_color.red/256, bg_color.green/256,
					   bg_color.blue/256, buf);
			strcpy(buf, tmp);
		}

	}

	g_free(tmp);
	return buf;
}

void
gaim_gtk_setup_gtkspell(GtkTextView *textview)
{
#ifdef USE_GTKSPELL
	GError *error = NULL;
	char *locale = NULL;

	g_return_if_fail(textview != NULL);
	g_return_if_fail(GTK_IS_TEXT_VIEW(textview));

	if (gtkspell_new_attach(textview, locale, &error) == NULL && error)
	{
		gaim_debug_warning("gtkspell", "Failed to setup GtkSpell: %s\n",
						   error->message);
		g_error_free(error);
	}
#endif /* USE_GTKSPELL */
}

void
gaim_gtk_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
														 GdkModifierType arg2, GClosure *arg3,
														 gpointer data)
{
	gaim_debug(GAIM_DEBUG_MISC, "accels", "accel changed, scheduling save.\n");

	if (!accels_save_timer)
		accels_save_timer = g_timeout_add(5000, gaim_gtk_save_accels, NULL);
}

gboolean
gaim_gtk_save_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gaim_debug(GAIM_DEBUG_MISC, "accels", "saving accels to %s\n", filename);
	gtk_accel_map_save(filename);
	g_free(filename);

	accels_save_timer = 0;
	return FALSE;
}

void
gaim_gtk_load_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gtk_accel_map_load(filename);
	g_free(filename);
}

gboolean
gaim_gtk_parse_x_im_contact(const char *msg, gboolean all_accounts,
							GaimAccount **ret_account, char **ret_protocol,
							char **ret_username, char **ret_alias)
{
	char *protocol = NULL;
	char *username = NULL;
	char *alias    = NULL;
	char *str;
	char *c, *s;
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

		if ((c = strchr(key, ':')) != NULL)
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
			GaimAccount *account = NULL;
			GList *l;
			const char *protoname;

			if (all_accounts)
				list = gaim_accounts_get_all();
			else
				list = gaim_connections_get_all();

			for (l = list; l != NULL; l = l->next)
			{
				GaimConnection *gc;
				GaimPluginProtocolInfo *prpl_info = NULL;
				GaimPlugin *plugin;

				if (all_accounts)
				{
					account = (GaimAccount *)l->data;

					plugin = gaim_plugins_find_with_id(
						gaim_account_get_protocol_id(account));

					if (plugin == NULL)
					{
						account = NULL;

						continue;
					}

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
				}
				else
				{
					gc = (GaimConnection *)l->data;
					account = gaim_connection_get_account(gc);

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
				}

				protoname = prpl_info->list_icon(account, NULL);

				if (!strcmp(protoname, protocol))
					break;

				account = NULL;
			}

			/* Special case for AIM and ICQ */
			if (account == NULL && (!strcmp(protocol, "aim") ||
									!strcmp(protocol, "icq")))
			{
				for (l = list; l != NULL; l = l->next)
				{
					GaimConnection *gc;
					GaimPluginProtocolInfo *prpl_info = NULL;
					GaimPlugin *plugin;

					if (all_accounts)
					{
						account = (GaimAccount *)l->data;

						plugin = gaim_plugins_find_with_id(
							gaim_account_get_protocol_id(account));

						if (plugin == NULL)
						{
							account = NULL;

							continue;
						}

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
					}
					else
					{
						gc = (GaimConnection *)l->data;
						account = gaim_connection_get_account(gc);

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
					}

					protoname = prpl_info->list_icon(account, NULL);

					if (!strcmp(protoname, "aim") || !strcmp(protoname, "icq"))
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

		if (username != NULL) g_free(username);
		if (protocol != NULL) g_free(protocol);
		if (alias    != NULL) g_free(alias);
	}

	g_free(str);

	return valid;
}

void
gaim_set_accessible_label (GtkWidget *w, GtkWidget *l)
{
	AtkObject *acc, *label;
	AtkObject *rel_obj[1];
	AtkRelationSet *set;
	AtkRelation *relation;
	const gchar *label_text;
	const gchar *existing_name;

	acc = gtk_widget_get_accessible (w);
	label = gtk_widget_get_accessible (l);

	/* If this object has no name, set it's name with the label text */
	existing_name = atk_object_get_name (acc);
	if (!existing_name) {
		label_text = gtk_label_get_text (GTK_LABEL(l));
		if (label_text)
			atk_object_set_name (acc, label_text);
	}

	/* Create the labeled-by relation */
	set = atk_object_ref_relation_set (acc);
	rel_obj[0] = label;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);

	/* Create the label-for relation */
	set = atk_object_ref_relation_set (label);
	rel_obj[0] = acc;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
}

#if GTK_CHECK_VERSION(2,2,0)
static void
gaim_gtk_menu_position_func(GtkMenu *menu,
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
	gint needed_width;
	gint needed_height;
	gint xthickness;
	gint ythickness;
	gboolean rtl;

	g_return_if_fail(GTK_IS_MENU(menu));

	widget     = GTK_WIDGET(menu);
	screen     = gtk_widget_get_screen(widget);
	xthickness = widget->style->xthickness;
	ythickness = widget->style->ythickness;
	rtl        = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);

	/*
	 * We need the requisition to figure out the right place to
	 * popup the menu. In fact, we always need to ask here, since
	 * if a size_request was queued while we weren't popped up,
	 * the requisition won't have been recomputed yet.
	 */
	gtk_widget_size_request (widget, &requisition);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);

	push_in = FALSE;
  
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

	/* the amount of space we need to position the menu. Note the
	 * menu is offset "xthickness" pixels 
	 */
	needed_width = requisition.width - xthickness;

	if (needed_width <= space_left ||
	    needed_width <= space_right)
	{
		if ((rtl  && needed_width <= space_left) ||
		    (!rtl && needed_width >  space_right))
		{
			/* position left */
			*x = *x + xthickness - requisition.width + 1;
		}
		else
		{
			/* position right */
			*x = *x - xthickness;
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
	needed_height = requisition.height - ythickness;

	if (needed_height <= space_above ||
	    needed_height <= space_below)
	{
		if (needed_height <= space_below)
			*y = *y - ythickness;
		else
			*y = *y + ythickness - requisition.height + 1;
 
		*y = CLAMP (*y, monitor.y,
			   monitor.y + monitor.height - requisition.height);
	}
	else if (needed_height > space_below && needed_height > space_above)
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

#endif

void
gaim_gtk_treeview_popup_menu_position_func(GtkMenu *menu,
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
	gint ythickness = GTK_WIDGET(menu)->style->ythickness;

	gdk_window_get_origin (widget->window, x, y);
	gtk_tree_view_get_cursor (tv, &path, &col);
	gtk_tree_view_get_cell_area (tv, path, col, &rect);

	*x += rect.x+rect.width;
	*y += rect.y+rect.height+ythickness;
#if GTK_CHECK_VERSION(2,2,0)
	gaim_gtk_menu_position_func (menu, x, y, push_in, data);
#endif
}

gboolean
gaim_running_gnome(void)
{
	if ((g_getenv("GNOME_DESKTOP_SESSION_ID") != NULL) &&
		(g_find_program_in_path("gnome-open") != NULL))
	{
		return TRUE;
	}

	return FALSE;
}
