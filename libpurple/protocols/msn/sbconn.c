#include "internal.h"

#include "msg.h"
#include "sbconn.h"

/* We have received the message ack */
void
msn_sbconn_msg_ack(MsnMessage *msg, void *data)
{
	MsnSlpMessage *slpmsg;
	long long real_size;

	slpmsg = data;

	real_size = (slpmsg->flags == P2P_ACK) ? 0 : slpmsg->size;

	slpmsg->offset += msg->slpmsg->header->length;

	slpmsg->msgs = g_list_remove(slpmsg->msgs, msg);

	if (slpmsg->offset < real_size)
	{
		if (slpmsg->slpcall->xfer && purple_xfer_get_status(slpmsg->slpcall->xfer) == PURPLE_XFER_STATUS_STARTED)
		{
			slpmsg->slpcall->xfer_msg = slpmsg;
			msn_message_ref(msg);
			purple_xfer_prpl_ready(slpmsg->slpcall->xfer);
		}
		else
			msn_slplink_send_msgpart(slpmsg->slplink, slpmsg);
	}
	else
	{
		/* The whole message has been sent */
		if (slpmsg->flags == P2P_MSN_OBJ_DATA ||
	        slpmsg->flags == (P2P_WML2009_COMP | P2P_MSN_OBJ_DATA) ||
	        slpmsg->flags == P2P_FILE_DATA) 
		{
			if (slpmsg->slpcall != NULL)
			{
				if (slpmsg->slpcall->cb)
					slpmsg->slpcall->cb(slpmsg->slpcall,
						NULL, 0);
			}
		}
	}

	msn_message_unref(msg);
}

/* We have received the message nak. */
void
msn_sbconn_msg_nak(MsnMessage *msg, void *data)
{
	MsnSlpMessage *slpmsg;

	slpmsg = data;

	msn_slplink_send_msgpart(slpmsg->slplink, slpmsg);

	slpmsg->msgs = g_list_remove(slpmsg->msgs, msg);
	msn_message_unref(msg);
}

void msn_sbconn_send_msg(MsnSlpLink *slplink, MsnMessage *msg)
{
	if (slplink->swboard == NULL)
	{
		slplink->swboard = msn_session_get_swboard(slplink->session,
				slplink->remote_user, MSN_SB_FLAG_FT);

		g_return_if_fail(slplink->swboard != NULL);

		/* If swboard is destroyed we will be too */
		slplink->swboard->slplinks = g_list_prepend(slplink->swboard->slplinks, slplink);
	}

	msn_switchboard_send_msg(slplink->swboard, msg, TRUE);
}

void msn_sbconn_send_part(MsnSlpLink *slplink, MsnSlpMessagePart *part)
{
	MsnMessage *msg;
	char *data;
	size_t size;

	msg = msn_message_new_msnslp();

	data = msn_slpmsgpart_serialize(part, &size);
	msg->part = part;

	msn_message_set_bin_data(msg, data, size);

	if (slplink->swboard == NULL)
	{
		slplink->swboard = msn_session_get_swboard(slplink->session,
				slplink->remote_user, MSN_SB_FLAG_FT);

		g_return_if_fail(slplink->swboard != NULL);

		/* If swboard is destroyed we will be too */
		slplink->swboard->slplinks = g_list_prepend(slplink->swboard->slplinks, slplink);
	}

	msn_switchboard_send_msg(slplink->swboard, msg, TRUE);
}
