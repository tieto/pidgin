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

