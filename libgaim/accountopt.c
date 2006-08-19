/**
 * @file accountopt.c Account Options API
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
#include "accountopt.h"
#include "util.h"

GaimAccountOption *
gaim_account_option_new(GaimPrefType type, const char *text,
						const char *pref_name)
{
	GaimAccountOption *option;

	g_return_val_if_fail(type      != GAIM_PREF_NONE, NULL);
	g_return_val_if_fail(text      != NULL,           NULL);
	g_return_val_if_fail(pref_name != NULL,           NULL);

	option = g_new0(GaimAccountOption, 1);

	option->type      = type;
	option->text      = g_strdup(text);
	option->pref_name = g_strdup(pref_name);

	return option;
}

GaimAccountOption *
gaim_account_option_bool_new(const char *text, const char *pref_name,
							 gboolean default_value)
{
	GaimAccountOption *option;

	option = gaim_account_option_new(GAIM_PREF_BOOLEAN, text, pref_name);

	if (option == NULL)
		return NULL;

	option->default_value.boolean = default_value;

	return option;
}

GaimAccountOption *
gaim_account_option_int_new(const char *text, const char *pref_name,
							int default_value)
{
	GaimAccountOption *option;

	option = gaim_account_option_new(GAIM_PREF_INT, text, pref_name);

	if (option == NULL)
		return NULL;

	option->default_value.integer = default_value;

	return option;
}

GaimAccountOption *
gaim_account_option_string_new(const char *text, const char *pref_name,
							   const char *default_value)
{
	GaimAccountOption *option;

	option = gaim_account_option_new(GAIM_PREF_STRING, text, pref_name);

	if (option == NULL)
		return NULL;

	option->default_value.string = g_strdup(default_value);

	return option;
}

GaimAccountOption *
gaim_account_option_list_new(const char *text, const char *pref_name,
							 GList *list)
{
	GaimAccountOption *option;

	option = gaim_account_option_new(GAIM_PREF_STRING_LIST, text, pref_name);

	if (option == NULL)
		return NULL;

	option->default_value.list = list;

	return option;
}

void
gaim_account_option_destroy(GaimAccountOption *option)
{
	g_return_if_fail(option != NULL);

	g_free(option->text);
	g_free(option->pref_name);

	if (option->type == GAIM_PREF_STRING)
	{
		g_free(option->default_value.string);
	}
	else if (option->type == GAIM_PREF_STRING_LIST)
	{
		if (option->default_value.list != NULL)
		{
			g_list_foreach(option->default_value.list, (GFunc)g_free, NULL);
			g_list_free(option->default_value.list);
		}
	}

	g_free(option);
}

void
gaim_account_option_set_default_bool(GaimAccountOption *option,
									 gboolean value)
{
	g_return_if_fail(option != NULL);
	g_return_if_fail(option->type == GAIM_PREF_BOOLEAN);

	option->default_value.boolean = value;
}

void
gaim_account_option_set_default_int(GaimAccountOption *option, int value)
{
	g_return_if_fail(option != NULL);
	g_return_if_fail(option->type == GAIM_PREF_INT);

	option->default_value.integer = value;
}

void
gaim_account_option_set_default_string(GaimAccountOption *option,
									   const char *value)
{
	g_return_if_fail(option != NULL);
	g_return_if_fail(option->type == GAIM_PREF_STRING);

	g_free(option->default_value.string);
	option->default_value.string = g_strdup(value);
}

void
gaim_account_option_set_masked(GaimAccountOption *option, gboolean masked)
{
	g_return_if_fail(option != NULL);
	g_return_if_fail(option->type == GAIM_PREF_STRING);

	option->masked = masked;
}


void
gaim_account_option_set_list(GaimAccountOption *option, GList *values)
{
	g_return_if_fail(option != NULL);
	g_return_if_fail(option->type == GAIM_PREF_STRING_LIST);

	if (option->default_value.list != NULL)
	{
		g_list_foreach(option->default_value.list, (GFunc)g_free, NULL);
		g_list_free(option->default_value.list);
	}

	option->default_value.list = values;
}

void
gaim_account_option_add_list_item(GaimAccountOption *option,
								  const char *key, const char *value)
{
	GaimKeyValuePair *kvp;

	g_return_if_fail(option != NULL);
	g_return_if_fail(key    != NULL);
	g_return_if_fail(value  != NULL);
	g_return_if_fail(option->type == GAIM_PREF_STRING_LIST);

	kvp = g_new0(GaimKeyValuePair, 1);
	kvp->key = g_strdup(key);
	kvp->value = g_strdup(value);

	option->default_value.list = g_list_append(option->default_value.list,
											   kvp);
}

GaimPrefType
gaim_account_option_get_type(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, GAIM_PREF_NONE);

	return option->type;
}

const char *
gaim_account_option_get_text(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, NULL);

	return option->text;
}

const char *
gaim_account_option_get_setting(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, NULL);

	return option->pref_name;
}

gboolean
gaim_account_option_get_default_bool(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, FALSE);
	g_return_val_if_fail(option->type == GAIM_PREF_BOOLEAN, FALSE);

	return option->default_value.boolean;
}

int
gaim_account_option_get_default_int(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, -1);
	g_return_val_if_fail(option->type == GAIM_PREF_INT, -1);

	return option->default_value.integer;
}

const char *
gaim_account_option_get_default_string(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, NULL);
	g_return_val_if_fail(option->type == GAIM_PREF_STRING, NULL);

	return option->default_value.string;
}

const char *
gaim_account_option_get_default_list_value(const GaimAccountOption *option)
{
	GaimKeyValuePair *kvp;

	g_return_val_if_fail(option != NULL, NULL);
	g_return_val_if_fail(option->type == GAIM_PREF_STRING_LIST, NULL);

	if (option->default_value.list == NULL)
		return NULL;

	kvp = option->default_value.list->data;

	return (kvp ? kvp->value : NULL);
}

gboolean
gaim_account_option_get_masked(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, FALSE);
	g_return_val_if_fail(option->type == GAIM_PREF_STRING, FALSE);

	return option->masked;
}

const GList *
gaim_account_option_get_list(const GaimAccountOption *option)
{
	g_return_val_if_fail(option != NULL, NULL);
	g_return_val_if_fail(option->type == GAIM_PREF_STRING_LIST, NULL);

	return option->default_value.list;
}

/**************************************************************************
 * Account User Split API
 **************************************************************************/
GaimAccountUserSplit *
gaim_account_user_split_new(const char *text, const char *default_value,
							char sep)
{
	GaimAccountUserSplit *split;

	g_return_val_if_fail(text != NULL, NULL);
	g_return_val_if_fail(sep != 0, NULL);

	split = g_new0(GaimAccountUserSplit, 1);

	split->text = g_strdup(text);
	split->field_sep = sep;
	split->default_value = g_strdup(default_value);

	return split;
}

void
gaim_account_user_split_destroy(GaimAccountUserSplit *split)
{
	g_return_if_fail(split != NULL);

	g_free(split->text);
	g_free(split->default_value);
	g_free(split);
}

const char *
gaim_account_user_split_get_text(const GaimAccountUserSplit *split)
{
	g_return_val_if_fail(split != NULL, NULL);

	return split->text;
}

const char *
gaim_account_user_split_get_default_value(const GaimAccountUserSplit *split)
{
	g_return_val_if_fail(split != NULL, NULL);

	return split->default_value;
}

char
gaim_account_user_split_get_separator(const GaimAccountUserSplit *split)
{
	g_return_val_if_fail(split != NULL, 0);

	return split->field_sep;
}
