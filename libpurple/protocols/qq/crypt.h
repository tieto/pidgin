/**
 * @file crypt.h
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

#ifndef _QQ_CRYPT_H_
#define _QQ_CRYPT_H_

#include <glib.h>

void qq_encrypt(const guint8 *const instr, gint instrlen, 
		const guint8 *const key, 
		guint8 *outstr, gint *outstrlen_ptr);
		
gint qq_decrypt(const guint8 *const instr, gint instrlen, 
		const guint8 *const key,
		guint8 *outstr, gint *outstrlen_ptr);
		
#endif
