/**
 * @file google_p2p.h
 *
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
 */

#ifndef PURPLE_JABBER_JINGLE_GOOGLE_P2P_H
#define PURPLE_JABBER_JINGLE_GOOGLE_P2P_H

#include <glib.h>
#include <glib-object.h>

#include "jingle/transport.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_GOOGLE_P2P            (jingle_google_p2p_get_type())
#define JINGLE_TYPE_GOOGLE_P2P_CANDIDATE  (jingle_google_p2p_candidate_get_type())
#define JINGLE_GOOGLE_P2P(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), JINGLE_TYPE_GOOGLE_P2P, JingleGoogleP2P))
#define JINGLE_GOOGLE_P2P_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), JINGLE_TYPE_GOOGLE_P2P, JingleGoogleP2PClass))
#define JINGLE_IS_GOOGLE_P2P(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), JINGLE_TYPE_GOOGLE_P2P))
#define JINGLE_IS_GOOGLE_P2P_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), JINGLE_TYPE_GOOGLE_P2P))
#define JINGLE_GOOGLE_P2P_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), JINGLE_TYPE_GOOGLE_P2P, JingleGoogleP2PClass))

/** @copydoc _JingleGoogleP2P */
typedef struct _JingleGoogleP2P JingleGoogleP2P;
/** @copydoc _JingleGoogleP2PClass */
typedef struct _JingleGoogleP2PClass JingleGoogleP2PClass;
/** @copydoc _JingleGoogleP2PPrivate */
typedef struct _JingleGoogleP2PPrivate JingleGoogleP2PPrivate;
/** @copydoc _JingleGoogleP2PCandidate */
typedef struct _JingleGoogleP2PCandidate JingleGoogleP2PCandidate;

/** The Google P2P class */
struct _JingleGoogleP2PClass
{
	JingleTransportClass parent_class;  /**< The parent class. */

	xmlnode *(*to_xml) (JingleTransport *transport, xmlnode *content, JingleActionType action);
	JingleTransport *(*parse) (xmlnode *transport);
};

/** The Google P2P class's private data */
struct _JingleGoogleP2P
{
	JingleTransport parent;         /**< The parent of this object. */
	JingleGoogleP2PPrivate *priv;   /**< The private data of this object. */
};

struct _JingleGoogleP2PCandidate
{
	gchar *id;
	gchar *address;
	guint port;
	guint preference;
	gchar *type;
	gchar *protocol;
	guint network;
	gchar *username;
	gchar *password;
	guint generation;

	gboolean rem_known; /* TRUE if the remote side knows
	                     * about this candidate */
};

GType jingle_google_p2p_candidate_get_type(void);

/**
 * Gets the Google P2P class's GType
 *
 * @return The Google P2P class's GType.
 */
GType jingle_google_p2p_get_type(void);

JingleGoogleP2PCandidate *jingle_google_p2p_candidate_new(const gchar *id,
		guint generation, const gchar *address, guint port, guint preference,
		const gchar *type, const gchar *protocol,
		const gchar *username, const gchar *password);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_GOOGLE_P2P_H */

