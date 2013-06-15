/**
 * @file des.h Purple DES Cipher
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef PURPLE_DES_CIPHER_H
#define PURPLE_DES_CIPHER_H

#include "cipher.h"

#define PURPLE_TYPE_DES_CIPHER            (purple_des_cipher_get_type())
#define PURPLE_DES_CIPHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_DES_CIPHER, PurpleDESCipher))
#define PURPLE_DES_CIPHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_DES_CIPHER, PurpleDESCipherClass))
#define PURPLE_IS_DES_CIPHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_DES_CIPHER))
#define PURPLE_IS_DES_CIPHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_DES_CIPHER))
#define PURPLE_DES_CIPHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_DES_CIPHER, PurpleDESCipherClass))

typedef struct _PurpleDESCipher           PurpleDESCipher;
typedef struct _PurpleDESCipherClass      PurpleDESCipherClass;

struct _PurpleDESCipher {
	/*< private >*/
	PurpleCipher gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

struct _PurpleDESCipherClass {
	/*< private >*/
	PurpleCipherClass gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_des_cipher_get_type(void);

PurpleCipher *purple_des_cipher_new(void);

int purple_des_cipher_ecb_crypt(PurpleDESCipher *des_cipher, const guint8 * from, guint8 * to, int mode);

G_END_DECLS

#endif /* PURPLE_DES_CIPHER_H */
