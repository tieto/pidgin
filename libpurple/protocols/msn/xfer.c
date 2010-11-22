#include "internal.h"
#include "debug.h"

#include "sbconn.h"
#include "xfer.h"

/**************************************************************************
 * Xfer
 **************************************************************************/

void
msn_xfer_init(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	/* MsnSlpLink *slplink; */
	char *content;

	purple_debug_info("msn", "xfer_init\n");

	slpcall = xfer->data;

	/* Send Ok */
	content = g_strdup_printf("SessionID: %lu\r\n\r\n",
							  slpcall->session_id);

	msn_slp_send_ok(slpcall, slpcall->branch, "application/x-msnmsgr-sessionreqbody",
			content);

	g_free(content);
	msn_slplink_send_queued_slpmsgs(slpcall->slplink);
}

void
msn_xfer_cancel(PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	char *content;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->data != NULL);

	slpcall = xfer->data;

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
	{
		if (slpcall->started)
		{
			msn_slpcall_close(slpcall);
		}
		else
		{
			content = g_strdup_printf("SessionID: %lu\r\n\r\n",
									slpcall->session_id);

			msn_slp_send_decline(slpcall, slpcall->branch, "application/x-msnmsgr-sessionreqbody",
						content);

			g_free(content);
			msn_slplink_send_queued_slpmsgs(slpcall->slplink);

			if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)
				slpcall->wasted = TRUE;
			else
				msn_slpcall_destroy(slpcall);
		}
	}
}

gssize
msn_xfer_write(const guchar *data, gsize len, PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;

	g_return_val_if_fail(xfer != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(len > 0, -1);

	g_return_val_if_fail(purple_xfer_get_type(xfer) == PURPLE_XFER_SEND, -1);

	slpcall = xfer->data;
	/* Not sure I trust it'll be there */
	g_return_val_if_fail(slpcall != NULL, -1);

	g_return_val_if_fail(slpcall->xfer_msg != NULL, -1);

	slpcall->u.outgoing.len = len;
	slpcall->u.outgoing.data = data;
	msn_slplink_send_msgpart(slpcall->slplink, slpcall->xfer_msg);

	return MIN(MSN_SBCONN_MAX_SIZE, len);
}

gssize
msn_xfer_read(guchar **data, PurpleXfer *xfer)
{
	MsnSlpCall *slpcall;
	gsize len;

	g_return_val_if_fail(xfer != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);

	g_return_val_if_fail(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE, -1);

	slpcall = xfer->data;
	/* Not sure I trust it'll be there */
	g_return_val_if_fail(slpcall != NULL, -1);

	/* Just pass up the whole GByteArray. We'll make another. */
	*data = slpcall->u.incoming_data->data;
	len = slpcall->u.incoming_data->len;

	g_byte_array_free(slpcall->u.incoming_data, FALSE);
	slpcall->u.incoming_data = g_byte_array_new();

	return len;
}

void
msn_xfer_end_cb(MsnSlpCall *slpcall, MsnSession *session)
{
	if ((purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_DONE) &&
		(purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_CANCEL_REMOTE) &&
		(purple_xfer_get_status(slpcall->xfer) != PURPLE_XFER_STATUS_CANCEL_LOCAL))
	{
		purple_xfer_cancel_remote(slpcall->xfer);
	}
}

void
msn_xfer_completed_cb(MsnSlpCall *slpcall, const guchar *body,
					  gsize size)
{
	PurpleXfer *xfer = slpcall->xfer;

	purple_xfer_set_completed(xfer, TRUE);
	purple_xfer_end(xfer);
}

