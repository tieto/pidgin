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
 * Protocol Class API
 **************************************************************************/

const char *
purple_protocol_get_id(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->id;
}

const char *
purple_protocol_get_name(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->name;
}

PurpleProtocolOptions
purple_protocol_get_options(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), 0);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->options;
}

GList *
purple_protocol_get_user_splits(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->user_splits;
}

GList *
purple_protocol_get_protocol_options(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->protocol_options;
}

PurpleBuddyIconSpec *
purple_protocol_get_icon_spec(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->icon_spec;
}

PurpleWhiteboardPrplOps *
purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol)
{
	PurpleProtocolClass *klass;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);
	return klass->whiteboard_protocol_ops;
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

	purple_request_close_with_handle(protocol);
	purple_notify_close_with_handle(protocol);

	purple_signals_disconnect_by_handle(protocol);
	purple_signals_unregister_by_instance(protocol);

	purple_prefs_disconnect_by_handle(protocol);

	PURPLE_DBUS_UNREGISTER_POINTER(protocol);

	parent_class->dispose(object);
}

static void
purple_protocol_class_init(PurpleProtocolClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_protocol_dispose;
}

static void
purple_protocol_base_finalize(PurpleProtocolClass *klass)
{
	GList *accounts, *l;

	accounts = purple_accounts_get_all_active();
	for (l = accounts; l != NULL; l = l->next) {
		PurpleAccount *account = PURPLE_ACCOUNT(l->data);
		if (purple_account_is_disconnected(account))
			continue;

		if (purple_strequal(klass->id, purple_account_get_protocol_id(account)))
			purple_account_disconnect(account);
	}

	g_list_free(accounts);

	while (klass->user_splits) {
		PurpleAccountUserSplit *split = klass->user_splits->data;
		purple_account_user_split_destroy(split);
		klass->user_splits = g_list_delete_link(klass->user_splits,
				klass->user_splits);
	}

	while (klass->protocol_options) {
		PurpleAccountOption *option = klass->protocol_options->data;
		purple_account_option_destroy(option);
		klass->protocol_options = g_list_delete_link(klass->protocol_options,
				klass->protocol_options);
	}

	purple_buddy_icon_spec_destroy(klass->icon_spec);
}

GType
purple_protocol_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.instance_size = sizeof(PurpleProtocol),
			.instance_init = (GInstanceInitFunc)purple_protocol_init,
			.class_size = sizeof(PurpleProtocolClass),
			.class_init = (GClassInitFunc)purple_protocol_class_init,
			.base_finalize = (GBaseFinalizeFunc)purple_protocol_base_finalize,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleProtocol",
		                              &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

GType
purple_protocol_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolInterface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
		                              "PurpleProtocolInterface", &info, 0);
	}

	return type;
}

/**************************************************************************
 * Protocol Interface API
 **************************************************************************/

#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolInterface *iface = PURPLE_PROTOCOL_GET_INTERFACE(protocol); \
	g_return_if_fail(iface != NULL); \
	if (iface->funcname) \
		iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolInterface *iface = PURPLE_PROTOCOL_GET_INTERFACE(protocol); \
	g_return_val_if_fail(iface != NULL, defaultreturn); \
	if (iface->funcname) \
		return iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GList *
purple_protocol_iface_get_actions(PurpleProtocol *protocol,
                                  PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_actions, gc);
}

const char *
purple_protocol_iface_list_icon(PurpleProtocol *protocol,
                                PurpleAccount *account, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, list_icon, account, buddy);
}

const char *
purple_protocol_iface_list_emblem(PurpleProtocol *protocol, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, list_emblem, buddy);
}

char *
purple_protocol_iface_status_text(PurpleProtocol *protocol, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, status_text, buddy);
}

void
purple_protocol_iface_tooltip_text(PurpleProtocol *protocol, PurpleBuddy *buddy,
                                   PurpleNotifyUserInfo *user_info,
                                   gboolean full)
{
	DEFINE_PROTOCOL_FUNC(protocol, tooltip_text, buddy, user_info, full);
}

GList *
purple_protocol_iface_status_types(PurpleProtocol *protocol,
                                   PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, status_types, account);
}

GList *
purple_protocol_iface_blist_node_menu(PurpleProtocol *protocol,
                                      PurpleBlistNode *node)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, blist_node_menu, node);
}

GList *
purple_protocol_iface_chat_info(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, chat_info, gc);
}

GHashTable *
purple_protocol_iface_chat_info_defaults(PurpleProtocol *protocol,
                                         PurpleConnection *gc,
                                         const char *chat_name)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, chat_info_defaults, gc,
	                                 chat_name);
}

void
purple_protocol_iface_login(PurpleProtocol *protocol, PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, login, account);
}

void
purple_protocol_iface_close(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, close, gc);
}

int 
purple_protocol_iface_send_im(PurpleProtocol *protocol, PurpleConnection *gc, 
                              const char *who, const char *message,
                              PurpleMessageFlags flags)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_im, gc, who, message,
	                                 flags);
}

void
purple_protocol_iface_set_info(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *info)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_info, gc, info);
}

unsigned int
purple_protocol_iface_send_typing(PurpleProtocol *protocol,
                                  PurpleConnection *gc, const char *name,
                                  PurpleIMTypingState state)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_typing, gc, name, state);
}

void
purple_protocol_iface_get_info(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_info, gc, who);
}

void
purple_protocol_iface_set_status(PurpleProtocol *protocol,
                                 PurpleAccount *account, PurpleStatus *status)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_status, account, status);
}

void
purple_protocol_iface_set_idle(PurpleProtocol *protocol, PurpleConnection *gc,
                               int idletime)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_idle, gc, idletime);
}

void
purple_protocol_iface_change_passwd(PurpleProtocol *protocol,
                                    PurpleConnection *gc, const char *old_pass,
                                    const char *new_pass)
{
	DEFINE_PROTOCOL_FUNC(protocol, change_passwd, gc, old_pass, new_pass);
}

void
purple_protocol_iface_add_buddy(PurpleProtocol *protocol, PurpleConnection *gc,
                                PurpleBuddy *buddy, PurpleGroup *group,
                                const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddy, gc, buddy, group, message);
}

void
purple_protocol_iface_add_buddies(PurpleProtocol *protocol,
                                  PurpleConnection *gc, GList *buddies,
                                  GList *groups, const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddies, gc, buddies, groups, message);
}

void
purple_protocol_iface_remove_buddy(PurpleProtocol *protocol,
                                   PurpleConnection *gc, PurpleBuddy *buddy,
                                   PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddy, gc, buddy, group);
}

void
purple_protocol_iface_remove_buddies(PurpleProtocol *protocol,
                                     PurpleConnection *gc, GList *buddies,
                                     GList *groups)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddies, gc, buddies, groups);
}

void
purple_protocol_iface_add_permit(PurpleProtocol *protocol, PurpleConnection *gc,
                                 const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_permit, gc, name);
}

void
purple_protocol_iface_add_deny(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_deny, gc, name);
}

void
purple_protocol_iface_rem_permit(PurpleProtocol *protocol, PurpleConnection *gc,
                                 const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, rem_permit, gc, name);
}

void
purple_protocol_iface_rem_deny(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *name)
{
	DEFINE_PROTOCOL_FUNC(protocol, rem_deny, gc, name);
}

void
purple_protocol_iface_set_permit_deny(PurpleProtocol *protocol,
                                      PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_permit_deny, gc);
}

void
purple_protocol_iface_join_chat(PurpleProtocol *protocol, PurpleConnection *gc,
                                GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC(protocol, join_chat, gc, components);
}

void
purple_protocol_iface_reject_chat(PurpleProtocol *protocol,
                                  PurpleConnection *gc, GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC(protocol, reject_chat, gc, components);
}

char *
purple_protocol_iface_get_chat_name(PurpleProtocol *protocol,
                                    GHashTable *components)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_chat_name, components);
}

void
purple_protocol_iface_chat_invite(PurpleProtocol *protocol,
                                  PurpleConnection *gc, int id,
                                  const char *message, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, chat_invite, gc, id, message, who);
}

void
purple_protocol_iface_chat_leave(PurpleProtocol *protocol, PurpleConnection *gc,
                                 int id)
{
	DEFINE_PROTOCOL_FUNC(protocol, chat_leave, gc, id);
}

void
purple_protocol_iface_chat_whisper(PurpleProtocol *protocol,
                                   PurpleConnection *gc, int id,
                                   const char *who, const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, chat_whisper, gc, id, who, message);
}

int 
purple_protocol_iface_chat_send(PurpleProtocol *protocol, PurpleConnection *gc,
                                int id, const char *message,
                                PurpleMessageFlags flags)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, chat_send, gc, id, message,
	                                 flags);
}

void
purple_protocol_iface_keepalive(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, keepalive, gc);
}

void
purple_protocol_iface_register_user(PurpleProtocol *protocol,
                                    PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, register_user, account);
}

void
purple_protocol_iface_unregister_user(PurpleProtocol *protocol,
                                      PurpleAccount *account,
                                      PurpleAccountUnregistrationCb cb,
                                      void *user_data)
{
	DEFINE_PROTOCOL_FUNC(protocol, unregister_user, account, cb, user_data);
}

void
purple_protocol_iface_alias_buddy(PurpleProtocol *protocol,
                                  PurpleConnection *gc, const char *who,
                                  const char *alias)
{
	DEFINE_PROTOCOL_FUNC(protocol, alias_buddy, gc, who, alias);
}

void
purple_protocol_iface_group_buddy(PurpleProtocol *protocol,
                                  PurpleConnection *gc, const char *who,
                                  const char *old_group, const char *new_group)
{
	DEFINE_PROTOCOL_FUNC(protocol, group_buddy, gc, who, old_group, new_group);
}

void
purple_protocol_iface_rename_group(PurpleProtocol *protocol,
                                   PurpleConnection *gc, const char *old_name,
                                   PurpleGroup *group, GList *moved_buddies)
{
	DEFINE_PROTOCOL_FUNC(protocol, rename_group, gc, old_name, group,
	                     moved_buddies);
}

void
purple_protocol_iface_buddy_free(PurpleProtocol *protocol, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC(protocol, buddy_free, buddy);
}

void
purple_protocol_iface_convo_closed(PurpleProtocol *protocol,
                                   PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, convo_closed, gc, who);
}

const char *
purple_protocol_iface_normalize(PurpleProtocol *protocol,
                                const PurpleAccount *account, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, normalize, account, who);
}

void
purple_protocol_iface_set_buddy_icon(PurpleProtocol *protocol,
                                     PurpleConnection *gc,
                                     PurpleStoredImage *img)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_buddy_icon, gc, img);
}

void
purple_protocol_iface_remove_group(PurpleProtocol *protocol,
                                   PurpleConnection *gc, PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_group, gc, group);
}

char *
purple_protocol_iface_get_cb_real_name(PurpleProtocol *protocol,
                                       PurpleConnection *gc, int id,
                                       const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_cb_real_name, gc, id,
	                                 who);
}

void
purple_protocol_iface_set_chat_topic(PurpleProtocol *protocol,
                                     PurpleConnection *gc, int id,
                                     const char *topic)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_chat_topic, gc, id, topic);
}

PurpleChat *
purple_protocol_iface_find_blist_chat(PurpleProtocol *protocol,
                                      PurpleAccount *account, const char *name)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, find_blist_chat, account,
	                                 name);
}

PurpleRoomlist *
purple_protocol_iface_roomlist_get_list(PurpleProtocol *protocol,
                                        PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, roomlist_get_list, gc);
}

void
purple_protocol_iface_roomlist_cancel(PurpleProtocol *protocol,
                                      PurpleRoomlist *list)
{
	DEFINE_PROTOCOL_FUNC(protocol, roomlist_cancel, list);
}

void
purple_protocol_iface_roomlist_expand_category(PurpleProtocol *protocol,
                                               PurpleRoomlist *list,
                                               PurpleRoomlistRoom *category)
{
	DEFINE_PROTOCOL_FUNC(protocol, roomlist_expand_category, list, category);
}

gboolean
purple_protocol_iface_can_receive_file(PurpleProtocol *protocol,
                                       PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, can_receive_file, gc,
	                                 who);
}

void
purple_protocol_iface_send_file(PurpleProtocol *protocol, PurpleConnection *gc,
                                const char *who, const char *filename)
{
	DEFINE_PROTOCOL_FUNC(protocol, send_file, gc, who, filename);
}

PurpleXfer *
purple_protocol_iface_new_xfer(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, new_xfer, gc, who);
}

gboolean
purple_protocol_iface_offline_message(PurpleProtocol *protocol,
                                      const PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, offline_message, buddy);
}

int
purple_protocol_iface_send_raw(PurpleProtocol *protocol, PurpleConnection *gc,
                               const char *buf, int len)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_raw, gc, buf, len);
}

char *
purple_protocol_iface_roomlist_room_serialize(PurpleProtocol *protocol,
                                              PurpleRoomlistRoom *room)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, roomlist_room_serialize,
	                                 room);
}

gboolean
purple_protocol_iface_send_attention(PurpleProtocol *protocol,
                                     PurpleConnection *gc, const char *username,
                                     guint type)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, send_attention, gc,
	                                 username, type);
}

GList *
purple_protocol_iface_get_attention_types(PurpleProtocol *protocol,
                                          PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_attention_types,
	                                 account);
}

GHashTable *
purple_protocol_iface_get_account_text_table(PurpleProtocol *protocol,
                                             PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_account_text_table,
	                                 account);
}

gboolean
purple_protocol_iface_initiate_media(PurpleProtocol *protocol,
                                     PurpleAccount *account, const char *who,
                                     PurpleMediaSessionType type)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, FALSE, initiate_media, account,
	                                 who, type);
}

PurpleMediaCaps
purple_protocol_iface_get_media_caps(PurpleProtocol *protocol,
                                     PurpleAccount *account, const char *who)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, get_media_caps, account, who);
}

PurpleMood *
purple_protocol_iface_get_moods(PurpleProtocol *protocol,
                                PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, get_moods, account);
}

void
purple_protocol_iface_set_public_alias(PurpleProtocol *protocol,
                                PurpleConnection *gc, const char *alias,
                                PurpleSetPublicAliasSuccessCallback success_cb,
                                PurpleSetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_public_alias, gc, alias, success_cb,
	                     failure_cb);
}

void
purple_protocol_iface_get_public_alias(PurpleProtocol *protocol,
                                PurpleConnection *gc,
                                PurpleGetPublicAliasSuccessCallback success_cb,
                                PurpleGetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_public_alias, gc, success_cb,
	                     failure_cb);
}

gsize
purple_protocol_iface_get_max_message_size(PurpleProtocol *protocol,
                                           PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, get_max_message_size, gc);
}

