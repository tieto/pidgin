/**
 * @file ntlm.h
 */

/* purple
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
 *
 * ntlm structs are taken from NTLM description on
 * http://www.innovation.ch/java/ntlm.html
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

#ifndef _PURPLE_NTLM_H
#define _PURPLE_NTLM_H

G_BEGIN_DECLS

/**
 * purple_ntlm_gen_type1:
 * @hostname: Your hostname
 * @domain: The domain to authenticate to
 *
 * Generates the base64 encoded type 1 message needed for NTLM authentication
 *
 * Returns: base64 encoded string to send to the server.  This should
 *         be g_free'd by the caller.
 */
gchar *purple_ntlm_gen_type1(const gchar *hostname, const gchar *domain);

/**
 * purple_ntlm_parse_type2:
 * @type2: String containing the base64 encoded type2 message
 * @flags: If not %NULL, this will store the flags for the message
 *
 * Parses the ntlm type 2 message
 *
 * Returns: The nonce for use in message type3.  This is a statically
 *         allocated 8 byte binary string.
 */
guint8 *purple_ntlm_parse_type2(const gchar *type2, guint32 *flags);

/**
 * purple_ntlm_gen_type3:
 * @username: The username
 * @passw: The password
 * @hostname: The hostname
 * @domain: The domain to authenticate against
 * @nonce: The nonce returned by purple_ntlm_parse_type2
 * @flags: Pointer to the flags returned by purple_ntlm_parse_type2
 *
 * Generates a type3 message
 *
 * Returns: A base64 encoded type3 message.  This should be g_free'd by
 *          the caller.
 */
gchar *purple_ntlm_gen_type3(const gchar *username, const gchar *passw, const gchar *hostname, const gchar *domain, const guint8 *nonce, guint32 *flags);

G_END_DECLS

#endif /* _PURPLE_NTLM_H */
