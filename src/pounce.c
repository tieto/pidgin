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

struct gaim_pounce *
gaim_pounce_new(struct gaim_account *pouncer, const char *pouncee,
				GaimPounceEvent event, gaim_pounce_cb cb,
				void *data, void (*free)(void *))
{
	struct gaim_pounce *pounce;

	if (pouncer == NULL || pouncee == NULL || event == 0 || cb == NULL)
		return NULL;

	pounce = g_new0(struct gaim_pounce, 1);

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
gaim_pounce_destroy(struct gaim_pounce *pounce)
{
	if (pounce == NULL)
		return;

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounces = g_list_remove(pounces, pounce);

	if (pounce->free != NULL && pounce->data != NULL)
		pounce->free(pounce->data);

	g_free(pounce);
}

void
gaim_pounce_set_events(struct gaim_pounce *pounce, GaimPounceEvent events)
{
	if (pounce == NULL || events == GAIM_POUNCE_NONE)
		return;

	pounce->events = events;
}

void
gaim_pounce_set_pouncer(struct gaim_pounce *pounce,
						struct gaim_account *pouncer)
{
	if (pounce == NULL || pouncer == NULL)
		return;

	pounce->pouncer = pouncer;
}

void
gaim_pounce_set_pouncee(struct gaim_pounce *pounce, const char *pouncee)
{
	if (pounce == NULL || pouncee == NULL)
		return;

	if (pounce->pouncee != NULL)
		g_free(pounce->pouncee);

	pounce->pouncee = (pouncee == NULL ? NULL : g_strdup(pouncee));
}

void
gaim_pounce_set_data(struct gaim_pounce *pounce, void *data)
{
	if (pounce == NULL)
		return;

	pounce->data = data;
}

GaimPounceEvent
gaim_pounce_get_events(const struct gaim_pounce *pounce)
{
	if (pounce == NULL)
		return GAIM_POUNCE_NONE;

	return pounce->events;
}

struct gaim_account *
gaim_pounce_get_pouncer(const struct gaim_pounce *pounce)
{
	if (pounce == NULL)
		return NULL;

	return pounce->pouncer;
}

const char *
gaim_pounce_get_pouncee(const struct gaim_pounce *pounce)
{
	if (pounce == NULL)
		return NULL;

	return pounce->pouncee;
}

void *
gaim_pounce_get_data(const struct gaim_pounce *pounce)
{
	if (pounce == NULL)
		return NULL;

	return pounce->data;
}

void
gaim_pounce_execute(const struct gaim_account *pouncer,
					const char *pouncee, GaimPounceEvent events)
{
	struct gaim_pounce *pounce;
	GList *l;

	if (events == GAIM_POUNCE_NONE || pouncer == NULL || pouncee == NULL)
		return;

	for (l = gaim_get_pounces(); l != NULL; l = l->next) {
		pounce = (struct gaim_pounce *)l->data;

		if ((gaim_pounce_get_events(pounce) & events) &&
			(gaim_pounce_get_pouncer(pounce) == pouncer) &&
			!strcmp(gaim_pounce_get_pouncee(pounce), pouncee)) {

			if (pounce->callback != NULL)
				pounce->callback(pounce, events, gaim_pounce_get_data(pounce));
		}
	}
}

struct gaim_pounce *
gaim_find_pounce(const struct gaim_account *pouncer,
				 const char *pouncee, GaimPounceEvent events)
{
	struct gaim_pounce *pounce;
	GList *l;

	if (events == GAIM_POUNCE_NONE || pouncer == NULL || pouncee == NULL)
		return NULL;

	for (l = gaim_get_pounces(); l != NULL; l = l->next) {
		pounce = (struct gaim_pounce *)l->data;

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
