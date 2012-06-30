#include "purplew.h"

#include <request.h>

guint ggp_purplew_http_input_add(struct gg_http *http_req,
	PurpleInputFunction func, gpointer user_data)
{
	PurpleInputCondition cond = 0;
	int check = http_req->check;

	if (check & GG_CHECK_READ)
		cond |= PURPLE_INPUT_READ;
	if (check & GG_CHECK_WRITE)
		cond |= PURPLE_INPUT_WRITE;

	return purple_input_add(http_req->fd, cond, func, user_data);
}

static void ggp_purplew_request_processing_cancel(
	ggp_purplew_request_processing_handle *handle, gint id)
{
	handle->cancel_cb(handle->gc, handle->user_data);
	g_free(handle);
}

ggp_purplew_request_processing_handle * ggp_purplew_request_processing(
	PurpleConnection *gc, const gchar *msg, void *user_data,
	ggp_purplew_request_processing_cancel_cb cancel_cb)
{
	ggp_purplew_request_processing_handle *handle =
		g_new(ggp_purplew_request_processing_handle, 1);

	handle->gc = gc;
	handle->cancel_cb = cancel_cb;
	handle->user_data = user_data;
	handle->request_handle = purple_request_action(gc, _("Please wait..."),
		(msg ? msg : _("Please wait...")), NULL,
		PURPLE_DEFAULT_ACTION_NONE, purple_connection_get_account(gc),
		NULL, NULL, handle, 1,
		_("Cancel"), G_CALLBACK(ggp_purplew_request_processing_cancel));
	
	return handle;
}

void ggp_purplew_request_processing_done(
	ggp_purplew_request_processing_handle *handle)
{
	purple_request_close(PURPLE_REQUEST_ACTION, handle->request_handle);
	g_free(handle);
}
