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

#include "account.h"

#include <libgadu.h>
#include <debug.h>

#include "deprecated.h"
#include "purplew.h"
#include "utils.h"
#include "libgaduw.h"
#include "validator.h"

/*******************************************************************************
 * Token requesting.
 ******************************************************************************/

typedef struct
{
	ggp_account_token_cb callback;
	PurpleConnection *gc;
	void *user_data;
} ggp_account_token_reqdata;

static void ggp_account_token_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _reqdata);

/******************************************************************************/

void ggp_account_token_free(ggp_account_token *token)
{
	if (!token)
		return;
	g_free(token->id);
	g_free(token->data);
	g_free(token);
}

void ggp_account_token_request(PurpleConnection *gc,
	ggp_account_token_cb callback, void *user_data)
{
	struct gg_http *h;
	ggp_account_token_reqdata *reqdata;

	purple_debug_info("gg", "ggp_account_token_request: "
		"requesting token...\n");

	if (!ggp_deprecated_setup_proxy(gc))
	{
		callback(gc, NULL, user_data);
		return;
	}
	
	h = gg_token(TRUE);
	
	if (!h)
	{
		callback(gc, NULL, user_data);
		return;
	}
	
	reqdata = g_new(ggp_account_token_reqdata, 1);
	reqdata->callback = callback;
	reqdata->gc = gc;
	reqdata->user_data = user_data;
	ggp_libgaduw_http_watch(gc, h, ggp_account_token_response, reqdata,
		TRUE);
}

static void ggp_account_token_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _reqdata)
{
	ggp_account_token_reqdata *reqdata = _reqdata;
	struct gg_token *token_info;
	ggp_account_token *token = NULL;
	
	g_assert(!(success && cancelled));
	
	if (cancelled)
		purple_debug_info("gg", "ggp_account_token_handler: "
			"cancelled\n");
	else if (success)
	{
		purple_debug_info("gg", "ggp_account_token_handler: "
			"got token\n");
	
		token = g_new(ggp_account_token, 1);
	
		token_info = h->data;
		token->id = g_strdup(token_info->tokenid);
		token->size = h->body_size;
		token->data = g_memdup(h->body, token->size);
		token->length = token_info->length;
	}
	else
	{
		purple_debug_error("gg", "ggp_account_token_handler: error\n");
		purple_notify_error(
			purple_connection_get_account(reqdata->gc),
			_("Token Error"),
			_("Unable to fetch the token."), NULL);
	}
	
	reqdata->callback(reqdata->gc, token, reqdata->user_data);
	g_free(reqdata);
}

gboolean ggp_account_token_validate(ggp_account_token *token,
	const gchar *value)
{
	if (strlen(value) != token->length)
		return FALSE;
	return g_regex_match_simple("^[a-zA-Z0-9]+$", value, 0, 0);
}

/*******************************************************************************
 * New account registration.
 ******************************************************************************/

typedef struct
{
	ggp_account_token *token;
	PurpleConnection *gc;
	
	gchar *email;
	gchar *password;
	gchar *token_value;
	gboolean password_remember;
} ggp_account_register_data;

static void ggp_account_register_dialog(PurpleConnection *gc,
	ggp_account_token *token, gpointer _register_data);
static void ggp_account_register_dialog_ok(
	ggp_account_register_data *register_data, PurpleRequestFields *fields);
#if 0
static void ggp_account_register_dialog_invalid(
	ggp_account_register_data *register_data, const gchar *message);
#endif
static void ggp_account_register_dialog_cancel(
	ggp_account_register_data *register_data, PurpleRequestFields *fields);
static void ggp_account_register_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _reqdata);
static void ggp_account_register_completed(
	ggp_account_register_data *register_data, gboolean success);

#define GGP_ACCOUNT_REGISTER_TITLE _("Register New Gadu-Gadu Account")

/******************************************************************************/

void ggp_account_register(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	ggp_account_register_data *register_data;
	
	purple_debug_info("gg", "ggp_account_register\n");
	
	register_data = g_new0(ggp_account_register_data, 1);
	register_data->gc = gc;
	register_data->password_remember = TRUE;
	
	ggp_account_token_request(gc, ggp_account_register_dialog,
		register_data);
}

static void ggp_account_register_dialog(PurpleConnection *gc,
	ggp_account_token *token, gpointer _register_data)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *main_group, *password_group, *token_group;
	PurpleRequestField *field, *field_password;
	ggp_account_register_data *register_data = _register_data;
	
	purple_debug_info("gg", "ggp_account_register_dialog(%p, %p, %p)\n",
		gc, token, _register_data);
	if (!token)
	{
		ggp_account_register_completed(register_data, FALSE);
		return;
	}
	
	fields = purple_request_fields_new();
	main_group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, main_group);
	
	field = purple_request_field_string_new("email", _("Email"),
		register_data->email, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_validator(field,
		purple_request_field_email_validator, NULL);
	purple_request_field_group_add_field(main_group, field);

	password_group = purple_request_field_group_new(_("Password"));
	purple_request_fields_add_group(fields, password_group);

	field = purple_request_field_string_new("password1", _("Password"),
		register_data->password, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_password, NULL);
	purple_request_field_group_add_field(password_group, field);
	field_password = field;
	
	field = purple_request_field_string_new("password2",
		_("Password (again)"), register_data->password, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_password_equal,
		field_password);
	purple_request_field_group_add_field(password_group, field);
	
	field = purple_request_field_bool_new("password_remember",
		_("Remember password"), register_data->password_remember);
	purple_request_field_group_add_field(password_group, field);
	
	token_group = purple_request_field_group_new(_("Captcha"));
	purple_request_fields_add_group(fields, token_group);
	
	field = purple_request_field_string_new("token_value",
		_("Enter text from image below"), register_data->token_value,
		FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_token, token);
	purple_request_field_group_add_field(token_group, field);
	purple_debug_info("gg", "token set %p\n", register_data->token);
	
	field = purple_request_field_image_new("token_image", _("Captcha"),
		token->data, token->size);
	purple_request_field_group_add_field(token_group, field);

	register_data->token = token;
	
	purple_request_fields(gc,
		GGP_ACCOUNT_REGISTER_TITLE,
		GGP_ACCOUNT_REGISTER_TITLE,
		_("Please, fill in the following fields"), fields,
		_("OK"), G_CALLBACK(ggp_account_register_dialog_ok),
		_("Cancel"), G_CALLBACK(ggp_account_register_dialog_cancel),
		purple_connection_get_account(gc), NULL, NULL, register_data);
}

static void ggp_account_register_dialog_cancel(
	ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	purple_debug_info("gg", "ggp_account_register_dialog_cancel(%p, %p)\n",
		register_data, fields);

	ggp_account_register_completed(register_data, FALSE);
}

static void ggp_account_register_dialog_ok(
	ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	struct gg_http *h;

	purple_debug_misc("gg", "ggp_account_register_dialog_ok(%p, %p)\n",
		register_data, fields);

	g_free(register_data->email);
	g_free(register_data->password);
	g_free(register_data->token_value);
	
	register_data->email = g_strdup(
		purple_request_fields_get_string(fields, "email"));
	register_data->password = g_strdup(
		purple_request_fields_get_string(fields, "password1"));
	register_data->password_remember =
		purple_request_fields_get_bool(fields, "password_remember");
	register_data->token_value = g_strdup(
		purple_request_fields_get_string(fields, "token_value"));

	g_assert(register_data->email != NULL);
	g_assert(register_data->password != NULL);
	g_assert(register_data->token_value != NULL);

	h = gg_register3(register_data->email, register_data->password,
		register_data->token->id, register_data->token_value, TRUE);
	
	ggp_libgaduw_http_watch(register_data->gc, h,
		ggp_account_register_response, register_data, TRUE);
}

#if 0
// libgadu 1.12.x: use it for invalid token
static void ggp_account_register_dialog_invalid(
	ggp_account_register_data *register_data, const gchar *message)
{
	purple_debug_warning("gg", "ggp_account_register_dialog_invalid: %s\n",
		message);
	ggp_account_register_dialog(register_data->gc, register_data->token,
		register_data);
	purple_notify_error(purple_connection_get_account(register_data->gc),
		GGP_ACCOUNT_REGISTER_TITLE, message, NULL);
}
#endif

static void ggp_account_register_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _register_data)
{
	ggp_account_register_data *register_data = _register_data;
	PurpleAccount *account =
		purple_connection_get_account(register_data->gc);
	struct gg_pubdir *register_result = h->data;
	uin_t uin;
	gchar *tmp;
	
	g_assert(!(success && cancelled));
	
	if (cancelled)
	{
		purple_debug_info("gg", "ggp_account_register_response: "
			"cancelled\n");
		ggp_account_register_completed(register_data, FALSE);
		return;
	}
	if (!success || !register_result->success)
	{
		//TODO (libgadu 1.12.x): check register_result->error
		purple_debug_error("gg", "ggp_account_register_response: "
			"error\n");
		purple_notify_error(NULL,
			GGP_ACCOUNT_REGISTER_TITLE,
			_("Unable to register new account. "
			"An unknown error occurred."), NULL);
		ggp_account_register_completed(register_data, FALSE);
		return;
	}

	uin = register_result->uin;
	purple_debug_info("gg", "ggp_account_register_response: "
		"registered uin %u\n", uin);
	
	purple_account_set_username(account, ggp_uin_to_str(uin));
	purple_account_set_remember_password(account,
		register_data->password_remember);
	purple_account_set_password(account, register_data->password, NULL, NULL);
	
	tmp = g_strdup_printf(_("Your new GG number: %u."), uin);
	purple_notify_info(account, GGP_ACCOUNT_REGISTER_TITLE,
		_("Registration completed successfully!"), tmp);
	g_free(tmp);
	
	ggp_account_register_completed(register_data, TRUE);
}

static void ggp_account_register_completed(
	ggp_account_register_data *register_data, gboolean success)
{
	PurpleAccount *account =
		purple_connection_get_account(register_data->gc);

	purple_debug_misc("gg", "ggp_account_register_completed: %d\n",
		success);
	
	g_free(register_data->email);
	g_free(register_data->password);
	g_free(register_data->token_value);
	ggp_account_token_free(register_data->token);
	g_free(register_data);
	
	purple_account_disconnect(account);
	purple_account_register_completed(account, success);
}

/*******************************************************************************
 * Password change.
 ******************************************************************************/

typedef struct
{
	ggp_account_token *token;
	PurpleConnection *gc;
	
	gchar *email;
	gchar *password_current;
	gchar *password_new;
	gchar *token_value;
} ggp_account_chpass_data;

static void ggp_account_chpass_data_free(ggp_account_chpass_data *chpass_data);
static void ggp_account_chpass_dialog(PurpleConnection *gc,
	ggp_account_token *token, gpointer _chpass_data);
static void ggp_account_chpass_dialog_ok(
	ggp_account_chpass_data *chpass_data, PurpleRequestFields *fields);
static void ggp_account_chpass_dialog_invalid(
	ggp_account_chpass_data *chpass_data, const gchar *message);
static void ggp_account_chpass_dialog_cancel(
	ggp_account_chpass_data *chpass_data, PurpleRequestFields *fields);
static void ggp_account_chpass_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _chpass_data);

#define GGP_ACCOUNT_CHPASS_TITLE _("Password change")

/******************************************************************************/

static void ggp_account_chpass_data_free(ggp_account_chpass_data *chpass_data)
{
	g_free(chpass_data->email);
	g_free(chpass_data->password_current);
	g_free(chpass_data->password_new);
	g_free(chpass_data->token_value);
	ggp_account_token_free(chpass_data->token);
	g_free(chpass_data);
}

void ggp_account_chpass(PurpleConnection *gc)
{
	ggp_account_chpass_data *chpass_data;
	void ggp_account_change_passwd(PurpleConnection *gc);
	purple_debug_info("gg", "ggp_account_chpass\n");
	
	chpass_data = g_new0(ggp_account_chpass_data, 1);
	chpass_data->gc = gc;
	
	ggp_account_token_request(gc, ggp_account_chpass_dialog, chpass_data);
}

static void ggp_account_chpass_dialog(PurpleConnection *gc,
	ggp_account_token *token, gpointer _chpass_data)
{
	ggp_account_chpass_data *chpass_data = _chpass_data;
	PurpleAccount *account = purple_connection_get_account(chpass_data->gc);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *main_group, *password_group, *token_group;
	PurpleRequestField *field, *field_password;
	gchar *primary;
	
	purple_debug_info("gg", "ggp_account_chpass_dialog(%p, %p, %p)\n",
		gc, token, _chpass_data);
	if (!token)
	{
		ggp_account_chpass_data_free(chpass_data);
		return;
	}
	
	fields = purple_request_fields_new();
	main_group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, main_group);
	
	field = purple_request_field_string_new("email",
		_("New email address"), chpass_data->email, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_validator(field,
		purple_request_field_email_validator, NULL);
	purple_request_field_group_add_field(main_group, field);

	password_group = purple_request_field_group_new(_("Password"));
	purple_request_fields_add_group(fields, password_group);

	field = purple_request_field_string_new("password_current",
		_("Current password"), chpass_data->password_current, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(password_group, field);
	
	field = purple_request_field_string_new("password_new1",
		_("Password"), chpass_data->password_new, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_password, NULL);
	purple_request_field_group_add_field(password_group, field);
	field_password = field;
	
	field = purple_request_field_string_new("password_new2",
		_("Password (retype)"), chpass_data->password_new, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_password_equal,
		field_password);
	purple_request_field_group_add_field(password_group, field);
	
	token_group = purple_request_field_group_new(_("Captcha"));
	purple_request_fields_add_group(fields, token_group);
	
	field = purple_request_field_string_new("token_value",
		_("Enter text from image below"), chpass_data->token_value,
		FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_validator(field, ggp_validator_token, token);
	purple_request_field_group_add_field(token_group, field);
	
	field = purple_request_field_image_new("token_image", _("Captcha"),
		token->data, token->size);
	purple_request_field_group_add_field(token_group, field);
	
	chpass_data->token = token;
	
	primary = g_strdup_printf(_("Change password for %s"),
		purple_account_get_username(account));
	
	purple_request_fields(gc, GGP_ACCOUNT_CHPASS_TITLE, primary,
		_("Please enter your current password and your new password."),
		fields,
		_("OK"), G_CALLBACK(ggp_account_chpass_dialog_ok),
		_("Cancel"), G_CALLBACK(ggp_account_chpass_dialog_cancel),
		account, NULL, NULL, chpass_data);
	
	g_free(primary);
}

static void ggp_account_chpass_dialog_ok(
	ggp_account_chpass_data *chpass_data, PurpleRequestFields *fields)
{
	PurpleAccount *account = purple_connection_get_account(chpass_data->gc);
	struct gg_http *h;
	uin_t uin;

	purple_debug_misc("gg", "ggp_account_chpass_dialog_ok(%p, %p)\n",
		chpass_data, fields);

	g_free(chpass_data->email);
	g_free(chpass_data->password_current);
	g_free(chpass_data->password_new);
	g_free(chpass_data->token_value);
	
	chpass_data->email = g_strdup(
		purple_request_fields_get_string(fields, "email"));
	chpass_data->password_current = g_strdup(
		purple_request_fields_get_string(fields, "password_current"));
	chpass_data->password_new = g_strdup(
		purple_request_fields_get_string(fields, "password_new1"));
	chpass_data->token_value = g_strdup(
		purple_request_fields_get_string(fields, "token_value"));

	g_assert(chpass_data->email != NULL);
	g_assert(chpass_data->password_current != NULL);
	g_assert(chpass_data->password_new != NULL);
	g_assert(chpass_data->token_value != NULL);

	if (g_utf8_collate(chpass_data->password_current,
		purple_connection_get_password(chpass_data->gc)) != 0)
	{
		g_free(chpass_data->password_current);
		chpass_data->password_current = NULL;
		ggp_account_chpass_dialog_invalid(chpass_data,
			_("Your current password is different from the one that"
			" you specified."));
		return;
	}
	if (g_utf8_collate(chpass_data->password_current,
		chpass_data->password_new) == 0)
	{
		g_free(chpass_data->password_new);
		chpass_data->password_new = NULL;
		ggp_account_chpass_dialog_invalid(chpass_data,
			_("New password have to be different from the current "
			"one."));
		return;
	}

	uin = ggp_str_to_uin(purple_account_get_username(account));
	purple_debug_info("gg", "ggp_account_chpass_dialog_ok: validation ok "
		"[token id=%s, value=%s]\n",
		chpass_data->token->id, chpass_data->token_value);
	h = gg_change_passwd4(uin, chpass_data->email,
		chpass_data->password_current, chpass_data->password_new,
		chpass_data->token->id, chpass_data->token_value, TRUE);
	
	ggp_libgaduw_http_watch(chpass_data->gc, h,
		ggp_account_chpass_response, chpass_data, TRUE);
}

static void ggp_account_chpass_dialog_invalid(
	ggp_account_chpass_data *chpass_data, const gchar *message)
{
	purple_debug_warning("gg", "ggp_account_chpass_dialog_invalid: %s\n",
		message);
	ggp_account_chpass_dialog(chpass_data->gc, chpass_data->token,
		chpass_data);
	purple_notify_error(purple_connection_get_account(chpass_data->gc),
		GGP_ACCOUNT_CHPASS_TITLE, message, NULL);
}

static void ggp_account_chpass_dialog_cancel(
	ggp_account_chpass_data *chpass_data, PurpleRequestFields *fields)
{
	ggp_account_chpass_data_free(chpass_data);
}

static void ggp_account_chpass_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _chpass_data)
{
	ggp_account_chpass_data *chpass_data = _chpass_data;
	PurpleAccount *account =
		purple_connection_get_account(chpass_data->gc);
	struct gg_pubdir *chpass_result = h->data;
	
	g_assert(!(success && cancelled));
	
	if (cancelled)
	{
		purple_debug_info("gg", "ggp_account_chpass_response: "
			"cancelled\n");
		ggp_account_chpass_data_free(chpass_data);
		return;
	}
	if (!success || !chpass_result->success)
	{
		//TODO (libgadu 1.12.x): check chpass_result->error
		purple_debug_error("gg", "ggp_account_chpass_response: "
			"error\n");
		purple_notify_error(NULL,
			GGP_ACCOUNT_CHPASS_TITLE,
			_("Unable to change password. "
			"An unknown error occurred."), NULL);
		ggp_account_chpass_data_free(chpass_data);
		return;
	}

	purple_debug_info("gg", "ggp_account_chpass_response: "
		"password changed\n");
	
	purple_account_set_password(account, chpass_data->password_new, NULL, NULL);

	purple_notify_info(account, GGP_ACCOUNT_CHPASS_TITLE,
		_("Your password has been changed."), NULL);
	
	ggp_account_chpass_data_free(chpass_data);
	
	//TODO: reconnect / check how it is done in original client
	purple_account_disconnect(account);
}
