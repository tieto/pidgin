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

/* libfacebook is the Facebook XMPP protocol plugin. It is linked against
 * libjabbercommon, which may be used to support other protocols (Bonjour)
 * which may need to share code.
 */

#include "internal.h"

#include "accountopt.h"
#include "core.h"
#include "debug.h"
#include "version.h"

#include "iq.h"
#include "jabber.h"
#include "chat.h"
#include "disco.h"
#include "message.h"
#include "roster.h"
#include "si.h"
#include "message.h"
#include "presence.h"
#include "google/google.h"
#include "pep.h"
#include "usermood.h"
#include "usertune.h"
#include "caps.h"
#include "data.h"
#include "ibb.h"

static const char *
facebook_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "facebook";
}

static void
facebook_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	JabberStream *js;

	jabber_login(account);

	gc = purple_account_get_connection(account);
	if (!gc)
		return;

	purple_connection_set_flags(gc, 0);

	js = purple_connection_get_protocol_data(gc);
	if (!js)
		return;

	js->server_caps |= JABBER_CAP_FACEBOOK;
}

static PurplePlugin *my_protocol = NULL;

static PurplePluginProtocolInfo prpl_info =
{
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	0, /* PurpleProtocolOptions */
	NULL,							/* user_splits */
	NULL,							/* protocol_options */
	NO_BUDDY_ICONS, /* icon_spec */
	facebook_list_icon,				/* list_icon */
	jabber_list_emblem,			/* list_emblems */
	jabber_status_text,				/* status_text */
	jabber_tooltip_text,			/* tooltip_text */
	jabber_status_types,			/* status_types */
	NULL,							/* blist_node_menu */
	NULL,							/* chat_info */
	NULL,							/* chat_info_defaults */
	facebook_login,					/* login */
	jabber_close,					/* close */
	jabber_message_send_im,			/* send_im */
	NULL,				/* set_info */
	jabber_send_typing,				/* send_typing */
	jabber_buddy_get_info,			/* get_info */
	jabber_set_status,				/* set_status */
	jabber_idle_set,				/* set_idle */
	NULL,							/* change_passwd */
	NULL,							/* add_buddy */
	NULL,							/* add_buddies */
	NULL,							/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	NULL,							/* add_deny */
	NULL,							/* rem_permit */
	NULL,							/* rem_deny */
	NULL,							/* set_permit_deny */
	NULL,							/* join_chat */
	NULL,							/* reject_chat */
	NULL,							/* get_chat_name */
	NULL,							/* chat_invite */
	NULL,							/* chat_leave */
	NULL,							/* chat_send */
	jabber_keepalive,				/* keepalive */
	NULL,							/* register_user */
	NULL,							/* get_cb_info */
	NULL,							/* alias_buddy */
	NULL,							/* group_buddy */
	NULL,							/* rename_group */
	NULL,							/* buddy_free */
	jabber_convo_closed,			/* convo_closed */
	jabber_normalize,				/* normalize */
	NULL,							/* set_buddy_icon */
	NULL,							/* remove_group */
	NULL,							/* get_cb_real_name */
	NULL,							/* set_chat_topic */
	NULL,							/* find_blist_chat */
	NULL,							/* roomlist_get_list */
	NULL,							/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	NULL,							/* can_receive_file */
	NULL,							/* send_file */
	NULL,							/* new_xfer */
	jabber_offline_message,			/* offline_message */
	NULL,							/* whiteboard_prpl_ops */
	jabber_prpl_send_raw,			/* send_raw */
	jabber_roomlist_room_serialize, /* roomlist_room_serialize */
	NULL,							/* unregister_user */
	NULL,							/* send_attention */
	NULL,							/* attention_types */
	NULL, /* get_account_text_table */
	NULL,							/* initiate_media */
	NULL,							/* get_media_caps */
	NULL,							/* get_moods */
	NULL, /* set_public_alias */
	NULL, /* get_public_alias */
	NULL, /* get_max_message_size */
	NULL  /* media_send_dtmf */
};

static gboolean load_plugin(PurplePlugin *plugin)
{
	jabber_plugin_init(plugin);

	return TRUE;
}

static gboolean unload_plugin(PurplePlugin *plugin)
{
	jabber_plugin_uninit(plugin);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                         /**< type           */
	NULL,                                           /**< ui_requirement */
	0,                                              /**< flags          */
	NULL,                                           /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority       */

	"prpl-facebook-xmpp",                           /**< id             */
	"Facebook (XMPP)",                              /**< name           */
	DISPLAY_VERSION,                                /**< version        */
	                                                /**  summary        */
	N_("Facebook XMPP Protocol Plugin"),
	                                                /**  description    */
	N_("Facebook XMPP Protocol Plugin"),
	NULL,                                           /**< author         */
	PURPLE_WEBSITE,                                 /**< homepage       */

	load_plugin,                                    /**< load           */
	unload_plugin,                                  /**< unload         */
	NULL,                                           /**< destroy        */

	NULL,                                           /**< ui_info        */
	&prpl_info,                                     /**< extra_info     */
	NULL,                                           /**< prefs_info     */
	NULL,                                           /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), "chat.facebook.com", '@');
	purple_account_user_split_set_reverse(split, FALSE);
	purple_account_user_split_set_constant(split, TRUE);
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	purple_account_user_split_set_constant(split, TRUE);
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

#define ADD_VALUE(list, desc, v) { \
	PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1); \
	kvp->key = g_strdup((desc)); \
	kvp->value = g_strdup((v)); \
	list = g_list_prepend(list, kvp); \
}

	ADD_VALUE(encryption_values, _("Use encryption if available"), "opportunistic_tls");
	ADD_VALUE(encryption_values, _("Require encryption"), "require_tls");
	ADD_VALUE(encryption_values, _("Use old-style SSL"), "old_ssl");
#if 0
	ADD_VALUE(encryption_values, "None", "none");
#endif
	encryption_values = g_list_reverse(encryption_values);

#undef ADD_VALUE

	option = purple_account_option_list_new(_("Connection security"), "connection_security", encryption_values);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						   option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
						  option);

	my_protocol = plugin;
}

PURPLE_INIT_PLUGIN(facebookxmpp, init_plugin, info);
