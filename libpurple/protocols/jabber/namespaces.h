/*
 * purple - Jabber Protocol Plugin
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#ifndef PURPLE_JABBER_NAMESPACES_H_
#define PURPLE_JABBER_NAMESPACES_H_

/* Implementation of XEP-0084 */

/* XEP-0047 IBB (In-band bytestreams) */
#define NS_IBB "http://jabber.org/protocol/ibb"

/* XEP-0084 v0.12 User Avatar */
#define NS_AVATAR_0_12_DATA     "http://www.xmpp.org/extensions/xep-0084.html#ns-data"
#define NS_AVATAR_0_12_METADATA "http://www.xmpp.org/extensions/xep-0084.html#ns-metadata"

/* XEP-0084 v1.1 User Avatar */
#define NS_AVATAR_1_1_DATA      "urn:xmpp:avatar:data"
#define NS_AVATAR_1_1_METADATA  "urn:xmpp:avatar:metadata"

/* XEP-0199 Ping */
#define NS_PING "urn:xmpp:ping"

/* XEP-0224 Attention */
#define NS_ATTENTION "urn:xmpp:attention:0"

/* XEP-0231 BoB (Bits of Binary) */
#define NS_BOB "urn:xmpp:bob"

/* Google extensions */
#define NS_GOOGLE_CAMERA "http://www.google.com/xmpp/protocol/camera/v1"
#define NS_GOOGLE_VIDEO "http://www.google.com/xmpp/protocol/video/v1"
#define NS_GOOGLE_VOICE "http://www.google.com/xmpp/protocol/voice/v1"
#define NS_GOOGLE_JINGLE_INFO "google:jingleinfo"

#define NS_GOOGLE_PROTOCOL_SESSION "http://www.google.com/xmpp/protocol/session"
#define NS_GOOGLE_SESSION "http://www.google.com/session"
#define NS_GOOGLE_SESSION_PHONE "http://www.google.com/session/phone"
#define NS_GOOGLE_SESSION_VIDEO "http://www.google.com/session/video"

#endif /* PURPLE_JABBER_NAMESPACES_H_ */
