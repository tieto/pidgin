#include "gtkmodule.h"

MODULE = Pidgin::Conversation  PACKAGE = Pidgin::Conversation  PREFIX = pidgin_conv_
PROTOTYPES: ENABLE

void
pidgin_conv_update_buddy_icon(im)
	Purple::IMConversation im

void
pidgin_conv_switch_active_conversation(conv)
	Purple::Conversation conv

void
pidgin_conv_update_buttons_by_protocol(conv)
	Purple::Conversation conv

void
pidgin_conv_present_conversation(conv)
	Purple::Conversation conv

Pidgin::Conversation::Window
pidgin_conv_get_window(conv)
	Pidgin::Conversation conv

void
pidgin_conv_new(class, conv)
	Purple::Conversation conv
    C_ARGS:
	conv

gboolean
pidgin_conv_is_hidden(gtkconv)
	Pidgin::Conversation gtkconv

void
pidgin_conv_get_gtkconv(conv)
	Purple::Conversation conv
PPCODE:
	if (conv != NULL && PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		XPUSHs(sv_2mortal(purple_perl_bless_object(
				PIDGIN_CONVERSATION(conv),
				"Pidgin::Conversation")));

MODULE = Pidgin::Conversation  PACKAGE = Pidgin::Conversations  PREFIX = pidgin_conversations_
PROTOTYPES: ENABLE

void
pidgin_conversations_get_unseen_all(min_state, hidden_only, max_count)
	Pidgin::UnseenState min_state
	gboolean hidden_only
	guint max_count

void
pidgin_conversations_get_unseen_ims(min_state, hidden_only, max_count)
	Pidgin::UnseenState min_state
	gboolean hidden_only
	guint max_count

void
pidgin_conversations_get_unseen_chats(min_state, hidden_only, max_count)
	Pidgin::UnseenState min_state
	gboolean hidden_only
	guint max_count

Purple::Handle
pidgin_conversations_get_handle()
