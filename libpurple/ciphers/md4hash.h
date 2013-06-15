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
#ifndef PURPLE_MD4_HASH_H
#define PURPLE_MD4_HASH_H

#include "hash.h"

#define PURPLE_TYPE_MD4_HASH				(purple_md4_hash_get_gtype())
#define PURPLE_MD4_HASH(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MD4_HASH, PurpleMD4Hash))
#define PURPLE_MD4_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MD4_HASH, PurpleMD4HashClass))
#define PURPLE_IS_MD4_HASH(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MD4_HASH))
#define PURPLE_IS_MD4_HASH_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((obj), PURPLE_TYPE_MD4_HASH))
#define PURPLE_MD4_HASH_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MD4_HASH, PurpleMD4HashClass))

typedef struct _PurpleMD4Hash				PurpleMD4Hash;
typedef struct _PurpleMD4HashClass			PurpleMD4HashClass;

struct _PurpleMD4Hash {
	PurpleHash parent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

struct _PurpleMD4HashClass {
	PurpleHashClass parent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

GType purple_md4_hash_get_gtype(void);

PurpleHash *purple_md4_hash_new(void);

G_END_DECLS

#endif /* PURPLE_MD4_HASH_H */
