#include "account.h"

#include <libgadu.h>
#include <debug.h>
#include <request.h>

#include "deprecated.h"
#include "purplew.h"
#include "utils.h"
#include "libgaduw.h"

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
	ggp_account_token *token, gpointer user_data);
static void ggp_account_register_dialog_ok(
	ggp_account_register_data *register_data, PurpleRequestFields *fields);
static void ggp_account_register_dialog_invalid(
	ggp_account_register_data *register_data, const gchar *message);
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
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *main_group, *password_group, *token_group;
	PurpleRequestField *field;
	ggp_account_register_data *register_data = _register_data;
	
	purple_debug_info("gg", "ggp_account_register_dialog(%x, %x, %x)\n",
		(unsigned int)gc, (unsigned int)token,
		(unsigned int)_register_data);
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
	purple_request_field_group_add_field(main_group, field);

	password_group = purple_request_field_group_new(_("Password"));
	purple_request_fields_add_group(fields, password_group);

	field = purple_request_field_string_new("password1", _("Password"),
		register_data->password, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(password_group, field);
	
	field = purple_request_field_string_new("password2",
		_("Password (again)"), register_data->password, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_string_set_masked(field, TRUE);
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
	purple_request_field_group_add_field(token_group, field);
	
	field = purple_request_field_image_new("token_image", _("Captcha"),
		token->data, token->size);
	purple_request_field_group_add_field(token_group, field);
	
	register_data->token = token;
	register_data->password = NULL;
	
	purple_request_fields(gc,
		GGP_ACCOUNT_REGISTER_TITLE,
		GGP_ACCOUNT_REGISTER_TITLE,
		_("Please, fill in the following fields"), fields,
		_("OK"), G_CALLBACK(ggp_account_register_dialog_ok),
		_("Cancel"), G_CALLBACK(ggp_account_register_dialog_cancel),
		account, NULL, NULL, register_data);
}

static void ggp_account_register_dialog_cancel(
	ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	purple_debug_info("gg", "ggp_account_register_dialog_cancel(%x, %x)\n",
		(unsigned int)register_data, (unsigned int)fields);

	ggp_account_register_completed(register_data, FALSE);
}

static void ggp_account_register_dialog_ok(
	ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	const gchar *password2;
	struct gg_http *h;

	purple_debug_misc("gg", "ggp_account_register_dialog_ok(%x, %x)\n",
		(unsigned int)register_data, (unsigned int)fields);

	g_free(register_data->email);
	g_free(register_data->password);
	g_free(register_data->token_value);
	
	register_data->email = g_strdup(
		purple_request_fields_get_string(fields, "email"));
	register_data->password = g_strdup(
		purple_request_fields_get_string(fields, "password1"));
	password2 = purple_request_fields_get_string(fields, "password2");
	register_data->password_remember =
		purple_request_fields_get_bool(fields, "password_remember");
	register_data->token_value = g_strdup(
		purple_request_fields_get_string(fields, "token_value"));

	g_assert(register_data->email != NULL);
	g_assert(register_data->password != NULL);
	g_assert(password2 != NULL);
	g_assert(register_data->token_value != NULL);

	if (g_utf8_collate(register_data->password, password2) != 0)
	{
		g_free(register_data->password);
		register_data->password = NULL;
		ggp_account_register_dialog_invalid(register_data,
			_("Passwords do not match"));
		return;
	}
	if (!ggp_password_validate(register_data->password))
	{
		g_free(register_data->password);
		register_data->password = NULL;
		ggp_account_register_dialog_invalid(register_data,
			_("Password can contain 6-15 alphanumeric characters"));
		return;
	}
	if (!purple_email_is_valid(register_data->email))
	{
		ggp_account_register_dialog_invalid(register_data,
			_("Invalid email address"));
		return;
	}
	if (strlen(register_data->token_value) != register_data->token->length
		|| !g_regex_match_simple("^[a-zA-Z0-9]+$",
			register_data->token_value, 0, 0))
	{
		ggp_account_register_dialog_invalid(register_data,
			_("Captcha validation failed"));
		return;
	}

	purple_debug_info("gg", "ggp_account_register_dialog_ok: validation ok "
		"[token id=%s, value=%s]\n",
		register_data->token->id, register_data->token_value);
	h = gg_register3(register_data->email, register_data->password,
		register_data->token->id, register_data->token_value, TRUE);
	
	ggp_libgaduw_http_watch(register_data->gc, h,
		ggp_account_register_response, register_data, TRUE);
}

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
	purple_debug_info("gg", "ggp_account_register_dialog_ok: "
		"registered uin %u\n", uin);
	
	purple_account_set_username(account, ggp_uin_to_str(uin));
	purple_account_set_remember_password(account,
		register_data->password_remember);
	purple_account_set_password(account, register_data->password);
	
	tmp = g_strdup_printf(_("Your new GG number: %u."), uin);
	purple_notify_info(NULL, GGP_ACCOUNT_REGISTER_TITLE,
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
