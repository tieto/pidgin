/**
 * @file request.h Request API
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PURPLE_REQUEST_H_
#define _PURPLE_REQUEST_H_

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

#include "certificate.h"
#include "request-datasheet.h"

/**
 * A request field.
 */
typedef struct _PurpleRequestField PurpleRequestField;

/**
 * Multiple fields request data.
 */
typedef struct _PurpleRequestFields PurpleRequestFields;

/**
 * A group of fields with a title.
 */
typedef struct _PurpleRequestFieldGroup PurpleRequestFieldGroup;

/**
 * Common parameters for UI operations.
 */
typedef struct _PurpleRequestCommonParameters PurpleRequestCommonParameters;

#include "account.h"

#define PURPLE_DEFAULT_ACTION_NONE	-1

/**
 * Request types.
 */
typedef enum
{
	PURPLE_REQUEST_INPUT = 0,  /**< Text input request.        */
	PURPLE_REQUEST_CHOICE,     /**< Multiple-choice request.   */
	PURPLE_REQUEST_ACTION,     /**< Action request.            */
	PURPLE_REQUEST_WAIT,       /**< Please wait dialog.        */
	PURPLE_REQUEST_FIELDS,     /**< Multiple fields request.   */
	PURPLE_REQUEST_FILE,       /**< File open or save request. */
	PURPLE_REQUEST_FOLDER      /**< Folder selection request.  */

} PurpleRequestType;

/**
 * A type of field.
 */
typedef enum
{
	PURPLE_REQUEST_FIELD_NONE,
	PURPLE_REQUEST_FIELD_STRING,
	PURPLE_REQUEST_FIELD_INTEGER,
	PURPLE_REQUEST_FIELD_BOOLEAN,
	PURPLE_REQUEST_FIELD_CHOICE,
	PURPLE_REQUEST_FIELD_LIST,
	PURPLE_REQUEST_FIELD_LABEL,
	PURPLE_REQUEST_FIELD_IMAGE,
	PURPLE_REQUEST_FIELD_ACCOUNT,
	PURPLE_REQUEST_FIELD_CERTIFICATE,
	PURPLE_REQUEST_FIELD_DATASHEET

} PurpleRequestFieldType;

typedef enum
{
	PURPLE_REQUEST_FEATURE_HTML = 0x00000001
} PurpleRequestFeature;

typedef enum
{
	PURPLE_REQUEST_ICON_DEFAULT = 0,
	PURPLE_REQUEST_ICON_REQUEST,
	PURPLE_REQUEST_ICON_DIALOG,
	PURPLE_REQUEST_ICON_WAIT,
	PURPLE_REQUEST_ICON_INFO,
	PURPLE_REQUEST_ICON_WARNING,
	PURPLE_REQUEST_ICON_ERROR
} PurpleRequestIconType;

typedef void (*PurpleRequestCancelCb)(gpointer);

/**
 * Request UI operations.
 */
typedef struct
{
	PurpleRequestFeature features;

	/** @see purple_request_input(). */
	void *(*request_input)(const char *title, const char *primary,
		const char *secondary, const char *default_value,
		gboolean multiline, gboolean masked, gchar *hint,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	/** @see purple_request_choice_varg(). */
	void *(*request_choice)(const char *title, const char *primary,
		const char *secondary, gpointer default_value,
		const char *ok_text, GCallback ok_cb, const char *cancel_text,
		GCallback cancel_cb, PurpleRequestCommonParameters *cpar,
		void *user_data, va_list choices);

	/** @see purple_request_action_varg(). */
	void *(*request_action)(const char *title, const char *primary,
		const char *secondary, int default_action,
		PurpleRequestCommonParameters *cpar, void *user_data,
		size_t action_count, va_list actions);

	/** @see purple_request_wait(). */
	void *(*request_wait)(const char *title, const char *primary,
		const char *secondary, gboolean with_progress,
		PurpleRequestCancelCb cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	/**
	 * @see purple_request_wait_pulse().
	 * @see purple_request_wait_progress().
	 */
	void (*request_wait_update)(void *ui_handle, gboolean pulse,
		gfloat fraction);

	/** @see purple_request_fields(). */
	void *(*request_fields)(const char *title, const char *primary,
		const char *secondary, PurpleRequestFields *fields,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	/** @see purple_request_file(). */
	void *(*request_file)(const char *title, const char *filename,
		gboolean savedialog, GCallback ok_cb, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	/** @see purple_request_folder(). */
	void *(*request_folder)(const char *title, const char *dirname,
		GCallback ok_cb, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void (*close_request)(PurpleRequestType type, void *ui_handle);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleRequestUiOps;

typedef void (*PurpleRequestInputCb)(void *, const char *);

typedef gboolean (*PurpleRequestFieldValidator)(PurpleRequestField *field,
	gchar **errmsg, gpointer user_data);

typedef gboolean (*PurpleRequestFieldSensitivityCb)(PurpleRequestField *field);

/** The type of callbacks passed to purple_request_action().  The first
 *  argument is the @a user_data parameter; the second is the index in the list
 *  of actions of the one chosen.
 */
typedef void (*PurpleRequestActionCb)(void *, int);
typedef void (*PurpleRequestChoiceCb)(void *, gpointer);
typedef void (*PurpleRequestFieldsCb)(void *, PurpleRequestFields *fields);
typedef void (*PurpleRequestFileCb)(void *, const char *filename);
typedef void (*PurpleRequestHelpCb)(gpointer);

G_BEGIN_DECLS

/**************************************************************************/
/** @name Common parameters API                                           */
/**************************************************************************/
/*@{*/

/**
 * Creates new parameters set for the request, which may or may not be used by
 * the UI to display the request.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_new(void);

/**
 * Creates new parameters set initially bound with the #PurpleConnection.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_connection(PurpleConnection *gc);

/**
 * Creates new parameters set initially bound with the #PurpleAccount.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_account(PurpleAccount *account);

/**
 * Creates new parameters set initially bound with the #PurpleConversation.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_conversation(PurpleConversation *conv);

/*
 * Increases the reference count on the parameters set.
 *
 * @cpar: The object to ref.
 */
void
purple_request_cpar_ref(PurpleRequestCommonParameters *cpar);

/**
 * Decreases the reference count on the parameters set.
 *
 * The object will be destroyed when this reaches 0.
 *
 * @cpar: The parameters set object to unref and possibly destroy.
 *
 * Returns: The NULL, if object was destroyed, cpar otherwise.
 */
PurpleRequestCommonParameters *
purple_request_cpar_unref(PurpleRequestCommonParameters *cpar);

/**
 * Sets the #PurpleAccount associated with the request, or %NULL, if none is.
 *
 * @cpar:    The parameters set.
 * @account: The #PurpleAccount to associate.
 */
void
purple_request_cpar_set_account(PurpleRequestCommonParameters *cpar,
	PurpleAccount *account);

/**
 * Gets the #PurpleAccount associated with the request.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: The associated #PurpleAccount, or NULL if none is.
 */
PurpleAccount *
purple_request_cpar_get_account(PurpleRequestCommonParameters *cpar);

/**
 * Sets the #PurpleConversation associated with the request, or %NULL, if
 * none is.
 *
 * @cpar: The parameters set.
 * @conv: The #PurpleConversation to associate.
 */
void
purple_request_cpar_set_conversation(PurpleRequestCommonParameters *cpar,
	PurpleConversation *conv);

/**
 * Gets the #PurpleConversation associated with the request.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: The associated #PurpleConversation, or NULL if none is.
 */
PurpleConversation *
purple_request_cpar_get_conversation(PurpleRequestCommonParameters *cpar);

/**
 * Sets the icon associated with the request.
 *
 * @cpar:      The parameters set.
 * @icon_type: The icon type.
 */
void
purple_request_cpar_set_icon(PurpleRequestCommonParameters *cpar,
	PurpleRequestIconType icon_type);

/**
 * Gets the icon associated with the request.
 *
 * @cpar: The parameters set.
 *
 * Returns:s icon_type The icon type.
 */
PurpleRequestIconType
purple_request_cpar_get_icon(PurpleRequestCommonParameters *cpar);

/**
 * Sets the custom icon associated with the request.
 *
 * @cpar:      The parameters set.
 * @icon_data: The icon image contents (%NULL to reset).
 * @icon_size: The icon image size.
 */
void
purple_request_cpar_set_custom_icon(PurpleRequestCommonParameters *cpar,
	gconstpointer icon_data, gsize icon_size);

/**
 * Gets the custom icon associated with the request.
 *
 * @cpar:      The parameters set (may be %NULL).
 * @icon_size: The pointer to variable, where icon size should be stored
 *                  (may be %NULL).
 *
 * Returns: The icon image contents.
 */
gconstpointer
purple_request_cpar_get_custom_icon(PurpleRequestCommonParameters *cpar,
	gsize *icon_size);

/**
 * Switches the request text to be HTML or not.
 *
 * @cpar:    The parameters set.
 * @enabled: 1, if the text passed with the request contains HTML,
 *                0 otherwise. Don't use any other values, as they may be
 *                redefined in the future.
 */
void
purple_request_cpar_set_html(PurpleRequestCommonParameters *cpar,
	gboolean enabled);

/**
 * Checks, if the text passed to the request is HTML.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: %TRUE, if the text is HTML, %FALSE otherwise.
 */
gboolean
purple_request_cpar_is_html(PurpleRequestCommonParameters *cpar);

/**
 * Sets dialog display mode to compact or default.
 *
 * @cpar:    The parameters set.
 * @compact: TRUE for compact, FALSE otherwise.
 */
void
purple_request_cpar_set_compact(PurpleRequestCommonParameters *cpar,
	gboolean compact);

/**
 * Gets dialog display mode.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: TRUE for compact, FALSE for default.
 */
gboolean
purple_request_cpar_is_compact(PurpleRequestCommonParameters *cpar);

/**
 * Sets the callback for the Help button.
 *
 * @cpar:      The parameters set.
 * @cb:        The callback.
 * @user_data: The data to be passed to the callback.
 */
void
purple_request_cpar_set_help_cb(PurpleRequestCommonParameters *cpar,
	PurpleRequestHelpCb cb, gpointer user_data);

/**
 * Gets the callback for the Help button.
 *
 * @cpar:      The parameters set (may be %NULL).
 * @user_data: The pointer to the variable, where user data (to be passed
 *                  to callback function) should be stored.
 *
 * Returns: The callback.
 */
PurpleRequestHelpCb
purple_request_cpar_get_help_cb(PurpleRequestCommonParameters *cpar,
	gpointer *user_data);

/**
 * Sets extra actions for the PurpleRequestFields dialog.
 *
 * @cpar: The parameters set.
 * @...:  A list of actions. These are pairs of arguments. The first of
 *             each pair is the <tt>char *</tt> label that appears on the
 *             button. It should have an underscore before the letter you want
 *             to use as the accelerator key for the button. The second of each
 *             pair is the #PurpleRequestFieldsCb function to use when the
 *             button is clicked. Should be terminated with the NULL label.
 */
void
purple_request_cpar_set_extra_actions(PurpleRequestCommonParameters *cpar, ...);

/**
 * Gets extra actions for the PurpleRequestFields dialog.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: A list of actions (pairs of arguments, as in setter).
 */
GSList *
purple_request_cpar_get_extra_actions(PurpleRequestCommonParameters *cpar);

/**
 * Sets the same parent window for this dialog, as the parent of specified
 * Notify API or Request API dialog UI handle.
 *
 * @cpar:      The parameters set.
 * @ui_handle: The UI handle.
 */
void
purple_request_cpar_set_parent_from(PurpleRequestCommonParameters *cpar,
	gpointer ui_handle);

/**
 * Gets the parent "donor" for this dialog.
 *
 * @cpar: The parameters set (may be %NULL).
 *
 * Returns: The donors UI handle.
 */
gpointer
purple_request_cpar_get_parent_from(PurpleRequestCommonParameters *cpar);

/*@}*/

/**************************************************************************/
/** @name Field List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a list of fields to pass to purple_request_fields().
 *
 * Returns: A PurpleRequestFields structure.
 */
PurpleRequestFields *purple_request_fields_new(void);

/**
 * Destroys a list of fields.
 *
 * @fields: The list of fields to destroy.
 */
void purple_request_fields_destroy(PurpleRequestFields *fields);

/**
 * Adds a group of fields to the list.
 *
 * @fields: The fields list.
 * @group:  The group to add.
 */
void purple_request_fields_add_group(PurpleRequestFields *fields,
								   PurpleRequestFieldGroup *group);

/**
 * Returns a list of all groups in a field list.
 *
 * @fields: The fields list.
 *
 * Returns: (TODO const): A list of groups.
 */
GList *purple_request_fields_get_groups(const PurpleRequestFields *fields);

/**
 * Set tab names for a field list.
 *
 * @fields:    The fields list.
 * @tab_names: NULL-terminated array of localized tab labels,
 *                  may be %NULL.
 */
void purple_request_fields_set_tab_names(PurpleRequestFields *fields,
	const gchar **tab_names);

/**
 * Returns tab names of a field list.
 *
 * @fields: The fields list.
 *
 * Returns: NULL-terminated array of localized tab labels, or NULL if tabs
 *         are disabled.
 */
const gchar **
purple_request_fields_get_tab_names(const PurpleRequestFields *fields);

/**
 * Returns whether or not the field with the specified ID exists.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: TRUE if the field exists, or FALSE.
 */
gboolean purple_request_fields_exists(const PurpleRequestFields *fields,
									const char *id);

/**
 * Returns a list of all required fields.
 *
 * @fields: The fields list.
 *
 * Returns: (TODO const): The list of required fields.
 */
const GList *purple_request_fields_get_required(
	const PurpleRequestFields *fields);

/**
 * Returns a list of all validated fields.
 *
 * @fields: The fields list.
 *
 * Returns: (TODO const): The list of validated fields.
 */
const GList *purple_request_fields_get_validatable(
	const PurpleRequestFields *fields);

/**
 * Returns a list of all fields with sensitivity callback added.
 *
 * @fields: The fields list.
 *
 * Returns: (TODO const): The list of fields with automatic sensitivity callback.
 */
const GList *
purple_request_fields_get_autosensitive(const PurpleRequestFields *fields);

/**
 * Returns whether or not a field with the specified ID is required.
 *
 * @fields: The fields list.
 * @id:     The field ID.
 *
 * Returns: TRUE if the specified field is required, or FALSE.
 */
gboolean purple_request_fields_is_field_required(const PurpleRequestFields *fields,
											   const char *id);

/**
 * Returns whether or not all required fields have values.
 *
 * @fields: The fields list.
 *
 * Returns: TRUE if all required fields have values, or FALSE.
 */
gboolean purple_request_fields_all_required_filled(
	const PurpleRequestFields *fields);

/**
 * Returns whether or not all fields are valid.
 *
 * @fields: The fields list.
 *
 * Returns: TRUE if all fields are valid, or FALSE.
 */
gboolean purple_request_fields_all_valid(const PurpleRequestFields *fields);

/**
 * Return the field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The field, if found.
 */
PurpleRequestField *purple_request_fields_get_field(
		const PurpleRequestFields *fields, const char *id);

/**
 * Returns the string value of a field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The string value, if found, or %NULL otherwise.
 */
const char *purple_request_fields_get_string(const PurpleRequestFields *fields,
										   const char *id);

/**
 * Returns the integer value of a field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The integer value, if found, or 0 otherwise.
 */
int purple_request_fields_get_integer(const PurpleRequestFields *fields,
									const char *id);

/**
 * Returns the boolean value of a field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The boolean value, if found, or %FALSE otherwise.
 */
gboolean purple_request_fields_get_bool(const PurpleRequestFields *fields,
									  const char *id);

/**
 * Returns the choice index of a field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The choice value, if found, or NULL otherwise.
 */
gpointer
purple_request_fields_get_choice(const PurpleRequestFields *fields,
	const char *id);

/**
 * Returns the account of a field with the specified ID.
 *
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns: The account value, if found, or NULL otherwise.
 */
PurpleAccount *purple_request_fields_get_account(const PurpleRequestFields *fields,
											 const char *id);

/**
 * Returns the UI data associated with this object.
 *
 * @fields: The fields list.
 *
 * Returns: The UI data associated with this object.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libuprple core.
 */
gpointer purple_request_fields_get_ui_data(const PurpleRequestFields *fields);

/**
 * Set the UI data associated with this object.
 *
 * @fields: The fields list.
 * @ui_data: A pointer to associate with this object.
 */
void purple_request_fields_set_ui_data(PurpleRequestFields *fields, gpointer ui_data);

/*@}*/

/**************************************************************************/
/** @name Fields Group API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a fields group with an optional title.
 *
 * @title: The optional title to give the group.
 *
 * Returns: A new fields group
 */
PurpleRequestFieldGroup *purple_request_field_group_new(const char *title);

/**
 * Sets tab number for a group.
 *
 * @group:  The group.
 * @tab_no: The tab number.
 *
 * @see purple_request_fields_set_tab_names
 */
void purple_request_field_group_set_tab(PurpleRequestFieldGroup *group,
	guint tab_no);

/**
 * Returns tab number of a group.
 *
 * @group: The group.
 *
 * Returns: Tab number.
 *
 * @see purple_request_fields_get_tab_names
 */
guint purple_request_field_group_get_tab(const PurpleRequestFieldGroup *group);

/**
 * Destroys a fields group.
 *
 * @group: The group to destroy.
 */
void purple_request_field_group_destroy(PurpleRequestFieldGroup *group);

/**
 * Adds a field to the group.
 *
 * @group: The group to add the field to.
 * @field: The field to add to the group.
 */
void purple_request_field_group_add_field(PurpleRequestFieldGroup *group,
										PurpleRequestField *field);

/**
 * Returns the title of a fields group.
 *
 * @group: The group.
 *
 * Returns: The title, if set.
 */
const char *purple_request_field_group_get_title(
		const PurpleRequestFieldGroup *group);

/**
 * Returns a list of all fields in a group.
 *
 * @group: The group.
 *
 * Returns: (TODO const): The list of fields in the group.
 */
GList *purple_request_field_group_get_fields(
		const PurpleRequestFieldGroup *group);

/**
 * Returns a list of all fields in a group.
 *
 * @group: The group.
 *
 * Returns: (TODO const): The list of fields in the group.
 */
PurpleRequestFields *purple_request_field_group_get_fields_list(
		const PurpleRequestFieldGroup *group);

/*@}*/

/**************************************************************************/
/** @name Field API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a field of the specified type.
 *
 * @id:   The field ID.
 * @text: The text label of the field.
 * @type: The type of field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_new(const char *id, const char *text,
										 PurpleRequestFieldType type);

/**
 * Destroys a field.
 *
 * @field: The field to destroy.
 */
void purple_request_field_destroy(PurpleRequestField *field);

/**
 * Sets the label text of a field.
 *
 * @field: The field.
 * @label: The text label.
 */
void purple_request_field_set_label(PurpleRequestField *field, const char *label);

/**
 * Sets whether or not a field is visible.
 *
 * @field:   The field.
 * @visible: TRUE if visible, or FALSE if not.
 */
void purple_request_field_set_visible(PurpleRequestField *field, gboolean visible);

/**
 * Sets the type hint for the field.
 *
 * This is optionally used by the UIs to provide such features as
 * auto-completion for type hints like "account" and "screenname".
 *
 * @field:     The field.
 * @type_hint: The type hint.
 */
void purple_request_field_set_type_hint(PurpleRequestField *field,
									  const char *type_hint);

/**
 * Sets the tooltip for the field.
 *
 * This is optionally used by the UIs to provide a tooltip for
 * the field.
 *
 * @field:     The field.
 * @tooltip:   The tooltip text.
 */
void purple_request_field_set_tooltip(PurpleRequestField *field,
									const char *tooltip);

/**
 * Sets whether or not a field is required.
 *
 * @field:    The field.
 * @required: TRUE if required, or FALSE.
 */
void purple_request_field_set_required(PurpleRequestField *field,
									 gboolean required);

/**
 * Returns the type of a field.
 *
 * @field: The field.
 *
 * Returns: The field's type.
 */
PurpleRequestFieldType purple_request_field_get_type(const PurpleRequestField *field);

/**
 * Returns the group for the field.
 *
 * @field: The field.
 *
 * Returns: The UI data.
 */
PurpleRequestFieldGroup *purple_request_field_get_group(const PurpleRequestField *field);

/**
 * Returns the ID of a field.
 *
 * @field: The field.
 *
 * Returns: The ID
 */
const char *purple_request_field_get_id(const PurpleRequestField *field);

/**
 * Returns the label text of a field.
 *
 * @field: The field.
 *
 * Returns: The label text.
 */
const char *purple_request_field_get_label(const PurpleRequestField *field);

/**
 * Returns whether or not a field is visible.
 *
 * @field: The field.
 *
 * Returns: TRUE if the field is visible. FALSE otherwise.
 */
gboolean purple_request_field_is_visible(const PurpleRequestField *field);

/**
 * Returns the field's type hint.
 *
 * @field: The field.
 *
 * Returns: The field's type hint.
 */
const char *purple_request_field_get_type_hint(const PurpleRequestField *field);

/**
 * Returns the field's tooltip.
 *
 * @field: The field.
 *
 * Returns: The field's tooltip.
 */
const char *purple_request_field_get_tooltip(const PurpleRequestField *field);

/**
 * Returns whether or not a field is required.
 *
 * @field: The field.
 *
 * Returns: TRUE if the field is required, or FALSE.
 */
gboolean purple_request_field_is_required(const PurpleRequestField *field);

/**
 * Checks, if specified field has value.
 *
 * @field: The field.
 *
 * Returns: TRUE if the field has value, or FALSE.
 */
gboolean purple_request_field_is_filled(const PurpleRequestField *field);

/**
 * Sets validator for a single field.
 *
 * @field: The field.
 * @validator: The validator callback, NULL to disable validation.
 * @user_data: The data to pass to the callback.
 */
void purple_request_field_set_validator(PurpleRequestField *field,
	PurpleRequestFieldValidator validator, void *user_data);

/**
 * Returns whether or not field has validator set.
 *
 * @field: The field.
 *
 * Returns: TRUE if the field has validator, or FALSE.
 */
gboolean purple_request_field_is_validatable(PurpleRequestField *field);

/**
 * Checks, if specified field is valid.
 *
 * If detailed message about failure reason is needed, there is an option to
 * return (via errmsg argument) pointer to newly allocated error message.
 * It must be freed with g_free after use.
 *
 * Note: empty, not required fields are valid.
 *
 * @field: The field.
 * @errmsg: If non-NULL, the memory area, where the pointer to validation
 *        failure message will be set.
 *
 * Returns: TRUE, if the field is valid, FALSE otherwise.
 */
gboolean purple_request_field_is_valid(PurpleRequestField *field, gchar **errmsg);

/**
 * Sets field editable.
 *
 * @field:     The field.
 * @sensitive: TRUE if the field should be sensitive for user input.
 */
void purple_request_field_set_sensitive(PurpleRequestField *field,
	gboolean sensitive);

/**
 * Checks, if field is editable.
 *
 * @field: The field.
 *
 * Returns: TRUE, if the field is sensitive for user input.
 */
gboolean purple_request_field_is_sensitive(PurpleRequestField *field);

/**
 * Sets the callback, used to determine if the field should be editable.
 *
 * @field: The field.
 * @cb:    The callback.
 */
void purple_request_field_set_sensitivity_cb(PurpleRequestField *field,
	PurpleRequestFieldSensitivityCb cb);

/**
 * Returns the ui_data for a field.
 *
 * @field: The field.
 *
 * Returns: The UI data.
 */
gpointer purple_request_field_get_ui_data(const PurpleRequestField *field);

/**
 * Sets the ui_data for a field.
 *
 * @field: The field.
 * @ui_data: The UI data.
 *
 * Returns: The UI data.
 */
void purple_request_field_set_ui_data(PurpleRequestField *field,
                                      gpointer ui_data);

/*@}*/

/**************************************************************************/
/** @name String Field API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a string request field.
 *
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The optional default value.
 * @multiline:     Whether or not this should be a multiline string.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_string_new(const char *id,
												const char *text,
												const char *default_value,
												gboolean multiline);

/**
 * Sets the default value in a string field.
 *
 * @field:         The field.
 * @default_value: The default value.
 */
void purple_request_field_string_set_default_value(PurpleRequestField *field,
												 const char *default_value);

/**
 * Sets the value in a string field.
 *
 * @field: The field.
 * @value: The value.
 */
void purple_request_field_string_set_value(PurpleRequestField *field,
										 const char *value);

/**
 * Sets whether or not a string field is masked
 * (commonly used for password fields).
 *
 * @field:  The field.
 * @masked: The masked value.
 */
void purple_request_field_string_set_masked(PurpleRequestField *field,
										  gboolean masked);

/**
 * Returns the default value in a string field.
 *
 * @field: The field.
 *
 * Returns: The default value.
 */
const char *purple_request_field_string_get_default_value(
		const PurpleRequestField *field);

/**
 * Returns the user-entered value in a string field.
 *
 * @field: The field.
 *
 * Returns: The value.
 */
const char *purple_request_field_string_get_value(const PurpleRequestField *field);

/**
 * Returns whether or not a string field is multi-line.
 *
 * @field: The field.
 *
 * Returns: %TRUE if the field is mulit-line, or %FALSE otherwise.
 */
gboolean purple_request_field_string_is_multiline(const PurpleRequestField *field);

/**
 * Returns whether or not a string field is masked.
 *
 * @field: The field.
 *
 * Returns: %TRUE if the field is masked, or %FALSE otherwise.
 */
gboolean purple_request_field_string_is_masked(const PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Integer Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates an integer field.
 *
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The default value.
 * @lower_bound:   The lower bound.
 * @upper_bound:   The upper bound.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_int_new(const char *id,
	const char *text, int default_value, int lower_bound, int upper_bound);

/**
 * Sets the default value in an integer field.
 *
 * @field:         The field.
 * @default_value: The default value.
 */
void purple_request_field_int_set_default_value(PurpleRequestField *field,
											  int default_value);

/**
 * Sets the lower bound in an integer field.
 *
 * @field:       The field.
 * @lower_bound: The lower bound.
 */
void purple_request_field_int_set_lower_bound(PurpleRequestField *field, int lower_bound);

/**
 * Sets the upper bound in an integer field.
 *
 * @field:       The field.
 * @upper_bound: The upper bound.
 */
void purple_request_field_int_set_upper_bound(PurpleRequestField *field, int lower_bound);

/**
 * Sets the value in an integer field.
 *
 * @field: The field.
 * @value: The value.
 */
void purple_request_field_int_set_value(PurpleRequestField *field, int value);

/**
 * Returns the default value in an integer field.
 *
 * @field: The field.
 *
 * Returns: The default value.
 */
int purple_request_field_int_get_default_value(const PurpleRequestField *field);

/**
 * Returns the lower bound in an integer field.
 *
 * @field: The field.
 *
 * Returns: The lower bound.
 */
int purple_request_field_int_get_lower_bound(const PurpleRequestField *field);

/**
 * Returns the upper bound in an integer field.
 *
 * @field: The field.
 *
 * Returns: The upper bound.
 */
int purple_request_field_int_get_upper_bound(const PurpleRequestField *field);

/**
 * Returns the user-entered value in an integer field.
 *
 * @field: The field.
 *
 * Returns: The value.
 */
int purple_request_field_int_get_value(const PurpleRequestField *field);

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
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The default value.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_bool_new(const char *id,
											  const char *text,
											  gboolean default_value);

/**
 * Sets the default value in an boolean field.
 *
 * @field:         The field.
 * @default_value: The default value.
 */
void purple_request_field_bool_set_default_value(PurpleRequestField *field,
											   gboolean default_value);

/**
 * Sets the value in an boolean field.
 *
 * @field: The field.
 * @value: The value.
 */
void purple_request_field_bool_set_value(PurpleRequestField *field,
									   gboolean value);

/**
 * Returns the default value in an boolean field.
 *
 * @field: The field.
 *
 * Returns: The default value.
 */
gboolean purple_request_field_bool_get_default_value(
		const PurpleRequestField *field);

/**
 * Returns the user-entered value in an boolean field.
 *
 * @field: The field.
 *
 * Returns: The value.
 */
gboolean purple_request_field_bool_get_value(const PurpleRequestField *field);

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
 * @id:            The field ID.
 * @text:          The optional label of the field.
 * @default_value: The default choice.
 *
 * Returns: The new field.
 */
PurpleRequestField *
purple_request_field_choice_new(const char *id, const char *text,
	gpointer default_value);

/**
 * Adds a choice to a multiple choice field.
 *
 * @field: The choice field.
 * @label: The choice label.
 * @data:  The choice value.
 */
void
purple_request_field_choice_add(PurpleRequestField *field, const char *label,
	gpointer data);

/**
 * Sets the default value in an choice field.
 *
 * @field:         The field.
 * @default_value: The default value.
 */
void
purple_request_field_choice_set_default_value(PurpleRequestField *field,
	gpointer default_value);

/**
 * Sets the value in an choice field.
 *
 * @field: The field.
 * @value: The value.
 */
void
purple_request_field_choice_set_value(PurpleRequestField *field,
	gpointer value);

/**
 * Returns the default value in an choice field.
 *
 * @field: The field.
 *
 * Returns: The default value.
 */
gpointer
purple_request_field_choice_get_default_value(const PurpleRequestField *field);

/**
 * Returns the user-entered value in an choice field.
 *
 * @field: The field.
 *
 * Returns: The value.
 */
gpointer
purple_request_field_choice_get_value(const PurpleRequestField *field);

/**
 * Returns a list of elements in a choice field.
 *
 * @field: The field.
 *
 * Returns: (TODO const): The list of pairs <label, value>.
 */
GList *
purple_request_field_choice_get_elements(const PurpleRequestField *field);

/**
 * Sets the destructor for field values.
 *
 * @field:   The field.
 * @destroy: The destroy function.
 */
void
purple_request_field_choice_set_data_destructor(PurpleRequestField *field,
	GDestroyNotify destroy);

/*@}*/

/**************************************************************************/
/** @name List Field API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a multiple list item field.
 *
 * @id:   The field ID.
 * @text: The optional label of the field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_list_new(const char *id, const char *text);

/**
 * Sets whether or not a list field allows multiple selection.
 *
 * @field:        The list field.
 * @multi_select: TRUE if multiple selection is enabled,
 *                     or FALSE otherwise.
 */
void purple_request_field_list_set_multi_select(PurpleRequestField *field,
											  gboolean multi_select);

/**
 * Returns whether or not a list field allows multiple selection.
 *
 * @field: The list field.
 *
 * Returns: TRUE if multiple selection is enabled, or FALSE otherwise.
 */
gboolean purple_request_field_list_get_multi_select(
	const PurpleRequestField *field);

/**
 * Returns the data for a particular item.
 *
 * @field: The list field.
 * @text:  The item text.
 *
 * Returns: The data associated with the item.
 */
void *purple_request_field_list_get_data(const PurpleRequestField *field,
									   const char *text);

/**
 * Adds an item to a list field.
 *
 * @field: The list field.
 * @item:  The list item.
 * @icon_path: The path to icon file, or %NULL for no icon.
 * @data:  The associated data.
 */
void purple_request_field_list_add_icon(PurpleRequestField *field,
								 const char *item, const char* icon_path, void* data);

/**
 * Adds a selected item to the list field.
 *
 * @field: The field.
 * @item:  The item to add.
 */
void purple_request_field_list_add_selected(PurpleRequestField *field,
										  const char *item);

/**
 * Clears the list of selected items in a list field.
 *
 * @field: The field.
 */
void purple_request_field_list_clear_selected(PurpleRequestField *field);

/**
 * Sets a list of selected items in a list field.
 *
 * @field: The field.
 * @items: The list of selected items, which is not modified or freed.
 */
void purple_request_field_list_set_selected(PurpleRequestField *field,
										  GList *items);

/**
 * Returns whether or not a particular item is selected in a list field.
 *
 * @field: The field.
 * @item:  The item.
 *
 * Returns: TRUE if the item is selected. FALSE otherwise.
 */
gboolean purple_request_field_list_is_selected(const PurpleRequestField *field,
											 const char *item);

/**
 * Returns a list of selected items in a list field.
 *
 * To retrieve the data for each item, use
 * purple_request_field_list_get_data().
 *
 * @field: The field.
 *
 * Returns: (TODO const): The list of selected items.
 */
GList *purple_request_field_list_get_selected(
	const PurpleRequestField *field);

/**
 * Returns a list of items in a list field.
 *
 * @field: The field.
 *
 * Returns: (TODO const): The list of items.
 */
GList *purple_request_field_list_get_items(const PurpleRequestField *field);

/**
 * Returns a list of icons in a list field.
 *
 * The icons will correspond with the items, in order.
 *
 * @field: The field.
 *
 * Returns: (TODO const): The list of icons or %NULL (i.e. the empty GList) if no
 *              items have icons.
 */
GList *purple_request_field_list_get_icons(const PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Label Field API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Creates a label field.
 *
 * @id:   The field ID.
 * @text: The label of the field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_label_new(const char *id,
											   const char *text);

/*@}*/

/**************************************************************************/
/** @name Image Field API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Creates an image field.
 *
 * @id:   The field ID.
 * @text: The label of the field.
 * @buf:  The image data.
 * @size: The size of the data in @a buffer.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_image_new(const char *id, const char *text,
											   const char *buf, gsize size);

/**
 * Sets the scale factors of an image field.
 *
 * @field: The image field.
 * @x:     The x scale factor.
 * @y:     The y scale factor.
 */
void purple_request_field_image_set_scale(PurpleRequestField *field, unsigned int x, unsigned int y);

/**
 * Returns pointer to the image.
 *
 * @field: The image field.
 *
 * Returns: Pointer to the image.
 */
const char *purple_request_field_image_get_buffer(PurpleRequestField *field);

/**
 * Returns size (in bytes) of the image.
 *
 * @field: The image field.
 *
 * Returns: Size of the image.
 */
gsize purple_request_field_image_get_size(PurpleRequestField *field);

/**
 * Returns X scale coefficient of the image.
 *
 * @field: The image field.
 *
 * Returns: X scale coefficient of the image.
 */
unsigned int purple_request_field_image_get_scale_x(PurpleRequestField *field);

/**
 * Returns Y scale coefficient of the image.
 *
 * @field: The image field.
 *
 * Returns: Y scale coefficient of the image.
 */
unsigned int purple_request_field_image_get_scale_y(PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Account Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates an account field.
 *
 * By default, this field will not show offline accounts.
 *
 * @id:      The field ID.
 * @text:    The text label of the field.
 * @account: The optional default account.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_account_new(const char *id,
												 const char *text,
												 PurpleAccount *account);

/**
 * Sets the default account on an account field.
 *
 * @field:         The account field.
 * @default_value: The default account.
 */
void purple_request_field_account_set_default_value(PurpleRequestField *field,
												  PurpleAccount *default_value);

/**
 * Sets the account in an account field.
 *
 * @field: The account field.
 * @value: The account.
 */
void purple_request_field_account_set_value(PurpleRequestField *field,
										  PurpleAccount *value);

/**
 * Sets whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 *
 * @field:    The account field.
 * @show_all: Whether or not to show all accounts.
 */
void purple_request_field_account_set_show_all(PurpleRequestField *field,
											 gboolean show_all);

/**
 * Sets the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 *
 * @field:       The account field.
 * @filter_func: The account filter function.
 */
void purple_request_field_account_set_filter(PurpleRequestField *field,
										   PurpleFilterAccountFunc filter_func);

/**
 * Returns the default account in an account field.
 *
 * @field: The field.
 *
 * Returns: The default account.
 */
PurpleAccount *purple_request_field_account_get_default_value(
		const PurpleRequestField *field);

/**
 * Returns the user-entered account in an account field.
 *
 * @field: The field.
 *
 * Returns: The user-entered account.
 */
PurpleAccount *purple_request_field_account_get_value(
		const PurpleRequestField *field);

/**
 * Returns whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 *
 * @field:    The account field.
 * Returns: Whether or not to show all accounts.
 */
gboolean purple_request_field_account_get_show_all(
		const PurpleRequestField *field);

/**
 * Returns the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 *
 * @field:       The account field.
 *
 * Returns: The account filter function.
 */
PurpleFilterAccountFunc purple_request_field_account_get_filter(
		const PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Certificate Field API                                           */
/**************************************************************************/
/*@{*/

/**
 * Creates a certificate field.
 *
 * @id:   The field ID.
 * @text: The label of the field.
 * @cert: The certificate of the field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_certificate_new(const char *id,
														 const char *text,
														 PurpleCertificate *cert);

/**
 * Returns the certificate in a certificate field.
 *
 * @field: The field.
 *
 * Returns: The certificate.
 */
PurpleCertificate *purple_request_field_certificate_get_value(
		const PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Datasheet Field API                                             */
/**************************************************************************/
/*@{*/

/**
 * Creates a datasheet item field.
 *
 * @id:    The field ID.
 * @text:  The label of the field, may be %NULL.
 * @sheet: The datasheet.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_datasheet_new(const char *id,
	const gchar *text, PurpleRequestDatasheet *sheet);

/**
 * Returns a datasheet for a field.
 *
 * @field: The field.
 *
 * Returns: (TODO const): The datasheet object.
 */
PurpleRequestDatasheet *purple_request_field_datasheet_get_sheet(
	PurpleRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Validators for request fields.                                  */
/**************************************************************************/
/*@{*/

/**
 * Validates a field which should contain an email address.
 *
 * @see purple_request_field_set_validator
 *
 * @field: The field.
 * @errmsg: (Optional) destination for error message.
 * @user_data: Ignored.
 *
 * Returns: TRUE, if field contains valid email address.
 */
gboolean purple_request_field_email_validator(PurpleRequestField *field,
	gchar **errmsg, void *user_data);

/**
 * Validates a field which should contain alphanumeric content.
 *
 * @see purple_request_field_set_validator
 *
 * @field: The field.
 * @errmsg: (Optional) destination for error message.
 * @user_data: (Optional) allowed character list (NULL-terminated string).
 *
 * Returns: TRUE, if field contains only alphanumeric characters.
 */
gboolean purple_request_field_alphanumeric_validator(PurpleRequestField *field,
	gchar **errmsg, void *allowed_characters);

/*@}*/

/**************************************************************************/
/** @name Request API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Prompts the user for text input.
 *
 * @handle:        The plugin or connection handle.  For some
 *                      things this is <em>extremely</em> important.  The
 *                      handle is used to programmatically close the request
 *                      dialog when it is no longer needed.  For PRPLs this
 *                      is often a pointer to the #PurpleConnection
 *                      instance.  For plugins this should be a similar,
 *                      unique memory location.  This value is important
 *                      because it allows a request to be closed with
 *                      purple_request_close_with_handle() when, for
 *                      example, you sign offline.  If the request is
 *                      <em>not</em> closed it is <strong>very</strong>
 *                      likely to cause a crash whenever the callback
 *                      handler functions are triggered.
 * @title:         The title of the message, or %NULL if it should have
 *                      no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                      feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @default_value: The default value.
 * @multiline:     %TRUE if the inputted text can span multiple lines.
 * @masked:        %TRUE if the inputted text should be masked in some
 *                      way (such as by displaying characters as stars).  This
 *                      might be because the input is some kind of password.
 * @hint:          Optionally suggest how the input box should appear.
 *                      Use "html", for example, to allow the user to enter
 *                      HTML.
 * @ok_text:       The text for the @c OK button, which may not be %NULL.
 * @ok_cb:         The callback for the @c OK button, which may not be @c
 *                      NULL.
 * @cancel_text:   The text for the @c Cancel button, which may not be @c
 *                      NULL.
 * @cancel_cb:     The callback for the @c Cancel button, which may be
 *                      %NULL.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                      unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *purple_request_input(void *handle, const char *title, const char *primary,
	const char *secondary, const char *default_value, gboolean multiline,
	gboolean masked, gchar *hint,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar,
	void *user_data);

/**
 * Prompts the user for multiple-choice input.
 *
 * @handle:        The plugin or connection handle.  For some things this
 *                      is <em>extremely</em> important.  See the comments on
 *                      purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                      no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                      feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @default_value: The default choice; this should be one of the values
 *                      listed in the varargs.
 * @ok_text:       The text for the @c OK button, which may not be %NULL.
 * @ok_cb:         The callback for the @c OK button, which may not be @c
 *                      NULL.
 * @cancel_text:   The text for the @c Cancel button, which may not be @c
 *                      NULL.
 * @cancel_cb:     The callback for the @c Cancel button, or %NULL to
 *                      do nothing.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                      unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 * @...:           The choices, which should be pairs of <tt>char *</tt>
 *                      descriptions and <tt>int</tt> values, terminated with a
 *                      %NULL parameter.
 *
 * Returns: A UI-specific handle.
 */
void *purple_request_choice(void *handle, const char *title, const char *primary,
	const char *secondary, gpointer default_value,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar,
	void *user_data, ...) G_GNUC_NULL_TERMINATED;

/**
 * <tt>va_list</tt> version of purple_request_choice(); see its documentation.
 */
void *purple_request_choice_varg(void *handle, const char *title,
	const char *primary, const char *secondary, gpointer default_value,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar,
	void *user_data, va_list choices);

/**
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @handle:         The plugin or connection handle.  For some things this
 *                       is <em>extremely</em> important.  See the comments on
 *                       purple_request_input().
 * @title:          The title of the message, or %NULL if it should have
 *                       no title.
 * @primary:        The main point of the message, or %NULL if you're
 *                       feeling enigmatic.
 * @secondary:      Secondary information, or %NULL if there is none.
 * @default_action: The default action, zero-indexed; if the third action
 *                       supplied should be the default, supply <tt>2</tt>.
 *                       The should be the action that users are most likely
 *                       to select.
 * @cpar:           The #PurpleRequestCommonParameters object, which gets
 *                       unref'ed after this call.
 * @user_data:      The data to pass to the callback.
 * @action_count:   The number of actions.
 * @...:            A list of actions.  These are pairs of
 *                       arguments.  The first of each pair is the
 *                       <tt>char *</tt> label that appears on the button.  It
 *                       should have an underscore before the letter you want
 *                       to use as the accelerator key for the button.  The
 *                       second of each pair is the #PurpleRequestActionCb
 *                       function to use when the button is clicked.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_action(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, ...);

/**
 * <tt>va_list</tt> version of purple_request_action(); see its documentation.
 */
void *
purple_request_action_varg(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, va_list actions);

/**
 * Displays a "please wait" dialog.
 *
 * @handle:        The plugin or connection handle.  For some things this
 *                      is <em>extremely</em> important.  See the comments on
 *                      purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                      default title.
 * @primary:       The main point of the message, or %NULL if you're
 *                      feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @with_progress: %TRUE, if we want to display progress bar, %FALSE
 *                      otherwise
 * @cancel_cb:     The callback for the @c Cancel button, which may be
 *                      %NULL.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                      unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_wait(void *handle, const char *title, const char *primary,
	const char *secondary, gboolean with_progress,
	PurpleRequestCancelCb cancel_cb, PurpleRequestCommonParameters *cpar,
	void *user_data);

/**
 * Notifies the "please wait" dialog that some progress has been made, but you
 * don't know how much.
 *
 * @ui_handle: The request UI handle.
 */
void
purple_request_wait_pulse(void *ui_handle);

/**
 * Notifies the "please wait" dialog about progress has been made.
 *
 * @ui_handle: The request UI handle.
 * @fraction:  The part of task that is done (between 0.0 and 1.0,
 *                  inclusive).
 */
void
purple_request_wait_progress(void *ui_handle, gfloat fraction);

/**
 * Displays groups of fields for the user to fill in.
 *
 * @handle:      The plugin or connection handle.  For some things this
 *                    is <em>extremely</em> important.  See the comments on
 *                    purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *                    no title.
 * @primary:     The main point of the message, or %NULL if you're
 *                    feeling enigmatic.
 * @secondary:   Secondary information, or %NULL if there is none.
 * @fields:      The list of fields.
 * @ok_text:     The text for the @c OK button, which may not be %NULL.
 * @ok_cb:       The callback for the @c OK button, which may not be @c
 *                    NULL.
 * @cancel_text: The text for the @c Cancel button, which may not be @c
 *                    NULL.
 * @cancel_cb:   The callback for the @c Cancel button, which may be
 *                    %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *                    unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_fields(void *handle, const char *title, const char *primary,
	const char *secondary, PurpleRequestFields *fields,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar,
	void *user_data);

/**
 * Checks, if passed UI handle is valid.
 *
 * @ui_handle: The UI handle.
 * @type:      The pointer to variable, where request type may be stored
 *                  (may be %NULL).
 *
 * Returns: TRUE, if handle is valid, FALSE otherwise.
 */
gboolean
purple_request_is_valid_ui_handle(void *ui_handle, PurpleRequestType *type);

/**
 * Adds a function called when notification dialog is closed.
 *
 * @ui_handle:   The UI handle.
 * @notify:      The function to be called.
 * @notify_data: The data to be passed to the callback function.
 */
void
purple_request_add_close_notify(void *ui_handle, GDestroyNotify notify,
	gpointer notify_data);

/**
 * Closes a request.
 *
 * @type:     The request type.
 * @uihandle: The request UI handle.
 */
void purple_request_close(PurpleRequestType type, void *uihandle);

/**
 * Closes all requests registered with the specified handle.
 *
 * @handle: The handle, as supplied as the @a handle parameter to one of the
 *               <tt>purple_request_*</tt> functions.
 *
 * @see purple_request_input().
 */
void purple_request_close_with_handle(void *handle);

/**
 * A wrapper for purple_request_action() that uses @c Yes and @c No buttons.
 */
#define purple_request_yes_no(handle, title, primary, secondary, \
	default_action, cpar, user_data, yes_cb, no_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_Yes"), (yes_cb), \
		_("_No"), (no_cb))

/**
 * A wrapper for purple_request_action() that uses @c OK and @c Cancel buttons.
 */
#define purple_request_ok_cancel(handle, title, primary, secondary, \
	default_action, cpar, user_data, ok_cb, cancel_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_OK"), (ok_cb), \
		_("_Cancel"), (cancel_cb))

/**
 * A wrapper for purple_request_action() that uses Accept and Cancel buttons.
 */
#define purple_request_accept_cancel(handle, title, primary, secondary, \
	default_action, cpar, user_data, accept_cb, cancel_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_Accept"), \
		(accept_cb), _("_Cancel"), (cancel_cb))

/**
 * Displays a file selector request dialog.  Returns the selected filename to
 * the callback.  Can be used for either opening a file or saving a file.
 *
 * @handle:      The plugin or connection handle.  For some things this
 *                    is <em>extremely</em> important.  See the comments on
 *                    purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *                    no title.
 * @filename:    The default filename (may be %NULL)
 * @savedialog:  True if this dialog is being used to save a file.
 *                    False if it is being used to open a file.
 * @ok_cb:       The callback for the @c OK button.
 * @cancel_cb:   The callback for the @c Cancel button, which may be %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *                    unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_file(void *handle, const char *title, const char *filename,
	gboolean savedialog, GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data);

/**
 * Displays a folder select dialog. Returns the selected filename to
 * the callback.
 *
 * @handle:      The plugin or connection handle.  For some things this
 *                    is <em>extremely</em> important.  See the comments on
 *                    purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *                    no title.
 * @dirname:     The default directory name (may be %NULL)
 * @ok_cb:       The callback for the @c OK button.
 * @cancel_cb:   The callback for the @c Cancel button, which may be %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *                    unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_folder(void *handle, const char *title, const char *dirname,
	GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data);

/**
 * Prompts the user for action over a certificate.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @handle:        The plugin or connection handle.  For some things this
 *                      is <em>extremely</em> important.  See the comments on
 *                      purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                      no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                      feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @cert:          The #PurpleCertificate associated with this request.
 * @ok_text:       The text for the @c OK button, which may not be %NULL.
 * @ok_cb:         The callback for the @c OK button, which may not be
 *                      %NULL.
 * @cancel_text:   The text for the @c Cancel button, which may not be
 *                      %NULL.
 * @cancel_cb:     The callback for the @c Cancel button, which may be
 *                      %NULL.
 * @user_data:     The data to pass to the callback.
 *
 * Returns: A UI-specific handle.
 */
void *purple_request_certificate(void *handle, const char *title,
	const char *primary, const char *secondary, PurpleCertificate *cert,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	void *user_data);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when displaying a
 * request.
 *
 * @ops: The UI operations structure.
 */
void purple_request_set_ui_ops(PurpleRequestUiOps *ops);

/**
 * Returns the UI operations structure to be used when displaying a
 * request.
 *
 * Returns: The UI operations structure.
 */
PurpleRequestUiOps *purple_request_get_ui_ops(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_REQUEST_H_ */
