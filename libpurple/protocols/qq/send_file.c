/**
 * @file send_file.c
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

#include "qq.h"

#include "send_file.h"
#include "debug.h"
#include "network.h"
#include "notify.h"

#include "buddy_list.h"
#include "file_trans.h"
#include "qq_define.h"
#include "im.h"
#include "qq_base.h"
#include "packet_parse.h"
#include "qq_network.h"
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
	purple_debug_info("QQ",
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
static ssize_t _qq_xfer_udp_recv(guint8 *buf, size_t len, PurpleXfer *xfer)
{
	struct sockaddr_in sin;
	socklen_t sinlen;
	ft_info *info;
	gint r;

	info = (ft_info *) xfer->data;
	sinlen = sizeof(sin);
	r = recvfrom(info->recv_fd, buf, len, 0, (struct sockaddr *) &sin, &sinlen);
	purple_debug_info("QQ",
			"==> recv %d bytes from File UDP Channel, remote ip[%s], remote port[%d]\n",
			r, inet_ntoa(sin.sin_addr), g_ntohs(sin.sin_port));
	return r;
}

/*
static ssize_t _qq_xfer_udp_send(const char *buf, size_t len, PurpleXfer *xfer)
{
	ft_info *info;

	info = (ft_info *) xfer->data;
	return send(info->sender_fd, buf, len, 0);
}
*/

static ssize_t _qq_xfer_udp_send(const guint8 *buf, size_t len, PurpleXfer *xfer)
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
	purple_debug_info("QQ", "sending to channel: %s:%d\n",
			inet_ntoa(sin.sin_addr),
			(int)g_ntohs(sin.sin_port)
		  );
	return sendto(info->sender_fd, buf, len, 0, (struct sockaddr *) &sin, sizeof(sin));
}

/* user-defined functions for purple_xfer_read and purple_xfer_write */

/*
static ssize_t _qq_xfer_read(char **buf, PurpleXfer *xfer)
{
	*buf = g_newa(char, QQ_FILE_FRAGMENT_MAXLEN + 100);
	return _qq_xfer_udp_recv(*buf, QQ_FILE_FRAGMENT_MAXLEN + 100, xfer);
}
*/

gssize _qq_xfer_write(const guint8 *buf, size_t len, PurpleXfer *xfer)
{
	return _qq_xfer_udp_send(buf, len, xfer);
}

static void _qq_xfer_recv_packet(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer = (PurpleXfer *) data;
	PurpleAccount *account = purple_xfer_get_account(xfer);
	PurpleConnection *gc = purple_account_get_connection(account);
	guint8 *buf;
	gint size;
	/* FIXME: It seems that the transfer never use a packet
	 * larger than 1500 bytes, so if it happened to be a
	 * larger packet, either error occurred or protocol should
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
static void _qq_xfer_send_start (PurpleXfer *xfer)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	ft_info *info;

	account = purple_xfer_get_account(xfer);
	gc = purple_account_get_connection(account);
	info = (ft_info *) xfer->data;
}
*/

/*
static void _qq_xfer_send_ack (PurpleXfer *xfer, const char *buffer, size_t len)
{
	PurpleAccount *account;
	PurpleConnection *gc;

	account = purple_xfer_get_account(xfer);
	gc = purple_account_get_connection(account);
	qq_process_recv_file(gc, (guint8 *) buffer, len);
}
*/

/*
static void _qq_xfer_recv_start(PurpleXfer *xfer)
{
}
*/

static void _qq_xfer_end(PurpleXfer *xfer)
{
	ft_info *info;
	g_return_if_fail(xfer != NULL && xfer->data != NULL);
	info = (ft_info *) xfer->data;

	qq_xfer_close_file(xfer);
	if (info->dest_fp != NULL) {
		fclose(info->dest_fp);
		purple_debug_info("QQ", "file closed\n");
	}
	if (info->major_fd != 0) {
		close(info->major_fd);
		purple_debug_info("QQ", "major port closed\n");
	}
	if (info->minor_fd != 0) {
		close(info->minor_fd);
		purple_debug_info("QQ", "minor port closed\n");
	}
	/*
	if (info->buffer != NULL) {
		munmap(info->buffer, purple_xfer_get_size(xfer));
		purple_debug_info("QQ", "file mapping buffer is freed.\n");
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
	purple_debug_info("QQ", "remote internet ip[%s:%d], major port[%d], real ip[%s], minor port[%d]\n",
			internet_ip_str, info->remote_internet_port,
			info->remote_major_port, real_ip_str, info->remote_minor_port
		  );
	g_free(real_ip_str);
	g_free(internet_ip_str);
}

#define QQ_CONN_INFO_LEN	61
gint qq_get_conn_info(ft_info *info, guint8 *data)
{
	gint bytes = 0;
	/* 16 + 30 + 1 + 4 + 2 + 2 + 4 + 2 = 61 */
	bytes += qq_getdata(info->file_session_key, 16, data + bytes);
	bytes += 30;	/* skip 30 bytes */
	bytes += qq_get8(&info->conn_method, data + bytes);
	bytes += qq_get32(&info->remote_internet_ip, data + bytes);
	bytes += qq_get16(&info->remote_internet_port, data + bytes);
	bytes += qq_get16(&info->remote_major_port, data + bytes);
	bytes += qq_get32(&info->remote_real_ip, data + bytes);
	bytes += qq_get16(&info->remote_minor_port, data + bytes);
	qq_show_conn_info(info);
	return bytes;
}

gint qq_fill_conn_info(guint8 *raw_data, ft_info *info)
{
	gint bytes = 0;
	/* 064: connection method, UDP 0x00, TCP 0x03 */
	bytes += qq_put8 (raw_data + bytes, info->conn_method);
	/* 065-068: outer ip address of sender (proxy address) */
	bytes += qq_put32 (raw_data + bytes, info->local_internet_ip);
	/* 069-070: sender port */
	bytes += qq_put16 (raw_data + bytes, info->local_internet_port);
	/* 071-072: the first listening port(TCP doesn't have this part) */
	bytes += qq_put16 (raw_data + bytes, info->local_major_port);
	/* 073-076: real ip */
	bytes += qq_put32 (raw_data + bytes, info->local_real_ip);
	/* 077-078: the second listening port */
	bytes += qq_put16 (raw_data + bytes, info->local_minor_port);
	return bytes;
}


/* fill in the common information of file transfer */
static gint _qq_create_packet_file_header
(guint8 *raw_data, guint32 to_uid, guint16 message_type, qq_data *qd, gboolean seq_ack)
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
	bytes += qq_put32 (raw_data + bytes, qd->uid);
	/* 004-007: sender uid */
	bytes += qq_put32 (raw_data + bytes, to_uid);
	/* 008-009: sender client version */
	bytes += qq_put16 (raw_data + bytes, qd->client_tag);
	/* 010-013: receiver uid */
	bytes += qq_put32 (raw_data + bytes, qd->uid);
	/* 014-017: sender uid */
	bytes += qq_put32 (raw_data + bytes, to_uid);
	/* 018-033: md5 of (uid+session_key) */
	bytes += qq_putdata (raw_data + bytes, qd->session_md5, 16);
	/* 034-035: message type */
	bytes += qq_put16 (raw_data + bytes, message_type);
	/* 036-037: sequence number */
	bytes += qq_put16 (raw_data + bytes, seq);
	/* 038-041: send time */
	bytes += qq_put32 (raw_data + bytes, (guint32) now);
	/* 042-042: always 0x00 */
	bytes += qq_put8 (raw_data + bytes, 0x00);
	/* 043-043: sender icon */
	bytes += qq_put8 (raw_data + bytes, qd->my_icon);
	/* 044-046: always 0x00 */
	bytes += qq_put16 (raw_data + bytes, 0x0000);
	bytes += qq_put8 (raw_data + bytes, 0x00);
	/* 047-047: we use font attr */
	bytes += qq_put8 (raw_data + bytes, 0x01);
	/* 048-051: always 0x00 */
	bytes += qq_put32 (raw_data + bytes, 0x00000000);

	/* 052-062: always 0x00 */
	bytes += qq_put32 (raw_data + bytes, 0x00000000);
	bytes += qq_put32 (raw_data + bytes, 0x00000000);
	bytes += qq_put16 (raw_data + bytes, 0x0000);
	bytes += qq_put8 (raw_data + bytes, 0x00);
	/* 063: transfer_type,  0x65: FILE 0x6b: FACE */
	bytes += qq_put8 (raw_data + bytes, QQ_FILE_TRANSFER_FILE); /* FIXME */

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
		/* buf[intrface].ifr_name */
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

static void _qq_xfer_init_socket(PurpleXfer *xfer)
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
	info->local_real_ip = g_ntohl(inet_addr(purple_network_get_my_ip(-1)));
	purple_debug_info("QQ", "local real ip is %x", info->local_real_ip);

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
				purple_debug_info("QQ", "UDP Major Channel created on port[%d]\n",
						info->local_major_port);
				break;
			case 1:
				info->local_minor_port = listen_port;
				info->minor_fd = sockfd;
				purple_debug_info("QQ", "UDP Minor Channel created on port[%d]\n",
						info->local_minor_port);
				break;
		}
	}

	if (_qq_in_same_lan(info)) {
		info->sender_fd = info->recv_fd = info->minor_fd;
	} else {
		info->sender_fd = info->recv_fd = info->major_fd;
	}
/*	xfer->watcher = purple_input_add(info->recv_fd, PURPLE_INPUT_READ, _qq_xfer_recv_packet, xfer); */
}

/* create the QQ_FILE_TRANS_REQ packet with file infomations */
static void _qq_send_packet_file_request (PurpleConnection *gc, guint32 to_uid, gchar *filename, gint filesize)
{
	qq_data *qd;
	guint8 *raw_data;
	gchar *filelen_str;
	gint filename_len, filelen_strlen, packet_len, bytes;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;

	info = g_new0(ft_info, 1);
	info->to_uid = to_uid;
	info->send_seq = qd->send_seq;
	info->local_internet_ip = qd->my_ip.s_addr;
	info->local_internet_port = qd->my_port;
	info->local_real_ip = 0x00000000;
	info->conn_method = 0x00;
	qd->xfer->data = info;

	filename_len = strlen(filename);
	filelen_str = g_strdup_printf("%d ?ֽ?", filesize);
	filelen_strlen = strlen(filelen_str);

	packet_len = 82 + filename_len + filelen_strlen;
	raw_data = g_newa(guint8, packet_len);
	bytes = 0;

	bytes += _qq_create_packet_file_header(raw_data + bytes, to_uid,
			QQ_FILE_TRANS_REQ, qd, FALSE);
	bytes += qq_fill_conn_info(raw_data + bytes, info);
	/* 079: 0x20 */
	bytes += qq_put8 (raw_data + bytes, 0x20);
	/* 080: 0x1f */
	bytes += qq_put8 (raw_data + bytes, 0x1f);
	/* undetermined len: filename */
	bytes += qq_putdata (raw_data + bytes, (guint8 *) filename, filename_len);
	/* 0x1f */
	bytes += qq_put8 (raw_data + bytes, 0x1f);
	/* file length */
	bytes += qq_putdata (raw_data + bytes, (guint8 *) filelen_str, filelen_strlen);

	if (packet_len == bytes)
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	else
		purple_debug_info("qq_send_packet_file_request",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	g_free (filelen_str);
}

/* tell the buddy we want to accept the file */
static void _qq_send_packet_file_accept(PurpleConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *raw_data;
	guint16 minor_port;
	guint32 real_ip;
	gint packet_len, bytes;
	ft_info *info;

	qd = (qq_data *) gc->proto_data;
	info = (ft_info *) qd->xfer->data;

	purple_debug_info("QQ", "I've accepted the file transfer request from %d\n", to_uid);
	_qq_xfer_init_socket(qd->xfer);

	packet_len = 79;
	raw_data = g_newa (guint8, packet_len);
	bytes = 0;

	minor_port = info->local_minor_port;
	real_ip = info->local_real_ip;
	info->local_minor_port = 0;
	info->local_real_ip = 0;

	bytes += _qq_create_packet_file_header(raw_data + bytes, to_uid, QQ_FILE_TRANS_ACC_UDP, qd, TRUE);
	bytes += qq_fill_conn_info(raw_data + bytes, info);

	info->local_minor_port = minor_port;
	info->local_real_ip = real_ip;

	if (packet_len == bytes)
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	else
		purple_debug_info("qq_send_packet_file_accept",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);
}

static void _qq_send_packet_file_notifyip(PurpleConnection *gc, guint32 to_uid)
{
	PurpleXfer *xfer;
	ft_info *info;
	qq_data *qd;
	guint8 *raw_data;
	gint packet_len, bytes;

	qd = (qq_data *) gc->proto_data;
	xfer = qd->xfer;
	info = xfer->data;

	packet_len = 79;
	raw_data = g_newa (guint8, packet_len);
	bytes = 0;

	purple_debug_info("QQ", "<== sending qq file notify ip packet\n");
	bytes += _qq_create_packet_file_header(raw_data + bytes, to_uid, QQ_FILE_TRANS_NOTIFY, qd, TRUE);
	bytes += qq_fill_conn_info(raw_data + bytes, info);
	if (packet_len == bytes)
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	else
		purple_debug_info("qq_send_packet_file_notify",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	if (xfer->watcher) purple_input_remove(xfer->watcher);
	xfer->watcher = purple_input_add(info->recv_fd, PURPLE_INPUT_READ, _qq_xfer_recv_packet, xfer);
	purple_input_add(info->major_fd, PURPLE_INPUT_READ, _qq_xfer_recv_packet, xfer);
}

/* tell the buddy we don't want the file */
static void _qq_send_packet_file_reject (PurpleConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *raw_data;
	gint packet_len, bytes;

	purple_debug_info("_qq_send_packet_file_reject", "start");
	qd = (qq_data *) gc->proto_data;

	packet_len = 64;
	raw_data = g_newa (guint8, packet_len);
	bytes = 0;

	bytes += _qq_create_packet_file_header(raw_data + bytes, to_uid, QQ_FILE_TRANS_DENY_UDP, qd, TRUE);

	if (packet_len == bytes)
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	else
		purple_debug_info("qq_send_packet_file",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);
}

/* tell the buddy to cancel transfer */
static void _qq_send_packet_file_cancel (PurpleConnection *gc, guint32 to_uid)
{
	qq_data *qd;
	guint8 *raw_data;
	gint packet_len, bytes;

	purple_debug_info("_qq_send_packet_file_cancel", "start\n");
	qd = (qq_data *) gc->proto_data;

	packet_len = 64;
	raw_data = g_newa (guint8, packet_len);
	bytes = 0;

	purple_debug_info("_qq_send_packet_file_cancel", "before create header\n");
	bytes += _qq_create_packet_file_header(raw_data + bytes, to_uid, QQ_FILE_TRANS_CANCEL, qd, TRUE);
	purple_debug_info("_qq_send_packet_file_cancel", "end create header\n");

	if (packet_len == bytes) {
		purple_debug_info("_qq_send_packet_file_cancel", "before send cmd\n");
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	}
	else
		purple_debug_info("qq_send_packet_file",
			    "%d bytes expected but got %d bytes\n",
			    packet_len, bytes);

	purple_debug_info("qq_send_packet_file_cancel", "end\n");
}

/* request to send a file */
static void
_qq_xfer_init (PurpleXfer * xfer)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	guint32 to_uid;
	const gchar *filename;
	gchar *base_filename;

	g_return_if_fail (xfer != NULL);
	account = purple_xfer_get_account(xfer);
	gc = purple_account_get_connection(account);

	to_uid = purple_name_to_uid (xfer->who);
	g_return_if_fail (to_uid != 0);

	filename = purple_xfer_get_local_filename (xfer);
	g_return_if_fail (filename != NULL);

	base_filename = g_path_get_basename(filename);

	_qq_send_packet_file_request (gc, to_uid, base_filename,
			purple_xfer_get_size(xfer));
	g_free(base_filename);
}

/* cancel the transfer of receiving files */
static void _qq_xfer_cancel(PurpleXfer *xfer)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	guint16 *seq;

	g_return_if_fail (xfer != NULL);
	seq = (guint16 *) xfer->data;
	account = purple_xfer_get_account(xfer);
	gc = purple_account_get_connection(account);

	switch (purple_xfer_get_status(xfer)) {
		case PURPLE_XFER_STATUS_CANCEL_LOCAL:
			_qq_send_packet_file_cancel(gc, purple_name_to_uid(xfer->who));
			break;
		case PURPLE_XFER_STATUS_CANCEL_REMOTE:
			_qq_send_packet_file_cancel(gc, purple_name_to_uid(xfer->who));
			break;
		case PURPLE_XFER_STATUS_NOT_STARTED:
			break;
		case PURPLE_XFER_STATUS_UNKNOWN:
			_qq_send_packet_file_reject(gc, purple_name_to_uid(xfer->who));
			break;
		case PURPLE_XFER_STATUS_DONE:
			break;
		case PURPLE_XFER_STATUS_ACCEPTED:
			break;
		case PURPLE_XFER_STATUS_STARTED:
			break;
	}
}

/* init the transfer of receiving files */
static void _qq_xfer_recv_init(PurpleXfer *xfer)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	ft_info *info;

	g_return_if_fail (xfer != NULL && xfer->data != NULL);
	info = (ft_info *) xfer->data;
	account = purple_xfer_get_account(xfer);
	gc = purple_account_get_connection(account);

	_qq_send_packet_file_accept(gc, purple_name_to_uid(xfer->who));
}

/* process reject im for file transfer request */
void qq_process_recv_file_reject (guint8 *data, gint data_len,
		guint32 sender_uid, PurpleConnection *gc)
{
	gchar *msg, *filename;
	qq_data *qd;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	g_return_if_fail (qd->xfer != NULL);

	/*	border has been checked before
	if (*cursor >= (data + data_len - 1)) {
		purple_debug_warning("QQ",
			    "Received file reject message is empty\n");
		return;
	}
	*/
	filename = g_path_get_basename(purple_xfer_get_local_filename(qd->xfer));
	msg = g_strdup_printf(_("%d has declined the file %s"),
		 sender_uid, filename);

	purple_notify_warning (gc, _("File Send"), msg, NULL);
	purple_xfer_request_denied(qd->xfer);
	qd->xfer = NULL;

	g_free(filename);
	g_free(msg);
}

/* process cancel im for file transfer request */
void qq_process_recv_file_cancel (guint8 *data, gint data_len,
		guint32 sender_uid, PurpleConnection *gc)
{
	gchar *msg, *filename;
	qq_data *qd;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	g_return_if_fail (qd->xfer != NULL
			&& purple_xfer_get_filename(qd->xfer) != NULL);

	/*	border has been checked before
	if (*cursor >= (data + data_len - 1)) {
		purple_debug_warning("QQ", "Received file reject message is empty\n");
		return;
	}
	*/
	filename = g_path_get_basename(purple_xfer_get_local_filename(qd->xfer));
	msg = g_strdup_printf
		(_("%d canceled the transfer of %s"),
		 sender_uid, filename);

	purple_notify_warning (gc, _("File Send"), msg, NULL);
	purple_xfer_cancel_remote(qd->xfer);
	qd->xfer = NULL;

	g_free(filename);
	g_free(msg);
}

/* process accept im for file transfer request */
void qq_process_recv_file_accept(guint8 *data, gint data_len, guint32 sender_uid, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes;
	ft_info *info;
	PurpleXfer *xfer;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;
	xfer = qd->xfer;
	info = (ft_info *) qd->xfer->data;

	if (data_len <= 30 + QQ_CONN_INFO_LEN) {
		purple_debug_warning("QQ", "Received file reject message is empty\n");
		return;
	}

	bytes = 18 + 12;	/* skip 30 bytes */
	qq_get_conn_info(info, data + bytes);
	_qq_xfer_init_socket(qd->xfer);

	_qq_xfer_init_udp_channel(info);
	_qq_send_packet_file_notifyip(gc, sender_uid);
}

/* process request from buddy's im for file transfer request */
void qq_process_recv_file_request(guint8 *data, gint data_len, guint32 sender_uid, PurpleConnection * gc)
{
	qq_data *qd;
	PurpleXfer *xfer;
	gchar *sender_name, **fileinfo;
	ft_info *info;
	PurpleBuddy *b;
	qq_buddy_data *bd;
	gint bytes;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;

	info = g_newa(ft_info, 1);
	info->local_internet_ip = qd->my_ip.s_addr;
	info->local_internet_port = qd->my_port;
	info->local_real_ip = 0x00000000;
	info->to_uid = sender_uid;

	if (data_len <= 2 + 30 + QQ_CONN_INFO_LEN) {
		purple_debug_warning("QQ", "Received file request message is empty\n");
		return;
	}
	bytes = 0;
	bytes += qq_get16(&(info->send_seq), data + bytes);

	bytes += 18 + 12;	/* skip 30 bytes */
	bytes += qq_get_conn_info(info, data + bytes);

	fileinfo = g_strsplit((gchar *) (data + 81 + 12), "\x1f", 2);
	g_return_if_fail (fileinfo != NULL && fileinfo[0] != NULL && fileinfo[1] != NULL);

	sender_name = uid_to_purple_name(sender_uid);

	/* FACE from IP detector, ignored by gfhuang */
	if(g_ascii_strcasecmp(fileinfo[0], "FACE") == 0) {
		purple_debug_warning("QQ",
			    "Received a FACE ip detect from %d, so he/she must be online :)\n", sender_uid);

		b = purple_find_buddy(gc->account, sender_name);
		bd = (b == NULL) ? NULL : purple_buddy_get_protocol_data(b);
		if (bd) {
			if(0 != info->remote_real_ip) {
				g_memmove(&(bd->ip), &info->remote_real_ip, sizeof(bd->ip));
				bd->port = info->remote_minor_port;
			}
			else if (0 != info->remote_internet_ip) {
				g_memmove(&(bd->ip), &info->remote_internet_ip, sizeof(bd->ip));
				bd->port = info->remote_major_port;
			}

			if(!is_online(bd->status)) {
				bd->status = QQ_BUDDY_ONLINE_INVISIBLE;
				bd->last_update = time(NULL);
				qq_update_buddy_status(gc, bd->uid, bd->status, bd->comm_flag);
			}
			else
				purple_debug_info("QQ", "buddy %d is already online\n", sender_uid);

		}
		else
			purple_debug_warning("QQ", "buddy %d is not in list\n", sender_uid);

		g_free(sender_name);
		g_strfreev(fileinfo);
		return;
	}

	xfer = purple_xfer_new(purple_connection_get_account(gc),
			PURPLE_XFER_RECEIVE,
			sender_name);
	if (xfer)
	{
		purple_xfer_set_filename(xfer, fileinfo[0]);
		purple_xfer_set_size(xfer, atoi(fileinfo[1]));

		purple_xfer_set_init_fnc(xfer, _qq_xfer_recv_init);
		purple_xfer_set_request_denied_fnc(xfer, _qq_xfer_cancel);
		purple_xfer_set_cancel_recv_fnc(xfer, _qq_xfer_cancel);
		purple_xfer_set_end_fnc(xfer, _qq_xfer_end);
		purple_xfer_set_write_fnc(xfer, _qq_xfer_write);

		xfer->data = info;
		qd->xfer = xfer;

		purple_xfer_request(xfer);
	}

	g_free(sender_name);
	g_strfreev(fileinfo);
}

static void _qq_xfer_send_notify_ip_ack(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleXfer *xfer = (PurpleXfer *) data;
	PurpleAccount *account = purple_xfer_get_account(xfer);
	PurpleConnection *gc = purple_account_get_connection(account);
	ft_info *info = (ft_info *) xfer->data;

	purple_input_remove(xfer->watcher);
	xfer->watcher = purple_input_add(info->recv_fd, PURPLE_INPUT_READ, _qq_xfer_recv_packet, xfer);
	qq_send_file_ctl_packet(gc, QQ_FILE_CMD_NOTIFY_IP_ACK, info->to_uid, 0);
	/*
	info->use_major = TRUE;
	qq_send_file_ctl_packet(gc, QQ_FILE_CMD_NOTIFY_IP_ACK, info->to_uid, 0);
	info->use_major = FALSE;
	*/
}

void qq_process_recv_file_notify(guint8 *data, gint data_len,
		guint32 sender_uid, PurpleConnection *gc)
{
	gint bytes;
	qq_data *qd;
	ft_info *info;
	PurpleXfer *xfer;

	g_return_if_fail (data != NULL && data_len != 0);
	qd = (qq_data *) gc->proto_data;

	xfer = qd->xfer;
	info = (ft_info *) qd->xfer->data;
	if (data_len <= 2 + 30 + QQ_CONN_INFO_LEN) {
		purple_debug_warning("QQ", "Received file notify message is empty\n");
		return;
	}

	bytes = 0;
	bytes += qq_get16(&(info->send_seq), data + bytes);

	bytes += 18 + 12;
	bytes += qq_get_conn_info(info, data + bytes);

	_qq_xfer_init_udp_channel(info);

	xfer->watcher = purple_input_add(info->sender_fd, PURPLE_INPUT_WRITE, _qq_xfer_send_notify_ip_ack, xfer);
}

/* temp placeholder until a working function can be implemented */
gboolean qq_can_receive_file(PurpleConnection *gc, const char *who)
{
	return TRUE;
}

void qq_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	qq_data *qd;
	PurpleXfer *xfer;

	qd = (qq_data *) gc->proto_data;

	xfer = purple_xfer_new (gc->account, PURPLE_XFER_SEND,
			      who);
	if (xfer)
	{
		purple_xfer_set_init_fnc (xfer, _qq_xfer_init);
		purple_xfer_set_cancel_send_fnc (xfer, _qq_xfer_cancel);
		purple_xfer_set_write_fnc(xfer, _qq_xfer_write);

		qd->xfer = xfer;
		purple_xfer_request(xfer);
	}
}

/*
static void qq_send_packet_request_key(PurpleConnection *gc, guint8 key)
{
	qq_send_cmd(gc, QQ_CMD_REQUEST_KEY, &key, 1);
}

static void qq_process_recv_request_key(PurpleConnection *gc)
{
}
*/
