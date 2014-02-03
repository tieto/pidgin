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
/**
 * SECTION:e2ee
 * @section_id: libpurple-e2ee
 * @short_description: <filename>e2ee.h</filename>
 * @title: End-to-end encryption API
 */

#ifndef _PURPLE_E2EE_H_
#define _PURPLE_E2EE_H_

typedef struct _PurpleE2eeState PurpleE2eeState;

typedef struct _PurpleE2eeProvider PurpleE2eeProvider;

#include <glib.h>
#include "conversation.h"

typedef GList * (*PurpleE2eeConvMenuCallback)(PurpleConversation *conv);

G_BEGIN_DECLS

/**************************************************************************/
/** @name Encryption states for conversations.                            */
/**************************************************************************/
/*@{*/

/**
 * purple_e2ee_state_new:
 * @provider: The E2EE provider that created this state.
 *
 * Creates new E2EE state.
 *
 * State objects are global (shared between multiple conversations).
 *
 * Returns: New E2EE state.
 */
PurpleE2eeState *
purple_e2ee_state_new(PurpleE2eeProvider *provider);

/**
 * purple_e2ee_state_ref:
 * @state: The E2EE state.
 *
 * Increment the reference count.
 */
void
purple_e2ee_state_ref(PurpleE2eeState *state);

/**
 * purple_e2ee_state_unref:
 * @state: The E2EE state.
 *
 * Decrement the reference count.
 *
 * If the reference count reaches zero, the state will be freed.
 *
 * Returns: @state or %NULL if the reference count reached zero.
 */
PurpleE2eeState *
purple_e2ee_state_unref(PurpleE2eeState *state);

/**
 * purple_e2ee_state_get_provider:
 * @state: The E2EE state.
 *
 * Gets the provider of specified E2EE state.
 *
 * Returns: The provider for this state.
 */
PurpleE2eeProvider *
purple_e2ee_state_get_provider(PurpleE2eeState *state);

/**
 * purple_e2ee_state_set_name:
 * @state: The E2EE state.
 * @name:  The localized name.
 *
 * Sets the name for the E2EE state.
 */
void
purple_e2ee_state_set_name(PurpleE2eeState *state, const gchar *name);

/**
 * purple_e2ee_state_get_name:
 * @state: The E2EE state.
 *
 * Gets the name of the E2EE state.
 *
 * Returns: The localized name.
 */
const gchar *
purple_e2ee_state_get_name(PurpleE2eeState *state);

/**
 * purple_e2ee_state_set_stock_icon:
 * @state:      The E2EE state.
 * @stock_icon: The stock icon identifier.
 *
 * Sets the icon for the E2EE state.
 */
void
purple_e2ee_state_set_stock_icon(PurpleE2eeState *state,
	const gchar *stock_icon);

/**
 * purple_e2ee_state_get_stock_icon:
 * @state: The E2EE state.
 *
 * Gets the icon of the E2EE state.
 *
 * Returns: The stock icon identifier.
 */
const gchar *
purple_e2ee_state_get_stock_icon(PurpleE2eeState *state);

/*@}*/


/**************************************************************************/
/** @name Encryption providers API.                                       */
/**************************************************************************/
/*@{*/

/**
 * purple_e2ee_provider_new:
 *
 * Creates new E2EE provider.
 *
 * Returns: New E2EE provider.
 */
PurpleE2eeProvider *
purple_e2ee_provider_new(void);

/**
 * purple_e2ee_provider_free:
 * @provider: The provider.
 *
 * Destroys the E2EE provider.
 *
 * The provider have to be unregistered prior.
 */
void
purple_e2ee_provider_free(PurpleE2eeProvider *provider);

/**
 * purple_e2ee_provider_register:
 * @provider: The E2EE provider.
 *
 * Registers the E2EE provider.
 *
 * Currently, there is no support for multiple E2EE providers - only the first
 * one is registered.
 *
 * Returns: %TRUE, if the provider was successfully registered,
 *         %FALSE otherwise.
 */
gboolean
purple_e2ee_provider_register(PurpleE2eeProvider *provider);

/**
 * purple_e2ee_provider_unregister:
 * @provider: The E2EE provider.
 *
 * Unregisters the E2EE provider.
 */
void
purple_e2ee_provider_unregister(PurpleE2eeProvider *provider);

/**
 * purple_e2ee_provider_get_main:
 *
 * Gets main E2EE provider.
 *
 * Returns: The main E2EE provider.
 */
PurpleE2eeProvider *
purple_e2ee_provider_get_main(void);

/**
 * purple_e2ee_provider_set_name:
 * @provider: The E2EE provider.
 * @name:     The localized name.
 *
 * Sets the name for the E2EE provider.
 */
void
purple_e2ee_provider_set_name(PurpleE2eeProvider *provider, const gchar *name);

/**
 * purple_e2ee_provider_get_name:
 * @provider: The E2EE provider.
 *
 * Gets the name of the E2EE provider.
 *
 * Returns: The localized name of specified E2EE provider.
 */
const gchar *
purple_e2ee_provider_get_name(PurpleE2eeProvider *provider);

/**
 * purple_e2ee_provider_set_conv_menu_cb:
 * @provider:     The E2EE provider.
 * @conv_menu_cb: The callback.
 *
 * Sets the conversation menu callback for the E2EE provider.
 *
 * The function is called, when user extends the E2EE menu for the conversation
 * specified in its parameter.
 *
 * Function should return the GList of PurpleMenuAction objects.
 */
void
purple_e2ee_provider_set_conv_menu_cb(PurpleE2eeProvider *provider,
	PurpleE2eeConvMenuCallback conv_menu_cb);

/**
 * purple_e2ee_provider_get_conv_menu_cb:
 * @provider: The E2EE provider.
 *
 * Gets the conversation menu callback of the E2EE provider.
 *
 * Returns: The callback.
 */
PurpleE2eeConvMenuCallback
purple_e2ee_provider_get_conv_menu_cb(PurpleE2eeProvider *provider);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_E2EE_H_ */
