/**
 * @file gaim-compat.h Gaim Compat macros
 * @ingroup core
 *
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * @see @ref account-signals
 */
#ifndef _GAIM_COMPAT_H_
#define _GAIM_COMPAT_H_

/* from account.h */
#define GaimAccountUiOps PurpleAccountUiOps
#define GaimAccount PurpleAccount

#define GaimFilterAccountFunc PurpleFilterAccountFunc
#define GaimAccountRequestAuthorizationCb PurpleAccountRequestAuthorizationCb

#define gaim_account_new           purple_account_new
#define gaim_account_destroy       purple_account_destroy
#define gaim_account_connect       purple_account_connect
#define gaim_account_register      purple_account_register
#define gaim_account_disconnect    purple_account_disconnect
#define gaim_account_notify_added  purple_account_notify_added
#define gaim_account_request_add   purple_account_request_add

#define gaim_account_request_authorization     purple_account_request_authorization
#define gaim_account_request_change_password   purple_account_request_change_password
#define gaim_account_request_change_user_info  purple_account_request_change_user_info

#define gaim_account_set_username            purple_account_set_username
#define gaim_account_set_password            purple_account_set_password
#define gaim_account_set_alias               purple_account_set_alias
#define gaim_account_set_user_info           purple_account_set_user_info
#define gaim_account_set_buddy_icon          purple_account_set_buddy_icon
#define gaim_account_set_buddy_icon_path     purple_account_set_buddy_icon_path
#define gaim_account_set_protocol_id         purple_account_set_protocol_id
#define gaim_account_set_connection          purple_account_set_connection
#define gaim_account_set_remember_password   purple_account_set_remember_password
#define gaim_account_set_check_mail          purple_account_set_check_mail
#define gaim_account_set_enabled             purple_account_set_enabled
#define gaim_account_set_proxy_info          purple_account_set_proxy_info
#define gaim_account_set_status_types        purple_account_set_status_types
#define gaim_account_set_status              purple_account_set_status
#define gaim_account_set_status_list         purple_account_set_status_list

#define gaim_account_clear_settings   purple_account_clear_settings

#define gaim_account_set_int    purple_account_set_int
#define gaim_account_set_string purple_account_set_string
#define gaim_account_set_bool   purple_account_set_bool

#define gaim_account_set_ui_int     purple_account_set_ui_int
#define gaim_account_set_ui_string  purple_account_set_ui_string
#define gaim_account_set_ui_bool    purple_account_set_ui_bool

#define gaim_account_is_connected     purple_account_is_connected
#define gaim_account_is_connecting    purple_account_is_connecting
#define gaim_account_is_disconnected  purple_account_is_disconnected

#define gaim_account_get_username           purple_account_get_username
#define gaim_account_get_password           purple_account_get_password
#define gaim_account_get_alias              purple_account_get_alias
#define gaim_account_get_user_info          purple_account_get_user_info
#define gaim_account_get_buddy_icon         purple_account_get_buddy_icon
#define gaim_account_get_buddy_icon_path    purple_account_get_buddy_icon_path
#define gaim_account_get_protocol_id        purple_account_get_protocol_id
#define gaim_account_get_protocol_name      purple_account_get_protocol_name
#define gaim_account_get_connection         purple_account_get_connection
#define gaim_account_get_remember_password  purple_account_get_remember_password
#define gaim_account_get_check_mail         purple_account_get_check_mail
#define gaim_account_get_enabled            purple_account_get_enabled
#define gaim_account_get_proxy_info         purple_account_get_proxy_info
#define gaim_account_get_active_status      purple_account_get_active_status
#define gaim_account_get_status             purple_account_get_status
#define gaim_account_get_status_type        purple_account_get_status_type
#define gaim_account_get_status_type_with_primitive \
	purple_account_get_status_type_with_primitive

#define gaim_account_get_presence       purple_account_get_presence
#define gaim_account_is_status_active   purple_account_is_status_active
#define gaim_account_get_status_types   purple_account_get_status_types

#define gaim_account_get_int            purple_account_get_int
#define gaim_account_get_string         purple_account_get_string
#define gaim_account_get_bool           purple_account_get_bool

#define gaim_account_get_ui_int     purple_account_get_ui_int
#define gaim_account_get_ui_string  purple_account_get_ui_string
#define gaim_account_get_ui_bool    purple_account_get_ui_bool


#define gaim_account_get_log      purple_account_get_log
#define gaim_account_destroy_log  purple_account_destroy_log

#define gaim_account_add_buddy       purple_account_add_buddy
#define gaim_account_add_buddies     purple_account_add_buddies
#define gaim_account_remove_buddy    purple_account_remove_buddy
#define gaim_account_remove_buddies  purple_account_remove_buddies

#define gaim_account_remove_group  purple_account_remove_group

#define gaim_account_change_password  purple_account_change_password

#define gaim_account_supports_offline_message  purple_account_supports_offline_message

#define gaim_accounts_add      purple_accounts_add
#define gaim_accounts_remove   purple_accounts_remove
#define gaim_accounts_delete   purple_accounts_delete
#define gaim_accounts_reorder  purple_accounts_reorder

#define gaim_accounts_get_all         purple_accounts_get_all
#define gaim_accounts_get_all_active  purple_accounts_get_all_active

#define gaim_accounts_find   purple_accounts_find

#define gaim_accounts_restore_current_statuses  purple_accounts_restore_current_statuses

#define gaim_accounts_set_ui_ops  purple_accounts_set_ui_ops
#define gaim_accounts_get_ui_ops  purple_accounts_get_ui_ops

#define gaim_accounts_get_handle  purple_accounts_get_handle

#define gaim_accounts_init    purple_accounts_init
#define gaim_accounts_uninit  purple_accounts_uninit

/* from accountopt.h */

#define GaimAccountOption     PurpleAccountOption
#define GaimAccountUserSplit  PurpleAccountUserSplit

#define gaim_account_option_new         purple_account_option_new
#define gaim_account_option_bool_new    purple_account_option_bool_new
#define gaim_account_option_int_new     purple_account_option_int_new
#define gaim_account_option_string_new  purple_account_option_string_new
#define gaim_account_option_list_new    purple_account_option_list_new

#define gaim_account_option_destroy  purple_account_option_destroy

#define gaim_account_option_set_default_bool    purple_account_option_set_default_bool
#define gaim_account_option_set_default_int     purple_account_option_set_default_int
#define gaim_account_option_set_default_string  purple_account_option_set_default_string

#define gaim_account_option_set_masked  purple_account_option_set_masked

#define gaim_account_option_set_list  purple_account_option_set_list

#define gaim_account_option_add_list_item  purple_account_option_add_list_item

#define gaim_account_option_get_type     purple_account_option_get_type
#define gaim_account_option_get_text     purple_account_option_get_text
#define gaim_account_option_get_setting  purple_account_option_get_setting

#define gaim_account_option_get_default_bool        purple_account_option_get_default_bool
#define gaim_account_option_get_default_int         purple_account_option_get_default_int
#define gaim_account_option_get_default_string      purple_account_option_get_default_string
#define gaim_account_option_get_default_list_value  purple_account_option_get_default_list_value

#define gaim_account_option_get_masked  purple_account_option_get_masked
#define gaim_account_option_get_list    purple_account_option_get_list

#define gaim_account_user_split_new      purple_account_user_split_new
#define gaim_account_user_split_destroy  purple_account_user_split_destroy

#define gaim_account_user_split_get_text           purple_account_user_split_get_text
#define gaim_account_user_split_get_default_value  purple_account_user_split_get_default_value
#define gaim_account_user_split_get_separator      purple_account_user_split_get_separator

/* from blist.h */

#define GaimBuddyList    PurpleBuddyList
#define GaimBlistUiOps   PurpleBlistUiOps
#define GaimBlistNode    PurpleBlistNode

#define GaimChat     PurpleChat
#define GaimGroup    PurpleGroup
#define GaimContact  PurpleContact
#define GaimBuddy    PurpleBuddy

#define GAIM_BLIST_GROUP_NODE     PURPLE_BLIST_GROUP_NODE
#define GAIM_BLIST_CONTACT_NODE   PURPLE_BLIST_CONTACT_NODE
#define GAIM_BLIST_BUDDY_NODE     PURPLE_BLIST_BUDDY_NODE
#define GAIM_BLIST_CHAT_NODE      PURPLE_BLIST_CHAT_NODE
#define GAIM_BLIST_OTHER_NODE     PURPLE_BLIST_OTHER_NODE
#define GaimBlistNodeType         PurpleBlistNodeType

#define GAIM_BLIST_NODE_IS_CHAT       PURPLE_BLIST_NODE_IS_CHAT
#define GAIM_BLIST_NODE_IS_BUDDY      PURPLE_BLIST_NODE_IS_BUDDY
#define GAIM_BLIST_NODE_IS_CONTACT    PURPLE_BLIST_NODE_IS_CONTACT
#define GAIM_BLIST_NODE_IS_GROUP      PURPLE_BLIST_NODE_IS_GROUP

#define GAIM_BUDDY_IS_ONLINE PURPLE_BUDDY_IS_ONLINE

#define GAIM_BLIST_NODE_FLAG_NO_SAVE  PURPLE_BLIST_NODE_FLAG_NO_SAVE
#define GaimBlistNodeFlags            PurpleBlistNodeFlags

#define GAIM_BLIST_NODE_HAS_FLAG     PURPLE_BLIST_NODE_HAS_FLAG
#define GAIM_BLIST_NODE_SHOULD_SAVE  PURPLE_BLIST_NODE_SHOULD_SAVE

#define GAIM_BLIST_NODE_NAME   PURPLE_BLIST_NODE_NAME


#define gaim_blist_new  purple_blist_new
#define gaim_set_blist  purple_set_blist
#define gaim_get_blist  purple_get_blist

#define gaim_blist_get_root   purple_blist_get_root
#define gaim_blist_node_next  purple_blist_node_next

#define gaim_blist_show  purple_blist_show

#define gaim_blist_destroy  purple_blist_destroy

#define gaim_blist_set_visible  purple_blist_set_visible

#define gaim_blist_update_buddy_status  purple_blist_update_buddy_status
#define gaim_blist_update_buddy_icon    purple_blist_update_buddy_icon


#define gaim_blist_alias_contact       purple_blist_alias_contact
#define gaim_blist_alias_buddy         purple_blist_alias_buddy
#define gaim_blist_server_alias_buddy  purple_blist_server_alias_buddy
#define gaim_blist_alias_chat          purple_blist_alias_chat

#define gaim_blist_rename_buddy  purple_blist_rename_buddy
#define gaim_blist_rename_group  purple_blist_rename_group

#define gaim_chat_new        purple_chat_new
#define gaim_blist_add_chat  purple_blist_add_chat

#define gaim_buddy_new           purple_buddy_new
#define gaim_buddy_set_icon      purple_buddy_set_icon
#define gaim_buddy_get_account   purple_buddy_get_account
#define gaim_buddy_get_name      purple_buddy_get_name
#define gaim_buddy_get_icon      purple_buddy_get_icon
#define gaim_buddy_get_contact   purple_buddy_get_contact
#define gaim_buddy_get_presence  purple_buddy_get_presence

#define gaim_blist_add_buddy  purple_blist_add_buddy

#define gaim_group_new  purple_group_new

#define gaim_blist_add_group  purple_blist_add_group

#define gaim_contact_new  purple_contact_new

#define gaim_blist_add_contact    purple_blist_add_contact
#define gaim_blist_merge_contact  purple_blist_merge_contact

#define gaim_contact_get_priority_buddy  purple_contact_get_priority_buddy
#define gaim_contact_set_alias           purple_contact_set_alias
#define gaim_contact_get_alias           purple_contact_get_alias
#define gaim_contact_on_account          purple_contact_on_account

#define gaim_contact_invalidate_priority_buddy  purple_contact_invalidate_priority_buddy

#define gaim_blist_remove_buddy    purple_blist_remove_buddy
#define gaim_blist_remove_contact  purple_blist_remove_contact
#define gaim_blist_remove_chat     purple_blist_remove_chat
#define gaim_blist_remove_group    purple_blist_remove_group

#define gaim_buddy_get_alias_only     purple_buddy_get_alias_only
#define gaim_buddy_get_server_alias   purple_buddy_get_server_alias
#define gaim_buddy_get_contact_alias  purple_buddy_get_contact_alias
#define gaim_buddy_get_local_alias    purple_buddy_get_local_alias
#define gaim_buddy_get_alias          purple_buddy_get_alias

#define gaim_chat_get_name  purple_chat_get_name

#define gaim_find_buddy           purple_find_buddy
#define gaim_find_buddy_in_group  purple_find_buddy_in_group
#define gaim_find_buddies         purple_find_buddies

#define gaim_find_group  purple_find_group

#define gaim_blist_find_chat  purple_blist_find_chat

#define gaim_chat_get_group   purple_chat_get_group
#define gaim_buddy_get_group  purple_buddy_get_group

#define gaim_group_get_accounts  purple_group_get_accounts
#define gaim_group_on_account    purple_group_on_account

#define gaim_blist_add_account     purple_blist_add_account
#define gaim_blist_remove_account  purple_blist_remove_account

#define gaim_blist_get_group_size          purple_blist_get_group_size
#define gaim_blist_get_group_online_count  purple_blist_get_group_online_count

#define gaim_blist_load           purple_blist_load
#define gaim_blist_schedule_save  purple_blist_schedule_save

#define gaim_blist_request_add_buddy  purple_blist_request_add_buddy
#define gaim_blist_request_add_chat   purple_blist_request_add_chat
#define gaim_blist_request_add_group  purple_blist_request_add_group

#define gaim_blist_node_set_bool    purple_blist_node_set_bool
#define gaim_blist_node_get_bool    purple_blist_node_get_bool
#define gaim_blist_node_set_int     purple_blist_node_set_int
#define gaim_blist_node_get_int     purple_blist_node_get_int
#define gaim_blist_node_set_string  purple_blist_node_set_string
#define gaim_blist_node_get_string  purple_blist_node_get_string

#define gaim_blist_node_remove_setting  purple_blist_node_remove_setting

#define gaim_blist_node_set_flags  purple_blist_node_set_flags
#define gaim_blist_node_get_flags  purple_blist_node_get_flags

#define gaim_blist_node_get_extended_menu  purple_blist_node_get_extended_menu

#define gaim_blist_set_ui_ops  purple_blist_set_ui_ops
#define gaim_blist_get_ui_ops  purple_blist_get_ui_ops

#define gaim_blist_get_handle  purple_blist_get_handle

#define gaim_blist_init    purple_blist_init
#define gaim_blist_uninit  purple_blist_uninit


#define GaimBuddyIcon  PurpleBuddyIcon

#define gaim_buddy_icon_new      purple_buddy_icon_new
#define gaim_buddy_icon_destroy  purple_buddy_icon_destroy
#define gaim_buddy_icon_ref      purple_buddy_icon_ref
#define gaim_buddy_icon_unref    purple_buddy_icon_unref
#define gaim_buddy_icon_update   purple_buddy_icon_update
#define gaim_buddy_icon_cache    purple_buddy_icon_cache
#define gaim_buddy_icon_uncache  purple_buddy_icon_uncache

#define gaim_buddy_icon_set_account   purple_buddy_icon_set_account
#define gaim_buddy_icon_set_username  purple_buddy_icon_set_username
#define gaim_buddy_icon_set_data      purple_buddy_icon_set_data
#define gaim_buddy_icon_set_path      purple_buddy_icon_set_path

#define gaim_buddy_icon_get_account   purple_buddy_icon_get_account
#define gaim_buddy_icon_get_username  purple_buddy_icon_get_username
#define gaim_buddy_icon_get_data      purple_buddy_icon_get_data
#define gaim_buddy_icon_get_path      purple_buddy_icon_get_path
#define gaim_buddy_icon_get_type      purple_buddy_icon_get_type

#define gaim_buddy_icons_set_for_user   purple_buddy_icons_set_for_user
#define gaim_buddy_icons_find           purple_buddy_icons_find
#define gaim_buddy_icons_set_caching    purple_buddy_icons_set_caching
#define gaim_buddy_icons_is_caching     purple_buddy_icons_is_caching
#define gaim_buddy_icons_set_cache_dir  purple_buddy_icons_set_cache_dir
#define gaim_buddy_icons_get_cache_dir  purple_buddy_icons_get_cache_dir
#define gaim_buddy_icons_get_full_path  purple_buddy_icons_get_full_path
#define gaim_buddy_icons_get_handle     purple_buddy_icons_get_handle

#define gaim_buddy_icons_init    purple_buddy_icons_init
#define gaim_buddy_icons_uninit  purple_buddy_icons_uninit

#define gaim_buddy_icon_get_scale_size  purple_buddy_icon_get_scale_size

#endif /* _GAIM_COMPAT_H_ */
