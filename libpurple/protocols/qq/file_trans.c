/**
 * @file file_trans.c
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
#include "ft.h"
#include "cipher.h"

#include "qq_crypt.h"
#include "file_trans.h"
#include "qq_define.h"
#include "im.h"
#include "packet_parse.h"
#include "proxy.h"
#include "qq_network.h"
#include "send_file.h"
#include "utils.h"

typedef struct _qq_file_header {
	guint16 client_ver;
	guint8 file_key;
	UID sender_uid;
	UID receiver_uid;
} qq_file_header;

static guint32 _get_file_key(guint8 seed)
{
	guint32 key;
	key = seed | (seed << 8) | (seed << 16) | (seed << 24);
	return key;
}

static guint32 _gen_file_key(void)
{
	guint8 seed;

	seed = rand() & 0xFF;
	return _get_file_key(seed);
}

static guint32 _decrypt_qq_uid(UID uid, guint32 key)
{
	return ~(uid ^ key);
}

static guint32 _encrypt_qq_uid(UID uid, guint32 key)
{
	return (~uid) ^ key;
}

static void _fill_file_md5(const gchar *filename, gint filelen, guint8 *md5)
{
	FILE *fp;
	guint8 *buffer;
	size_t wc;

	const gint QQ_MAX_FILE_MD5_LENGTH = 10002432;

	g_return_if_fail(filename != NULL && md5 != NULL);
	if (filelen > QQ_MAX_FILE_MD5_LENGTH)
		filelen = QQ_MAX_FILE_MD5_LENGTH;

	fp = g_fopen(filename, "rb");
	g_return_if_fail(fp != NULL);

	buffer = g_newa(guint8, filelen);
	g_return_if_fail(buffer != NULL);
	wc = fread(buffer, filelen, 1, fp);
	fclose(fp);
	if (wc != 1) {
		purple_debug_error("qq", "Unable to read file: %s\n", filename);

		/* TODO: XXX: Really, the caller should be modified to deal with this properly. */
		return;
	}

	qq_get_md5(md5, QQ_KEY_LENGTH, buffer, filelen);
}

static gint _qq_get_file_header(qq_file_header *fh, guint8 *buf)
{
	gint bytes = 0;
	bytes += qq_get16(&(fh->client_ver), buf + bytes);
	bytes += qq_get8(&fh->file_key, buf + bytes);
	bytes += qq_get32(&(fh->sender_uid), buf + bytes);
	bytes += qq_get32(&(fh->receiver_uid), buf + bytes);

	fh->sender_uid = _decrypt_qq_uid(fh->sender_uid, _get_file_key(fh->file_key));
	fh->receiver_uid = _decrypt_qq_uid(fh->receiver_uid, _get_file_key(fh->file_key));
	return bytes;
}

static const gchar *qq_get_file_cmd_desc(gint type)
{
	switch (type) {
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
			return "QQ_FILE_CMD_SENDER_SAY_HELLO";
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
			return "QQ_FILE_CMD_SENDER_SAY_HELLO_ACK";
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
			return "QQ_FILE_CMD_RECEIVER_SAY_HELLO";
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
			return "QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK";
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
			return "QQ_FILE_CMD_NOTIFY_IP_ACK";
		case QQ_FILE_CMD_PING:
			return "QQ_FILE_CMD_PING";
		case QQ_FILE_CMD_PONG:
			return "QQ_FILE_CMD_PONG";
		case QQ_FILE_CMD_INITATIVE_CONNECT:
			return "QQ_FILE_CMD_INITATIVE_CONNECT";
		case QQ_FILE_CMD_FILE_OP:
			return "QQ_FILE_CMD_FILE_OP";
		case QQ_FILE_CMD_FILE_OP_ACK:
			return "QQ_FILE_CMD_FILE_OP_ACK";
		case QQ_FILE_BASIC_INFO:
			return "QQ_FILE_BASIC_INFO";
		case QQ_FILE_DATA_INFO:
			return "QQ_FILE_DATA_INFO";
		case QQ_FILE_EOF:
			return "QQ_FILE_EOF";
		default:
			return "UNKNOWN_TYPE";
	}
}

/* The memmap version has better performance for big files transfering
 * but it will spend plenty of memory, so do not use it in a low-memory host */
#ifdef USE_MMAP
#include <sys/mman.h>

static int _qq_xfer_open_file(const gchar *filename, const gchar *method, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;
	int fd;
	if (method[0] == 'r') {
		fd = open(purple_xfer_get_local_filename(xfer), O_RDONLY);
		info->buffer = mmap(0, purple_xfer_get_size(xfer), PROT_READ, MAP_PRIVATE, fd, 0);
	}
	else
	{
		fd = open(purple_xfer_get_local_filename(xfer), O_RDWR|O_CREAT, 0644);
		info->buffer = mmap(0, purple_xfer_get_size(xfer), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	}

	if (info->buffer == NULL) {
		return - 1;
	}
	return 0;
}

static gint _qq_xfer_read_file(guint8 *buffer, guint index, guint len, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;
	gint readbytes;

	buffer = info->buffer + len * index;
	readbytes = purple_xfer_get_size(xfer) - (buffer - info->buffer);
	if (readbytes > info->fragment_len) readbytes = info->fragment_len;
	return readbytes;
}

static gint _qq_xfer_write_file(guint8 *buffer, guint index, guint len, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;

	memcpy(info->buffer + index * len, buffer, len);
	return 0;
}

void qq_xfer_close_file(PurpleXfer *xfer)
{
	ft_info *info = xfer->data;

	if (info->buffer) munmap(info->buffer, purple_xfer_get_size(xfer));
}
#else
static int _qq_xfer_open_file(const gchar *filename, const gchar *method, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;
	info->dest_fp = g_fopen(purple_xfer_get_local_filename(xfer), method);
	if (info->dest_fp == NULL) {
		return -1;
	}
	return 0;
}

static gint _qq_xfer_read_file(guint8 *buffer, guint index, guint len, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;

	fseek(info->dest_fp, index * len, SEEK_SET);
	return fread(buffer, 1, len, info->dest_fp);
}

static gint _qq_xfer_write_file(guint8 *buffer, guint index, guint len, PurpleXfer *xfer)
{
	ft_info *info = xfer->data;
	fseek(info->dest_fp, index * len, SEEK_SET);
	return fwrite(buffer, 1, len, info->dest_fp);
}

void qq_xfer_close_file(PurpleXfer *xfer)
{
	ft_info *info = xfer->data;

	if (info->dest_fp) fclose(info->dest_fp);
}
#endif

static gint _qq_send_file(PurpleConnection *gc, guint8 *data, gint len, guint16 packet_type, UID to_uid)
{
	guint8 *raw_data;
	gint bytes = 0;
	guint32 file_key;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;

	raw_data = g_newa(guint8, MAX_PACKET_SIZE);
	file_key = _gen_file_key();

	bytes += qq_put8(raw_data + bytes, packet_type);
	bytes += qq_put16(raw_data + bytes, qd->client_tag);
	bytes += qq_put8(raw_data + bytes, file_key & 0xff);
	bytes += qq_put32(raw_data + bytes, _encrypt_qq_uid(qd->uid, file_key));
	bytes += qq_put32(raw_data + bytes, _encrypt_qq_uid(to_uid, file_key));
	bytes += qq_putdata(raw_data + bytes, data, len);

	if (bytes == len + 12) {
		_qq_xfer_write(raw_data, bytes, qd->xfer);
	} else
		purple_debug_info("QQ", "send_file: want %d but got %d\n", len + 12, bytes);
	return bytes;
}

/* send a file to udp channel with QQ_FILE_CONTROL_PACKET_TAG */
void qq_send_file_ctl_packet(PurpleConnection *gc, guint16 packet_type, UID to_uid, guint8 hellobyte)
{
	qq_data *qd;
	gint bytes, bytes_expected, encrypted_len;
	guint8 *raw_data, *encrypted;
	time_t now;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	raw_data = g_newa (guint8, 61);
	bytes = 0;

	now = time(NULL);

	bytes += qq_putdata(raw_data + bytes, qd->session_md5, 16);
	bytes += qq_put16(raw_data + bytes, packet_type);
	switch (packet_type) {
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
			bytes += qq_put16(raw_data + bytes, info->send_seq);
			break;
		default:
			bytes += qq_put16(raw_data + bytes, ++qd->send_seq);
	}
	bytes += qq_put32(raw_data + bytes, (guint32) now);
	bytes += qq_put8(raw_data + bytes, 0x00);
	bytes += qq_put8(raw_data + bytes, qd->my_icon);
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put16(raw_data + bytes, 0x0000);
	bytes += qq_put8(raw_data + bytes, 0x00);
	/* 0x65: send a file, 0x6b: send a custom face */
	bytes += qq_put8(raw_data + bytes, QQ_FILE_TRANSFER_FILE); /* FIXME temp by gfhuang */
	switch (packet_type)
	{
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
			bytes += qq_put8(raw_data + bytes, 0x00);
			bytes += qq_put8(raw_data + bytes, hellobyte);
			bytes_expected = 48;
			break;
		case QQ_FILE_CMD_PING:
		case QQ_FILE_CMD_PONG:
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
			bytes += qq_fill_conn_info(raw_data, info);
			bytes_expected = 61;
			break;
		default:
			purple_debug_info("QQ", "qq_send_file_ctl_packet: Unknown packet type[%d]\n",
					packet_type);
			bytes_expected = 0;
	}

	if (bytes != bytes_expected) {
		purple_debug_error("QQ", "qq_send_file_ctl_packet: Expected to get %d bytes, but get %d\n",
				bytes_expected, bytes);
		return;
	}

	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ",
		raw_data, bytes,
		"sending packet[%s]:", qq_get_file_cmd_desc(packet_type));

	encrypted = g_newa(guint8, bytes + 17);
	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, info->file_session_key);
	/*debug: try to decrypt it */

#if 0
	guint8 *buf;
	int buflen;
	hex_dump = hex_dump_to_str(encrypted, encrypted_len);
	purple_debug_info("QQ", "encrypted packet: \n%s\n", hex_dump);
	g_free(hex_dump);
	buf = g_newa(guint8, MAX_PACKET_SIZE);
	buflen = encrypted_len;
	if (qq_crypt(DECRYPT, encrypted, encrypted_len, info->file_session_key, buf, &buflen)) {
		purple_debug_info("QQ", "decrypt success\n");
	   if (buflen == bytes && memcmp(raw_data, buf, buflen) == 0)
			purple_debug_info("QQ", "checksum ok\n");

		hex_dump = hex_dump_to_str(buf, buflen);
		purple_debug_info("QQ", "decrypted packet: \n%s\n", hex_dump);
		g_free(hex_dump);
	 } else {
		purple_debug_info("QQ", "decrypt fail\n");
	}
#endif

	purple_debug_info("QQ", "<== send %s packet\n", qq_get_file_cmd_desc(packet_type));
	_qq_send_file(gc, encrypted, encrypted_len, QQ_FILE_CONTROL_PACKET_TAG, info->to_uid);
}

/* send a file to udp channel with QQ_FILE_DATA_PACKET_TAG */
static void _qq_send_file_data_packet(PurpleConnection *gc, guint16 packet_type, guint8 sub_type,
		guint32 fragment_index, guint16 seq, guint8 *data, gint len)
{
	guint8 *raw_data, filename_md5[QQ_KEY_LENGTH], file_md5[QQ_KEY_LENGTH];
	gint bytes;
	guint32 fragment_size = 1000;
	const char *filename;
	gint filename_len, filesize;
	qq_data *qd;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	filename = purple_xfer_get_filename(qd->xfer);
	filesize = purple_xfer_get_size(qd->xfer);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE);
	bytes = 0;

	bytes += qq_put8(raw_data + bytes, 0x00);
	bytes += qq_put16(raw_data + bytes, packet_type);
	switch (packet_type) {
		case QQ_FILE_BASIC_INFO:
		case QQ_FILE_DATA_INFO:
		case QQ_FILE_EOF:
			bytes += qq_put16(raw_data + bytes, 0x0000);
			bytes += qq_put8(raw_data + bytes, 0x00);
			break;
		case QQ_FILE_CMD_FILE_OP:
			switch(sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					filename_len = strlen(filename);
					qq_get_md5(filename_md5, sizeof(filename_md5), (guint8 *)filename, filename_len);
					_fill_file_md5(purple_xfer_get_local_filename(qd->xfer),
							purple_xfer_get_size(qd->xfer),
							file_md5);

					info->fragment_num = (filesize - 1) / QQ_FILE_FRAGMENT_MAXLEN + 1;
					info->fragment_len = QQ_FILE_FRAGMENT_MAXLEN;

					purple_debug_info("QQ",
							"start transfering data, %d fragments with %d length each\n",
							info->fragment_num, info->fragment_len);
					/* Unknown */
					bytes += qq_put16(raw_data  + bytes, 0x0000);
					/* Sub-operation type */
					bytes += qq_put8(raw_data + bytes, sub_type);
					/* Length of file */
					bytes += qq_put32(raw_data + bytes, filesize);
					/* Number of fragments */
					bytes += qq_put32(raw_data + bytes, info->fragment_num);
					/* Length of a single fragment */
					bytes += qq_put32(raw_data + bytes, info->fragment_len);
					bytes += qq_putdata(raw_data + bytes, file_md5, 16);
					bytes += qq_putdata(raw_data + bytes, filename_md5, 16);
					/* Length of filename */
					bytes += qq_put16(raw_data + bytes, filename_len);
					/* 8 unknown bytes */
					bytes += qq_put32(raw_data + bytes, 0x00000000);
					bytes += qq_put32(raw_data + bytes, 0x00000000);
					/* filename */
					bytes += qq_putdata(raw_data + bytes, (guint8 *) filename,
							filename_len);
					break;
				case QQ_FILE_DATA_INFO:
					purple_debug_info("QQ",
							"sending %dth fragment with length %d, offset %d\n",
							fragment_index, len, (fragment_index-1)*fragment_size);
					/* bytes += qq_put16(raw_data + bytes, ++(qd->send_seq)); */
					bytes += qq_put16(raw_data + bytes, info->send_seq);
					bytes += qq_put8(raw_data + bytes, sub_type);
					/* bytes += qq_put32(raw_data + bytes, fragment_index); */
					bytes += qq_put32(raw_data + bytes, fragment_index - 1);
					bytes += qq_put32(raw_data + bytes, (fragment_index - 1) * fragment_size);
					bytes += qq_put16(raw_data + bytes, len);
					bytes += qq_putdata(raw_data + bytes, data, len);
					break;
				case QQ_FILE_EOF:
					purple_debug_info("QQ", "end of sending data\n");
					/* bytes += qq_put16(raw_data + bytes, info->fragment_num + 1); */
					bytes += qq_put16(raw_data + bytes, info->fragment_num);
					bytes += qq_put8(raw_data + bytes, sub_type);
					/* purple_xfer_set_completed(qd->xfer, TRUE); */
			}
			break;
		case QQ_FILE_CMD_FILE_OP_ACK:
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					bytes += qq_put16(raw_data + bytes, 0x0000);
					bytes += qq_put8(raw_data + bytes, sub_type);
					bytes += qq_put32(raw_data + bytes, 0x00000000);
					break;
				case QQ_FILE_DATA_INFO:
					bytes += qq_put16(raw_data + bytes, seq);
					bytes += qq_put8(raw_data + bytes, sub_type);
					bytes += qq_put32(raw_data + bytes, fragment_index);
					break;
				case QQ_FILE_EOF:
					bytes += qq_put16(raw_data + bytes, filesize / QQ_FILE_FRAGMENT_MAXLEN + 2);
					bytes += qq_put8(raw_data + bytes, sub_type);
					break;
			}
	}
	purple_debug_info("QQ", "<== send %s packet\n", qq_get_file_cmd_desc(packet_type));
	_qq_send_file(gc, raw_data, bytes, QQ_FILE_DATA_PACKET_TAG, info->to_uid);
}

/* A conversation starts like this:
 * Sender ==> Receiver [QQ_FILE_CMD_PING]
 * Sender <== Receiver [QQ_FILE_CMD_PONG]
 * Sender ==> Receiver [QQ_FILE_CMD_SENDER_SAY_HELLO]
 * Sender <== Receiver [QQ_FILE_CMD_SENDER_SAY_HELLO_ACK]
 * Sender <== Receiver [QQ_FILE_CMD_RECEIVER_SAY_HELLO]
 * Sender ==> Receiver [QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK]
 * Sender ==> Receiver [QQ_FILE_CMD_FILE_OP, QQ_FILE_BASIC_INFO]
 * Sender <== Receiver [QQ_FILE_CMD_FILE_OP_ACK, QQ_FILE_BASIC_INFO]
 * Sender ==> Receiver [QQ_FILE_CMD_FILE_OP, QQ_FILE_DATA_INFO]
 * Sender <== Receiver [QQ_FILE_CMD_FILE_OP_ACK, QQ_FILE_DATA_INFO]
 * Sender ==> Receiver [QQ_FILE_CMD_FILE_OP, QQ_FILE_DATA_INFO]
 * Sender <== Receiver [QQ_FILE_CMD_FILE_OP_ACK, QQ_FILE_DATA_INFO]
 * ......
 * Sender ==> Receiver [QQ_FILE_CMD_FILE_OP, QQ_FILE_EOF]
 * Sender <== Receiver [QQ_FILE_CMD_FILE_OP_ACK, QQ_FILE_EOF]
 */


static void _qq_process_recv_file_ctl_packet(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint bytes ;
	gint decryped_bytes;
	qq_file_header fh;
	guint8 *decrypted_data;
	gint decrypted_len;
	qq_data *qd = (qq_data *) gc->proto_data;
	guint16 packet_type;
	guint16 seq;
	guint8 hellobyte;
	ft_info *info = (ft_info *) qd->xfer->data;

	bytes = 0;
	bytes += _qq_get_file_header(&fh, data + bytes);

	decrypted_data = g_newa(guint8, data_len);
	decrypted_len = qq_decrypt(decrypted_data, data, data_len, qd->session_md5);
	if ( decrypted_len <= 0 ) {
		purple_debug_error("QQ", "Error decrypt rcv file ctrl packet\n");
		return;
	}

	/* only for debug info */
	decryped_bytes = 16;	/* skip md5 section */
	decryped_bytes += qq_get16(&packet_type, decrypted_data + decryped_bytes);
	decryped_bytes += qq_get16(&seq, decrypted_data + decryped_bytes);
	decryped_bytes += 4+1+1+19+1;	/* skip something */

	purple_debug_info("QQ", "==> [%d] receive %s packet\n", seq, qq_get_file_cmd_desc(packet_type));
	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ",
		decrypted_data, decrypted_len,
		"decrypted control packet received:");

	switch (packet_type) {
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
			decryped_bytes = 0;
			qq_get_conn_info(info, decrypted_data + decryped_bytes);
			/* qq_send_file_ctl_packet(gc, QQ_FILE_CMD_PING, fh->sender_uid, 0); */
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO, fh.sender_uid, 0);
			break;
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
			/* I'm receiver, if we receive SAY_HELLO from sender, we send back the ACK */
			decryped_bytes += 47;
			decryped_bytes += qq_get8(&hellobyte, decrypted_data + decryped_bytes);
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO_ACK, fh.sender_uid, hellobyte);
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_RECEIVER_SAY_HELLO, fh.sender_uid, 0);
			break;
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
			/* I'm sender, do nothing */
			break;
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
			/* I'm sender, ack the hello packet and send the first data */
			decryped_bytes += 47;
			decryped_bytes += qq_get8(&hellobyte, decrypted_data + decryped_bytes);
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK, fh.sender_uid, hellobyte);
			_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_BASIC_INFO, 0, 0, NULL, 0);
			break;
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
			/* I'm receiver, do nothing */
			break;
		case QQ_FILE_CMD_PING:
			/* I'm receiver, ack the PING */
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_PONG, fh.sender_uid, 0);
			break;
		case QQ_FILE_CMD_PONG:
			qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO, fh.sender_uid, 0);
			break;
		default:
			purple_debug_info("QQ", "unprocess file command %d\n", packet_type);
	}
}

static void _qq_recv_file_progess(PurpleConnection *gc, guint8 *buffer, guint16 len, guint32 index, guint32 offset)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	PurpleXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;
	guint32 mask;

	purple_debug_info("QQ",
			"receiving %dth fragment with length %d, slide window status %o, max_fragment_index %d\n",
			index, len, info->window, info->max_fragment_index);
	if (info->window == 0 && info->max_fragment_index == 0) {
		if (_qq_xfer_open_file(purple_xfer_get_local_filename(xfer), "wb", xfer) == -1) {
			purple_xfer_cancel_local(xfer);
			return;
		}
		purple_debug_info("QQ", "object file opened for writing\n");
	}
	mask = 0x1 << (index % sizeof(info->window));
	if (index < info->max_fragment_index || (info->window & mask)) {
		purple_debug_info("QQ", "duplicate %dth fragment, drop it!\n", index+1);
		return;
	}

	info->window |= mask;

	_qq_xfer_write_file(buffer, index, len, xfer);

	xfer->bytes_sent += len;
	xfer->bytes_remaining -= len;
	purple_xfer_update_progress(xfer);

	mask = 0x1 << (info->max_fragment_index % sizeof(info->window));
	while (info->window & mask)
	{
		info->window &= ~mask;
		info->max_fragment_index ++;
		if (mask & 0x8000) mask = 0x0001;
		else mask = mask << 1;
	}
	purple_debug_info("QQ", "procceed %dth fragment, slide window status %o, max_fragment_index %d\n",
			index, info->window, info->max_fragment_index);
}

static void _qq_send_file_progess(PurpleConnection *gc)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	PurpleXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;
	guint32 mask;
	guint8 *buffer;
	guint i;
	gint readbytes;

	if (purple_xfer_get_bytes_remaining(xfer) <= 0) return;
	if (info->window == 0 && info->max_fragment_index == 0)
	{
		if (_qq_xfer_open_file(purple_xfer_get_local_filename(xfer), "rb", xfer) == -1) {
			purple_xfer_cancel_local(xfer);
			return;
		}
	}
	buffer = g_newa(guint8, info->fragment_len);
	mask = 0x1 << (info->max_fragment_index % sizeof(info->window));
	for (i = 0; i < sizeof(info->window); i++) {
		if ((info->window & mask) == 0) {
			readbytes = _qq_xfer_read_file(buffer, info->max_fragment_index + i, info->fragment_len, xfer);
			if (readbytes > 0)
				_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_DATA_INFO,
						info->max_fragment_index + i + 1, 0, buffer, readbytes);
		}
		if (mask & 0x8000) mask = 0x0001;
		else mask = mask << 1;
	}
}

static void _qq_update_send_progess(PurpleConnection *gc, guint32 fragment_index)
{
	guint32 mask;
	guint8 *buffer;
	gint readbytes;
	qq_data *qd = (qq_data *) gc->proto_data;
	PurpleXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;

	purple_debug_info("QQ",
			"receiving %dth fragment ack, slide window status %o, max_fragment_index %d\n",
			fragment_index, info->window, info->max_fragment_index);
	if (fragment_index < info->max_fragment_index ||
			fragment_index >= info->max_fragment_index + sizeof(info->window)) {
		purple_debug_info("QQ", "duplicate %dth fragment, drop it!\n", fragment_index+1);
		return;
	}
	mask = 0x1 << (fragment_index % sizeof(info->window));
	if ((info->window & mask) == 0)
	{
		info->window |= mask;
		if (fragment_index + 1 != info->fragment_num) {
			xfer->bytes_sent += info->fragment_len;
		} else {
			xfer->bytes_sent += purple_xfer_get_size(xfer) % info->fragment_len;
		}
		xfer->bytes_remaining = purple_xfer_get_size(xfer) - purple_xfer_get_bytes_sent(xfer);
		purple_xfer_update_progress(xfer);
		if (purple_xfer_get_bytes_remaining(xfer) <= 0) {
			/* We have finished sending the file */
			purple_xfer_set_completed(xfer, TRUE);
			return;
		}
		mask = 0x1 << (info->max_fragment_index % sizeof(info->window));
		while (info->window & mask)
		{
			/* move the slide window */
			info->window &= ~mask;

			buffer = g_newa(guint8, info->fragment_len);
			readbytes = _qq_xfer_read_file(buffer, info->max_fragment_index + sizeof(info->window),
					info->fragment_len, xfer);
			if (readbytes > 0)
				_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_DATA_INFO,
						info->max_fragment_index + sizeof(info->window) + 1, 0, buffer, readbytes);

			info->max_fragment_index ++;
			if (mask & 0x8000) mask = 0x0001;
			else mask = mask << 1;
		}
	}
	purple_debug_info("QQ",
			"procceed %dth fragment ack, slide window status %o, max_fragment_index %d\n",
			fragment_index, info->window, info->max_fragment_index);
}

static void _qq_process_recv_file_data(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes ;
	qq_file_header fh;
	guint16 packet_type;
	guint16 packet_seq;
	guint8 sub_type;
	guint32 fragment_index;
	guint16 fragment_len;
	guint32 fragment_offset;
	qq_data *qd = (qq_data *) gc->proto_data;
	ft_info *info = (ft_info *) qd->xfer->data;

	bytes = 0;
	bytes += _qq_get_file_header(&fh, data + bytes);

	bytes += 1; /* skip an unknown byte */
	bytes += qq_get16(&packet_type, data + bytes);
	switch(packet_type)
	{
		case QQ_FILE_CMD_FILE_OP:
			bytes += qq_get16(&packet_seq, data + bytes);
			bytes += qq_get8(&sub_type, data + bytes);
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					bytes += 4;	/* file length, we have already known it from xfer */
					bytes += qq_get32(&info->fragment_num, data + bytes);
					bytes += qq_get32(&info->fragment_len, data + bytes);

					/* FIXME: We must check the md5 here,
					 * if md5 doesn't match we will ignore
					 * the packet or send sth as error number */

					info->max_fragment_index = 0;
					info->window = 0;
					purple_debug_info("QQ",
							"start receiving data, %d fragments with %d length each\n",
							info->fragment_num, info->fragment_len);
					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
							0, 0, NULL, 0);
					break;
				case QQ_FILE_DATA_INFO:
					bytes += qq_get32(&fragment_index, data + bytes);
					bytes += qq_get32(&fragment_offset, data + bytes);
					bytes += qq_get16(&fragment_len, data + bytes);
					purple_debug_info("QQ",
							"received %dth fragment with length %d, offset %d\n",
							fragment_index, fragment_len, fragment_offset);

					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
							fragment_index, packet_seq, NULL, 0);
					_qq_recv_file_progess(gc, data + bytes, fragment_len, fragment_index, fragment_offset);
					break;
				case QQ_FILE_EOF:
					purple_debug_info("QQ", "end of receiving\n");
					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
							0, 0, NULL, 0);
					break;
			}
			break;
		case QQ_FILE_CMD_FILE_OP_ACK:
			bytes += qq_get16(&packet_seq, data + bytes);
			bytes += qq_get8(&sub_type, data + bytes);
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					info->max_fragment_index = 0;
					info->window = 0;
					/* It is ready to send file data */
					_qq_send_file_progess(gc);
					break;
				case QQ_FILE_DATA_INFO:
					bytes += qq_get32(&fragment_index, data + bytes);
					_qq_update_send_progess(gc, fragment_index);
					if (purple_xfer_is_completed(qd->xfer))
						_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_EOF, 0, 0, NULL, 0);
					/*	else
						_qq_send_file_progess(gc); */
					break;
				case QQ_FILE_EOF:
					/* FIXME: OK, we can end the connection successfully */

					_qq_send_file_data_packet(gc, QQ_FILE_EOF, 0, 0, 0, NULL, 0);
					purple_xfer_set_completed(qd->xfer, TRUE);
					break;
			}
			break;
		case QQ_FILE_EOF:
			_qq_send_file_data_packet(gc, QQ_FILE_EOF, 0, 0, 0, NULL, 0);
			purple_xfer_set_completed(qd->xfer, TRUE);
			purple_xfer_end(qd->xfer);
			break;
		case QQ_FILE_BASIC_INFO:
			purple_debug_info("QQ", "here\n");
			_qq_send_file_data_packet(gc, QQ_FILE_DATA_INFO, 0, 0, 0, NULL, 0);
			break;
		default:
			purple_debug_info("QQ", "_qq_process_recv_file_data: unknown packet type [%d]\n",
					packet_type);
			break;
	}
}

void qq_process_recv_file(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes;
	guint8 tag;

	bytes = 0;
	bytes += qq_get8(&tag, data + bytes);

	switch (tag) {
		case QQ_FILE_CONTROL_PACKET_TAG:
			_qq_process_recv_file_ctl_packet(gc, data + bytes, len - bytes);
			break;
		case QQ_FILE_DATA_PACKET_TAG:
			_qq_process_recv_file_data(gc, data + bytes, len - bytes);
			break;
		default:
			purple_debug_info("QQ", "unknown packet tag\n");
	}
}
