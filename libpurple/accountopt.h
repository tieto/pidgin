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

#ifndef _PURPLE_ACCOUNTOPT_H_
#define _PURPLE_ACCOUNTOPT_H_
/**
 * SECTION:accountopt
 * @section_id: libpurple-accountopt
 * @short_description: <filename>accountopt.h</filename>
 * @title: Account Options API
 */

#include "prefs.h"

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

/**
 * PurpleAccountOption:
 *
 * An option for an account.
 *
 * This is set by protocols, and appears in the account settings
 * dialogs.
 */
typedef struct _PurpleAccountOption		PurpleAccountOption;

/**
 * PurpleAccountUserSplit:
 *
 * A username split.
 *
 * This is used by some protocols to separate the fields of the username
 * into more human-readable components.
 */
typedef struct _PurpleAccountUserSplit	PurpleAccountUserSplit;

G_BEGIN_DECLS

/**************************************************************************/
/* Account Option API                                                     */
/**************************************************************************/

/**
 * purple_account_option_new:
 * @type:      The type of option.
 * @text:      The text of the option.
 * @pref_name: The account preference name for the option.
 *
 * Creates a new account option.  If you know what @a type will be in advance,
 * consider using purple_account_option_bool_new(),
 * purple_account_option_int_new(), purple_account_option_string_new() or
 * purple_account_option_list_new() (as appropriate) instead.
 *
 * Returns: The account option.
 */
PurpleAccountOption *purple_account_option_new(PurplePrefType type,
	const char *text, const char *pref_name);

/**
 * purple_account_option_bool_new:
 * @text:          The text of the option.
 * @pref_name:     The account preference name for the option.
 * @default_value: The default value.
 *
 * Creates a new boolean account option.
 *
 * Returns: The account option.
 */
PurpleAccountOption *purple_account_option_bool_new(const char *text,
	const char *pref_name, gboolean default_value);

/**
 * purple_account_option_int_new:
 * @text:          The text of the option.
 * @pref_name:     The account preference name for the option.
 * @default_value: The default value.
 *
 * Creates a new integer account option.
 *
 * Returns: The account option.
 */
PurpleAccountOption *purple_account_option_int_new(const char *text,
	const char *pref_name, int default_value);

/**
 * purple_account_option_string_new:
 * @text:          The text of the option.
 * @pref_name:     The account preference name for the option.
 * @default_value: The default value.
 *
 * Creates a new string account option.
 *
 * Returns: The account option.
 */
PurpleAccountOption *purple_account_option_string_new(const char *text,
	const char *pref_name, const char *default_value);

/**
 * purple_account_option_list_new:
 * @text:      The text of the option.
 * @pref_name: The account preference name for the option.
 * @list:      The key, value list.
 *
 * Creates a new list account option.
 *
 * The list passed will be owned by the account option, and the
 * strings inside will be freed automatically.
 *
 * The list is a list of #PurpleKeyValuePair items. The key is the label that
 * should be displayed to the user, and the <type>(const char *)</type> value is
 * the internal ID that should be passed to purple_account_set_string() to
 * choose that value.
 *
 * Returns: The account option.
 */
PurpleAccountOption *purple_account_option_list_new(const char *text,
	const char *pref_name, GList *list);

/**
 * purple_account_option_destroy:
 * @option: The option to destroy.
 *
 * Destroys an account option.
 */
void purple_account_option_destroy(PurpleAccountOption *option);

/**
 * purple_account_option_set_default_bool:
 * @option: The account option.
 * @value:  The default boolean value.
 *
 * Sets the default boolean value for an account option.
 */
void purple_account_option_set_default_bool(PurpleAccountOption *option,
										  gboolean value);

/**
 * purple_account_option_set_default_int:
 * @option: The account option.
 * @value:  The default integer value.
 *
 * Sets the default integer value for an account option.
 */
void purple_account_option_set_default_int(PurpleAccountOption *option,
										 int value);

/**
 * purple_account_option_set_default_string:
 * @option: The account option.
 * @value:  The default string value.
 *
 * Sets the default string value for an account option.
 */
void purple_account_option_set_default_string(PurpleAccountOption *option,
											const char *value);

/**
 * purple_account_option_string_set_masked:
 * @option: The account option.
 * @masked: The masking.
 *
 * Sets the masking for an account option. Setting this to %TRUE acts
 * as a hint to the UI that the option's value should be obscured from
 * view, like a password.
 */
void
purple_account_option_string_set_masked(PurpleAccountOption *option, gboolean masked);

/**
 * purple_account_option_string_set_hints:
 * @option: The account option.
 * @hints: The list of hints, stored as strings.
 *
 * Sets the hint list for an account option.
 *
 * The list passed will be owned by the account option, and the
 * strings inside will be freed automatically.
 */
void purple_account_option_string_set_hints(PurpleAccountOption *option,
	GSList *hints);

/**
 * purple_account_option_set_list:
 * @option: The account option.
 * @values: The default list value.
 *
 * Sets the list values for an account option.
 *
 * The list passed will be owned by the account option, and the
 * strings inside will be freed automatically.
 *
 * The list is in key, value pairs. The key is the ID stored and used
 * internally, and the value is the label displayed.
 */
void purple_account_option_set_list(PurpleAccountOption *option, GList *values);

/**
 * purple_account_option_add_list_item:
 * @option: The account option.
 * @key:    The key.
 * @value:  The value.
 *
 * Adds an item to a list account option.
 */
void purple_account_option_add_list_item(PurpleAccountOption *option,
									   const char *key, const char *value);

/**
 * purple_account_option_get_pref_type:
 * @option: The account option.
 *
 * Returns the specified account option's type.
 *
 * Returns: The account option's type.
 */
PurplePrefType purple_account_option_get_pref_type(const PurpleAccountOption *option);

/**
 * purple_account_option_get_text:
 * @option: The account option.
 *
 * Returns the text for an account option.
 *
 * Returns: The account option's text.
 */
const char *purple_account_option_get_text(const PurpleAccountOption *option);

/**
 * purple_account_option_get_setting:
 * @option: The account option.
 *
 * Returns the name of an account option.  This corresponds to the @c pref_name
 * parameter supplied to purple_account_option_new() or one of the
 * type-specific constructors.
 *
 * Returns: The option's name.
 */
const char *purple_account_option_get_setting(const PurpleAccountOption *option);

/**
 * purple_account_option_get_default_bool:
 * @option: The account option.
 *
 * Returns the default boolean value for an account option.
 *
 * Returns: The default boolean value.
 */
gboolean purple_account_option_get_default_bool(const PurpleAccountOption *option);

/**
 * purple_account_option_get_default_int:
 * @option: The account option.
 *
 * Returns the default integer value for an account option.
 *
 * Returns: The default integer value.
 */
int purple_account_option_get_default_int(const PurpleAccountOption *option);

/**
 * purple_account_option_get_default_string:
 * @option: The account option.
 *
 * Returns the default string value for an account option.
 *
 * Returns: The default string value.
 */
const char *purple_account_option_get_default_string(
	const PurpleAccountOption *option);

/**
 * purple_account_option_get_default_list_value:
 * @option: The account option.
 *
 * Returns the default string value for a list account option.
 *
 * Returns: The default list string value.
 */
const char *purple_account_option_get_default_list_value(
	const PurpleAccountOption *option);

/**
 * purple_account_option_string_get_masked:
 * @option: The account option.
 *
 * Returns whether an option's value should be masked from view, like a
 * password.  If so, the UI might display each character of the option
 * as a '*' (for example).
 *
 * Returns: %TRUE if the option's value should be obscured.
 */
gboolean
purple_account_option_string_get_masked(const PurpleAccountOption *option);

/**
 * purple_account_option_string_get_hints:
 * @option: The account option.
 *
 * Returns the list of hints for an account option.
 *
 * Returns: (transfer none): A list of hints, stored as strings.
 */
const GSList * purple_account_option_string_get_hints(const PurpleAccountOption *option);

/**
 * purple_account_option_get_list:
 * @option: The account option.
 *
 * Returns the list values for an account option.
 *
 * Returns: (transfer none): A list of #PurpleKeyValuePair, mapping the human-readable
 *              description of the value to the <type>(const char *)</type> that
 *              should be passed to purple_account_set_string() to set the
 *              option.
 */
GList *purple_account_option_get_list(const PurpleAccountOption *option);


/**************************************************************************/
/* Account User Split API                                                 */
/**************************************************************************/

/**
 * purple_account_user_split_new:
 * @text:          The text of the option.
 * @default_value: The default value.
 * @sep:           The field separator.
 *
 * Creates a new account username split.
 *
 * Returns: The new user split.
 */
PurpleAccountUserSplit *purple_account_user_split_new(const char *text,
												  const char *default_value,
												  char sep);

/**
 * purple_account_user_split_destroy:
 * @split: The split to destroy.
 *
 * Destroys an account username split.
 */
void purple_account_user_split_destroy(PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_text:
 * @split: The account username split.
 *
 * Returns the text for an account username split.
 *
 * Returns: The account username split's text.
 */
const char *purple_account_user_split_get_text(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_default_value:
 * @split: The account username split.
 *
 * Returns the default string value for an account split.
 *
 * Returns: The default string.
 */
const char *purple_account_user_split_get_default_value(
		const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_separator:
 * @split: The account username split.
 *
 * Returns the field separator for an account split.
 *
 * Returns: The field separator.
 */
char purple_account_user_split_get_separator(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_reverse:
 * @split: The account username split.
 *
 * Returns the 'reverse' value for an account split.
 *
 * Returns: The 'reverse' value.
 */
gboolean purple_account_user_split_get_reverse(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_set_reverse:
 * @split:   The account username split.
 * @reverse: The 'reverse' value
 *
 * Sets the 'reverse' value for an account split.
 */
void purple_account_user_split_set_reverse(PurpleAccountUserSplit *split, gboolean reverse);

G_END_DECLS

#endif /* _PURPLE_ACCOUNTOPT_H_ */
