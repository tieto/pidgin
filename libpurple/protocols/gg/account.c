#include "account.h"

#include <libgadu.h>
#include <debug.h>
#include <request.h>

#include "deprecated.h"
#include "purplew.h"
#include "utils.h"

/*******************************************************************************
 * Token requesting.
 ******************************************************************************/

typedef struct
{
	ggp_account_token_cb callback;
	PurpleConnection *gc;
	void *user_data;
	
	struct gg_http *h;
	ggp_purplew_request_processing_handle *req_processing;
	guint inpa;
} ggp_account_token_reqdata;

static void ggp_account_token_handler(gpointer _reqdata, gint fd, PurpleInputCondition cond);

/* user cancel isn't a failure */
static void ggp_account_token_cb_call(ggp_account_token_reqdata *reqdata, ggp_account_token *token, gboolean failure);

static void ggp_account_token_request_cancel(PurpleConnection *gc, void *_reqdata);

/******************************************************************************/

void ggp_account_token_free(ggp_account_token *token)
{
	g_free(token->id);
	g_free(token->data);
	g_free(token);
}

static void ggp_account_token_cb_call(ggp_account_token_reqdata *reqdata, ggp_account_token *token, gboolean failure)
{
	g_assert(!failure || !token); // failure => not token
	
	if (reqdata->req_processing)
	{
		ggp_purplew_request_processing_done(reqdata->req_processing);
		reqdata->req_processing = NULL;
	}
	reqdata->callback(reqdata->gc, token, reqdata->user_data);
	purple_input_remove(reqdata->inpa);
	gg_token_free(reqdata->h);
	g_free(reqdata);
	
	if (failure)
		purple_notify_error(purple_connection_get_account(reqdata->gc),
			_("Token Error"),
			_("Unable to fetch the token.\n"), NULL);
}

void ggp_account_token_request(PurpleConnection *gc, ggp_account_token_cb callback, void *user_data)
{
	struct gg_http *h;
	ggp_account_token_reqdata *reqdata;
	
	if (!ggp_deprecated_setup_proxy(gc))
	{
		callback(gc, NULL, user_data);
		return;
	}
	
	h = gg_token(1);
	
	if (!h)
	{
		callback(gc, NULL, user_data);
		return;
	}
	
	reqdata = g_new(ggp_account_token_reqdata, 1);
	reqdata->callback = callback;
	reqdata->gc = gc;
	reqdata->user_data = user_data;
	reqdata->h = h;
	reqdata->req_processing = ggp_purplew_request_processing(gc, NULL, reqdata, ggp_account_token_request_cancel);
	reqdata->inpa = purple_input_add(h->fd, PURPLE_INPUT_READ, ggp_account_token_handler, reqdata);
}

static void ggp_account_token_request_cancel(PurpleConnection *gc, void *_reqdata)
{
	ggp_account_token_reqdata *reqdata = _reqdata;

	purple_debug_info("gg", "ggp_account_token_request_cancel\n");
	reqdata->req_processing = NULL;
	gg_http_stop(reqdata->h);
	ggp_account_token_cb_call(reqdata, NULL, FALSE);
}

static void ggp_account_token_handler(gpointer _reqdata, gint fd, PurpleInputCondition cond)
{
	ggp_account_token_reqdata *reqdata = _reqdata;
	struct gg_token *token_info;
	ggp_account_token *token;
	
	//TODO: verbose mode
	purple_debug_misc("gg", "ggp_account_token_handler: got fd update [check=%d, state=%d]\n",
		reqdata->h->check, reqdata->h->state);
	
	if (gg_token_watch_fd(reqdata->h) == -1 || reqdata->h->state == GG_STATE_ERROR)
	{
		purple_debug_error("gg", "ggp_account_token_handler: failed to request token: %d\n",
			reqdata->h->error);
		ggp_account_token_cb_call(reqdata, NULL, TRUE);
		return;
	}
	
	if (reqdata->h->state != GG_STATE_DONE)
	{
		purple_input_remove(reqdata->inpa);
		reqdata->inpa = purple_input_add(reqdata->h->fd,
			(reqdata->h->check == 1) ? PURPLE_INPUT_WRITE : PURPLE_INPUT_READ,
			ggp_account_token_handler, reqdata);
		return;
	}
	
	if (!reqdata->h->data || !reqdata->h->body)
	{
		purple_debug_error("gg", "ggp_account_token_handler: failed to fetch token: %d\n",
			reqdata->h->error); // TODO: will anything be here in that case?
		ggp_account_token_cb_call(reqdata, NULL, TRUE);
		return;
	}
	
	purple_debug_info("gg", "ggp_account_token_handler: got token\n");
	
	token = g_new(ggp_account_token, 1);
	
	token_info = reqdata->h->data;
	token->id = g_strdup(token_info->tokenid);
	token->size = reqdata->h->body_size;
	token->data = g_memdup(reqdata->h->body, token->size);
	
	ggp_account_token_cb_call(reqdata, token, FALSE);
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
