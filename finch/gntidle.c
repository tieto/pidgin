/*
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 *
 */

#include "finch.h"
#include "gntidle.h"
#include "gntwm.h"

#include "idle.h"

static time_t
finch_get_idle_time(void)
{
	return gnt_wm_get_idle_time();
}

static PurpleIdleUiOps ui_ops =
{
	finch_get_idle_time,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleIdleUiOps *
finch_idle_get_ui_ops()
{
	return &ui_ops;
}
