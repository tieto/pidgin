/**
 * @file request.h Request API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
 */
#ifndef _GAIM_REQUEST_H_
#define _GAIM_REQUEST_H_

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

/**
 * Request types.
 */
typedef enum
{
	GAIM_REQUEST_INPUT = 0,  /**< Text input request.      */
	GAIM_REQUEST_CHOICE,     /**< Multiple-choice request. */
	GAIM_REQUEST_ACTION      /**< Action request.          */

} GaimRequestType;

/**
 * Request UI operations.
 */
typedef struct
{
	void *(*request_input)(const char *title, const char *primary,
						   const char *secondary, const char *default_value,
						   gboolean multiline,
						   const char *ok_text, GCallback ok_cb,
						   const char *cancel_text, GCallback cancel_cb,
						   void *user_data);
	void *(*request_choice)(const char *title, const char *primary,
							const char *secondary, unsigned int default_value,
							const char *ok_text, GCallback ok_cb,
							const char *cancel_text, GCallback cancel_cb,
							void *user_data, size_t choice_count,
							va_list choices);
	void *(*request_action)(const char *title, const char *primary,
							const char *secondary, unsigned int default_action,
							void *user_data, size_t action_count,
							va_list actions);

	void (*close_request)(GaimRequestType type, void *ui_handle);

} GaimRequestUiOps;

typedef void (*GaimRequestInputCb)(const char *, void *);
typedef void (*GaimRequestActionCb)(int, void *);

/**************************************************************************/
/** @name Request API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Prompts the user for text input.
 *
 * @param handle        The plugin or connection handle.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param multiline     TRUE if the inputted text can span multiple lines.
 * @param ok_text       The text for the OK button.
 * @param ok_cb         The callback for the OK button.
 * @param cancel_text   The text for the cancel button.
 * @param cancel_cb     The callback for the cancel button.
 * @param user_data     The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_input(void *handle, const char *title,
						 const char *primary, const char *secondary,
						 const char *default_value, gboolean multiline,
						 const char *ok_text, GCallback ok_cb,
						 const char *cancel_text, GCallback cancel_cb,
						 void *user_data);

/**
 * Prompts the user for multiple-choice input.
 *
 * @param handle        The plugin or connection handle.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param ok_text       The text for the OK button.
 * @param ok_cb         The callback for the OK button.
 * @param cancel_text   The text for the cancel button.
 * @param cancel_cb     The callback for the cancel button.
 * @param user_data     The data to pass to the callback.
 * @param choice_count  The number of choices.
 * @param choice        The choices.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_choice(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  unsigned int default_value,
						  const char *ok_text, GCallback ok_cb,
						  const char *cancel_text, GCallback cancel_cb,
						  void *user_data, size_t choice_count, ...);

/**
 * Prompts the user for multiple-choice input.
 *
 * @param handle        The plugin or connection handle.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param ok_text       The text for the OK button.
 * @param ok_cb         The callback for the OK button.
 * @param cancel_text   The text for the cancel button.
 * @param cancel_cb     The callback for the cancel button.
 * @param user_data     The data to pass to the callback.
 * @param choice_count  The number of choices.
 * @param choices       The choices.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_choice_varg(void *handle, const char *title,
							   const char *primary, const char *secondary,
							   unsigned int default_value,
							   const char *ok_text, GCallback ok_cb,
							   const char *cancel_text, GCallback cancel_cb,
							   void *user_data, size_t choice_count,
							   va_list choices);

/**
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @param handle         The plugin or connection handle.
 * @param title          The title of the message.
 * @param primary        The main point of the message.
 * @param secondary      The secondary information.
 * @param default_action The default value.
 * @param user_data      The data to pass to the callback.
 * @param action_count   The number of actions.
 * @param action         The first action.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_action(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  unsigned int default_action,
						  void *user_data, size_t action_count, ...);

/**
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @param handle         The plugin or connection handle.
 * @param title          The title of the message.
 * @param primary        The main point of the message.
 * @param secondary      The secondary information.
 * @param default_action The default value.
 * @param user_data      The data to pass to the callback.
 * @param action_count   The number of actions.
 * @param actions        A list of actions and callbacks.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_action_varg(void *handle, const char *title,
							   const char *primary, const char *secondary,
							   unsigned int default_action,
							   void *user_data, size_t action_count,
							   va_list actions);

/**
 * Closes a request.
 *
 * This should be used only by the UI operation functions and part of the
 * core.
 *
 * @param type     The request type.
 * @param uihandle The request UI handle.
 */
void gaim_request_close(GaimRequestType type, void *uihandle);

/**
 * Closes all requests registered with the specified handle.
 *
 * @param handle The handle.
 */
void gaim_request_close_with_handle(void *handle);

/**
 * A wrapper for gaim_request_action() that uses Yes and No buttons.
 */
#define gaim_request_yes_no(handle, title, primary, secondary, \
							default_action, user_data, yes_cb, no_cb) \
	gaim_request_action((handle), (title), (primary), (secondary) \
						(default_action), (user_data), \
						_("Yes"), (yes_cb), _("No"), (no_cb), NULL)

/**
 * A wrapper for gaim_request_action() that uses OK and Cancel buttons.
 */
#define gaim_request_ok_cancel(handle, title, primary, secondary, \
							default_action, user_data, ok_cb, cancel_cb) \
	gaim_request_action((handle), (title), (primary), (secondary) \
						(default_action), (user_data), \
						_("OK"), (ok_cb), _("Cancel"), (cancel_cb), NULL)

/*@}*/

/**************************************************************************/
/** @name UI Operations API                                               */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when displaying a
 * request.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_request_ui_ops(GaimRequestUiOps *ops);

/**
 * Returns the UI operations structure to be used when displaying a
 * request.
 *
 * @param ops The UI operations structure.
 */
GaimRequestUiOps *gaim_get_request_ui_ops(void);

/*@}*/

#endif /* _GAIM_REQUEST_H_ */
