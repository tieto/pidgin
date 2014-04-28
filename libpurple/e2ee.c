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
	PurpleE2eeConvMenuCallback conv_menu_cb;
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
purple_e2ee_provider_new(void)
{
	PurpleE2eeProvider *provider;

	provider = g_new0(PurpleE2eeProvider, 1);

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
	GList *it, *clear_states = NULL;
	g_return_if_fail(provider != NULL);

	if (main_provider != provider) {
		purple_debug_warning("e2ee", "This provider is not registered");
		return;
	}

	for (it = purple_conversations_get_all(); it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;
		PurpleE2eeState *state;

		state = purple_conversation_get_e2ee_state(conv);
		if (!state)
			continue;
		if (provider == purple_e2ee_state_get_provider(state))
			clear_states = g_list_prepend(clear_states, conv);
	}

	main_provider = NULL;

	for (it = clear_states; it; it = g_list_next(it))
		purple_conversation_set_e2ee_state(it->data, NULL);
	g_list_free(clear_states);
}

PurpleE2eeProvider *
purple_e2ee_provider_get_main(void)
{
	return main_provider;
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
purple_e2ee_provider_set_conv_menu_cb(PurpleE2eeProvider *provider,
	PurpleE2eeConvMenuCallback conv_menu_cb)
{
	g_return_if_fail(provider != NULL);

	provider->conv_menu_cb = conv_menu_cb;
}

PurpleE2eeConvMenuCallback
purple_e2ee_provider_get_conv_menu_cb(PurpleE2eeProvider *provider)
{
	g_return_val_if_fail(provider != NULL, NULL);

	return provider->conv_menu_cb;
}

GList *
purple_e2ee_provider_get_conv_menu_actions(PurpleE2eeProvider *provider,
	PurpleConversation *conv)
{
	PurpleE2eeConvMenuCallback cb;

	g_return_val_if_fail(provider, NULL);
	g_return_val_if_fail(conv, NULL);

	cb = purple_e2ee_provider_get_conv_menu_cb(provider);
	if (cb == NULL)
		return NULL;

	return cb(conv);
}
