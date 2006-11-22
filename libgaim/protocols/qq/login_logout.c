/**
 * @file login_logout.c
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

#include "debug.h"
#include "internal.h"
#include "server.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_status.h"
#include "char_conv.h"
#include "crypt.h"
#include "group.h"
#include "header_info.h"
#include "login_logout.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_proxy.h"
#include "send_core.h"
#include "utils.h"

#define QQ_LOGIN_DATA_LENGTH		    416
#define QQ_LOGIN_REPLY_OK_PACKET_LEN        139
#define QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN  11

#define QQ_REQUEST_LOGIN_TOKEN_REPLY_OK 	0x00

#define QQ_LOGIN_REPLY_OK                   0x00
#define QQ_LOGIN_REPLY_REDIRECT             0x01
#define QQ_LOGIN_REPLY_PWD_ERROR            0x05
#define QQ_LOGIN_REPLY_MISC_ERROR           0xff	/* defined by myself */

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
static const gint8 login_23_51[29] = {
	0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, -122, 
	-52, 76, 53, 44, -45, 115, 108, 20, -10, -10, 
	-81, -61, -6, 51, -92, 1
};

static const gint8 login_53_68[16] = {
	-115, -117, -6, -20, -43, 82, 23, 74, -122, -7, 
	-89, 117, -26, 50, -47, 109
};

static const gint8 login_100_bytes[100] = {
	64, 
	11, 4, 2, 0, 1, 0, 0, 0, 0, 0, 
	3, 9, 0, 0, 0, 0, 0, 0, 0, 1, 
	-23, 3, 1, 0, 0, 0, 0, 0, 1, -13, 
	3, 0, 0, 0, 0, 0, 0, 1, -19, 3, 
	0, 0, 0, 0, 0, 0, 1, -20, 3, 0, 
	0, 0, 0, 0, 0, 3, 5, 0, 0, 0, 
	0, 0, 0, 0, 3, 7, 0, 0, 0, 0, 
	0, 0, 0, 1, -18, 3, 0, 0, 0, 0, 
	0, 0, 1, -17, 3, 0, 0, 0, 0, 0, 
	0, 1, -21, 3, 0, 0, 0, 0, 0
};

/* fixed value, not affected by version, or mac address */
/*
static const guint8 login_53_68[16] = {
	0x82, 0x2a, 0x91, 0xfd, 0xa5, 0xca, 0x67, 0x4c,
	0xac, 0x81, 0x1f, 0x6f, 0x52, 0x05, 0xa7, 0xbf
};
*/


typedef struct _qq_login_reply_ok qq_login_reply_ok_packet;
typedef struct _qq_login_reply_redirect qq_login_reply_redirect_packet;

struct _qq_login_reply_ok {
	guint8 result;
	guint8 *session_key;
	guint32 uid;
	guint8 client_ip[4];	/* those detected by server */
	guint16 client_port;
	guint8 server_ip[4];
	guint16 server_port;
	time_t login_time;
	guint8 unknown1[26];
	guint8 unknown_server1_ip[4];
	guint16 unknown_server1_port;
	guint8 unknown_server2_ip[4];
	guint16 unknown_server2_port;
	guint16 unknown2;	/* 0x0001 */
	guint16 unknown3;	/* 0x0000 */
	guint8 unknown4[32];
	guint8 unknown5[12];
	guint8 last_client_ip[4];
	time_t last_login_time;
	guint8 unknown6[8];
};

struct _qq_login_reply_redirect {
	guint8 result;
	guint32 uid;
	guint8 new_server_ip[4];
	guint16 new_server_port;
};

extern gint			/* defined in send_core.c */
 _create_packet_head_seq(guint8 *buf,
			 guint8 **cursor, GaimConnection *gc, guint16 cmd, gboolean is_auto_seq, guint16 *seq);
extern gint			/* defined in send_core.c */
 _qq_send_packet(GaimConnection *gc, guint8 *buf, gint len, guint16 cmd);

/* It is fixed to 16 bytes 0x01 for QQ2003, 
 * Any value works (or a random 16 bytes string) */
static guint8 *_gen_login_key(void)
{
	return (guint8 *) g_strnfill(QQ_KEY_LENGTH, 0x01);
}

/* process login reply which says OK */
static gint _qq_process_login_ok(GaimConnection *gc, guint8 *data, gint len)
{
	gint bytes;
	guint8 *cursor;
	qq_data *qd;
	qq_login_reply_ok_packet lrop;

	qd = (qq_data *) gc->proto_data;
	cursor = data;
	bytes = 0;

	/* 000-000: reply code */
	bytes += read_packet_b(data, &cursor, len, &lrop.result);
	/* 001-016: session key */
	lrop.session_key = g_memdup(cursor, QQ_KEY_LENGTH);
	cursor += QQ_KEY_LENGTH;
	bytes += QQ_KEY_LENGTH;
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Get session_key done\n");
	/* 017-020: login uid */
	bytes += read_packet_dw(data, &cursor, len, &lrop.uid);
	/* 021-024: server detected user public IP */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.client_ip, 4);
	/* 025-026: server detected user port */
	bytes += read_packet_w(data, &cursor, len, &lrop.client_port);
	/* 027-030: server detected itself ip 127.0.0.1 ? */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.server_ip, 4);
	/* 031-032: server listening port */
	bytes += read_packet_w(data, &cursor, len, &lrop.server_port);
	/* 033-036: login time for current session */
	bytes += read_packet_dw(data, &cursor, len, (guint32 *) &lrop.login_time);
	/* 037-062: 26 bytes, unknown */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown1, 26);
	/* 063-066: unknown server1 ip address */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown_server1_ip, 4);
	/* 067-068: unknown server1 port */
	bytes += read_packet_w(data, &cursor, len, &lrop.unknown_server1_port);
	/* 069-072: unknown server2 ip address */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown_server2_ip, 4);
	/* 073-074: unknown server2 port */
	bytes += read_packet_w(data, &cursor, len, &lrop.unknown_server2_port);
	/* 075-076: 2 bytes unknown */
	bytes += read_packet_w(data, &cursor, len, &lrop.unknown2);
	/* 077-078: 2 bytes unknown */
	bytes += read_packet_w(data, &cursor, len, &lrop.unknown3);
	/* 079-110: 32 bytes unknown */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown4, 32);
	/* 111-122: 12 bytes unknown */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown5, 12);
	/* 123-126: login IP of last session */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.last_client_ip, 4);
	/* 127-130: login time of last session */
	bytes += read_packet_dw(data, &cursor, len, (guint32 *) &lrop.last_login_time);
	/* 131-138: 8 bytes unknown */
	bytes += read_packet_data(data, &cursor, len, (guint8 *) &lrop.unknown6, 8);

	if (bytes != QQ_LOGIN_REPLY_OK_PACKET_LEN) {	/* fail parsing login info */
		gaim_debug(GAIM_DEBUG_WARNING, "QQ",
			   "Fail parsing login info, expect %d bytes, read %d bytes\n",
			   QQ_LOGIN_REPLY_OK_PACKET_LEN, bytes);
	}			/* but we still go on as login OK */

	qd->session_key = lrop.session_key;
	qd->session_md5 = _gen_session_md5(qd->uid, qd->session_key);
	qd->my_ip = gen_ip_str(lrop.client_ip);
	qd->my_port = lrop.client_port;
	qd->login_time = lrop.login_time;
	qd->last_login_time = lrop.last_login_time;
	qd->last_login_ip = gen_ip_str(lrop.last_client_ip);

	gaim_connection_set_state(gc, GAIM_CONNECTED);
	qd->logged_in = TRUE;	/* must be defined after sev_finish_login */

	/* now initiate QQ Qun, do it first as it may take longer to finish */
	qq_group_init(gc);

	/* Now goes on updating my icon/nickname, not showing info_window */
	qd->modifying_face = FALSE;
	qq_send_packet_get_info(gc, qd->uid, FALSE);

	qq_send_packet_change_status(gc);

	/* refresh buddies */
	qq_send_packet_get_buddies_list(gc, QQ_FRIENDS_LIST_POSITION_START);
	/* refresh groups */
	qq_send_packet_get_all_list_with_group(gc, QQ_FRIENDS_LIST_POSITION_START);

	return QQ_LOGIN_REPLY_OK;
}

/* process login reply packet which includes redirected new server address */
static gint _qq_process_login_redirect(GaimConnection *gc, guint8 *data, gint len)
{
	gint bytes, ret;
	guint8 *cursor;
	gchar *new_server_str;
	qq_data *qd;
	qq_login_reply_redirect_packet lrrp;

	qd = (qq_data *) gc->proto_data;
	cursor = data;
	bytes = 0;
	/* 000-000: reply code */
	bytes += read_packet_b(data, &cursor, len, &lrrp.result);
	/* 001-004: login uid */
	bytes += read_packet_dw(data, &cursor, len, &lrrp.uid);
	/* 005-008: redirected new server IP */
	bytes += read_packet_data(data, &cursor, len, lrrp.new_server_ip, 4);
	/* 009-010: redirected new server port */
	bytes += read_packet_w(data, &cursor, len, &lrrp.new_server_port);

	if (bytes != QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail parsing login redirect packet, expect %d bytes, read %d bytes\n",
			   QQ_LOGIN_REPLY_REDIRECT_PACKET_LEN, bytes);
		ret = QQ_LOGIN_REPLY_MISC_ERROR;
	} else {		/* start new connection */
		new_server_str = gen_ip_str(lrrp.new_server_ip);
		gaim_debug(GAIM_DEBUG_WARNING, "QQ",
			   "Redirected to new server: %s:%d\n", new_server_str, lrrp.new_server_port);
		qq_connect(gc->account, new_server_str, lrrp.new_server_port, qd->use_tcp, TRUE);
		g_free(new_server_str);
		ret = QQ_LOGIN_REPLY_REDIRECT;
	}

	return ret;
}

/* process login reply which says wrong password */
static gint _qq_process_login_wrong_pwd(GaimConnection *gc, guint8 *data, gint len)
{
	gchar *server_reply, *server_reply_utf8;
	server_reply = g_new0(gchar, len);
	g_memmove(server_reply, data + 1, len - 1);
	server_reply_utf8 = qq_to_utf8(server_reply, QQ_CHARSET_DEFAULT);
	gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Wrong password, server msg in UTF8: %s\n", server_reply_utf8);
	g_free(server_reply);
	g_free(server_reply_utf8);

	return QQ_LOGIN_REPLY_PWD_ERROR;
}

/* request before login */
void qq_send_packet_request_login_token(GaimConnection *gc)
{
	qq_data *qd;
	guint8 *buf, *cursor;
	guint16 seq_ret;
	gint bytes;

	qd = (qq_data *) gc->proto_data;
	buf = g_newa(guint8, MAX_PACKET_SIZE);

	cursor = buf;
	bytes = 0;
	bytes += _create_packet_head_seq(buf, &cursor, gc, QQ_CMD_REQUEST_LOGIN_TOKEN, TRUE, &seq_ret);
	bytes += create_packet_dw(buf, &cursor, qd->uid);
	bytes += create_packet_b(buf, &cursor, 0);
	bytes += create_packet_b(buf, &cursor, QQ_PACKET_TAIL);

	if (bytes == (cursor - buf))	/* packet creation OK */
		_qq_send_packet(gc, buf, bytes, QQ_CMD_REQUEST_LOGIN_TOKEN);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail create request login token packet\n");
}

/* send login packet to QQ server */
static void qq_send_packet_login(GaimConnection *gc, guint8 token_length, guint8 *token)
{
	qq_data *qd;
	guint8 *buf, *cursor, *raw_data, *encrypted_data;
	guint16 seq_ret;
	gint encrypted_len, bytes;
	gint pos;

	qd = (qq_data *) gc->proto_data;
	buf = g_newa(guint8, MAX_PACKET_SIZE);
	raw_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH);
	encrypted_data = g_newa(guint8, QQ_LOGIN_DATA_LENGTH + 16);	/* 16 bytes more */
	qd->inikey = _gen_login_key();

	/* now generate the encrypted data
	 * 000-015 use pwkey as key to encrypt empty string */
	qq_crypt(ENCRYPT, (guint8 *) "", 0, qd->pwkey, raw_data, &encrypted_len);
	/* 016-016 */
	raw_data[16] = 0x00;
	/* 017-020, used to be IP, now zero */
	*((guint32 *) (raw_data + 17)) = 0x00000000;
	/* 021-022, used to be port, now zero */
	*((guint16 *) (raw_data + 21)) = 0x0000;
	/* 023-051, fixed value, unknown */
	g_memmove(raw_data + 23, login_23_51, 29);
	/* 052-052, login mode */
	raw_data[52] = qd->login_mode;
	/* 053-068, fixed value, maybe related to per machine */
	g_memmove(raw_data + 53, login_53_68, 16);

	/* 069, login token length */
	raw_data[69] = token_length;
	pos = 70;
	/* 070-093, login token, normally 24 bytes */
	g_memmove(raw_data + pos, token, token_length);
	pos += token_length;
	/* 100 bytes unknown */
	g_memmove(raw_data + pos, login_100_bytes, 100);
	pos += 100;
	/* all zero left */
	memset(raw_data+pos, 0, QQ_LOGIN_DATA_LENGTH - pos);

	qq_crypt(ENCRYPT, raw_data, QQ_LOGIN_DATA_LENGTH, qd->inikey, encrypted_data, &encrypted_len);

	cursor = buf;
	bytes = 0;
	bytes += _create_packet_head_seq(buf, &cursor, gc, QQ_CMD_LOGIN, TRUE, &seq_ret);
	bytes += create_packet_dw(buf, &cursor, qd->uid);
	bytes += create_packet_data(buf, &cursor, qd->inikey, QQ_KEY_LENGTH);
	bytes += create_packet_data(buf, &cursor, encrypted_data, encrypted_len);
	bytes += create_packet_b(buf, &cursor, QQ_PACKET_TAIL);

	if (bytes == (cursor - buf))	/* packet creation OK */
		_qq_send_packet(gc, buf, bytes, QQ_CMD_LOGIN);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Fail create login packet\n");
}

void qq_process_request_login_token_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
        qq_data *qd;
	gchar *hex_dump;

        g_return_if_fail(buf != NULL && buf_len != 0);

        qd = (qq_data *) gc->proto_data;

	if (buf[0] == QQ_REQUEST_LOGIN_TOKEN_REPLY_OK) {
		if (buf[1] != buf_len-2) {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", 
					"Malformed login token reply packet. Packet specifies length of %d, actual length is %d\n", buf[1], buf_len-2);
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
					"Attempting to proceed with the actual packet length.\n");
		}
		hex_dump = hex_dump_to_str(buf+2, buf_len-2);
		gaim_debug(GAIM_DEBUG_INFO, "QQ",
                                   "<<< got a token with %d bytes -> [default] decrypt and dump\n%s", buf_len-2, hex_dump);
		qq_send_packet_login(gc, buf_len-2, buf+2);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Unknown request login token reply code : %d\n", buf[0]);
		hex_dump = hex_dump_to_str(buf, buf_len);
                gaim_debug(GAIM_DEBUG_WARNING, "QQ",
           		           ">>> %d bytes -> [default] decrypt and dump\n%s",
	                           buf_len, hex_dump);
               		try_dump_as_gbk(buf, buf_len);
		gaim_connection_error(gc, _("Request login token error!"));
	}		
	g_free(hex_dump);
}

/* send logout packets to QQ server */
void qq_send_packet_logout(GaimConnection *gc)
{
	gint i;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	for (i = 0; i < 4; i++)
		qq_send_cmd(gc, QQ_CMD_LOGOUT, FALSE, 0xffff, FALSE, qd->pwkey, QQ_KEY_LENGTH);

	qd->logged_in = FALSE;	/* update login status AFTER sending logout packets */
}

/* process the login reply packet */
void qq_process_login_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	gint len, ret, bytes;
	guint8 *data;
	qq_data *qd;
	gchar *hex_dump;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->pwkey, data, &len)) {
		/* should be able to decrypt with pwkey */
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Decrypt login reply packet with pwkey, %d bytes\n", len);
		if (data[0] == QQ_LOGIN_REPLY_OK) {
			ret = _qq_process_login_ok(gc, data, len);
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Unknown login reply code : %d\n", data[0]);
			ret = QQ_LOGIN_REPLY_MISC_ERROR;
		}
	} else {		/* decrypt with pwkey error */
		len = buf_len;	/* reset len, decrypt will fail if len is too short */
		if (qq_crypt(DECRYPT, buf, buf_len, qd->inikey, data, &len)) {
			/* decrypt ok with inipwd, it might be password error */
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", 
					"Decrypt login reply packet with inikey, %d bytes\n", len);
			bytes = 0;
			switch (data[0]) {
			case QQ_LOGIN_REPLY_REDIRECT:
				ret = _qq_process_login_redirect(gc, data, len);
				break;
			case QQ_LOGIN_REPLY_PWD_ERROR:
				ret = _qq_process_login_wrong_pwd(gc, data, len);
				break;
			default:
				gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Unknown reply code: %d\n", data[0]);
				hex_dump = hex_dump_to_str(data, len);
		                gaim_debug(GAIM_DEBUG_WARNING, "QQ",
                		           ">>> %d bytes -> [default] decrypt and dump\n%s",
		                           buf_len, hex_dump);
				g_free(hex_dump);
                		try_dump_as_gbk(data, len);

				ret = QQ_LOGIN_REPLY_MISC_ERROR;
			}
		} else {	/* no idea how to decrypt */
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "No idea how to decrypt login reply\n");
			ret = QQ_LOGIN_REPLY_MISC_ERROR;
		}
	}

	switch (ret) {
	case QQ_LOGIN_REPLY_PWD_ERROR:
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc, _("Incorrect password."));
		break;
	case QQ_LOGIN_REPLY_MISC_ERROR:
		gaim_connection_error(gc, _("Unable to login, check debug log"));
		break;
	case QQ_LOGIN_REPLY_OK:
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Login replys OK, everything is fine\n");
		break;
	case QQ_LOGIN_REPLY_REDIRECT:
		/* the redirect has been done in _qq_process_login_reply */
		break;
	default:{;
		}
	}
}
