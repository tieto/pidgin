/**
 * @file send_file.c
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "send_file.h"
#include "debug.h"
#include "network.h"
#include "notify.h"

#include "buddy_status.h"
#include "crypt.h"
#include "file_trans.h"
#include "header_info.h"
#include "im.h"
#include "keep_alive.h"
#include "packet_parse.h"
#include "qq.h"
#include "send_core.h"
#include "utils.h"

enum
{
	QQ_FILE_TRANS_REQ = 0x0035,
	QQ_FILE_TRANS_ACC_UDP = 0x0037,
	QQ_FILE_TRANS_ACC_TCP = 0x0003,
	QQ_FILE_TRANS_DENY_UDP = 0x0039,
	QQ_FILE_TRANS_DENY_TCP = 0x0005,
	QQ_FILE_TRANS_NOTIFY = 0x003b,
	QQ_FILE_TRANS_NOTIFY_ACK = 0x003c,
	QQ_FILE_TRANS_CANCEL = 0x0049,
	QQ_FILE_TRANS_PASV = 0x003f
};

static int _qq_in_same_lan(ft_info *info)
{
	if (info->remote_internet_ip == info->local_internet_ip) return 1;
	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"Not in the same LAN, remote internet ip[%x], local internet ip[%x]\n",  
			info->remote_internet_ip
			, info->local_internet_ip);
	return 0;
}

static int _qq_xfer_init_udp_channel(ft_info *info)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (!_qq_in_same_lan(info)) {
		sin.sin_port = g_htons(info->remote_major_port);
		sin.sin_addr.s_addr = g_htonl(info->remote_internet_ip);
	} else {
		sin.sin_port = g_htons(info->remote_minor_port);
		sin.sin_addr.s_addr = g_htonl(info->remote_real_ip);
	}
	return 0;
}

/* these 2 functions send and recv buffer from/to UDP channel */
static ssize_t _qq_xfer_udp_recv(guint8 *buf, size_t len, GaimXfer *xfer)
{
	struct sockaddr_in sin;
	socklen_t sinlen;
	ft_info *info;
	gint r;

	info = (ft_info *) xfer->data;
	sinlen = sizeof(sin);
	r = recvfrom(info->recv_fd, buf, len, 0, (struct sockaddr *) &sin, &sinlen);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"==> recv %d bytes from File UDP Channel, remote ip[%s], remote port[%d]\n",
			r, inet_ntoa(sin.sin_addr), g_ntohs(sin.sin_port));
	return r;
}

/*
static ssize_t _qq_xfer_udp_send(const char *buf, size_t len, GaimXfer *xfer)
{
	ft_info *info;

	info = (ft_info *) xfer->data;
	return send(info->sender_fd, buf, len, 0);
}
*/
static ssize_t _qq_xfer_udp_send(const guint8 *buf, size_t len, GaimXfer *xfer)
{
	struct sockaddr_in sin;
	ft_info *info;

	info = (ft_info *) xfer->data;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (!_qq_in_same_lan(info)) {
		sin.sin_port = g_htons(info->remote_major_port);
		sin.sin_addr.s_addr = g_htonl(info->remote_internet_ip);
	} else if (info->use_major) {
		sin.sin_port = g_htons(info->remote_major_port);
		sin.sin_addr.s_addr = g_htonl(info->remote_real_ip);
	} else {
		sin.sin_port = g_htons(info->remote_minor_port);
		sin.sin_addr.s_addr = g_htonl(info->remote_real_ip);
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "sending to channel: %d.%d.%d.%d:%d\n",
			sin.sin_addr.s_addr & 0xff,
			(sin.sin_addr.s_addr >> 8) & 0xff,
			(sin.sin_addr.s_addr >> 16) & 0xff,
			sin.sin_addr.s_addr >> 24,
			g_ntohs(sin.sin_port)
		  );
	return sendto(info->sender_fd, buf, len, 0, (struct sockaddr *) &sin, sizeof(sin));
}

/* user-defined functions for gaim_xfer_read and gaim_xfer_write */

/*
static ssize_t _qq_xfer_read(char **buf, GaimXfer *xfer)
{
	*buf = g_newa(char, QQ_FILE_FRAGMENT_MAXLEN + 100);
	return _qq_xfer_udp_recv(*buf, QQ_FILE_FRAGMENT_MAXLEN + 100, xfer);
}
*/

gssize _qq_xfer_write(const guint8 *buf, size_t len, GaimXfer *xfer)
{
	return _qq_xfer_udp_send(buf, len, xfer);
}

static void _qq_xfer_recv_packet(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXfer *xfer = (GaimXfer *) data;
	GaimAccount *account = gaim_xfer_get_account(xfer);
	GaimConnection *gc = gaim_account_get_connection(account);
	guint8 *buf;
	gint size;
	/* FIXME: It seems that the transfer never use a packet
	 * larger than 1500 bytes, so if it happened to be a
	 * larger packet, either error occured or protocol should
	 * be modified
	 */
	ft_info *info;
	info = xfer->data;
	g_return_if_fail (source == info->recv_fd);
	buf = g_newa(guint8, 1500);
	size = _qq_xfer_udp_recv(buf, 1500, xfer);
	qq_process_recv_file(gc, buf, size);
}

/* start file transfer process */
/*
static void _qq_xfer_send_start (GaimXfer *xfer)
{
	GaimAccount *account;
	GaimConnection *gc;
	ft_info *info;

	account = gaim_xfer_get_account(xfer);
	gc = gaim_account_get_connection(account);
	info = (ft_info *) xfer->data;
}
*/

/*
static void _qq_xfer_send_ack (GaimXfer *xfer, const char *buffer, size_t len)
{
	GaimAccount *account;
	GaimConnection *gc;

	account = gaim_xfer_get_account(xfer);
	gc = gaim_account_get_connection(account);
	qq_process_recv_file(gc, (guint8 *) buffer, len);
}
*/

/*
static void _qq_xfer_recv_start(GaimXfer *xfer)
{
}
*/

static void _qq_xfer_end(GaimXfer *xfer)
{
	ft_info *info;
	g_return_if_fail(xfer != NULL && xfer->data != NULL);
	info = (ft_info *) xfer->data;

	qq_xfer_close_file(xfer);
	if (info->dest_fp != NULL) {
		fclose(info->dest_fp);
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "file closed\n");
	}
	if (info->major_fd != 0) {
		close(info->major_fd);
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "major port closed\n");
	}
	if (info->minor_fd != 0) {
		close(info->minor_fd);
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "minor port closed\n");
	}
	/*
	if (info->buffer != NULL) {
		munmap(info->buffer, gaim_xfer_get_size(xfer));
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "file mapping buffer is freed.\n");
	}
	*/
	g_free(info);
}

static void qq_show_conn_info(ft_info *info)
{
	gchar *internet_ip_str, *real_ip_str;
	guint32 ip;

	ip = g_htonl(info->remote_real_ip);
	real_ip_str = gen_ip_str((guint8 *) &ip);
	ip = g_htonl(info->remote_internet_ip);
	internet_ip_str = gen_ip_str((guint8 *) &ip);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "remote internet ip[%s:%d], major port[%d], real ip[%s], minor port[%d]\n",
			internet_ip_str, info->remote_internet_port,
			info->remote_major_port, real_ip_str, info->remote_minor_port
		  );
	g_free(real_ip_str);
	g_free(internet_ip_str);
}

void qq_get_conn_info(guint8 *data, guint8 **cursor, gint data_len, ft_info *info)
{
	read_packet_data(data, cursor, data_len, info->file_session_key, 16);
	*cursor += 30;
	read_packet_b(data, cursor, data_len, &info->conn_method);
	read_packet_dw(data, cursor, data_len, &info->remote_internet_ip);
	read_packet_w(data, cursor, data_len, &info->remote_internet_port);
	read_packet_w(data, cursor, data_len, &info->remote_major_port);
	read_packet_dw(data, cursor, data_len, &info->remote_real_ip);
	read_packet_w(data, cursor, data_len, &info->remote_minor_port);
	qq_show_conn_info(info);
}

gint qq_fill_conn_info(guint8 *raw_data, guint8 **cursor, ft_info *info)
{
	gint bytes;
	bytes = 0;
	/* 064: connection method, UDP 0x00, TCP 0x03 */
	bytes += create_packet_b (raw_data, cursor, info->conn_method);
	/* 065-068: outer ip address of sender (proxy address) */
	bytes += create_packet_dw (raw_data, cursor, info->local_internet_ip);
	/* 069-070: sender port */
	bytes += create_packet_w (raw_data, cursor, info->local_internet_port);
	/* 071-072: the first listening port(TCP doesn't have this part) */
	bytes += create_packet_w (raw_data, cursor, info->local_major_port);
	/* 073-076: real ip */
	bytes += create_packet_dw (raw_data, cursor, info->local_real_ip);
	/* 077-078: the second listening port */
	bytes += create_packet_w (raw_data, cursor, info->local_minor_port);
	return bytes;
}


/* fill in the common information of file transfer */
static gint _qq_create_packet_file_header
(guint8 *raw_data, guint8 **cursor, guint32 to_uid, guint16 message_type, qq_data *qd, gboolean seq_ack)
{
	gint bytes;
	time_t now;
	guint16 seq;
	ft_info *info;

	bytes = 0;
	now = time(NULL);
	if (!seq_ack) seq = qd->send_seq;
	else {
		info = (ft_info *) qd->xfer->data;
		seq = info->send_seq;
	}

	/* 000-003: receiver uid */
	bytes += create_packet_dw (raw_data, cursor, qd->uid);
	/* 004-007: sender uid */
	bytes += create_packet_dw (raw_data, cursor, to_uid);
	/* 008-009: sender client version */
	bytes += create_packet_w (raw_data, cursor, QQ_CLIENT);
	/* 010-013: receiver uid */
	bytes += create_packet_dw (raw_data, cursor, qd->uid);
	/* 014-017: sender uid */
	bytes += create_packet_dw (raw_data, cursor, to_uid);
	/* 018-033: md5 of (uid+session_key) */
	bytes += create_packet_data (raw_data, cursor, qd->session_md5, 16);
	/* 034-035: message type */
	bytes += create_packet_w (raw_data, cursor, message_type);
	/* 036-037: sequence number */
	bytes += create_packet_w (raw_data, cursor, seq);
	/* 038-041: send time */
	bytes += create_packet_dw (raw_data, cursor, (guint32) now);
	/* 042-042: always 0x00 */
	bytes += create_packet_b (raw_data, cursor, 0x00);
	/* 043-043: sender icon */
	bytes += create_packet_b (raw_data, cursor, qd->my_icon);
	/* 044-046: always 0x00 */
	bytes += create_packet_w (raw_data, cursor, 0x0000);
	bytes += create_packet_b (raw_data, cursor, 0x00);
	/* 047-047: we use font attr */
	bytes += create_packet_b (raw_data, cursor, 0x01);
	/* 048-051: always 0x00 */
	bytes += create_packet_dw (raw_data, cursor, 0x00000000);

	/* 052-062: always 0x00 */
	bytes += create_packet_dw (raw_data, cursor, 0x00000000);
	bytes += create_packet_dw (raw_data, cursor, 0x00000000);
	bytes += create_packet_w (raw_data, cursor, 0x0000);
	bytes += create_packet_b (raw_data, cursor, 0x00);
	/* 063: transfer_type,  0x65: FILE 0x6b: FACE */
	bytes += create_packet_b (raw_data, cursor, QQ_FILE_TRANSFER_FILE); /* FIXME */

	return bytes;
}

#if 0
in_addr_t get_real_ip()
{
	char hostname[40];
	struct hostent *host;

	gethostname(hostname, sizeof(hostname));
	host = gethostbyname(hostname);
	return *(host->h_addr);
}


#include <sys/ioctl.h>
#include <net/if.h>

#define MAXINTERFACES 16
in_addr_t get_real_ip()
{
	int fd, intrface, i;
	struct ifconf ifc;
	struct ifreq buf[MAXINTERFACES];
	in_addr_t ret;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return 0;
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = (caddr_t) buf;
	if (ioctl(fd, SIOCGIFCONF, (char *) &ifc) < 0) return 0;
	intrface = ifc.ifc_len / sizeof(struct ifreq);
	for (i = 0; i < intrface; i++) {
		//buf[intrface].ifr_name
		if (ioctl(fd, SIOCGIFADDR, (char *) &buf[i]) >= 0)
		{
			ret = (((struct sockaddr_in *)(&buf[i].ifr_addr))->sin_addr).s_addr;
			if (ret == g_ntohl(0x7f000001)) continue;
			return ret;
		}
	}
	return 0;
}
#endif

static void _qq_xfer_init_socket(GaimXfer *xfer)
{
	gint sockfd, listen_port = 0, i; 
	socklen_t sin_len;
	struct sockaddr_in sin;
	ft_info *info;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->data != NULL);
	info = (ft_info *) xfer->data;

	/* debug 
	info->local_real_ip = 0x7f000001;
	*/
	info->local_real_ip = g_ntohl(inet_addr(gaim_network_get_my_ip(-1)));
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "local real ip is %x", info->local_real_ip);

	for (i = 0; i < 2; i++) {
		sockfd = socket(PF_INET, SOCK_DGRAM, 0);
		g_return_if_fail(sockfd >= 0);

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin_len = sizeof(sin);
		bind(sockfd, (struct sockaddr *) &sin, sin_len);
		getsockname(sockfd, (struct sockaddr *) &sin, &sin_len);
		listen_port = g_ntohs(sin.sin_port);

		switch (i) {
			case 0:
				info->local_major_port = listen_port;
				info->major_fd = sockfd;
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "UDP Major Channel created on port[%d]\n",
						info->local_major_port);
				break;
			case 1:
				info->local_minor_port = listen_port;
				info->minor_fd = sockfd;
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "UDP Minor Channel created on port[%d]\n",
						info->local_minor_port);
				break;
		}
	}

	if (_qq_in_same_lan(info)) {
		info->sender_fd = info->recv_fd = info->minor_fd;
	} else {
		info->sender_fd = info->recv_fd = info->major_fd;
	}
/*	xfer->watcher = gaim_input_add(info->recv_fd, GAIM_INPUT_READ, _qq_xfer_recv_packet, xfer); */
}

/* create the QQ_FILE_TRANS_REQ packet with file infomations */
static void _qq_send_packet_file_request (GaimConnection *gc, guint32 to_uid, gchar *filename, gint filesize)
{
	qq_data *qd;
	guint8 *cursor, *raw_data;
	gchar *filelen_str;
	gint filename_len, filelen_strlen, packet_len, bytes;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;

	info = g_new0(ft_info, 1);
	info->to_uid = to_uid;
	info->send_seq = qd->send_seq;
	info->local_internet_ip = g_ntohl(inet_addr(qd->my_ip));
	info->local_internet_port = qd->my_port;
	info->local_real_ip = 0x00000000;
	info->conn_method = 0x00;
	qd->xfer->data = info;

	filename_len = strlen(filename);
	filelen_str = g_strdup_printf("%d ?ֽ?", filesize);
	filelen_strlen = strlen(filelen_str);

	packet_len = 82 + filename_len + filelen_strlen;
	raw_data = g_newa(guint8, packet_len);
	cursor = raw_data;

	bytes = _qq_create_packet_file_header(raw_data, &cursor, to_uid, 
			QQ_FILE_TRANS_REQ, qd, FALSE);
	bytes += qq_fill_conn_info(raw_data, &cursor, info);
	/* 079: 0x20 */
	bytes += create_packet_b (raw_data, &cursor, 0x20);
	/* 080: 0x1f */
	bytes += create_packet_b (raw_data, &cursor, 0x1f);
	/* undetermined len: filename */
	bytes += create_packet_data (raw_data, &cursor, (guint8 *) filename,
				     filename_len);
	/* 0x1f */
	bytes += create_packet_b (raw_data, &cursor, 0x1f);
	/* file length */
	bytes += create_packet_data (raw_data, &cursor, (guint8 *) filelen_str,
				     filelen_strlen);

	if (packet_len == bytes)
		qq_send_cmd (gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data,
			     cursor - raw_data);
	else
		gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file_request",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	g_free (filelen_str);
}

/* tell the buddy we want to accept the file */
static void _qq_send_packet_file_accept(GaimConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *cursor, *raw_data;
	guint16 minor_port;
	guint32 real_ip;
	gint packet_len, bytes;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "I've accepted the file transfer request from %d\n", to_uid);
	_qq_xfer_init_socket(qd->xfer);

	packet_len = 79;
	raw_data = g_newa (guint8, packet_len);
	cursor = raw_data;

	minor_port = info->local_minor_port;
	real_ip = info->local_real_ip;
	info->local_minor_port = 0;
	info->local_real_ip = 0;

	bytes = _qq_create_packet_file_header(raw_data, &cursor, to_uid, QQ_FILE_TRANS_ACC_UDP, qd, TRUE);
	bytes += qq_fill_conn_info(raw_data, &cursor, info);

	info->local_minor_port = minor_port;
	info->local_real_ip = real_ip;

	if (packet_len == bytes)
		qq_send_cmd (gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data,
			     cursor - raw_data);
	else
		gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file_accept",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);
}

static void _qq_send_packet_file_notifyip(GaimConnection *gc, guint32 to_uid)
{
	GaimXfer *xfer;
	ft_info *info;
	qq_data *qd;
	guint8 *cursor, *raw_data;
	gint packet_len, bytes;

	qd = (qq_data *) gc->proto_data;
	xfer = qd->xfer;
	info = xfer->data;

	packet_len = 79;
	raw_data = g_newa (guint8, packet_len);
	cursor = raw_data;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "<== sending qq file notify ip packet\n");
	bytes = _qq_create_packet_file_header(raw_data, &cursor, to_uid, QQ_FILE_TRANS_NOTIFY, qd, TRUE);
	bytes += qq_fill_conn_info(raw_data, &cursor, info);
	if (packet_len == bytes)
		qq_send_cmd (gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data,
			     cursor - raw_data);
	else
		gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file_notify",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	if (xfer->watcher) gaim_input_remove(xfer->watcher);
	xfer->watcher = gaim_input_add(info->recv_fd, GAIM_INPUT_READ, _qq_xfer_recv_packet, xfer);
	gaim_input_add(info->major_fd, GAIM_INPUT_READ, _qq_xfer_recv_packet, xfer);
}

/* tell the buddy we don't want the file */
static void _qq_send_packet_file_reject (GaimConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *cursor, *raw_data;
	gint packet_len, bytes;

	gaim_debug(GAIM_DEBUG_INFO, "_qq_send_packet_file_reject", "start");
	qd = (qq_data *) gc->proto_data;

	packet_len = 64;
	raw_data = g_newa (guint8, packet_len);
	cursor = raw_data;
	bytes = 0;

	bytes = _qq_create_packet_file_header(raw_data, &cursor, to_uid, QQ_FILE_TRANS_DENY_UDP, qd, TRUE);

	if (packet_len == bytes)
		qq_send_cmd (gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data,
			     cursor - raw_data);
	else
		gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);
}

/* tell the buddy to cancel transfer */
static void _qq_send_packet_file_cancel (GaimConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *cursor, *raw_data;
	gint packet_len, bytes;

	gaim_debug(GAIM_DEBUG_INFO, "_qq_send_packet_file_cancel", "start\n");
	qd = (qq_data *) gc->proto_data;

	packet_len = 64;
	raw_data = g_newa (guint8, packet_len);
	cursor = raw_data;
	bytes = 0;

	gaim_debug(GAIM_DEBUG_INFO, "_qq_send_packet_file_cancel", "before create header\n");
	bytes = _qq_create_packet_file_header(raw_data, &cursor, to_uid, QQ_FILE_TRANS_CANCEL, qd, TRUE);
	gaim_debug(GAIM_DEBUG_INFO, "_qq_send_packet_file_cancel", "end create header\n");

	if (packet_len == bytes) {
		gaim_debug(GAIM_DEBUG_INFO, "_qq_send_packet_file_cancel", "before send cmd\n");
		qq_send_cmd (gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data,
			     cursor - raw_data);
	}
	else
		gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	gaim_debug (GAIM_DEBUG_INFO, "qq_send_packet_file_cancel", "end\n");
}

/* request to send a file */
static void
_qq_xfer_init (GaimXfer * xfer)
{
	GaimConnection *gc;
	GaimAccount *account;
	guint32 to_uid;
	gchar *filename, *filename_without_path;

	g_return_if_fail (xfer != NULL);
	account = gaim_xfer_get_account(xfer);
	gc = gaim_account_get_connection(account);

	to_uid = gaim_name_to_uid (xfer->who);
	g_return_if_fail (to_uid != 0);

	filename = (gchar *) gaim_xfer_get_local_filename (xfer);
	g_return_if_fail (filename != NULL);

	filename_without_path = strrchr (filename, '/') + 1;

	_qq_send_packet_file_request (gc, to_uid, filename_without_path,
			gaim_xfer_get_size(xfer));
}

/* cancel the transfer of receiving files */
static void _qq_xfer_cancel(GaimXfer *xfer)
{
	GaimConnection *gc;
	GaimAccount *account;
	guint16 *seq;

	g_return_if_fail (xfer != NULL);
	seq = (guint16 *) xfer->data;
	account = gaim_xfer_get_account(xfer);
	gc = gaim_account_get_connection(account);

	switch (gaim_xfer_get_status(xfer)) {
		case GAIM_XFER_STATUS_CANCEL_LOCAL:
			_qq_send_packet_file_cancel(gc, gaim_name_to_uid(xfer->who));
			break;
		case GAIM_XFER_STATUS_CANCEL_REMOTE:
			_qq_send_packet_file_cancel(gc, gaim_name_to_uid(xfer->who));
			break;
		case GAIM_XFER_STATUS_NOT_STARTED:
			break;
		case GAIM_XFER_STATUS_UNKNOWN:
			_qq_send_packet_file_reject(gc, gaim_name_to_uid(xfer->who));
			break;
		case GAIM_XFER_STATUS_DONE:
			break;
		case GAIM_XFER_STATUS_ACCEPTED:
			break;
		case GAIM_XFER_STATUS_STARTED:
			break;
	}
}

/* init the transfer of receiving files */
static void _qq_xfer_recv_init(GaimXfer *xfer)
{
	GaimConnection *gc;
	GaimAccount *account;
	ft_info *info;

	g_return_if_fail (xfer != NULL && xfer->data != NULL);
	info = (ft_info *) xfer->data;
	account = gaim_xfer_get_account(xfer);
	gc = gaim_account_get_connection(account);

	_qq_send_packet_file_accept(gc, gaim_name_to_uid(xfer->who));
}

/* process reject im for file transfer request */
void qq_process_recv_file_reject (guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, GaimConnection *gc)
{
	gchar *msg, *filename;
	qq_data *qd;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	g_return_if_fail (qd->xfer != NULL);

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received file reject message is empty\n");
		return;
	}
	filename = strrchr(gaim_xfer_get_local_filename(qd->xfer), '/') + 1;
	msg = g_strdup_printf(_("%d has declined the file %s"),
		 sender_uid, filename);

	gaim_notify_warning (gc, _("File Send"), msg, NULL);
	gaim_xfer_request_denied(qd->xfer);
	qd->xfer = NULL;

	g_free (msg);
}

/* process cancel im for file transfer request */
void qq_process_recv_file_cancel (guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, GaimConnection *gc)
{
	gchar *msg, *filename;
	qq_data *qd;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	g_return_if_fail (qd->xfer != NULL
			&& gaim_xfer_get_filename(qd->xfer) != NULL);

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received file reject message is empty\n");
		return;
	}
	filename = strrchr(gaim_xfer_get_local_filename(qd->xfer), '/') + 1;
	msg = g_strdup_printf
		(_("%d canceled the transfer of %s"),
		 sender_uid, filename);

	gaim_notify_warning (gc, _("File Send"), msg, NULL);
	gaim_xfer_cancel_remote(qd->xfer);
	qd->xfer = NULL;

	g_free (msg);
}

/* process accept im for file transfer request */
void qq_process_recv_file_accept(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, GaimConnection *gc)
{
	qq_data *qd;
	ft_info *info;
	GaimXfer *xfer;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	xfer = qd->xfer;

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received file reject message is empty\n");
		return;
	}

	info = (ft_info *) qd->xfer->data;

	*cursor = data + 18 + 12;
	qq_get_conn_info(data, cursor, data_len, info);
	_qq_xfer_init_socket(qd->xfer);

	_qq_xfer_init_udp_channel(info);
	_qq_send_packet_file_notifyip(gc, sender_uid);
}

/* process request from buddy's im for file transfer request */
void qq_process_recv_file_request(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, GaimConnection * gc)
{
	qq_data *qd;
	GaimXfer *xfer;
	gchar *sender_name, **fileinfo;
	ft_info *info;
	GaimBuddy *b;
	qq_buddy *q_bud;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received file reject message is empty\n");
		return;
	}

	info = g_new0(ft_info, 1);
	info->local_internet_ip = g_ntohl(inet_addr(qd->my_ip));
	info->local_internet_port = qd->my_port;
	info->local_real_ip = 0x00000000;
	info->to_uid = sender_uid;
	read_packet_w(data, cursor, data_len, &(info->send_seq));

	*cursor = data + 18 + 12;
	qq_get_conn_info(data, cursor, data_len, info);

	fileinfo = g_strsplit((gchar *) (data + 81 + 12), "\x1f", 2);
	g_return_if_fail (fileinfo != NULL && fileinfo[0] != NULL && fileinfo[1] != NULL);

	sender_name = uid_to_gaim_name(sender_uid);

	/* FACE from IP detector, ignored by gfhuang */
	if(g_ascii_strcasecmp(fileinfo[0], "FACE") == 0) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ",
			    "Received a FACE ip detect from qq-%d, so he/she must be online :)\n", sender_uid);

		b = gaim_find_buddy(gc->account, sender_name);
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
		if (q_bud) {
			if(0 != info->remote_real_ip) {
				g_memmove(q_bud->ip, &info->remote_real_ip, 4);
				q_bud->port = info->remote_minor_port;
			}
			else if (0 != info->remote_internet_ip) {
				g_memmove(q_bud->ip, &info->remote_internet_ip, 4);
				q_bud->port = info->remote_major_port;
			}

			if(!is_online(q_bud->status)) {
				q_bud->status = QQ_BUDDY_ONLINE_INVISIBLE;
				qq_update_buddy_contact(gc, q_bud);
			}
			else 
				gaim_debug(GAIM_DEBUG_INFO, "QQ", "buddy %d is already online\n", sender_uid);

		}
		else 
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "buddy %d is not in my friendlist\n", sender_uid);

		g_free(sender_name);	    
		g_strfreev(fileinfo);
		return;
	}
	
	xfer = gaim_xfer_new(gaim_connection_get_account(gc),
			GAIM_XFER_RECEIVE,
			sender_name);
	gaim_xfer_set_filename(xfer, fileinfo[0]);
	gaim_xfer_set_size(xfer, atoi(fileinfo[1]));

	gaim_xfer_set_init_fnc(xfer, _qq_xfer_recv_init);
	gaim_xfer_set_request_denied_fnc(xfer, _qq_xfer_cancel);
	gaim_xfer_set_cancel_recv_fnc(xfer, _qq_xfer_cancel);
	gaim_xfer_set_end_fnc(xfer, _qq_xfer_end);
	gaim_xfer_set_write_fnc(xfer, _qq_xfer_write);

	xfer->data = info;
	qd->xfer = xfer;

	gaim_xfer_request(xfer);

	g_free(sender_name);
	g_strfreev(fileinfo);
}

static void _qq_xfer_send_notify_ip_ack(gpointer data, gint source, GaimInputCondition cond)
{
	GaimXfer *xfer = (GaimXfer *) data;
	GaimAccount *account = gaim_xfer_get_account(xfer);
	GaimConnection *gc = gaim_account_get_connection(account);
	ft_info *info = (ft_info *) xfer->data;

	gaim_input_remove(xfer->watcher);
	xfer->watcher = gaim_input_add(info->recv_fd, GAIM_INPUT_READ, _qq_xfer_recv_packet, xfer);
	qq_send_file_ctl_packet(gc, QQ_FILE_CMD_NOTIFY_IP_ACK, info->to_uid, 0);
	/*
	info->use_major = TRUE;
	qq_send_file_ctl_packet(gc, QQ_FILE_CMD_NOTIFY_IP_ACK, info->to_uid, 0);
	info->use_major = FALSE;
	*/
}

void qq_process_recv_file_notify(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, GaimConnection *gc)
{
	qq_data *qd;
	ft_info *info;
	GaimXfer *xfer;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received file notify message is empty\n");
		return;
	}

	xfer = qd->xfer;
	info = (ft_info *) qd->xfer->data;
	/* FIXME */
	read_packet_w(data, cursor, data_len, &(info->send_seq));

	*cursor = data + 18 + 12;
	qq_get_conn_info(data, cursor, data_len, info);

	_qq_xfer_init_udp_channel(info);

	xfer->watcher = gaim_input_add(info->sender_fd, GAIM_INPUT_WRITE, _qq_xfer_send_notify_ip_ack, xfer);
}

/* temp placeholder until a working function can be implemented */
gboolean qq_can_receive_file(GaimConnection *gc, const char *who)
{
	return TRUE;
}

void qq_send_file(GaimConnection *gc, const char *who, const char *file)
{
	qq_data *qd;
	GaimXfer *xfer;

	qd = (qq_data *) gc->proto_data;

	xfer = gaim_xfer_new (gc->account, GAIM_XFER_SEND,
			      who);
	gaim_xfer_set_init_fnc (xfer, _qq_xfer_init);
	gaim_xfer_set_cancel_send_fnc (xfer, _qq_xfer_cancel);
	gaim_xfer_set_write_fnc(xfer, _qq_xfer_write);

	qd->xfer = xfer;
	gaim_xfer_request (xfer);
}

/*
static void qq_send_packet_request_key(GaimConnection *gc, guint8 key)
{
	qq_send_cmd(gc, QQ_CMD_REQUEST_KEY, TRUE, 0, TRUE, &key, 1);
}

static void qq_process_recv_request_key(GaimConnection *gc)
{
}
*/
