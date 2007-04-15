/**
 * @file page.c Paging functions
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "msn.h"
#include "page.h"

MsnPage *
msn_page_new(void)
{
	MsnPage *page;

	page = g_new0(MsnPage, 1);

	return page;
}

void
msn_page_destroy(MsnPage *page)
{
	g_return_if_fail(page != NULL);

	if (page->body != NULL)
		g_free(page->body);

	if (page->from_location != NULL)
		g_free(page->from_location);

	if (page->from_phone != NULL)
		g_free(page->from_phone);

	g_free(page);
}

char *
msn_page_gen_payload(const MsnPage *page, size_t *ret_size)
{
	char *str;

	g_return_val_if_fail(page != NULL, NULL);

	str =
		g_strdup_printf("<TEXT xml:space=\"preserve\" enc=\"utf-8\">%s</TEXT>",
						msn_page_get_body(page));

	if (ret_size != NULL)
		*ret_size = strlen(str);

	return str;
}

void
msn_page_set_body(MsnPage *page, const char *body)
{
	g_return_if_fail(page != NULL);
	g_return_if_fail(body != NULL);

	if (page->body != NULL)
		g_free(page->body);

	page->body = g_strdup(body);
}

const char *
msn_page_get_body(const MsnPage *page)
{
	g_return_val_if_fail(page != NULL, NULL);

	return page->body;
}
