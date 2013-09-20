/*
 * An example plugin that demonstrates exporting of a cipher object
 * type to be used in another plugin.
 *
 * Copyright (C) 2013, Ankit Vani <a@nevitus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifndef _CAESAR_CIPHER_H_
#define _CAESAR_CIPHER_H_

#include "cipher.h"
#include "plugins.h"

#define CAESAR_TYPE_CIPHER             (caesar_cipher_get_type())
#define CAESAR_CIPHER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), CAESAR_TYPE_CIPHER, CaesarCipher))
#define CAESAR_CIPHER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), CAESAR_TYPE_CIPHER, CaesarCipherClass))
#define CAESAR_IS_CIPHER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), CAESAR_TYPE_CIPHER))
#define CAESAR_IS_CIPHER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), CAESAR_TYPE_CIPHER))
#define CAESAR_CIPHER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), CAESAR_TYPE_CIPHER, CaesarCipherClass))

typedef struct _CaesarCipher CaesarCipher;
typedef struct _CaesarCipherClass CaesarCipherClass;

/*
 * A caesar cipher object.
 *
 * The Caesar cipher is one of the simplest and most widely known encryption
 * techniques. It is a type of substitution cipher in which each letter in the
 * plaintext is replaced by a letter some fixed number of positions down the
 * alphabet.
 */
struct _CaesarCipher
{
	PurpleCipher gparent;
};

/* The base class for all CaesarCipher's. */
struct _CaesarCipherClass
{
	PurpleCipherClass gparent;
};

/*
 * Returns the GType for the CaesarCipher object.
 */
G_MODULE_EXPORT GType caesar_cipher_get_type(void);

/*
 * Creates a new CaesarCipher instance and returns it.
 */
G_MODULE_EXPORT PurpleCipher *caesar_cipher_new(void);

#endif /* _CAESAR_CIPHER_H_ */
