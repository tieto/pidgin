/*
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#else
#ifdef small
#undef small
#endif
#endif /*_WIN32*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif
#include "gtkimhtml.h"
#include <gdk/gdkkeysyms.h>
#include "prpl.h"
#include "ui.h"

#ifdef _WIN32
#include "wspell.h"
#endif

void
gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	if (!(convo_options & OPT_CONVO_SHOW_SMILEY))
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
gaim_gtk_remove_tags(struct gaim_gtk_conversation *gtkconv, const char *tag)
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
gaim_gtk_surround(struct gaim_gtk_conversation *gtkconv,
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
gaim_gtk_advance_past(struct gaim_gtk_conversation *gtkconv,
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
gaim_gtk_set_font_face(struct gaim_gtk_conversation *gtkconv,
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
			  struct gaim_gtk_conversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;

	return TRUE;
}

static void
do_save_icon(GtkObject *obj, struct gaim_conversation *c)
{
	struct gaim_gtk_conversation *gtkconv;
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
		do_error_dialog("Can't save icon file to disk",
						strerror(errno), GAIM_ERROR);
	}

	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}

static void
cancel_save_icon(GtkObject *obj, struct gaim_gtk_conversation *gtkconv)
{
	gtk_widget_destroy(gtkconv->u.im->save_icon);
	gtkconv->u.im->save_icon = NULL;
}


void
gaim_gtk_save_icon_dialog(GtkObject *obj, struct gaim_conversation *conv)
{
	struct gaim_gtk_conversation *gtkconv;
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

	g_signal_connect(GTK_OBJECT(gtkconv->u.im->save_icon), "delete_event",
					 G_CALLBACK(des_save_icon), gtkconv);
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->ok_button), "clicked",
					 G_CALLBACK(do_save_icon), conv);
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(gtkconv->u.im->save_icon)->cancel_button), "clicked",
					 G_CALLBACK(cancel_save_icon), gtkconv);

	gtk_widget_show(gtkconv->u.im->save_icon);
}

int
gaim_gtk_get_dispstyle(GaimConversationType type)
{
	int dispstyle = 2;

	if (type == GAIM_CONV_CHAT) {
		switch (chat_options & (OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM)) {

			case OPT_CHAT_BUTTON_TEXT: dispstyle = 1; break;
			case OPT_CHAT_BUTTON_XPM:  dispstyle = 0; break;
			default:                   dispstyle = 2; break; /* both/neither */
		}
	}
	else if (type == GAIM_CONV_IM) {
		switch (im_options & (OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM)) {

			case OPT_IM_BUTTON_TEXT: dispstyle = 1; break;
			case OPT_IM_BUTTON_XPM:  dispstyle = 0; break;
			default:                 dispstyle = 2; break; /* both/neither */
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

void gaim_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
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
		g_signal_connect(GTK_OBJECT(menuitem), "activate", sf, data);

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
