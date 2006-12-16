/**
 * @file gg-utils.h
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GAIM_GG_UTILS_H
#define _GAIM_GG_UTILS_H

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


/*
 * Convert a base 10 string to a UIN.
 *
 * @param str The string to convert
 * @return    UIN or 0 if an error occurred.
 */
uin_t
ggp_str_to_uin(const char *str);

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
ggp_get_uin(GaimAccount *account);

/**
 * Returns the best name of a buddy from the buddylist.
 *
 * @param gc  GaimConnection instance.
 * @param uin UIN of the buddy.
 *
 * @return Name of the buddy, or UIN converted to string.
 */
char *
ggp_buddy_get_name(GaimConnection *gc, const uin_t uin);

#endif /* _GAIM_GG_UTILS_H */

/* vim: set ts=8 sts=0 sw=8 noet: */
