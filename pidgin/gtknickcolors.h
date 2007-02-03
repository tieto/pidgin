/**
 * @file gtknickcolors.h GTK+ Conversation API
 * @ingroup gtkui
 *
 * gaim
 * Gaim is the legal property of its developers, whose names are too numerous
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
#ifndef _PIDGINNICKCOLORS_H_
#define _PIDGINNICKCOLORS_H_

static GdkColor nick_seed_colors[] = {
    {0, 47616, 46336, 43776},       /* Basic 3D Medium */
    {0, 32768, 32000, 29696},       /* Basic 3D Dark */
    {0, 22016, 20992, 18432},       /* 3D Shadow */
    {0, 33536, 42496, 32512},       /* Green Medium */
    {0, 23808, 29952, 21760},       /* Green Dark */
    {0, 17408, 22016, 12800},       /* Green Shadow */
    {0, 57344, 46592, 44800},       /* Red Hilight */
    {0, 49408, 26112, 23040},       /* Red Medium */
    {0, 34816, 17920, 12544},       /* Red Dark */
    {0, 49408, 14336,  8704},       /* Red Shadow */
    {0, 34816, 32512, 41728},       /* Purple Medium */
    {0, 25088, 23296, 33024},       /* Purple Dark */
    {0, 18688, 16384, 26112},       /* Purple Shadow */
    {0, 40192, 47104, 53760},       /* Blue Hilight */
    {0, 29952, 36864, 44544},       /* Blue Medium */
    {0, 57344, 49920, 40448},       /* Face Skin Medium */
    {0, 45824, 37120, 26880},       /* Face skin Dark */
    {0, 33280, 26112, 18176},       /* Face Skin Shadow */
    {0, 57088, 16896,  7680},       /* Accent Red */
    {0, 39168,     0,     0},       /* Accent Red Dark */
    {0, 17920, 40960, 17920},       /* Accent Green */
    {0,  9728, 50944,  9728}        /* Accent Green Dark */
};

#define NUM_NICK_SEED_COLORS (sizeof(nick_seed_colors) / sizeof(nick_seed_colors[0]))

#endif
