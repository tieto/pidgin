#include "account.h"

#include <libgadu.h>
#include <debug.h>

#include "deprecated.h"

typedef struct
{
	ggp_account_token_cb callback;
	void *user_data;
	
	struct gg_http *h;
	guint inpa;
} ggp_account_token_reqdata;

static void ggp_account_token_handler(gpointer _reqdata, gint fd, PurpleInputCondition cond);

static void ggp_account_token_cb_call(ggp_account_token_reqdata *reqdata, ggp_account_token *token);

/***********************/

void ggp_account_token_free(ggp_account_token *token)
{
	g_free(token->id);
	g_free(token->data);
	g_free(token);
}

static void ggp_account_token_cb_call(ggp_account_token_reqdata *reqdata, ggp_account_token *token)
{
	reqdata->callback(token, reqdata->user_data);
	purple_input_remove(reqdata->inpa);
	gg_token_free(reqdata->h);
	g_free(reqdata);
}

void ggp_account_token_request(PurpleConnection *gc, ggp_account_token_cb callback, void *user_data)
{
	struct gg_http *h;
	ggp_account_token_reqdata *reqdata;
	
	if (!ggp_deprecated_setup_proxy(gc))
	{
		callback(NULL, user_data);
		return;
	}
	
	h = gg_token(1);
	
	if (!h)
	{
		callback(NULL, user_data);
		return;
	}
	
	reqdata = g_new(ggp_account_token_reqdata, 1);
	reqdata->callback = callback;
	reqdata->user_data = user_data;
	reqdata->h = h;
	
	reqdata->inpa = purple_input_add(h->fd, PURPLE_INPUT_READ, ggp_account_token_handler, reqdata);
}

static void ggp_account_token_handler(gpointer _reqdata, gint fd, PurpleInputCondition cond)
{
	ggp_account_token_reqdata *reqdata = _reqdata;
	struct gg_token *token_info;
	ggp_account_token *token;
	
	purple_debug_info("gg", "ggp_account_token_handler: got fd update [check=%d, state=%d]\n",
		reqdata->h->check, reqdata->h->state);
	
	if (reqdata->h->state == GG_STATE_ERROR || gg_token_watch_fd(reqdata->h) == -1)
	{
		purple_debug_error("gg", "ggp_account_token_handler: failed to request token: %d\n",
			reqdata->h->error);
		ggp_account_token_cb_call(reqdata, NULL);
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
		ggp_account_token_cb_call(reqdata, NULL);
		return;
	}
	
	
	token = g_new(ggp_account_token, 1);
	
	token_info = reqdata->h->data;
	token->id = g_strdup(token_info->tokenid);
	token->size = reqdata->h->body_size;
	token->data = g_memdup(reqdata->h->body, token->size);
	
	ggp_account_token_cb_call(reqdata, token);
}
