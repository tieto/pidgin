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


MsnMessage *
msn_message_new(void)
{
	MsnMessage *msg;

	msg = g_new0(MsnMessage, 1);

	msg->attr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
											g_free, g_free);

	msn_message_ref(msg);

	return msg;
}

MsnMessage *
msn_message_new_plain(const char *message)
{
	MsnMessage *msg;
	char *message_cr;

	msg = msn_message_new();
	msn_message_set_attr(msg, "User-Agent", "Gaim/" VERSION);
	msn_message_set_content_type(msg, "text/plain");
	msn_message_set_charset(msg, "UTF-8");
	msn_message_set_flag(msg, 'N');
	msn_message_set_attr(msg, "X-MMS-IM-Format",
						 "FN=MS%20Sans%20Serif; EF=; CO=0; PF=0");

	message_cr = gaim_str_add_cr(message);
	msn_message_set_bin_data(msg, message_cr, strlen(message_cr));
	g_free(message_cr);

	return msg;
}

MsnMessage *
msn_message_new_msnslp(void)
{
	MsnMessage *msg;

	msg = msn_message_new();

	msn_message_set_attr(msg, "User-Agent", NULL);

	msg->msnslp_message = TRUE;

	msn_message_set_flag(msg, 'D');
	msn_message_set_content_type(msg, "application/x-msnmsgrp2p");

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

	msg->msnslp_header.session_id  = acked_msg->msnslp_header.session_id;
	msg->msnslp_header.total_size  = acked_msg->msnslp_header.total_size;
	msg->msnslp_header.flags       = 0x02;
	msg->msnslp_header.ack_id      = acked_msg->msnslp_header.id;
	msg->msnslp_header.ack_sub_id  = acked_msg->msnslp_header.ack_id;
	msg->msnslp_header.ack_size    = acked_msg->msnslp_header.total_size;

	return msg;
}

void
msn_message_parse_payload(MsnMessage *msg,
						  const char *payload, size_t payload_len)
{
	char *tmp_base, *tmp;
	const char *content_type;

	g_return_if_fail(payload != NULL);

	tmp_base = tmp = g_memdup(payload, payload_len);

	/* Back to the parsination. */
	while (*tmp != '\r')
	{
		char *key, *value, *c;

		key = tmp;

		GET_NEXT(tmp); /* Key */

		value = tmp;

		GET_NEXT_LINE(tmp); /* Value */

		if ((c = strchr(key, ':')) != NULL)
			*c = '\0';

		if (!g_ascii_strcasecmp(key, "MIME-Version"))
			continue;

		if (!g_ascii_strcasecmp(key, "Content-Type"))
		{
			char *charset;

			if ((c = strchr(value, ';')) != NULL)
			{
				if ((charset = strchr(c, '=')) != NULL)
				{
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
		MsnSlpHeader header;
		MsnSlpFooter footer;

		msg->msnslp_message = TRUE;

		/* Import the header. */
		memcpy(&header, tmp, sizeof(header));
		tmp += sizeof(header);

		msg->msnslp_header.session_id = GUINT32_FROM_LE(header.session_id);
		msg->msnslp_header.id         = GUINT64_FROM_LE(header.id);
		msg->msnslp_header.offset     = GUINT64_FROM_LE(header.offset);
		msg->msnslp_header.total_size = GUINT32_FROM_LE(header.total_size);
		msg->msnslp_header.length     = GUINT32_FROM_LE(header.length);
		msg->msnslp_header.flags      = GUINT32_FROM_LE(header.flags);
		msg->msnslp_header.ack_id     = GUINT32_FROM_LE(header.ack_id);
		msg->msnslp_header.ack_sub_id = GUINT32_FROM_LE(header.ack_sub_id);
		msg->msnslp_header.ack_size   = GUINT64_FROM_LE(header.ack_size);

		/* Import the body. */
		msg->body_len = payload_len - (tmp - tmp_base) - sizeof(footer);
		
		if (msg->body_len > 0)
			msg->body = g_memdup(tmp, msg->body_len);

		tmp += msg->body_len;

		/* Import the footer. */
		memcpy(&footer, tmp, sizeof(footer));
		tmp += sizeof(footer);

		msg->msnslp_footer.value = GUINT32_FROM_BE(footer.value);
	}
	else
	{
		msg->body_len = payload_len - (tmp - tmp_base);
		msg->body = g_memdup(tmp, msg->body_len);
	}

	g_free(tmp_base);

	/* Done! */
}

void
msn_message_destroy(MsnMessage *msg)
{
	g_return_if_fail(msg != NULL);

	if (msg->ref_count > 0)
	{
		msn_message_unref(msg);

		return;
	}

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
msn_message_gen_payload(const MsnMessage *msg, size_t *ret_size)
{
	GList *l;
	char *n, *base, *end;
	int len;
	size_t body_len;
	const void *body;

	g_return_val_if_fail(msg != NULL, NULL);

	len = MSN_BUF_LEN;

	base = n = end = g_malloc(len + 1);
	end += len;

	/* Standard header. */
	if (msg->charset == NULL)
	{
		g_snprintf(n, len,
				   "MIME-Version: 1.0\r\n"
				   "Content-Type: %s\r\n",
				   msg->content_type);
	}
	else
	{
		g_snprintf(n, len,
				   "MIME-Version: 1.0\r\n"
				   "Content-Type: %s; charset=%s\r\n",
				   msg->content_type, msg->charset);
	}

	n += strlen(n);

	for (l = msg->attr_list; l != NULL; l = l->next)
	{
		const char *key;
		const char *value;

		key = l->data;
		value = msn_message_get_attr(msg, key);

		g_snprintf(n, end - n, "%s: %s\r\n", key, value);
		n += strlen(n);
	}

	n += g_strlcpy(n, "\r\n", end - n);

	body = msn_message_get_bin_data(msg, &body_len);

	if (msg->msnslp_message)
	{
		MsnSlpHeader header;
		MsnSlpFooter footer;

		header.session_id = GUINT32_TO_LE(msg->msnslp_header.session_id);
		header.id         = GUINT64_TO_LE(msg->msnslp_header.id);
		header.offset     = GUINT64_TO_LE(msg->msnslp_header.offset);
		header.total_size = GUINT32_TO_LE(msg->msnslp_header.total_size);
		header.length     = GUINT32_TO_LE(msg->msnslp_header.length);
		header.flags      = GUINT32_TO_LE(msg->msnslp_header.flags);
		header.ack_id     = GUINT32_TO_LE(msg->msnslp_header.ack_id);
		header.ack_sub_id = GUINT32_TO_LE(msg->msnslp_header.ack_sub_id);
		header.ack_size   = GUINT64_TO_LE(msg->msnslp_header.ack_size);

		memcpy(n, &header, 48);
		n += 48;

		if (body != NULL)
		{
			memcpy(n, body, body_len);
			n += body_len;
		}

		footer.value = GUINT32_TO_BE(msg->msnslp_footer.value);

		memcpy(n, &footer, 4);
		n += 4;
	}
	else
	{
		if (body != NULL)
		{
			memcpy(n, body, body_len);
			n += body_len;
		}
	}

	if (ret_size != NULL)
	{
		*ret_size = n - base;

		if (*ret_size > 1664)
			*ret_size = 1664;
	}

	return base;
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
msn_message_set_bin_data(MsnMessage *msg, const void *data, size_t len)
{
	g_return_if_fail(msg != NULL);

	/* There is no need to waste memory on data we cannot send anyway */
	if (len > 1664)
		len = 1664;

	if (msg->body != NULL)
		g_free(msg->body);

	if (data != NULL && len > 0)
	{
		msg->body = g_memdup(data, len);
		msg->body_len = len;
	}
	else
	{
		msg->body = NULL;
		msg->body_len = 0;
	}
}

const void *
msn_message_get_bin_data(const MsnMessage *msg, size_t *len)
{
	g_return_val_if_fail(msg != NULL, NULL);

	if (len)
		*len = msg->body_len;

	return msg->body;
}

void
msn_message_set_content_type(MsnMessage *msg, const char *type)
{
	g_return_if_fail(msg != NULL);

	if (msg->content_type != NULL)
		g_free(msg->content_type);

	if (type != NULL)
		msg->content_type = g_strdup(type);
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

	if (msg->charset != NULL)
		g_free(msg->charset);

	if (charset != NULL)
		msg->charset = g_strdup(charset);
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

	if (value == NULL)
	{
		if (temp != NULL)
		{
			GList *l;

			for (l = msg->attr_list; l != NULL; l = l->next)
			{
				if (!g_ascii_strcasecmp(l->data, attr))
				{
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

	if (temp == NULL)
		msg->attr_list = g_list_append(msg->attr_list, new_attr);
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

	s = body = g_strdup(msn_message_get_bin_data(msg, NULL));

	g_return_val_if_fail(body != NULL, NULL);

	table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	while (*s != '\r' && *s != '\0')
	{
		char *key, *value;

		key = s;

		GET_NEXT(s);

		value = s;

		GET_NEXT_LINE(s);

		if ((c = strchr(key, ':')) != NULL)
		{
			*c = '\0';

			g_hash_table_insert(table, g_strdup(key), g_strdup(value));
		}
	}

	g_free(body);

	return table;
}
