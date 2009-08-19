/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#ifndef PIDGIN_XMPP_DISCO_H
#define PIDGIN_XMPP_DISCO_H

typedef struct _XmppDiscoService XmppDiscoService;

#include "gtkdisco.h"

#define XMPP_PLUGIN_ID      "prpl-jabber"
#define NS_DISCO_INFO       "http://jabber.org/protocol/disco#info"
#define NS_DISCO_ITEMS      "http://jabber.org/protocol/disco#items"
#define NS_MUC              "http://jabber.org/protocol/muc"
#define NS_REGISTER         "jabber:iq:register"

#include "plugin.h"
extern PurplePlugin *my_plugin;

/**
 * The types of services.
 */
typedef enum
{
	XMPP_DISCO_SERVICE_TYPE_UNSET,
	/**
	 * A registerable gateway to another protocol. An example would be
	 * XMPP legacy transports.
	 */
	XMPP_DISCO_SERVICE_TYPE_GATEWAY,

	/**
	 * A directory (e.g. allows the user to search for other users).
	 */
	XMPP_DISCO_SERVICE_TYPE_DIRECTORY,

	/**
	 * A chat (multi-user conversation).
	 */
	XMPP_DISCO_SERVICE_TYPE_CHAT,

	/**
	 * A pubsub collection (contains nodes)
	 */
	XMPP_DISCO_SERVICE_TYPE_PUBSUB_COLLECTION,

	/**
	 * A pubsub leaf (contains stuff, not nodes).
	 */
	XMPP_DISCO_SERVICE_TYPE_PUBSUB_LEAF,

	/**
	 * Something else. Do we need more categories?
	 */
	XMPP_DISCO_SERVICE_TYPE_OTHER
} XmppDiscoServiceType;

/**
 * The flags of services.
 */
typedef enum
{
	XMPP_DISCO_NONE          = 0x0000,
	XMPP_DISCO_ADD           = 0x0001, /**< Supports an 'add' operation */
	XMPP_DISCO_BROWSE        = 0x0002, /**< Supports browsing */
	XMPP_DISCO_REGISTER      = 0x0004  /**< Supports a 'register' operation */
} XmppDiscoServiceFlags;

struct _XmppDiscoService {
	PidginDiscoList *list;
	gchar *name;
	gchar *description;

	gchar *gateway_type;
	XmppDiscoServiceType type;
	XmppDiscoServiceFlags flags;

	XmppDiscoService *parent;
	gchar *jid;
	gchar *node;
	gboolean expanded;
};

void xmpp_disco_start(PidginDiscoList *list);

void xmpp_disco_service_expand(XmppDiscoService *service);
void xmpp_disco_service_register(XmppDiscoService *service);

#endif /* PIDGIN_XMPP_DISCO_H */
