/**
 * @file msg.c Message functions
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
#include "msn.h"
#include "msg.h"

#define GET_NEXT(tmp) \
	while (*(tmp) && *(tmp) != ' ' && *(tmp) != '\r') \
		(tmp)++; \
	*(tmp)++ = '\0'; \
	if (*(tmp) == '\n') *(tmp)++; \
	while (*(tmp) && *(tmp) == ' ') \
		(tmp)++

#define GET_NEXT_LINE(tmp) \
	while (*(tmp) && *(tmp) != '\r') \
		(tmp)++; \
	*(tmp)++ = '\0'; \
	if (*(tmp) == '\n') *(tmp)++

/*
 * "MIME-Version: 1.0\r\n" == 19
 * "Content-Type: "        == 14
 * "\r\n"                  ==  2
 * "\r\n" before body      ==  2
 *                           ----
 *                            37
 *  MATH PAYS OFF!!
 */
#define MSN_MESSAGE_BASE_SIZE 37

MsnMessage *
msn_message_new(void)
{
	MsnMessage *msg;

	msg = g_new0(MsnMessage, 1);

	msg->attr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
											g_free, g_free);
	msg->size = MSN_MESSAGE_BASE_SIZE;

	msn_message_set_attr(msg, "User-Agent", "Gaim/" VERSION);
	msn_message_set_content_type(msg, "text/plain");
	msn_message_set_charset(msg, "UTF-8");
	msn_message_set_flag(msg, 'N');

	return msg;
}

MsnMessage *
msn_message_new_from_str(MsnSession *session, const char *str)
{
	MsnMessage *msg;
	char *tmp, *field1, *field2, *c;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(!g_ascii_strncasecmp(str, "MSG", 3), NULL);

	msg = msn_message_new();

	tmp = g_strdup(str);

	GET_NEXT(tmp); /* Skip MSG */
	field1 = tmp;

	GET_NEXT(tmp); /* Skip the passport or TID */
	field2 = tmp;

	GET_NEXT(tmp); /* Skip the username or flag */
	msg->size = atoi(tmp);

	if (msg->size != strlen(strchr(str, '\n') + 1)) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Incoming message size (%d) and string length (%d) "
				   "do not match!\n", msg->size, strlen(str));
	}

	/*
	 * We're going to make sure this is incoming by checking field1.
	 * If it has any non-numbers in it, it's incoming. Otherwise, outgoing.
	 */
	msg->incoming = FALSE;

	for (c = field1; *c != '\0'; c++) {
		if (*c < '0' || *c > '9') {
			msg->incoming = TRUE;
			break;
		}
	}

	if (msg->incoming) {
		msg->sender = msn_users_find_with_passport(session->users, field1);

		if (msg->sender == NULL)
			msg->sender = msn_user_new(session, field1, field2);
		else
			msn_user_ref(msg->sender);
	}
	else {
		msg->tid  = atoi(field1);
		msg->flag = *field2;
	}

	/* Back to the parsination. */
	while (*tmp != '\r') {
		char *key, *value;

		key = tmp;

		GET_NEXT(tmp); /* Key */

		value = tmp;

		GET_NEXT_LINE(tmp); /* Value */

		if ((c = strchr(key, ':')) != NULL)
			*c = '\0';

		if (!g_ascii_strcasecmp(key, "Content-Type")) {
			char *charset;

			if ((c = strchr(value, ';')) != NULL) {
				if ((charset = strchr(c, '=')) != NULL) {
					charset++;
					msn_message_set_charset(msg, charset);
				}

				*c = '\0';
			}

			msn_message_set_content_type(msg, value);
		}
		else
			msn_message_set_attr(msg, key, value);
	}

	/* "\r\n" */
	tmp += 2;

	/* Now we *should* be at the body. */
	msn_message_set_body(msg, tmp);

	/* Done! */

	return msg;
}

void
msn_message_destroy(MsnMessage *msg)
{
	g_return_if_fail(msg != NULL);

	if (msg->sender != NULL)
		msn_user_unref(msg->sender);

	if (msg->receiver != NULL)
		msn_user_unref(msg->receiver);

	if (msg->body != NULL)
		g_free(msg->body);

	if (msg->content_type != NULL)
		g_free(msg->content_type);

	if (msg->charset != NULL)
		g_free(msg->charset);

	g_hash_table_destroy(msg->attr_table);
	g_list_free(msg->attr_list);

	g_free(msg);
}

char *
msn_message_build_string(const MsnMessage *msg)
{
	GList *l;
	char *msg_start;
	char *str;
	char buf[MSN_BUF_LEN];
	int len;

	/*
	 * Okay, how we do things here is just bad. I don't like writing to
	 * a static buffer and then copying to the string. Unfortunately,
	 * just trying to append to the string is causing issues.. Such as
	 * the string you're appending to being erased. Ugh. So, this is
	 * good enough for now.
	 *
	 *     -- ChipX86
	 */
	g_return_val_if_fail(msg != NULL, NULL);

	if (msn_message_is_incoming(msg)) {
		MsnUser *sender = msn_message_get_sender(msg);

		g_snprintf(buf, sizeof(buf), "MSG %s %s %d\r\n",
				   msn_user_get_passport(sender), msn_user_get_name(sender),
				   msg->size);
	}
	else {
		g_snprintf(buf, sizeof(buf), "MSG %d %c %d\r\n",
				   msn_message_get_transaction_id(msg),
				   msn_message_get_flag(msg), msg->size);
	}

	len = strlen(buf) + msg->size + 1;

	str = g_new0(char, len);

	g_strlcpy(str, buf, len);

	msg_start = str + strlen(str);

	/* Standard header. */
	if (msg->charset == NULL) {
		g_snprintf(buf, sizeof(buf),
				   "MIME-Version: 1.0\r\n"
				   "Content-Type: %s\r\n",
				   msg->content_type);
	}
	else {
		g_snprintf(buf, sizeof(buf),
				   "MIME-Version: 1.0\r\n"
				   "Content-Type: %s; charset=%s\r\n",
				   msg->content_type, msg->charset);
	}

	g_strlcat(str, buf, len);

	for (l = msg->attr_list; l != NULL; l = l->next) {
		const char *key = (char *)l->data;
		const char *value;

		value = msn_message_get_attr(msg, key);

		g_snprintf(buf, sizeof(buf), "%s: %s\r\n", key, value);

		g_strlcat(str, buf, len);
	}

	g_snprintf(buf, sizeof(buf), "\r\n%s", msn_message_get_body(msg));

	g_strlcat(str, buf, len);

	if (msg->size != strlen(msg_start)) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Outgoing message size (%d) and string length (%d) "
				   "do not match!\n", msg->size, strlen(msg_start));
	}

	return str;
}

gboolean
msn_message_is_outgoing(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, FALSE);

	return !msg->incoming;
}

gboolean
msn_message_is_incoming(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, FALSE);

	return msg->incoming;
}

void
msn_message_set_sender(MsnMessage *msg, MsnUser *user)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(user != NULL);

	msg->sender = user;
	
	msn_user_ref(msg->sender);
}

MsnUser *
msn_message_get_sender(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msg->sender;
}

void
msn_message_set_receiver(MsnMessage *msg, MsnUser *user)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(user != NULL);

	msg->receiver = user;
	
	msn_user_ref(msg->receiver);
}

MsnUser *
msn_message_get_receiver(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msg->receiver;
}

void
msn_message_set_transaction_id(MsnMessage *msg, unsigned int tid)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(tid > 0);

	msg->tid = tid;
}

unsigned int
msn_message_get_transaction_id(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, 0);

	return msg->tid;
}

void
msn_message_set_flag(MsnMessage *msg, char flag)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(flag != 0);

	msg->flag = flag;
}

char
msn_message_get_flag(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, 0);

	return msg->flag;
}

void
msn_message_set_body(MsnMessage *msg, const char *body)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(body != NULL);

	if (msg->body != NULL) {
		msg->size -= strlen(msg->body);
		g_free(msg->body);
	}

	msg->body = g_strdup(body);

	msg->size += strlen(body);
}

const char *
msn_message_get_body(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msg->body;
}

void
msn_message_set_content_type(MsnMessage *msg, const char *type)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(type != NULL);

	if (msg->content_type != NULL) {
		msg->size -= strlen(msg->content_type);
		g_free(msg->content_type);
	}

	msg->content_type = g_strdup(type);

	msg->size += strlen(type);
}

const char *
msn_message_get_content_type(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msg->content_type;
}

void
msn_message_set_charset(MsnMessage *msg, const char *charset)
{
	g_return_if_fail(msg != NULL);

	if (msg->charset != NULL) {
		msg->size -= strlen(msg->charset) + strlen("; charset=");
		g_free(msg->charset);
	}

	if (charset != NULL) {
		msg->charset = g_strdup(charset);

		msg->size += strlen(charset) + strlen("; charset=");
	}
	else
		msg->charset = NULL;
}

const char *
msn_message_get_charset(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msg->charset;
}

void
msn_message_set_attr(MsnMessage *msg, const char *attr, const char *value)
{
	const char *temp;
	char *new_attr;

	g_return_if_fail(msg != NULL);
	g_return_if_fail(attr != NULL);

	temp = msn_message_get_attr(msg, attr);

	if (value == NULL) {
		if (temp != NULL) {
			GList *l;

			msg->size -= strlen(temp) + strlen(attr) + 4;

			for (l = msg->attr_list; l != NULL; l = l->next) {
				if (!g_ascii_strcasecmp(l->data, attr)) {
					msg->attr_list = g_list_remove(msg->attr_list, l->data);

					break;
				}
			}

			g_hash_table_remove(msg->attr_table, attr);
		}

		return;
	}

	new_attr = g_strdup(attr);

	g_hash_table_insert(msg->attr_table, new_attr, g_strdup(value));

	if (temp == NULL) {
		msg->attr_list = g_list_append(msg->attr_list, new_attr);
		msg->size += strlen(attr) + 4;
	}
	else
		msg->size -= strlen(temp);

	msg->size += strlen(value);
}

const char *
msn_message_get_attr(const MsnMessage *msg, const char *attr)
{
	g_return_val_if_fail(msg != NULL, NULL);
	g_return_val_if_fail(attr != NULL, NULL);

	return g_hash_table_lookup(msg->attr_table, attr);
}

GHashTable *
msn_message_get_hashtable_from_body(const MsnMessage *msg)
{
	GHashTable *table;
	char *body, *s, *c;

	g_return_val_if_fail(msg != NULL, NULL);
	g_return_val_if_fail(msn_message_get_body(msg) != NULL, NULL);

	s = body = g_strdup(msn_message_get_body(msg));

	table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	while (*s != '\r') {
		char *key, *value;

		key = s;

		GET_NEXT(s);

		value = s;

		GET_NEXT_LINE(s);

		if ((c = strchr(key, ':')) != NULL) {
			*c = '\0';

			g_hash_table_insert(table, g_strdup(key), g_strdup(value));
		}
	}

	g_free(body);

	return table;
}

