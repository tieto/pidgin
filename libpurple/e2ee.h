/**
 * @file e2ee.h End-to-end encryption API
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
 * Creates new E2EE state.
 *
 * State objects are global (shared between multiple conversations).
 *
 * @provider: The E2EE provider that created this state.
 *
 * Returns: New E2EE state.
 */
PurpleE2eeState *
purple_e2ee_state_new(PurpleE2eeProvider *provider);

/**
 * Increment the reference count.
 *
 * @state: The E2EE state.
 */
void
purple_e2ee_state_ref(PurpleE2eeState *state);

/**
 * Decrement the reference count.
 *
 * If the reference count reaches zero, the state will be freed.
 *
 * @state: The E2EE state.
 *
 * Returns: @a state or @c NULL if the reference count reached zero.
 */
PurpleE2eeState *
purple_e2ee_state_unref(PurpleE2eeState *state);

/**
 * Gets the provider of specified E2EE state.
 *
 * @state: The E2EE state.
 *
 * Returns: The provider for this state.
 */
PurpleE2eeProvider *
purple_e2ee_state_get_provider(PurpleE2eeState *state);

/**
 * Sets the name for the E2EE state.
 *
 * @state: The E2EE state.
 * @name:  The localized name.
 */
void
purple_e2ee_state_set_name(PurpleE2eeState *state, const gchar *name);

/**
 * Gets the name of the E2EE state.
 *
 * @state: The E2EE state.
 *
 * Returns: The localized name.
 */
const gchar *
purple_e2ee_state_get_name(PurpleE2eeState *state);

/**
 * Sets the icon for the E2EE state.
 *
 * @state:      The E2EE state.
 * @stock_icon: The stock icon identifier.
 */
void
purple_e2ee_state_set_stock_icon(PurpleE2eeState *state,
	const gchar *stock_icon);

/**
 * Gets the icon of the E2EE state.
 *
 * @state: The E2EE state.
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
 * Creates new E2EE provider.
 *
 * Returns: New E2EE provider.
 */
PurpleE2eeProvider *
purple_e2ee_provider_new(void);

/**
 * Destroys the E2EE provider.
 *
 * The provider have to be unregistered prior.
 *
 * @provider: The provider.
 */
void
purple_e2ee_provider_free(PurpleE2eeProvider *provider);

/**
 * Registers the E2EE provider.
 *
 * Currently, there is no support for multiple E2EE providers - only the first
 * one is registered.
 *
 * @provider: The E2EE provider.
 *
 * Returns: @c TRUE, if the provider was successfully registered,
 *         @c FALSE otherwise.
 */
gboolean
purple_e2ee_provider_register(PurpleE2eeProvider *provider);

/**
 * Unregisters the E2EE provider.
 *
 * @provider: The E2EE provider.
 */
void
purple_e2ee_provider_unregister(PurpleE2eeProvider *provider);

/**
 * Gets main E2EE provider.
 *
 * Returns: The main E2EE provider.
 */
PurpleE2eeProvider *
purple_e2ee_provider_get_main(void);

/**
 * Sets the name for the E2EE provider.
 *
 * @provider: The E2EE provider.
 * @name:     The localized name.
 */
void
purple_e2ee_provider_set_name(PurpleE2eeProvider *provider, const gchar *name);

/**
 * Gets the name of the E2EE provider.
 *
 * @provider: The E2EE provider.
 *
 * Returns: The localized name of specified E2EE provider.
 */
const gchar *
purple_e2ee_provider_get_name(PurpleE2eeProvider *provider);

/**
 * Sets the conversation menu callback for the E2EE provider.
 *
 * The function is called, when user extends the E2EE menu for the conversation
 * specified in its parameter.
 *
 * Function should return the GList of PurpleMenuAction objects.
 *
 * @provider:     The E2EE provider.
 * @conv_menu_cb: The callback.
 */
void
purple_e2ee_provider_set_conv_menu_cb(PurpleE2eeProvider *provider,
	PurpleE2eeConvMenuCallback conv_menu_cb);

/**
 * Gets the conversation menu callback of the E2EE provider.
 *
 * @provider: The E2EE provider.
 *
 * Returns: The callback.
 */
PurpleE2eeConvMenuCallback
purple_e2ee_provider_get_conv_menu_cb(PurpleE2eeProvider *provider);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_E2EE_H_ */
