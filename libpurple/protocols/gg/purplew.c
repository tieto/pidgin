#include "purplew.h"

#include <request.h>
#include <debug.h>

guint ggp_purplew_http_input_add(struct gg_http *http_req,
	PurpleInputFunction func, gpointer user_data)
{
	PurpleInputCondition cond = 0;
	int check = http_req->check;

	if (check & GG_CHECK_READ)
		cond |= PURPLE_INPUT_READ;
	if (check & GG_CHECK_WRITE)
		cond |= PURPLE_INPUT_WRITE;

	//TODO: verbose mode
	//purple_debug_misc("gg", "ggp_purplew_http_input_add: "
	//	"[req=%x, fd=%d, cond=%d]\n",
	//	(unsigned int)http_req, http_req->fd, cond);
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

PurpleGroup * ggp_purplew_buddy_get_group_only(PurpleBuddy *buddy)
{
	PurpleGroup *group = purple_buddy_get_group(buddy);
	if (!group) // TODO: get contact's group
		return NULL;
	if (0 == strcmp(_("Buddies"), purple_group_get_name(group)))
		return NULL;
	return group;
}
