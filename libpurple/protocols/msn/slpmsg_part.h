#ifndef MSN_SLPMSG_PART_H
#define MSN_SLPMSG_PART_H

#include "p2p.h"

typedef struct _MsnSlpMessagePart MsnSlpMessagePart;
typedef void (*MsnSlpPartCb)(MsnSlpMessagePart *part, void *data);

struct _MsnSlpMessagePart
{
	MsnP2PHeader *header;
	MsnP2PFooter *footer;

	MsnSlpPartCb ack_cb;
	MsnSlpPartCb nak_cb;
	void *ack_data;

	guchar *buffer;
	size_t size;
};

MsnSlpMessagePart *msn_slpmsgpart_new(MsnP2PHeader *header, MsnP2PFooter *footer);

MsnSlpMessagePart *msn_slpmsgpart_new_from_data(const char *data, size_t data_len);

void msn_slpmsgpart_destroy(MsnSlpMessagePart *part);

void msn_slpmsgpart_set_bin_data(MsnSlpMessagePart *part, const void *data, size_t len);

char *msn_slpmsgpart_serialize(MsnSlpMessagePart *part, size_t *ret_size);

void msn_slpmsgpart_ack(MsnSlpMessagePart *part, void *data);

void msn_slpmsgpart_nak(MsnSlpMessagePart *part, void *data);
#endif /* MSN_SLPMSG_PART_H */
