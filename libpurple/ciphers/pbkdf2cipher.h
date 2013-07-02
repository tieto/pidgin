/**
 * @file pbkdf2.h Purple PBKDF2 Cipher
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
#ifndef PURPLE_PBKDF2_CIPHER_H
#define PURPLE_PBKDF2_CIPHER_H

#include "cipher.h"
#include "hash.h"

#define PURPLE_TYPE_PBKDF2_CIPHER				(purple_pbkdf2_cipher_get_gtype())
#define PURPLE_PBKDF2_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PBKDF2_CIPHER, PurplePBKDF2Cipher))
#define PURPLE_PBKDF2_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PBKDF2_CIPHER, PurplePBKDF2CipherClass))
#define PURPLE_IS_PBKDF2_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PBKDF2_CIPHER))
#define PURPLE_IS_PBKDF2_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_PBKDF2_CIPHER))
#define PURPLE_PBKDF2_CIPHER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PBKDF2_CIPHER, PurplePBKDF2CipherClass))

typedef struct _PurplePBKDF2Cipher			PurplePBKDF2Cipher;
typedef struct _PurplePBKDF2CipherClass		PurplePBKDF2CipherClass;

struct _PurplePBKDF2Cipher {
	/*< private >*/
	PurpleCipher gparent;
};

struct _PurplePBKDF2CipherClass {
	/*< private >*/
	PurpleCipherClass gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_pbkdf2_cipher_get_gtype(void);

PurpleCipher *purple_pbkdf2_cipher_new(PurpleHash *hash);

PurpleHash *purple_pbkdf2_cipher_get_hash(const PurplePBKDF2Cipher *cipher);

G_END_DECLS

#endif /* PURPLE_PBKDF2_CIPHER_H */

