#include "module.h"

MODULE = Gaim::Conversation::Chat  PACKAGE = Gaim::Conversation::Chat  PREFIX = gaim_chat_
PROTOTYPES: ENABLE

Gaim::Conversation::Chat
new(account, name)
	Gaim::Account account
	const char *name
CODE:
	RETVAL = GAIM_CHAT(gaim_conversation_new(GAIM_CONV_CHAT, account, name));
OUTPUT:
	RETVAL

void
DESTROY(chat)
	Gaim::Conversation::Chat chat
CODE:
	gaim_conversation_destroy(gaim_chat_get_conversation(chat));


Gaim::Conversation
gaim_chat_get_conversation(chat)
	Gaim::Conversation::Chat chat

void
users(chat)
	Gaim::Conversation::Chat chat
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_chat_get_users(chat); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

void
gaim_chat_ignore(chat, name)
	Gaim::Conversation::Chat chat
	const char *name

void
gaim_chat_unignore(chat, name)
	Gaim::Conversation::Chat chat
	const char *name

void
ignored_users(chat)
	Gaim::Conversation::Chat chat
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_chat_get_ignored(chat); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

gboolean
gaim_chat_is_user_ignored(chat, user)
	Gaim::Conversation::Chat chat
	const char *user

void
gaim_chat_set_topic(chat, who, topic)
	Gaim::Conversation::Chat chat
	const char *who
	const char *topic

const char *
gaim_chat_get_topic(chat)
	Gaim::Conversation::Chat chat

int
gaim_chat_get_id(chat)
	Gaim::Conversation::Chat chat

void
write(chat, who, message, flags)
	Gaim::Conversation::Chat chat
	const char *who
	const char *message
	int flags
CODE:
	gaim_chat_write(chat, who, message, flags, time(NULL);

void
gaim_chat_send(chat, message)
	Gaim::Conversation::Chat chat
	const char *message


MODULE = Gaim::Conversation  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

void
chats()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_chats(); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data,
			"Gaim::Conversation")));
	}
