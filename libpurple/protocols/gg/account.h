/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#ifndef _GGP_ACCOUNT_H
#define _GGP_ACCOUNT_H

#include <internal.h>

typedef struct
{
	gchar *id;
	gpointer data;
	size_t size;
	guint length;
} ggp_account_token;

/**
 * token must be free'd with ggp_account_token_free
 */
typedef void (*ggp_account_token_cb)(PurpleConnection *gc,
	ggp_account_token *token, gpointer user_data);

void ggp_account_token_request(PurpleConnection *gc,
	ggp_account_token_cb callback, void *user_data);
gboolean ggp_account_token_validate(ggp_account_token *token,
	const gchar *value);
void ggp_account_token_free(ggp_account_token *token);


void ggp_account_register(PurpleAccount *account);

void ggp_account_chpass(PurpleConnection *gc);

#endif /* _GGP_ACCOUNT_H */
