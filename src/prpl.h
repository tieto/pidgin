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

#include "core.h"
#include "proxy.h"
#include "multi.h"

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
	PROTO_TOC = 0,     /**< AIM TOC protocol          */
	PROTO_OSCAR,       /**< AIM OSCAR protocol        */
	PROTO_YAHOO,       /**< Yahoo Messenger protocol  */
	PROTO_ICQ,         /**< Outdated ICQ protocol     */
	PROTO_MSN,         /**< MSN Messenger protocol    */
	PROTO_IRC,         /**< IRC protocol              */
	PROTO_FTP,         /**< FTP protocol              */
	PROTO_VGATE,       /**< VGATE protocol            */
	PROTO_JABBER,      /**< Jabber protocol           */
	PROTO_NAPSTER,     /**< Napster/OpenNAP protocol  */
	PROTO_ZEPHYR,      /**< MIT Zephyr protocol       */
	PROTO_GADUGADU,    /**< Gadu-Gadu protocol        */
	PROTO_SAMETIME,    /**< SameTime protocol         */
	PROTO_TLEN,        /**< TLEN protocol             */
	PROTO_RVP,         /**< RVP protocol              */
	PROTO_BACKRUB,     /**< Instant Massager protocol */
	PROTO_UNTAKEN      /**< Untaken protocol number   */

} GaimProtocol;

/** Default protocol plugin description */
#define PRPL_DESC(x) \
		"Allows gaim to use the " x " protocol.\n\n"        \
		"Now that you have loaded this protocol, use the "  \
		"Account Editor to add an account that uses this "  \
		"protocol. You can access the Account Editor from " \
		"the \"Accounts\" button on the login window or "   \
		"in the \"Tools\" menu in the buddy list window."

/** Default protocol */
#define DEFAULT_PROTO   PROTO_OSCAR

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

} GaimProtocolOptions;

/** Custom away message. */
#define GAIM_AWAY_CUSTOM _("Custom")

/**
 * Protocol plugin initialization function.
 */
typedef void (*proto_init)(struct prpl *);

/**
 * A protocol plugin structure.
 *
 * Every protocol plugin initializes this structure. It is the gateway
 * between gaim and the protocol plugin.
 */
struct prpl
{
	GaimProtocol protocol;        /**< The protocol type.         */
	GaimProtocolOptions options;  /**< Protocol options.          */
	struct gaim_plugin *plug;     /**< The base plugin structure. */
	char *name;

	/** 
	 * Returns the base icon name for the given buddy and account.  
	 * If buddy is NULL, it will return the name to use for the account's icon
	 */
	const char *(* list_icon)(struct gaim_account *account, struct buddy *buddy);

	/**
	 * Fills the four char**'s with string identifiers for "emblems" that the UI will
	 * interpret and display as relevant
	 */
	void (* list_emblems)(struct buddy *buddy, char **se, char **sw, char **nw, char **ne);

	/**
	 * Gets a short string representing this buddy's status.  This will be shown
	 * on the buddy list.
	 */
	char *(* status_text)(struct buddy *buddy);
	
	/**
	 * Gets a string to put in the buddy list tooltip.
	 */
	char *(* tooltip_text)(struct buddy *buddy);
	
	GList *(* away_states)(struct gaim_connection *gc);
	GList *(* actions)(struct gaim_connection *gc);
	/* user_splits is a GList of g_malloc'd struct proto_user_split */
	GList *user_splits;
	/* user_opts is a GList* of g_malloc'd struct proto_user_opts */
	GList *user_opts;
	GList *(* buddy_menu)(struct gaim_connection *, char *);
	GList *(* chat_info)(struct gaim_connection *);

	/* All the server-related functions */

	/*
	 * A lot of these (like get_dir) are protocol-dependent and should
	 * be removed. ones like set_dir (which is also protocol-dependent)
	 * can stay though because there's a dialog (i.e. the prpl says you
	 * can set your dir info, the ui shows a dialog and needs to call
	 * set_dir in order to set it)
	 */
	void (* login)		(struct gaim_account *);
	void (* close)		(struct gaim_connection *);
	int  (* send_im)	(struct gaim_connection *, char *who, char *message,
						 int len, int away);
	void (* set_info)	(struct gaim_connection *, char *info);
	int  (* send_typing)    (struct gaim_connection *, char *name, int typing);
	void (* get_info)	(struct gaim_connection *, char *who);
	void (* set_away)	(struct gaim_connection *, char *state, char *message);
	void (* get_away)       (struct gaim_connection *, char *who);
	void (* set_dir)	(struct gaim_connection *, const char *first,
							   const char *middle,
							   const char *last,
							   const char *maiden,
							   const char *city,
							   const char *state,
							   const char *country,
							   int web);
	void (* get_dir)	(struct gaim_connection *, char *who);
	void (* dir_search)	(struct gaim_connection *, const char *first,
							   const char *middle,
							   const char *last,
							   const char *maiden,
							   const char *city,
							   const char *state,
							   const char *country,
							   const char *email);
	void (* set_idle)	(struct gaim_connection *, int idletime);
	void (* change_passwd)	(struct gaim_connection *, const char *old,
							 const char *new);
	void (* add_buddy)	(struct gaim_connection *, const char *name);
	void (* add_buddies)	(struct gaim_connection *, GList *buddies);
	void (* remove_buddy)	(struct gaim_connection *, char *name, char *group);
	void (* remove_buddies)	(struct gaim_connection *, GList *buddies,
							 const char *group);
	void (* add_permit)	(struct gaim_connection *, const char *name);
	void (* add_deny)	(struct gaim_connection *, const char *name);
	void (* rem_permit)	(struct gaim_connection *, const char *name);
	void (* rem_deny)	(struct gaim_connection *, const char *name);
	void (* set_permit_deny)(struct gaim_connection *);
	void (* warn)		(struct gaim_connection *, char *who, int anonymous);
	void (* join_chat)	(struct gaim_connection *, GList *data);
	void (* chat_invite)	(struct gaim_connection *, int id,
							 const char *who, const char *message);
	void (* chat_leave)	(struct gaim_connection *, int id);
	void (* chat_whisper)	(struct gaim_connection *, int id,
							 char *who, char *message);
	int  (* chat_send)	(struct gaim_connection *, int id, char *message);
	void (* keepalive)	(struct gaim_connection *);

	/* new user registration */
	void (* register_user)	(struct gaim_account *);

	/* get "chat buddy" info and away message */
	void (* get_cb_info)	(struct gaim_connection *, int, char *who);
	void (* get_cb_away)	(struct gaim_connection *, int, char *who);

	/* save/store buddy's alias on server list/roster */
	void (* alias_buddy)	(struct gaim_connection *, const char *who,
							 const char *alias);

	/* change a buddy's group on a server list/roster */
	void (* group_buddy)	(struct gaim_connection *, const char *who,
							 const char *old_group, const char *new_group);

	/* rename a group on a server list/roster */
	void (* rename_group)	(struct gaim_connection *, const char *old_group,
							 const char *new_group, GList *members);

	void (* buddy_free)	(struct buddy *);

	/* this is really bad. */
	void (* convo_closed)   (struct gaim_connection *, char *who);

	char *(* normalize)(const char *);
};

/** A list of all loaded protocol plugins. */
extern GSList *protocols;

/** The number of accounts using a given protocol plugin. */
extern int prpl_accounts[];

/**
 * Initializes static protocols.
 *
 * This is mostly just for main.c, when it initializes the protocols.
 */
void static_proto_init();

/**
 * Loads a protocol plugin.
 *
 * @param prpl The initial protocol plugin structure.
 * 
 * @return @c TRUE if the protocol was loaded, or @c FALSE otherwise.
 */
gboolean load_prpl(struct prpl *prpl);

/**
 * Loads a statically compiled protocol plugin.
 *
 * @param pi The initialization function.
 */
void load_protocol(proto_init pi);

/**
 * Unloads a protocol plugin.
 *
 * @param prpl The protocol plugin to unload.
 */
extern void unload_protocol(struct prpl *prpl);

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
gint proto_compare(struct prpl *a, struct prpl *b);

/**
 * Finds a protocol plugin structure of the specified type.
 *
 * @param type The protocol plugin type.
 */
struct prpl *find_prpl(GaimProtocol type);

/**
 * Creates a menu of all protocol plugins and their protocol-specific
 * actions.
 *
 * @note This should be UI-specific code, or rewritten in such a way as to
 *       not use any any GTK code.
 */
void do_proto_menu();

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
void show_got_added(struct gaim_connection *gc, const char *id,
					const char *who, const char *alias, const char *msg);

/**
 * Closes do_ask_dialogs when the associated plugin is unloaded.
 *
 * @param module The module.
 */
void do_ask_cancel_by_handle(GModule *module);

/**
 * Asks the user a question and acts on the response.
 *
 * @param prim    The primary question.
 * @param sec     The secondary question.
 * @param data    The data to be passed to a callback.
 * @param yestext The text for the Yes button.
 * @param doit    The callback function to call when the Yes button is clicked.
 * @param notext  The text for the No button.
 * @param dont    The callback function to call when the No button is clicked.
 * @param handle  The module handle to associate with this dialog, or @c NULL.
 * @param modal   @c TRUE if the dialog should be modal; @c FALSE otherwise.
 */
void do_ask_dialog(const char *prim, const char *sec, void *data,
				   char *yestext, void *doit, char *notext, void *dont,
				   GModule *handle, gboolean modal);

/**
 * Prompts the user for data.
 *
 * @param text The text to present to the user.
 * @param def  The default data, or @c NULL.
 * @param data The data to be passed to the callback.
 * @param doit The callback function to call when the Accept button is clicked.
 * @param dont The callback function to call when the Cancel button is clicked.
 */
void do_prompt_dialog(const char *text, const char *sdef, void *data,
					  void *doit, void *dont);

/**
 * Called to notify the user that the account has new mail.
 *
 * If @a count is less than 0, the dialog will display the the sender
 * and the subject, if available. If @a count is greater than 0, it will
 * display how many messages the user has.
 *
 * @param gc      The gaim connection.
 * @param count   The number of new e-mails.
 * @param from    The sender, or @c NULL.
 * @param subject The subject, or @c NULL.
 * @param url     The URL to go to to read the new mail.
 */
void connection_has_mail(struct gaim_connection *gc, int count,
						 const char *from, const char *subject,
						 const char *url);

/**
 * Retrieves and sets the new buddy icon for a user.
 *
 * @param gc   The gaim connection.
 * @param who  The user.
 * @param data The icon data.
 * @param len  The length of @a data.
 */
void set_icon_data(struct gaim_connection *gc, const char *who, void *data, int len);

/**
 * Retrieves the buddy icon data for a user.
 *
 * @param gc  The gaim connection.
 * @param who The user.
 * @param len The returned length of the data.
 *
 * @return The buddy icon data.
 */
void *get_icon_data(struct gaim_connection *gc, const char *who, int *len);

/* stuff to load/unload PRPLs as necessary */

/**
 * Increments the reference count on a protocol plugin.
 *
 * @param prpl The protocol plugin.
 *
 * @return @c TRUE if the protocol plugin is loaded, or @c FALSE otherwise.
 */
gboolean ref_protocol(struct prpl *prpl);

/**
 * Decrements the reference count on a protocol plugin. 
 *
 * When the reference count hits 0, the protocol plugin is unloaded.
 */
void unref_protocol(struct prpl *prpl);

#endif /* _PRPL_H_ */
