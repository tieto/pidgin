/*
 * Gaim's oscar protocol plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "oscar.h"

typedef struct _SnacHandler SnacHandler;

struct _SnacHandler
{
	guint16 family;
	guint16 subtype;
	aim_rxcallback_t handler;
	guint16 flags;
};

/**
 * Allocates a new OscarData and initializes it with default values.
 */
OscarData *
oscar_data_new(void)
{
	OscarData *od;

	od = g_new0(OscarData, 1);

	aim_initsnachash(od);
	od->snacid_next = 0x00000001;
	od->buddyinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	od->handlerlist = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

	/*
	 * Register all the modules for this session...
	 */
	aim__registermodule(od, misc_modfirst); /* load the catch-all first */
	aim__registermodule(od, service_modfirst);
	aim__registermodule(od, locate_modfirst);
	aim__registermodule(od, buddylist_modfirst);
	aim__registermodule(od, msg_modfirst);
	aim__registermodule(od, adverts_modfirst);
	aim__registermodule(od, invite_modfirst);
	aim__registermodule(od, admin_modfirst);
	aim__registermodule(od, popups_modfirst);
	aim__registermodule(od, bos_modfirst);
	aim__registermodule(od, search_modfirst);
	aim__registermodule(od, stats_modfirst);
	aim__registermodule(od, translate_modfirst);
	aim__registermodule(od, chatnav_modfirst);
	aim__registermodule(od, chat_modfirst);
	aim__registermodule(od, odir_modfirst);
	aim__registermodule(od, bart_modfirst);
	/* missing 0x11 - 0x12 */
	aim__registermodule(od, ssi_modfirst);
	/* missing 0x14 */
	aim__registermodule(od, icq_modfirst);
	/* missing 0x16 */
	aim__registermodule(od, auth_modfirst);
	aim__registermodule(od, email_modfirst);

	return od;
}

/**
 * Logoff and deallocate a session.
 *
 * @param od Session to kill
 */
void
oscar_data_destroy(OscarData *od)
{
	aim_cleansnacs(od, -1);

	while (od->requesticon)
	{
		gchar *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		g_free(sn);
	}
	g_free(od->email);
	g_free(od->newp);
	g_free(od->oldp);
	if (od->icontimer > 0)
		gaim_timeout_remove(od->icontimer);
	if (od->getblisttimer > 0)
		gaim_timeout_remove(od->getblisttimer);
	if (od->getinfotimer > 0)
		gaim_timeout_remove(od->getinfotimer);
	while (od->oscar_connections != NULL)
		flap_connection_destroy(od->oscar_connections->data,
				OSCAR_DISCONNECT_DONE, NULL);

	while (od->peer_connections != NULL)
		peer_connection_destroy(od->peer_connections->data,
				OSCAR_DISCONNECT_LOCAL_CLOSED, NULL);

	aim__shutdownmodules(od);

	g_hash_table_destroy(od->buddyinfo);
	g_hash_table_destroy(od->handlerlist);

	g_free(od);
}

void
oscar_data_addhandler(OscarData *od, guint16 family, guint16 subtype, aim_rxcallback_t newhandler, guint16 flags)
{
	SnacHandler *snac_handler;

	gaim_debug_misc("oscar", "Adding handler for %04x/%04x\n", family, subtype);

	snac_handler = g_new0(SnacHandler, 1);

	snac_handler->family = family;
	snac_handler->subtype = subtype;
	snac_handler->flags = flags;
	snac_handler->handler = newhandler;

	g_hash_table_insert(od->handlerlist,
			GUINT_TO_POINTER((family << 16) + subtype),
			snac_handler);
}

aim_rxcallback_t
aim_callhandler(OscarData *od, guint16 family, guint16 subtype)
{
	SnacHandler *snac_handler;

	snac_handler = g_hash_table_lookup(od->handlerlist, GUINT_TO_POINTER((family << 16) + subtype));

	return snac_handler ? snac_handler->handler : NULL;
}
