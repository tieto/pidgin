/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 *	Author: Henry Ou <henry@linux.net>
 *
 * Copyright (C) 2004 Puzzlebird
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef _WIN32
#define random rand
#endif

#include "debug.h"
#include "ft.h"
#include "cipher.h"

#include "crypt.h"
#include "file_trans.h"
#include "header_info.h"
#include "im.h"
#include "packet_parse.h"
#include "proxy.h"
#include "send_core.h"
#include "send_file.h"
#include "utils.h"

struct _qq_file_header {
	guint8 tag;
	guint16 client_ver;
	guint8 file_key;
	guint32 sender_uid;
	guint32 receiver_uid;
};

typedef struct _qq_file_header qq_file_header;

static guint32 _get_file_key(guint8 seed)
{
	guint32 key;
	key = seed | (seed << 8) | (seed << 16) | (seed << 24);
	return key;
}
		
static guint32 _gen_file_key()
{
	guint8 seed;
	
	seed = random();
	return _get_file_key(seed);
}

static guint32 _decrypt_qq_uid(guint32 uid, guint32 key)
{
	return ~(uid ^ key);
}

static guint32 _encrypt_qq_uid(guint32 uid, guint32 key)
{
	return (~uid) ^ key;
}

static void _fill_filename_md5(const gchar *filename, guint8 *md5)
{
	GaimCipher *cipher;
	GaimCipherContext *context;

	g_return_if_fail(filename != NULL && md5 != NULL);

	cipher = gaim_ciphers_find_cipher("md5");
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, (guint8 *) filename, strlen(filename));
	gaim_cipher_context_digest(context, 16, md5, NULL);
	gaim_cipher_context_destroy(context);
}

static void _fill_file_md5(const gchar *filename, gint filelen, guint8 *md5)
{
	FILE *fp;
	guint8 *buffer;
	GaimCipher *cipher;
	GaimCipherContext *context;

	const gint QQ_MAX_FILE_MD5_LENGTH = 10002432;

	g_return_if_fail(filename != NULL && md5 != NULL);
	if (filelen > QQ_MAX_FILE_MD5_LENGTH) 
		filelen = QQ_MAX_FILE_MD5_LENGTH;

	fp = fopen(filename, "rb");
	g_return_if_fail(fp != NULL);

	buffer = g_newa(guint8, filelen);
	g_return_if_fail(buffer != NULL);
	fread(buffer, filelen, 1, fp);

	cipher = gaim_ciphers_find_cipher("md5");
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, buffer, filelen);
	gaim_cipher_context_digest(context, 16, md5, NULL);
	gaim_cipher_context_destroy(context);

	fclose(fp);
}

static void _qq_get_file_header(guint8 *buf, guint8 **cursor, gint buflen, qq_file_header *fh)
{
	read_packet_b(buf, cursor, buflen, &(fh->tag));
	read_packet_w(buf, cursor, buflen, &(fh->client_ver));
	read_packet_b(buf, cursor, buflen, &fh->file_key);
	read_packet_dw(buf, cursor, buflen, &(fh->sender_uid));
	read_packet_dw(buf, cursor, buflen, &(fh->receiver_uid));

	fh->sender_uid = _decrypt_qq_uid(fh->sender_uid, _get_file_key(fh->file_key));
	fh->receiver_uid = _decrypt_qq_uid(fh->receiver_uid, _get_file_key(fh->file_key));
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

static int _qq_xfer_open_file(const gchar *filename, const gchar *method, GaimXfer *xfer)
{
	ft_info *info = xfer->data;
	int fd;
	if (method[0] == 'r') {
		fd = open(gaim_xfer_get_local_filename(xfer), O_RDONLY);
		info->buffer = mmap(0, gaim_xfer_get_size(xfer), PROT_READ, MAP_PRIVATE, fd, 0);
	}
	else 
	{
		fd = open(gaim_xfer_get_local_filename(xfer), O_RDWR|O_CREAT, 0644);
		info->buffer = mmap(0, gaim_xfer_get_size(xfer), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
	}
		
	if (info->buffer == NULL) {
		return - 1;
	}
	return 0;
}

static gint _qq_xfer_read_file(guint8 *buffer, guint index, guint len, GaimXfer *xfer)
{
	ft_info *info = xfer->data;
	gint readbytes;

	buffer = info->buffer + len * index;
	readbytes = gaim_xfer_get_size(xfer) - (buffer - info->buffer);
	if (readbytes > info->fragment_len) readbytes = info->fragment_len;
	return readbytes;
}

static gint _qq_xfer_write_file(guint8 *buffer, guint index, guint len, GaimXfer *xfer)
{
	ft_info *info = xfer->data;

	memcpy(info->buffer + index * len, buffer, len);
	return 0;
}

void qq_xfer_close_file(GaimXfer *xfer)
{
	ft_info *info = xfer->data;

	if (info->buffer) munmap(info->buffer, gaim_xfer_get_size(xfer));
}
#else
static int _qq_xfer_open_file(const gchar *filename, const gchar *method, GaimXfer *xfer)
{
	ft_info *info = xfer->data;
	info->dest_fp = fopen(gaim_xfer_get_local_filename(xfer), method);
	if (info->dest_fp == NULL) {
		return -1;
	}
	return 0;
}

static gint _qq_xfer_read_file(guint8 *buffer, guint index, guint len, GaimXfer *xfer)
{
	ft_info *info = xfer->data;

	fseek(info->dest_fp, index * len, SEEK_SET);
	return fread(buffer, 1, len, info->dest_fp);
}

static gint _qq_xfer_write_file(guint8 *buffer, guint index, guint len, GaimXfer *xfer)
{
	ft_info *info = xfer->data;
	fseek(info->dest_fp, index * len, SEEK_SET);
	return fwrite(buffer, 1, len, info->dest_fp);
}

void qq_xfer_close_file(GaimXfer *xfer)
{
	ft_info *info = xfer->data;

	if (info->dest_fp) fclose(info->dest_fp);
}
#endif

static gint _qq_send_file(GaimConnection *gc, guint8 *data, gint len, guint16 packet_type, guint32 to_uid)
{
	gint bytes;
	guint8 *cursor, *buf;
	guint32 file_key;
	qq_data *qd;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(qd->session_key != NULL, -1);
	info = (ft_info *) qd->xfer->data;
	bytes = 0;

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	cursor = buf;
	file_key = _gen_file_key();

	bytes += create_packet_b(buf, &cursor, packet_type);
	bytes += create_packet_w(buf, &cursor, QQ_CLIENT);
	bytes += create_packet_b(buf, &cursor, file_key & 0xff);
	bytes += create_packet_dw(buf, &cursor, _encrypt_qq_uid(qd->uid, file_key));
	bytes += create_packet_dw(buf, &cursor, _encrypt_qq_uid(to_uid, file_key));
	bytes += create_packet_data(buf, &cursor, data, len);

	if (bytes == len + 12) {
		_qq_xfer_write(buf, bytes, qd->xfer);
	} else
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "send_file: want %d but got %d\n", len + 12, bytes);
	return bytes;
}

/* send a file to udp channel with QQ_FILE_CONTROL_PACKET_TAG */
void qq_send_file_ctl_packet(GaimConnection *gc, guint16 packet_type, guint32 to_uid, guint8 hellobyte)
{
	qq_data *qd;
	gint bytes, bytes_expected, encrypted_len;
	guint8 *raw_data, *cursor, *encrypted_data;
	time_t now;
	ft_info *info;
	
	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	raw_data = g_new0 (guint8, 61);
	cursor = raw_data;
	
	bytes = 0;
	now = time(NULL);

	bytes += create_packet_data(raw_data, &cursor, qd->session_md5, 16);
	bytes += create_packet_w(raw_data, &cursor, packet_type);
	switch (packet_type) {
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
			bytes += create_packet_w(raw_data, &cursor, info->send_seq);
			break;
		default:
			bytes += create_packet_w(raw_data, &cursor, ++qd->send_seq);
	}
	bytes += create_packet_dw(raw_data, &cursor, (guint32) now);
	bytes += create_packet_b(raw_data, &cursor, 0x00);
	bytes += create_packet_b(raw_data, &cursor, qd->my_icon);
	bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
	bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
	bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
	bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
	bytes += create_packet_w(raw_data, &cursor, 0x0000);
	bytes += create_packet_b(raw_data, &cursor, 0x00);
	/* 0x65: send a file, 0x6b: send a custom face */
	bytes += create_packet_b(raw_data, &cursor, QQ_FILE_TRANSFER_FILE); /* FIXME temp by gfhuang */
	switch (packet_type)
	{
		case QQ_FILE_CMD_SENDER_SAY_HELLO:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
		case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
		case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
			bytes += create_packet_b(raw_data, &cursor, 0x00);
			bytes += create_packet_b(raw_data, &cursor, hellobyte);
			bytes_expected = 48;
			break;
		case QQ_FILE_CMD_PING:
		case QQ_FILE_CMD_PONG:
		case QQ_FILE_CMD_NOTIFY_IP_ACK:
			bytes += qq_fill_conn_info(raw_data, &cursor, info);
			bytes_expected = 61;
			break;
		default:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "qq_send_file_ctl_packet: Unknown packet type[%d]\n",
					packet_type);
			bytes_expected = 0;
	}
	
	if (bytes == bytes_expected) {
		gchar *hex_dump = hex_dump_to_str(raw_data, bytes);
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "sending packet[%s]: \n%s", qq_get_file_cmd_desc(packet_type), hex_dump);
		g_free(hex_dump);
		encrypted_len = bytes + 16;
		encrypted_data = g_newa(guint8, encrypted_len);
		qq_crypt(ENCRYPT, raw_data, bytes, info->file_session_key, encrypted_data, &encrypted_len);
		/*debug: try to decrypt it */
		/*
		if (QQ_DEBUG) {
			guint8 *buf;
			int buflen;
			hex_dump = hex_dump_to_str(encrypted_data, encrypted_len);
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "encrypted packet: \n%s", hex_dump);
			g_free(hex_dump);
			buf = g_newa(guint8, MAX_PACKET_SIZE);
			buflen = encrypted_len;
			if (qq_crypt(DECRYPT, encrypted_data, encrypted_len, info->file_session_key, buf, &buflen)) {
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "decrypt success\n");
				if (buflen == bytes && memcmp(raw_data, buf, buflen) == 0)
					gaim_debug(GAIM_DEBUG_INFO, "QQ", "checksum ok\n");
				hex_dump = hex_dump_to_str(buf, buflen);
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "decrypted packet: \n%s", hex_dump);
				g_free(hex_dump);
			} else {
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "decrypt fail\n");
			}
		}
		*/

		gaim_debug(GAIM_DEBUG_INFO, "QQ", "<== send %s packet\n", qq_get_file_cmd_desc(packet_type));
		_qq_send_file(gc, encrypted_data, encrypted_len, QQ_FILE_CONTROL_PACKET_TAG, info->to_uid);
	}
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "qq_send_file_ctl_packet: Expected to get %d bytes, but get %d",
				bytes_expected, bytes);
}

/* send a file to udp channel with QQ_FILE_DATA_PACKET_TAG */
static void _qq_send_file_data_packet(GaimConnection *gc, guint16 packet_type, guint8 sub_type, 
		guint32 fragment_index, guint16 seq, guint8 *data, gint len)
{
	gint bytes;
	guint8 *raw_data, *cursor, filename_md5[QQ_KEY_LENGTH], file_md5[QQ_KEY_LENGTH];
	guint32 fragment_size = 1000;
	gchar *filename;
	gint filename_len, filesize;
	qq_data *qd;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	filename = (gchar *) gaim_xfer_get_filename(qd->xfer);
	filesize = gaim_xfer_get_size(qd->xfer);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE);
	cursor = raw_data;
	bytes = 0;

	bytes += create_packet_b(raw_data, &cursor, 0x00);
	bytes += create_packet_w(raw_data, &cursor, packet_type);
	switch (packet_type) {
		case QQ_FILE_BASIC_INFO:
		case QQ_FILE_DATA_INFO:
		case QQ_FILE_EOF:
			bytes += create_packet_w(raw_data, &cursor, 0x0000);
			bytes += create_packet_b(raw_data, &cursor, 0x00);
			break;
		case QQ_FILE_CMD_FILE_OP:
			switch(sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					filename_len = strlen(filename);
					_fill_filename_md5(filename, filename_md5);
					_fill_file_md5(gaim_xfer_get_local_filename(qd->xfer),
							gaim_xfer_get_size(qd->xfer),
							file_md5);

					info->fragment_num = (filesize - 1) / QQ_FILE_FRAGMENT_MAXLEN + 1;
					info->fragment_len = QQ_FILE_FRAGMENT_MAXLEN;

					gaim_debug(GAIM_DEBUG_INFO, "QQ", 
							"start transfering data, %d fragments with %d length each\n",
							info->fragment_num, info->fragment_len);
					/* Unknown */
					bytes += create_packet_w(raw_data, &cursor, 0x0000);
					/* Sub-operation type */
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					/* Length of file */
					bytes += create_packet_dw(raw_data, &cursor, filesize);
					/* Number of fragments */
					bytes += create_packet_dw(raw_data, &cursor, info->fragment_num);
					/* Length of a single fragment */
					bytes += create_packet_dw(raw_data, &cursor, info->fragment_len);
					bytes += create_packet_data(raw_data, &cursor, file_md5, 16);
					bytes += create_packet_data(raw_data, &cursor, filename_md5, 16);
					/* Length of filename */
					bytes += create_packet_w(raw_data, &cursor, filename_len);
					/* 8 unknown bytes */
					bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
					bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
					/* filename */
					bytes += create_packet_data(raw_data, &cursor, (guint8 *) filename,
							filename_len);
					break;
				case QQ_FILE_DATA_INFO:
					gaim_debug(GAIM_DEBUG_INFO, "QQ", 
							"sending %dth fragment with length %d, offset %d\n",
							fragment_index, len, (fragment_index-1)*fragment_size);
					/* bytes += create_packet_w(raw_data, &cursor, ++(qd->send_seq)); */
					bytes += create_packet_w(raw_data, &cursor, info->send_seq);
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					/* bytes += create_packet_dw(raw_data, &cursor, fragment_index); */
					bytes += create_packet_dw(raw_data, &cursor, fragment_index - 1);
					bytes += create_packet_dw(raw_data, &cursor, (fragment_index - 1) * fragment_size);
					bytes += create_packet_w(raw_data, &cursor, len);
					bytes += create_packet_data(raw_data, &cursor, data, len);
					break;
				case QQ_FILE_EOF:
					gaim_debug(GAIM_DEBUG_INFO, "QQ", "end of sending data\n");
					/* bytes += create_packet_w(raw_data, &cursor, info->fragment_num + 1); */
					bytes += create_packet_w(raw_data, &cursor, info->fragment_num);
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					/* gaim_xfer_set_completed(qd->xfer, TRUE); */
			}
			break;
		case QQ_FILE_CMD_FILE_OP_ACK:
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					bytes += create_packet_w(raw_data, &cursor, 0x0000);
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
					break;
				case QQ_FILE_DATA_INFO:
					bytes += create_packet_w(raw_data, &cursor, seq);
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					bytes += create_packet_dw(raw_data, &cursor, fragment_index);
					break;
				case QQ_FILE_EOF:
					bytes += create_packet_w(raw_data, &cursor, filesize / QQ_FILE_FRAGMENT_MAXLEN + 2);
					bytes += create_packet_b(raw_data, &cursor, sub_type);
					break;
			}
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "<== send %s packet\n", qq_get_file_cmd_desc(packet_type));
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


static void _qq_process_recv_file_ctl_packet(GaimConnection *gc, guint8 *data, guint8 *cursor,
		gint len, qq_file_header *fh)
{
	guint8 *decrypted_data;
	gint decrypted_len;
	qq_data *qd = (qq_data *) gc->proto_data;
	guint16 packet_type;
	guint16 seq;
	guint8 hellobyte;
	ft_info *info = (ft_info *) qd->xfer->data;

	decrypted_data = g_newa(guint8, len);
	decrypted_len = len;

	if (qq_crypt(DECRYPT, cursor, len - (cursor - data), qd->session_md5, decrypted_data, &decrypted_len)) {
		gchar *hex_dump;
		cursor = decrypted_data + 16;	/* skip md5 section */
		read_packet_w(decrypted_data, &cursor, decrypted_len, &packet_type);
		read_packet_w(decrypted_data, &cursor, decrypted_len, &seq);
		cursor += 4+1+1+19+1;
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "==> [%d] receive %s packet\n", seq, qq_get_file_cmd_desc(packet_type));
		hex_dump = hex_dump_to_str(decrypted_data, decrypted_len);
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "decrypted control packet received: \n%s", hex_dump);
		g_free(hex_dump);
		switch (packet_type) {
			case QQ_FILE_CMD_NOTIFY_IP_ACK:
				cursor = decrypted_data;
				qq_get_conn_info(decrypted_data, &cursor, decrypted_len, info);
/*				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_PING, fh->sender_uid, 0); */
				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO, fh->sender_uid, 0);	
				break;
			case QQ_FILE_CMD_SENDER_SAY_HELLO:
				/* I'm receiver, if we receive SAY_HELLO from sender, we send back the ACK */
				cursor += 47;
				read_packet_b(decrypted_data, &cursor, 
						decrypted_len, &hellobyte);

				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO_ACK, fh->sender_uid, hellobyte);
				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_RECEIVER_SAY_HELLO, fh->sender_uid, 0);
				break;
			case QQ_FILE_CMD_SENDER_SAY_HELLO_ACK:
				/* I'm sender, do nothing */
				break;
			case QQ_FILE_CMD_RECEIVER_SAY_HELLO:
				/* I'm sender, ack the hello packet and send the first data */
				cursor += 47;
				read_packet_b(decrypted_data, &cursor, 
						decrypted_len, &hellobyte);
				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK, fh->sender_uid, hellobyte);
				_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_BASIC_INFO, 0, 0, NULL, 0);
				break;
			case QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK:
				/* I'm receiver, do nothing */
				break;
			case QQ_FILE_CMD_PING:
				/* I'm receiver, ack the PING */
				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_PONG, fh->sender_uid, 0);
				break;
			case QQ_FILE_CMD_PONG:
				qq_send_file_ctl_packet(gc, QQ_FILE_CMD_SENDER_SAY_HELLO, fh->sender_uid, 0);
				break;
			default:
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "unprocess file command %d\n", packet_type);
		}
	} 
}

static void _qq_recv_file_progess(GaimConnection *gc, guint8 *buffer, guint16 len, guint32 index, guint32 offset)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	GaimXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;
	guint32 mask;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"receiving %dth fragment with length %d, slide window status %o, max_fragment_index %d\n", 
			index, len, info->window, info->max_fragment_index);
	if (info->window == 0 && info->max_fragment_index == 0) {
		if (_qq_xfer_open_file(gaim_xfer_get_local_filename(xfer), "wb", xfer) == -1) {
			gaim_xfer_cancel_local(xfer);
			return;
		}
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "object file opened for writing\n");
	}
	mask = 0x1 << (index % sizeof(info->window));
	if (index < info->max_fragment_index || (info->window & mask)) {
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "duplicate %dth fragment, drop it!\n", index+1);
		return;
	}
		
	info->window |= mask;

	_qq_xfer_write_file(buffer, index, len, xfer);
	
	xfer->bytes_sent += len;
	xfer->bytes_remaining -= len;
	gaim_xfer_update_progress(xfer);
	
	mask = 0x1 << (info->max_fragment_index % sizeof(info->window));
	while (info->window & mask)
	{
		info->window &= ~mask;
		info->max_fragment_index ++;
		if (mask & 0x8000) mask = 0x0001;
		else mask = mask << 1;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "procceed %dth fragment, slide window status %o, max_fragment_index %d\n", 
			index, info->window, info->max_fragment_index);
}

static void _qq_send_file_progess(GaimConnection *gc)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	GaimXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;
	guint32 mask;
	guint8 *buffer;
	guint i;
	gint readbytes;
	
	if (gaim_xfer_get_bytes_remaining(xfer) <= 0) return;
	if (info->window == 0 && info->max_fragment_index == 0)
	{
		if (_qq_xfer_open_file(gaim_xfer_get_local_filename(xfer), "rb", xfer) == -1) {
			gaim_xfer_cancel_local(xfer);
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

static void _qq_update_send_progess(GaimConnection *gc, guint32 fragment_index)
{
	guint32 mask;
	guint8 *buffer;
	gint readbytes;
	qq_data *qd = (qq_data *) gc->proto_data;
	GaimXfer *xfer = qd->xfer;
	ft_info *info = (ft_info *) xfer->data;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"receiving %dth fragment ack, slide window status %o, max_fragment_index %d\n", 
			fragment_index, info->window, info->max_fragment_index);
	if (fragment_index < info->max_fragment_index || 
			fragment_index >= info->max_fragment_index + sizeof(info->window)) {
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "duplicate %dth fragment, drop it!\n", fragment_index+1);
		return;
	}
	mask = 0x1 << (fragment_index % sizeof(info->window));
	if ((info->window & mask) == 0)
	{
		info->window |= mask;
		if (fragment_index + 1 != info->fragment_num) {
			xfer->bytes_sent += info->fragment_len;
		} else {
			xfer->bytes_sent += gaim_xfer_get_size(xfer) % info->fragment_len;
		}
		xfer->bytes_remaining = gaim_xfer_get_size(xfer) - gaim_xfer_get_bytes_sent(xfer);
		gaim_xfer_update_progress(xfer);
		if (gaim_xfer_get_bytes_remaining(xfer) <= 0) {
			/* We have finished sending the file */
			gaim_xfer_set_completed(xfer, TRUE);
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
	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"procceed %dth fragment ack, slide window status %o, max_fragment_index %d\n", 
			fragment_index, info->window, info->max_fragment_index);
}

static void _qq_process_recv_file_data(GaimConnection *gc, guint8 *data, guint8 *cursor,
		gint len, guint32 to_uid)
{
	guint16 packet_type;
	guint16 packet_seq;
	guint8 sub_type;
	guint32 fragment_index;
	guint16 fragment_len;
	guint32 fragment_offset;
	qq_data *qd = (qq_data *) gc->proto_data;
	ft_info *info = (ft_info *) qd->xfer->data;
	
	cursor += 1; /* skip an unknown byte */
	read_packet_w(data, &cursor, len, &packet_type);
	switch(packet_type)
	{
		case QQ_FILE_CMD_FILE_OP:
			read_packet_w(data, &cursor, len, &packet_seq);
			read_packet_b(data, &cursor, len, &sub_type);
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					cursor += 4;	/* file length, we have already known it from xfer */
					read_packet_dw(data, &cursor, len, &info->fragment_num);
					read_packet_dw(data, &cursor, len, &info->fragment_len);

					/* FIXME: We must check the md5 here, if md5 doesn't match
					 * we will ignore the packet or send sth as error number
					 */

					info->max_fragment_index = 0;
					info->window = 0;
					gaim_debug(GAIM_DEBUG_INFO, "QQ", 
							"start receiving data, %d fragments with %d length each\n",
							info->fragment_num, info->fragment_len);
					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
							0, 0, NULL, 0);
					break;
				case QQ_FILE_DATA_INFO:
					read_packet_dw(data, &cursor, len, &fragment_index);
					read_packet_dw(data, &cursor, len, &fragment_offset);
					read_packet_w(data, &cursor, len, &fragment_len);
					gaim_debug(GAIM_DEBUG_INFO, "QQ", 
							"received %dth fragment with length %d, offset %d\n",
							fragment_index, fragment_len, fragment_offset);
					
					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
							fragment_index, packet_seq, NULL, 0);
					_qq_recv_file_progess(gc, cursor, fragment_len, fragment_index, fragment_offset);
					break;
				case QQ_FILE_EOF:
					gaim_debug(GAIM_DEBUG_INFO, "QQ", "end of receiving\n");
					_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP_ACK, sub_type,
						0, 0, NULL, 0);
					break;
			}
			break;
		case QQ_FILE_CMD_FILE_OP_ACK:
			read_packet_w(data, &cursor, len, &packet_seq);
			read_packet_b(data, &cursor, len, &sub_type);
			switch (sub_type)
			{
				case QQ_FILE_BASIC_INFO:
					info->max_fragment_index = 0;
					info->window = 0;
					/* It is ready to send file data */
					_qq_send_file_progess(gc);
					break;
				case QQ_FILE_DATA_INFO:
					read_packet_dw(data, &cursor, len, &fragment_index);
					_qq_update_send_progess(gc, fragment_index);
					if (gaim_xfer_is_completed(qd->xfer))
						_qq_send_file_data_packet(gc, QQ_FILE_CMD_FILE_OP, QQ_FILE_EOF, 0, 0, NULL, 0);
				/*	else
						_qq_send_file_progess(gc); */
					break;
				case QQ_FILE_EOF:
					/* FIXME: OK, we can end the connection successfully */
					
					_qq_send_file_data_packet(gc, QQ_FILE_EOF, 0, 0, 0, NULL, 0);
					gaim_xfer_set_completed(qd->xfer, TRUE);
					break;
			}
			break;
		case QQ_FILE_EOF:
			_qq_send_file_data_packet(gc, QQ_FILE_EOF, 0, 0, 0, NULL, 0);
			gaim_xfer_set_completed(qd->xfer, TRUE);
			gaim_xfer_end(qd->xfer);
			break;
		case QQ_FILE_BASIC_INFO:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "here\n");
			_qq_send_file_data_packet(gc, QQ_FILE_DATA_INFO, 0, 0, 0, NULL, 0);
			break;
		default:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "_qq_process_recv_file_data: unknown packet type [%d]\n",
					packet_type);
			break;
	}
}

void qq_process_recv_file(GaimConnection *gc, guint8 *data, gint len)
{
	guint8 *cursor;
	qq_file_header fh;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;

	cursor = data;
	_qq_get_file_header(data, &cursor, len, &fh);

	switch (fh.tag) {
		case QQ_FILE_CONTROL_PACKET_TAG:
			_qq_process_recv_file_ctl_packet(gc, data, cursor, len, &fh);
			break;
		case QQ_FILE_DATA_PACKET_TAG:
			_qq_process_recv_file_data(gc, data, cursor, len, fh.sender_uid);
			break;
		default:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "unknown packet tag");
	}
}
