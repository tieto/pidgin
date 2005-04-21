#include "module.h"

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversation  PREFIX = gaim_conversation_
PROTOTYPES: ENABLE

void
gaim_conversation_set_account(conv, account)
	Gaim::Conversation conv
	Gaim::Account account

Gaim::Account
gaim_conversation_get_account(conv)
	Gaim::Conversation conv

Gaim::Connection
gaim_conversation_get_gc(conv)
	Gaim::Conversation conv

void
gaim_conversation_set_title(conv, title)
	Gaim::Conversation conv
	const char *title

void
gaim_conversation_autoset_title(conv)
	Gaim::Conversation conv

const char *
gaim_conversation_get_name(conv)
	Gaim::Conversation conv

void
gaim_conversation_set_logging(conv, log)
	Gaim::Conversation conv
	gboolean log

gboolean
gaim_conversation_is_logging(conv)
	Gaim::Conversation conv

Gaim::ConvWindow
gaim_conversation_get_window(conv)
	Gaim::Conversation conv

gboolean
is_chat(conv)
	Gaim::Conversation conv
CODE:
	RETVAL = (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT);
OUTPUT:
	RETVAL

gboolean
is_im(conv)
	Gaim::Conversation conv
CODE:
	RETVAL = (gaim_conversation_get_type(conv) == GAIM_CONV_IM);
OUTPUT:
	RETVAL

void
gaim_conversation_set_data(conv, key, data)
	Gaim::Conversation conv
	const char *key
	void *data

void *
gaim_conversation_get_data(conv, key)
	Gaim::Conversation conv
	const char *key

void
gaim_conversation_write(conv, who, message, flags)
	Gaim::Conversation conv
	const char *who
	const char *message
	int flags
CODE:
	gaim_conversation_write(conv, who, message, flags, time(NULL));

Gaim::Conversation::IM
gaim_conversation_get_im_data(conv)
	Gaim::Conversation conv

Gaim::Conversation::Chat
gaim_conversation_get_chat_data(conv)
	Gaim::Conversation conv


MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversations  PREFIX = gaim_conversations_
PROTOTYPES: ENABLE

Gaim::Conversation
find_with_account(name, account)
	const char *name
	Gaim::Account account
CODE:
	RETVAL = gaim_find_conversation_with_account(GAIM_CONV_ANY, name, account);
OUTPUT:
	RETVAL

void *
handle()
CODE:
	RETVAL = gaim_conversations_get_handle();
OUTPUT:
	RETVAL


MODULE = Gaim::Conversation  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

void
conversations()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_conversations(); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data,
			"Gaim::Conversation")));
	}
