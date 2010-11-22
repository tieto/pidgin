/* Pidgin
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
#include "pidgin.h"
#include "debug.h"
#include "version.h"

#include "theme-manager.h"

#include "gtkblist.h"
#include "gtkblist-theme.h"
#include "gtkutils.h"
#include "gtkplugin.h"

#include "pidginstock.h"
#include "themeedit-icon.h"

typedef enum
{
	FLAG_SIZE_MICROSOPIC = 0,
	FLAG_SIZE_EXTRA_SMALL,
	FLAG_SIZE_SMALL,
	FLAG_SIZE_MEDIUM,
	FLAG_SIZE_LARGE,
	FLAG_SIZE_HUGE,
	FLAG_SIZE_NONE,
} SectionFlags;

#define SECTION_FLAGS_ALL (0x3f)

static const char *stocksizes [] = {
	[FLAG_SIZE_MICROSOPIC] = PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC,
	[FLAG_SIZE_EXTRA_SMALL] = PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL,
	[FLAG_SIZE_SMALL] = PIDGIN_ICON_SIZE_TANGO_SMALL,
	[FLAG_SIZE_MEDIUM] = PIDGIN_ICON_SIZE_TANGO_MEDIUM,
	[FLAG_SIZE_LARGE] = PIDGIN_ICON_SIZE_TANGO_LARGE,
	[FLAG_SIZE_HUGE] = PIDGIN_ICON_SIZE_TANGO_HUGE,
	[FLAG_SIZE_NONE] = NULL,
};

static const struct options {
	const char *stockid;
	const char *text;
} statuses[] = {
	{PIDGIN_STOCK_STATUS_AVAILABLE, N_("Available")},
	{PIDGIN_STOCK_STATUS_AWAY, N_("Away")},
	{PIDGIN_STOCK_STATUS_XA, N_("Extended Away")},
	{PIDGIN_STOCK_STATUS_BUSY, N_("Busy")},
	{PIDGIN_STOCK_STATUS_OFFLINE, N_("Offline")},
	{PIDGIN_STOCK_STATUS_LOGIN, N_("Just logged in")},
	{PIDGIN_STOCK_STATUS_LOGOUT, N_("Just logged out")},
	{PIDGIN_STOCK_STATUS_PERSON, N_("Icon for Contact/\nIcon for Unknown person")},
	{PIDGIN_STOCK_STATUS_CHAT, N_("Icon for Chat")},
	{NULL, NULL}
}, chatemblems[] = {
	{PIDGIN_STOCK_STATUS_IGNORED, N_("Ignored")},
	{PIDGIN_STOCK_STATUS_FOUNDER, N_("Founder")},
	/* A user in a chat room who has special privileges. */
	{PIDGIN_STOCK_STATUS_OPERATOR, N_("Operator")},
	/* A half operator is someone who has a subset of the privileges
	   that an operator has. */
	{PIDGIN_STOCK_STATUS_HALFOP, N_("Half Operator")},
	{PIDGIN_STOCK_STATUS_VOICE, N_("Voice")},
	{NULL, NULL}
}, dialogicons[] = {
	{PIDGIN_STOCK_DIALOG_AUTH, N_("Authorization dialog")},
	{PIDGIN_STOCK_DIALOG_ERROR, N_("Error dialog")},
	{PIDGIN_STOCK_DIALOG_INFO, N_("Information dialog")},
	{PIDGIN_STOCK_DIALOG_MAIL, N_("Mail dialog")},
	{PIDGIN_STOCK_DIALOG_QUESTION, N_("Question dialog")},
	{PIDGIN_STOCK_DIALOG_WARNING, N_("Warning dialog")},
	{NULL, NULL},
	{PIDGIN_STOCK_DIALOG_COOL, N_("What kind of dialog is this?")},
};

static const struct {
	const char *heading;
	const struct options *options;
	SectionFlags flags;
} sections[] = {
	{N_("Status Icons"), statuses, SECTION_FLAGS_ALL ^ (1 << FLAG_SIZE_HUGE)},
	{N_("Chatroom Emblems"), chatemblems, FLAG_SIZE_SMALL},
	{N_("Dialog Icons"), dialogicons, (1 << FLAG_SIZE_EXTRA_SMALL) | (1 << FLAG_SIZE_HUGE)},
	{NULL, NULL, 0}
};

static PidginStatusIconTheme *
create_icon_theme(GtkWidget *window)
{
	int s, i, j;
	const char *dirname = g_get_tmp_dir();
	PidginStatusIconTheme *theme;
	const char *author;
#ifndef _WIN32
	author = getlogin();
#else
	author = "user";
#endif
	theme = g_object_new(PIDGIN_TYPE_STATUS_ICON_THEME, "type", "status-icon",
				"author", author,
				"directory", dirname,
				NULL);

	for (s = 0; sections[s].heading; s++) {
		GtkWidget *vbox = g_object_get_data(G_OBJECT(window), sections[s].heading);
		for (i = 0; sections[s].options[i].stockid; i++) {
			GtkWidget *image = g_object_get_data(G_OBJECT(vbox), sections[s].options[i].stockid);
			GdkPixbuf *pixbuf = g_object_get_data(G_OBJECT(image), "pixbuf");
			if (!pixbuf)
				continue;
			pidgin_icon_theme_set_icon(PIDGIN_ICON_THEME(theme), sections[s].options[i].stockid,
					sections[s].options[i].stockid);
			for (j = 0; stocksizes[j]; j++) {
				int width, height;
				GtkIconSize iconsize;
				char size[8];
				char *name;
				GdkPixbuf *scale;
				GError *error = NULL;

				if (!(sections[s].flags & (1 << j)))
					continue;

				iconsize = gtk_icon_size_from_name(stocksizes[j]);
				gtk_icon_size_lookup(iconsize, &width, &height);
				g_snprintf(size, sizeof(size), "%d", width);

				if (i == 0) {
					name = g_build_filename(dirname, size, NULL);
					purple_build_dir(name, S_IRUSR | S_IWUSR | S_IXUSR);
					g_free(name);
				}

				name = g_build_filename(dirname, size, sections[s].options[i].stockid, NULL);
				scale = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
				gdk_pixbuf_save(scale, name, "png", &error, "compression", "9", NULL);
				g_free(name);
				g_object_unref(G_OBJECT(scale));
				if (error)
					g_error_free(error);
			}
		}
	}
	return theme;
}

static void
use_icon_theme(GtkWidget *w, GtkWidget *window)
{
	/* I don't quite understand the icon-theme stuff. For example, I don't
	 * know why PidginIconTheme needs to be abstract, or how PidginStatusIconTheme
	 * would be different from other PidginIconTheme's (e.g. PidginStockIconTheme)
	 * etc., but anyway, this works for now.
	 *
	 * Here's an interesting note: A PidginStatusIconTheme can be used for both
	 * stock and status icons. Like I said, I don't quite know how they could be
	 * different. So I am going to just keep it as it is, for now anyway, until I
	 * have the time to dig through this, or someone explains this stuff to me
	 * clearly.
	 *		-- Sad
	 */
	PidginStatusIconTheme *theme = create_icon_theme(window);
	pidgin_stock_load_status_icon_theme(PIDGIN_STATUS_ICON_THEME(theme));
	pidgin_stock_load_stock_icon_theme((PidginStockIconTheme *)theme);
	pidgin_blist_refresh(purple_get_blist());
	g_object_unref(theme);
}

#ifdef NOT_SADRUL
static void
save_icon_theme(GtkWidget *w, GtkWidget *window)
{
	/* TODO: SAVE! */
	gtk_widget_destroy(window);
}
#endif

static void
close_icon_theme(GtkWidget *w, GtkWidget *window)
{
	gtk_widget_destroy(window);
}

static void
stock_icon_selected(const char *filename, gpointer image)
{
	GError *error = NULL;
	GdkPixbuf *scale;
	int i;
	GdkPixbuf *pixbuf;

	if (!filename)
		return;

	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (error || !pixbuf) {
		purple_debug_error("theme-editor-icon", "Unable to load icon file '%s' (%s)\n",
				filename, error ? error->message : "Reason unknown");
		if (error)
			g_error_free(error);
		return;
	}

	scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
	gtk_image_set_from_pixbuf(GTK_IMAGE(image), scale);
	g_object_unref(G_OBJECT(scale));

	/* Update the size previews */
	for (i = 0; stocksizes[i]; i++) {
		int width, height;
		GtkIconSize iconsize;
		GtkWidget *prev = g_object_get_data(G_OBJECT(image), stocksizes[i]);
		if (!prev)
			continue;
		iconsize = gtk_icon_size_from_name(stocksizes[i]);
		gtk_icon_size_lookup(iconsize, &width, &height);
		scale = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(GTK_IMAGE(prev), scale);
		g_object_unref(G_OBJECT(scale));
	}

	/* Save the original pixbuf so we can use it for resizing later */
	g_object_set_data_full(G_OBJECT(image), "pixbuf", pixbuf,
			(GDestroyNotify)g_object_unref);
}

static gboolean
change_stock_image(GtkWidget *widget, GdkEventButton *event, GtkWidget *image)
{
	GtkWidget *win = pidgin_buddy_icon_chooser_new(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			stock_icon_selected, image);
	gtk_window_set_title(GTK_WINDOW(win),
	                     g_object_get_data(G_OBJECT(image), "localized-name"));
	gtk_widget_show_all(win);

	return TRUE;
}

void pidgin_icon_theme_edit(PurplePluginAction *unused)
{
	GtkWidget *dialog;
	GtkWidget *box, *vbox;
	GtkWidget *notebook;
	GtkSizeGroup *sizegroup;
	int s, i, j;
	dialog = pidgin_create_dialog(_("Pidgin Icon Theme Editor"), 0, "theme-editor-icon", FALSE);
	box = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(dialog), FALSE, PIDGIN_HIG_BOX_SPACE);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(box), notebook, TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);
	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (s = 0; sections[s].heading; s++) {
		const char *heading = sections[s].heading;

		box = gtk_vbox_new(FALSE, 0);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box, gtk_label_new(heading));

		vbox = pidgin_make_frame(box, heading);
		g_object_set_data(G_OBJECT(dialog), heading, vbox);

		for (i = 0; sections[s].options[i].stockid; i++) {
			const char *id = sections[s].options[i].stockid;
			const char *text = _(sections[s].options[i].text);

			GtkWidget *hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
			GtkWidget *label = gtk_label_new(text);
			GtkWidget *image = gtk_image_new_from_stock(id,
					gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
			GtkWidget *ebox = gtk_event_box_new();
			gtk_container_add(GTK_CONTAINER(ebox), image);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

			g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(change_stock_image), image);
			g_object_set_data(G_OBJECT(image), "property-name", (gpointer)id);
			g_object_set_data(G_OBJECT(image), "localized-name", (gpointer)text);

			gtk_size_group_add_widget(sizegroup, label);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox), ebox, FALSE, FALSE, 0);

			for (j = 0; stocksizes[j]; j++) {
				GtkWidget *sh;

				if (!(sections[s].flags & (1 << j)))
					continue;

				sh = gtk_image_new_from_stock(id, gtk_icon_size_from_name(stocksizes[j]));
				gtk_box_pack_start(GTK_BOX(hbox), sh, FALSE, FALSE, 0);
				g_object_set_data(G_OBJECT(image), stocksizes[j], sh);
			}

			gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

			g_object_set_data(G_OBJECT(vbox), id, image);
		}
	}

#ifdef NOT_SADRUL
	pidgin_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, G_CALLBACK(save_icon_theme), dialog);
#endif
	pidgin_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_APPLY, G_CALLBACK(use_icon_theme), dialog);
	pidgin_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, G_CALLBACK(close_icon_theme), dialog);
	gtk_widget_show_all(dialog);
	g_object_unref(sizegroup);
}

