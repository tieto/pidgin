/**
 * @file gtknotify.c GTK+ Notification API
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
#include "gtkinternal.h"

#include <gdk/gdkkeysyms.h>

#include "connection.h"
#include "debug.h"
#include "prefs.h"
#include "stock.h"
#include "util.h"

#include "gtkimhtml.h"
#include "gtknotify.h"
#include "gtkutils.h"

#include "ui.h"

typedef struct
{
	GaimConnection *gc;
	char *url;
	GtkWidget *dialog;
	GtkWidget *label;

} GaimNotifyMailData;

static void *gaim_gtk_notify_emails(size_t count, gboolean detailed,
									const char **subjects,
									const char **froms, const char **tos,
									const char **urls, GCallback cb,
									void *user_data);

static void
message_response_cb(GtkDialog *dialog, gint id, GtkWidget *widget)
{
	gaim_notify_close(GAIM_NOTIFY_MESSAGE, widget);
}

static void
email_response_cb(GtkDialog *dialog, gint id, GaimNotifyMailData *data)
{
	if (id == 0)
		gaim_notify_uri(NULL, data->url);

	gaim_notify_close(GAIM_NOTIFY_EMAILS, data);
}

static void
formatted_close_cb(GtkWidget *win, GdkEvent *event, void *user_data)
{
	gaim_notify_close(GAIM_NOTIFY_FORMATTED, win);
}

static void *
gaim_gtk_notify_message(GaimNotifyMsgType type, const char *title,
						const char *primary, const char *secondary,
						GCallback cb, void *user_data)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img = NULL;
	char label_text[2048];
	const char *icon_name = NULL;

	switch (type)
	{
		case GAIM_NOTIFY_MSG_ERROR:
			icon_name = GAIM_STOCK_DIALOG_ERROR;
			break;

		case GAIM_NOTIFY_MSG_WARNING:
			icon_name = GAIM_STOCK_DIALOG_WARNING;
			break;

		case GAIM_NOTIFY_MSG_INFO:
			icon_name = GAIM_STOCK_DIALOG_INFO;
			break;

		default:
			icon_name = NULL;
			break;
	}

	if (icon_name != NULL)
	{
		img = gtk_image_new_from_stock(icon_name, GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	}

	dialog = gtk_dialog_new_with_buttons(title ? title : GAIM_ALERT_TITLE,
					     NULL, 0, GTK_STOCK_OK,
					     GTK_RESPONSE_ACCEPT, NULL);

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(message_response_cb), dialog);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	if (img != NULL)
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			   primary, (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(dialog);

	return dialog;
}

static void *
gaim_gtk_notify_email(const char *subject, const char *from,
					  const char *to, const char *url,
					  GCallback cb, void *user_data)
{
	return gaim_gtk_notify_emails(1, TRUE,
								  (subject == NULL ? NULL : &subject),
								  (from    == NULL ? NULL : &from),
								  (to      == NULL ? NULL : &to),
								  (url     == NULL ? NULL : &url),
								  cb, user_data);
}

static void *
gaim_gtk_notify_emails(size_t count, gboolean detailed,
					   const char **subjects, const char **froms,
					   const char **tos, const char **urls,
					   GCallback cb, void *user_data)
{
	GaimNotifyMailData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	char *detail_text;
	char *label_text;

	data = g_new0(GaimNotifyMailData, 1);

	data->url = g_strdup(urls[0]);

	/* Create the dialog. */
	data->dialog = dialog = gtk_dialog_new();

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, 1);

	if (urls != NULL)
		gtk_dialog_add_button(GTK_DIALOG(dialog), GAIM_STOCK_OPEN_MAIL, 0);

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(email_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_INFO,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	detail_text = g_strdup_printf(ngettext("%s has %d new message.",
										   "%s has %d new messages.",
										   (int)count),
								  *tos, (int)count);

	if (count == 1)
	{
		char *from_text = NULL, *subject_text = NULL;

		if (froms != NULL)
		{
			from_text = g_strdup_printf(
				_("<span weight=\"bold\">From:</span> %s\n"), *froms);
		}

		if (subjects != NULL)
		{
			subject_text = g_strdup_printf(
				_("<span weight=\"bold\">Subject:</span> %s\n"), *subjects);
		}

		label_text = g_strdup_printf(
			_("<span weight=\"bold\" size=\"larger\">You have mail!</span>"
			"\n\n%s%s%s%s"),
			detail_text,
			(from_text == NULL && subject_text == NULL ? "" : "\n\n"),
			(from_text == NULL ? "" : from_text),
			(subject_text == NULL ? "" : subject_text));

		if (from_text != NULL)
			g_free(from_text);

		if (subject_text != NULL)
			g_free(subject_text);
	}
	else
	{
		label_text = g_strdup_printf(
			_("<span weight=\"bold\" size=\"larger\">You have mail!</span>"
			"\n\n%s"), detail_text);
	}

	g_free(detail_text);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

static gboolean
formatted_input_cb(GtkWidget *dialog, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Escape)
	{
		gtk_widget_destroy(dialog);

		return TRUE;
	}

	return FALSE;
}

static void *
gaim_gtk_notify_formatted(const char *title, const char *primary,
						  const char *secondary, const char *text,
						  GCallback cb, void *user_data)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *imhtml;
	GtkWidget *sw;
	GSList *images = NULL;
	int options = 0;
	char label_text[2048];
	char *linked_text;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(title), title);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), 12);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(formatted_close_cb), NULL);

	/* Setup the main vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	/* Setup the descriptive label */
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			   primary,
			   (secondary ? "\n" : ""),
			   (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Setup the scrolled window that we're putting the gtkimhtml in. */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_NEVER,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_set_size_request(sw, 300, 250);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Now build that gtkimhtml */
	imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), imhtml);
	gtk_widget_show(imhtml);
	gaim_setup_imhtml(imhtml);

	/* Add the Close button. */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(formatted_close_cb), window);
	g_signal_connect(G_OBJECT(window), "key_press_event",
					 G_CALLBACK(formatted_input_cb), NULL);


	/* Add the text to the gtkimhtml */
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_colors"))
		options ^= GTK_IMHTML_NO_COLOURS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_fonts"))
		options ^= GTK_IMHTML_NO_FONTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_font_sizes"))
		options ^= GTK_IMHTML_NO_SIZES;

	options ^= GTK_IMHTML_NO_COMMENTS;
	options ^= GTK_IMHTML_NO_TITLE;
	options ^= GTK_IMHTML_NO_NEWLINE;
	options ^= GTK_IMHTML_NO_SCROLL;

	gaim_gtk_find_images(text, &images);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_urls_as_links"))
	{
		linked_text = gaim_markup_linkify(text);
		gtk_imhtml_append_text_with_images(GTK_IMHTML(imhtml), linked_text,
										   options, images);
		g_free(linked_text);
	}
	else
	{
		gtk_imhtml_append_text_with_images(GTK_IMHTML(imhtml), text,
										   options, images);
	}

	if (images)
	{
		GSList *tmp;

		for (tmp = images; tmp; tmp = tmp->next)
		{
			GdkPixbuf *pixbuf = tmp->data;

			if (pixbuf != NULL)
				g_object_unref(pixbuf);
		}

		g_slist_free(images);
	}

	/* Show the window */
	gtk_widget_show(window);

	return window;
}

static void
gaim_gtk_close_notify(GaimNotifyType type, void *ui_handle)
{
	if (type == GAIM_NOTIFY_EMAIL || type == GAIM_NOTIFY_EMAILS)
	{
		GaimNotifyMailData *data = (GaimNotifyMailData *)ui_handle;

		gtk_widget_destroy(data->dialog);

		g_free(data->url);
		g_free(data);
	}
	else
		gtk_widget_destroy(GTK_WIDGET(ui_handle));
}

#ifndef _WIN32
static gint
uri_command(const char *command, gboolean sync)
{
	GError *error = NULL;
	gint ret = 0;

	gaim_debug_misc("gtknotify", "Executing %s\n", command);

	if (!gaim_program_is_valid(command))
	{
		gchar *tmp;

		tmp = g_strdup_printf(_("The browser command \"%s\" is invalid."),
							  command);

		gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);

		g_free(tmp);

	}
	else if (sync)
	{
		gint status;

		if (!g_spawn_command_line_sync(command, NULL, NULL, &status, &error))
		{
			char *tmp = g_strdup_printf(_("Error launching \"%s\": %s"),
										command, error->message);

			gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);

			g_free(tmp);
			g_error_free(error);
		}
		else
			ret = status;
	}
	else
	{
		if (!g_spawn_command_line_async(command, &error))
		{
			char *tmp = g_strdup_printf(_("Error launching \"%s\": %s"),
										command, error->message);

			gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);

			g_free(tmp);
			g_error_free(error);
		}
	}

	return ret;
}
#endif /* _WIN32 */

static void *
gaim_gtk_notify_uri(const char *uri)
{
#ifndef _WIN32
	char *command = NULL;
	char *remote_command = NULL;
	const char *web_browser;
	int place;

	web_browser = gaim_prefs_get_string("/gaim/gtk/browsers/browser");
	place = gaim_prefs_get_int("/gaim/gtk/browsers/place");

	if (!strcmp(web_browser, "netscape"))
	{
		command = g_strdup_printf("netscape \"%s\"", uri);

		if (place == GAIM_BROWSER_NEW_WINDOW)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "\"openURL(\"%s\",new-window)\"",
											 uri);
		}
		else if (place == GAIM_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "\"openURL(\"%s\")\"", uri);
		}
	}
	else if (!strcmp(web_browser, "opera"))
	{
		if (place == GAIM_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("opera -newwindow \"%s\"", uri);
		else if (place == GAIM_BROWSER_NEW_TAB)
			command = g_strdup_printf("opera -newpage \"%s\"", uri);
		else if (place == GAIM_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("opera -remote "
											 "\"openURL(\"%s\")\"", uri);
			command = g_strdup_printf("opera \"%s\"", uri);
		}
		else
			command = g_strdup_printf("opera \"%s\"", uri);

	}
	else if (!strcmp(web_browser, "kfmclient"))
	{
		command = g_strdup_printf("kfmclient openURL \"%s\"", uri);
		/*
		 * Does Konqueror have options to open in new tab
		 * and/or current window?
		 */
	}
	else if (!strcmp(web_browser, "galeon"))
	{
		if (place == GAIM_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("galeon -w \"%s\"", uri);
		else if (place == GAIM_BROWSER_NEW_TAB)
			command = g_strdup_printf("galeon -n \"%s\"", uri);
		else
			command = g_strdup_printf("galeon \"%s\"", uri);
	}
	else if (!strcmp(web_browser, "mozilla") ||
			 !strcmp(web_browser, "mozilla-firebird") ||
			 !strcmp(web_browser, "firefox"))
	{
		command = g_strdup_printf("%s \"%s\"", web_browser, uri);

		if (place == GAIM_BROWSER_NEW_WINDOW)
		{
			remote_command = g_strdup_printf("%s -remote \"openURL(\"%s\","
											 "new-window)\"",
											 web_browser, uri);
		}
		else if (place == GAIM_BROWSER_NEW_TAB)
		{
			remote_command = g_strdup_printf("%s -remote \"openURL(\"%s\","
											 "new-tab)\"",
											 web_browser, uri);
		}
		else if (place == GAIM_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("%s -remote \"openURL(\"%s\")\"",
											 web_browser, uri);
		}
	}
	else if (!strcmp(web_browser, "custom"))
	{
		const char *web_command;

		web_command = gaim_prefs_get_string("/gaim/gtk/browsers/command");

		if (web_command == NULL || *web_command == '\0')
		{
			gaim_notify_error(NULL, NULL, _("Unable to open URL"),
							  _("The 'Manual' browser command has been "
								"chosen, but no command has been set."));
			return NULL;
		}

		if (strstr(web_command, "%s"))
			command = gaim_strreplace(web_command, "%s", uri);
		else
		{
			/*
			 * There is no "%s" in the browser command.  Assume the user
			 * wanted the URL tacked on to the end of the command.
			 */
			command = g_strdup_printf("%s %s", web_command, uri);
		}
	}

	if (remote_command != NULL)
	{
		/* try the remote command first */
		if (uri_command(remote_command, TRUE) != 0)
			uri_command(command, FALSE);

		g_free(remote_command);

	}
	else
		uri_command(command, FALSE);

	g_free(command);

#else /* !_WIN32 */
	ShellExecute(NULL, NULL, uri, NULL, ".\\", 0);
#endif /* !_WIN32 */

	return NULL;
}

static GaimNotifyUiOps ops =
{
	gaim_gtk_notify_message,
	gaim_gtk_notify_email,
	gaim_gtk_notify_emails,
	gaim_gtk_notify_formatted,
	gaim_gtk_notify_uri,
	gaim_gtk_close_notify
};

GaimNotifyUiOps *
gaim_gtk_notify_get_ui_ops(void)
{
	return &ops;
}
