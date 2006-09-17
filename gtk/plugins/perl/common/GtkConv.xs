#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
guint
gaim_gtk_conversations_fill_menu(menu, convs)
	Gtk::Widget menu
	SV *convs
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(convs));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(convs), i, 0), t_sl));
	}
	gaim_gtk_conversations_fill_menu(menu, t_GL);
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
GdkPixbuf
gaim_gtkconv_get_tab_icon(conv, small_icon)
	Gaim::Conversation conv
	gboolean small_icon
*/

/* This can't work at the moment since I don't have a typemap for gboolean *.
int
gaim_gtkconv_get_tab_at_xy(win, x, y, to_right)
	Gaim::GtkUI::Conversation::Window win
	int x
	int y
	gboolean * to_right
*/

MODULE = Gaim::GtkUI::Conversation  PACKAGE = Gaim::GtkUI::Conversation  PREFIX = gaim_gtkconv_
PROTOTYPES: ENABLE

void
gaim_gtkconv_update_buddy_icon(conv)
	Gaim::Conversation conv

void
gaim_gtkconv_switch_active_conversation(conv)
	Gaim::Conversation conv

void
gaim_gtkconv_update_buttons_by_protocol(conv)
	Gaim::Conversation conv

void
gaim_gtkconv_present_conversation(conv)
	Gaim::Conversation conv

Gaim::GtkUI::Conversation::Window
gaim_gtkconv_get_window(conv)
	Gaim::GtkUI::Conversation conv

void
gaim_gtkconv_new(conv)
	Gaim::Conversation conv

gboolean
gaim_gtkconv_is_hidden(gtkconv)
	Gaim::GtkUI::Conversation gtkconv

MODULE = Gaim::GtkUI::Conversation  PACKAGE = Gaim::GtkUI::Conversations  PREFIX = gaim_gtk_conversations_
PROTOTYPES: ENABLE

void
gaim_gtk_conversations_find_unseen_list(type, min_state, hidden_only, max_count)
	Gaim::ConversationType type
	Gaim::UnseenState min_state
	gboolean hidden_only
	guint max_count

void *
gaim_gtk_conversations_get_handle()
