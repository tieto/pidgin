/**
 * @file gtknickcolors.h GTK+ Conversation API
 * @ingroup pidgin
 */

/* pidgin
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#ifndef _PIDGINNICKCOLORS_H_
#define _PIDGINNICKCOLORS_H_

static const GdkColor nick_seed_colors[] = {
	{0, 64764, 59881, 20303},       /* Butter #1 */
	{0, 60909, 54484, 0},           /* Butter #2 */
	{0, 50372, 41120, 0},           /* Butter #3 */
	{0, 64764, 44975, 15934},       /* Orange #1 */
	{0, 62965, 31097, 0},           /* Orange #2 */
	{0, 52942, 23644, 0},           /* Orange #3 */
	{0, 59811, 47545, 28270},       /* Chocolate #1 */
	{0, 49601, 32125, 4369},        /* Chocolate #2 */
	{0, 36751, 22873, 514},         /* Chocolate #3 */
	{0, 35466, 58082,  13364},      /* Chameleon #1 */
	{0, 29555, 53970, 5654},        /* Chameleon #2 */
	{0, 20046, 39578, 1542},        /* Chameleon #3 */
	{0, 29289, 40863, 53199},       /* Sky Blue #1 */
	{0, 13364, 25957, 42148},       /* Sky Blue #2 */
	{0, 8224, 19018, 34695},        /* Sky Blue #3 */
	{0, 44461, 32639, 43167},       /* Plum #1 */
	{0, 30069, 20560, 31611},       /* Plum #2 */
	{0, 23644, 13621, 26214},       /* Plum #3 */
	{0, 61423, 10537,  10537},      /* Scarlet Red #1 */
	{0, 52428, 0, 0},               /* Scarlet Red #2 */
	{0, 42148, 0, 0},               /* Scarlet Red #3 */
	{0,  34952, 35466,  34181},     /* Aluminium #4*/
	{0,  21845, 22359,  21331},     /* Aluminium #5*/
	{0,  11822, 13364,  13878}      /* Aluminium #6*/
};

#define NUM_NICK_SEED_COLORS (sizeof(nick_seed_colors) / sizeof(nick_seed_colors[0]))

#endif
