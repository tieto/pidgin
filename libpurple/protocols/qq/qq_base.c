/**
 * @file qq_base.c
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

#include "debug.h"
#include "internal.h"
#include "server.h"
#include "cipher.h"
#include "request.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "char_conv.h"
#include "qq_crypt.h"
#include "group.h"
#include "qq_define.h"
#include "qq_base.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_network.h"
#include "utils.h"

#define QQ_LOGIN_DATA_LENGTH		    416
#define QQ_LOGIN_REPLY_OK_PACKET_LEN        139
#define QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN  11

/* for QQ 2003iii 0117, fixed value */
/* static const guint8 login_23_51[29] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xbf, 0x14, 0x11, 0x20,
	0x03, 0x9d, 0xb2, 0xe6, 0xb3, 0x11, 0xb7, 0x13,
	0x95, 0x67, 0xda, 0x2c, 0x01
}; */

/* for QQ 2003iii 0304, fixed value */
/*
static const guint8 login_23_51[29] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x9a, 0x93, 0xfe, 0x85,
	0xd3, 0xd9, 0x2a, 0x41, 0xc8, 0x0d, 0xff, 0xb6,
	0x40, 0xb8, 0xac, 0x32, 0x01
};
*/

/* for QQ 2005? copy from lumaqq */
/* FIXME: change to guint8 */
static const guint8 login_23_51[29] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x86, 0xcc, 0x4c, 0x35,
	0x2c, 0xd3, 0x73, 0x6c, 0x14, 0xf6, 0xf6, 0xaf,
	0xc3, 0xfa, 0x33, 0xa4, 0x01
};

static const guint8 login_53_68[16] = {
 	0x8D, 0x8B, 0xFA, 0xEC, 0xD5, 0x52, 0x17, 0x4A,
 	0x86, 0xF9, 0xA7, 0x75, 0xE6, 0x32, 0xD1, 0x6D
};

static const guint8 login_100_bytes[100] = {
	0x40, 0x0B, 0x04, 0x02, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x03, 0x09, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0xE9, 0x03, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xF3, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xED,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0xEC, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0xEE, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01, 0xEF, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0xEB, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00
};


/* fixed value, not affected by version, or mac address */
/*
static const guint8 login_53_68[16] = {
	0x82, 0x2a, 0x91, 0xfd, 0xa5, 0xca, 0x67, 0x4c,
	0xac, 0x81, 0x1f, 0x6f, 0x52, 0x05, 0xa7, 0xbf
};
*/


/* generate a md5 key using uid and session_key */
static void get_session_md5(guint8 *session_md5, guint32 uid, guint8 *session_key)
{
	guint8 src[QQ_KEY_LENGTH + QQ_KEY_LENGTH];
	gint bytes = 0;

	bytes += qq_put32(src + bytes, uid);
	bytes += qq_putdata(src + bytes, session_key, QQ_KEY_LENGTH);

	qq_get_md5(session_md5, QQ_KEY_LENGTH, src, bytes);
}

/* process login reply which says OK */
static gint8 process_login_ok(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes;
	qq_data *qd;

	struct {
		guint8 result;
		guint8 session_key[QQ_KEY_LENGTH];
		guint32 uid;
		struct in_addr client_ip;	/* those detected by server */
		guint16 client_port;
		struct in_addr server_ip;
		guint16 server_port;
		time_t login_time;
		guint8 unknown1[26];
		struct in_addr unknown_server1_ip;
		guint16 unknown_server1_port;
		struct in_addr unknown_server2_ip;
		guint16 unknown_server2_port;
		guint16 unknown2;	/* 0x0001 */
		guint16 unknown3;	/* 0x0000 */
		guint8 unknown4[32];
		guint8 unknown5[12];
		struct in_addr last_client_ip;
		time_t last_login_time;
		guint8 unknown6[8];
	} packet;

	qd = (qq_data *) gc->proto_data;
	/* FIXME, check QQ_LOGIN_REPLY_OK_PACKET_LEN here */
	bytes = 0;

	/* 000-000: reply code */
	bytes += qq_get8(&packet.result, data + bytes);
	/* 001-016: session key */
	bytes += qq_getdata(packet.session_key, sizeof(packet.session_key), data + bytes);
	purple_debug_info("QQ", "Got session_key\n");
	/* 017-020: login uid */
	bytes += qq_get32(&packet.uid, data + bytes);
	/* 021-024: server detected user public IP */
	bytes += qq_getIP(&packet.client_ip, data + bytes);
	/* 025-026: server detected user port */
	bytes += qq_get16(&packet.client_port, data + bytes);
	/* 027-030: server detected itself ip 127.0.0.1 ? */
	bytes += qq_getIP(&packet.server_ip, data + bytes);
	/* 031-032: server listening port */
	bytes += qq_get16(&packet.server_port, data + bytes);
	/* 033-036: login time for current session */
	bytes += qq_getime(&packet.login_time, data + bytes);
	/* 037-062: 26 bytes, unknown */
	bytes += qq_getdata((guint8 *) &packet.unknown1, 26, data + bytes);
	/* 063-066: unknown server1 ip address */
	bytes += qq_getIP(&packet.unknown_server1_ip, data + bytes);
	/* 067-068: unknown server1 port */
	bytes += qq_get16(&packet.unknown_server1_port, data + bytes);
	/* 069-072: unknown server2 ip address */
	bytes += qq_getIP(&packet.unknown_server2_ip, data + bytes);
	/* 073-074: unknown server2 port */
	bytes += qq_get16(&packet.unknown_server2_port, data + bytes);
	/* 075-076: 2 bytes unknown */
	bytes += qq_get16(&packet.unknown2, data + bytes);
	/* 077-078: 2 bytes unknown */
	bytes += qq_get16(&packet.unknown3, data + bytes);
	/* 079-110: 32 bytes unknown */
	bytes += qq_getdata((guint8 *) &packet.unknown4, 32, data + bytes);
	/* 111-122: 12 bytes unknown */
	bytes += qq_getdata((guint8 *) &packet.unknown5, 12, data + bytes);
	/* 123-126: login IP of last session */
	bytes += qq_getIP(&packet.last_client_ip, data + bytes);
	/* 127-130: login time of last session */
	bytes += qq_getime(&packet.last_login_time, data + bytes);
	/* 131-138: 8 bytes unknown */
	bytes += qq_getdata((guint8 *) &packet.unknown6, 8, data + bytes);

	if (bytes != QQ_LOGIN_REPLY_OK_PACKET_LEN) {	/* fail parsing login info */
		purple_debug_warning("QQ",
			   "Fail parsing login info, expect %d bytes, read %d bytes\n",
			   QQ_LOGIN_REPLY_OK_PACKET_LEN, bytes);
	}			/* but we still go on as login OK */

	memcpy(qd->session_key, packet.session_key, sizeof(qd->session_key));
	get_session_md5(qd->session_md5, qd->uid, qd->session_key);

	qd->my_ip.s_addr = packet.client_ip.s_addr;

	qd->my_port = packet.client_port;
	qd->login_time = packet.login_time;
	qd->last_login_time = packet.last_login_time;
	qd->last_login_ip = g_strdup( inet_ntoa(packet.last_client_ip) );

	return QQ_LOGIN_REPLY_OK;
}

/* process login reply packet which includes redirected new server address */
static gint8 process_login_redirect(PurpleConnection *gc, guint8 *data, gint len)
{
	qq_data *qd;
	gint bytes;
	struct {
		guint8 result;
		guint32 uid;
		struct in_addr new_server_ip;
		guint16 new_server_port;
	} packet;


	qd = (qq_data *) gc->proto_data;
	bytes = 0;
	/* 000-000: reply code */
	bytes += qq_get8(&packet.result, data + bytes);
	/* 001-004: login uid */
	bytes += qq_get32(&packet.uid, data + bytes);
	/* 005-008: redirected new server IP */
	bytes += qq_getIP(&packet.new_server_ip, data + bytes);
	/* 009-010: redirected new server port */
	bytes += qq_get16(&packet.new_server_port, data + bytes);

	if (bytes != QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN) {
		purple_debug_error("QQ",
			   "Fail parsing login redirect packet, expect %d bytes, read %d bytes\n",
			   QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN, bytes);
		return QQ_LOGIN_REPLY_ERR;
	}

	/* redirect to new server, do not disconnect or connect here
	 * those connect should be called at packet_process */
	qd->redirect_ip.s_addr = packet.new_server_ip.s_addr;
	qd->redirect_port = packet.new_server_port;
	return QQ_LOGIN_REPLY_REDIRECT;
}

/* request before login */
void qq_request_token(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 buf[16] = {0};
	gint bytes = 0;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	bytes += qq_put8(buf + bytes, 0);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN, qd->send_seq, buf, bytes, TRUE);
}

/* send login packet to QQ server */
void qq_request_login(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH);
	memset(raw_data, 0, QQ_LOGIN_DATA_LENGTH);

	encrypted_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH + 16);	/* 16 bytes more */

	bytes = 0;
	/* now generate the encrypted data
	 * 000-015 use password_twice_md5 as key to encrypt empty string */
	encrypted_len = qq_encrypt(raw_data + bytes, (guint8 *) "", 0, qd->ld.pwd_twice_md5);
	g_return_if_fail(encrypted_len == 16);
	bytes += encrypted_len;

	/* 016-016 */
	bytes += qq_put8(raw_data + bytes, 0x00);
	/* 017-020, used to be IP, now zero */
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	/* 021-022, used to be port, now zero */
	bytes += qq_put16(raw_data + bytes, 0x0000);
	/* 023-051, fixed value, unknown */
	bytes += qq_putdata(raw_data + bytes, login_23_51, 29);
	/* 052-052, login mode */
	bytes += qq_put8(raw_data + bytes, qd->login_mode);
	/* 053-068, fixed value, maybe related to per machine */
	bytes += qq_putdata(raw_data + bytes, login_53_68, 16);
	/* 069, login token length */
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	/* 070-093, login token, normally 24 bytes */
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	/* 100 bytes unknown */
	bytes += qq_putdata(raw_data + bytes, login_100_bytes, 100);
	/* all zero left */

	encrypted_len = qq_encrypt(encrypted_data, raw_data, QQ_LOGIN_DATA_LENGTH, qd->ld.init_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.init_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_LOGIN, qd->send_seq, buf, bytes, TRUE);
}

guint8 qq_process_token(PurpleConnection *gc, guint8 *buf, gint buf_len)
{
	qq_data *qd;
	guint8 ret;
	int token_len;
	gchar *error_msg;

	g_return_val_if_fail(buf != NULL && buf_len != 0, -1);

	g_return_val_if_fail(gc != NULL  && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

	ret = buf[0];

	if (ret != QQ_LOGIN_REPLY_OK) {
		purple_debug_error("QQ", "Failed to request token: %d\n", buf[0]);
		qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ",
				buf, buf_len,
				">>> [default] decrypt and dump");
		error_msg = try_dump_as_gbk(buf, buf_len);
		if (error_msg == NULL) {
				error_msg = g_strdup_printf( _("Invalid token reply code, 0x%02X"), ret);
		}
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_msg);
		g_free(error_msg);
		return QQ_LOGIN_REPLY_ERR;
	}

	token_len = buf_len-2;
	if (token_len <= 0) {
		error_msg = g_strdup_printf( _("Invalid token len, %d"), token_len);
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_msg);
		g_free(error_msg);
		return QQ_LOGIN_REPLY_ERR;
	}

	if (buf[1] != token_len) {
		purple_debug_info("QQ",
				"Invalid token len. Packet specifies length of %d, actual length is %d\n", buf[1], buf_len-2);
	}
	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ",
			buf+2, token_len,
			"<<< got a token -> [default] decrypt and dump");

	if (qd->ld.token != NULL) {
		g_free(qd->ld.token);
		qd->ld.token = NULL;
		qd->ld.token_len = 0;
	}
	qd->ld.token = g_new0(guint8, token_len);
	qd->ld.token_len = token_len;
	g_memmove(qd->ld.token, buf + 2, qd->ld.token_len);
	return ret;
}

/* send logout packets to QQ server */
void qq_request_logout(PurpleConnection *gc)
{
	gint i;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	for (i = 0; i < 4; i++)
		qq_send_cmd(gc, QQ_CMD_LOGOUT, qd->ld.pwd_twice_md5, QQ_KEY_LENGTH);

	qd->is_login = FALSE;	/* update login status AFTER sending logout packets */
}

/* process the login reply packet */
guint8 qq_process_login( PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint8 ret = data[0];
	gchar *server_reply, *server_reply_utf8;
	gchar *error_msg;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	qd = (qq_data *) gc->proto_data;

	switch (ret) {
		case QQ_LOGIN_REPLY_OK:
			purple_debug_info("QQ", "Login OK\n");
			return process_login_ok(gc, data, data_len);
		case QQ_LOGIN_REPLY_REDIRECT:
			purple_debug_info("QQ", "Redirect new server\n");
			return process_login_redirect(gc, data, data_len);

		case QQ_LOGIN_REPLY_REDIRECT_EX:
			purple_debug_error("QQ", "Extend redirect new server, not supported yet\n");
			error_msg = g_strdup( _("Unable login for not support Redirect_EX now") );
			return QQ_LOGIN_REPLY_REDIRECT_EX;

		case QQ_LOGIN_REPLY_ERR_PWD:
			server_reply = g_strndup((gchar *)data + 1, data_len - 1);
			server_reply_utf8 = qq_to_utf8(server_reply, QQ_CHARSET_DEFAULT);

			purple_debug_error("QQ", "Error password: %s\n", server_reply_utf8);
			error_msg = g_strdup_printf( _("Error password: %s"), server_reply_utf8);

			g_free(server_reply);
			g_free(server_reply_utf8);

			if (!purple_account_get_remember_password(gc->account)) {
				purple_account_set_password(gc->account, NULL);
			}

			purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, error_msg);
			g_free(error_msg);

			return QQ_LOGIN_REPLY_ERR_PWD;

		case QQ_LOGIN_REPLY_NEED_REACTIVE:
			server_reply = g_strndup((gchar *)data + 1, data_len - 1);
			server_reply_utf8 = qq_to_utf8(server_reply, QQ_CHARSET_DEFAULT);

			purple_debug_error("QQ", "Need active: %s\n", server_reply_utf8);
			error_msg = g_strdup_printf( _("Need active: %s"), server_reply_utf8);

			g_free(server_reply);
			g_free(server_reply_utf8);
			break;

		default:
			purple_debug_error("QQ",
				"Unable login for unknow reply code 0x%02X\n", data[0]);
			qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ",
				data, data_len,
				">>> [default] decrypt and dump");
			error_msg = try_dump_as_gbk(data, data_len);
			if (error_msg == NULL) {
				error_msg = g_strdup_printf(
					_("Unable login for unknow reply code 0x%02X"), data[0] );
			}
			break;
	}

	purple_connection_error_reason(gc,
		PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_msg);
	g_free(error_msg);
	return ret;
}

/* send keep-alive packet to QQ server (it is a heart-beat) */
void qq_request_keep_alive(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 raw_data[16] = {0};
	gint bytes= 0;

	qd = (qq_data *) gc->proto_data;

	/* In fact, we can send whatever we like to server
	 * with this command, server return the same result including
	 * the amount of online QQ users, my ip and port */
	bytes += qq_put32(raw_data + bytes, qd->uid);

	qq_send_cmd(gc, QQ_CMD_KEEP_ALIVE, raw_data, 4);
}

/* parse the return of keep-alive packet, it includes some system information */
gboolean qq_process_keep_alive(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gchar **segments;

	g_return_val_if_fail(data != NULL && data_len != 0, FALSE);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Keep alive reply packet", data, len); */

	/* the last one is 60, don't know what it is */
	if (NULL == (segments = split_data(data, data_len, "\x1f", 6)))
			return TRUE;

	/* segments[0] and segment[1] are all 0x30 ("0") */
	qd->online_total = strtol(segments[2], NULL, 10);
	if(0 == qd->online_total) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Keep alive error"));
	}
	qd->my_ip.s_addr = inet_addr(segments[3]);
	qd->my_port = strtol(segments[4], NULL, 10);

	purple_debug_info("QQ", "keep alive, %s:%d\n",
		inet_ntoa(qd->my_ip), qd->my_port);

	g_strfreev(segments);
	return TRUE;
}

/* For QQ2007/2008 */
void qq_request_get_server(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	raw_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH);
	memset(raw_data, 0, QQ_LOGIN_DATA_LENGTH);

	encrypted_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH + 16);	/* 16 bytes more */

	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, (guint8 *)&qd->redirect_data, sizeof(qd->redirect_data));

	encrypted_len = qq_encrypt(encrypted_data, raw_data, QQ_LOGIN_DATA_LENGTH, qd->ld.init_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.init_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_LOGIN, qd->send_seq, buf, bytes, TRUE);
}

guint16 qq_process_get_server(PurpleConnection *gc, guint8 *rcved, gint rcved_len)
{
	qq_data *qd;
	guint8 *data;
	gint data_len;
	qq_redirect_data *redirect;

	g_return_val_if_fail (gc != NULL && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	/* May use password_twice_md5 in the past version like QQ2005*/
	data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.init_key);
	if (data_len < 0) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Can not decrypt get server reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

	if (data_len < sizeof(qq_redirect_data)) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Can not decrypt get server reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

	redirect = (qq_redirect_data *)data;
	if (redirect->ret == 0) {
		memset(&qd->redirect_data, 0, sizeof(qd->redirect_data));
		qd->redirect_ip.s_addr = 0;
		return QQ_LOGIN_REPLY_OK;
	}

	g_memmove(&qd->redirect_data, redirect, sizeof(qd->redirect_data));
	g_memmove(&qd->redirect_ip, &redirect->ip, sizeof(qd->redirect_ip));
	return QQ_LOGIN_REPLY_REDIRECT;
}

void qq_request_token_ex(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 16);
	memset(raw_data, 0, MAX_PACKET_SIZE - 16);

	encrypted_data = g_newa(guint8, MAX_PACKET_SIZE);	/* 16 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 3); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put8(raw_data + bytes, 0); 		/* fragment index */
	bytes += qq_put16(raw_data + bytes, 0); 	/* captcha token */

	encrypted_len = qq_encrypt(encrypted_data, raw_data, QQ_LOGIN_DATA_LENGTH, qd->ld.init_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.init_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);
}

void qq_request_token_ex_next(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 16);
	memset(raw_data, 0, MAX_PACKET_SIZE - 16);

	encrypted_data = g_newa(guint8, MAX_PACKET_SIZE);	/* 16 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 3); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put8(raw_data + bytes, qd->captcha.next_index); 		/* fragment index */
	bytes += qq_put16(raw_data + bytes, qd->captcha.token_len); 	/* captcha token */
	bytes += qq_putdata(raw_data + bytes, qd->captcha.token, qd->captcha.token_len);

	encrypted_len = qq_encrypt(encrypted_data, raw_data, QQ_LOGIN_DATA_LENGTH, qd->ld.init_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.init_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);

	purple_connection_update_progress(gc, _("Requesting captcha ..."), 3, QQ_CONNECT_STEPS);
}

static void request_token_ex_code(PurpleConnection *gc,
		guint8 *token, guint16 token_len, guint8 *code, guint16 code_len)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);
	g_return_if_fail(code != NULL && code_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 16);
	memset(raw_data, 0, MAX_PACKET_SIZE - 16);

	encrypted_data = g_newa(guint8, MAX_PACKET_SIZE);	/* 16 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 4); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put16(raw_data + bytes, code_len);
	bytes += qq_putdata(raw_data + bytes, code, code_len);
	bytes += qq_put16(raw_data + bytes, qd->captcha.token_len); 	/* captcha token */
	bytes += qq_putdata(raw_data + bytes, qd->captcha.token, qd->captcha.token_len);

	encrypted_len = qq_encrypt(encrypted_data, raw_data, QQ_LOGIN_DATA_LENGTH, qd->ld.init_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.init_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);

	purple_connection_update_progress(gc, _("Checking code of  captcha ..."), 3, QQ_CONNECT_STEPS);
}

typedef struct {
	PurpleConnection *gc;
	guint8 *token;
	guint16 token_len;
} qq_captcha_request;

static void captcha_request_destory(qq_captcha_request *captcha_req)
{
	g_return_if_fail(captcha_req != NULL);
	if (captcha_req->token) g_free(captcha_req->token);
	g_free(captcha_req);
}

static void captcha_input_cancel_cb(qq_captcha_request *captcha_req,
		PurpleRequestFields *fields)
{
	captcha_request_destory(captcha_req);

	purple_connection_error_reason(captcha_req->gc,
			PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
			_("Failed captcha verify"));
}

static void captcha_input_ok_cb(qq_captcha_request *captcha_req,
		PurpleRequestFields *fields)
{
	gchar *code;

	g_return_if_fail(captcha_req != NULL && captcha_req->gc != NULL);

	code = utf8_to_qq(
			purple_request_fields_get_string(fields, "captcha_code"),
			QQ_CHARSET_DEFAULT);

	if (strlen(code) <= 0) {
		captcha_input_cancel_cb(captcha_req, fields);
		return;
	}

	request_token_ex_code(captcha_req->gc,
			captcha_req->token, captcha_req->token_len,
			(guint8 *)code, strlen(code));

	captcha_request_destory(captcha_req);
}

void qq_captcha_input_dialog(PurpleConnection *gc,qq_captcha_data *captcha)
{
	PurpleAccount *account;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	qq_captcha_request *captcha_req;

	g_return_if_fail(captcha->token != NULL && captcha->token_len > 0);
	g_return_if_fail(captcha->data != NULL && captcha->data_len > 0);

	captcha_req = g_new0(qq_captcha_request, 1);
	captcha_req->gc = gc;
	captcha_req->token = g_new0(guint8, captcha->token_len);
	g_memmove(captcha_req->token, captcha->token, captcha->token_len);
	captcha_req->token_len = captcha->token_len;

	account = purple_connection_get_account(gc);

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_image_new("captcha_img",
			_("Captcha Image"), (char *)captcha->data, captcha->data_len);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("captcha_code",
			_("Enter code"), "", FALSE);
	purple_request_field_string_set_masked(field, FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(account,
		_("QQ Captcha Verifing"),
		_("QQ Captcha Verifing"),
		_("Please fill code according to image"),
		fields,
		_("OK"), G_CALLBACK(captcha_input_ok_cb),
		_("Cancel"), G_CALLBACK(captcha_input_cancel_cb),
		purple_connection_get_account(gc), NULL, NULL,
		captcha_req);
}

guint8 qq_process_token_ex(PurpleConnection *gc, guint8 *buf, gint buf_len)
{
	qq_data *qd;
	int bytes;
	guint8 ret;
	guint8 sub_cmd;
	guint8 reply;
	guint16 captcha_len;
	guint8 curr_index;

	g_return_val_if_fail(buf != NULL && buf_len != 0, -1);

	g_return_val_if_fail(gc != NULL  && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

	ret = buf[0];

	bytes = 0;
	bytes += qq_get8(&sub_cmd, buf + bytes);
	bytes += 2;
	bytes += qq_get8(&reply, buf + bytes);

	bytes += qq_get16(&(qd->ld.token_ex_len), buf + bytes);
	if (qd->ld.token_ex != NULL)	g_free(qd->ld.token_ex);
	qd->ld.token_ex = g_new0(guint8, qd->ld.token_ex_len);
	bytes += qq_getdata(qd->ld.token_ex, qd->ld.token_ex_len , buf + bytes);

	if(reply != 1)
	{
		purple_debug_info("QQ", "All captchaes is verified\n");
		return QQ_LOGIN_REPLY_OK;
	}

	bytes += qq_get16(&captcha_len, buf + bytes);

	qd->captcha.data = g_realloc(qd->captcha.data, qd->captcha.data_len + captcha_len);
	bytes += qq_getdata(qd->captcha.data + qd->captcha.data_len, captcha_len, buf + bytes);
	qd->captcha.data_len += captcha_len;

	bytes += qq_get8(&curr_index, buf + bytes);
	bytes += qq_get8(&qd->captcha.next_index, buf + bytes);

	bytes += qq_get16(&qd->captcha.token_len, buf + bytes);
	qd->captcha.token = g_new0(guint8, qd->captcha.token_len);
	bytes += qq_getdata(qd->captcha.token, qd->captcha.token_len, buf + bytes);

	if(qd->captcha.next_index > 0)
	{
		return QQ_LOGIN_REPLY_NEXT_TOKEN_EX;
	}

	return QQ_LOGIN_REPLY_CAPTCHA_DLG;
}
