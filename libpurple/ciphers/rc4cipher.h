/**
 * @file rc4.h Purple RC4 Cipher
 * @ingroup core
 */

/* purple
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
#ifndef PURPLE_RC4_CIPHER_H
#define PURPLE_RC4_CIPHER_H

#include "cipher.h"

#define PURPLE_TYPE_RC4_CIPHER				(purple_rc4_cipher_get_gtype())
#define PURPLE_RC4_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_RC4_CIPHER, PurpleRC4Cipher))
#define PURPLE_RC4_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_RC4_CIPHER, PurpleRC4CipherClass))
#define PURPLE_IS_RC4_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_RC4_CIPHER))
#define PURPLE_IS_RC4_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_RC4_CIPHER))
#define PURPLE_RC4_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_RC4_CIPHER, PurpleRC4CipherClass))

typedef struct _PurpleRC4Cipher				PurpleRC4Cipher;
typedef struct _PurpleRC4CipherClass		PurpleRC4CipherClass;

struct _PurpleRC4Cipher {
	/*< private >*/
	PurpleCipher gparent;
};

struct _PurpleRC4CipherClass {
	/*< private >*/
	PurpleCipherClass gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_rc4_cipher_get_gtype(void);

PurpleCipher *purple_rc4_cipher_new(void);

gint purple_rc4_cipher_get_key_len(PurpleRC4Cipher *rc4_cipher);
void purple_rc4_cipher_set_key_len(PurpleRC4Cipher *rc4_cipher, gint key_len);

G_END_DECLS

#endif /* PURPLE_RC4_CIPHER_H */


