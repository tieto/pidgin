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
	GAIM_REQUEST_ACTION,     /**< Action request.          */
	GAIM_REQUEST_FIELDS      /**< Multiple fields request. */

} GaimRequestType;

/**
 * A type of field.
 */
typedef enum
{
	GAIM_REQUEST_FIELD_NONE,
	GAIM_REQUEST_FIELD_STRING,
	GAIM_REQUEST_FIELD_INTEGER,
	GAIM_REQUEST_FIELD_BOOLEAN,
	GAIM_REQUEST_FIELD_CHOICE

} GaimRequestFieldType;

/**
 * A request field.
 */
typedef struct
{
	GaimRequestFieldType type;

	char *id;
	char *label;

	union
	{
		struct
		{
			gboolean multiline;
			char *default_value;
			char *value;

		} string;

		struct
		{
			int default_value;
			int value;

		} integer;

		struct
		{
			gboolean default_value;
			gboolean value;

		} boolean;

		struct
		{
			int default_value;
			int value;

			GList *labels;

		} choice;

	} u;

	void *ui_data;

} GaimRequestField;

/**
 * Multiple fields request data.
 */
typedef struct
{
	GList *groups;

	GHashTable *fields;

} GaimRequestFields;

/**
 * A group of fields with a title.
 */
typedef struct
{
	GaimRequestFields *fields_list;

	char *title;

	GList *fields;

} GaimRequestFieldGroup;

/**
 * Request UI operations.
 */
typedef struct
{
	void *(*request_input)(const char *title, const char *primary,
						   const char *secondary, const char *default_value,
						   gboolean multiline, gboolean masked,
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
	void *(*request_fields)(const char *title, const char *primary,
							const char *secondary, GaimRequestFields *fields,
							const char *ok_text, GCallback ok_cb,
							const char *cancel_text, GCallback cancel_cb,
							void *user_data);

	void (*close_request)(GaimRequestType type, void *ui_handle);

} GaimRequestUiOps;

typedef void (*GaimRequestInputCb)(void *, const char *);
typedef void (*GaimRequestActionCb)(void *, int);
typedef void (*GaimRequestFieldsCb)(void *, GaimRequestFields *fields);

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Field List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a list of fields to pass to gaim_request_fields().
 *
 * @return A GaimRequestFields structure.
 */
GaimRequestFields *gaim_request_fields_new(void);

/**
 * Destroys a list of fields.
 *
 * @param fields The list of fields to destroy.
 */
void gaim_request_fields_destroy(GaimRequestFields *fields);

/**
 * Adds a group of fields to the list.
 *
 * @param fields The fields list.
 * @param group  The group to add.
 */
void gaim_request_fields_add_group(GaimRequestFields *fields,
								   GaimRequestFieldGroup *group);

/**
 * Returns a list of all groups in a field list.
 *
 * @param fields The fields list.
 *
 * @return A list of groups.
 */
GList *gaim_request_fields_get_groups(const GaimRequestFields *fields);

/**
 * Return the field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The field, if found.
 */
GaimRequestField *gaim_request_fields_get_field(
		const GaimRequestFields *fields, const char *id);

/**
 * Returns the string value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The string value, if found, or @c NULL otherwise.
 */
const char *gaim_request_fields_get_string(const GaimRequestFields *fields,
										   const char *id);

/**
 * Returns the integer value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The integer value, if found, or 0 otherwise.
 */
int gaim_request_fields_get_integer(const GaimRequestFields *fields,
									const char *id);

/**
 * Returns the boolean value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The boolean value, if found, or @c FALSE otherwise.
 */
gboolean gaim_request_fields_get_bool(const GaimRequestFields *fields,
									  const char *id);

/**
 * Returns the choice index of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The choice index, if found, or -1 otherwise.
 */
int gaim_request_fields_get_choice(const GaimRequestFields *fields,
								   const char *id);

/*@}*/

/**************************************************************************/
/** @name Fields Group API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a fields group with an optional title.
 *
 * @param title The optional title to give the group.
 *
 * @return A new fields group
 */
GaimRequestFieldGroup *gaim_request_field_group_new(const char *title);

/**
 * Destroys a fields group.
 *
 * @param group The group to destroy.
 */
void gaim_request_field_group_destroy(GaimRequestFieldGroup *group);

/**
 * Adds a field to the group.
 *
 * @param group The group to add the field to.
 * @param field The field to add to the group.
 */
void gaim_request_field_group_add_field(GaimRequestFieldGroup *group,
										GaimRequestField *field);

/**
 * Returns the title of a fields group.
 *
 * @param group The group.
 *
 * @return The title, if set.
 */
const char *gaim_request_field_group_get_title(
		const GaimRequestFieldGroup *group);

/**
 * Returns a list of all fields in a group.
 *
 * @param group The group.
 *
 * @return The list of fields in the group.
 */
GList *gaim_request_field_group_get_fields(
		const GaimRequestFieldGroup *group);

/*@}*/

/**************************************************************************/
/** @name Field API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a field of the specified type.
 *
 * @param id   The field ID.
 * @param text The text label of the field.
 * @param type The type of field.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_new(const char *id, const char *text,
										 GaimRequestFieldType type);

/**
 * Destroys a field.
 *
 * @param field The field to destroy.
 */
void gaim_request_field_destroy(GaimRequestField *field);

/**
 * Sets the label text of a field.
 *
 * @param field The field.
 * @param label The text label.
 */
void gaim_request_field_set_label(GaimRequestField *field, const char *label);

/**
 * Returns the type of a field.
 *
 * @param field The field.
 *
 * @return The field's type.
 */
GaimRequestFieldType gaim_request_field_get_type(const GaimRequestField *field);

/**
 * Returns the ID of a field.
 *
 * @param field The field.
 *
 * @return The ID
 */
const char *gaim_request_field_get_id(const GaimRequestField *field);

/**
 * Returns the label text of a field.
 *
 * @param field The field.
 *
 * @return The label text.
 */
const char *gaim_request_field_get_label(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name String Field API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a string request field.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The optional default value.
 * @param multiline     Whether or not this should be a multiline string.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_string_new(const char *id,
												const char *text,
												const char *default_value,
												gboolean multiline);

/**
 * Sets the default value in a string field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_string_set_default_value(GaimRequestField *field,
												 const char *default_value);

/**
 * Sets the value in a string field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_string_set_value(GaimRequestField *field,
										 const char *value);

/**
 * Returns the default value in a string field.
 *
 * @param field The field.
 * 
 * @return The default value.
 */
const char *gaim_request_field_string_get_default_value(
		const GaimRequestField *field);

/**
 * Returns the user-entered value in a string field.
 *
 * @param field The field.
 *
 * @return The value.
 */
const char *gaim_request_field_string_get_value(const GaimRequestField *field);

/**
 * Returns whether or not a string field is multi-line.
 *
 * @param field The field.
 *
 * @return @c TRUE if the field is mulit-line, or @c FALSE otherwise.
 */
gboolean gaim_request_field_string_is_multiline(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Integer Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates an integer field.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The default value.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_int_new(const char *id,
											 const char *text,
											 int default_value);

/**
 * Sets the default value in an integer field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_int_set_default_value(GaimRequestField *field,
											  int default_value);

/**
 * Sets the value in an integer field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_int_set_value(GaimRequestField *field, int value);

/**
 * Returns the default value in an integer field.
 *
 * @param field The field.
 * 
 * @return The default value.
 */
int gaim_request_field_int_get_default_value(const GaimRequestField *field);

/**
 * Returns the user-entered value in an integer field.
 *
 * @param field The field.
 *
 * @return The value.
 */
int gaim_request_field_int_get_value(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Boolean Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates a boolean field.
 *
 * This is often represented as a checkbox.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The default value.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_bool_new(const char *id,
											  const char *text,
											  gboolean default_value);

/**
 * Sets the default value in an boolean field.
 *
 * @param field The field.
 * @param value The default value.
 */
void gaim_request_field_bool_set_default_value(GaimRequestField *field,
											   gboolean default_value);

/**
 * Sets the value in an boolean field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_bool_set_value(GaimRequestField *field,
									   gboolean value);

/**
 * Returns the default value in an boolean field.
 *
 * @param field The field.
 * 
 * @return The default value.
 */
gboolean gaim_request_field_bool_get_default_value(
		const GaimRequestField *field);

/**
 * Returns the user-entered value in an boolean field.
 *
 * @param field The field.
 *
 * @return The value.
 */
gboolean gaim_request_field_bool_get_value(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Choice Field API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a multiple choice field.
 *
 * This is often represented as a group of radio buttons.
 *
 * @param id            The field ID.
 * @param text          The optional label of the field.
 * @param default_value The default choice.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_choice_new(const char *id,
												const char *text,
												int default_value);

/**
 * Adds a choice to a multiple choice field.
 *
 * @param field The choice field.
 * @param label The choice label.
 */
void gaim_request_field_choice_add(GaimRequestField *field,
								   const char *label);

/**
 * Sets the default value in an choice field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_choice_set_default_value(GaimRequestField *field,
												 int default_value);

/**
 * Sets the value in an choice field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_choice_set_value(GaimRequestField *field, int value);

/**
 * Returns the default value in an choice field.
 *
 * @param field The field.
 * 
 * @return The default value.
 */
int gaim_request_field_choice_get_default_value(const GaimRequestField *field);

/**
 * Returns the user-entered value in an choice field.
 *
 * @param field The field.
 *
 * @return The value.
 */
int gaim_request_field_choice_get_value(const GaimRequestField *field);

/**
 * Returns a list of labels in a choice field.
 *
 * @param field The field.
 *
 * @return The list of labels.
 */
GList *gaim_request_field_choice_get_labels(const GaimRequestField *field);

/*@}*/

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
 * @param masked        TRUE if the inputted text should be masked in some way.
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
						 const char *default_value,
						 gboolean multiline, gboolean masked,
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
 * Displays groups of fields for the user to fill in.
 *
 * @param handle      The plugin or connection handle.
 * @param title       The title of the message.
 * @param primary     The main point of the message.
 * @param secondary   The secondary information.
 * @param fields      The list of fields.
 * @param ok_text     The text for the OK button.
 * @param ok_cb       The callback for the OK button.
 * @param cancel_text The text for the cancel button.
 * @param cancel_cb   The callback for the cancel button.
 * @param user_data   The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_fields(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  GaimRequestFields *fields,
						  const char *ok_text, GCallback ok_cb,
						  const char *cancel_text, GCallback cancel_cb,
						  void *user_data);

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
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("Yes"), (yes_cb), _("No"), (no_cb))

/**
 * A wrapper for gaim_request_action() that uses OK and Cancel buttons.
 */
#define gaim_request_ok_cancel(handle, title, primary, secondary, \
							default_action, user_data, ok_cb, cancel_cb) \
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("OK"), (ok_cb), _("Cancel"), (cancel_cb))

/**
 * A wrapper for gaim_request_action() that uses Accept and Cancel buttons.
 */
#define gaim_request_accept_cancel(handle, title, primary, secondary, \
								   default_action, user_data, accept_cb, \
								   cancel_cb) \
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("Accept"), (accept_cb), _("Cancel"), (cancel_cb))

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

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_REQUEST_H_ */
