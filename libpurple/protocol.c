/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "dbus-maybe.h"
#include "protocol.h"

static GObjectClass *parent_class;

/**************************************************************************
 * Protocol Object API
 **************************************************************************/
const char *
purple_protocol_get_id(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->id;
}

const char *
purple_protocol_get_name(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->name;
}

PurpleProtocolOptions
purple_protocol_get_options(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), 0);

	return protocol->options;
}

GList *
purple_protocol_get_user_splits(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->user_splits;
}

GList *
purple_protocol_get_protocol_options(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->protocol_options;
}

PurpleBuddyIconSpec *
purple_protocol_get_icon_spec(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->icon_spec;
}

PurpleWhiteboardOps *
purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->whiteboard_ops;
}

static void
user_splits_free(PurpleProtocol *protocol)
{
	g_return_if_fail(PURPLE_IS_PROTOCOL(protocol));

	while (protocol->user_splits) {
		PurpleAccountUserSplit *split = protocol->user_splits->data;
		purple_account_user_split_destroy(split);
		protocol->user_splits = g_list_delete_link(protocol->user_splits,
				protocol->user_splits);
	}
}

static void
protocol_options_free(PurpleProtocol *protocol)
{
	g_return_if_fail(PURPLE_IS_PROTOCOL(protocol));

	while (protocol->protocol_options) {
		PurpleAccountOption *option = protocol->protocol_options->data;
		purple_account_option_destroy(option);
		protocol->protocol_options =
				g_list_delete_link(protocol->protocol_options,
				                   protocol->protocol_options);
	}
}

static void
icon_spec_free(PurpleProtocol *protocol)
{
	g_return_if_fail(PURPLE_IS_PROTOCOL(protocol));

	g_free(protocol->icon_spec);
	protocol->icon_spec = NULL;
}

void
purple_protocol_override(PurpleProtocol *protocol,
                         PurpleProtocolOverrideFlags flags)
{
	g_return_if_fail(PURPLE_IS_PROTOCOL(protocol));

	if (flags & PURPLE_PROTOCOL_OVERRIDE_USER_SPLITS)
		user_splits_free(protocol);
	if (flags & PURPLE_PROTOCOL_OVERRIDE_PROTOCOL_OPTIONS)
		protocol_options_free(protocol);
	if (flags & PURPLE_PROTOCOL_OVERRIDE_ICON_SPEC)
		icon_spec_free(protocol);
}

/**************************************************************************
 * GObject stuff
 **************************************************************************/
static void
purple_protocol_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_PROTOCOL(instance), PurpleProtocol);
}

static void
purple_protocol_dispose(GObject *object)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(object);
	GList *accounts, *l;

	accounts = purple_accounts_get_all_active();
	for (l = accounts; l != NULL; l = l->next) {
		PurpleAccount *account = PURPLE_ACCOUNT(l->data);
		if (purple_account_is_disconnected(account))
			continue;

		if (purple_strequal(protocol->id,
				purple_account_get_protocol_id(account)))
			purple_account_disconnect(account);
	}

	g_list_free(accounts);

	purple_request_close_with_handle(protocol);
	purple_notify_close_with_handle(protocol);

	purple_signals_disconnect_by_handle(protocol);
	purple_signals_unregister_by_instance(protocol);

	purple_prefs_disconnect_by_handle(protocol);

	PURPLE_DBUS_UNREGISTER_POINTER(protocol);

	parent_class->dispose(object);
}

static void
purple_protocol_finalize(GObject *object)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(object);

	user_splits_free(protocol);
	protocol_options_free(protocol);
	icon_spec_free(protocol);

	parent_class->finalize(object);
}

static void
purple_protocol_class_init(PurpleProtocolClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose  = purple_protocol_dispose;
	obj_class->finalize = purple_protocol_finalize;
}

GType
purple_protocol_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolClass),
			.class_init = (GClassInitFunc)purple_protocol_class_init,
			.instance_size = sizeof(PurpleProtocol),
			.instance_init = (GInstanceInitFunc)purple_protocol_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleProtocol",
		                              &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**************************************************************************
 * Protocol Class API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolClass *klass = PURPLE_PROTOCOL_GET_CLASS(protocol); \
	g_return_if_fail(klass != NULL); \
	if (klass->funcname) \
		klass->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolClass *klass = PURPLE_PROTOCOL_GET_CLASS(protocol); \
	g_return_val_if_fail(klass != NULL, defaultreturn); \
	if (klass->funcname) \
		return klass->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

void
purple_protocol_class_login(PurpleProtocol *protocol, PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, login, account);
}

void
purple_protocol_class_close(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, close, gc);
}

GList *
purple_protocol_class_status_types(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, status_types, account);
}

const char *
purple_protocol_class_list_icon(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, list_icon, account, buddy);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Client Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolClientIface *client_iface = \
		PURPLE_PROTOCOL_GET_CLIENT_IFACE(protocol); \
	if (client_iface && client_iface->funcname) \
		client_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolClientIface *client_iface = \
		PURPLE_PROTOCOL_GET_CLIENT_IFACE(protocol); \
	if (client_iface && client_iface->funcname) \
		return client_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_client_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolClientIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolClientIface", &info, 0);
	}
	return type;
}

GList *
purple_protocol_client_iface_get_actions(PurpleProtocol *protocol,
		PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_actions, gc);
}

const char *
purple_protocol_client_iface_list_emblem(PurpleProtocol *protocol,
		PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, list_emblem, buddy);
}

char *
purple_protocol_client_iface_status_text(PurpleProtocol *protocol,
		PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, status_text, buddy);
}

void
purple_protocol_client_iface_tooltip_text(PurpleProtocol *protocol,
		PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	DEFINE_PROTOCOL_FUNC(protocol, tooltip_text, buddy, user_info, full);
}

GList *
purple_protocol_client_iface_blist_node_menu(PurpleProtocol *protocol,
		PurpleBlistNode *node)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, blist_node_menu, node);
}

void
purple_protocol_client_iface_buddy_free(PurpleProtocol *protocol,
		PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC(protocol, buddy_free, buddy);
}

void
purple_protocol_client_iface_convo_closed(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, convo_closed, gc, who);
}

const char *
purple_protocol_client_iface_normalize(PurpleProtocol *protocol,
		const PurpleAccount *account, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, normalize, account, who);
}

PurpleChat *
purple_protocol_client_iface_find_blist_chat(PurpleProtocol *protocol,
		PurpleAccount *account, const char *name)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, find_blist_chat, account,
			name);
}

gboolean
purple_protocol_client_iface_offline_message(PurpleProtocol *protocol,
		const PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, offline_message, buddy);
}

GHashTable *
purple_protocol_client_iface_get_account_text_table(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_account_text_table,
			account);
}

PurpleMood *
purple_protocol_client_iface_get_moods(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_moods, account);
}

gssize
purple_protocol_client_iface_get_max_message_size(PurpleProtocol *protocol,
		PurpleConversation *conv)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, get_max_message_size, conv);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Server Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolServerIface *server_iface = \
		PURPLE_PROTOCOL_GET_SERVER_IFACE(protocol); \
	if (server_iface && server_iface->funcname) \
		server_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolServerIface *server_iface = \
		PURPLE_PROTOCOL_GET_SERVER_IFACE(protocol); \
	if (server_iface && server_iface->funcname) \
		return server_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_server_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolServerIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolServerIface", &info, 0);
	}
	return type;
}

void
purple_protocol_server_iface_register_user(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, register_user, account);
}

void
purple_protocol_server_iface_unregister_user(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleAccountUnregistrationCb cb,
		void *user_data)
{
	DEFINE_PROTOCOL_FUNC(protocol, unregister_user, account, cb, user_data);
}

void
purple_protocol_server_iface_set_info(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *info)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_info, gc, info);
}

void
purple_protocol_server_iface_get_info(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_info, gc, who);
}

void
purple_protocol_server_iface_set_status(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleStatus *status)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_status, account, status);
}

void
purple_protocol_server_iface_set_idle(PurpleProtocol *protocol,
		PurpleConnection *gc, int idletime)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_idle, gc, idletime);
}

void
purple_protocol_server_iface_change_passwd(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *old_pass, const char *new_pass)
{
	DEFINE_PROTOCOL_FUNC(protocol, change_passwd, gc, old_pass, new_pass);
}

void
purple_protocol_server_iface_add_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group,
		const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddy, gc, buddy, group, message);
}

void
purple_protocol_server_iface_add_buddies(PurpleProtocol *protocol,
		PurpleConnection *gc, GList *buddies, GList *groups,
		const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddies, gc, buddies, groups, message);
}

void
purple_protocol_server_iface_remove_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddy, gc, buddy, group);
}

void
purple_protocol_server_iface_remove_buddies(PurpleProtocol *protocol,
		PurpleConnection *gc, GList *buddies, GList *groups)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddies, gc, buddies, groups);
}

void
purple_protocol_server_iface_keepalive(PurpleProtocol *protocol,
		PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, keepalive, gc);
}

void
purple_protocol_server_iface_alias_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who, const char *alias)
{
	DEFINE_PROTOCOL_FUNC(protocol, alias_buddy, gc, who, alias);
}

void
purple_protocol_server_iface_group_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who, const char *old_group,
		const char *new_group)
{
	DEFINE_PROTOCOL_FUNC(protocol, group_buddy, gc, who, old_group, new_group);
}

void
purple_protocol_server_iface_rename_group(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *old_name, PurpleGroup *group,
		GList *moved_buddies)
{
	DEFINE_PROTOCOL_FUNC(protocol, rename_group, gc, old_name, group,
			moved_buddies);
}

void
purple_protocol_server_iface_set_buddy_icon(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleStoredImage *img)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_buddy_icon, gc, img);
}

void
purple_protocol_server_iface_remove_group(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_group, gc, group);
}

int
purple_protocol_server_iface_send_raw(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *buf, int len)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_raw, gc, buf, len);
}

void
purple_protocol_server_iface_set_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *alias,
		PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_public_alias, gc, alias, success_cb,
			failure_cb);
}

void
purple_protocol_server_iface_get_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_public_alias, gc, success_cb,
			failure_cb);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol IM Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolIMIface *im_iface = \
		PURPLE_PROTOCOL_GET_IM_IFACE(protocol); \
	if (im_iface && im_iface->funcname) \
		im_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolIMIface *im_iface = \
		PURPLE_PROTOCOL_GET_IM_IFACE(protocol); \
	if (im_iface && im_iface->funcname) \
		return im_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_im_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolIMIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolIMIface", &info, 0);
	}
	return type;
}

int
purple_protocol_im_iface_send(PurpleProtocol *protocol, PurpleConnection *gc,
		const char *who, const char *message, PurpleMessageFlags flags)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send, gc, who, message,
			flags);
}

unsigned int
purple_protocol_im_iface_send_typing(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *name, PurpleIMTypingState state)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_typing, gc, name, state);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Chat Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolChatIface *chat_iface = \
		PURPLE_PROTOCOL_GET_CHAT_IFACE(protocol); \
	if (chat_iface && chat_iface->funcname) \
		chat_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolChatIface *chat_iface = \
		PURPLE_PROTOCOL_GET_CHAT_IFACE(protocol); \
	if (chat_iface && chat_iface->funcname) \
		return chat_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_chat_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolChatIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolChatIface", &info, 0);
	}
	return type;
}

GList *
purple_protocol_chat_iface_info(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, info, gc);
}

GHashTable *
purple_protocol_chat_iface_info_defaults(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *chat_name)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, info_defaults, gc,
			chat_name);
}

void
purple_protocol_chat_iface_join(PurpleProtocol *protocol, PurpleConnection *gc,
		GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC(protocol, join, gc, components);
}

void
purple_protocol_chat_iface_reject(PurpleProtocol *protocol,
		PurpleConnection *gc, GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC(protocol, reject, gc, components);
}

char *
purple_protocol_chat_iface_get_name(PurpleProtocol *protocol,
		GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_name, components);
}

void
purple_protocol_chat_iface_invite(PurpleProtocol *protocol,
		PurpleConnection *gc, int id, const char *message, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, invite, gc, id, message, who);
}

void
purple_protocol_chat_iface_leave(PurpleProtocol *protocol, PurpleConnection *gc,
		int id)
{
	DEFINE_PROTOCOL_FUNC(protocol, leave, gc, id);
}

void
purple_protocol_chat_iface_whisper(PurpleProtocol *protocol,
		PurpleConnection *gc, int id, const char *who, const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, whisper, gc, id, who, message);
}

int 
purple_protocol_chat_iface_send(PurpleProtocol *protocol, PurpleConnection *gc,
		int id, const char *message, PurpleMessageFlags flags)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send, gc, id, message, flags);
}

char *
purple_protocol_chat_iface_get_user_real_name(PurpleProtocol *protocol,
		PurpleConnection *gc, int id, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_user_real_name, gc, id,
			who);
}

void
purple_protocol_chat_iface_set_topic(PurpleProtocol *protocol,
		PurpleConnection *gc, int id, const char *topic)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_topic, gc, id, topic);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Privacy Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolPrivacyIface *privacy_iface = \
		PURPLE_PROTOCOL_GET_PRIVACY_IFACE(protocol); \
	if (privacy_iface && privacy_iface->funcname) \
		privacy_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolPrivacyIface *privacy_iface = \
		PURPLE_PROTOCOL_GET_PRIVACY_IFACE(protocol); \
	if (privacy_iface && privacy_iface->funcname) \
		return privacy_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_privacy_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolPrivacyIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolPrivacyIface", &info, 0);
	}
	return type;
}

void
purple_protocol_privacy_iface_add_permit(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_permit, gc, name);
}

void
purple_protocol_privacy_iface_add_deny(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_deny, gc, name);
}

void
purple_protocol_privacy_iface_rem_permit(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, rem_permit, gc, name);
}

void
purple_protocol_privacy_iface_rem_deny(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, rem_deny, gc, name);
}

void
purple_protocol_privacy_iface_set_permit_deny(PurpleProtocol *protocol,
		PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_permit_deny, gc);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Xfer Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolXferIface *xfer_iface = \
		PURPLE_PROTOCOL_GET_XFER_IFACE(protocol); \
	if (xfer_iface && xfer_iface->funcname) \
		xfer_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolXferIface *xfer_iface = \
		PURPLE_PROTOCOL_GET_XFER_IFACE(protocol); \
	if (xfer_iface && xfer_iface->funcname) \
		return xfer_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_xfer_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolXferIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolXferIface", &info, 0);
	}
	return type;
}

gboolean
purple_protocol_xfer_iface_can_receive(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, can_receive, gc, who);
}

void
purple_protocol_xfer_iface_send(PurpleProtocol *protocol, PurpleConnection *gc,
		const char *who, const char *filename)
{
	DEFINE_PROTOCOL_FUNC(protocol, send, gc, who, filename);
}

PurpleXfer *
purple_protocol_xfer_iface_new_xfer(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, new_xfer, gc, who);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Roomlist Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolRoomlistIface *roomlist_iface = \
		PURPLE_PROTOCOL_GET_ROOMLIST_IFACE(protocol); \
	if (roomlist_iface && roomlist_iface->funcname) \
		roomlist_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolRoomlistIface *roomlist_iface = \
		PURPLE_PROTOCOL_GET_ROOMLIST_IFACE(protocol); \
	if (roomlist_iface && roomlist_iface->funcname) \
		return roomlist_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_roomlist_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolRoomlistIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolRoomlistIface", &info, 0);
	}
	return type;
}

PurpleRoomlist *
purple_protocol_roomlist_iface_get_list(PurpleProtocol *protocol,
		PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_list, gc);
}

void
purple_protocol_roomlist_iface_cancel(PurpleProtocol *protocol,
		PurpleRoomlist *list)
{
	DEFINE_PROTOCOL_FUNC(protocol, cancel, list);
}

void
purple_protocol_roomlist_iface_expand_category(PurpleProtocol *protocol,
		PurpleRoomlist *list, PurpleRoomlistRoom *category)
{
	DEFINE_PROTOCOL_FUNC(protocol, expand_category, list, category);
}

char *
purple_protocol_roomlist_iface_room_serialize(PurpleProtocol *protocol,
		PurpleRoomlistRoom *room)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, room_serialize, room);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Attention Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolAttentionIface *attention_iface = \
		PURPLE_PROTOCOL_GET_ATTENTION_IFACE(protocol); \
	if (attention_iface && attention_iface->funcname) \
		attention_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolAttentionIface *attention_iface = \
		PURPLE_PROTOCOL_GET_ATTENTION_IFACE(protocol); \
	if (attention_iface && attention_iface->funcname) \
		return attention_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_attention_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolAttentionIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolAttentionIface", &info, 0);
	}
	return type;
}

gboolean
purple_protocol_attention_iface_send(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *username, guint type)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, send, gc, username, type);
}

GList *
purple_protocol_attention_iface_get_types(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_types, account);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Media Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolMediaIface *media_iface = \
		PURPLE_PROTOCOL_GET_MEDIA_IFACE(protocol); \
	if (media_iface && media_iface->funcname) \
		media_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolMediaIface *media_iface = \
		PURPLE_PROTOCOL_GET_MEDIA_IFACE(protocol); \
	if (media_iface && media_iface->funcname) \
		return media_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_media_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolMediaIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolMediaIface", &info, 0);
	}
	return type;
}

gboolean
purple_protocol_media_iface_initiate_session(PurpleProtocol *protocol,
		PurpleAccount *account, const char *who, PurpleMediaSessionType type)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, initiate_session, account,
			who, type);
}

PurpleMediaCaps
purple_protocol_media_iface_get_caps(PurpleProtocol *protocol,
		PurpleAccount *account, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, get_caps, account, who);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Factory Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolFactoryIface *factory_iface = \
		PURPLE_PROTOCOL_GET_FACTORY_IFACE(protocol); \
	if (factory_iface && factory_iface->funcname) \
		factory_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolFactoryIface *factory_iface = \
		PURPLE_PROTOCOL_GET_FACTORY_IFACE(protocol); \
	if (factory_iface && factory_iface->funcname) \
		return factory_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_factory_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolFactoryIface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolFactoryIface", &info, 0);
	}
	return type;
}

PurpleConnection *
purple_protocol_factory_iface_connection_new(PurpleProtocol *protocol,
		PurpleAccount *account, const char *password)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, connection_new, protocol,
			account, password);
}

PurpleRoomlist *
purple_protocol_factory_iface_roomlist_new(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, roomlist_new, account);
}

PurpleWhiteboard *
purple_protocol_factory_iface_whiteboard_new(PurpleProtocol *protocol,
		PurpleAccount *account, const char *who, int state)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, whiteboard_new, account,
			who, state);
}

PurpleXfer *
purple_protocol_factory_iface_xfer_new(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleXferType type, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, xfer_new, account, type,
			who);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC
