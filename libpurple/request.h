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
/**
 * SECTION:request
 * @section_id: libpurple-request
 * @short_description: <filename>request.h</filename>
 * @title: Request API
 */

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

#include "certificate.h"
#include "conversation.h"
#include "request-datasheet.h"

#define PURPLE_TYPE_REQUEST_UI_OPS (purple_request_ui_ops_get_type())

/**
 * PurpleRequestField:
 *
 * A request field.
 */
typedef struct _PurpleRequestField PurpleRequestField;

/**
 * PurpleRequestFields:
 *
 * Multiple fields request data.
 */
typedef struct _PurpleRequestFields PurpleRequestFields;

/**
 * PurpleRequestFieldGroup:
 *
 * A group of fields with a title.
 */
typedef struct _PurpleRequestFieldGroup PurpleRequestFieldGroup;

/**
 * PurpleRequestCommonParameters:
 *
 * Common parameters for UI operations.
 */
typedef struct _PurpleRequestCommonParameters PurpleRequestCommonParameters;

typedef struct _PurpleRequestUiOps PurpleRequestUiOps;

#include "account.h"

#define PURPLE_DEFAULT_ACTION_NONE	-1

/**
 * PurpleRequestType:
 * @PURPLE_REQUEST_INPUT:  Text input request.
 * @PURPLE_REQUEST_CHOICE: Multiple-choice request.
 * @PURPLE_REQUEST_ACTION: Action request.
 * @PURPLE_REQUEST_WAIT:   Please wait dialog.
 * @PURPLE_REQUEST_FIELDS: Multiple fields request.
 * @PURPLE_REQUEST_FILE:   File open or save request.
 * @PURPLE_REQUEST_FOLDER: Folder selection request.
 *
 * Request types.
 */
typedef enum
{
	PURPLE_REQUEST_INPUT = 0,
	PURPLE_REQUEST_CHOICE,
	PURPLE_REQUEST_ACTION,
	PURPLE_REQUEST_WAIT,
	PURPLE_REQUEST_FIELDS,
	PURPLE_REQUEST_FILE,
	PURPLE_REQUEST_FOLDER

} PurpleRequestType;

/**
 * PurpleRequestFieldType:
 *
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
 * PurpleRequestUiOps:
 * @request_input:       See purple_request_input().
 * @request_choice:      See purple_request_choice_varg().
 * @request_action:      See purple_request_action_varg().
 * @request_wait:        See purple_request_wait().
 * @request_wait_update: See purple_request_wait_pulse(),
 *                           purple_request_wait_progress().
 * @request_fields:      See purple_request_fields().
 * @request_file:        See purple_request_file().
 * @request_folder:      See purple_request_folder().
 *
 * Request UI operations.
 */
struct _PurpleRequestUiOps
{
	PurpleRequestFeature features;

	void *(*request_input)(const char *title, const char *primary,
		const char *secondary, const char *default_value,
		gboolean multiline, gboolean masked, gchar *hint,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void *(*request_choice)(const char *title, const char *primary,
		const char *secondary, gpointer default_value,
		const char *ok_text, GCallback ok_cb, const char *cancel_text,
		GCallback cancel_cb, PurpleRequestCommonParameters *cpar,
		void *user_data, va_list choices);

	void *(*request_action)(const char *title, const char *primary,
		const char *secondary, int default_action,
		PurpleRequestCommonParameters *cpar, void *user_data,
		size_t action_count, va_list actions);

	void *(*request_wait)(const char *title, const char *primary,
		const char *secondary, gboolean with_progress,
		PurpleRequestCancelCb cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void (*request_wait_update)(void *ui_handle, gboolean pulse,
		gfloat fraction);

	void *(*request_fields)(const char *title, const char *primary,
		const char *secondary, PurpleRequestFields *fields,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void *(*request_file)(const char *title, const char *filename,
		gboolean savedialog, GCallback ok_cb, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void *(*request_folder)(const char *title, const char *dirname,
		GCallback ok_cb, GCallback cancel_cb,
		PurpleRequestCommonParameters *cpar, void *user_data);

	void (*close_request)(PurpleRequestType type, void *ui_handle);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

typedef void (*PurpleRequestInputCb)(void *, const char *);

typedef gboolean (*PurpleRequestFieldValidator)(PurpleRequestField *field,
	gchar **errmsg, gpointer user_data);

typedef gboolean (*PurpleRequestFieldSensitivityCb)(PurpleRequestField *field);

/**
 * PurpleRequestActionCb:
 *
 * The type of callbacks passed to purple_request_action().  The first
 * argument is the <literal>user_data</literal> parameter; the second is the
 * index in the list of actions of the one chosen.
 */
typedef void (*PurpleRequestActionCb)(void *, int);

typedef void (*PurpleRequestChoiceCb)(void *, gpointer);
typedef void (*PurpleRequestFieldsCb)(void *, PurpleRequestFields *fields);
typedef void (*PurpleRequestFileCb)(void *, const char *filename);
typedef void (*PurpleRequestHelpCb)(gpointer);

G_BEGIN_DECLS

/**************************************************************************/
/* Common parameters API                                                  */
/**************************************************************************/

/**
 * purple_request_cpar_new:
 *
 * Creates new parameters set for the request, which may or may not be used by
 * the UI to display the request.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_new(void);

/**
 * purple_request_cpar_from_connection:
 *
 * Creates new parameters set initially bound with the #PurpleConnection.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_connection(PurpleConnection *gc);

/**
 * purple_request_cpar_from_account:
 *
 * Creates new parameters set initially bound with the #PurpleAccount.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_account(PurpleAccount *account);

/**
 * purple_request_cpar_from_conversation:
 *
 * Creates new parameters set initially bound with the #PurpleConversation.
 *
 * Returns: The new parameters set.
 */
PurpleRequestCommonParameters *
purple_request_cpar_from_conversation(PurpleConversation *conv);

/**
 * purple_request_cpar_ref:
 * @cpar: The object to ref.
 *
 * Increases the reference count on the parameters set.
 */
void
purple_request_cpar_ref(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_unref:
 * @cpar: The parameters set object to unref and possibly destroy.
 *
 * Decreases the reference count on the parameters set.
 *
 * The object will be destroyed when this reaches 0.
 *
 * Returns: The NULL, if object was destroyed, cpar otherwise.
 */
PurpleRequestCommonParameters *
purple_request_cpar_unref(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_account:
 * @cpar:    The parameters set.
 * @account: The #PurpleAccount to associate.
 *
 * Sets the #PurpleAccount associated with the request, or %NULL, if none is.
 */
void
purple_request_cpar_set_account(PurpleRequestCommonParameters *cpar,
	PurpleAccount *account);

/**
 * purple_request_cpar_get_account:
 * @cpar: The parameters set (may be %NULL).
 *
 * Gets the #PurpleAccount associated with the request.
 *
 * Returns: The associated #PurpleAccount, or NULL if none is.
 */
PurpleAccount *
purple_request_cpar_get_account(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_conversation:
 * @cpar: The parameters set.
 * @conv: The #PurpleConversation to associate.
 *
 * Sets the #PurpleConversation associated with the request, or %NULL, if
 * none is.
 */
void
purple_request_cpar_set_conversation(PurpleRequestCommonParameters *cpar,
	PurpleConversation *conv);

/**
 * purple_request_cpar_get_conversation:
 * @cpar: The parameters set (may be %NULL).
 *
 * Gets the #PurpleConversation associated with the request.
 *
 * Returns: The associated #PurpleConversation, or NULL if none is.
 */
PurpleConversation *
purple_request_cpar_get_conversation(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_icon:
 * @cpar:      The parameters set.
 * @icon_type: The icon type.
 *
 * Sets the icon associated with the request.
 */
void
purple_request_cpar_set_icon(PurpleRequestCommonParameters *cpar,
	PurpleRequestIconType icon_type);

/**
 * purple_request_cpar_get_icon:
 * @cpar: The parameters set.
 *
 * Gets the icon associated with the request.
 *
 * Returns: icon_type The icon type.
 */
PurpleRequestIconType
purple_request_cpar_get_icon(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_custom_icon:
 * @cpar:      The parameters set.
 * @icon_data: The icon image contents (%NULL to reset).
 * @icon_size: The icon image size.
 *
 * Sets the custom icon associated with the request.
 */
void
purple_request_cpar_set_custom_icon(PurpleRequestCommonParameters *cpar,
	gconstpointer icon_data, gsize icon_size);

/**
 * purple_request_cpar_get_custom_icon:
 * @cpar:      The parameters set (may be %NULL).
 * @icon_size: The pointer to variable, where icon size should be stored
 *                  (may be %NULL).
 *
 * Gets the custom icon associated with the request.
 *
 * Returns: The icon image contents.
 */
gconstpointer
purple_request_cpar_get_custom_icon(PurpleRequestCommonParameters *cpar,
	gsize *icon_size);

/**
 * purple_request_cpar_set_html:
 * @cpar:    The parameters set.
 * @enabled: 1, if the text passed with the request contains HTML,
 *                0 otherwise. Don't use any other values, as they may be
 *                redefined in the future.
 *
 * Switches the request text to be HTML or not.
 */
void
purple_request_cpar_set_html(PurpleRequestCommonParameters *cpar,
	gboolean enabled);

/**
 * purple_request_cpar_is_html:
 * @cpar: The parameters set (may be %NULL).
 *
 * Checks, if the text passed to the request is HTML.
 *
 * Returns: %TRUE, if the text is HTML, %FALSE otherwise.
 */
gboolean
purple_request_cpar_is_html(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_compact:
 * @cpar:    The parameters set.
 * @compact: TRUE for compact, FALSE otherwise.
 *
 * Sets dialog display mode to compact or default.
 */
void
purple_request_cpar_set_compact(PurpleRequestCommonParameters *cpar,
	gboolean compact);

/**
 * purple_request_cpar_is_compact:
 * @cpar: The parameters set (may be %NULL).
 *
 * Gets dialog display mode.
 *
 * Returns: TRUE for compact, FALSE for default.
 */
gboolean
purple_request_cpar_is_compact(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_help_cb:
 * @cpar:      The parameters set.
 * @cb:        The callback.
 * @user_data: The data to be passed to the callback.
 *
 * Sets the callback for the Help button.
 */
void
purple_request_cpar_set_help_cb(PurpleRequestCommonParameters *cpar,
	PurpleRequestHelpCb cb, gpointer user_data);

/**
 * purple_request_cpar_get_help_cb:
 * @cpar:      The parameters set (may be %NULL).
 * @user_data: The pointer to the variable, where user data (to be passed
 *                  to callback function) should be stored.
 *
 * Gets the callback for the Help button.
 *
 * Returns: The callback.
 */
PurpleRequestHelpCb
purple_request_cpar_get_help_cb(PurpleRequestCommonParameters *cpar,
	gpointer *user_data);

/**
 * purple_request_cpar_set_extra_actions:
 * @cpar: The parameters set.
 * @...:  A list of actions. These are pairs of arguments. The first of
 *             each pair is the <type>char *</type> label that appears on the
 *             button. It should have an underscore before the letter you want
 *             to use as the accelerator key for the button. The second of each
 *             pair is the #PurpleRequestFieldsCb function to use when the
 *             button is clicked. Should be terminated with the NULL label.
 *
 * Sets extra actions for the PurpleRequestFields dialog.
 */
void
purple_request_cpar_set_extra_actions(PurpleRequestCommonParameters *cpar, ...);

/**
 * purple_request_cpar_get_extra_actions:
 * @cpar: The parameters set (may be %NULL).
 *
 * Gets extra actions for the PurpleRequestFields dialog.
 *
 * Returns: A list of actions (pairs of arguments, as in setter).
 */
GSList *
purple_request_cpar_get_extra_actions(PurpleRequestCommonParameters *cpar);

/**
 * purple_request_cpar_set_parent_from:
 * @cpar:      The parameters set.
 * @ui_handle: The UI handle.
 *
 * Sets the same parent window for this dialog, as the parent of specified
 * Notify API or Request API dialog UI handle.
 */
void
purple_request_cpar_set_parent_from(PurpleRequestCommonParameters *cpar,
	gpointer ui_handle);

/**
 * purple_request_cpar_get_parent_from:
 * @cpar: The parameters set (may be %NULL).
 *
 * Gets the parent "donor" for this dialog.
 *
 * Returns: The donors UI handle.
 */
gpointer
purple_request_cpar_get_parent_from(PurpleRequestCommonParameters *cpar);

/**************************************************************************/
/* Field List API                                                         */
/**************************************************************************/

/**
 * purple_request_fields_new:
 *
 * Creates a list of fields to pass to purple_request_fields().
 *
 * Returns: A PurpleRequestFields structure.
 */
PurpleRequestFields *purple_request_fields_new(void);

/**
 * purple_request_fields_destroy:
 * @fields: The list of fields to destroy.
 *
 * Destroys a list of fields.
 */
void purple_request_fields_destroy(PurpleRequestFields *fields);

/**
 * purple_request_fields_add_group:
 * @fields: The fields list.
 * @group:  The group to add.
 *
 * Adds a group of fields to the list.
 */
void purple_request_fields_add_group(PurpleRequestFields *fields,
								   PurpleRequestFieldGroup *group);

/**
 * purple_request_fields_get_groups:
 * @fields: The fields list.
 *
 * Returns a list of all groups in a field list.
 *
 * Returns: (transfer none): A list of groups.
 */
GList *purple_request_fields_get_groups(const PurpleRequestFields *fields);

/**
 * purple_request_fields_set_tab_names:
 * @fields:    The fields list.
 * @tab_names: NULL-terminated array of localized tab labels,
 *                  may be %NULL.
 *
 * Set tab names for a field list.
 */
void purple_request_fields_set_tab_names(PurpleRequestFields *fields,
	const gchar **tab_names);

/**
 * purple_request_fields_get_tab_names:
 * @fields: The fields list.
 *
 * Returns tab names of a field list.
 *
 * Returns: NULL-terminated array of localized tab labels, or NULL if tabs
 *         are disabled.
 */
const gchar **
purple_request_fields_get_tab_names(const PurpleRequestFields *fields);

/**
 * purple_request_fields_exists:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns whether or not the field with the specified ID exists.
 *
 * Returns: TRUE if the field exists, or FALSE.
 */
gboolean purple_request_fields_exists(const PurpleRequestFields *fields,
									const char *id);

/**
 * purple_request_fields_get_required:
 * @fields: The fields list.
 *
 * Returns a list of all required fields.
 *
 * Returns: (transfer none): The list of required fields.
 */
const GList *purple_request_fields_get_required(
	const PurpleRequestFields *fields);

/**
 * purple_request_fields_get_validatable:
 * @fields: The fields list.
 *
 * Returns a list of all validated fields.
 *
 * Returns: (transfer none): The list of validated fields.
 */
const GList *purple_request_fields_get_validatable(
	const PurpleRequestFields *fields);

/**
 * purple_request_fields_get_autosensitive:
 * @fields: The fields list.
 *
 * Returns a list of all fields with sensitivity callback added.
 *
 * Returns: (transfer none): The list of fields with automatic sensitivity
 *          callback.
 */
const GList *
purple_request_fields_get_autosensitive(const PurpleRequestFields *fields);

/**
 * purple_request_fields_is_field_required:
 * @fields: The fields list.
 * @id:     The field ID.
 *
 * Returns whether or not a field with the specified ID is required.
 *
 * Returns: TRUE if the specified field is required, or FALSE.
 */
gboolean purple_request_fields_is_field_required(const PurpleRequestFields *fields,
											   const char *id);

/**
 * purple_request_fields_all_required_filled:
 * @fields: The fields list.
 *
 * Returns whether or not all required fields have values.
 *
 * Returns: TRUE if all required fields have values, or FALSE.
 */
gboolean purple_request_fields_all_required_filled(
	const PurpleRequestFields *fields);

/**
 * purple_request_fields_all_valid:
 * @fields: The fields list.
 *
 * Returns whether or not all fields are valid.
 *
 * Returns: TRUE if all fields are valid, or FALSE.
 */
gboolean purple_request_fields_all_valid(const PurpleRequestFields *fields);

/**
 * purple_request_fields_get_field:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Return the field with the specified ID.
 *
 * Returns: The field, if found.
 */
PurpleRequestField *purple_request_fields_get_field(
		const PurpleRequestFields *fields, const char *id);

/**
 * purple_request_fields_get_string:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns the string value of a field with the specified ID.
 *
 * Returns: The string value, if found, or %NULL otherwise.
 */
const char *purple_request_fields_get_string(const PurpleRequestFields *fields,
										   const char *id);

/**
 * purple_request_fields_get_integer:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns the integer value of a field with the specified ID.
 *
 * Returns: The integer value, if found, or 0 otherwise.
 */
int purple_request_fields_get_integer(const PurpleRequestFields *fields,
									const char *id);

/**
 * purple_request_fields_get_bool:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns the boolean value of a field with the specified ID.
 *
 * Returns: The boolean value, if found, or %FALSE otherwise.
 */
gboolean purple_request_fields_get_bool(const PurpleRequestFields *fields,
									  const char *id);

/**
 * purple_request_fields_get_choice:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns the choice index of a field with the specified ID.
 *
 * Returns: The choice value, if found, or NULL otherwise.
 */
gpointer
purple_request_fields_get_choice(const PurpleRequestFields *fields,
	const char *id);

/**
 * purple_request_fields_get_account:
 * @fields: The fields list.
 * @id:     The ID of the field.
 *
 * Returns the account of a field with the specified ID.
 *
 * Returns: The account value, if found, or NULL otherwise.
 */
PurpleAccount *purple_request_fields_get_account(const PurpleRequestFields *fields,
											 const char *id);

/**
 * purple_request_fields_get_ui_data:
 * @fields: The fields list.
 *
 * Returns the UI data associated with this object.
 *
 * Returns: The UI data associated with this object.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_request_fields_get_ui_data(const PurpleRequestFields *fields);

/**
 * purple_request_fields_set_ui_data:
 * @fields: The fields list.
 * @ui_data: A pointer to associate with this object.
 *
 * Set the UI data associated with this object.
 */
void purple_request_fields_set_ui_data(PurpleRequestFields *fields, gpointer ui_data);

/**************************************************************************/
/* Fields Group API                                                       */
/**************************************************************************/

/**
 * purple_request_field_group_new:
 * @title: The optional title to give the group.
 *
 * Creates a fields group with an optional title.
 *
 * Returns: A new fields group
 */
PurpleRequestFieldGroup *purple_request_field_group_new(const char *title);

/**
 * purple_request_field_group_set_tab:
 * @group:  The group.
 * @tab_no: The tab number.
 *
 * Sets tab number for a group.
 *
 * See purple_request_fields_set_tab_names().
 */
void purple_request_field_group_set_tab(PurpleRequestFieldGroup *group,
	guint tab_no);

/**
 * purple_request_field_group_get_tab:
 * @group: The group.
 *
 * Returns tab number of a group.
 *
 * See purple_request_fields_get_tab_names().
 *
 * Returns: Tab number.
 */
guint purple_request_field_group_get_tab(const PurpleRequestFieldGroup *group);

/**
 * purple_request_field_group_destroy:
 * @group: The group to destroy.
 *
 * Destroys a fields group.
 */
void purple_request_field_group_destroy(PurpleRequestFieldGroup *group);

/**
 * purple_request_field_group_add_field:
 * @group: The group to add the field to.
 * @field: The field to add to the group.
 *
 * Adds a field to the group.
 */
void purple_request_field_group_add_field(PurpleRequestFieldGroup *group,
										PurpleRequestField *field);

/**
 * purple_request_field_group_get_title:
 * @group: The group.
 *
 * Returns the title of a fields group.
 *
 * Returns: The title, if set.
 */
const char *purple_request_field_group_get_title(
		const PurpleRequestFieldGroup *group);

/**
 * purple_request_field_group_get_fields:
 * @group: The group.
 *
 * Returns a list of all fields in a group.
 *
 * Returns: (transfer none): The list of fields in the group.
 */
GList *purple_request_field_group_get_fields(
		const PurpleRequestFieldGroup *group);

/**
 * purple_request_field_group_get_fields_list:
 * @group: The group.
 *
 * Returns a list of all fields in a group.
 *
 * Returns: (transfer none): The list of fields in the group.
 */
PurpleRequestFields *purple_request_field_group_get_fields_list(
		const PurpleRequestFieldGroup *group);

/**************************************************************************/
/* Field API                                                              */
/**************************************************************************/

/**
 * purple_request_field_new:
 * @id:   The field ID.
 * @text: The text label of the field.
 * @type: The type of field.
 *
 * Creates a field of the specified type.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_new(const char *id, const char *text,
										 PurpleRequestFieldType type);

/**
 * purple_request_field_destroy:
 * @field: The field to destroy.
 *
 * Destroys a field.
 */
void purple_request_field_destroy(PurpleRequestField *field);

/**
 * purple_request_field_set_label:
 * @field: The field.
 * @label: The text label.
 *
 * Sets the label text of a field.
 */
void purple_request_field_set_label(PurpleRequestField *field, const char *label);

/**
 * purple_request_field_set_visible:
 * @field:   The field.
 * @visible: TRUE if visible, or FALSE if not.
 *
 * Sets whether or not a field is visible.
 */
void purple_request_field_set_visible(PurpleRequestField *field, gboolean visible);

/**
 * purple_request_field_set_type_hint:
 * @field:     The field.
 * @type_hint: The type hint.
 *
 * Sets the type hint for the field.
 *
 * This is optionally used by the UIs to provide such features as
 * auto-completion for type hints like "account" and "screenname".
 */
void purple_request_field_set_type_hint(PurpleRequestField *field,
									  const char *type_hint);

/**
 * purple_request_field_set_tooltip:
 * @field:     The field.
 * @tooltip:   The tooltip text.
 *
 * Sets the tooltip for the field.
 *
 * This is optionally used by the UIs to provide a tooltip for
 * the field.
 */
void purple_request_field_set_tooltip(PurpleRequestField *field,
									const char *tooltip);

/**
 * purple_request_field_set_required:
 * @field:    The field.
 * @required: TRUE if required, or FALSE.
 *
 * Sets whether or not a field is required.
 */
void purple_request_field_set_required(PurpleRequestField *field,
									 gboolean required);

/**
 * purple_request_field_get_field_type:
 * @field: The field.
 *
 * Returns the type of a field.
 *
 * Returns: The field's type.
 */
PurpleRequestFieldType purple_request_field_get_field_type(const PurpleRequestField *field);

/**
 * purple_request_field_get_group:
 * @field: The field.
 *
 * Returns the group for the field.
 *
 * Returns: The UI data.
 */
PurpleRequestFieldGroup *purple_request_field_get_group(const PurpleRequestField *field);

/**
 * purple_request_field_get_id:
 * @field: The field.
 *
 * Returns the ID of a field.
 *
 * Returns: The ID
 */
const char *purple_request_field_get_id(const PurpleRequestField *field);

/**
 * purple_request_field_get_label:
 * @field: The field.
 *
 * Returns the label text of a field.
 *
 * Returns: The label text.
 */
const char *purple_request_field_get_label(const PurpleRequestField *field);

/**
 * purple_request_field_is_visible:
 * @field: The field.
 *
 * Returns whether or not a field is visible.
 *
 * Returns: TRUE if the field is visible. FALSE otherwise.
 */
gboolean purple_request_field_is_visible(const PurpleRequestField *field);

/**
 * purple_request_field_get_type_hint:
 * @field: The field.
 *
 * Returns the field's type hint.
 *
 * Returns: The field's type hint.
 */
const char *purple_request_field_get_field_type_hint(const PurpleRequestField *field);

/**
 * purple_request_field_get_tooltip:
 * @field: The field.
 *
 * Returns the field's tooltip.
 *
 * Returns: The field's tooltip.
 */
const char *purple_request_field_get_tooltip(const PurpleRequestField *field);

/**
 * purple_request_field_is_required:
 * @field: The field.
 *
 * Returns whether or not a field is required.
 *
 * Returns: TRUE if the field is required, or FALSE.
 */
gboolean purple_request_field_is_required(const PurpleRequestField *field);

/**
 * purple_request_field_is_filled:
 * @field: The field.
 *
 * Checks, if specified field has value.
 *
 * Returns: TRUE if the field has value, or FALSE.
 */
gboolean purple_request_field_is_filled(const PurpleRequestField *field);

/**
 * purple_request_field_set_validator:
 * @field: The field.
 * @validator: The validator callback, NULL to disable validation.
 * @user_data: The data to pass to the callback.
 *
 * Sets validator for a single field.
 */
void purple_request_field_set_validator(PurpleRequestField *field,
	PurpleRequestFieldValidator validator, void *user_data);

/**
 * purple_request_field_is_validatable:
 * @field: The field.
 *
 * Returns whether or not field has validator set.
 *
 * Returns: TRUE if the field has validator, or FALSE.
 */
gboolean purple_request_field_is_validatable(PurpleRequestField *field);

/**
 * purple_request_field_is_valid:
 * @field: The field.
 * @errmsg: If non-NULL, the memory area, where the pointer to validation
 *        failure message will be set.
 *
 * Checks, if specified field is valid.
 *
 * If detailed message about failure reason is needed, there is an option to
 * return (via errmsg argument) pointer to newly allocated error message.
 * It must be freed with g_free after use.
 *
 * Note: empty, not required fields are valid.
 *
 * Returns: TRUE, if the field is valid, FALSE otherwise.
 */
gboolean purple_request_field_is_valid(PurpleRequestField *field, gchar **errmsg);

/**
 * purple_request_field_set_sensitive:
 * @field:     The field.
 * @sensitive: TRUE if the field should be sensitive for user input.
 *
 * Sets field editable.
 */
void purple_request_field_set_sensitive(PurpleRequestField *field,
	gboolean sensitive);

/**
 * purple_request_field_is_sensitive:
 * @field: The field.
 *
 * Checks, if field is editable.
 *
 * Returns: TRUE, if the field is sensitive for user input.
 */
gboolean purple_request_field_is_sensitive(PurpleRequestField *field);

/**
 * purple_request_field_set_sensitivity_cb:
 * @field: The field.
 * @cb:    The callback.
 *
 * Sets the callback, used to determine if the field should be editable.
 */
void purple_request_field_set_sensitivity_cb(PurpleRequestField *field,
	PurpleRequestFieldSensitivityCb cb);

/**
 * purple_request_field_get_ui_data:
 * @field: The field.
 *
 * Returns the ui_data for a field.
 *
 * Returns: The UI data.
 */
gpointer purple_request_field_get_ui_data(const PurpleRequestField *field);

/**
 * purple_request_field_set_ui_data:
 * @field: The field.
 * @ui_data: The UI data.
 *
 * Sets the ui_data for a field.
 *
 * Returns: The UI data.
 */
void purple_request_field_set_ui_data(PurpleRequestField *field,
                                      gpointer ui_data);

/**************************************************************************/
/* String Field API                                                       */
/**************************************************************************/

/**
 * purple_request_field_string_new:
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The optional default value.
 * @multiline:     Whether or not this should be a multiline string.
 *
 * Creates a string request field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_string_new(const char *id,
												const char *text,
												const char *default_value,
												gboolean multiline);

/**
 * purple_request_field_string_set_default_value:
 * @field:         The field.
 * @default_value: The default value.
 *
 * Sets the default value in a string field.
 */
void purple_request_field_string_set_default_value(PurpleRequestField *field,
												 const char *default_value);

/**
 * purple_request_field_string_set_value:
 * @field: The field.
 * @value: The value.
 *
 * Sets the value in a string field.
 */
void purple_request_field_string_set_value(PurpleRequestField *field,
										 const char *value);

/**
 * purple_request_field_string_set_masked:
 * @field:  The field.
 * @masked: The masked value.
 *
 * Sets whether or not a string field is masked
 * (commonly used for password fields).
 */
void purple_request_field_string_set_masked(PurpleRequestField *field,
										  gboolean masked);

/**
 * purple_request_field_string_get_default_value:
 * @field: The field.
 *
 * Returns the default value in a string field.
 *
 * Returns: The default value.
 */
const char *purple_request_field_string_get_default_value(
		const PurpleRequestField *field);

/**
 * purple_request_field_string_get_value:
 * @field: The field.
 *
 * Returns the user-entered value in a string field.
 *
 * Returns: The value.
 */
const char *purple_request_field_string_get_value(const PurpleRequestField *field);

/**
 * purple_request_field_string_is_multiline:
 * @field: The field.
 *
 * Returns whether or not a string field is multi-line.
 *
 * Returns: %TRUE if the field is mulit-line, or %FALSE otherwise.
 */
gboolean purple_request_field_string_is_multiline(const PurpleRequestField *field);

/**
 * purple_request_field_string_is_masked:
 * @field: The field.
 *
 * Returns whether or not a string field is masked.
 *
 * Returns: %TRUE if the field is masked, or %FALSE otherwise.
 */
gboolean purple_request_field_string_is_masked(const PurpleRequestField *field);

/**************************************************************************/
/* Integer Field API                                                      */
/**************************************************************************/

/**
 * purple_request_field_int_new:
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The default value.
 * @lower_bound:   The lower bound.
 * @upper_bound:   The upper bound.
 *
 * Creates an integer field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_int_new(const char *id,
	const char *text, int default_value, int lower_bound, int upper_bound);

/**
 * purple_request_field_int_set_default_value:
 * @field:         The field.
 * @default_value: The default value.
 *
 * Sets the default value in an integer field.
 */
void purple_request_field_int_set_default_value(PurpleRequestField *field,
											  int default_value);

/**
 * purple_request_field_int_set_lower_bound:
 * @field:       The field.
 * @lower_bound: The lower bound.
 *
 * Sets the lower bound in an integer field.
 */
void purple_request_field_int_set_lower_bound(PurpleRequestField *field, int lower_bound);

/**
 * purple_request_field_int_set_upper_bound:
 * @field:       The field.
 * @upper_bound: The upper bound.
 *
 * Sets the upper bound in an integer field.
 */
void purple_request_field_int_set_upper_bound(PurpleRequestField *field, int upper_bound);

/**
 * purple_request_field_int_set_value:
 * @field: The field.
 * @value: The value.
 *
 * Sets the value in an integer field.
 */
void purple_request_field_int_set_value(PurpleRequestField *field, int value);

/**
 * purple_request_field_int_get_default_value:
 * @field: The field.
 *
 * Returns the default value in an integer field.
 *
 * Returns: The default value.
 */
int purple_request_field_int_get_default_value(const PurpleRequestField *field);

/**
 * purple_request_field_int_get_lower_bound:
 * @field: The field.
 *
 * Returns the lower bound in an integer field.
 *
 * Returns: The lower bound.
 */
int purple_request_field_int_get_lower_bound(const PurpleRequestField *field);

/**
 * purple_request_field_int_get_upper_bound:
 * @field: The field.
 *
 * Returns the upper bound in an integer field.
 *
 * Returns: The upper bound.
 */
int purple_request_field_int_get_upper_bound(const PurpleRequestField *field);

/**
 * purple_request_field_int_get_value:
 * @field: The field.
 *
 * Returns the user-entered value in an integer field.
 *
 * Returns: The value.
 */
int purple_request_field_int_get_value(const PurpleRequestField *field);

/**************************************************************************/
/* Boolean Field API                                                      */
/**************************************************************************/

/**
 * purple_request_field_bool_new:
 * @id:            The field ID.
 * @text:          The text label of the field.
 * @default_value: The default value.
 *
 * Creates a boolean field.
 *
 * This is often represented as a checkbox.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_bool_new(const char *id,
											  const char *text,
											  gboolean default_value);

/**
 * purple_request_field_bool_set_default_value:
 * @field:         The field.
 * @default_value: The default value.
 *
 * Sets the default value in an boolean field.
 */
void purple_request_field_bool_set_default_value(PurpleRequestField *field,
											   gboolean default_value);

/**
 * purple_request_field_bool_set_value:
 * @field: The field.
 * @value: The value.
 *
 * Sets the value in an boolean field.
 */
void purple_request_field_bool_set_value(PurpleRequestField *field,
									   gboolean value);

/**
 * purple_request_field_bool_get_default_value:
 * @field: The field.
 *
 * Returns the default value in an boolean field.
 *
 * Returns: The default value.
 */
gboolean purple_request_field_bool_get_default_value(
		const PurpleRequestField *field);

/**
 * purple_request_field_bool_get_value:
 * @field: The field.
 *
 * Returns the user-entered value in an boolean field.
 *
 * Returns: The value.
 */
gboolean purple_request_field_bool_get_value(const PurpleRequestField *field);

/**************************************************************************/
/* Choice Field API                                                       */
/**************************************************************************/

/**
 * purple_request_field_choice_new:
 * @id:            The field ID.
 * @text:          The optional label of the field.
 * @default_value: The default choice.
 *
 * Creates a multiple choice field.
 *
 * This is often represented as a group of radio buttons.
 *
 * Returns: The new field.
 */
PurpleRequestField *
purple_request_field_choice_new(const char *id, const char *text,
	gpointer default_value);

/**
 * purple_request_field_choice_add:
 * @field: The choice field.
 * @label: The choice label.
 * @data:  The choice value.
 *
 * Adds a choice to a multiple choice field.
 */
void
purple_request_field_choice_add(PurpleRequestField *field, const char *label,
	gpointer data);

/**
 * purple_request_field_choice_set_default_value:
 * @field:         The field.
 * @default_value: The default value.
 *
 * Sets the default value in an choice field.
 */
void
purple_request_field_choice_set_default_value(PurpleRequestField *field,
	gpointer default_value);

/**
 * purple_request_field_choice_set_value:
 * @field: The field.
 * @value: The value.
 *
 * Sets the value in an choice field.
 */
void
purple_request_field_choice_set_value(PurpleRequestField *field,
	gpointer value);

/**
 * purple_request_field_choice_get_default_value:
 * @field: The field.
 *
 * Returns the default value in an choice field.
 *
 * Returns: The default value.
 */
gpointer
purple_request_field_choice_get_default_value(const PurpleRequestField *field);

/**
 * purple_request_field_choice_get_value:
 * @field: The field.
 *
 * Returns the user-entered value in an choice field.
 *
 * Returns: The value.
 */
gpointer
purple_request_field_choice_get_value(const PurpleRequestField *field);

/**
 * purple_request_field_choice_get_elements:
 * @field: The field.
 *
 * Returns a list of elements in a choice field.
 *
 * Returns: (transfer none): The list of pairs of {label, value}.
 */
GList *
purple_request_field_choice_get_elements(const PurpleRequestField *field);

/**
 * purple_request_field_choice_set_data_destructor:
 * @field:   The field.
 * @destroy: The destroy function.
 *
 * Sets the destructor for field values.
 */
void
purple_request_field_choice_set_data_destructor(PurpleRequestField *field,
	GDestroyNotify destroy);

/**************************************************************************/
/* List Field API                                                         */
/**************************************************************************/

/**
 * purple_request_field_list_new:
 * @id:   The field ID.
 * @text: The optional label of the field.
 *
 * Creates a multiple list item field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_list_new(const char *id, const char *text);

/**
 * purple_request_field_list_set_multi_select:
 * @field:        The list field.
 * @multi_select: TRUE if multiple selection is enabled,
 *                     or FALSE otherwise.
 *
 * Sets whether or not a list field allows multiple selection.
 */
void purple_request_field_list_set_multi_select(PurpleRequestField *field,
											  gboolean multi_select);

/**
 * purple_request_field_list_get_multi_select:
 * @field: The list field.
 *
 * Returns whether or not a list field allows multiple selection.
 *
 * Returns: TRUE if multiple selection is enabled, or FALSE otherwise.
 */
gboolean purple_request_field_list_get_multi_select(
	const PurpleRequestField *field);

/**
 * purple_request_field_list_get_data:
 * @field: The list field.
 * @text:  The item text.
 *
 * Returns the data for a particular item.
 *
 * Returns: The data associated with the item.
 */
void *purple_request_field_list_get_data(const PurpleRequestField *field,
									   const char *text);

/**
 * purple_request_field_list_add_icon:
 * @field: The list field.
 * @item:  The list item.
 * @icon_path: The path to icon file, or %NULL for no icon.
 * @data:  The associated data.
 *
 * Adds an item to a list field.
 */
void purple_request_field_list_add_icon(PurpleRequestField *field,
								 const char *item, const char* icon_path, void* data);

/**
 * purple_request_field_list_add_selected:
 * @field: The field.
 * @item:  The item to add.
 *
 * Adds a selected item to the list field.
 */
void purple_request_field_list_add_selected(PurpleRequestField *field,
										  const char *item);

/**
 * purple_request_field_list_clear_selected:
 * @field: The field.
 *
 * Clears the list of selected items in a list field.
 */
void purple_request_field_list_clear_selected(PurpleRequestField *field);

/**
 * purple_request_field_list_set_selected:
 * @field: The field.
 * @items: The list of selected items, which is not modified or freed.
 *
 * Sets a list of selected items in a list field.
 */
void purple_request_field_list_set_selected(PurpleRequestField *field,
										  GList *items);

/**
 * purple_request_field_list_is_selected:
 * @field: The field.
 * @item:  The item.
 *
 * Returns whether or not a particular item is selected in a list field.
 *
 * Returns: TRUE if the item is selected. FALSE otherwise.
 */
gboolean purple_request_field_list_is_selected(const PurpleRequestField *field,
											 const char *item);

/**
 * purple_request_field_list_get_selected:
 * @field: The field.
 *
 * Returns a list of selected items in a list field.
 *
 * To retrieve the data for each item, use
 * purple_request_field_list_get_data().
 *
 * Returns: (transfer none): The list of selected items.
 */
GList *purple_request_field_list_get_selected(
	const PurpleRequestField *field);

/**
 * purple_request_field_list_get_items:
 * @field: The field.
 *
 * Returns a list of items in a list field.
 *
 * Returns: (transfer none): The list of items.
 */
GList *purple_request_field_list_get_items(const PurpleRequestField *field);

/**
 * purple_request_field_list_get_icons:
 * @field: The field.
 *
 * Returns a list of icons in a list field.
 *
 * The icons will correspond with the items, in order.
 *
 * Returns: (transfer none): The list of icons or %NULL (i.e. the empty #GList)
 *          if no items have icons.
 */
GList *purple_request_field_list_get_icons(const PurpleRequestField *field);

/**************************************************************************/
/* Label Field API                                                        */
/**************************************************************************/

/**
 * purple_request_field_label_new:
 * @id:   The field ID.
 * @text: The label of the field.
 *
 * Creates a label field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_label_new(const char *id,
											   const char *text);

/**************************************************************************/
/* Image Field API                                                        */
/**************************************************************************/

/**
 * purple_request_field_image_new:
 * @id:   The field ID.
 * @text: The label of the field.
 * @buf:  The image data.
 * @size: The size of the data in @buf.
 *
 * Creates an image field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_image_new(const char *id, const char *text,
											   const char *buf, gsize size);

/**
 * purple_request_field_image_set_scale:
 * @field: The image field.
 * @x:     The x scale factor.
 * @y:     The y scale factor.
 *
 * Sets the scale factors of an image field.
 */
void purple_request_field_image_set_scale(PurpleRequestField *field, unsigned int x, unsigned int y);

/**
 * purple_request_field_image_get_buffer:
 * @field: The image field.
 *
 * Returns pointer to the image.
 *
 * Returns: Pointer to the image.
 */
const char *purple_request_field_image_get_buffer(PurpleRequestField *field);

/**
 * purple_request_field_image_get_size:
 * @field: The image field.
 *
 * Returns size (in bytes) of the image.
 *
 * Returns: Size of the image.
 */
gsize purple_request_field_image_get_size(PurpleRequestField *field);

/**
 * purple_request_field_image_get_scale_x:
 * @field: The image field.
 *
 * Returns X scale coefficient of the image.
 *
 * Returns: X scale coefficient of the image.
 */
unsigned int purple_request_field_image_get_scale_x(PurpleRequestField *field);

/**
 * purple_request_field_image_get_scale_y:
 * @field: The image field.
 *
 * Returns Y scale coefficient of the image.
 *
 * Returns: Y scale coefficient of the image.
 */
unsigned int purple_request_field_image_get_scale_y(PurpleRequestField *field);

/**************************************************************************/
/* Account Field API                                                      */
/**************************************************************************/

/**
 * purple_request_field_account_new:
 * @id:      The field ID.
 * @text:    The text label of the field.
 * @account: The optional default account.
 *
 * Creates an account field.
 *
 * By default, this field will not show offline accounts.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_account_new(const char *id,
												 const char *text,
												 PurpleAccount *account);

/**
 * purple_request_field_account_set_default_value:
 * @field:         The account field.
 * @default_value: The default account.
 *
 * Sets the default account on an account field.
 */
void purple_request_field_account_set_default_value(PurpleRequestField *field,
												  PurpleAccount *default_value);

/**
 * purple_request_field_account_set_value:
 * @field: The account field.
 * @value: The account.
 *
 * Sets the account in an account field.
 */
void purple_request_field_account_set_value(PurpleRequestField *field,
										  PurpleAccount *value);

/**
 * purple_request_field_account_set_show_all:
 * @field:    The account field.
 * @show_all: Whether or not to show all accounts.
 *
 * Sets whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 */
void purple_request_field_account_set_show_all(PurpleRequestField *field,
											 gboolean show_all);

/**
 * purple_request_field_account_set_filter:
 * @field:       The account field.
 * @filter_func: The account filter function.
 *
 * Sets the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 */
void purple_request_field_account_set_filter(PurpleRequestField *field,
										   PurpleFilterAccountFunc filter_func);

/**
 * purple_request_field_account_get_default_value:
 * @field: The field.
 *
 * Returns the default account in an account field.
 *
 * Returns: The default account.
 */
PurpleAccount *purple_request_field_account_get_default_value(
		const PurpleRequestField *field);

/**
 * purple_request_field_account_get_value:
 * @field: The field.
 *
 * Returns the user-entered account in an account field.
 *
 * Returns: The user-entered account.
 */
PurpleAccount *purple_request_field_account_get_value(
		const PurpleRequestField *field);

/**
 * purple_request_field_account_get_show_all:
 * @field:    The account field.
 *
 * Returns whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 *
 * Returns: Whether or not to show all accounts.
 */
gboolean purple_request_field_account_get_show_all(
		const PurpleRequestField *field);

/**
 * purple_request_field_account_get_filter:
 * @field:       The account field.
 *
 * Returns the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 *
 * Returns: The account filter function.
 */
PurpleFilterAccountFunc purple_request_field_account_get_filter(
		const PurpleRequestField *field);

/**************************************************************************/
/* Certificate Field API                                                  */
/**************************************************************************/

/**
 * purple_request_field_certificate_new:
 * @id:   The field ID.
 * @text: The label of the field.
 * @cert: The certificate of the field.
 *
 * Creates a certificate field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_certificate_new(const char *id,
														 const char *text,
														 PurpleCertificate *cert);

/**
 * purple_request_field_certificate_get_value:
 * @field: The field.
 *
 * Returns the certificate in a certificate field.
 *
 * Returns: The certificate.
 */
PurpleCertificate *purple_request_field_certificate_get_value(
		const PurpleRequestField *field);

/**************************************************************************/
/* Datasheet Field API                                                    */
/**************************************************************************/

/**
 * purple_request_field_datasheet_new:
 * @id:    The field ID.
 * @text:  The label of the field, may be %NULL.
 * @sheet: The datasheet.
 *
 * Creates a datasheet item field.
 *
 * Returns: The new field.
 */
PurpleRequestField *purple_request_field_datasheet_new(const char *id,
	const gchar *text, PurpleRequestDatasheet *sheet);

/**
 * purple_request_field_datasheet_get_sheet:
 * @field: The field.
 *
 * Returns a datasheet for a field.
 *
 * Returns: (transfer none): The datasheet object.
 */
PurpleRequestDatasheet *purple_request_field_datasheet_get_sheet(
	PurpleRequestField *field);

/**************************************************************************/
/* Validators for request fields.                                         */
/**************************************************************************/

/**
 * purple_request_field_email_validator:
 * @field: The field.
 * @errmsg: (Optional) destination for error message.
 * @user_data: Ignored.
 *
 * Validates a field which should contain an email address.
 *
 * See purple_request_field_set_validator().
 *
 * Returns: TRUE, if field contains valid email address.
 */
gboolean purple_request_field_email_validator(PurpleRequestField *field,
	gchar **errmsg, void *user_data);

/**
 * purple_request_field_alphanumeric_validator:
 * @field: The field.
 * @errmsg: (allow-none): destination for error message.
 * @allowed_characters: (allow-none): allowed character list
 *                      (NULL-terminated string).
 *
 * Validates a field which should contain alphanumeric content.
 *
 * See purple_request_field_set_validator().
 *
 * Returns: TRUE, if field contains only alphanumeric characters.
 */
gboolean purple_request_field_alphanumeric_validator(PurpleRequestField *field,
	gchar **errmsg, void *allowed_characters);

/**************************************************************************/
/* Request API                                                            */
/**************************************************************************/

/**
 * purple_request_input:
 * @handle:        The plugin or connection handle.  For some
 *                 things this is <emphasis>extremely</emphasis> important.  The
 *                 handle is used to programmatically close the request
 *                 dialog when it is no longer needed.  For protocols this
 *                 is often a pointer to the #PurpleConnection
 *                 instance.  For plugins this should be a similar,
 *                 unique memory location.  This value is important
 *                 because it allows a request to be closed with
 *                 purple_request_close_with_handle() when, for
 *                 example, you sign offline.  If the request is
 *                 <emphasis>not</emphasis> closed it is
 *                 <emphasis>very</emphasis> likely to cause a crash whenever
 *                 the callback handler functions are triggered.
 * @title:         The title of the message, or %NULL if it should have
 *                 no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                 feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @default_value: The default value.
 * @multiline:     %TRUE if the inputted text can span multiple lines.
 * @masked:        %TRUE if the inputted text should be masked in some
 *                 way (such as by displaying characters as stars).  This
 *                 might be because the input is some kind of password.
 * @hint:          Optionally suggest how the input box should appear.
 *                 Use "html", for example, to allow the user to enter HTML.
 * @ok_text:       The text for the <literal>OK</literal> button, which may not
 *                 be %NULL.
 * @ok_cb:         The callback for the <literal>OK</literal> button, which may
 *                 not be %NULL.
 * @cancel_text:   The text for the <literal>Cancel</literal> button, which may
 *                 not be %NULL.
 * @cancel_cb:     The callback for the <literal>Cancel</literal> button, which
 *                 may be %NULL.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                 unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 *
 * Prompts the user for text input.
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
 * purple_request_choice:
 * @handle:        The plugin or connection handle.  For some things this
 *                 is <emphasis>extremely</emphasis> important.  See the comments on
 *                 purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                 no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                 feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @default_value: The default choice; this should be one of the values
 *                 listed in the varargs.
 * @ok_text:       The text for the <literal>OK</literal> button, which may not
 *                 be %NULL.
 * @ok_cb:         The callback for the <literal>OK</literal> button, which may
 *                 not be %NULL.
 * @cancel_text:   The text for the <literal>Cancel</literal> button, which may
 *                 not be %NULL.
 * @cancel_cb:     The callback for the <literal>Cancel</literal> button, or
 *                 %NULL to do nothing.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                 unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 * @...:           The choices, which should be pairs of <type>char *</type>
 *                 descriptions and <type>int</type> values, terminated with a
 *                 %NULL parameter.
 *
 * Prompts the user for multiple-choice input.
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
 * purple_request_choice_varg:
 *
 * <literal>va_list</literal> version of purple_request_choice(); see its
 * documentation.
 */
void *purple_request_choice_varg(void *handle, const char *title,
	const char *primary, const char *secondary, gpointer default_value,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar,
	void *user_data, va_list choices);

/**
 * purple_request_action:
 * @handle:         The plugin or connection handle.  For some things this
 *                       is <emphasis>extremely</emphasis> important.  See the comments on
 *                       purple_request_input().
 * @title:          The title of the message, or %NULL if it should have
 *                       no title.
 * @primary:        The main point of the message, or %NULL if you're
 *                       feeling enigmatic.
 * @secondary:      Secondary information, or %NULL if there is none.
 * @default_action: The default action, zero-indexed; if the third action
 *                       supplied should be the default, supply
 *                       <literal>2</literal>.  This should be the action that
 *                       users are most likely to select.
 * @cpar:           The #PurpleRequestCommonParameters object, which gets
 *                       unref'ed after this call.
 * @user_data:      The data to pass to the callback.
 * @action_count:   The number of actions.
 * @...:            A list of actions.  These are pairs of
 *                       arguments.  The first of each pair is the
 *                       <type>char *</type> label that appears on the button.
 *                       It should have an underscore before the letter you want
 *                       to use as the accelerator key for the button.  The
 *                       second of each pair is the #PurpleRequestActionCb
 *                       function to use when the button is clicked.
 *
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_action(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, ...);

/**
 * purple_request_action_varg:
 *
 * <literal>va_list</literal> version of purple_request_action(); see its
 * documentation.
 */
void *
purple_request_action_varg(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, va_list actions);

/**
 * purple_request_wait:
 * @handle:        The plugin or connection handle.  For some things this
 *                 is <emphasis>extremely</emphasis> important.  See the comments on
 *                 purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                 default title.
 * @primary:       The main point of the message, or %NULL if you're
 *                 feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @with_progress: %TRUE, if we want to display progress bar, %FALSE
 *                 otherwise
 * @cancel_cb:     The callback for the <literal>Cancel</literal> button, which
 *                 may be %NULL.
 * @cpar:          The #PurpleRequestCommonParameters object, which gets
 *                 unref'ed after this call.
 * @user_data:     The data to pass to the callback.
 *
 * Displays a "please wait" dialog.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_wait(void *handle, const char *title, const char *primary,
	const char *secondary, gboolean with_progress,
	PurpleRequestCancelCb cancel_cb, PurpleRequestCommonParameters *cpar,
	void *user_data);

/**
 * purple_request_wait_pulse:
 * @ui_handle: The request UI handle.
 *
 * Notifies the "please wait" dialog that some progress has been made, but you
 * don't know how much.
 */
void
purple_request_wait_pulse(void *ui_handle);

/**
 * purple_request_wait_progress:
 * @ui_handle: The request UI handle.
 * @fraction:  The part of task that is done (between 0.0 and 1.0,
 *                  inclusive).
 *
 * Notifies the "please wait" dialog about progress has been made.
 */
void
purple_request_wait_progress(void *ui_handle, gfloat fraction);

/**
 * purple_request_fields:
 * @handle:      The plugin or connection handle.  For some things this
 *               is <emphasis>extremely</emphasis> important.  See the comments on
 *               purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *               no title.
 * @primary:     The main point of the message, or %NULL if you're
 *               feeling enigmatic.
 * @secondary:   Secondary information, or %NULL if there is none.
 * @fields:      The list of fields.
 * @ok_text:     The text for the <literal>OK</literal> button, which may not be
 *               %NULL.
 * @ok_cb:       The callback for the <literal>OK</literal> button, which may
 *               not be
 *               %NULL.
 * @cancel_text: The text for the <literal>Cancel</literal> button, which may
 *               not be %NULL.
 * @cancel_cb:   The callback for the <literal>Cancel</literal> button, which
 *               may be %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *               unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Displays groups of fields for the user to fill in.
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
 * purple_request_is_valid_ui_handle:
 * @ui_handle: The UI handle.
 * @type:      The pointer to variable, where request type may be stored
 *                  (may be %NULL).
 *
 * Checks, if passed UI handle is valid.
 *
 * Returns: TRUE, if handle is valid, FALSE otherwise.
 */
gboolean
purple_request_is_valid_ui_handle(void *ui_handle, PurpleRequestType *type);

/**
 * purple_request_add_close_notify:
 * @ui_handle:   The UI handle.
 * @notify:      The function to be called.
 * @notify_data: The data to be passed to the callback function.
 *
 * Adds a function called when notification dialog is closed.
 */
void
purple_request_add_close_notify(void *ui_handle, GDestroyNotify notify,
	gpointer notify_data);

/**
 * purple_request_close:
 * @type:     The request type.
 * @uihandle: The request UI handle.
 *
 * Closes a request.
 */
void purple_request_close(PurpleRequestType type, void *uihandle);

/**
 * purple_request_close_with_handle:
 * @handle: The handle, as supplied as the @handle parameter to one of the
 *               <literal>purple_request_*</literal> functions.
 *
 * Closes all requests registered with the specified handle.
 *
 * See purple_request_input().
 */
void purple_request_close_with_handle(void *handle);

/**
 * purple_request_yes_no:
 *
 * A wrapper for purple_request_action() that uses <literal>Yes</literal> and
 * <literal>No</literal> buttons.
 */
#define purple_request_yes_no(handle, title, primary, secondary, \
	default_action, cpar, user_data, yes_cb, no_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_Yes"), (yes_cb), \
		_("_No"), (no_cb))

/**
 * purple_request_ok_cancel:
 *
 * A wrapper for purple_request_action() that uses <literal>OK</literal> and
 * <literal>Cancel</literal> buttons.
 */
#define purple_request_ok_cancel(handle, title, primary, secondary, \
	default_action, cpar, user_data, ok_cb, cancel_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_OK"), (ok_cb), \
		_("_Cancel"), (cancel_cb))

/**
 * purple_request_accept_cancel:
 *
 * A wrapper for purple_request_action() that uses Accept and Cancel buttons.
 */
#define purple_request_accept_cancel(handle, title, primary, secondary, \
	default_action, cpar, user_data, accept_cb, cancel_cb) \
	purple_request_action((handle), (title), (primary), (secondary), \
		(default_action), (cpar), (user_data), 2, _("_Accept"), \
		(accept_cb), _("_Cancel"), (cancel_cb))

/**
 * purple_request_file:
 * @handle:      The plugin or connection handle.  For some things this
 *                    is <emphasis>extremely</emphasis> important.  See the comments on
 *                    purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *                    no title.
 * @filename:    The default filename (may be %NULL)
 * @savedialog:  True if this dialog is being used to save a file.
 *                    False if it is being used to open a file.
 * @ok_cb:       The callback for the <literal>OK</literal> button.
 * @cancel_cb:   The callback for the <literal>Cancel</literal> button, which
 *                    may be %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *                    unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Displays a file selector request dialog.  Returns the selected filename to
 * the callback.  Can be used for either opening a file or saving a file.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_file(void *handle, const char *title, const char *filename,
	gboolean savedialog, GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data);

/**
 * purple_request_folder:
 * @handle:      The plugin or connection handle.  For some things this
 *                    is <emphasis>extremely</emphasis> important.  See the comments on
 *                    purple_request_input().
 * @title:       The title of the message, or %NULL if it should have
 *                    no title.
 * @dirname:     The default directory name (may be %NULL)
 * @ok_cb:       The callback for the <literal>OK</literal> button.
 * @cancel_cb:   The callback for the <literal>Cancel</literal> button, which
 *                    may be %NULL.
 * @cpar:        The #PurpleRequestCommonParameters object, which gets
 *                    unref'ed after this call.
 * @user_data:   The data to pass to the callback.
 *
 * Displays a folder select dialog. Returns the selected filename to
 * the callback.
 *
 * Returns: A UI-specific handle.
 */
void *
purple_request_folder(void *handle, const char *title, const char *dirname,
	GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data);

/**
 * purple_request_certificate:
 * @handle:        The plugin or connection handle.  For some things this
 *                 is <emphasis>extremely</emphasis> important.  See the comments on
 *                 purple_request_input().
 * @title:         The title of the message, or %NULL if it should have
 *                 no title.
 * @primary:       The main point of the message, or %NULL if you're
 *                 feeling enigmatic.
 * @secondary:     Secondary information, or %NULL if there is none.
 * @cert:          The #PurpleCertificate associated with this request.
 * @ok_text:       The text for the <literal>OK</literal> button, which may not
 *                 be %NULL.
 * @ok_cb:         The callback for the <literal>OK</literal> button, which may
 *                 not be %NULL.
 * @cancel_text:   The text for the <literal>Cancel</literal> button, which may
 *                 not be %NULL.
 * @cancel_cb:     The callback for the <literal>Cancel</literal> button, which
 *                 may be %NULL.
 * @user_data:     The data to pass to the callback.
 *
 * Prompts the user for action over a certificate.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * Returns: A UI-specific handle.
 */
void *purple_request_certificate(void *handle, const char *title,
	const char *primary, const char *secondary, PurpleCertificate *cert,
	const char *ok_text, GCallback ok_cb,
	const char *cancel_text, GCallback cancel_cb,
	void *user_data);

/**************************************************************************/
/* UI Registration Functions                                              */
/**************************************************************************/

/**
 * purple_request_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleRequestUiOps boxed structure.
 */
GType purple_request_ui_ops_get_type(void);

/**
 * purple_request_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used when displaying a
 * request.
 */
void purple_request_set_ui_ops(PurpleRequestUiOps *ops);

/**
 * purple_request_get_ui_ops:
 *
 * Returns the UI operations structure to be used when displaying a
 * request.
 *
 * Returns: The UI operations structure.
 */
PurpleRequestUiOps *purple_request_get_ui_ops(void);

G_END_DECLS

#endif /* _PURPLE_REQUEST_H_ */
