/**
 * @file p2p.c MSN P2P functions
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"

#include "p2p.h"
#include "msnutils.h"

MsnP2PHeader *
msn_p2p_header_from_wire(const char *wire)
{
	MsnP2PHeader *header;

	header = g_new(MsnP2PHeader, 1);

	header->session_id = msn_pop32le(wire);
	header->id         = msn_pop32le(wire);
	header->offset     = msn_pop64le(wire);
	header->total_size = msn_pop64le(wire);
	header->length     = msn_pop32le(wire);
	header->flags      = msn_pop32le(wire);
	header->ack_id     = msn_pop32le(wire);
	header->ack_sub_id = msn_pop32le(wire);
	header->ack_size   = msn_pop64le(wire);

	return header;
}

char *
msn_p2p_header_to_wire(MsnP2PHeader *header)
{
	char *wire;
	char *tmp;
	
	tmp = wire = g_new(char, P2P_PACKET_HEADER_SIZE);

	msn_push32le(tmp, header->session_id);
	msn_push32le(tmp, header->id);
	msn_push64le(tmp, header->offset);
	msn_push64le(tmp, header->total_size);
	msn_push32le(tmp, header->length);
	msn_push32le(tmp, header->flags);
	msn_push32le(tmp, header->ack_id);
	msn_push32le(tmp, header->ack_sub_id);
	msn_push64le(tmp, header->ack_size);

	return wire;

}

MsnP2PFooter *
msn_p2p_footer_from_wire(const char *wire)
{
	MsnP2PFooter *footer;

	footer = g_new(MsnP2PFooter, 1);

	footer->value = msn_pop32be(wire);

	return footer;
}

char *
msn_p2p_footer_to_wire(MsnP2PFooter *footer)
{
	char *wire;
	char *tmp;

	tmp = wire = g_new(char, P2P_PACKET_FOOTER_SIZE);

	msn_push32be(tmp, footer->value);

	return wire;
}

gboolean
msn_p2p_msg_is_data(const MsnP2PHeaderFlag flags)
{
	return (flags == P2P_MSN_OBJ_DATA ||
	        flags == (P2P_WLM2009_COMP | P2P_MSN_OBJ_DATA) ||
	        flags == P2P_FILE_DATA);
}

