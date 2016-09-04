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
/**
 * SECTION:gtknickcolors
 * @section_id: pidgin-gtknickcolors
 * @short_description: <filename>gtknickcolors.h</filename>
 * @title: Conversation Nick Colors
 */

static const GdkRGBA nick_seed_colors[] = {
	{64764/65535.0f, 59881/65535.0f, 20303/65535.0f, 1},    /* Butter #1 */
	{60909/65535.0f, 54484/65535.0f,     0/65535.0f, 1},    /* Butter #2 */
	{50372/65535.0f, 41120/65535.0f,     0/65535.0f, 1},    /* Butter #3 */
	{64764/65535.0f, 44975/65535.0f, 15934/65535.0f, 1},    /* Orange #1 */
	{62965/65535.0f, 31097/65535.0f,     0/65535.0f, 1},    /* Orange #2 */
	{52942/65535.0f, 23644/65535.0f,     0/65535.0f, 1},    /* Orange #3 */
	{59811/65535.0f, 47545/65535.0f, 28270/65535.0f, 1},    /* Chocolate #1 */
	{49601/65535.0f, 32125/65535.0f,  4369/65535.0f, 1},    /* Chocolate #2 */
	{36751/65535.0f, 22873/65535.0f,   514/65535.0f, 1},    /* Chocolate #3 */
	{35466/65535.0f, 58082/65535.0f, 13364/65535.0f, 1},    /* Chameleon #1 */
	{29555/65535.0f, 53970/65535.0f,  5654/65535.0f, 1},    /* Chameleon #2 */
	{20046/65535.0f, 39578/65535.0f,  1542/65535.0f, 1},    /* Chameleon #3 */
	{29289/65535.0f, 40863/65535.0f, 53199/65535.0f, 1},    /* Sky Blue #1 */
	{13364/65535.0f, 25957/65535.0f, 42148/65535.0f, 1},    /* Sky Blue #2 */
	{ 8224/65535.0f, 19018/65535.0f, 34695/65535.0f, 1},    /* Sky Blue #3 */
	{44461/65535.0f, 32639/65535.0f, 43167/65535.0f, 1},    /* Plum #1 */
	{30069/65535.0f, 20560/65535.0f, 31611/65535.0f, 1},    /* Plum #2 */
	{23644/65535.0f, 13621/65535.0f, 26214/65535.0f, 1},    /* Plum #3 */
	{61423/65535.0f, 10537/65535.0f, 10537/65535.0f, 1},    /* Scarlet Red #1 */
	{52428/65535.0f,     0/65535.0f,     0/65535.0f, 1},    /* Scarlet Red #2 */
	{42148/65535.0f,     0/65535.0f,     0/65535.0f, 1},    /* Scarlet Red #3 */
	{34952/65535.0f, 35466/65535.0f, 34181/65535.0f, 1},    /* Aluminium #4*/
	{21845/65535.0f, 22359/65535.0f, 21331/65535.0f, 1},    /* Aluminium #5*/
	{11822/65535.0f, 13364/65535.0f, 13878/65535.0f, 1}     /* Aluminium #6*/
};

#define PIDGIN_NUM_NICK_SEED_COLORS (sizeof(nick_seed_colors) / sizeof(nick_seed_colors[0]))

#endif
