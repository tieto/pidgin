/**
 * @file mdns_cache.c Multicast DNS resource record caching code.
 *
 * gaim
 *
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
 *
 */

#include "internal.h"

#include "mdns.h"
#include "mdns_cache.h"

GSList *rrs = NULL;

static ResourceRecord *
mdns_cache_find(gchar *name, unsigned short type)
{
	ResourceRecord *rr;
	GSList *cur;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail((type != 0) || (type != RENDEZVOUS_RRTYPE_ALL), NULL);

	for (cur = rrs; cur != NULL; cur = cur->next) {
		rr = cur->data;
		if ((type == rr->type) && (!strcmp(name, rr->name)))
			return rr;
	}

	return NULL;
}

void
mdns_cache_add(const ResourceRecord *rr)
{
	g_return_if_fail(rr != NULL);
	g_return_if_fail((rr->type != 0) && (rr->type != RENDEZVOUS_RRTYPE_ALL));

	mdns_cache_remove(rr->name, rr->type);

	new = mdns_copy_rr(rr);
	rrs = g_slist_prepend(rrs, new);
}

void
mdns_cache_remove(gchar *name, unsigned short type)
{
	ResourceRecord *rr;

	g_return_if_fail(name != NULL);
	g_return_if_fail((type != 0) && (type != RENDEZVOUS_RRTYPE_ALL));

	rr = mdns_cache_find(name, type);
	if (rr == NULL)
		return;

	rrs = g_slist_remove(rrs, rr);
	mdns_free_rr(rr);
}

void mdns_cache_remove_all()
{
	while (resourcerecords != NULL)
		mdns_cache_remove(resourcerecords->data);
	rrs = NULL;
}

void mdns_cache_respond(int fd, Question *q)
{
	GSList *slist;
	ResourceRecord *cur;

	g_return_if_fail(q != NULL);

	for (slist = rrs; slist != NULL; slist = slist->next) {
		cur = slist->data;
		if (((q->type == RENDEZVOUS_RRTYPE_ALL) || (q->type == cur->type)) && (!strcmp(q->name, cur->name))) {
			mdns_send_rr(fd, cur);
		}
	}
}
