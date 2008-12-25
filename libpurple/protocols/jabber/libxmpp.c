/* purple
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

/* libxmpp is the XMPP protocol plugin. It is linked against libjabbercommon,
 * which may be used to support other protocols (Bonjour) which may need to
 * share code.
 */

#include "internal.h"

#include "accountopt.h"
#include "debug.h"
#include "version.h"

#include "iq.h"
#include "jabber.h"
#include "chat.h"
#include "message.h"
#include "roster.h"
#include "si.h"
#include "message.h"
#include "presence.h"
#include "google.h"
#include "pep.h"
#include "usertune.h"
#include "caps.h"
#include "data.h"

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME | OPT_PROTO_MAIL_CHECK |
#ifdef HAVE_CYRUS_SASL
	OPT_PROTO_PASSWORD_OPTIONAL |
#endif
	OPT_PROTO_SLASH_COMMANDS_NATIVE,
	NULL,							/* user_splits */
	NULL,							/* protocol_options */
	{"png", 32, 32, 96, 96, 0, PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
	jabber_list_icon,				/* list_icon */
	jabber_list_emblem,			/* list_emblems */
	jabber_status_text,				/* status_text */
	jabber_tooltip_text,			/* tooltip_text */
	jabber_status_types,			/* status_types */
	jabber_blist_node_menu,			/* blist_node_menu */
	jabber_chat_info,				/* chat_info */
	jabber_chat_info_defaults,		/* chat_info_defaults */
	jabber_login,					/* login */
	jabber_close,					/* close */
	jabber_message_send_im,			/* send_im */
	jabber_set_info,				/* set_info */
	jabber_send_typing,				/* send_typing */
	jabber_buddy_get_info,			/* get_info */
	jabber_presence_send,			/* set_status */
	jabber_idle_set,				/* set_idle */
	NULL,							/* change_passwd */
	jabber_roster_add_buddy,		/* add_buddy */
	NULL,							/* add_buddies */
	jabber_roster_remove_buddy,		/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	jabber_add_deny,				/* add_deny */
	NULL,							/* rem_permit */
	jabber_rem_deny,				/* rem_deny */
	NULL,							/* set_permit_deny */
	jabber_chat_join,				/* join_chat */
	NULL,							/* reject_chat */
	jabber_get_chat_name,			/* get_chat_name */
	jabber_chat_invite,				/* chat_invite */
	jabber_chat_leave,				/* chat_leave */
	NULL,							/* chat_whisper */
	jabber_message_send_chat,		/* chat_send */
	jabber_keepalive,				/* keepalive */
	jabber_register_account,		/* register_user */
	NULL,							/* get_cb_info */
	NULL,							/* get_cb_away */
	jabber_roster_alias_change,		/* alias_buddy */
	jabber_roster_group_change,		/* group_buddy */
	jabber_roster_group_rename,		/* rename_group */
	NULL,							/* buddy_free */
	jabber_convo_closed,			/* convo_closed */
	jabber_normalize,				/* normalize */
	jabber_set_buddy_icon,			/* set_buddy_icon */
	NULL,							/* remove_group */
	jabber_chat_buddy_real_name,	/* get_cb_real_name */
	jabber_chat_set_topic,			/* set_chat_topic */
	jabber_find_blist_chat,			/* find_blist_chat */
	jabber_roomlist_get_list,		/* roomlist_get_list */
	jabber_roomlist_cancel,			/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	NULL,							/* can_receive_file */
	jabber_si_xfer_send,			/* send_file */
	jabber_si_new_xfer,				/* new_xfer */
	jabber_offline_message,			/* offline_message */
	NULL,							/* whiteboard_prpl_ops */
	jabber_prpl_send_raw,			/* send_raw */
	jabber_roomlist_room_serialize, /* roomlist_room_serialize */
	jabber_unregister_account,		/* unregister_user */
	jabber_send_attention,			/* send_attention */
	jabber_attention_types,			/* attention_types */

	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	NULL
};

static gboolean load_plugin(PurplePlugin *plugin)
{
	purple_signal_register(plugin, "jabber-receiving-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new_outgoing(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	purple_signal_register(plugin, "jabber-sending-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new_outgoing(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_XMLNODE));

	purple_signal_register(plugin, "jabber-sending-text",
			     purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			     purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			     purple_value_new_outgoing(PURPLE_TYPE_STRING));
	
	return TRUE;
}

static gboolean unload_plugin(PurplePlugin *plugin)
{
	purple_signal_unregister(plugin, "jabber-receiving-xmlnode");

	purple_signal_unregister(plugin, "jabber-sending-xmlnode");
	
	purple_signal_unregister(plugin, "jabber-sending-text");
	
	jabber_data_uninit();
	
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-jabber",                                    /**< id             */
	"XMPP",                                           /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("XMPP Protocol Plugin"),
	                                                  /**  description    */
	N_("XMPP Protocol Plugin"),
	NULL,                                             /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	load_plugin,                                      /**< load           */
	unload_plugin,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	jabber_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
#ifdef HAVE_CYRUS_SASL
#ifdef _WIN32
	UINT old_error_mode;
	gchar *sasldir;
#endif
	int ret;
#endif
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	
	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), NULL, '@');
	purple_account_user_split_set_reverse(split, FALSE);
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);
	
	split = purple_account_user_split_new(_("Resource"), NULL, '/');
	purple_account_user_split_set_reverse(split, FALSE);
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);
	
	option = purple_account_option_bool_new(_("Require SSL/TLS"), "require_tls", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);
	
	option = purple_account_option_bool_new(_("Force old (port 5223) SSL"), "old_ssl", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);
	
	option = purple_account_option_bool_new(
						_("Allow plaintext auth over unencrypted streams"),
						"auth_plain_in_clear", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						   option);
	
	option = purple_account_option_int_new(_("Connect port"), "port", 5222);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						   option);

	option = purple_account_option_string_new(_("Connect server"),
						  "connect_server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						  option);

	option = purple_account_option_string_new(_("File transfer proxies"),
						  "ft_proxies",
						/* TODO: Is this an acceptable default? */
						  "proxy.jabber.org");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						  option);

	/* this should probably be part of global smiley theme settings later on,
	  shared with MSN */
	option = purple_account_option_bool_new(_("Show Custom Smileys"),
		"custom_smileys", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
		option);

	jabber_init_plugin(plugin);

	purple_prefs_remove("/plugins/prpl/jabber");

	/* XXX - If any other plugin wants SASL this won't be good ... */
#ifdef HAVE_CYRUS_SASL
#ifdef _WIN32
	sasldir = g_build_filename(wpurple_install_dir(), "sasl2", NULL);
	sasl_set_path(SASL_PATH_TYPE_PLUGIN, sasldir);
	g_free(sasldir);
	/* Suppress error popups for failing to load sasl plugins */
	old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
	if ((ret = sasl_client_init(NULL)) != SASL_OK) {
		purple_debug_error("xmpp", "Error (%d) initializing SASL.\n", ret);
	}
#ifdef _WIN32
	/* Restore the original error mode */
	SetErrorMode(old_error_mode);
#endif
#endif
	jabber_register_commands();
	
	jabber_iq_init();
	jabber_pep_init();
	
	jabber_tune_init();
	jabber_caps_init();
	
	jabber_data_init();
	
	jabber_add_feature("avatarmeta", AVATARNAMESPACEMETA, jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_add_feature("avatardata", AVATARNAMESPACEDATA, jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_add_feature("buzz", "http://www.xmpp.org/extensions/xep-0224.html#ns",
					   jabber_buzz_isenabled);
	jabber_add_feature("bob", XEP_0231_NAMESPACE,
					   jabber_custom_smileys_isenabled);

	jabber_pep_register_handler("avatar", AVATARNAMESPACEMETA, jabber_buddy_avatar_update_metadata);
}


PURPLE_INIT_PLUGIN(jabber, init_plugin, info);
