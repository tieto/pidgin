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

#include "internal.h"
#include "debug.h"
#include "server.h"
#include "cipher.h"
#include "request.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "char_conv.h"
#include "qq_crypt.h"
#include "group.h"
#include "qq_define.h"
#include "qq_network.h"
#include "qq_base.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_network.h"
#include "utils.h"

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
	qq_data *qd;
	gint bytes;

	guint8 ret;
	guint32 uid;
	struct in_addr ip;
	guint16 port;
	struct tm *tm_local;

	qd = (qq_data *) gc->proto_data;
	/* qq_show_packet("Login reply", data, len); */

	if (len < 148) {
		qq_show_packet("Login reply OK, but length < 139", data, len);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
				_("Unable to decrypt server reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

	bytes = 0;
	bytes += qq_get8(&ret, data + bytes);
	bytes += qq_getdata(qd->session_key, sizeof(qd->session_key), data + bytes);
	get_session_md5(qd->session_md5, qd->uid, qd->session_key);
	purple_debug_info("QQ", "Got session_key\n");
	bytes += qq_get32(&uid, data + bytes);
	if (uid != qd->uid) {
		purple_debug_warning("QQ", "My uid in login reply is %u, not %u\n", uid, qd->uid);
	}
	bytes += qq_getIP(&qd->my_ip, data + bytes);
	bytes += qq_get16(&qd->my_port, data + bytes);
	purple_debug_info("QQ", "Internet IP: %s, %d\n", inet_ntoa(qd->my_ip), qd->my_port);

	bytes += qq_getIP(&qd->my_local_ip, data + bytes);
	bytes += qq_get16(&qd->my_local_port, data + bytes);
	purple_debug_info("QQ", "Local IP: %s, %d\n", inet_ntoa(qd->my_local_ip), qd->my_local_port);

	bytes += qq_getime(&qd->login_time, data + bytes);
	tm_local = localtime(&qd->login_time);
	purple_debug_info("QQ", "Login time: %d-%d-%d, %d:%d:%d\n",
			(1900 +tm_local->tm_year), (1 + tm_local->tm_mon), tm_local->tm_mday,
			tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec);
	/* skip unknown 2 bytes, 0x(03 0a) */
	bytes += 2;
	/* skip unknown 24 bytes, maybe token to access Qun shared files */
	bytes += 24;
	/* unknow ip and port */
	bytes += qq_getIP(&ip, data + bytes);
	bytes += qq_get16(&port, data + bytes);
	purple_debug_info("QQ", "Unknow IP: %s, %d\n", inet_ntoa(ip), port);
	/* unknow ip and port */
	bytes += qq_getIP(&ip, data + bytes);
	bytes += qq_get16(&port, data + bytes);
	purple_debug_info("QQ", "Unknow IP: %s, %d\n", inet_ntoa(ip), port);
	/* unknown 4 bytes, 0x(00 81 00 00)*/
	bytes += 4;
	/* skip unknown 32 bytes, maybe key to access QQ Home */
	bytes += 32;
	/* skip unknown 16 bytes, 0x(00 00 00 00 00 00 00 00 00 00 00 40 00 00 00 00) */
	bytes += 16;
	/* time */
	bytes += qq_getime(&qd->last_login_time[0], data + bytes);
	tm_local = localtime(&qd->last_login_time[0]);
	purple_debug_info("QQ", "Last login time: %d-%d-%d, %d:%d:%d\n",
			(1900 +tm_local->tm_year), (1 + tm_local->tm_mon), tm_local->tm_mday,
			tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec);
	/* unknow time */
	g_return_val_if_fail(sizeof(qd->last_login_time) / sizeof(time_t) > 1, QQ_LOGIN_REPLY_OK);
	bytes += qq_getime(&qd->last_login_time[1], data + bytes);
	tm_local = localtime(&qd->last_login_time[1]);
	purple_debug_info("QQ", "Time: %d-%d-%d, %d:%d:%d\n",
			(1900 +tm_local->tm_year), (1 + tm_local->tm_mon), tm_local->tm_mday,
			tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec);

	g_return_val_if_fail(sizeof(qd->last_login_time) / sizeof(time_t) > 2, QQ_LOGIN_REPLY_OK);
	bytes += qq_getime(&qd->last_login_time[2], data + bytes);
	tm_local = localtime(&qd->last_login_time[2]);
	purple_debug_info("QQ", "Time: %d-%d-%d, %d:%d:%d\n",
			(1900 +tm_local->tm_year), (1 + tm_local->tm_mon), tm_local->tm_mday,
			tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec);
	/* unknow 9 bytes, 0x(00 0a 00 0a 01 00 00 0e 10) */

	if (len > 148) {
		qq_show_packet("Login reply OK, but length > 139", data, len);
	}
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


	if (len < 11) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
				_("Unable to decrypt server reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

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

	if (len > 11) {
		purple_debug_error("QQ", "Login redirect more than expected %d bytes, read %d bytes\n", 11, bytes);
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
	guint8 *encrypted;
	gint encrypted_len;

	/* for QQ 2005? copy from lumaqq */
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

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	bytes = 0;
	/* now generate the encrypted data
	 * 000-015 use password_twice_md5 as key to encrypt empty string */
	encrypted_len = qq_encrypt(encrypted, (guint8 *) "", 0, qd->ld.pwd_twice_md5);
	g_return_if_fail(encrypted_len == 16);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);

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
	memset(raw_data + bytes, 0, 416 - bytes);
	bytes = 416;

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_LOGIN, qd->send_seq, buf, bytes, TRUE);
}

guint8 qq_process_token(PurpleConnection *gc, guint8 *buf, gint buf_len)
{
	qq_data *qd;
	gint bytes;
	guint8 ret;
	guint8 token_len;
	gchar *msg;

	g_return_val_if_fail(buf != NULL && buf_len != 0, QQ_LOGIN_REPLY_ERR);

	g_return_val_if_fail(gc != NULL  && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&ret, buf + bytes);
	bytes += qq_get8(&token_len, buf + bytes);

	if (ret != QQ_LOGIN_REPLY_OK) {
		qq_show_packet("Failed requesting token", buf, buf_len);

		msg = g_strdup_printf( _("Failed requesting token, 0x%02X"), ret );
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
				msg);
		g_free(msg);
		return QQ_LOGIN_REPLY_ERR;
	}

	if (bytes + token_len < buf_len) {
		msg = g_strdup_printf( _("Invalid token len, %d"), token_len);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
				msg);
		g_free(msg);
		return QQ_LOGIN_REPLY_ERR;
	}

	if (bytes + token_len > buf_len) {
		purple_debug_info("QQ", "Extra token data, %d %d\n", token_len, buf_len - bytes);
	}
	/* qq_show_packet("Got token", buf + bytes, buf_len - bytes); */

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

/* fixed value, not affected by version, or mac address */
/*
static const guint8 login_53_68[16] = {
	0x82, 0x2a, 0x91, 0xfd, 0xa5, 0xca, 0x67, 0x4c,
	0xac, 0x81, 0x1f, 0x6f, 0x52, 0x05, 0xa7, 0xbf
};
*/

/* process the login reply packet */
guint8 qq_process_login( PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint8 ret = data[0];
	gchar *msg, *msg_utf8;
	gchar *error;
	PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	qd = (qq_data *) gc->proto_data;

	switch (ret) {
		case QQ_LOGIN_REPLY_OK:
			purple_debug_info("QQ", "Login OK\n");
			return process_login_ok(gc, data, data_len);
		case QQ_LOGIN_REPLY_REDIRECT:
			purple_debug_info("QQ", "Redirect new server\n");
			return process_login_redirect(gc, data, data_len);

		case 0x0A:		/* extend redirect used in QQ2006 */
			error = g_strdup( _("Redirect_EX is not currently supported") );
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			break;
		case 0x05:		/* invalid password */
			if (!purple_account_get_remember_password(gc->account)) {
				purple_account_set_password(gc->account, NULL);
			}
			error = g_strdup( _("Incorrect password"));
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			break;
		case 0x06:		/* need activation */
			error = g_strdup( _("Activation required"));
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			break;

		default:
			qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ", data, data_len,
					">>> [default] decrypt and dump");
			error = g_strdup_printf(
						_("Unknown reply code when logging in (0x%02X)"),
						ret );
			reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
			break;
	}

	msg = g_strndup((gchar *)data + 1, data_len - 1);
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);

	purple_debug_error("QQ", "%s: %s\n", error, msg_utf8);
	purple_connection_error_reason(gc, reason, msg_utf8);

	g_free(error);
	g_free(msg);
	g_free(msg_utf8);
	return QQ_LOGIN_REPLY_ERR;
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
	qq_send_cmd(gc, QQ_CMD_KEEP_ALIVE, raw_data, bytes);
}

/* parse the return ofqq_process_keep_alive keep-alive packet, it includes some system information */
gboolean qq_process_keep_alive(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gchar **segments;

	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(data_len != 0, FALSE);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Keep alive reply packet", data, len); */

	/* the last one is 60, don't know what it is */
	segments = split_data(data, data_len, "\x1f", 6);
	if (segments == NULL)
			return TRUE;

	/* segments[0] and segment[1] are all 0x30 ("0") */
	qd->online_total = strtol(segments[2], NULL, 10);
	if(0 == qd->online_total) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Lost connection with server"));
	}
	qd->my_ip.s_addr = inet_addr(segments[3]);
	qd->my_port = strtol(segments[4], NULL, 10);

	purple_debug_info("QQ", "keep alive, %s:%d\n",
		inet_ntoa(qd->my_ip), qd->my_port);

	g_strfreev(segments);
	return TRUE;
}

void qq_request_keep_alive_2007(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 raw_data[32] = {0};
	gint bytes= 0;
	gchar *uid_str;

	qd = (qq_data *) gc->proto_data;

	/* In fact, we can send whatever we like to server
	 * with this command, server return the same result including
	 * the amount of online QQ users, my ip and port */
	uid_str = g_strdup_printf("%u", qd->uid);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)uid_str, strlen(uid_str));
	qq_send_cmd(gc, QQ_CMD_KEEP_ALIVE, raw_data, bytes);

	g_free(uid_str);
}

gboolean qq_process_keep_alive_2007(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes= 0;
	guint8 ret;

	g_return_val_if_fail(data != NULL && data_len != 0, FALSE);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Keep alive reply packet", data, len); */

	bytes = 0;
	bytes += qq_get8(&ret, data + bytes);
	bytes += qq_get32(&qd->online_total, data + bytes);
	if(0 == qd->online_total) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Lost connection with server"));
	}

	bytes += qq_getIP(&qd->my_ip, data + bytes);
	bytes += qq_get16(&qd->my_port, data + bytes);
	return TRUE;
}

void qq_request_keep_alive_2008(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 raw_data[16] = {0};
	gint bytes= 0;

	qd = (qq_data *) gc->proto_data;

	/* In fact, we can send whatever we like to server
	 * with this command, server return the same result including
	 * the amount of online QQ users, my ip and port */
	bytes += qq_put32(raw_data + bytes, qd->uid);
	bytes += qq_putime(raw_data + bytes, &qd->login_time);
	qq_send_cmd(gc, QQ_CMD_KEEP_ALIVE, raw_data, bytes);
}

gboolean qq_process_keep_alive_2008(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes= 0;
	guint8 ret;
	time_t server_time;
	struct tm *tm_local;

	g_return_val_if_fail(data != NULL && data_len != 0, FALSE);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Keep alive reply packet", data, len); */

	bytes = 0;
	bytes += qq_get8(&ret, data + bytes);
	bytes += qq_get32(&qd->online_total, data + bytes);
	if(0 == qd->online_total) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Lost connection with server"));
	}

	bytes += qq_getIP(&qd->my_ip, data + bytes);
	bytes += qq_get16(&qd->my_port, data + bytes);
	/* skip 2 byytes, 0x(00 3c) */
	bytes += 2;
	bytes += qq_getime(&server_time, data + bytes);
	/* skip 5 bytes, all are 0 */

	purple_debug_info("QQ", "keep alive, %s:%d\n",
		inet_ntoa(qd->my_ip), qd->my_port);

	tm_local = localtime(&server_time);

	if (tm_local != NULL)
		purple_debug_info("QQ", "Server time: %d-%d-%d, %d:%d:%d\n",
				(1900 +tm_local->tm_year), (1 + tm_local->tm_mon), tm_local->tm_mday,
				tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec);
	else
		purple_debug_error("QQ", "Server time could not be parsed\n");

	return TRUE;
}

/* For QQ2007/2008 */
void qq_request_get_server(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	raw_data = g_newa(guint8, 128);
	memset(raw_data, 0, 128);

	encrypted = g_newa(guint8, 128 + 17);	/* 17 bytes more */

	bytes = 0;
	if (qd->redirect == NULL) {
		/* first packet to get server */
		qd->redirect_len = 15;
		qd->redirect = g_realloc(qd->redirect, qd->redirect_len);
		memset(qd->redirect, 0, qd->redirect_len);
	}
	bytes += qq_putdata(raw_data + bytes, qd->redirect, qd->redirect_len);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_GET_SERVER, qd->send_seq, buf, bytes, TRUE);
}

guint16 qq_process_get_server(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	gint bytes;
	guint16 ret;

	g_return_val_if_fail (gc != NULL && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail (data != NULL, QQ_LOGIN_REPLY_ERR);

	/* qq_show_packet("Get Server", data, data_len); */
	bytes = 0;
	bytes += qq_get16(&ret, data + bytes);
	if (ret == 0) {
		/* Notice: do not clear redirect_data here. It will be used in login*/
		qd->redirect_ip.s_addr = 0;
		return QQ_LOGIN_REPLY_OK;
	}

	if (data_len < 15) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
				_("Unable to decrypt server reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

	qd->redirect_len = data_len;
	qd->redirect = g_realloc(qd->redirect, qd->redirect_len);
	qq_getdata(qd->redirect, qd->redirect_len, data);
	/* qq_show_packet("Redirect to", qd->redirect, qd->redirect_len); */

	qq_getIP(&qd->redirect_ip, data + 11);
	purple_debug_info("QQ", "Get server %s\n", inet_ntoa(qd->redirect_ip));
	return QQ_LOGIN_REPLY_REDIRECT;
}

void qq_request_token_ex(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 3); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put8(raw_data + bytes, 0); 		/* fragment index */
	bytes += qq_put16(raw_data + bytes, 0); 	/* captcha token */

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);
}

void qq_request_token_ex_next(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 3); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put8(raw_data + bytes, qd->captcha.next_index); 		/* fragment index */
	bytes += qq_put16(raw_data + bytes, qd->captcha.token_len); 	/* captcha token */
	bytes += qq_putdata(raw_data + bytes, qd->captcha.token, qd->captcha.token_len);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);

	purple_connection_update_progress(gc, _("Requesting captcha"), 3, QQ_CONNECT_STEPS);
}

static void request_token_ex_code(PurpleConnection *gc,
		guint8 *token, guint16 token_len, guint8 *code, guint16 code_len)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);
	g_return_if_fail(code != NULL && code_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, qd->ld.token_len);
	bytes += qq_putdata(raw_data + bytes, qd->ld.token, qd->ld.token_len);
	bytes += qq_put8(raw_data + bytes, 4); 		/* Subcommand */
	bytes += qq_put16(raw_data + bytes, 5);
	bytes += qq_put32(raw_data + bytes, 0);
	bytes += qq_put16(raw_data + bytes, code_len);
	bytes += qq_putdata(raw_data + bytes, code, code_len);
	bytes += qq_put16(raw_data + bytes, qd->ld.token_ex_len); 	/* login token ex */
	bytes += qq_putdata(raw_data + bytes, qd->ld.token_ex, qd->ld.token_ex_len);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_TOKEN_EX, qd->send_seq, buf, bytes, TRUE);

	purple_connection_update_progress(gc, _("Checking captcha"), 3, QQ_CONNECT_STEPS);
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
	purple_connection_error_reason(captcha_req->gc,
			PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
			_("Failed captcha verification"));

	captcha_request_destory(captcha_req);
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
		_("QQ Captcha Verification"),
		_("QQ Captcha Verification"),
		_("Enter the text from the image"),
		fields,
		_("OK"), G_CALLBACK(captcha_input_ok_cb),
		_("Cancel"), G_CALLBACK(captcha_input_cancel_cb),
		purple_connection_get_account(gc), NULL, NULL,
		captcha_req);
}

guint8 qq_process_token_ex(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	int bytes;
	guint8 ret;
	guint8 sub_cmd;
	guint8 reply;
	guint16 captcha_len;
	guint8 curr_index;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	g_return_val_if_fail(gc != NULL  && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	ret = data[0];

	bytes = 0;
	bytes += qq_get8(&sub_cmd, data + bytes); /* 03: ok; 04: need verifying */
	bytes += 2;	/* 0x(00 05) */
	bytes += qq_get8(&reply, data + bytes);

	bytes += qq_get16(&(qd->ld.token_ex_len), data + bytes);
	qd->ld.token_ex = g_realloc(qd->ld.token_ex, qd->ld.token_ex_len);
	bytes += qq_getdata(qd->ld.token_ex, qd->ld.token_ex_len, data + bytes);
	/* qq_show_packet("Get token ex", qd->ld.token_ex, qd->ld.token_ex_len); */

	if(reply != 1)
	{
		purple_debug_info("QQ", "Captcha verified, result %d\n", reply);
		return QQ_LOGIN_REPLY_OK;
	}

	bytes += qq_get16(&captcha_len, data + bytes);

	qd->captcha.data = g_realloc(qd->captcha.data, qd->captcha.data_len + captcha_len);
	bytes += qq_getdata(qd->captcha.data + qd->captcha.data_len, captcha_len, data + bytes);
	qd->captcha.data_len += captcha_len;

	bytes += qq_get8(&curr_index, data + bytes);
	bytes += qq_get8(&qd->captcha.next_index, data + bytes);

	bytes += qq_get16(&qd->captcha.token_len, data + bytes);
	qd->captcha.token = g_realloc(qd->captcha.token, qd->captcha.token_len);
	bytes += qq_getdata(qd->captcha.token, qd->captcha.token_len, data + bytes);
	/* qq_show_packet("Get captcha token", qd->captcha.token, qd->captcha.token_len); */

	purple_debug_info("QQ", "Request next captcha %d, new %d, total %d\n",
			qd->captcha.next_index, captcha_len, qd->captcha.data_len);
	if(qd->captcha.next_index > 0)
	{
		return QQ_LOGIN_REPLY_NEXT_TOKEN_EX;
	}

	return QQ_LOGIN_REPLY_CAPTCHA_DLG;
}

/* source copy from gg's common.c */
static guint32 crc32_table[256];
static int crc32_initialized = 0;

static void crc32_make_table()
{
	guint32 h = 1;
	unsigned int i, j;

	memset(crc32_table, 0, sizeof(crc32_table));

	for (i = 128; i; i >>= 1) {
		h = (h >> 1) ^ ((h & 1) ? 0xedb88320L : 0);

		for (j = 0; j < 256; j += 2 * i)
			crc32_table[i + j] = crc32_table[j] ^ h;
	}

	crc32_initialized = 1;
}

static guint32 crc32(guint32 crc, const guint8 *buf, int len)
{
	if (!crc32_initialized)
		crc32_make_table();

	if (!buf || len < 0)
		return crc;

	crc ^= 0xffffffffL;

	while (len--)
		crc = (crc >> 8) ^ crc32_table[(crc ^ *buf++) & 0xff];

	return crc ^ 0xffffffffL;
}

void qq_request_check_pwd(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;
	static guint8 header[] = {
			0x00, 0x5F, 0x00, 0x00, 0x08, 0x04, 0x01, 0xE0
	};
	static guint8 unknown[] = {
			0xDB, 0xB9, 0xF3, 0x0B, 0xF9, 0x13, 0x87, 0xB2,
			0xE6, 0x20, 0x43, 0xBE, 0x53, 0xCA, 0x65, 0x03
	};

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token_ex != NULL && qd->ld.token_ex_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	/* Encrypted password and put in encrypted */
	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, qd->ld.pwd_md5, sizeof(qd->ld.pwd_md5));
	bytes += qq_put16(raw_data + bytes, 0);
	bytes += qq_put16(raw_data + bytes, rand() & 0xffff);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.pwd_twice_md5);

	/* create packet */
	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, header, sizeof(header));
	/* token get from qq_request_token_ex */
	bytes += qq_put8(raw_data + bytes, (guint8)(qd->ld.token_ex_len & 0xff));
	bytes += qq_putdata(raw_data + bytes, qd->ld.token_ex, qd->ld.token_ex_len);
	/* password encrypted */
	bytes += qq_put16(raw_data + bytes, encrypted_len);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);
	/* len of unknown + len of CRC32 */
	bytes += qq_put16(raw_data + bytes, sizeof(unknown) + 4);
	bytes += qq_putdata(raw_data + bytes, unknown, sizeof(unknown));
	bytes += qq_put32(
			raw_data + bytes, crc32(0xFFFFFFFF, unknown, sizeof(unknown)));

	/* put length into first 2 bytes */
	qq_put8(raw_data + 1, bytes - 2);

	/* tail */
	bytes += qq_put16(raw_data + bytes, 0x0003);
	bytes += qq_put8(raw_data + bytes, 0);
	bytes += qq_put8(raw_data + bytes, qd->ld.pwd_md5[1]);
	bytes += qq_put8(raw_data + bytes, qd->ld.pwd_md5[2]);

	/* qq_show_packet("Check password", raw_data, bytes); */
	/* Encrypted by random key*/
	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.random_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	bytes += qq_putdata(buf + bytes, qd->ld.random_key, QQ_KEY_LENGTH);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_CHECK_PWD, qd->send_seq, buf, bytes, TRUE);
}

guint8 qq_process_check_pwd( PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	int bytes;
	guint8 ret;
	gchar *error = NULL;
	guint16 unknow_token_len;
	gchar *msg, *msg_utf8;
	guint16 msg_len;
	PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	g_return_val_if_fail(gc != NULL  && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Check password reply", data, data_len); */

	bytes = 0;
	bytes += qq_get16(&unknow_token_len, data + bytes);	/* maybe total length */
	bytes += qq_get8(&ret, data + bytes);
	bytes += 4; /* 0x(00 00 6d b9) */
	/* unknow_token_len may 0 when not reply ok*/
	bytes += qq_get16(&unknow_token_len, data + bytes);	/* 0x0020 */
	bytes += unknow_token_len;
	bytes += qq_get16(&unknow_token_len, data + bytes);	/* 0x0020 */
	bytes += unknow_token_len;

	if (ret == 0) {
		/* get login_token */
		bytes += qq_get16(&qd->ld.login_token_len, data + bytes);
		if (qd->ld.login_token != NULL) g_free(qd->ld.login_token);
		qd->ld.login_token = g_new0(guint8, qd->ld.login_token_len);
		bytes += qq_getdata(qd->ld.login_token, qd->ld.login_token_len, data + bytes);
		/* qq_show_packet("Get login token", qd->ld.login_token, qd->ld.login_token_len); */

		/* get login_key */
		bytes += qq_getdata(qd->ld.login_key, sizeof(qd->ld.login_key), data + bytes);
		/* qq_show_packet("Get login key", qd->ld.login_key, sizeof(qd->ld.login_key)); */
		return QQ_LOGIN_REPLY_OK;
	}

	switch (ret)
	{
		case 0x34:		/* invalid password */
			if (!purple_account_get_remember_password(gc->account)) {
				purple_account_set_password(gc->account, NULL);
			}
			error = g_strdup(_("Incorrect password"));
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			break;
		case 0x33:		/* need activation */
		case 0x51:		/* need activation */
			error = g_strdup(_("Activation required"));
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			break;
		case 0xBF:		/* uid is not exist */
			error = g_strdup(_("Username does not exist"));
			reason = PURPLE_CONNECTION_ERROR_INVALID_USERNAME;
			break;
		default:
			qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ", data, data_len,
					">>> [default] decrypt and dump");
			error = g_strdup_printf(
						_("Unknown reply when checking password (0x%02X)"),
						ret );
			reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
			break;
	}

	bytes += qq_get16(&msg_len, data + bytes);

	msg = g_strndup((gchar *)data + bytes, msg_len);
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);

	purple_debug_error("QQ", "%s: %s\n", error, msg_utf8);
	purple_connection_error_reason(gc, reason, msg_utf8);

	g_free(error);
	g_free(msg);
	g_free(msg_utf8);
	return QQ_LOGIN_REPLY_ERR;
}

void qq_request_login_2007(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;
	static const guint8 login_1_16[] = {
			0x56, 0x4E, 0xC8, 0xFB, 0x0A, 0x4F, 0xEF, 0xB3,
			0x7A, 0x5D, 0xD8, 0x86, 0x0F, 0xAC, 0xE5, 0x1A
	};
	static const guint8 login_2_16[] = {
			0x5E, 0x22, 0x3A, 0xBE, 0x13, 0xBF, 0xDA, 0x4C,
			0xA9, 0xB7, 0x0B, 0x43, 0x63, 0x51, 0x8E, 0x28
	};
	static const guint8 login_3_83[] = {
			0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x40, 0x01, 0x01, 0x58, 0x83,
			0xD0, 0x00, 0x10, 0x9D, 0x14, 0x64, 0x0A, 0x2E,
			0xE2, 0x11, 0xF7, 0x90, 0xF0, 0xB5, 0x5F, 0x16,
			0xFB, 0x41, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x02, 0x76, 0x3C, 0xEE,
			0x4A, 0x00, 0x10, 0x86, 0x81, 0xAD, 0x1F, 0xC8,
			0xC9, 0xCC, 0xCF, 0xCA, 0x9F, 0xFF, 0x88, 0xC0,
			0x5C, 0x88, 0xD5
	};
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	/* Encrypted password and put in encrypted */
	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, qd->ld.pwd_md5, sizeof(qd->ld.pwd_md5));
	bytes += qq_put16(raw_data + bytes, 0);
	bytes += qq_put16(raw_data + bytes, 0xffff);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.pwd_twice_md5);

	/* create packet */
	bytes = 0;
	bytes += qq_put16(raw_data + bytes, 0);		/* Unknow */
	/* password encrypted */
	bytes += qq_put16(raw_data + bytes, encrypted_len);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);
	/* put data which NULL string encrypted by key pwd_twice_md5 */
	encrypted_len = qq_encrypt(encrypted, (guint8 *) "", 0, qd->ld.pwd_twice_md5);
	g_return_if_fail(encrypted_len == 16);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);
	/* unknow fill 0 */
	memset(raw_data + bytes, 0, 19);
	bytes += 19;
	bytes += qq_putdata(raw_data + bytes, login_1_16, sizeof(login_1_16));

	bytes += qq_put8(raw_data + bytes, rand() & 0xff);
	bytes += qq_put8(raw_data + bytes, qd->login_mode);
	/* unknow 10 bytes zero filled*/
	memset(raw_data + bytes, 0, 10);
	bytes += 10;
	/* redirect data, 15 bytes */
	/* qq_show_packet("Redirect", qd->redirect, qd->redirect_len); */
	bytes += qq_putdata(raw_data + bytes, qd->redirect, qd->redirect_len);
	/* unknow fill */
	bytes += qq_putdata(raw_data + bytes, login_2_16, sizeof(login_2_16));
	/* captcha token get from qq_process_token_ex */
	bytes += qq_put8(raw_data + bytes, (guint8)(qd->ld.token_ex_len & 0xff));
	bytes += qq_putdata(raw_data + bytes, qd->ld.token_ex, qd->ld.token_ex_len);
	/* unknow fill */
	bytes += qq_putdata(raw_data + bytes, login_3_83, sizeof(login_3_83));
	memset(raw_data + bytes, 0, 332 - sizeof(login_3_83));
	bytes += 332 - sizeof(login_3_83);

	/* qq_show_packet("Login", raw_data, bytes); */

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.login_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	/* logint token get from qq_process_check_pwd_2007 */
	bytes += qq_put16(buf + bytes, qd->ld.login_token_len);
	bytes += qq_putdata(buf + bytes, qd->ld.login_token, qd->ld.login_token_len);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_LOGIN, qd->send_seq, buf, bytes, TRUE);
}

/* process the login reply packet */
guint8 qq_process_login_2007( PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	gint bytes;
	guint8 ret;
	guint32 uid;
	gchar *error;
	gchar *msg;
	gchar *msg_utf8;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&ret, data + bytes);
	if (ret != 0) {
		msg = g_strndup((gchar *)data + bytes, data_len - bytes);
		msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		g_free(msg);

		switch (ret) {
			case 0x05:
				purple_debug_error("QQ", "Server busy for %s\n", msg_utf8);
				return QQ_LOGIN_REPLY_REDIRECT;
			case 0x0A:
				/* 0a 2d 9a 4b 9a 01 01 00 00 00 05 00 00 00 00 79 0e 5f fd */
				/* Missing get server before login*/
			default:
				error = g_strdup_printf(
						_("Unknown reply code when logging in (0x%02X):\n%s"),
						ret, msg_utf8);
				break;
		}

		purple_debug_error("QQ", "%s\n", error);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_OTHER_ERROR,
				error);

		qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ", data, data_len, error);

		g_free(error);
		g_free(msg_utf8);
		return QQ_LOGIN_REPLY_ERR;
	}

	bytes += qq_getdata(qd->session_key, sizeof(qd->session_key), data + bytes);
	purple_debug_info("QQ", "Got session_key\n");
	get_session_md5(qd->session_md5, qd->uid, qd->session_key);

	bytes += qq_get32(&uid, data + bytes);
	if (uid != qd->uid) {
		purple_debug_warning("QQ", "My uid in login reply is %u, not %u\n", uid, qd->uid);
	}
	bytes += qq_getIP(&qd->my_ip, data + bytes);
	bytes += qq_get16(&qd->my_port, data + bytes);
	bytes += qq_getIP(&qd->my_local_ip, data + bytes);
	bytes += qq_get16(&qd->my_local_port, data + bytes);
	bytes += qq_getime(&qd->login_time, data + bytes);
	/* skip unknow 50 byte */
	bytes += 50;
	/* skip client key 32 byte */
	bytes += 32;
	/* skip unknow 12 byte */
	bytes += 12;
	/* last login */
	bytes += qq_getIP(&qd->last_login_ip, data + bytes);
	bytes += qq_getime(&qd->last_login_time[0], data + bytes);
	purple_debug_info("QQ", "Last Login: %s, %s\n",
			inet_ntoa(qd->last_login_ip), ctime(&qd->last_login_time[0]));
	return QQ_LOGIN_REPLY_OK;
}

void qq_request_login_2008(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *raw_data;
	gint bytes;
	guint8 *encrypted;
	gint encrypted_len;
	guint8 index, count;

	static const guint8 login_1_16[] = {
			0xD2, 0x41, 0x75, 0x12, 0xC2, 0x86, 0x57, 0x10,
			0x78, 0x57, 0xDC, 0x24, 0x8C, 0xAA, 0x8F, 0x4E
	};

	static const guint8 login_2_16[] = {
			0x94, 0x0B, 0x73, 0x7A, 0xA2, 0x51, 0xF0, 0x4B,
			0x95, 0x2F, 0xC6, 0x0A, 0x5B, 0xF6, 0x76, 0x52
	};
	static const guint8 login_3_18[] = {
			0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x40, 0x01, 0x1b, 0x02, 0x84,
			0x50, 0x00
	};
	static const guint8 login_4_16[] = {
			0x2D, 0x49, 0x15, 0x55, 0x78, 0xFC, 0xF3, 0xD4,
			0x53, 0x55, 0x60, 0x9C, 0x37, 0x9F, 0xE9, 0x59
	};
	static const guint8 login_5_6[] = {
			0x02, 0x68, 0xe8, 0x07, 0x83, 0x00
	};
	static const guint8 login_6_16[] = {
			0x3B, 0xCE, 0x43, 0xF1, 0x8B, 0xA4, 0x2B, 0xB5,
			0xB3, 0x51, 0x57, 0xF7, 0x06, 0x4B, 0x18, 0xFC
	};
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_if_fail(qd->ld.token != NULL && qd->ld.token_len > 0);

	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 17);
	memset(raw_data, 0, MAX_PACKET_SIZE - 17);

	encrypted = g_newa(guint8, MAX_PACKET_SIZE);	/* 17 bytes more */

	/* Encrypted password and put in encrypted */
	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, qd->ld.pwd_md5, sizeof(qd->ld.pwd_md5));
	bytes += qq_put16(raw_data + bytes, 0);
	bytes += qq_put16(raw_data + bytes, 0xffff);

	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.pwd_twice_md5);

	/* create packet */
	bytes = 0;
	bytes += qq_put16(raw_data + bytes, 0);		/* Unknow */
	/* password encrypted */
	bytes += qq_put16(raw_data + bytes, encrypted_len);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);
	/* put data which NULL string encrypted by key pwd_twice_md5 */
	encrypted_len = qq_encrypt(encrypted, (guint8 *) "", 0, qd->ld.pwd_twice_md5);
	g_return_if_fail(encrypted_len == 16);
	bytes += qq_putdata(raw_data + bytes, encrypted, encrypted_len);
	/* unknow 19 bytes zero filled*/
	memset(raw_data + bytes, 0, 19);
	bytes += 19;
	bytes += qq_putdata(raw_data + bytes, login_1_16, sizeof(login_1_16));

	index = rand() % 3;	  /* can be set as 1 */
	for( count = 0; count < encrypted_len;  count++ )
		index ^= encrypted[count];
	for( count = 0; count < sizeof(login_1_16);  count++ )
		index ^= login_1_16[count];
	bytes += qq_put8(raw_data + bytes, index);	/* random in QQ 2007*/

	bytes += qq_put8(raw_data + bytes, qd->login_mode);
	/* unknow 10 bytes zero filled*/
	memset(raw_data + bytes, 0, 10);
	bytes += 10;
	/* redirect data, 15 bytes */
	bytes += qq_putdata(raw_data + bytes, qd->redirect, qd->redirect_len);
	/* unknow fill */
	bytes += qq_putdata(raw_data + bytes, login_2_16, sizeof(login_2_16));
	/* captcha token get from qq_process_token_ex */
	bytes += qq_put8(raw_data + bytes, (guint8)(qd->ld.token_ex_len & 0xff));
	bytes += qq_putdata(raw_data + bytes, qd->ld.token_ex, qd->ld.token_ex_len);
	/* unknow fill */
	bytes += qq_putdata(raw_data + bytes, login_3_18, sizeof(login_3_18));
	bytes += qq_put8(raw_data + bytes , sizeof(login_4_16));
	bytes += qq_putdata(raw_data + bytes, login_4_16, sizeof(login_4_16));
	/* unknow 10 bytes zero filled*/
	memset(raw_data + bytes, 0, 10);
	bytes += 10;
	/* redirect data, 15 bytes */
	bytes += qq_putdata(raw_data + bytes, qd->redirect, qd->redirect_len);
	/* unknow fill */
	bytes += qq_putdata(raw_data + bytes, login_5_6, sizeof(login_5_6));
	bytes += qq_put8(raw_data + bytes , sizeof(login_6_16));
	bytes += qq_putdata(raw_data + bytes, login_6_16, sizeof(login_6_16));
	/* unknow 249 bytes zero filled*/
	memset(raw_data + bytes, 0, 249);
	bytes += 249;

	/* qq_show_packet("Login request", raw_data, bytes); */
	encrypted_len = qq_encrypt(encrypted, raw_data, bytes, qd->ld.login_key);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	bytes = 0;
	/* logint token get from qq_process_check_pwd_2007 */
	bytes += qq_put16(buf + bytes, qd->ld.login_token_len);
	bytes += qq_putdata(buf + bytes, qd->ld.login_token, qd->ld.login_token_len);
	bytes += qq_putdata(buf + bytes, encrypted, encrypted_len);

	qd->send_seq++;
	qq_send_cmd_encrypted(gc, QQ_CMD_LOGIN, qd->send_seq, buf, bytes, TRUE);
}

guint8 qq_process_login_2008( PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	gint bytes;
	guint8 ret;
	guint32 uid;
	gchar *error;
	gchar *msg;
	gchar *msg_utf8;

	g_return_val_if_fail(data != NULL && data_len != 0, QQ_LOGIN_REPLY_ERR);

	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&ret, data + bytes);
	if (ret != 0) {
		msg = g_strndup((gchar *)data + bytes, data_len - bytes);
		msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		g_free(msg);

		switch (ret) {
			case 0x05:
				purple_debug_error("QQ", "Server busy for %s\n", msg_utf8);
				return QQ_LOGIN_REPLY_REDIRECT;
				break;
			default:
				error = g_strdup_printf(
						_("Unknown reply code when logging in (0x%02X):\n%s"),
						ret, msg_utf8);
				break;
		}

		purple_debug_error("QQ", "%s\n", error);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_OTHER_ERROR,
				error);

		qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ", data, data_len, error);

		g_free(error);
		g_free(msg_utf8);
		return QQ_LOGIN_REPLY_ERR;
	}

	bytes += qq_getdata(qd->session_key, sizeof(qd->session_key), data + bytes);
	purple_debug_info("QQ", "Got session_key\n");
	get_session_md5(qd->session_md5, qd->uid, qd->session_key);

	bytes += qq_get32(&uid, data + bytes);
	if (uid != qd->uid) {
		purple_debug_warning("QQ", "My uid in login reply is %u, not %u\n", uid, qd->uid);
	}
	bytes += qq_getIP(&qd->my_ip, data + bytes);
	bytes += qq_get16(&qd->my_port, data + bytes);
	bytes += qq_getIP(&qd->my_local_ip, data + bytes);
	bytes += qq_get16(&qd->my_local_port, data + bytes);
	bytes += qq_getime(&qd->login_time, data + bytes);
	/* skip 1 byte, always 0x03 */
	/* skip 1 byte, login mode */
	bytes = 131;
	bytes += qq_getIP(&qd->last_login_ip, data + bytes);
	bytes += qq_getime(&qd->last_login_time[0], data + bytes);
	purple_debug_info("QQ", "Last Login: %s, %s\n",
			inet_ntoa(qd->last_login_ip), ctime(&qd->last_login_time[0]));
	return QQ_LOGIN_REPLY_OK;
}
