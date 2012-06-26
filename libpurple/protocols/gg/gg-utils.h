/**
 * @file gg-utils.h
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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

#ifndef _PURPLE_GG_UTILS_H
#define _PURPLE_GG_UTILS_H

#include "internal.h"

#include "plugin.h"
#include "version.h"
#include "notify.h"
#include "status.h"
#include "blist.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "request.h"

#include "gg.h"

#define GGP_UIN_LEN_MAX 10

/**
 * Convert a base 10 string to a UIN.
 *
 * @param str The string to convert
 *
 * @return UIN or 0 if an error occurred.
 */
uin_t
ggp_str_to_uin(const char *str);

const char * ggp_uin_to_str(uin_t uin);

/**
 * Calculate size of a NULL-terminated array.
 *
 * @param array The array.
 *
 * @return Size of the array.
 */
unsigned int
ggp_array_size(char **array);

/**
 * Convert enconding of a given string.
 *
 * @param locstr Input string.
 * @param encsrc Current encoding of the string.
 * @param encdst Target encoding of the string.
 *
 * @return Converted string (it must be g_free()ed when not used. Or NULL if
 * locstr is NULL.
 */
char *
charset_convert(const gchar *locstr, const char *encsrc, const char *encdst);

/**
 * Get UIN of a given account.
 *
 * @param account Current account.
 *
 * @return UIN of an account.
 */
uin_t
ggp_get_uin(PurpleAccount *account);

/**
 * Returns the best name of a buddy from the buddylist.
 *
 * @param gc  PurpleConnection instance.
 * @param uin UIN of the buddy.
 *
 * @return Name of the buddy, or UIN converted to string.
 */
char *
ggp_buddy_get_name(PurpleConnection *gc, const uin_t uin);

/**
 * Manages the display of account's status in the buddylist.
 *
 * @param account Current account.
 */
void
ggp_status_fake_to_self(PurpleAccount *account);


/**
 * Adds an input handler in purple event loop for http request.
 *
 * @see purple_input_add
 *
 * @param http_req  Http connection to watch.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle (will be greater than 0).
 */
guint
ggp_http_input_add(struct gg_http *http_req, PurpleInputFunction func,
	gpointer user_data);

#endif /* _PURPLE_GG_UTILS_H */

/* vim: set ts=8 sts=0 sw=8 noet: */
