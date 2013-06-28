#include "module.h"

MODULE = Purple::Conversation  PACKAGE = Purple  PREFIX = purple_
PROTOTYPES: ENABLE

BOOT:
{
	HV *update_stash = gv_stashpv("Purple::Conversation::UpdateType", 1);
	HV *typing_stash = gv_stashpv("Purple::IMTypingState", 1);
	HV *flags_stash = gv_stashpv("Purple::MessageFlags", 1);
	HV *cbflags_stash = gv_stashpv("Purple::ChatUser::Flags", 1);

	static const constiv *civ, update_const_iv[] = {
#undef const_iv
#define const_iv(name) {#name, (IV)PURPLE_CONVERSATION_UPDATE_##name}
		const_iv(ADD),
		const_iv(REMOVE),
		const_iv(ACCOUNT),
		const_iv(TYPING),
		const_iv(UNSEEN),
		const_iv(LOGGING),
		const_iv(TOPIC),
/*
		const_iv(ONLINE),
		const_iv(OFFLINE),
*/
		const_iv(AWAY),
		const_iv(ICON),
		const_iv(TITLE),
		const_iv(CHATLEFT),
		const_iv(FEATURES),
	};
	static const constiv typing_const_iv[] = {
#undef const_iv
#define const_iv(name) {#name, (IV)PURPLE_IM_##name}
		const_iv(NOT_TYPING),
		const_iv(TYPING),
		const_iv(TYPED),
	};
	static const constiv flags_const_iv[] = {
#undef const_iv
#define const_iv(name) {#name, (IV)PURPLE_MESSAGE_##name}
		const_iv(SEND),
		const_iv(RECV),
		const_iv(SYSTEM),
		const_iv(AUTO_RESP),
		const_iv(ACTIVE_ONLY),
		const_iv(NICK),
		const_iv(NO_LOG),
		const_iv(WHISPER),
		const_iv(ERROR),
		const_iv(DELAYED),
		const_iv(RAW),
		const_iv(IMAGES),
		const_iv(NOTIFY),
		const_iv(NO_LINKIFY),
	};
	static const constiv cbflags_const_iv[] = {
#undef const_iv
#define const_iv(name) {#name, (IV)PURPLE_CHAT_USER_##name}
		const_iv(NONE),
		const_iv(VOICE),
		const_iv(HALFOP),
		const_iv(OP),
		const_iv(FOUNDER),
		const_iv(TYPING),
	};

	for (civ = update_const_iv + sizeof(update_const_iv) / sizeof(update_const_iv[0]); civ-- > update_const_iv; )
		newCONSTSUB(update_stash, (char *)civ->name, newSViv(civ->iv));

	for (civ = typing_const_iv + sizeof(typing_const_iv) / sizeof(typing_const_iv[0]); civ-- > typing_const_iv; )
		newCONSTSUB(typing_stash, (char *)civ->name, newSViv(civ->iv));

	for (civ = flags_const_iv + sizeof(flags_const_iv) / sizeof(flags_const_iv[0]); civ-- > flags_const_iv; )
		newCONSTSUB(flags_stash, (char *)civ->name, newSViv(civ->iv));

	for (civ = cbflags_const_iv + sizeof(cbflags_const_iv) / sizeof(cbflags_const_iv[0]); civ-- > cbflags_const_iv; )
		newCONSTSUB(cbflags_stash, (char *)civ->name, newSViv(civ->iv));
}

MODULE = Purple::Conversation  PACKAGE = Purple::Conversations  PREFIX = purple_conversations_
PROTOTYPES: ENABLE

void
purple_conversations_add(conv)
    Purple::Conversation conv

void
purple_conversations_remove(conv)
    Purple::Conversation conv

void
purple_conversations_update_cache(conv, name, account)
	Purple::Conversation conv
	const char * name
	Purple::Account account

Purple::Handle
purple_conversations_get_handle()

Purple::ChatConversation
purple_conversations_find_chat(gc, id)
	Purple::Connection gc
	int id

void
purple_conversations_get_ims()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_conversations_get_ims(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::IMConversation")));
	}

void
purple_conversations_get_all()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Conversation")));
	}

void
purple_conversations_get_chats()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_conversations_get_chats(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ChatConversation")));
	}

Purple::Conversation
purple_conversations_find_with_account(name, account)
	const char *name
	Purple::Account account

Purple::ChatConversation
purple_conversations_find_chat_with_account(name, account)
	const char *name
	Purple::Account account

Purple::IMConversation
purple_conversations_find_im_with_account(name, account)
	const char *name
	Purple::Account account

MODULE = Purple::Conversation  PACKAGE = Purple::Conversation  PREFIX = purple_conversation_
PROTOTYPES: ENABLE

Purple::Account
purple_conversation_get_account(conv)
	Purple::Conversation conv

Purple::Connection
purple_conversation_get_connection(conv)
	Purple::Conversation conv

void
purple_conversation_set_title(conv, title);
	Purple::Conversation conv
	const char * title

const char *
purple_conversation_get_title(conv)
	Purple::Conversation conv

void
purple_conversation_autoset_title(conv)
	Purple::Conversation conv

void
purple_conversation_set_name(conv, name)
	Purple::Conversation conv
	const char *name

const char *
purple_conversation_get_name(conv)
	Purple::Conversation conv

void
purple_conversation_set_logging(conv, log)
	Purple::Conversation conv
	gboolean log

gboolean
purple_conversation_is_logging(conv)
	Purple::Conversation conv

gpointer
purple_conversation_get_data(conv, key)
	Purple::Conversation conv
	const char * key

Purple::ConnectionFlags
purple_conversation_get_features(conv)
	Purple::Conversation conv

gboolean
purple_conversation_has_focus(conv)
	Purple::Conversation conv

void
purple_conversation_update(conv, type)
	Purple::Conversation conv
	Purple::Conversation::UpdateType type

void
purple_conversation_set_account(conv, account);
	Purple::Conversation conv
	Purple::Account account

void
purple_conversation_write(conv, who, message, flags, mtime)
	Purple::Conversation conv
	const char *who
	const char *message
	Purple::MessageFlags flags
	time_t mtime

void
purple_conversation_write_message(conv, who, message, flags, mtime)
	Purple::Conversation conv
	const char *who
	const char *message
	Purple::MessageFlags flags
	time_t mtime

void
purple_conversation_send(conv, message)
	Purple::Conversation conv
	const char *message

void
purple_conversation_send_with_flags(conv, message, flags)
	Purple::Conversation conv
	const char *message
	Purple::MessageFlags flags

gboolean
purple_conversation_do_command(conv, cmdline, markup, error)
	Purple::Conversation conv
	const char *cmdline
	const char *markup
	char **error

MODULE = Purple::Conversation  PACKAGE = Purple::IMConversation  PREFIX = purple_im_conversation_
PROTOTYPES: ENABLE

Purple::IMConversation
purple_im_conversation_new(class, account, name)
	Purple::Account account
	const char *name
    C_ARGS:
	account, name

void
purple_im_conversation_set_icon(im, icon)
	Purple::IMConversation im
	Purple::Buddy::Icon icon

Purple::Buddy::Icon
purple_im_conversation_get_icon(im)
	Purple::IMConversation im

void
purple_im_conversation_set_typing_state(im, state)
	Purple::IMConversation im
	Purple::IMTypingState state

Purple::IMTypingState
purple_im_conversation_get_typing_state(im)
	Purple::IMConversation im

void
purple_im_conversation_start_typing_timeout(im, timeout)
	Purple::IMConversation im
	int timeout

void
purple_im_conversation_stop_typing_timeout(im)
	Purple::IMConversation im

guint
purple_im_conversation_get_typing_timeout(im)
	Purple::IMConversation im

void
purple_im_conversation_set_type_again(im, val)
	Purple::IMConversation im
	time_t val

time_t
purple_im_conversation_get_type_again(im)
	Purple::IMConversation im

void
purple_im_conversation_start_send_typed_timeout(im)
	Purple::IMConversation im

void
purple_im_conversation_stop_send_typed_timeout(im)
	Purple::IMConversation im

guint
purple_im_conversation_get_send_typed_timeout(im)
	Purple::IMConversation im

void
purple_im_conversation_update_typing(im)
	Purple::IMConversation im

MODULE = Purple::Conversation  PACKAGE = Purple::Conversation::Helper  PREFIX = purple_conversation_helper_
PROTOTYPES: ENABLE

gboolean
purple_conversation_helper_present_error(who, account, what)
	const char *who
	Purple::Account account
	const char *what

MODULE = Purple::Conversation  PACKAGE = Purple::Conversation  PREFIX = purple_conversation_
PROTOTYPES: ENABLE

void
purple_conversation_custom_smiley_close(conv, smile)
	Purple::Conversation conv
	const char *smile

MODULE = Purple::Conversation  PACKAGE = Purple::ChatConversation  PREFIX = purple_chat_conversation_
PROTOTYPES: ENABLE

Purple::ChatConversation
purple_chat_conversation_new(class, account, name)
	Purple::Account account
	const char *name
    C_ARGS:
	account, name

void
purple_chat_conversation_get_users(chat)
	Purple::ChatConversation chat
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_chat_conversation_get_users(chat); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

void
purple_chat_conversation_ignore(chat, name)
	Purple::ChatConversation chat
	const char *name

void
purple_chat_conversation_unignore(chat, name)
	Purple::ChatConversation chat
	const char *name

void
purple_chat_conversation_set_ignored(chat, ignored)
	Purple::ChatConversation chat
	SV * ignored
PREINIT:
	GList *l, *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(ignored));

	for (i = 0; i <= t_len; i++)
		t_GL = g_list_append(t_GL, SvPVutf8_nolen(*av_fetch((AV *)SvRV(ignored), i, 0)));

	for (l = purple_chat_conversation_set_ignored(chat, t_GL); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

void
purple_chat_conversation_get_ignored(chat)
	Purple::ChatConversation chat
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_chat_conversation_get_ignored(chat); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

const char *
purple_chat_conversation_get_topic(chat)
	Purple::ChatConversation chat

void
purple_chat_conversation_set_id(chat, id)
	Purple::ChatConversation chat
	int id

int
purple_chat_conversation_get_id(chat)
	Purple::ChatConversation chat

void
purple_chat_conversation_add_users(chat, users, extra_msgs, flags, new_arrivals)
	Purple::ChatConversation chat
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

	for (i = 0; i <= t_len; i++)
		t_GL_users = g_list_append(t_GL_users, SvPVutf8_nolen(*av_fetch((AV *)SvRV(users), i, 0)));

	t_GL_flags = NULL;
	t_len = av_len((AV *)SvRV(flags));

	for (i = 0; i <= t_len; i++)
		t_GL_flags = g_list_append(t_GL_flags, SvPVutf8_nolen(*av_fetch((AV *)SvRV(flags), i, 0)));

	t_GL_extra_msgs = NULL;
	t_len = av_len((AV *)SvRV(extra_msgs));

	for (i = 0; i <= t_len; i++)
		t_GL_extra_msgs = g_list_append(t_GL_extra_msgs, SvPVutf8_nolen(*av_fetch((AV *)SvRV(extra_msgs), i, 0)));

	purple_chat_conversation_add_users(chat, t_GL_users, t_GL_extra_msgs, t_GL_flags, new_arrivals);

	g_list_free(t_GL_users);
	g_list_free(t_GL_extra_msgs);
	g_list_free(t_GL_flags);

gboolean
purple_chat_conversation_has_user(chat, user)
	Purple::ChatConversation chat
	const char * user

void purple_chat_conversation_clear_users(chat)
	Purple::ChatConversation chat

void purple_chat_conversation_set_nick(chat, nick)
	Purple::ChatConversation chat
	const char * nick

const char *
purple_chat_conversation_get_nick(chat)
	Purple::ChatConversation chat

void purple_chat_conversation_leave(chat)
	Purple::ChatConversation chat

gboolean purple_chat_conversation_has_left(chat)
	Purple::ChatConversation chat

Purple::ChatUser
purple_chat_conversation_find_buddy(chat, name)
	Purple::ChatConversation chat
	const char *name

const char *
purple_chat_user_get_name(cb)
	Purple::ChatUser cb
