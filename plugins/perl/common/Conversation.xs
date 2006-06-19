#include "module.h"

MODULE = Gaim::Conversation  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

void
gaim_get_ims()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_ims(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Conversation")));
	}

void
gaim_get_conversations()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Conversation")));
	}

void
gaim_get_chats()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_chats(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Conversation")));
	}

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversations  PREFIX = gaim_conversations_
PROTOTYPES: ENABLE

void *
gaim_conversations_get_handle()

void
gaim_conversations_init()

void
gaim_conversations_uninit()

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversation  PREFIX = gaim_conversation_
PROTOTYPES: ENABLE

void
gaim_conversation_destroy(conv)
	Gaim::Conversation conv

Gaim::ConversationType
gaim_conversation_get_type(conv)
	Gaim::Conversation conv

Gaim::Account
gaim_conversation_get_account(conv)
	Gaim::Conversation conv

Gaim::Connection
gaim_conversation_get_gc(conv)
	Gaim::Conversation conv

void
gaim_conversation_set_title(conv, title);
	Gaim::Conversation conv
	const char * title

const char *
gaim_conversation_get_title(conv)
	Gaim::Conversation conv

void
gaim_conversation_autoset_title(conv)
	Gaim::Conversation conv

void
gaim_conversation_set_name(conv, name)
	Gaim::Conversation conv
	const char *name

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

Gaim::Conversation::IM
gaim_conversation_get_im_data(conv)
	Gaim::Conversation conv

Gaim::Conversation::Chat
gaim_conversation_get_chat_data(conv)
	Gaim::Conversation conv

gpointer
gaim_conversation_get_data(conv, key)
	Gaim::Conversation conv
	const char * key

Gaim::ConnectionFlags
gaim_conversation_get_features(conv)
	Gaim::Conversation conv

gboolean
gaim_conversation_has_focus(conv)
	Gaim::Conversation conv

void
gaim_conversation_update(conv, type)
	Gaim::Conversation conv
	Gaim::ConvUpdateType type

Gaim::Conversation
gaim_conversation_new(class, type, account, name)
	Gaim::ConversationType type
	Gaim::Account account
	const char *name
    C_ARGS:
	type, account, name

void
gaim_conversation_set_account(conv, account);
	Gaim::Conversation conv
	Gaim::Account account

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversation::IM  PREFIX = gaim_conv_im_
PROTOTYPES: ENABLE

Gaim::Conversation
gaim_conv_im_get_conversation(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_set_icon(im, icon)
	Gaim::Conversation::IM im
	Gaim::Buddy::Icon icon

Gaim::Buddy::Icon
gaim_conv_im_get_icon(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_set_typing_state(im, state)
	Gaim::Conversation::IM im
	Gaim::TypingState state

Gaim::TypingState
gaim_conv_im_get_typing_state(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_start_typing_timeout(im, timeout)
	Gaim::Conversation::IM im
	int timeout

void
gaim_conv_im_stop_typing_timeout(im)
	Gaim::Conversation::IM im

guint
gaim_conv_im_get_typing_timeout(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_set_type_again(im, val)
	Gaim::Conversation::IM im
	time_t val

time_t
gaim_conv_im_get_type_again(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_start_type_again_timeout(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_stop_type_again_timeout(im)
	Gaim::Conversation::IM im

guint
gaim_conv_im_get_type_again_timeout(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_update_typing(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_send(im, message)
	Gaim::Conversation::IM im
	const char *message

void
gaim_conv_im_write(im, who, message, flags, mtime)
	Gaim::Conversation::IM im
	const char *who
	const char *message
	Gaim::MessageFlags flags
	time_t mtime

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversation  PREFIX = gaim_conv_
PROTOTYPES: ENABLE

gboolean
gaim_conv_present_error(who, account, what)
	const char *who
	Gaim::Account account
	const char *what

void
gaim_conv_custom_smiley_close(conv, smile)
	Gaim::Conversation conv
	const char *smile

MODULE = Gaim::Conversation  PACKAGE = Gaim::Conversation::Chat  PREFIX = gaim_conv_chat_
PROTOTYPES: ENABLE

Gaim::Conversation
gaim_conv_chat_get_conversation(chat)
	Gaim::Conversation::Chat chat

void
gaim_conv_chat_set_users(chat, users)
	Gaim::Conversation::Chat chat
	SV * users
PREINIT:
	GList *l, *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(users));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(users), i, 0), t_sl));
	}

	for (l = gaim_conv_chat_set_users(chat, t_GL); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

void
gaim_conv_chat_get_users(chat)
	Gaim::Conversation::Chat chat
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_conv_chat_get_users(chat); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

void
gaim_conv_chat_ignore(chat, name)
	Gaim::Conversation::Chat chat
	const char *name

void
gaim_conv_chat_unignore(chat, name)
	Gaim::Conversation::Chat chat
	const char *name

void
gaim_conv_chat_set_ignored(chat, ignored)
	Gaim::Conversation::Chat chat
	SV * ignored
PREINIT:
	GList *l, *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(ignored));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(ignored), i, 0), t_sl));
	}

	for (l = gaim_conv_chat_set_ignored(chat, t_GL); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

void
gaim_conv_chat_get_ignored(chat)
	Gaim::Conversation::Chat chat
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_conv_chat_get_ignored(chat); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

const char *
gaim_conv_chat_get_topic(chat)
	Gaim::Conversation::Chat chat

void
gaim_conv_chat_set_id(chat, id)
	Gaim::Conversation::Chat chat
	int id

int
gaim_conv_chat_get_id(chat)
	Gaim::Conversation::Chat chat

void
gaim_conv_chat_send(chat, message)
	Gaim::Conversation::Chat chat
	const char * message

void
gaim_conv_chat_write(chat, who, message, flags, mtime)
	Gaim::Conversation::Chat chat
	const char *who
	const char *message
	Gaim::MessageFlags flags
	time_t mtime

void
gaim_conv_chat_add_users(chat, users, extra_msgs, flags, new_arrivals)
	Gaim::Conversation::Chat chat
	SV * users
	SV * extra_msgs
	SV * flags
	gboolean new_arrivals
PREINIT:
	GList *t_GL_users, *t_GL_extra_msgs, *t_GL_flags;
	int i, t_len;
PPCODE:
	t_GL_users = NULL;
	t_len = av_len((AV *)SvRV(users));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL_users = g_list_append(t_GL_users, SvPV(*av_fetch((AV *)SvRV(users), i, 0), t_sl));
	}

	t_GL_flags = NULL;
	t_len = av_len((AV *)SvRV(flags));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL_flags = g_list_append(t_GL_flags, SvPV(*av_fetch((AV *)SvRV(flags), i, 0), t_sl));
	}

	t_GL_extra_msgs = NULL;
	t_len = av_len((AV *)SvRV(extra_msgs));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL_extra_msgs = g_list_append(t_GL_extra_msgs, SvPV(*av_fetch((AV *)SvRV(extra_msgs), i, 0), t_sl));
	}

	gaim_conv_chat_add_users(chat, t_GL_users, t_GL_extra_msgs, t_GL_flags, new_arrivals);

gboolean
gaim_conv_chat_find_user(chat, user)
	Gaim::Conversation::Chat chat
	const char * user

void gaim_conv_chat_clear_users(chat)
	Gaim::Conversation::Chat chat

void gaim_conv_chat_set_nick(chat, nick)
	Gaim::Conversation::Chat chat
	const char * nick

const char *
gaim_conv_chat_get_nick(chat)
	Gaim::Conversation::Chat chat

Gaim::Conversation
gaim_find_chat(gc, id)
	Gaim::Connection gc
	int id

void gaim_conv_chat_left(chat)
	Gaim::Conversation::Chat chat

gboolean gaim_conv_chat_has_left(chat)
	Gaim::Conversation::Chat chat

Gaim::Conversation::ChatBuddy
gaim_conv_chat_cb_find(chat, name)
	Gaim::Conversation::Chat chat
	const char *name

const char *
gaim_conv_chat_cb_get_name(cb)
	Gaim::Conversation::ChatBuddy cb

void
gaim_conv_chat_cb_destroy(cb);
	Gaim::Conversation::ChatBuddy cb
