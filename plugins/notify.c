/* Rewritten by Etan Reisner <deryni@eden.rutgers.edu>
 *
 * Added config dialog
 * Added control over notification method
 * Added control over when to release notification
 * 
 * Added option to get notification for chats also
 * Cleaned up code
 * Added option to notify on click as it's own option
 *  rather then as what happens when on focus isn't clicked
 * Added apply button to change the denotification methods for
 *  open conversation windows
 *
 * Thanks to Carles Pina i Estany <carles@pinux.info>
 *   for count of new messages option
 */

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include "gaim.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

guint type = 1;
#define TYPE_IM         0x00000001
#define TYPE_CHAT       0x00000002

guint choice = 1;
#define NOTIFY_FOCUS		0x00000001
#define NOTIFY_TYPE			0x00000002
#define NOTIFY_IN_FOCUS	0x00000004
#define NOTIFY_CLICK    0x00000008

guint method = 1;
#define METHOD_STRING		0x00000001
#define METHOD_QUOTE		0x00000002
#define METHOD_URGENT		0x00000004
#define METHOD_COUNT		0x00000008

void *handle;
GtkWidget *Entry;
gchar *title_string = "(*) ";

/* predefine some functions, less warnings */
void options(GtkWidget *widget, gpointer data);
void un_star(GtkWidget *widget, gpointer data);
/* this returns an int so that typing events don't get stopped here */
int un_star_window(GtkWidget *widget, gpointer data);
int counter (char *buf, int *length);
/*string functions */
void string_add(GtkWidget *widget);
gboolean string_remove(GtkWidget *widget);
/* count functions */
void count_add(GtkWidget *widget);
gboolean count_remove(GtkWidget *widget);
/* quote functions */
void quote_add(GtkWidget *widget);
gboolean quote_remove(GtkWidget *widget);
/* urgent functions */
void urgent_add(struct conversation *c);
gboolean urgent_remove(struct conversation *c);

struct conversation *find_chat(struct gaim_connection *gc, int id) {
	GList *cnv = chats;
	struct conversation *c;

	while (cnv) {
		c = (struct conversation *) cnv->data;

		if (c && (c->gc == gc) && c->is_chat && (c->id == id))
			return c;

		cnv = cnv->next;
	}
	return NULL;
}

int notify(struct conversation *cnv) {
	Window focus_return;
	int revert_to_return;

	XGetInputFocus(GDK_WINDOW_XDISPLAY(cnv->window->window), &focus_return, &revert_to_return);

	if ((choice & NOTIFY_IN_FOCUS) || focus_return != GDK_WINDOW_XWINDOW(cnv->window->window)) {
		if (method & METHOD_STRING)
			string_add(cnv->window);
		if (method & METHOD_COUNT)
			count_add(cnv->window);
		if (method & METHOD_QUOTE)
			quote_add(cnv->window);
		if (method & METHOD_URGENT)
			urgent_add(cnv);
	}
	return 0;
}

guint unnotify(struct conversation *c, gboolean clean) {
	guint option = 0;
	/* The top level ifs check whether we are either cleaning all methods,
	 * or whether we have that method is currently selected.
	 * If we do then they are cleaned
	 *
	 * The second level ifs check if we removed something,
	 * and if that method is currently selected.
	 * If we did and it is then set option so that it can be re-added */
	if (clean || (method & METHOD_QUOTE))
		if (quote_remove(c->window) && (method & METHOD_QUOTE))
			option ^= METHOD_QUOTE;
	if (clean || (method & METHOD_COUNT))
		if (count_remove(c->window) && (method & METHOD_COUNT))
			option ^= METHOD_COUNT;
	if (clean || (method & METHOD_STRING))
		if (string_remove(c->window) && (method & METHOD_STRING))
			option ^= METHOD_STRING;
	if (clean || (method & METHOD_URGENT))
		if (urgent_remove(c) && (method & METHOD_URGENT))
			option ^= METHOD_URGENT;
	return option;
}

void chat_recv_im(struct gaim_connection *gc, int id, char **who, char **text) {
	struct conversation *c = find_chat(gc, id);

	if (c && (type & TYPE_CHAT))
		notify(c);
	return;
}

void chat_sent_im(struct gaim_connection *gc, int id, char **text) {
	struct conversation *c = find_chat(gc, id);

	if (c && (type & TYPE_CHAT))
		unnotify(c, FALSE);
	return;
}

int im_recv_im(struct gaim_connection *gc, char **who, char **what, void *m) {
	struct conversation *c = find_conversation(*who);

	if (c && (type & TYPE_IM))
		notify(c);
	return 0;
}

int im_sent_im(struct gaim_connection *gc, char *who, char **what, void *m) {
	struct conversation *c = find_conversation(who);

	if (c && (type & TYPE_IM))
		unnotify(c, FALSE);
	return 0;
}

int attach_signals(struct conversation *c) {
	if (choice & NOTIFY_FOCUS) {
		g_signal_connect(G_OBJECT(c->window), "focus-in-event", G_CALLBACK(un_star), NULL);
	}

	if (choice & NOTIFY_CLICK) {
		g_signal_connect(G_OBJECT(c->window), "button_press_event", G_CALLBACK(un_star), NULL);

		g_signal_connect(G_OBJECT(c->text), "button_press_event", G_CALLBACK(un_star_window), NULL);

		g_signal_connect(G_OBJECT(c->entry), "button_press_event", G_CALLBACK(un_star_window), NULL);
	}

	if (choice & NOTIFY_TYPE) {
		g_signal_connect(G_OBJECT(c->entry), "key-press-event", G_CALLBACK(un_star_window), NULL);
	}

	g_object_set_data(G_OBJECT(c->window), "user_data", c);
	g_object_set_data(G_OBJECT(c->window), "notify_data", GUINT_TO_POINTER(choice));
	return 0;
}

void new_conv(char *who) {
	struct conversation *c = find_conversation(who);

	if (c && (type & TYPE_IM))
		attach_signals(c);
	return;
}

void chat_join(struct gaim_connection *gc, int id, char *room) {
	struct conversation *c = find_chat(gc, id);

	if (c && (type & TYPE_CHAT))
		attach_signals(c);
	return;
}

void un_star(GtkWidget *widget, gpointer data) {
	struct conversation *c = g_object_get_data(G_OBJECT(widget), "user_data");

	if (method & METHOD_QUOTE)
		quote_remove(widget);
	if (method & METHOD_COUNT)
		count_remove(widget);
	if (method & METHOD_STRING)
		string_remove(widget);
	if (c && method & METHOD_URGENT)
		urgent_remove(c);
	return;
}

int un_star_window(GtkWidget *widget, gpointer data) {
	GtkWidget *parent = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
	un_star(parent, data);
	/* return 0 so that the event continues to propagate */
	return 0;
}

/* This function returns the number in [ ]'s or 0 */
int counter (char *buf, int *length) {
	char temp[256];
	int i = 1;
	*length = 0;

	while (isdigit(buf[i]) && i<sizeof(buf)) {
		temp[i-1] = buf[i];
		(*length)++;
		i++;
	}
	temp[i] = '\0';

	if (buf[i] != ']') {
		*length = 0;
		return (0);
	}

	return (atoi(temp));
}

void string_add(GtkWidget *widget) {
	char buf[256];
	GtkWindow *win = GTK_WINDOW(widget);

	strncpy(buf, win->title, sizeof(buf));
	if (!strstr(buf, title_string)) {
		g_snprintf(buf, sizeof(buf), "%s%s", title_string, win->title);
		gtk_window_set_title(win, buf);
	}
}

gboolean string_remove(GtkWidget *widget) {
	char buf[256];
	GtkWindow *win = GTK_WINDOW(widget);

	strncpy(buf, win->title, sizeof(buf));
	if (strstr(buf, title_string)) {
		g_snprintf(buf, sizeof(buf), "%s", &win->title[strlen(title_string)]);
		gtk_window_set_title(win, buf);
		return TRUE;
	}
	return FALSE;
}

void count_add(GtkWidget *widget) {
	char buf[256];
	int c, length;
	GtkWindow *win = GTK_WINDOW(widget);

	strncpy(buf, win->title, sizeof(buf));
	c = counter(buf, &length);
	if (!c) {
		g_snprintf(buf, sizeof(buf), "[1] %s", win->title);
	}
	else if (!g_strncasecmp(buf, "[", 1)) {
		g_snprintf(buf, sizeof(buf), "[%d] %s", c+1, &win->title[3+length]);
	}
	gtk_window_set_title(win, buf);
}

gboolean count_remove(GtkWidget *widget) {
	char buf[256];
	GtkWindow *win = GTK_WINDOW(widget);
	int length;

	strncpy(buf, win->title, sizeof(buf));
	if (!g_strncasecmp(buf, "[", 1)) {
		counter(buf, &length);
		g_snprintf(buf, sizeof(buf), "%s", &win->title[3+length]);
		gtk_window_set_title(win, buf);
		return TRUE; 
	}
	return FALSE;
}

void quote_add(GtkWidget *widget) {
	char buf[256];
	GtkWindow *win = GTK_WINDOW(widget);

	strncpy(buf, win->title, sizeof(buf));
	if (g_strncasecmp(buf, "\"", 1)) {
		g_snprintf(buf, sizeof(buf), "\"%s\"", win->title);
		gtk_window_set_title(win, buf);
	}
}

gboolean quote_remove(GtkWidget *widget) {
	char buf[256];
	GtkWindow *win = GTK_WINDOW(widget);

	strncpy(buf, win->title, sizeof(buf));
	if (!g_strncasecmp(buf, "\"", 1)) {
		g_snprintf(buf, strlen(buf) - 1, "%s", &win->title[1]);
		gtk_window_set_title(win, buf);
		return TRUE;
	}
	return FALSE;
}

void urgent_add(struct conversation *c) {
	XWMHints *hints = XGetWMHints(GDK_WINDOW_XDISPLAY(c->window->window), GDK_WINDOW_XWINDOW(c->window->window));
	hints->flags |= XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(c->window->window), GDK_WINDOW_XWINDOW(c->window->window), hints);
}

gboolean urgent_remove(struct conversation *c) {
	GdkWindow *win = c->window->window;

	XWMHints *hints = XGetWMHints(GDK_WINDOW_XDISPLAY(win), GDK_WINDOW_XWINDOW(win));
	if (hints->flags & XUrgencyHint) {
		hints->flags &= ~XUrgencyHint;
		XSetWMHints(GDK_WINDOW_XDISPLAY(win), GDK_WINDOW_XWINDOW(win), hints);
		return TRUE;
	}
	return FALSE;
}

void save_notify_prefs() {
	gchar buf[1000];
	FILE *fp;

	snprintf(buf, 1000, "%s/.gaim/.notify", getenv("HOME"));
	if (!(fp = fopen(buf, "w"))) {
		do_error_dialog(_("Unable to write to config file"), _("Notify plugin"), GAIM_ERROR);
		return;
	}

	fprintf(fp, "%d=TYPE\n", type);
	fprintf(fp, "%d=CHOICE\n", choice);
	fprintf(fp, "%d=METHOD\n", method);
	fprintf(fp, "%s=STRING\n", title_string);
	fclose(fp);
}

void load_notify_prefs() {
	gchar buf[1000];
	gchar **parsed;
	FILE *fp;

	g_snprintf(buf, sizeof(buf), "%s/.gaim/.notify", getenv("HOME"));
	if (!(fp = fopen(buf, "r")))
		return;

	while (fgets(buf, 1000, fp) != NULL) {
		parsed = g_strsplit(g_strchomp(buf), "=", 2);
		if (parsed[0] && parsed[1]) {
			if (!strcmp(parsed[1], "TYPE"))
				type = atoi(parsed[0]);
			if (!strcmp(parsed[1], "CHOICE"))
				choice = atoi(parsed[0]);
			if (!strcmp(parsed[1], "METHOD"))
				method = atoi(parsed[0]);
			if (!strcmp(parsed[1], "STRING"))
				if (title_string != NULL) g_free(title_string);
			title_string = g_strdup(parsed[0]);
		}
		g_strfreev(parsed);
	}
	fclose(fp);
	return;
}

void options(GtkWidget *widget, gpointer data) {
	gint option = GPOINTER_TO_INT(data);

	if (option == 0)
		choice ^= NOTIFY_FOCUS;
	else if (option == 1)
		choice ^= NOTIFY_CLICK;
	else if (option == 2)
		choice ^= NOTIFY_TYPE;
	else if (option == 3) {
		method ^= METHOD_STRING;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			gtk_widget_set_sensitive(Entry, TRUE);
		else
			gtk_widget_set_sensitive(Entry, FALSE);
	}
	else if (option == 4)
		method ^= METHOD_QUOTE;
	else if (option == 5)
		method ^= METHOD_URGENT;
	else if (option == 6)
		choice ^= NOTIFY_IN_FOCUS;
	else if (option == 7)
		method ^= METHOD_COUNT;
	else if (option == 8)
		type ^= TYPE_IM;
	else if (option == 9)
		type ^= TYPE_CHAT;

	save_notify_prefs();
}

void apply_options(GtkWidget *widget, gpointer data) {
	GList *cnv = conversations;

	while (cnv) {
		guint notification;
		struct conversation *c = (struct conversation *) cnv->data;
		guint options = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(c->window), "notify_data"));

		if (options & NOTIFY_FOCUS)
			g_signal_handlers_disconnect_by_func(G_OBJECT(c->window), un_star, NULL);

		if (options & NOTIFY_CLICK) {
			g_signal_handlers_disconnect_by_func(G_OBJECT(c->window), un_star, NULL);
			g_signal_handlers_disconnect_by_func(G_OBJECT(c->text), un_star_window, NULL);
			g_signal_handlers_disconnect_by_func(G_OBJECT(c->entry), un_star_window, NULL);
		}

		if (options & NOTIFY_TYPE)
			g_signal_handlers_disconnect_by_func(G_OBJECT(c->entry), un_star_window, NULL);

		/* works except for count, always get reset to [1] */
		/* clean off all notification markings */
		notification = unnotify(c, TRUE);
		/* re-add appropriate notification methods cleaned above */
		if (notification & METHOD_STRING) /* re-add string */
			string_add(c->window);
		if (notification & METHOD_QUOTE) /* re-add quote */
			quote_add(c->window);
		if (notification & METHOD_COUNT) /* re-add count */
			count_add(c->window);
		if (notification & METHOD_URGENT) /* re-add urgent */
			urgent_add(c);
		/* attach new unnotification signals */
		attach_signals(c);

		cnv = cnv->next;
	}

	return;
}

char *gaim_plugin_init(GModule *hndl) {
	handle = hndl;

	load_notify_prefs();

	gaim_signal_connect(handle, event_im_recv, im_recv_im, NULL);
	gaim_signal_connect(handle, event_chat_recv, chat_recv_im, NULL);
	gaim_signal_connect(handle, event_im_send, im_sent_im, NULL);
	gaim_signal_connect(handle, event_chat_send, chat_sent_im, NULL);
	gaim_signal_connect(handle, event_new_conversation, new_conv, NULL);
	gaim_signal_connect(handle, event_chat_join, chat_join, NULL);
	return NULL;
}

void gaim_plugin_remove() {
	GList *c = conversations;

	while (c) {
		struct conversation *cnv = (struct conversation *)c->data;
		un_star(cnv->window, NULL);
		c = c->next;
	}
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Message Notification");
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup("Provides a variety of ways of notifying you of unread messages.");
	desc.authors = g_strdup("Etan Reisner &lt;deryni@eden.rutgers.edu>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

char *name() {
	return "Visual Notification";
}

char *description() {
	return "Puts an asterisk in the title bar of all conversations"
		" where you have not responded to a message yet.";
}

GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *ret;
	GtkWidget *vbox, *hbox;
	GtkWidget *toggle, *button;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame(ret, _("Notify For"));
	toggle = gtk_check_button_new_with_mnemonic(_("_IM windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), type & TYPE_IM);
	g_signal_connect(GTK_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(7));

	toggle = gtk_check_button_new_with_mnemonic(_("_Chat windows"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), type & TYPE_CHAT);
	g_signal_connect(GTK_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(8));

	/*--------------*/
	vbox = make_frame(ret, _("Notification Methods"));
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	toggle = gtk_check_button_new_with_mnemonic(_("Prepend _string into window title:"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), method & METHOD_STRING);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(3));
	gtk_box_pack_start(GTK_BOX(hbox), toggle, FALSE, FALSE, 0);
	Entry = gtk_entry_new_with_max_length(7);
	gtk_widget_set_sensitive(GTK_WIDGET(Entry), method & METHOD_STRING);
	gtk_box_pack_start(GTK_BOX(hbox), Entry, FALSE, FALSE, 0);
	gtk_entry_set_text(GTK_ENTRY(Entry), title_string);

	toggle = gtk_check_button_new_with_mnemonic(_("_Quote window title"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), method & METHOD_QUOTE);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(4));

	toggle = gtk_check_button_new_with_mnemonic(_("Set Window Manager \"_URGENT\" Hint"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), method & METHOD_URGENT);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(5));

	toggle = gtk_check_button_new_with_mnemonic(_("Insert c_ount of new messages into window title"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), method & METHOD_COUNT);
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(7));

	toggle = gtk_check_button_new_with_mnemonic(_("_Notify even if conversation is in focus"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), choice & NOTIFY_IN_FOCUS);
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(6));

	/*--------------*/
	vbox = make_frame(ret, _("Notification Removal"));
	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window gains _focus"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), choice & NOTIFY_FOCUS);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(0));

	toggle = gtk_check_button_new_with_mnemonic(_("Remove when conversation window _receives click"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), choice & NOTIFY_CLICK);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(1));

	toggle = gtk_check_button_new_with_mnemonic(_("Remove when _typing in conversation window"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), choice & NOTIFY_TYPE);
	g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(options), GINT_TO_POINTER(2));

	button = gtk_button_new_with_mnemonic(_("Appl_y"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(apply_options), NULL);

	gtk_widget_show_all(ret);
	return ret;
}
