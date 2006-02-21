/**
 * @file accountopt.h Account Options API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_ACCOUNTOPT_H_
#define _GAIM_ACCOUNTOPT_H_

#include "prefs.h"

/**
 * An option for an account.
 *
 * This is set by protocol plugins, and appears in the account settings
 * dialogs.
 */
typedef struct
{
	GaimPrefType type;      /**< The type of value.                     */

	char *text;             /**< The text that will appear to the user. */
	char *pref_name;        /**< The name of the associated preference. */

	union
	{
		gboolean boolean;   /**< The default boolean value.             */
		int integer;        /**< The default integer value.             */
		char *string;       /**< The default string value.              */
		GList *list;        /**< The default list value.                */

	} default_value;

	gboolean masked;

} GaimAccountOption;

/**
 * A username split.
 *
 * This is used by some protocols to separate the fields of the username
 * into more human-readable components.
 */
typedef struct
{
	char *text;             /**< The text that will appear to the user. */
	char *default_value;    /**< The default value.                     */
	char  field_sep;        /**< The field separator.                   */

} GaimAccountUserSplit;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Account Option API                                              */
/**************************************************************************/
/*@{*/

/**
 * Creates a new account option.
 *
 * @param type      The type of option.
 * @param text      The text of the option.
 * @param pref_name The account preference name for the option.
 *
 * @return The account option.
 */
GaimAccountOption *gaim_account_option_new(GaimPrefType type, const char *text,
										   const char *pref_name);

/**
 * Creates a new boolean account option.
 *
 * @param text          The text of the option.
 * @param pref_name     The account preference name for the option.
 * @param default_value The default value.
 *
 * @return The account option.
 */
GaimAccountOption *gaim_account_option_bool_new(const char *text,
												const char *pref_name,
												gboolean default_value);

/**
 * Creates a new integer account option.
 *
 * @param text          The text of the option.
 * @param pref_name     The account preference name for the option.
 * @param default_value The default value.
 *
 * @return The account option.
 */
GaimAccountOption *gaim_account_option_int_new(const char *text,
											   const char *pref_name,
											   int default_value);

/**
 * Creates a new string account option.
 *
 * @param text          The text of the option.
 * @param pref_name     The account preference name for the option.
 * @param default_value The default value.
 *
 * @return The account option.
 */
GaimAccountOption *gaim_account_option_string_new(const char *text,
												  const char *pref_name,
												  const char *default_value);

/**
 * Creates a new list account option.
 *
 * The list passed will be owned by the account option, and the
 * strings inside will be freed automatically.
 *
 * The list is a list of GaimKeyValuePair items. The key is the ID stored and
 * used internally, and the value is the label displayed.
 *
 * @param text      The text of the option.
 * @param pref_name The account preference name for the option.
 * @param list      The key, value list.
 *
 * @return The account option.
 */
GaimAccountOption *gaim_account_option_list_new(const char *text,
												const char *pref_name,
												GList *list);

/**
 * Destroys an account option.
 *
 * @param option The option to destroy.
 */
void gaim_account_option_destroy(GaimAccountOption *option);

/**
 * Sets the default boolean value for an account option.
 *
 * @param option The account option.
 * @param value  The default boolean value.
 */
void gaim_account_option_set_default_bool(GaimAccountOption *option,
										  gboolean value);

/**
 * Sets the default integer value for an account option.
 *
 * @param option The account option.
 * @param value  The default integer value.
 */
void gaim_account_option_set_default_int(GaimAccountOption *option,
										 int value);

/**
 * Sets the default string value for an account option.
 *
 * @param option The account option.
 * @param value  The default string value.
 */
void gaim_account_option_set_default_string(GaimAccountOption *option,
											const char *value);

/**
 * Sets the masking for an account option.
 *
 * @param option The account option.
 * @param masked  The masking.
 */
void
gaim_account_option_set_masked(GaimAccountOption *option, gboolean masked);

/**
 * Sets the list values for an account option.
 *
 * The list passed will be owned by the account option, and the
 * strings inside will be freed automatically.
 *
 * The list is in key, value pairs. The key is the ID stored and used
 * internally, and the value is the label displayed.
 *
 * @param option The account option.
 * @param values The default list value.
 */
void gaim_account_option_set_list(GaimAccountOption *option, GList *values);

/**
 * Adds an item to a list account option.
 *
 * @param option The account option.
 * @param key    The key.
 * @param value  The value.
 */
void gaim_account_option_add_list_item(GaimAccountOption *option,
									   const char *key, const char *value);

/**
 * Returns the specified account option's type.
 *
 * @param option The account option.
 *
 * @return The account option's type.
 */
GaimPrefType gaim_account_option_get_type(const GaimAccountOption *option);

/**
 * Returns the text for an account option.
 *
 * @param option The account option.
 *
 * @return The account option's text.
 */
const char *gaim_account_option_get_text(const GaimAccountOption *option);

/**
 * Returns the account setting for an account option.
 *
 * @param option The account option.
 *
 * @return The account setting.
 */
const char *gaim_account_option_get_setting(const GaimAccountOption *option);

/**
 * Returns the default boolean value for an account option.
 *
 * @param option The account option.
 *
 * @return The default boolean value.
 */
gboolean gaim_account_option_get_default_bool(const GaimAccountOption *option);

/**
 * Returns the default integer value for an account option.
 *
 * @param option The account option.
 *
 * @return The default integer value.
 */
int gaim_account_option_get_default_int(const GaimAccountOption *option);

/**
 * Returns the default string value for an account option.
 *
 * @param option The account option.
 *
 * @return The default string value.
 */
const char *gaim_account_option_get_default_string(
	const GaimAccountOption *option);

/**
 * Returns the default string value for a list account option.
 *
 * @param option The account option.
 *
 * @return The default list string value.
 */
const char *gaim_account_option_get_default_list_value(
	const GaimAccountOption *option);

/**
 * Returns the masking for an account option.
 *
 * @param option The account option.
 *
 * @return The masking.
 */
gboolean
gaim_account_option_get_masked(const GaimAccountOption *option);

/**
 * Returns the list values for an account option.
 *
 * @param option The account option.
 *
 * @return The list values.
 */
const GList *gaim_account_option_get_list(const GaimAccountOption *option);

/*@}*/


/**************************************************************************/
/** @name Account User Split API                                          */
/**************************************************************************/
/*@{*/

/**
 * Creates a new account username split.
 *
 * @param text          The text of the option.
 * @param default_value The default value.
 * @param sep           The field separator.
 *
 * @return The new user split.
 */
GaimAccountUserSplit *gaim_account_user_split_new(const char *text,
												  const char *default_value,
												  char sep);

/**
 * Destroys an account username split.
 *
 * @param split The split to destroy.
 */
void gaim_account_user_split_destroy(GaimAccountUserSplit *split);

/**
 * Returns the text for an account username split.
 *
 * @param split The account username split.
 *
 * @return The account username split's text.
 */
const char *gaim_account_user_split_get_text(const GaimAccountUserSplit *split);

/**
 * Returns the default string value for an account split.
 *
 * @param split The account username split.
 *
 * @return The default string.
 */
const char *gaim_account_user_split_get_default_value(
		const GaimAccountUserSplit *split);

/**
 * Returns the field separator for an account split.
 *
 * @param split The account username split.
 *
 * @return The field separator.
 */
char gaim_account_user_split_get_separator(const GaimAccountUserSplit *split);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_ACCOUNTOPT_H_ */
