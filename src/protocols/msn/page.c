/**
 * @file page.c Paging functions
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
#include "page.h"

#define GET_NEXT(tmp) \
	while (*(tmp) && *(tmp) != ' ' && *(tmp) != '\r') \
		(tmp)++; \
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') *(tmp)++; \
	while (*(tmp) && *(tmp) == ' ') \
		(tmp)++

#define GET_NEXT_LINE(tmp) \
	while (*(tmp) && *(tmp) != '\r') \
		(tmp)++; \
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') *(tmp)++

/*
 * "<TEXT>"    ==  6
 * "</TEXT>"   ==  7
 *               ----
 *                13
 */
#define MSN_PAGE_BASE_SIZE 13

MsnPage *
msn_page_new(void)
{
	MsnPage *page;

	page = g_new0(MsnPage, 1);

	page->size = MSN_PAGE_BASE_SIZE;

	return page;
}

MsnPage *
msn_page_new_from_str(MsnSession *session, const char *str)
{
	g_return_val_if_fail(str != NULL, NULL);

	return NULL;
}

void
msn_page_destroy(MsnPage *page)
{
	g_return_if_fail(page != NULL);

	if (page->sender != NULL)
		msn_user_unref(page->sender);

	if (page->receiver != NULL)
		msn_user_unref(page->receiver);

	if (page->body != NULL)
		g_free(page->body);

	if (page->from_location != NULL)
		g_free(page->from_location);

	if (page->from_phone != NULL)
		g_free(page->from_phone);

	g_free(page);
}

char *
msn_page_build_string(const MsnPage *page)
{
	char *page_start;
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
	g_return_val_if_fail(page != NULL, NULL);

	if (msn_page_is_incoming(page)) {
		/* We don't know this yet :) */
		return NULL;
	}
	else {
		MsnUser *receiver = msn_page_get_receiver(page);

		g_snprintf(buf, sizeof(buf), "PAG %d %s %d\r\n",
				   msn_page_get_transaction_id(page),
				   msn_user_get_passport(receiver),
				   page->size);
	}

	len = strlen(buf) + page->size + 1;

	str = g_new0(char, len);

	g_strlcpy(str, buf, len);

	page_start = str + strlen(str);

	g_snprintf(buf, sizeof(buf), "<TEXT>%s</TEXT>", msn_page_get_body(page));

	g_strlcat(str, buf, len);

	if (page->size != strlen(page_start)) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "Outgoing page size (%d) and string length (%d) "
				   "do not match!\n", page->size, strlen(page_start));
	}

	return str;
}

gboolean
msn_page_is_outgoing(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, FALSE);

	return !page->incoming;
}

gboolean
msn_page_is_incoming(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, FALSE);

	return page->incoming;
}

void
msn_page_set_sender(MsnPage *page, MsnUser *user)
{
	g_return_if_fail(page != NULL);
	g_return_if_fail(user != NULL);

	page->sender = user;
	
	msn_user_ref(page->sender);
}

MsnUser *
msn_page_get_sender(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, NULL);

	return page->sender;
}

void
msn_page_set_receiver(MsnPage *page, MsnUser *user)
{
	g_return_if_fail(page != NULL);
	g_return_if_fail(user != NULL);

	page->receiver = user;
	
	msn_user_ref(page->receiver);
}

MsnUser *
msn_page_get_receiver(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, NULL);

	return page->receiver;
}

void
msn_page_set_transaction_id(MsnPage *page, unsigned int tid)
{
	g_return_if_fail(page != NULL);
	g_return_if_fail(tid > 0);

	page->trId = tid;
}

unsigned int
msn_page_get_transaction_id(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, 0);

	return page->trId;
}

void
msn_page_set_body(MsnPage *page, const char *body)
{
	g_return_if_fail(page != NULL);
	g_return_if_fail(body != NULL);

	if (page->body != NULL) {
		page->size -= strlen(page->body);
		g_free(page->body);
	}

	page->body = g_strdup(body);

	page->size += strlen(body);
}

const char *
msn_page_get_body(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, NULL);

	return page->body;
}

