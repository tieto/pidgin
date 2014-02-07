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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_POUNCE_H_
#define _PURPLE_POUNCE_H_
/**
 * SECTION:pounce
 * @section_id: libpurple-pounce
 * @short_description: <filename>pounce.h</filename>
 * @title: Buddy Pounce API
 */

typedef struct _PurplePounce PurplePounce;

#include <glib.h>
#include "account.h"

/**
 * PurplePounceEvent:
 * @PURPLE_POUNCE_NONE:             No events.
 * @PURPLE_POUNCE_SIGNON:           The buddy signed on.
 * @PURPLE_POUNCE_SIGNOFF:          The buddy signed off.
 * @PURPLE_POUNCE_AWAY:             The buddy went away.
 * @PURPLE_POUNCE_AWAY_RETURN:      The buddy returned from away.
 * @PURPLE_POUNCE_IDLE:             The buddy became idle.
 * @PURPLE_POUNCE_IDLE_RETURN:      The buddy is no longer idle.
 * @PURPLE_POUNCE_TYPING:           The buddy started typing.
 * @PURPLE_POUNCE_TYPED:            The buddy has entered text.
 * @PURPLE_POUNCE_TYPING_STOPPED:   The buddy stopped typing.
 * @PURPLE_POUNCE_MESSAGE_RECEIVED: The buddy sent a message.
 *
 * Events that trigger buddy pounces.
 */
typedef enum
{
	PURPLE_POUNCE_NONE             = 0x000,
	PURPLE_POUNCE_SIGNON           = 0x001,
	PURPLE_POUNCE_SIGNOFF          = 0x002,
	PURPLE_POUNCE_AWAY             = 0x004,
	PURPLE_POUNCE_AWAY_RETURN      = 0x008,
	PURPLE_POUNCE_IDLE             = 0x010,
	PURPLE_POUNCE_IDLE_RETURN      = 0x020,
	PURPLE_POUNCE_TYPING           = 0x040,
	PURPLE_POUNCE_TYPED            = 0x080,
	PURPLE_POUNCE_TYPING_STOPPED   = 0x100,
	PURPLE_POUNCE_MESSAGE_RECEIVED = 0x200

} PurplePounceEvent;

/**
 * PurplePounceOption:
 * @PURPLE_POUNCE_OPTION_NONE: No Option
 * @PURPLE_POUNCE_OPTION_AWAY: Pounce only when away
 */
typedef enum
{
	PURPLE_POUNCE_OPTION_NONE		= 0x00,
	PURPLE_POUNCE_OPTION_AWAY		= 0x01
} PurplePounceOption;

/**
 * PurplePounceCb:
 *
 * A pounce callback.
 */
typedef void (*PurplePounceCb)(PurplePounce *, PurplePounceEvent, void *);

G_BEGIN_DECLS

/**************************************************************************/
/* Buddy Pounce API                                                       */
/**************************************************************************/

/**
 * purple_pounce_new:
 * @ui_type: The type of UI the pounce is for.
 * @pouncer: The account that will pounce.
 * @pouncee: The buddy to pounce on.
 * @event:   The event(s) to pounce on.
 * @option:  Pounce options.
 *
 * Creates a new buddy pounce.
 *
 * Returns: The new buddy pounce structure.
 */
PurplePounce *purple_pounce_new(const char *ui_type, PurpleAccount *pouncer,
							const char *pouncee, PurplePounceEvent event,
							PurplePounceOption option);

/**
 * purple_pounce_destroy:
 * @pounce: The buddy pounce.
 *
 * Destroys a buddy pounce.
 */
void purple_pounce_destroy(PurplePounce *pounce);

/**
 * purple_pounce_destroy_all_by_account:
 * @account: The account to remove all pounces from.
 *
 * Destroys all buddy pounces for the account
 */
void purple_pounce_destroy_all_by_account(PurpleAccount *account);

/**
 * purple_pounce_destroy_all_by_buddy:
 * @buddy: The buddy whose pounces are to be removed
 *
 * Destroys all buddy pounces for a buddy
 */
void purple_pounce_destroy_all_by_buddy(PurpleBuddy *buddy);

/**
 * purple_pounce_set_events:
 * @pounce: The buddy pounce.
 * @events: The events to watch for.
 *
 * Sets the events a pounce should watch for.
 */
void purple_pounce_set_events(PurplePounce *pounce, PurplePounceEvent events);

/**
 * purple_pounce_set_options:
 * @pounce:  The buddy pounce.
 * @options: The options for the pounce.
 *
 * Sets the options for a pounce.
 */
void purple_pounce_set_options(PurplePounce *pounce, PurplePounceOption options);

/**
 * purple_pounce_set_pouncer:
 * @pounce:  The buddy pounce.
 * @pouncer: The account that will pounce.
 *
 * Sets the account that will do the pouncing.
 */
void purple_pounce_set_pouncer(PurplePounce *pounce, PurpleAccount *pouncer);

/**
 * purple_pounce_set_pouncee:
 * @pounce:  The buddy pounce.
 * @pouncee: The buddy to pounce on.
 *
 * Sets the buddy a pounce should pounce on.
 */
void purple_pounce_set_pouncee(PurplePounce *pounce, const char *pouncee);

/**
 * purple_pounce_set_save:
 * @pounce: The buddy pounce.
 * @save:   %TRUE if the pounce should be saved, or %FALSE otherwise.
 *
 * Sets whether or not the pounce should be saved after execution.
 */
void purple_pounce_set_save(PurplePounce *pounce, gboolean save);

/**
 * purple_pounce_action_register:
 * @pounce: The buddy pounce.
 * @name:   The action name.
 *
 * Registers an action type for the pounce.
 */
void purple_pounce_action_register(PurplePounce *pounce, const char *name);

/**
 * purple_pounce_action_set_enabled:
 * @pounce:  The buddy pounce.
 * @action:  The name of the action.
 * @enabled: The enabled state.
 *
 * Enables or disables an action for a pounce.
 */
void purple_pounce_action_set_enabled(PurplePounce *pounce, const char *action,
									gboolean enabled);

/**
 * purple_pounce_action_set_attribute:
 * @pounce: The buddy pounce.
 * @action: The action name.
 * @attr:   The attribute name.
 * @value:  The value.
 *
 * Sets a value for an attribute in an action.
 *
 * If @value is %NULL, the value will be unset.
 */
void purple_pounce_action_set_attribute(PurplePounce *pounce, const char *action,
									  const char *attr, const char *value);

/**
 * purple_pounce_set_data:
 * @pounce: The buddy pounce.
 * @data:   Data specific to the pounce.
 *
 * Sets the pounce-specific data.
 */
void purple_pounce_set_data(PurplePounce *pounce, void *data);

/**
 * purple_pounce_get_events:
 * @pounce: The buddy pounce.
 *
 * Returns the events a pounce should watch for.
 *
 * Returns: The events the pounce is watching for.
 */
PurplePounceEvent purple_pounce_get_events(const PurplePounce *pounce);

/**
 * purple_pounce_get_options:
 * @pounce: The buddy pounce.
 *
 * Returns the options for a pounce.
 *
 * Returns: The options for the pounce.
 */
PurplePounceOption purple_pounce_get_options(const PurplePounce *pounce);

/**
 * purple_pounce_get_pouncer:
 * @pounce: The buddy pounce.
 *
 * Returns the account that will do the pouncing.
 *
 * Returns: The account that will pounce.
 */
PurpleAccount *purple_pounce_get_pouncer(const PurplePounce *pounce);

/**
 * purple_pounce_get_pouncee:
 * @pounce: The buddy pounce.
 *
 * Returns the buddy a pounce should pounce on.
 *
 * Returns: The buddy to pounce on.
 */
const char *purple_pounce_get_pouncee(const PurplePounce *pounce);

/**
 * purple_pounce_get_save:
 * @pounce: The buddy pounce.
 *
 * Returns whether or not the pounce should save after execution.
 *
 * Returns: %TRUE if the pounce should be saved after execution, or
 *         %FALSE otherwise.
 */
gboolean purple_pounce_get_save(const PurplePounce *pounce);

/**
 * purple_pounce_action_is_enabled:
 * @pounce: The buddy pounce.
 * @action: The action name.
 *
 * Returns whether or not an action is enabled.
 *
 * Returns: %TRUE if the action is enabled, or %FALSE otherwise.
 */
gboolean purple_pounce_action_is_enabled(const PurplePounce *pounce,
									   const char *action);

/**
 * purple_pounce_action_get_attribute:
 * @pounce: The buddy pounce.
 * @action: The action name.
 * @attr:   The attribute name.
 *
 * Returns the value for an attribute in an action.
 *
 * Returns: The attribute value, if it exists, or %NULL.
 */
const char *purple_pounce_action_get_attribute(const PurplePounce *pounce,
											 const char *action,
											 const char *attr);

/**
 * purple_pounce_get_data:
 * @pounce: The buddy pounce.
 *
 * Returns the pounce-specific data.
 *
 * Returns: The data specific to a buddy pounce.
 */
void *purple_pounce_get_data(const PurplePounce *pounce);

/**
 * purple_pounce_execute:
 * @pouncer: The account that will do the pouncing.
 * @pouncee: The buddy that is being pounced.
 * @events:  The events that triggered the pounce.
 *
 * Executes a pounce with the specified pouncer, pouncee, and event type.
 */
void purple_pounce_execute(const PurpleAccount *pouncer, const char *pouncee,
						 PurplePounceEvent events);

/**************************************************************************/
/* Buddy Pounce Subsystem API                                             */
/**************************************************************************/

/**
 * purple_find_pounce:
 * @pouncer: The account to match against.
 * @pouncee: The buddy to match against.
 * @events:  The event(s) to match against.
 *
 * Finds a pounce with the specified event(s) and buddy.
 *
 * Returns: The pounce if found, or %NULL otherwise.
 */
PurplePounce *purple_find_pounce(const PurpleAccount *pouncer,
							 const char *pouncee, PurplePounceEvent events);

/**
 * purple_pounces_register_handler:
 * @ui:          The UI name.
 * @cb:          The callback function.
 * @new_pounce:  The function called when a pounce is created.
 * @free_pounce: The function called when a pounce is freed.
 *
 * Registers a pounce handler for a UI.
 */
void purple_pounces_register_handler(const char *ui, PurplePounceCb cb,
								   void (*new_pounce)(PurplePounce *pounce),
								   void (*free_pounce)(PurplePounce *pounce));

/**
 * purple_pounces_unregister_handler:
 * @ui: The UI name.
 *
 * Unregisters a pounce handle for a UI.
 */
void purple_pounces_unregister_handler(const char *ui);

/**
 * purple_pounces_get_all:
 *
 * Returns a list of all registered buddy pounces.
 *
 * Returns: (transfer none): The list of buddy pounces.
 */
GList *purple_pounces_get_all(void);

/**
 * purple_pounces_get_all_for_ui:
 * @ui:  The ID of the UI using the core.
 *
 * Returns a list of registered buddy pounces for the ui-type.
 *
 * Returns: The list of buddy pounces. The list should be freed by
 *         the caller when it's no longer used.
 */
GList *purple_pounces_get_all_for_ui(const char *ui);

/**
 * purple_pounces_get_handle:
 *
 * Returns the buddy pounce subsystem handle.
 *
 * Returns: The subsystem handle.
 */
void *purple_pounces_get_handle(void);

/**
 * purple_pounces_init:
 *
 * Initializes the pounces subsystem.
 */
void purple_pounces_init(void);

/**
 * purple_pounces_uninit:
 *
 * Uninitializes the pounces subsystem.
 */
void purple_pounces_uninit(void);

G_END_DECLS

#endif /* _PURPLE_POUNCE_H_ */
