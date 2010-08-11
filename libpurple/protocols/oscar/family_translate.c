/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

/*
 * Family 0x000c - Translation.
 *
 * I have no idea why this group was issued.  I have never seen anything
 * that uses it.  From what I remember, the last time I tried to poke at
 * the server with this group, it whined about not supporting it.
 *
 * But we advertise it anyway, because its fun.
 *
 */

#include "oscar.h"

int translate_modfirst(OscarData *od, aim_module_t *mod)
{

	mod->family = SNAC_FAMILY_TRANSLATE;
	mod->version = 0x0001;
	mod->toolid = 0x0104;
	mod->toolversion = 0x0001;
	mod->flags = 0;
	strncpy(mod->name, "translate", sizeof(mod->name));
	mod->snachandler = NULL;

	return 0;
}
