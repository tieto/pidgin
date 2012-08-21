/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "validator.h"

#include "account.h"
#include "utils.h"

gboolean ggp_validator_token(PurpleRequestField *field, gchar **errmsg,
	void *token)
{
	const char *value;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value = purple_request_field_string_get_value(field);
	
	if (value != NULL && ggp_account_token_validate(token, value))
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Captcha validation failed"));
	return FALSE;
}

gboolean ggp_validator_password(PurpleRequestField *field, gchar **errmsg,
	void *user_data)
{
	const char *value;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value = purple_request_field_string_get_value(field);
	
	if (value != NULL && ggp_password_validate(value))
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Password can contain 6-15 alphanumeric characters"));
	return FALSE;
}

gboolean ggp_validator_password_equal(PurpleRequestField *field, gchar **errmsg,
	void *field2_p)
{
	const char *value1, *value2;
	PurpleRequestField *field2 = field2_p;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field2 != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field2) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value1 = purple_request_field_string_get_value(field);
	value2 = purple_request_field_string_get_value(field2);
	
	if (g_strcmp0(value1, value2) == 0)
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Passwords do not match"));
	return FALSE;
}
