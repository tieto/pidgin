#include "gtkmodule.h"

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
gaim_gtkconv_new(class, conv)
	Gaim::Conversation conv
    C_ARGS:
	conv

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
