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
#include "debug.h"

#include "mdns.h"

GSList *resourcerecords = NULL;

void mdns_cache_add(ResourceRecord *rr)
{
	g_return_if_fail(rr != NULL);

	resourcerecords = g_slist_prepend(resourcerecords, rr);
}

void mdns_cache_remove(ResourceRecord *rr)
{
	g_return_if_fail(rr != NULL);

	resourcerecords = g_slist_remove_all(resourcerecords, rr);

	mdns_free_rr(rr);
}

void mdns_cache_remove_all()
{
	while (resourcerecords != NULL)
		mdns_cache_remove(resourcerecords->data);
}

void mdns_cache_respond(int fd, Question *q)
{
	GSList *slist;
	ResourceRecord *cur;

	g_return_if_fail(q != NULL);

	for (slist = resourcerecords; slist != NULL; slist = g_slist_next(slist)) {
		cur = slist->data;
		if ((q->type == cur->type) && (!strcmp(q->name, cur->name)))
			mdns_send_rr(fd, cur);
	}
}
