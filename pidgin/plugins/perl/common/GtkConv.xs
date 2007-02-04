#include "gtkmodule.h"

MODULE = Pidgin::Conversation  PACKAGE = Pidgin::Conversation  PREFIX = pidgin_conv_
PROTOTYPES: ENABLE

void
pidgin_conv_update_buddy_icon(conv)
	Gaim::Conversation conv

void
pidgin_conv_switch_active_conversation(conv)
	Gaim::Conversation conv

void
pidgin_conv_update_buttons_by_protocol(conv)
	Gaim::Conversation conv

void
pidgin_conv_present_conversation(conv)
	Gaim::Conversation conv

Pidgin::Conversation::Window
pidgin_conv_get_window(conv)
	Pidgin::Conversation conv

void
pidgin_conv_new(class, conv)
	Gaim::Conversation conv
    C_ARGS:
	conv

gboolean
pidgin_conv_is_hidden(gtkconv)
	Pidgin::Conversation gtkconv

void
pidgin_conv_get_gtkconv(conv)
	Gaim::Conversation conv
PPCODE:
	if (conv != NULL && GAIM_IS_GTK_CONVERSATION(conv))
		XPUSHs(sv_2mortal(gaim_perl_bless_object(
				PIDGIN_CONVERSATION(conv),
				"Pidgin::Conversation")));

MODULE = Pidgin::Conversation  PACKAGE = Pidgin::Conversations  PREFIX = pidgin_conversations_
PROTOTYPES: ENABLE

void
pidgin_conversations_find_unseen_list(type, min_state, hidden_only, max_count)
	Gaim::ConversationType type
	Gaim::UnseenState min_state
	gboolean hidden_only
	guint max_count

Gaim::Handle
pidgin_conversations_get_handle()
