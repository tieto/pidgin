/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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

#ifndef _WIN32
# include <X11/Xlib.h>
#else
# ifdef small
#  undef small
# endif
#endif /*_WIN32*/

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "ui.h"

#ifdef _WIN32
#include "wspell.h"
#endif

void
gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/show_smileys"))
		gtk_imhtml_show_smileys(GTK_IMHTML(imhtml), FALSE);

	g_signal_connect(G_OBJECT(imhtml), "url_clicked",
					 G_CALLBACK(open_url), NULL);

	smiley_themeize(imhtml);
}

void
toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

static void
gaim_gtk_remove_tags(GaimGtkConversation *gtkconv, const char *tag)
{
	GtkTextIter start, end, m_start, m_end;

	if (gtkconv == NULL || tag == NULL)
		return;

	if (!gtk_text_buffer_get_selection_bounds(gtkconv->entry_buffer,
											  &start, &end))
		return;

	/* FIXMEif (strstr(tag, "<FONT SIZE=")) {
		while ((t = strstr(t, "<FONT SIZE="))) {
			if (((t - s) < finish) && ((t - s) >= start)) {
				gtk_editable_delete_text(GTK_EDITABLE(entry), (t - s),
							 (t - s) + strlen(tag));
				g_free(s);
				s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
				t = s;
			} else
				t++;
		}
	} else*/ {
		while (gtk_text_iter_forward_search(&start, tag, 0, &m_start,
											&m_end, &end)) {

			gtk_text_buffer_delete(gtkconv->entry_buffer, &m_start, &m_end);
			gtk_text_buffer_get_selection_bounds(gtkconv->entry_buffer,
												 &start, &end);
		}
	}
}

void
gaim_gtk_surround(GaimGtkConversation *gtkconv,
				  const char *pre, const char *post)
{
	GtkTextIter start, end;
	GtkTextMark *mark_start, *mark_end;
	GtkTextBuffer *entry_buffer;

	if (gtkconv == NULL || pre == NULL || post == NULL)
		return;

	entry_buffer = gtkconv->entry_buffer;

	if (gtk_text_buffer_get_selection_bounds(entry_buffer,
											 &start, &end)) {
		gaim_gtk_remove_tags(gtkconv, pre);
		gaim_gtk_remove_tags(gtkconv, post);

		mark_start = gtk_text_buffer_create_mark(entry_buffer, "m1",
												 &start, TRUE);
		mark_end = gtk_text_buffer_create_mark(entry_buffer, "m2",
											   &end, FALSE);
		gtk_text_buffer_insert(entry_buffer, &start, pre, -1);
		gtk_text_buffer_get_selection_bounds(entry_buffer, &start, &end);
		gtk_text_buffer_insert(entry_buffer, &end, post, -1);
		gtk_text_buffer_get_iter_at_mark(entry_buffer, &start, mark_start);
		gtk_text_buffer_move_mark_by_name(entry_buffer, "selection_bound",
										  &start);
	} else {
		gtk_text_buffer_insert(entry_buffer, &start, pre, -1);
		gtk_text_buffer_insert(entry_buffer, &start, post, -1);
		mark_start = gtk_text_buffer_get_insert(entry_buffer);
		gtk_text_buffer_get_iter_at_mark(entry_buffer, &start, mark_start);
		gtk_text_iter_backward_chars(&start, strlen(post));
		gtk_text_buffer_place_cursor(entry_buffer, &start);
	}

	gtk_widget_grab_focus(gtkconv->entry);
}

static gboolean
invert_tags(GtkTextBuffer *buffer, const char *s1, const char *s2,
			gboolean really)
{
	GtkTextIter start1, start2, end1, end2;
	char *b1, *b2;

	if (gtk_text_buffer_get_selection_bounds(buffer, &start1, &end2)) {
		start2 = start1;
		end1 = end2;

		if (!gtk_text_iter_forward_chars(&start2, strlen(s1)))
			return FALSE;

		if (!gtk_text_iter_backward_chars(&end1, strlen(s2)))
			return FALSE;

		b1 = gtk_text_buffer_get_text(buffer, &start1, &start2, FALSE);
		b2 = gtk_text_buffer_get_text(buffer, &end1, &end2, FALSE);

		if (!g_ascii_strncasecmp(b1, s1, strlen(s1)) &&
		    !g_ascii_strncasecmp(b2, s2, strlen(s2))) {

			if (really) {
				GtkTextMark *m_end1, *m_end2;
 
				m_end1= gtk_text_buffer_create_mark(buffer, "m1", &end1, TRUE);
				m_end2= gtk_text_buffer_create_mark(buffer, "m2", &end2, TRUE);

				gtk_text_buffer_delete(buffer, &start1, &start2);
				gtk_text_buffer_get_iter_at_mark(buffer, &end1, m_end1);
				gtk_text_buffer_get_iter_at_mark(buffer, &end2, m_end2);
				gtk_text_buffer_delete(buffer, &end1, &end2);
				gtk_text_buffer_delete_mark(buffer, m_end1);
				gtk_text_buffer_delete_mark(buffer, m_end2);
			}

			g_free(b1);
			g_free(b2);

			return TRUE;
		}

		g_free(b1);
		g_free(b2);
	}

	return FALSE;
}

void
gaim_gtk_advance_past(GaimGtkConversation *gtkconv,
					  const char *pre, const char *post)
{
	GtkTextIter current_pos, start, end;

	if (invert_tags(gtkconv->entry_buffer, pre, post, TRUE))
		return;

	gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &current_pos,
			gtk_text_buffer_get_insert(gtkconv->entry_buffer));

	if (gtk_text_iter_forward_search(&current_pos, post, 0,
									 &start, &end, NULL))
		gtk_text_buffer_place_cursor(gtkconv->entry_buffer, &end);
	else
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, post, -1);

	gtk_widget_grab_focus(gtkconv->entry);
}

void
gaim_gtk_set_font_face(GaimGtkConversation *gtkconv,
					   const char *font)
{
	char *pre_fontface;

	if (gtkconv == NULL || font == NULL)
		return;

	strncpy(gtkconv->fontface,
			(font && *font ? font : DEFAULT_FONT_FACE),
			sizeof(gtkconv->fontface));

	gtkconv->has_font = TRUE;

	pre_fontface = g_strconcat("<FONT FACE=\"",
							   gtkconv->fontface, "\">", NULL);
	gaim_gtk_surround(gtkconv, pre_fontface, "</FONT>");

	gtk_widget_grab_focus(gtkconv->entry);

	g_free(pre_fontface);
}

static int
des_save_icon(GtkObject *obj, GdkEvent *e,
			  GaimGtkConversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;

	return TRUE;
}

static void
do_save_icon(GtkObject *obj, GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	FILE *file;
	const char *f;
	
	gtkconv = GAIM_GTK_CONVERSATION(c);

	f = gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon));

	if (file_is_dir(f, gtkconv->u.im->save_icon))
		return;

	if ((file = fopen(f, "w")) != NULL) {
		int len;
		void *data = get_icon_data(gaim_conversation_get_gc(c),
								   normalize(gaim_conversation_get_name(c)),
								   &len);

		if (data)
			fwrite(data, 1, len, file);

		fclose(file);
	} else {
		gaim_notify_error(NULL, NULL,
						  _("Can't save icon file to disk."), NULL);
	}

	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}

static void
cancel_save_icon(GtkObject *obj, GaimGtkConversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}


void
gaim_gtk_save_icon_dialog(GtkObject *obj, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	char buf[BUF_LEN];

	if (conv == NULL || gaim_conversation_get_type(conv) != GAIM_CONV_IM)
		return;

	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtkconv->u.im->save_icon != NULL)
	{
		gdk_window_raise(gtkconv->u.im->save_icon->window);
		return;
	}

	gtkconv->u.im->save_icon = gtk_file_selection_new(_("Gaim - Save Icon"));

	gtk_file_selection_hide_fileop_buttons(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon));

	g_snprintf(buf, BUF_LEN - 1,
			   "%s" G_DIR_SEPARATOR_S "%s.icon",
			   gaim_home_dir(), gaim_conversation_get_name(conv));

	gtk_file_selection_set_filename(
		GTK_FILE_SELECTION(gtkconv->u.im->save_icon), buf);

	g_signal_connect(G_OBJECT(gtkconv->u.im->save_icon), "delete_event",
					 G_CALLBACK(des_save_icon), gtkconv);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->ok_button), "clicked",
					 G_CALLBACK(do_save_icon), conv);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->cancel_button), "clicked",
					 G_CALLBACK(cancel_save_icon), gtkconv);

	gtk_widget_show(gtkconv->u.im->save_icon);
}

int
gaim_gtk_get_dispstyle(GaimConversationType type)
{
	int dispstyle = 2;
	int value;

	if (type == GAIM_CONV_CHAT) {
		value = gaim_prefs_get_int("/gaim/gtk/conversations/chat/button_type");

		switch (value) {
			case GAIM_BUTTON_TEXT:  dispstyle = 1; break;
			case GAIM_BUTTON_IMAGE: dispstyle = 0; break;
			default:                dispstyle = 2; break; /* both/neither */
		}
	}
	else if (type == GAIM_CONV_IM) {
		value = gaim_prefs_get_int("/gaim/gtk/conversations/im/button_type");

		switch (value) {
			case GAIM_BUTTON_TEXT:  dispstyle = 1; break;
			case GAIM_BUTTON_IMAGE: dispstyle = 0; break;
			default:                dispstyle = 2; break; /* both/neither */
		}
	}

	return dispstyle;
}

GtkWidget *
gaim_gtk_change_text(const char *text, GtkWidget *button,
					 const char *stock, GaimConversationType type)
{
	int dispstyle = gaim_gtk_get_dispstyle(type);

	if (button != NULL)
		gtk_widget_destroy(button);

	button = gaim_pixbuf_button_from_stock((dispstyle == 0 ? NULL : text),
										   (dispstyle == 1 ? NULL : stock),
										   GAIM_BUTTON_VERTICAL);

	gtk_widget_show(button);

	return button;
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
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);
/* FIXME: Go back and fix this 
	gtk_widget_add_accelerator(menuitem, "activate", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
*/
	return menuitem;
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
	GaimProtocol protocol;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	protocol = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "protocol"));
	user_data = (g_object_get_data(G_OBJECT(optmenu), "user_data"));

	if (cb != NULL)
		((void (*)(GtkWidget *, GaimProtocol, gpointer))cb)(item, protocol,
															user_data);
}

GtkWidget *
gaim_gtk_protocol_option_menu_new(GaimProtocol protocol, GCallback cb,
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

		g_object_set_data(G_OBJECT(item), "protocol",
						 GINT_TO_POINTER(prpl_info->protocol));

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		if (prpl_info->protocol == protocol)
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

GtkWidget *
gaim_gtk_account_option_menu_new(GaimAccount *default_account,
								 gboolean show_all, GCallback cb,
								 gpointer user_data)
{
	GaimAccount *account;
	GList *list;
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

	if (show_all)
		list = gaim_accounts_get_all();
	else
		list = gaim_connections_get_all();

	gaim_debug(GAIM_DEBUG_INFO, "gtkutils", "Populating menu\n");

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		if (show_all)
			account = (GaimAccount *)p->data;
		else {
			GaimConnection *gc = (GaimConnection *)p->data;

			account = gaim_connection_get_account(gc);
		}

		gaim_debug(GAIM_DEBUG_INFO, "gtkutils", "Adding item.\n");

		plugin = gaim_find_prpl(gaim_account_get_protocol(account));

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
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		g_snprintf(buf, sizeof(buf), "%s (%s)",
				   gaim_account_get_username(account), plugin->info->name);

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "account", account);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		if (default_account != NULL && account == default_account)
			selected_index = i;
	}

	gaim_debug(GAIM_DEBUG_INFO, "gtkutils", "Done populating menu\n");

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);

	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(account_menu_cb), cb);

	g_object_unref(sg);

	return optmenu;
}
