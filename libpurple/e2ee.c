/**
 * @file e2ee.c End-to-end encryption API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "e2ee.h"

#include "debug.h"

struct _PurpleE2eeState
{
	PurpleE2eeProvider *provider;

	gchar *name;
	gchar *stock_icon;

	guint ref_count;
};

struct _PurpleE2eeProvider
{
	gchar *name;
	PurpleE2eeFeatures features;
	PurpleE2eeState *default_state;
};

static PurpleE2eeProvider *main_provider = NULL;

/*** Encryption states for conversations. *************************************/

PurpleE2eeState *
purple_e2ee_state_new(PurpleE2eeProvider *provider)
{
	PurpleE2eeState *state;

	g_return_val_if_fail(provider != NULL, NULL);

	state = g_new0(PurpleE2eeState, 1);
	state->provider = provider;
	state->ref_count = 1;

	return state;
}

void
purple_e2ee_state_ref(PurpleE2eeState *state)
{
	g_return_if_fail(state != NULL);

	state->ref_count++;
}

PurpleE2eeState *
purple_e2ee_state_unref(PurpleE2eeState *state)
{
	if (state == NULL)
		return NULL;

	state->ref_count--;
	if (state->ref_count > 0)
		return state;

	g_free(state->name);
	g_free(state->stock_icon);
	g_free(state);

	return NULL;
}

PurpleE2eeProvider *
purple_e2ee_state_get_provider(PurpleE2eeState *state)
{
	g_return_val_if_fail(state != NULL, NULL);

	return state->provider;
}

void
purple_e2ee_state_set_name(PurpleE2eeState *state, const gchar *name)
{
	g_return_if_fail(state != NULL);
	g_return_if_fail(name != NULL);

	g_free(state->name);
	state->name = g_strdup(name);
}

const gchar *
purple_e2ee_state_get_name(PurpleE2eeState *state)
{
	g_return_val_if_fail(state != NULL, NULL);

	return state->name;
}

void
purple_e2ee_state_set_stock_icon(PurpleE2eeState *state,
	const gchar *stock_icon)
{
	g_return_if_fail(state != NULL);
	g_return_if_fail(stock_icon != NULL);

	g_free(state->stock_icon);
	state->stock_icon = g_strdup(stock_icon);
}

const gchar *
purple_e2ee_state_get_stock_icon(PurpleE2eeState *state)
{
	g_return_val_if_fail(state, NULL);

	return state->stock_icon;
}

/*** Encryption providers API. ************************************************/

PurpleE2eeProvider *
purple_e2ee_provider_new(PurpleE2eeFeatures features)
{
	PurpleE2eeProvider *provider;

	provider = g_new0(PurpleE2eeProvider, 1);
	provider->features = features;

	return provider;
}

void
purple_e2ee_provider_free(PurpleE2eeProvider *provider)
{
	g_return_if_fail(provider != NULL);

	if (provider == main_provider) {
		purple_debug_error("e2ee", "This provider is still registered");
		return;
	}

	g_free(provider->name);
	g_free(provider);
}

gboolean
purple_e2ee_provider_register(PurpleE2eeProvider *provider)
{
	g_return_val_if_fail(provider != NULL, FALSE);

	if (main_provider != NULL)
		return FALSE;

	main_provider = provider;
	return TRUE;
}

void
purple_e2ee_provider_unregister(PurpleE2eeProvider *provider)
{
	g_return_if_fail(provider != NULL);

	if (main_provider != provider) {
		purple_debug_warning("e2ee", "This provider is not registered");
		return;
	}

	main_provider = NULL;
}

PurpleE2eeProvider *
purple_e2ee_provider_get_main(void)
{
	return main_provider;
}

PurpleE2eeFeatures
purple_e2ee_provider_get_features(PurpleE2eeProvider *provider)
{
	g_return_val_if_fail(provider != NULL, 0);

	return provider->features;
}

void
purple_e2ee_provider_set_name(PurpleE2eeProvider *provider, const gchar *name)
{
	g_return_if_fail(provider != NULL);
	g_return_if_fail(name != NULL);

	g_free(provider->name);
	provider->name = g_strdup(name);
}

const gchar *
purple_e2ee_provider_get_name(PurpleE2eeProvider *provider)
{
	g_return_val_if_fail(provider != NULL, NULL);

	return provider->name;
}

void
purple_e2ee_provider_set_default_state(PurpleE2eeProvider *provider,
	PurpleE2eeState *state)
{
	g_return_if_fail(provider != NULL);
	g_return_if_fail(state != NULL);

	provider->default_state = state;
}

PurpleE2eeState *
purple_e2ee_provider_get_default_state(PurpleE2eeProvider *provider)
{
	g_return_val_if_fail(provider != NULL, NULL);

	return provider->default_state;
}
