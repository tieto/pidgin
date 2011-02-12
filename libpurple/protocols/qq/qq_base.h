/**
 * file qq_base.h
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

#ifndef _QQ_BASE_H_
#define _QQ_BASE_H_

#include <glib.h>
#include "connection.h"

#define QQ_LOGIN_REPLY_OK				0x00
#define QQ_LOGIN_REPLY_REDIRECT				0x01
/* defined by myself */
#define QQ_LOGIN_REPLY_CAPTCHA_DLG			0xfd
#define QQ_LOGIN_REPLY_NEXT_TOKEN_EX			0xfe
#define QQ_LOGIN_REPLY_ERR				0xff

#define QQ_LOGIN_MODE_NORMAL		0x0a
#define QQ_LOGIN_MODE_AWAY	    	0x1e
#define QQ_LOGIN_MODE_HIDDEN		0x28

#define QQ_UPDATE_ONLINE_INTERVAL   300	/* in sec */

void qq_request_token(PurpleConnection *gc);
guint8 qq_process_token(PurpleConnection *gc, guint8 *buf, gint buf_len);

void qq_request_login(PurpleConnection *gc);
guint8 qq_process_login( PurpleConnection *gc, guint8 *data, gint data_len);

void qq_request_logout(PurpleConnection *gc);

void qq_request_keep_alive(PurpleConnection *gc);
gboolean qq_process_keep_alive(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_request_keep_alive_2007(PurpleConnection *gc);
gboolean qq_process_keep_alive_2007(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_request_keep_alive_2008(PurpleConnection *gc);
gboolean qq_process_keep_alive_2008(guint8 *data, gint data_len, PurpleConnection *gc);

/* for QQ2007/2008 */
void qq_request_get_server(PurpleConnection *gc);
guint16 qq_process_get_server(PurpleConnection *gc, guint8 *rcved, gint rcved_len);

void qq_request_token_ex(PurpleConnection *gc);
void qq_request_token_ex_next(PurpleConnection *gc);
guint8 qq_process_token_ex(PurpleConnection *gc, guint8 *buf, gint buf_len);
void qq_captcha_input_dialog(PurpleConnection *gc,qq_captcha_data *captcha);

void qq_request_check_pwd(PurpleConnection *gc);
guint8 qq_process_check_pwd( PurpleConnection *gc, guint8 *data, gint data_len);

void qq_request_login_2007(PurpleConnection *gc);
guint8 qq_process_login_2007( PurpleConnection *gc, guint8 *data, gint data_len);

void qq_request_login_2008(PurpleConnection *gc);
guint8 qq_process_login_2008( PurpleConnection *gc, guint8 *data, gint data_len);
#endif

