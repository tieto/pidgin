/**
 * @file prpl.h Protocol Plugin functions
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 */

/* this file should be all that prpls need to include. therefore, by including
 * this file, they should get glib, proxy, gaim_connection, prpl, etc. */

#ifndef _GAIM_PRPL_H_
#define _GAIM_PRPL_H_

typedef struct _GaimPluginProtocolInfo GaimPluginProtocolInfo;

/**************************************************************************/
/** @name Basic Protocol Information                                      */
/**************************************************************************/
/*@{*/

/**
 * Protocol types and numbers.
 *
 * Do not assume a new protocol number without talking to
 * Rob Flynn or Sean Egan first!
 */
typedef enum
{
	GAIM_PROTO_TOC = 0,     /**< AIM TOC protocol          */
	GAIM_PROTO_OSCAR,       /**< AIM OSCAR protocol        */
	GAIM_PROTO_YAHOO,       /**< Yahoo Messenger protocol  */
	GAIM_PROTO_ICQ,         /**< Outdated ICQ protocol     */
	GAIM_PROTO_MSN,         /**< MSN Messenger protocol    */
	GAIM_PROTO_IRC,         /**< IRC protocol              */
	GAIM_PROTO_FTP,         /**< FTP protocol              */
	GAIM_PROTO_VGATE,       /**< VGATE protocol            */
	GAIM_PROTO_JABBER,      /**< Jabber protocol           */
	GAIM_PROTO_NAPSTER,     /**< Napster/OpenNAP protocol  */
	GAIM_PROTO_ZEPHYR,      /**< MIT Zephyr protocol       */
	GAIM_PROTO_GADUGADU,    /**< Gadu-Gadu protocol        */
	GAIM_PROTO_SAMETIME,    /**< SameTime protocol         */
	GAIM_PROTO_TLEN,        /**< TLEN protocol             */
	GAIM_PROTO_RVP,         /**< RVP protocol              */
	GAIM_PROTO_BACKRUB,     /**< Instant Massager protocol */
	GAIM_PROTO_MOO,         /**< MOO protocol              */
	GAIM_PROTO_UNTAKEN      /**< Untaken protocol number   */

} GaimProtocol;

#include "core.h"
#include "proxy.h"
#include "multi.h"

/** Default protocol plugin description */
#define GAIM_PRPL_DESC(x) \
		"Allows gaim to use the " (x) " protocol.\n\n"      \
		"Now that you have loaded this protocol, use the "  \
		"Account Editor to add an account that uses this "  \
		"protocol. You can access the Account Editor from " \
		"the \"Accounts\" button on the login window or "   \
		"in the \"Tools\" menu in the buddy list window."

/** Default protocol */
#define GAIM_PROTO_DEFAULT GAIM_PROTO_OSCAR

/*@}*/

/**
 * Protocol options
 *
 * These should all be stuff that some plugins can do and others can't.
 */
typedef enum
{
	/**
	 * TOC and Oscar send HTML-encoded messages;
	 * most other protocols don't.
	 */
#if 0
	#define OPT_PROTO_HTML            0x00000001 this should be per-connection */
#endif

	/**
	 * Synchronize the time between the local computer and the server.
	 *
	 * TOC and Oscar have signon time, and the server's time needs
	 * to be adjusted to match your computer's time.
	 *
	 * We wouldn't need this if everyone used NTP.
	 */
	OPT_PROTO_CORRECT_TIME = 0x00000002,

	/**
	 * Use a unique name, not an alias, for chat rooms.
	 *
	 * Jabber lets you choose what name you want for chat.
	 * So it shouldn't be pulling the alias for when you're in chat;
	 * it gets annoying.
	 */
	OPT_PROTO_UNIQUE_CHATNAME = 0x00000004,

	/**
	 * Chat rooms have topics.
	 *
	 * IRC and Jabber support this.
	 */
	OPT_PROTO_CHAT_TOPIC = 0x00000008,

	/**
	 * Don't require passwords for sign-in.
	 *
	 * Zephyr doesn't require passwords, so there's no need for
	 * a password prompt.
	 */
	OPT_PROTO_NO_PASSWORD = 0x00000010,

	/**
	 * Notify on new mail.
	 *
	 * MSN and Yahoo notify you when you have new mail.
	 */
	OPT_PROTO_MAIL_CHECK = 0x00000020,

	/**
	 * Buddy icon support.
	 * 
	 * Oscar and Jabber have buddy icons.
	 */
	OPT_PROTO_BUDDY_ICON = 0x00000040,

	/**
	 * Images in IMs.
	 *
	 * Oscar lets you send images in direct IMs.
	 */
	OPT_PROTO_IM_IMAGE = 0x00000080,

	/**
	 * Allow passwords to be optional.
	 *
	 * Passwords in IRC are optional, and are needed for certain
	 * functionality.
	 */
	OPT_PROTO_PASSWORD_OPTIONAL = 0x00000100,

	/**
	 * Allows font size to be specified in sane point size
	 *
	 * Probably just Jabber and Y!M
	 */
	OPT_PROTO_USE_POINTSIZE = 0x00000200,

} GaimProtocolOptions;

/** Custom away message. */
#define GAIM_AWAY_CUSTOM _("Custom")

/**
 * A protocol plugin information structure.
 *
 * Every protocol plugin initializes this structure. It is the gateway
 * between gaim and the protocol plugin.
 */
struct _GaimPluginProtocolInfo
{
	GaimProtocol protocol;        /**< The protocol type.         */
	GaimProtocolOptions options;  /**< Protocol options.          */

	/* user_splits is a GList of g_malloc'd struct proto_user_split */
	GList *user_splits;
	/* user_opts is a GList* of g_malloc'd struct proto_user_opts */
	GList *user_opts;

	/** 
	 * Returns the base icon name for the given buddy and account.  
	 * If buddy is NULL, it will return the name to use for the account's icon
	 */
	const char *(*list_icon)(GaimAccount *account, struct buddy *buddy);

	/**
	 * Fills the four char**'s with string identifiers for "emblems"
	 * that the UI will interpret and display as relevant
	 */
	void (*list_emblems)(struct buddy *buddy, char **se, char **sw,
						  char **nw, char **ne);

	/**
	 * Gets a short string representing this buddy's status.  This will
	 * be shown on the buddy list.
	 */
	char *(*status_text)(struct buddy *buddy);
	
	/**
	 * Gets a string to put in the buddy list tooltip.
	 */
	char *(*tooltip_text)(struct buddy *buddy);
	
	GList *(*away_states)(GaimConnection *gc);
	GList *(*actions)(GaimConnection *gc);

	GList *(*buddy_menu)(GaimConnection *, const char *);
	GList *(*chat_info)(GaimConnection *);

	/* All the server-related functions */

	/*
	 * A lot of these (like get_dir) are protocol-dependent and should
	 * be removed. ones like set_dir (which is also protocol-dependent)
	 * can stay though because there's a dialog (i.e. the prpl says you
	 * can set your dir info, the ui shows a dialog and needs to call
	 * set_dir in order to set it)
	 */
	void (*login)(GaimAccount *);
	void (*close)(GaimConnection *);
	int  (*send_im)(GaimConnection *, const char *who,
					const char *message, int len, int away);
	void (*set_info)(GaimConnection *, char *info);
	int  (*send_typing)(GaimConnection *, char *name, int typing);
	void (*get_info)(GaimConnection *, const char *who);
	void (*set_away)(GaimConnection *, char *state, char *message);
	void (*get_away)(GaimConnection *, const char *who);
	void (*set_dir)(GaimConnection *, const char *first,
					const char *middle, const char *last,
					const char *maiden, const char *city,
					const char *state, const char *country, int web);
	void (*get_dir)(GaimConnection *, const char *who);
	void (*dir_search)(GaimConnection *, const char *first,
					   const char *middle, const char *last,
					   const char *maiden, const char *city,
					   const char *state, const char *country,
					   const char *email);
	void (*set_idle)(GaimConnection *, int idletime);
	void (*change_passwd)(GaimConnection *, const char *old,
						  const char *new);
	void (*add_buddy)(GaimConnection *, const char *name);
	void (*add_buddies)(GaimConnection *, GList *buddies);
	void (*remove_buddy)(GaimConnection *, char *name, char *group);
	void (*remove_buddies)(GaimConnection *, GList *buddies,
						   const char *group);
	void (*add_permit)(GaimConnection *, const char *name);
	void (*add_deny)(GaimConnection *, const char *name);
	void (*rem_permit)(GaimConnection *, const char *name);
	void (*rem_deny)(GaimConnection *, const char *name);
	void (*set_permit_deny)(GaimConnection *);
	void (*warn)(GaimConnection *, char *who, int anonymous);
	void (*join_chat)(GaimConnection *, GHashTable *components);
	void (*chat_invite)(GaimConnection *, int id,
						const char *who, const char *message);
	void (*chat_leave)(GaimConnection *, int id);
	void (*chat_whisper)(GaimConnection *, int id,
						 char *who, char *message);
	int  (*chat_send)(GaimConnection *, int id, char *message);
	void (*keepalive)(GaimConnection *);

	/* new user registration */
	void (*register_user)(GaimAccount *);

	/* get "chat buddy" info and away message */
	void (*get_cb_info)(GaimConnection *, int, char *who);
	void (*get_cb_away)(GaimConnection *, int, char *who);

	/* save/store buddy's alias on server list/roster */
	void (*alias_buddy)(GaimConnection *, const char *who,
						const char *alias);

	/* change a buddy's group on a server list/roster */
	void (*group_buddy)(GaimConnection *, const char *who,
						const char *old_group, const char *new_group);

	/* rename a group on a server list/roster */
	void (*rename_group)(GaimConnection *, const char *old_group,
						 const char *new_group, GList *members);

	void (*buddy_free)(struct buddy *);

	/* this is really bad. */
	void (*convo_closed)(GaimConnection *, char *who);

	char *(*normalize)(const char *);
};

#define GAIM_IS_PROTOCOL_PLUGIN(plugin) \
	((plugin)->info->type == GAIM_PLUGIN_PROTOCOL)

#define GAIM_PLUGIN_PROTOCOL_INFO(plugin) \
	((GaimPluginProtocolInfo *)(plugin)->info->extra_info)

/** A list of all loaded protocol plugins. */
extern GSList *protocols;

/**
 * Compares two protocol plugins, based off their protocol plugin number.
 *
 * @param a The first protocol plugin.
 * @param b The second protocol plugin.
 *
 * @return <= 1 if the first plugin's number is smaller than the second;
 *         0 if the first plugin's number is equal to the second; or
 *         >= 1 if the first plugin's number is greater than the second.
 */
gint gaim_prpl_compare(GaimPlugin *a, GaimPlugin *b);

/**
 * Finds a protocol plugin structure of the specified type.
 *
 * @param type The protocol plugin;
 */
GaimPlugin *gaim_find_prpl(GaimProtocol type);

/**
 * Creates a menu of all protocol plugins and their protocol-specific
 * actions.
 *
 * @note This should be UI-specific code, or rewritten in such a way as to
 *       not use any any GTK code.
 */
void do_proto_menu(void);

/**
 * Shows a message saying that somebody added you as a buddy, and asks
 * if you would like to do the same.
 *
 * @param gc    The gaim connection.
 * @param id    The ID of the user.
 * @param who   The username.
 * @param alias The user's alias.
 * @param msg   The message to go along with the request.
 */
void show_got_added(GaimConnection *gc, const char *id,
					const char *who, const char *alias, const char *msg);

/**
 * Retrieves and sets the new buddy icon for a user.
 *
 * @param gc   The gaim connection.
 * @param who  The user.
 * @param data The icon data.
 * @param len  The length of @a data.
 */
void set_icon_data(GaimConnection *gc, const char *who, void *data, int len);

/**
 * Retrieves the buddy icon data for a user.
 *
 * @param gc  The gaim connection.
 * @param who The user.
 * @param len The returned length of the data.
 *
 * @return The buddy icon data.
 */
void *get_icon_data(GaimConnection *gc, const char *who, int *len);

#endif /* _PRPL_H_ */
