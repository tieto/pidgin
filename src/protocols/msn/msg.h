/**
 * @file msg.h Message functions
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _MSN_MSG_H_
#define _MSN_MSG_H_

typedef struct _MsnMessage MsnMessage;

#include "session.h"
#include "user.h"

/**
 * A message.
 */
struct _MsnMessage
{
	size_t ref_count;           /**< The reference count.       */

	MsnUser *sender;
	MsnUser *receiver;

	unsigned int tid;
	char flag;

	gboolean incoming;

	size_t size;

	char *content_type;
	char *charset;
	char *body;

	GHashTable *attr_table;
	GList *attr_list;
};

/**
 * Creates a new, empty message.
 *
 * @return A new message.
 */
MsnMessage *msn_message_new(void);

/**
 * Creates a new message based off a string.
 *
 * @param session The MSN session.
 * @param str     The string.
 *
 * @return The new message.
 */
MsnMessage *msn_message_new_from_str(MsnSession *session, const char *str);

/**
 * Destroys a message.
 */
void msn_message_destroy(MsnMessage *msg);

/**
 * Increments the reference count on a message.
 *
 * @param msg The message.
 *
 * @return @a msg
 */
MsnMessage *msn_message_ref(MsnMessage *msg);

/**
 * Decrements the reference count on a message.
 *
 * This will destroy the structure if the count hits 0.
 *
 * @param msg The message.
 *
 * @return @a msg, or @c NULL if the new count is 0.
 */
MsnMessage *msn_message_unref(MsnMessage *msg);

/**
 * Converts a message to a string.
 *
 * @param msg The message.
 *
 * @return The string representation of a message.
 */
char *msn_message_build_string(const MsnMessage *msg);

/**
 * Returns TRUE if the message is outgoing.
 *
 * @param msg The message.
 *
 * @return @c TRUE if the message is outgoing, or @c FALSE otherwise.
 */
gboolean msn_message_is_outgoing(const MsnMessage *msg);

/**
 * Returns TRUE if the message is incoming.
 *
 * @param msg The message.
 *
 * @return @c TRUE if the message is incoming, or @c FALSE otherwise.
 */
gboolean msn_message_is_incoming(const MsnMessage *msg);

/**
 * Sets the message's sender.
 *
 * @param msg  The message.
 * @param user The sender.
 */
void msn_message_set_sender(MsnMessage *msg, MsnUser *user);

/**
 * Returns the message's sender.
 *
 * @param msg The message.
 *
 * @return The sender.
 */
MsnUser *msn_message_get_sender(const MsnMessage *msg);

/**
 * Sets the message's receiver.
 *
 * @param msg  The message.
 * @param user The receiver.
 */
void msn_message_set_receiver(MsnMessage *msg, MsnUser *user);

/**
 * Returns the message's receiver.
 *
 * @param msg The message.
 *
 * @return The receiver.
 */
MsnUser *msn_message_get_receiver(const MsnMessage *msg);

/**
 * Sets the message transaction ID.
 *
 * @param msg The message.
 * @param tid The transaction ID.
 */
void msn_message_set_transaction_id(MsnMessage *msg, unsigned int tid);

/**
 * Returns the message transaction ID.
 *
 * @param msg The message.
 *
 * @return The transaction ID.
 */
unsigned int msn_message_get_transaction_id(const MsnMessage *msg);

/**
 * Sets the flag for an outgoing message.
 *
 * @param msg  The message.
 * @param flag The flag.
 */
void msn_message_set_flag(MsnMessage *msg, char flag);

/**
 * Returns the flag for an outgoing message.
 *
 * @param msg The message.
 *
 * @return The flag.
 */
char msn_message_get_flag(const MsnMessage *msg);

/**
 * Sets the body of a message.
 *
 * @param msg  The message.
 * @param body The body of the message.
 */
void msn_message_set_body(MsnMessage *msg, const char *body);

/**
 * Returns the body of the message.
 *
 * @param msg The message.
 *
 * @return The body of the message.
 */
const char *msn_message_get_body(const MsnMessage *msg);

/**
 * Sets the content type in a message.
 *
 * @param msg  The message.
 * @param type The content-type.
 */
void msn_message_set_content_type(MsnMessage *msg, const char *type);

/**
 * Returns the content type in a message.
 *
 * @param msg The message.
 *
 * @return The content-type.
 */
const char *msn_message_get_content_type(const MsnMessage *msg);

/**
 * Sets the charset in a message.
 *
 * @param msg     The message.
 * @param charset The charset.
 */
void msn_message_set_charset(MsnMessage *msg, const char *charset);

/**
 * Returns the charset in a message.
 *
 * @param msg The message.
 *
 * @return The charset.
 */
const char *msn_message_get_charset(const MsnMessage *msg);

/**
 * Sets an attribute in a message.
 *
 * @param msg   The message.
 * @param attr  The attribute name.
 * @param value The attribute value.
 */
void msn_message_set_attr(MsnMessage *msg, const char *attr,
						  const char *value);

/**
 * Returns an attribute from a message.
 *
 * @param msg  The message.
 * @param attr The attribute.
 *
 * @return The value, or @c NULL if not found.
 */
const char *msn_message_get_attr(const MsnMessage *msg, const char *attr);

/**
 * Parses the body and returns it in the form of a hashtable.
 *
 * @param msg The message.
 *
 * @return The resulting hashtable.
 */
GHashTable *msn_message_get_hashtable_from_body(const MsnMessage *msg);

#endif /* _MSN_MSG_H_ */
