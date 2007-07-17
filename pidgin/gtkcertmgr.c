/*
 * @file gtkcertmgr.c GTK+ Certificate Manager API
 * @ingroup pidgin
 *
 * pidgin
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "internal.h"
#include "pidgin.h"

#include "certificate.h"
#include "debug.h"
#include "notify.h"

#include "gtkcertmgr.h"

void pidgin_certmgr_show(void)
{
	purple_notify_info(NULL, "Certificate Manager!!!1", "Certificates!!!", "LOL");
	/* Enumerate all the certificates on file */
	{
		GList *idlist, *poollist;

		for ( poollist = purple_certificate_get_pools();
		      poollist;
		      poollist = poollist->next ) {
			PurpleCertificatePool *pool = poollist->data;
			GList *l;
			
			purple_debug_info("gtkcertmgr",
					  "Pool %s found for scheme %s -"
					  "Enumerating certificates:\n",
					  pool->name, pool->scheme_name);

			idlist = purple_certificate_pool_get_idlist(pool);

			for (l=idlist; l; l = l->next) {
				purple_debug_info("gtkcertmgr",
						  "- %s\n",
						  (gchar *) l->data);
			} /* idlist */
			purple_certificate_pool_destroy_idlist(idlist);
		} /* poollist */
	}
}
