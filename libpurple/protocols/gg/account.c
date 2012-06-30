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

	ggp_libgaduw_http_req *req;
} ggp_account_token_reqdata;

static void ggp_account_token_response(struct gg_http *h, gboolean success, gboolean cancelled, gpointer _reqdata);

/******************************************************************************/

void ggp_account_token_free(ggp_account_token *token)
{
	g_free(token->id);
	g_free(token->data);
	g_free(token);
}

void ggp_account_token_request(PurpleConnection *gc, ggp_account_token_cb callback, void *user_data)
{
	struct gg_http *h;
	ggp_account_token_reqdata *reqdata;

	purple_debug_info("gg", "ggp_account_token_request: requesting token...\n");

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
	reqdata->req = ggp_libgaduw_http_watch(gc, h, ggp_account_token_response, reqdata, TRUE);
}

static void ggp_account_token_response(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer _reqdata)
{
	ggp_account_token_reqdata *reqdata = _reqdata;
	struct gg_token *token_info;
	ggp_account_token *token = NULL;
	
	g_assert(!(success && cancelled));
	
	if (cancelled)
		purple_debug_info("gg", "ggp_account_token_handler: cancelled\n");
	else if (success)
	{
		purple_debug_info("gg", "ggp_account_token_handler: got token\n");
	
		token = g_new(ggp_account_token, 1);
	
		token_info = h->data;
		token->id = g_strdup(token_info->tokenid);
		token->size = h->body_size;
		token->data = g_memdup(h->body, token->size);
	}
	else
	{
		purple_debug_error("gg", "ggp_account_token_handler: error\n");
		purple_notify_error(
			purple_connection_get_account(reqdata->gc),
			_("Token Error"),
			_("Unable to fetch the token.\n"), NULL);
	}
	
	gg_token_free(h);
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
} ggp_account_register_data;

static void ggp_account_register_dialog(PurpleConnection *gc, ggp_account_token *token, gpointer user_data);
static void ggp_account_register_dialog_ok(ggp_account_register_data *register_data, PurpleRequestFields *fields);
static void ggp_account_register_dialog_cancel(ggp_account_register_data *register_data, PurpleRequestFields *fields);
static void ggp_account_register_completed(ggp_account_register_data *register_data, gboolean success);

/******************************************************************************/

static void ggp_account_register_completed(ggp_account_register_data *register_data, gboolean success)
{
	PurpleAccount *account = purple_connection_get_account(register_data->gc);
	
	ggp_account_token_free(register_data->token);
	g_free(register_data);
	
	purple_account_disconnect(account);
	purple_account_register_completed(account, TRUE);
}

void ggp_account_register(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	
	purple_debug_info("gg", "ggp_account_register\n");
	
	ggp_account_token_request(gc, ggp_account_register_dialog, NULL);
}

static void ggp_account_register_dialog(PurpleConnection *gc, ggp_account_token *token, gpointer user_data)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	ggp_account_register_data *register_data;
	
	purple_debug_info("gg", "ggp_account_register_dialog(%x, %x, %x)\n",
		(unsigned int)gc, (unsigned int)token, (unsigned int)user_data);
	if (!token)
	{
		purple_account_disconnect(account);
		purple_account_register_completed(account, FALSE);
		return;
	}
	
	// TODO: required fields
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	
	field = purple_request_field_string_new("email", _("Email"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	
	field = purple_request_field_string_new("password1", _("Password"), "",
		FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(group, field);
	
	field = purple_request_field_string_new("password2", _("Password (again)"), "",
		FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_group_add_field(group, field);
	
	//TODO: move it into a new group?
	field = purple_request_field_string_new("token_value",
		_("Enter captcha text"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	
	field = purple_request_field_image_new("token_image", _("Captcha"),
		token->data, token->size);
	purple_request_field_group_add_field(group, field);
	
	register_data = g_new(ggp_account_register_data, 1);
	register_data->gc = gc;
	register_data->token = token;
	
	purple_request_fields(gc,
		_("Register New Gadu-Gadu Account"),
		_("Register New Gadu-Gadu Account"),
		_("Please, fill in the following fields"), fields,
		_("OK"), G_CALLBACK(ggp_account_register_dialog_ok),
		_("Cancel"), G_CALLBACK(ggp_account_register_dialog_cancel),
		account, NULL, NULL, register_data);
}

static void ggp_account_register_dialog_cancel(ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	purple_debug_info("gg", "ggp_account_register_dialog_cancel(%x, %x)\n",
		(unsigned int)register_data, (unsigned int)fields);

	ggp_account_register_completed(register_data, FALSE);
}

static void ggp_account_register_dialog_ok(ggp_account_register_data *register_data, PurpleRequestFields *fields)
{
	PurpleAccount *account = purple_connection_get_account(register_data->gc);
	const gchar *email, *password1, *password2, *token_value;
	struct gg_http *h;
	struct gg_pubdir *register_result;
	uin_t uin;

	purple_debug_info("gg", "ggp_account_register_dialog_ok(%x, %x)\n",
		(unsigned int)register_data, (unsigned int)fields);

	email = purple_request_fields_get_string(fields, "email");
	password1 = purple_request_fields_get_string(fields, "password1");
	password2 = purple_request_fields_get_string(fields, "password2");
	token_value = purple_request_fields_get_string(fields, "token_value");

	g_assert(email != NULL);
	g_assert(password1 != NULL);
	g_assert(password2 != NULL);
	g_assert(token_value != NULL);

	if (g_utf8_collate(password1, password2) != 0)
	{
		purple_debug_warning("gg", "ggp_account_register_dialog_ok: validation failed - passwords does not match\n");
		purple_notify_error(purple_connection_get_account(register_data->gc),
			_("TODO: title"),
			_("Passwords do not match"), NULL);
		ggp_account_register_completed(register_data, FALSE);
		return;
	}

	purple_debug_info("gg", "ggp_account_register_dialog_ok: validation ok [token id=%s, value=%s]\n",
		register_data->token->id, token_value);
	h = gg_register3(email, password1, register_data->token->id, token_value, 0); // TODO: async (use code from ggp_account_token_handler)
	if (h == NULL || !(register_result = h->data) || !register_result->success)
	{
		purple_debug_warning("gg", "ggp_account_register_dialog_ok: registration failed\n");
		purple_notify_error(purple_connection_get_account(register_data->gc),
			_("TODO: title"),
			_("Unable to register new account.  An unknown error occurred."), NULL);
		ggp_account_register_completed(register_data, FALSE);
		return;
	}
	uin = register_result->uin;
	
	purple_debug_info("gg", "ggp_account_register_dialog_ok: registered uin %u\n", uin);
	gg_register_free(h);
	
	purple_account_set_username(account, ggp_uin_to_str(uin));
	purple_account_set_remember_password(account, TRUE); //TODO: option
	purple_account_set_password(account, password1);
	
	purple_notify_info(NULL, _("New Gadu-Gadu Account Registered"),
		_("Registration completed successfully!"), NULL); //TODO: show new UIN
	
	ggp_account_register_completed(register_data, TRUE);
}
