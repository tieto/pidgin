/**
 * @file page.h Paging functions
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
#ifndef _MSN_PAGE_H_
#define _MSN_PAGE_H_

typedef struct _MsnPage MsnPage;

#include "session.h"
#include "user.h"

/**
 * A page.
 */
struct _MsnPage
{
	MsnUser *sender;
	MsnUser *receiver;

	char *from_location;
	char *from_phone;

	gboolean incoming;

	unsigned int trId;

	size_t size;

	char *body;
};

/**
 * Creates a new, empty page.
 *
 * @return A new page.
 */
MsnPage *msn_page_new(void);

/**
 * Creates a new page based off a string.
 *
 * @param session The MSN session.
 * @param str     The string.
 *
 * @return The new page.
 */
MsnPage *msn_page_new_from_str(MsnSession *session, const char *str);

/**
 * Destroys a page.
 */
void msn_page_destroy(MsnPage *page);

/**
 * Converts a page to a string.
 *
 * @param page The page.
 *
 * @return The string representation of a page.
 */
char *msn_page_build_string(const MsnPage *page);

/**
 * Returns TRUE if the page is outgoing.
 *
 * @param page The page.
 *
 * @return @c TRUE if the page is outgoing, or @c FALSE otherwise.
 */
gboolean msn_page_is_outgoing(const MsnPage *page);

/**
 * Returns TRUE if the page is incoming.
 *
 * @param page The page.
 *
 * @return @c TRUE if the page is incoming, or @c FALSE otherwise.
 */
gboolean msn_page_is_incoming(const MsnPage *page);

/**
 * Sets the page's sender.
 *
 * @param page  The page.
 * @param user The sender.
 */
void msn_page_set_sender(MsnPage *page, MsnUser *user);

/**
 * Returns the page's sender.
 *
 * @param page The page.
 *
 * @return The sender.
 */
MsnUser *msn_page_get_sender(const MsnPage *page);

/**
 * Sets the page's receiver.
 *
 * @param page  The page.
 * @param user The receiver.
 */
void msn_page_set_receiver(MsnPage *page, MsnUser *user);

/**
 * Returns the page's receiver.
 *
 * @param page The page.
 *
 * @return The receiver.
 */
MsnUser *msn_page_get_receiver(const MsnPage *page);

/**
 * Sets the page transaction ID.
 *
 * @param page The page.
 * @param tid  The transaction ID.
 */
void msn_page_set_transaction_id(MsnPage *page, unsigned int tid);

/**
 * Returns the page transaction ID.
 *
 * @param page The page.
 *
 * @return The transaction ID.
 */
unsigned int msn_page_get_transaction_id(const MsnPage *page);


/**
 * Sets the body of a page.
 *
 * @param page  The page.
 * @param body The body of the page.
 */
void msn_page_set_body(MsnPage *page, const char *body);

/**
 * Returns the body of the page.
 *
 * @param page The page.
 *
 * @return The body of the page.
 */
const char *msn_page_get_body(const MsnPage *page);

#endif /* _MSN_PAGE_H_ */
