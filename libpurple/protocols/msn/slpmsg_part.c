#include "internal.h"

#include "slpmsg.h"
#include "slpmsg_part.h"

MsnSlpMessagePart *msn_slpmsgpart_new(MsnP2PHeader *header, MsnP2PFooter *footer)
{
	MsnSlpMessagePart *part;

	part = g_new0(MsnSlpMessagePart, 1);

	if (header)
		part->header = g_memdup(header, P2P_PACKET_HEADER_SIZE);
	if (footer)
		part->footer = g_memdup(footer, P2P_PACKET_FOOTER_SIZE);

	part->ack_cb = msn_slpmsgpart_ack;
	part->nak_cb = msn_slpmsgpart_nak;

	return part;
}

MsnSlpMessagePart *msn_slpmsgpart_new_from_data(const char *data, size_t data_len)
{
	MsnSlpMessagePart *part;
	MsnP2PHeader *header;
	const char *tmp;
	int body_len;

	tmp = data;
	part = msn_slpmsgpart_new(NULL, NULL);

	if (data_len < sizeof(*header)) {
		return NULL;
	}

	/* Extract the binary SLP header */
	part->header = msn_p2p_header_from_wire((MsnP2PHeader*)tmp);

	/* Extract the body */
	body_len = data_len - (tmp - data);
	/* msg->body_len = msg->msnslp_header.length; */

	if (body_len > 0) {
		part->size = body_len;
		part->buffer = g_malloc(body_len);
		memcpy(part->buffer, tmp, body_len);
		tmp += body_len;
	}

	/* Extract the footer */
	if (body_len >= 0) 
		part->footer = msn_p2p_footer_from_wire((MsnP2PFooter*)tmp);

	return part;
}

void msn_slpmsgpart_destroy(MsnSlpMessagePart *part)
{
	g_free(part->header);
	g_free(part->footer);

	g_free(part);

}

void msn_slpmsgpart_set_bin_data(MsnSlpMessagePart *part, const void *data, size_t len)
{
	g_return_if_fail(part != NULL);

	if (part->buffer != NULL)
		g_free(part->buffer);

	if (data != NULL && len > 0) {
		part->buffer = g_malloc(len + 1);
		memcpy(part->buffer, data, len);
		part->buffer[len] = '\0';
		part->size = len;
	} else {
		part->buffer = NULL;
		part->size = 0;
	}

}

char *msn_slpmsgpart_serialize(MsnSlpMessagePart *part, size_t *ret_size)
{
	MsnP2PHeader *header;
	MsnP2PFooter *footer;
	char *base;
	char *tmp;
	size_t siz;

	base = g_malloc(P2P_PACKET_HEADER_SIZE + part->size + sizeof(MsnP2PFooter));
	tmp = base;

	header = msn_p2p_header_to_wire(part->header);
	footer = msn_p2p_footer_to_wire(part->footer);

	siz = sizeof(MsnP2PHeader);
	/* Copy header */
	memcpy(tmp, (char*)header, siz);
	tmp += siz;

	/* Copy body */
	memcpy(tmp, part->buffer, part->size);
	tmp += part->size;

	/* Copy footer */
	siz = sizeof(MsnP2PFooter);
	memcpy(tmp, (char*)footer, siz);
	tmp += siz;

	*ret_size = tmp - base;

	return base;
}
/* We have received the message ack */
void
msn_slpmsgpart_ack(MsnSlpMessagePart *part, void *data)
{
	MsnSlpMessage *slpmsg;
	long long real_size;

	slpmsg = data;

	real_size = (slpmsg->flags == P2P_ACK) ? 0 : slpmsg->size;

	slpmsg->offset += part->header->length;

	slpmsg->parts = g_list_remove(slpmsg->parts, part);

	if (slpmsg->offset < real_size)
	{
		if (slpmsg->slpcall->xfer && purple_xfer_get_status(slpmsg->slpcall->xfer) == PURPLE_XFER_STATUS_STARTED)
		{
			slpmsg->slpcall->xfer_msg = slpmsg;
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
}

/* We have received the message nak. */
void
msn_slpmsgpart_nak(MsnSlpMessagePart *part, void *data)
{
	MsnSlpMessage *slpmsg;

	slpmsg = data;

	msn_slplink_send_msgpart(slpmsg->slplink, slpmsg);

	slpmsg->parts = g_list_remove(slpmsg->parts, part);
}
