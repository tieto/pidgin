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

MsnP2PInfo *
msn_p2p_info_new(void)
{
	return g_new0(MsnP2PInfo, 1);
}

MsnP2PInfo *
msn_p2p_info_dup(MsnP2PInfo *info)
{
	MsnP2PInfo *new_info = g_new0(MsnP2PInfo, 1);
	*new_info = *info;
	return new_info;
}

void
msn_p2p_info_free(MsnP2PInfo *info)
{
	g_free(info);
}

size_t
msn_p2p_header_from_wire(MsnP2PInfo *info, const char *wire)
{
	MsnP2PHeader *header;

	header = &info->header;

	header->session_id = msn_pop32le(wire);
	header->id         = msn_pop32le(wire);
	header->offset     = msn_pop64le(wire);
	header->total_size = msn_pop64le(wire);
	header->length     = msn_pop32le(wire);
	header->flags      = msn_pop32le(wire);
	header->ack_id     = msn_pop32le(wire);
	header->ack_sub_id = msn_pop32le(wire);
	header->ack_size   = msn_pop64le(wire);

	return P2P_PACKET_HEADER_SIZE;
}

char *
msn_p2p_header_to_wire(MsnP2PInfo *info, size_t *len)
{
	MsnP2PHeader *header;
	char *wire;
	char *tmp;

	header = &info->header;
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

	if (len)
		*len = P2P_PACKET_HEADER_SIZE;

	return wire;

}

size_t
msn_p2p_footer_from_wire(MsnP2PInfo *info, const char *wire)
{
	MsnP2PFooter *footer;

	footer = &info->footer;

	footer->value = msn_pop32be(wire);

	return P2P_PACKET_FOOTER_SIZE;
}

char *
msn_p2p_footer_to_wire(MsnP2PInfo *info, size_t *len)
{
	MsnP2PFooter *footer;
	char *wire;
	char *tmp;

	footer = &info->footer;
	tmp = wire = g_new(char, P2P_PACKET_FOOTER_SIZE);

	msn_push32be(tmp, footer->value);

	if (len)
		*len = P2P_PACKET_FOOTER_SIZE;

	return wire;
}

void
msn_p2p_info_to_string(MsnP2PInfo *info, GString *str)
{
	g_string_append_printf(str, "Session ID: %u\r\n", info->header.session_id);
	g_string_append_printf(str, "ID:         %u\r\n", info->header.id);
	g_string_append_printf(str, "Offset:     %" G_GUINT64_FORMAT "\r\n", info->header.offset);
	g_string_append_printf(str, "Total size: %" G_GUINT64_FORMAT "\r\n", info->header.total_size);
	g_string_append_printf(str, "Length:     %u\r\n", info->header.length);
	g_string_append_printf(str, "Flags:      0x%x\r\n", info->header.flags);
	g_string_append_printf(str, "ACK ID:     %u\r\n", info->header.ack_id);
	g_string_append_printf(str, "SUB ID:     %u\r\n", info->header.ack_sub_id);
	g_string_append_printf(str, "ACK Size:   %" G_GUINT64_FORMAT "\r\n", info->header.ack_size);
	g_string_append_printf(str, "Footer:     0x%08X\r\n", info->footer.value);
}

gboolean
msn_p2p_msg_is_data(const MsnP2PHeaderFlag flags)
{
	return (flags == P2P_MSN_OBJ_DATA ||
	        flags == (P2P_WLM2009_COMP | P2P_MSN_OBJ_DATA) ||
	        flags == P2P_FILE_DATA);
}

gboolean
msn_p2p_info_is_valid(MsnP2PInfo *info)
{
	return info->header.total_size >= info->header.length;
}

gboolean
msn_p2p_info_is_final(MsnP2PInfo *info)
{
	return info->header.offset + info->header.length >= info->header.total_size;
}

guint32
msn_p2p_info_get_session_id(MsnP2PInfo *info)
{
	return info->header.session_id;
}

guint32
msn_p2p_info_get_id(MsnP2PInfo *info)
{
	return info->header.id;
}

guint64
msn_p2p_info_get_offset(MsnP2PInfo *info)
{
	return info->header.offset;
}

guint64
msn_p2p_info_get_total_size(MsnP2PInfo *info)
{
	return info->header.total_size;
}

guint32
msn_p2p_info_get_length(MsnP2PInfo *info)
{
	return info->header.length;
}

guint32
msn_p2p_info_get_flags(MsnP2PInfo *info)
{
	return info->header.flags;
}

guint32
msn_p2p_info_get_ack_id(MsnP2PInfo *info)
{
	return info->header.ack_id;
}

guint32
msn_p2p_info_get_ack_sub_id(MsnP2PInfo *info)
{
	return info->header.ack_sub_id;
}

guint64
msn_p2p_info_get_ack_size(MsnP2PInfo *info)
{
	return info->header.ack_size;
}

guint32
msn_p2p_info_get_app_id(MsnP2PInfo *info)
{
	return info->footer.value;
}

void
msn_p2p_info_set_session_id(MsnP2PInfo *info, guint32 session_id)
{
	info->header.session_id = session_id;
}

void
msn_p2p_info_set_id(MsnP2PInfo *info, guint32 id)
{
	info->header.id = id;
}

void
msn_p2p_info_set_offset(MsnP2PInfo *info, guint64 offset)
{
	info->header.offset = offset;
}

void
msn_p2p_info_set_total_size(MsnP2PInfo *info, guint64 total_size)
{
	info->header.total_size = total_size;
}

void
msn_p2p_info_set_length(MsnP2PInfo *info, guint32 length)
{
	info->header.length = length;
}

void
msn_p2p_info_set_flags(MsnP2PInfo *info, guint32 flags)
{
	info->header.flags = flags;
}

void
msn_p2p_info_set_ack_id(MsnP2PInfo *info, guint32 ack_id)
{
	info->header.ack_id = ack_id;
}

void
msn_p2p_info_set_ack_sub_id(MsnP2PInfo *info, guint32 ack_sub_id)
{
	info->header.ack_sub_id = ack_sub_id;
}

void
msn_p2p_info_set_ack_size(MsnP2PInfo *info, guint64 ack_size)
{
	info->header.ack_size = ack_size;
}

void
msn_p2p_info_set_app_id(MsnP2PInfo *info, guint32 app_id)
{
	info->footer.value = app_id;
}

