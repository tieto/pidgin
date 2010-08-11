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
 * Family 0x0005 - Advertisements.
 *
 */

#include "oscar.h"

void
aim_ads_requestads(OscarData *od, FlapConnection *conn)
{
	aim_genericreq_n(od, conn, SNAC_FAMILY_ADVERT, 0x0002);
}

static int snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	return 0;
}

int adverts_modfirst(OscarData *od, aim_module_t *mod)
{

	mod->family = SNAC_FAMILY_ADVERT;
	mod->version = 0x0001;
	mod->toolid = 0x0001;
	mod->toolversion = 0x0001;
	mod->flags = 0;
	strncpy(mod->name, "advert", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
