/**
 * @file msg.c Message functions
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') (tmp)++; \
	while (*(tmp) && *(tmp) == ' ') \
		(tmp)++

#define GET_NEXT_LINE(tmp) \
	while (*(tmp) && *(tmp) != '\r') \
		(tmp)++; \
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') (tmp)++


#define msn_put16(buf, data) ( \
		(*(buf) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+1) = (unsigned char)(data)&0xff),  \
		2)
#define msn_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define msn_put32(buf, data) ( \
		(*((buf)) = (unsigned char)((data)>>24)&0xff), \
		(*((buf)+1) = (unsigned char)((data)>>16)&0xff), \
		(*((buf)+2) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+3) = (unsigned char)(data)&0xff), \
		4)
#define msn_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))


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
	msn_message_set_flag(msg, 'U');

	msn_message_ref(msg);

	return msg;
}

MsnMessage *
msn_message_new_msnslp(void)
{
	MsnMessage *msg;

	msg = msn_message_new();

	msn_message_set_attr(msg, "User-Agent", NULL);

	msg->msnslp_message = TRUE;
	msg->size += 52;

	msn_message_set_flag(msg, 'D');
	msn_message_set_content_type(msg, "application/x-msnmsgrp2p");
	msn_message_set_charset(msg, NULL);

	return msg;
}

MsnMessage *
msn_message_new_msnslp_ack(MsnMessage *acked_msg)
{
	MsnMessage *msg;

	g_return_val_if_fail(acked_msg != NULL, NULL);
	g_return_val_if_fail(acked_msg->msnslp_message, NULL);

	msg = msn_message_new_msnslp();

	msg->msnslp_ack_message = TRUE;
	msg->acked_msg = msn_message_ref(acked_msg);

	msg->msnslp_header.session_id     = acked_msg->msnslp_header.session_id;
	msg->msnslp_header.total_size_1   = acked_msg->msnslp_header.total_size_1;
	msg->msnslp_header.total_size_2   = acked_msg->msnslp_header.total_size_2;
	msg->msnslp_header.flags          = 0x02;
	msg->msnslp_header.ack_session_id = acked_msg->msnslp_header.session_id;
	msg->msnslp_header.ack_unique_id  = acked_msg->msnslp_header.ack_session_id;
	msg->msnslp_header.ack_length_1   = acked_msg->msnslp_header.total_size_1;
	msg->msnslp_header.ack_length_2   = acked_msg->msnslp_header.total_size_2;

	return msg;
}

MsnMessage *
msn_message_new_from_str(MsnSession *session, const char *str)
{
	MsnMessage *msg;
	char *command_header;
	char *tmp_base, *msg_base, *tmp, *field1, *field2, *c;
	const char *content_type;
	const char *c2;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(!g_ascii_strncasecmp(str, "MSG", 3), NULL);

	msg = msn_message_new();

	/* Clear out the old stuff. */
	msn_message_set_attr(msg, "User-Agent", NULL);
	msn_message_set_content_type(msg, NULL);
	msn_message_set_charset(msg, NULL);

	/*
	 * We need to grab the header and then the size, since this might have
	 * binary data.
	 */
	if ((c2 = strchr(str, '\r')) != NULL)
	{
		tmp = command_header = g_strndup(str, (c2 - str));

		GET_NEXT(tmp); /* Skip MSG */
		field1 = tmp;

		GET_NEXT(tmp); /* Skip the passport or TID */
		field2 = tmp;

		GET_NEXT(tmp); /* Skip the username or flag */
		msg->size = atoi(tmp);
	}
	else
	{
		/* Kind of screwed :) This won't happen. */
		msn_message_destroy(msg);

		return NULL;
	}

	tmp_base = g_malloc(msg->size + 1);
	memcpy(tmp_base, c2 + 2, msg->size);
	tmp_base[msg->size] = '\0';

	tmp = tmp_base;

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

	msg_base = tmp;

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
	content_type = msn_message_get_content_type(msg);

	if (content_type != NULL &&
		!strcmp(content_type, "application/x-msnmsgrp2p"))
	{
		char header[48];
		char footer[4];
		size_t body_len;
		char *tmp2;

		msg->msnslp_message = TRUE;

		memcpy(header, tmp, 48);

		tmp += 48;

		body_len = msg->size - (tmp - tmp_base) - 5;
		msg->body = g_malloc(body_len + 1);

		if (body_len > 0)
			memcpy(msg->body, tmp, body_len);

		msg->body[body_len] = '\0';

		tmp++;

		memcpy(footer, tmp, 4);

		tmp += 4;

		/* Import the header. */
		tmp2 = header;
		msg->msnslp_header.session_id     = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.id             = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.offset_1       = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.offset_2       = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.total_size_1   = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.total_size_2   = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.length         = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.flags          = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.ack_session_id = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.ack_unique_id  = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.ack_length_1   = msn_get32(tmp2); tmp2 += 4;
		msg->msnslp_header.ack_length_2   = msn_get32(tmp2); tmp2 += 4;

		/* Convert to the right endianness */
		msg->msnslp_header.session_id = ntohl(msg->msnslp_header.session_id);
		msg->msnslp_header.id         = ntohl(msg->msnslp_header.id);
		msg->msnslp_header.length     = ntohl(msg->msnslp_header.length);
		msg->msnslp_header.flags      = ntohl(msg->msnslp_header.flags);
		msg->msnslp_header.ack_length_1 =
			ntohl(msg->msnslp_header.ack_length_1);
		msg->msnslp_header.ack_length_2 =
			ntohl(msg->msnslp_header.ack_length_2);
		msg->msnslp_header.ack_session_id =
			ntohl(msg->msnslp_header.ack_session_id);
		msg->msnslp_header.ack_unique_id =
			ntohl(msg->msnslp_header.ack_unique_id);

		/* Import the footer. */
		msg->msnslp_footer.app_id = (long)footer;
	}
	else
	{
		char *tmp2;
		size_t body_len;

		body_len = msg->size - (tmp - tmp_base);

		tmp2 = g_malloc(body_len + 1);

		if (body_len > 0)
			memcpy(tmp2, tmp, body_len);

		tmp2[body_len] = '\0';

		msn_message_set_body(msg, tmp2);

		g_free(tmp2);
	}

	g_free(command_header);
	g_free(tmp_base);

	/* Done! */

	return msg;
}

void
msn_message_destroy(MsnMessage *msg)
{
	g_return_if_fail(msg != NULL);

	if (msg->ref_count > 0) {
		msn_message_unref(msg);

		return;
	}

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

	if (msg->msnslp_ack_message)
		msn_message_unref(msg->acked_msg);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Destroying message\n");
	g_free(msg);
}

MsnMessage *
msn_message_ref(MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	msg->ref_count++;

	return msg;
}

MsnMessage *
msn_message_unref(MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	if (msg->ref_count <= 0)
		return NULL;

	msg->ref_count--;

	if (msg->ref_count == 0) {
		msn_message_destroy(msg);

		return NULL;
	}

	return msg;
}

char *
msn_message_to_string(const MsnMessage *msg, size_t *ret_size)
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
				   (int)msg->size);
	}
	else {
		g_snprintf(buf, sizeof(buf), "MSG %d %c %d\r\n",
				   msn_message_get_transaction_id(msg),
				   msn_message_get_flag(msg), (int)msg->size);
	}

	len = strlen(buf) + msg->size + 1;

	str = g_new0(char, len + 1);

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

	g_strlcat(str, "\r\n", len);

	if (msg->msnslp_message)
	{
		char *c;
		long session_id, id, offset_1, offset_2, total_size_1, total_size_2;
		long length, flags;
		long ack_session_id, ack_unique_id, ack_length_1, ack_length_2;

		c = str + strlen(str);

		session_id      = htonl(msg->msnslp_header.session_id);
		id              = htonl(msg->msnslp_header.id);
		offset_1        = htonl(msg->msnslp_header.offset_1);
		offset_2        = htonl(msg->msnslp_header.offset_2);
		total_size_1    = htonl(msg->msnslp_header.total_size_1);
		total_size_2    = htonl(msg->msnslp_header.total_size_2);
		length          = htonl(msg->msnslp_header.length);
		flags           = htonl(msg->msnslp_header.flags);
		ack_session_id  = htonl(msg->msnslp_header.ack_session_id);
		ack_unique_id   = htonl(msg->msnslp_header.ack_unique_id);
		ack_length_1    = htonl(msg->msnslp_header.ack_length_1);
		ack_length_2    = htonl(msg->msnslp_header.ack_length_2);

		c += msn_put32(c, session_id);
		c += msn_put32(c, id);
		c += msn_put32(c, offset_1);
		c += msn_put32(c, offset_2);
		c += msn_put32(c, total_size_1);
		c += msn_put32(c, total_size_2);
		c += msn_put32(c, length);
		c += msn_put32(c, flags);
		c += msn_put32(c, ack_session_id);
		c += msn_put32(c, ack_unique_id);
		c += msn_put32(c, ack_length_1);
		c += msn_put32(c, ack_length_2);

		if (msg->bin_content)
		{
			size_t bin_len;
			const void *body = msn_message_get_bin_data(msg, &bin_len);

			if (body != NULL)
			{
				memcpy(c, body, bin_len);

				c += bin_len;
			}
		}
		else
		{
			const char *body = msn_message_get_body(msg);

			if (body != NULL)
			{
				g_strlcpy(c, body, msg->size - (c - msg_start));

				c += strlen(body);

				if (strlen(body) > 0)
					*c++ = '\0';
			}
		}

		c += msn_put32(c, msg->msnslp_footer.app_id);

		if (msg->size != (c - msg_start))
		{
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Outgoing message size (%d) and data length (%d) "
					   "do not match!\n", msg->size, (c - msg_start));
		}
	}
	else
	{
		const char *body = msn_message_get_body(msg);

		g_strlcat(str, body, len);

		if (msg->size != strlen(msg_start)) {
			gaim_debug(GAIM_DEBUG_ERROR, "msn",
					   "Outgoing message size (%d) and string length (%d) "
					   "do not match!\n", msg->size, strlen(msg_start));
		}
	}

	if (ret_size != NULL)
		*ret_size = len - 1;

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

	if (msg->msnslp_message)
		msn_message_set_attr(msg, "P2P-Dest", msn_user_get_passport(user));

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
	const char *c;
	char *buf, *d;
	int newline_count = 0;
	size_t new_len;

	g_return_if_fail(msg  != NULL);

	if (msg->bin_content)
	{
		msn_message_set_bin_data(msg, NULL, 0);
		return;
	}

	if (msg->body != NULL) {
		msg->size -= strlen(msg->body);
		g_free(msg->body);

		if (msg->msnslp_message)
			msg->size--;
	}

	if (body != NULL)
	{
		for (c = body; *c != '\0'; c++)
		{
			if (*c == '\n' && (c == body || *(c - 1) != '\r'))
				newline_count++;
		}

		new_len = strlen(body) + newline_count;

		buf = g_new0(char, new_len + 1);

		for (c = body, d = buf; *c != '\0'; c++) {
			if (*c == '\n' && (c == body || *(c - 1) != '\r')) {
				*d++ = '\r';
				*d++ = '\n';
			}
			else
				*d++ = *c;
		}

		msg->body = buf;
		msg->size += new_len;

		msg->bin_content = FALSE;

		if (msg->msnslp_message)
			msg->size++;
	}
	else
		msg->body = NULL;
}

const char *
msn_message_get_body(const MsnMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);
	g_return_val_if_fail(!msg->bin_content, NULL);

	return msg->body;
}

void
msn_message_set_bin_data(MsnMessage *msg, const void *data, size_t len)
{
	g_return_if_fail(msg != NULL);

	if (!msg->bin_content)
		msn_message_set_body(msg, NULL);

	msg->bin_content = TRUE;

	if (msg->body != NULL)
	{
		msg->size -= msg->bin_len;
		g_free(msg->body);
	}

	if (data != NULL && len > 0)
	{
		msg->body = g_memdup(data, len);
		msg->bin_len = len;

		msg->size += len;
	}
	else
	{
		msg->body = NULL;
		msg->bin_len = 0;
	}
}

const void *
msn_message_get_bin_data(const MsnMessage *msg, size_t *len)
{
	g_return_val_if_fail(msg != NULL, NULL);
	g_return_val_if_fail(len != NULL, NULL);
	g_return_val_if_fail(msg->bin_content, NULL);

	*len = msg->bin_len;

	return msg->body;
}

void
msn_message_set_content_type(MsnMessage *msg, const char *type)
{
	g_return_if_fail(msg != NULL);

	if (msg->content_type != NULL) {
		msg->size -= strlen(msg->content_type);
		g_free(msg->content_type);
	}

	if (type != NULL) {
		msg->content_type = g_strdup(type);

		msg->size += strlen(type);
	}
	else
		msg->content_type = NULL;
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

	while (*s != '\r' && *s != '\0') {
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

