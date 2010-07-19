#include "internal.h"

#include "p2p.h"

MsnP2PHeader *
msn_p2p_header_from_wire(MsnP2PHeader *wire)
{
	MsnP2PHeader *header;

	header = g_new(MsnP2PHeader, 1);

	header->session_id = GUINT32_FROM_LE(wire->session_id);
	header->id         = GUINT32_FROM_LE(wire->id);
	header->offset     = GUINT64_FROM_LE(wire->offset);
	header->total_size = GUINT64_FROM_LE(wire->total_size);
	header->length     = GUINT32_FROM_LE(wire->length);
	header->flags      = GUINT32_FROM_LE(wire->flags);
	header->ack_id     = GUINT32_FROM_LE(wire->ack_id);
	header->ack_sub_id = GUINT32_FROM_LE(wire->ack_sub_id);
	header->ack_size   = GUINT64_FROM_LE(wire->ack_size);

	return header;
}

MsnP2PHeader *
msn_p2p_header_to_wire(MsnP2PHeader *header)
{
	MsnP2PHeader *wire;
	
	wire = g_new(MsnP2PHeader, 1);

	wire->session_id = GUINT32_TO_LE(header->session_id);
	wire->id         = GUINT32_TO_LE(header->id);
	wire->offset     = GUINT64_TO_LE(header->offset);
	wire->total_size = GUINT64_TO_LE(header->total_size);
	wire->length     = GUINT32_TO_LE(header->length);
	wire->flags      = GUINT32_TO_LE(header->flags);
	wire->ack_id     = GUINT32_TO_LE(header->ack_id);
	wire->ack_sub_id = GUINT32_TO_LE(header->ack_sub_id);
	wire->ack_size   = GUINT64_TO_LE(header->ack_size);

	return wire;

}

MsnP2PFooter *
msn_p2p_footer_from_wire(MsnP2PFooter *wire)
{
	MsnP2PFooter *footer;

	footer = g_new(MsnP2PFooter, 1);

	footer->value = GUINT32_FROM_BE(wire->value);

	return footer;
}

MsnP2PFooter *
msn_p2p_footer_to_wire(MsnP2PFooter *footer)
{
	MsnP2PFooter *wire;

	wire = g_new(MsnP2PFooter, 1);

	wire->value = GUINT32_TO_BE(footer->value);

	return wire;
}

