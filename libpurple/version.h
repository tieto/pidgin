/**
 * @file version.h Purple Versioning
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
#ifndef _PURPLE_VERSION_H_
#define _PURPLE_VERSION_H_

#define PURPLE_MAJOR_VERSION 2
#define PURPLE_MINOR_VERSION 0
#define PURPLE_MICRO_VERSION 0

#define PURPLE_VERSION_CHECK(x,y,z) ((x) == PURPLE_MAJOR_VERSION && ((y) < PURPLE_MINOR_VERSION || ((y) == PURPLE_MINOR_VERSION && (z) <= PURPLE_MICRO_VERSION)))

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_VERSION_H_ */

