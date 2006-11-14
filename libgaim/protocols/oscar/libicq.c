/* gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 */

/* libicq is the ICQ protocol plugin. It is linked against liboscarcommon,
 * which contains all the shared implementation code with libaim
 */


#include "oscarcommon.h"

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_IM_IMAGE,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"gif,jpeg,bmp,ico", 48, 48, 50, 50,
		GAIM_ICON_SCALE_SEND | GAIM_ICON_SCALE_DISPLAY},	/* icon_spec */
	oscar_list_icon_icq,		/* list_icon */
	oscar_list_emblems,		/* list_emblems */
	oscar_status_text,		/* status_text */
	oscar_tooltip_text,		/* tooltip_text */
	oscar_status_types,		/* status_types */
	oscar_blist_node_menu,	/* blist_node_menu */
	oscar_chat_info,		/* chat_info */
	oscar_chat_info_defaults, /* chat_info_defaults */
	oscar_login,			/* login */
	oscar_close,			/* close */
	oscar_send_im,			/* send_im */
	oscar_set_info,			/* set_info */
	oscar_send_typing,		/* send_typing */
	oscar_get_info,			/* get_info */
	oscar_set_status,		/* set_status */
	oscar_set_idle,			/* set_idle */
	oscar_change_passwd,	/* change_passwd */
	oscar_add_buddy,		/* add_buddy */
	NULL,					/* add_buddies */
	oscar_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	oscar_add_permit,		/* add_permit */
	oscar_add_deny,			/* add_deny */
	oscar_rem_permit,		/* rem_permit */
	oscar_rem_deny,			/* rem_deny */
	oscar_set_permit_deny,	/* set_permit_deny */
	oscar_join_chat,		/* join_chat */
	NULL,					/* reject_chat */
	oscar_get_chat_name,	/* get_chat_name */
	oscar_chat_invite,		/* chat_invite */
	oscar_chat_leave,		/* chat_leave */
	NULL,					/* chat_whisper */
	oscar_send_chat,		/* chat_send */
	oscar_keepalive,		/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	oscar_alias_buddy,		/* alias_buddy */
	oscar_move_buddy,		/* group_buddy */
	oscar_rename_group,		/* rename_group */
	NULL,					/* buddy_free */
	oscar_convo_closed,		/* convo_closed */
	oscar_normalize,		/* normalize */
	oscar_set_icon,			/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	oscar_can_receive_file,	/* can_receive_file */
	oscar_send_file,		/* send_file */
	oscar_new_xfer,			/* new_xfer */
	oscar_offline_message,	/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,				/* send_raw */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-icq",                                     /**< id             */
	"ICQ",                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("ICQ Protocol Plugin"),
	                                                  /**  description    */
	N_("ICQ Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	oscar_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server"), "server", OSCAR_DEFAULT_LOGIN_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Port"), "port", OSCAR_DEFAULT_LOGIN_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Encoding"), "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_bool_new(
		_("Always use ICQ proxy server for file transfers\n(slower, but does not reveal your IP address)"), "always_use_rv_proxy",
		OSCAR_DEFAULT_ALWAYS_USE_RV_PROXY);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	/* Preferences */
	gaim_prefs_add_none("/plugins/prpl/oscar");
	gaim_prefs_add_bool("/plugins/prpl/oscar/recent_buddies", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/oscar/show_idle", FALSE);
	gaim_prefs_remove("/plugins/prpl/oscar/always_use_rv_proxy");
}

GAIM_INIT_PLUGIN(oscar, init_plugin, info);
