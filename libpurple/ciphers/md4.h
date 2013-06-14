/**
 * @file md4.h Purple MD4 hash
 * @ingroup core
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef PURPLE_MD4_CIPHER_H
#define PURPLE_MD4_CIPHER_H

#include "cipher.h"

#define PURPLE_TYPE_MD4_CIPHER				(purple_md4_cipher_get_gtype())
#define PURPLE_MD4_CIPHER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MD4_CIPHER, PurpleMD4Cipher))
#define PURPLE_MD4_CIPHER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MD4_CIPHER, PurpleMD4CipherClass))
#define PURPLE_IS_MD4_CIPHER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MD4_CIPHER))
#define PURPLE_IS_MD4_CIPHER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_MD4_CIPHER))
#define PURPLE_MD4_CIPHER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MD4_CIPHER, PurpleMD4CipherClass))

typedef struct _PurpleMD4Cipher				PurpleMD4Cipher;
typedef struct _PurpleMD4CipherClass		PurpleMD4CipherClass;

struct _PurpleMD4Cipher {
	PurpleCipher parent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

struct _PurpleMD4CipherClass {
	PurpleCipherClass parent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_md4_cipher_get_gtype(void);

PurpleCipher *purple_md4_cipher_new(void);

G_END_DECLS

#endif /* PURPLE_MD4_CIPHER_H */
