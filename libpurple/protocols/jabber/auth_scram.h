/**
 * @file auth_scram.h Implementation of SASL-SCRAM authentication
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef PURPLE_JABBER_AUTH_SCRAM_H_
#define PURPLE_JABBER_AUTH_SCRAM_H_

/**
 * Implements the Hi() function as described in the SASL-SCRAM I-D.
 *
 * @param hash The name of a hash function to be used with HMAC.  This should
 *             be suitable to be passed to the libpurple cipher API.  Typically
 *             it will be "sha1".
 * @param str  The string to perform the PBKDF2 operation on.
 * @param salt The salt.
 * @param iterations The number of iterations to perform.
 *
 * @returns A newly allocated string containing the result.
 */
GString *jabber_auth_scram_hi(const char *hash, const GString *str,
                              GString *salt, guint iterations);

#endif /* PURPLE_JABBER_AUTH_SCRAM_H_ */
