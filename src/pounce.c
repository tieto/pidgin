/**
 * @file pounce.h Buddy pounce API
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include <string.h>
#include "gaim.h"

static GList *pounces = NULL;

GaimPounce *
gaim_pounce_new(GaimAccount *pouncer, const char *pouncee,
				GaimPounceEvent event, GaimPounceCb cb,
				void *data, void (*free)(void *))
{
	GaimPounce *pounce;

	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(event   != 0,    NULL);
	g_return_val_if_fail(cb      != NULL, NULL);

	pounce = g_new0(GaimPounce, 1);

	pounce->pouncer  = pouncer;
	pounce->pouncee  = g_strdup(pouncee);
	pounce->events   = event;
	pounce->callback = cb;
	pounce->data     = data;
	pounce->free     = free;

	pounces = g_list_append(pounces, pounce);

	return pounce;
}

void
gaim_pounce_destroy(GaimPounce *pounce)
{
	g_return_if_fail(pounce != NULL);

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounces = g_list_remove(pounces, pounce);

	if (pounce->free != NULL && pounce->data != NULL)
		pounce->free(pounce->data);

	g_free(pounce);
}

void
gaim_pounce_set_events(GaimPounce *pounce, GaimPounceEvent events)
{
	g_return_if_fail(pounce != NULL);
	g_return_if_fail(pounce != GAIM_POUNCE_NONE);

	pounce->events = events;
}

void
gaim_pounce_set_pouncer(GaimPounce *pounce, GaimAccount *pouncer)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncer != NULL);

	pounce->pouncer = pouncer;
}

void
gaim_pounce_set_pouncee(GaimPounce *pounce, const char *pouncee)
{
	g_return_if_fail(pounce  != NULL);
	g_return_if_fail(pouncee != NULL);

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounce->pouncee = (pouncee == NULL ? NULL : g_strdup(pouncee));
}

void
gaim_pounce_set_data(GaimPounce *pounce, void *data)
{
	g_return_if_fail(pounce != NULL);

	pounce->data = data;
}

GaimPounceEvent
gaim_pounce_get_events(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, GAIM_POUNCE_NONE);

	return pounce->events;
}

GaimAccount *
gaim_pounce_get_pouncer(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncer;
}

const char *
gaim_pounce_get_pouncee(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->pouncee;
}

void *
gaim_pounce_get_data(const GaimPounce *pounce)
{
	g_return_val_if_fail(pounce != NULL, NULL);

	return pounce->data;
}

void
gaim_pounce_execute(const GaimAccount *pouncer, const char *pouncee,
					GaimPounceEvent events)
{
	GaimPounce *pounce;
	GList *l;

	g_return_if_fail(pouncer != NULL);
	g_return_if_fail(pouncee != NULL);
	g_return_if_fail(events  != GAIM_POUNCE_NONE);

	for (l = gaim_get_pounces(); l != NULL; l = l->next) {
		pounce = (GaimPounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!strcmp(gaim_pounce_get_pouncee(pounce), pouncee)) {

			if (pounce->callback != NULL)
				pounce->callback(pounce, events, gaim_pounce_get_data(pounce));
		}
	}
}

GaimPounce *
gaim_find_pounce(const GaimAccount *pouncer, const char *pouncee,
				 GaimPounceEvent events)
{
	GaimPounce *pounce;
	GList *l;

	g_return_val_if_fail(pouncer != NULL, NULL);
	g_return_val_if_fail(pouncee != NULL, NULL);
	g_return_val_if_fail(events  != GAIM_POUNCE_NONE, NULL);

	for (l = gaim_get_pounces(); l != NULL; l = l->next) {
		pounce = (GaimPounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!strcmp(gaim_pounce_get_pouncee(pounce), pouncee)) {

			return pounce;
		}
	}

	return NULL;
}

GList *
gaim_get_pounces(void)
{
	return pounces;
}
