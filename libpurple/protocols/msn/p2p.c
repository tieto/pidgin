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
#include "debug.h"

#include "p2p.h"
#include "msnutils.h"

MsnP2PInfo *
msn_p2p_info_new(MsnP2PVersion version)
{
	MsnP2PInfo *info = g_new0(MsnP2PInfo, 1);
	info->version = version;

	switch (version) {
		case MSN_P2P_VERSION_ONE:
		case MSN_P2P_VERSION_TWO:
			/* Nothing to do */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", version);
			g_free(info);
			info = NULL;
	}

	return info;
}

MsnP2PInfo *
msn_p2p_info_dup(MsnP2PInfo *info)
{
	MsnP2PInfo *new_info = g_new0(MsnP2PInfo, 1);

	new_info->version = info->version;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
		case MSN_P2P_VERSION_TWO:
			*new_info = *info;
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
			g_free(new_info);
			new_info = NULL;
	}

	return new_info;
}

void
msn_p2p_info_free(MsnP2PInfo *info)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	g_free(info);
}

size_t
msn_p2p_header_from_wire(MsnP2PInfo *info, const char *wire, size_t max_len)
{
	size_t len;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE: {
			MsnP2PHeader *header = &info->header.v1;

			header->session_id = msn_pop32le(wire);
			header->id         = msn_pop32le(wire);
			header->offset     = msn_pop64le(wire);
			header->total_size = msn_pop64le(wire);
			header->length     = msn_pop32le(wire);
			header->flags      = msn_pop32le(wire);
			header->ack_id     = msn_pop32le(wire);
			header->ack_sub_id = msn_pop32le(wire);
			header->ack_size   = msn_pop64le(wire);

			len = P2P_PACKET_HEADER_SIZE;
			break;
		}

		case MSN_P2P_VERSION_TWO:
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return len;
}

char *
msn_p2p_header_to_wire(MsnP2PInfo *info, size_t *len)
{
	char *wire = NULL;
	char *tmp;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE: {
			MsnP2PHeader *header = &info->header.v1;
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

			break;
		}

		case MSN_P2P_VERSION_TWO:
			if (len)
				*len = 0;

			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

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
	switch (info->version) {
		case MSN_P2P_VERSION_ONE: {
			MsnP2PHeader *header = &info->header.v1;
			g_string_append_printf(str, "Session ID: %u\r\n", header->session_id);
			g_string_append_printf(str, "ID:         %u\r\n", header->id);
			g_string_append_printf(str, "Offset:     %" G_GUINT64_FORMAT "\r\n", header->offset);
			g_string_append_printf(str, "Total size: %" G_GUINT64_FORMAT "\r\n", header->total_size);
			g_string_append_printf(str, "Length:     %u\r\n", header->length);
			g_string_append_printf(str, "Flags:      0x%x\r\n", header->flags);
			g_string_append_printf(str, "ACK ID:     %u\r\n", header->ack_id);
			g_string_append_printf(str, "SUB ID:     %u\r\n", header->ack_sub_id);
			g_string_append_printf(str, "ACK Size:   %" G_GUINT64_FORMAT "\r\n", header->ack_size);

			break;
		}

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

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
	gboolean valid = FALSE;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			valid = info->header.v1.total_size >= info->header.v1.length;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return valid;
}

gboolean
msn_p2p_info_is_final(MsnP2PInfo *info)
{
	gboolean final = FALSE;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			final = info->header.v1.offset + info->header.v1.length >= info->header.v1.total_size;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return final;
}

guint32
msn_p2p_info_get_session_id(MsnP2PInfo *info)
{
	guint32 session_id = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			session_id = info->header.v1.session_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return session_id;
}

guint32
msn_p2p_info_get_id(MsnP2PInfo *info)
{
	guint32 id = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			id = info->header.v1.id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return id;
}

guint64
msn_p2p_info_get_offset(MsnP2PInfo *info)
{
	guint64 offset = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			offset = info->header.v1.offset;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return offset;
}

guint64
msn_p2p_info_get_total_size(MsnP2PInfo *info)
{
	guint64 total_size = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			total_size = info->header.v1.total_size;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return total_size;
}

guint32
msn_p2p_info_get_length(MsnP2PInfo *info)
{
	guint32 length = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			length = info->header.v1.length;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return length;
}

guint32
msn_p2p_info_get_flags(MsnP2PInfo *info)
{
	guint32 flags = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			flags = info->header.v1.flags;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return flags;
}

guint32
msn_p2p_info_get_ack_id(MsnP2PInfo *info)
{
	guint32 ack_id = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			ack_id = info->header.v1.ack_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return ack_id;
}

guint32
msn_p2p_info_get_ack_sub_id(MsnP2PInfo *info)
{
	guint32 ack_sub_id = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			ack_sub_id = info->header.v1.ack_sub_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return ack_sub_id;
}

guint64
msn_p2p_info_get_ack_size(MsnP2PInfo *info)
{
	guint64 ack_size = 0;

	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			ack_size = info->header.v1.ack_size;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

	return ack_size;
}

guint32
msn_p2p_info_get_app_id(MsnP2PInfo *info)
{
	return info->footer.value;
}

void
msn_p2p_info_set_session_id(MsnP2PInfo *info, guint32 session_id)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.session_id = session_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

}

void
msn_p2p_info_set_id(MsnP2PInfo *info, guint32 id)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.id = id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}

}

void
msn_p2p_info_set_offset(MsnP2PInfo *info, guint64 offset)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.offset = offset;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_total_size(MsnP2PInfo *info, guint64 total_size)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.total_size = total_size;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_length(MsnP2PInfo *info, guint32 length)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.length = length;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_flags(MsnP2PInfo *info, guint32 flags)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.flags = flags;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_ack_id(MsnP2PInfo *info, guint32 ack_id)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.ack_id = ack_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_ack_sub_id(MsnP2PInfo *info, guint32 ack_sub_id)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.ack_sub_id = ack_sub_id;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_ack_size(MsnP2PInfo *info, guint64 ack_size)
{
	switch (info->version) {
		case MSN_P2P_VERSION_ONE:
			info->header.v1.ack_size = ack_size;
			break;

		case MSN_P2P_VERSION_TWO:
			/* Nothing to do! */
			break;

		default:
			purple_debug_error("msn", "Invalid P2P Info version: %d\n", info->version);
	}
}

void
msn_p2p_info_set_app_id(MsnP2PInfo *info, guint32 app_id)
{
	info->footer.value = app_id;
}

