/**
 * @file yahoo.h The Yahoo! protocol plugin
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

#ifndef _YAHOO_H_
#define _YAHOO_H_

#include "prpl.h"

#define YAHOO_PAGER_HOST "scs.msg.yahoo.com"
#define YAHOO_PAGER_PORT 5050
#define YAHOO_PROFILE_URL "http://profiles.yahoo.com/"
#define YAHOO_MAIL_URL "http://mail.yahoo.com/"
#define YAHOO_XFER_HOST "filetransfer.msg.yahoo.com"
#define YAHOO_XFER_PORT 80
#define YAHOO_ROOMLIST_URL "http://insider.msg.yahoo.com/ycontent/"

/* really we should get the list of servers from
 http://update.messenger.yahoo.co.jp/servers.html */
#define YAHOOJP_PAGER_HOST "cs.yahoo.co.jp"
#define YAHOOJP_PROFILE_URL "http://profiles.yahoo.co.jp/"
#define YAHOOJP_MAIL_URL "http://mail.yahoo.co.jp/"
#define YAHOOJP_XFER_HOST "filetransfer.msg.yahoo.co.jp"
#define YAHOOJP_WEBCAM_HOST "wc.yahoo.co.jp"

#define WEBMESSENGER_URL "http://login.yahoo.com/config/login?.src=pg"

#define YAHOO_ICON_CHECKSUM_KEY "icon_checksum"
#define YAHOO_PICURL_SETTING "picture_url"
#define YAHOO_PICCKSUM_SETTING "picture_checksum"
#define YAHOO_PICEXPIRE_SETTING "picture_expire"

enum yahoo_service { /* these are easier to see in hex */
	YAHOO_SERVICE_LOGON = 1,
	YAHOO_SERVICE_LOGOFF,
	YAHOO_SERVICE_ISAWAY,
	YAHOO_SERVICE_ISBACK,
	YAHOO_SERVICE_IDLE, /* 5 (placemarker) */
	YAHOO_SERVICE_MESSAGE,
	YAHOO_SERVICE_IDACT,
	YAHOO_SERVICE_IDDEACT,
	YAHOO_SERVICE_MAILSTAT,
	YAHOO_SERVICE_USERSTAT, /* 0xa */
	YAHOO_SERVICE_NEWMAIL,
	YAHOO_SERVICE_CHATINVITE,
	YAHOO_SERVICE_CALENDAR,
	YAHOO_SERVICE_NEWPERSONALMAIL,
	YAHOO_SERVICE_NEWCONTACT,
	YAHOO_SERVICE_ADDIDENT, /* 0x10 */
	YAHOO_SERVICE_ADDIGNORE,
	YAHOO_SERVICE_PING,
	YAHOO_SERVICE_GOTGROUPRENAME,
	YAHOO_SERVICE_SYSMESSAGE = 0x14,
	YAHOO_SERVICE_SKINNAME = 0x15,
	YAHOO_SERVICE_PASSTHROUGH2 = 0x16,
	YAHOO_SERVICE_CONFINVITE = 0x18,
	YAHOO_SERVICE_CONFLOGON,
	YAHOO_SERVICE_CONFDECLINE,
	YAHOO_SERVICE_CONFLOGOFF,
	YAHOO_SERVICE_CONFADDINVITE,
	YAHOO_SERVICE_CONFMSG,
	YAHOO_SERVICE_CHATLOGON,
	YAHOO_SERVICE_CHATLOGOFF,
	YAHOO_SERVICE_CHATMSG = 0x20,
	YAHOO_SERVICE_GAMELOGON = 0x28,
	YAHOO_SERVICE_GAMELOGOFF,
	YAHOO_SERVICE_GAMEMSG = 0x2a,
	YAHOO_SERVICE_FILETRANSFER = 0x46,
	YAHOO_SERVICE_VOICECHAT = 0x4A,
	YAHOO_SERVICE_NOTIFY = 0x4B,
	YAHOO_SERVICE_VERIFY,
	YAHOO_SERVICE_P2PFILEXFER,
	YAHOO_SERVICE_PEEPTOPEER = 0x4F,
	YAHOO_SERVICE_WEBCAM,
	YAHOO_SERVICE_AUTHRESP = 0x54,
	YAHOO_SERVICE_LIST = 0x55,
	YAHOO_SERVICE_AUTH = 0x57,
	YAHOO_SERVICE_ADDBUDDY = 0x83,
	YAHOO_SERVICE_REMBUDDY = 0x84,
	YAHOO_SERVICE_IGNORECONTACT,    /* > 1, 7, 13 < 1, 66, 13, 0*/
	YAHOO_SERVICE_REJECTCONTACT,
	YAHOO_SERVICE_GROUPRENAME = 0x89, /* > 1, 65(new), 66(0), 67(old) */
	/* YAHOO_SERVICE_??? = 0x8A, */
	YAHOO_SERVICE_CHATONLINE = 0x96, /* > 109(id), 1, 6(abcde) < 0,1*/
	YAHOO_SERVICE_CHATGOTO,
	YAHOO_SERVICE_CHATJOIN, /* > 1 104-room 129-1600326591 62-2 */
	YAHOO_SERVICE_CHATLEAVE,
	YAHOO_SERVICE_CHATEXIT = 0x9b,
	YAHOO_SERVICE_CHATADDINVITE = 0x9d,
	YAHOO_SERVICE_CHATLOGOUT = 0xa0,
	YAHOO_SERVICE_CHATPING,
	YAHOO_SERVICE_COMMENT = 0xa8,
	YAHOO_SERVICE_AVATAR = 0xbc,
	YAHOO_SERVICE_PICTURE_CHECKSUM = 0xbd,
	YAHOO_SERVICE_PICTURE = 0xbe,
	YAHOO_SERVICE_PICTURE_UPDATE = 0xc1,
	YAHOO_SERVICE_PICTURE_UPLOAD = 0xc2,
	YAHOO_SERVICE_Y6_VISIBLE_TOGGLE = 0xc5,
	YAHOO_SERVICE_Y6_STATUS_UPDATE = 0xc6,
	YAHOO_SERVICE_AVATAR_UPDATE = 0xc7,
	YAHOO_SERVICE_VERIFY_ID_EXISTS = 0xc8,
	YAHOO_SERVICE_AUDIBLE = 0xd0,
	YAHOO_SERVICE_WEBLOGIN = 0x0226,
	YAHOO_SERVICE_SMS_MSG = 0x02ea
};

#define YAHOO_STATUS_TYPE_OFFLINE "offline"
#define YAHOO_STATUS_TYPE_ONLINE "online"
#define YAHOO_STATUS_TYPE_AVAILABLE "available"
#define YAHOO_STATUS_TYPE_AVAILABLE_WM "available-wm"
#define YAHOO_STATUS_TYPE_BRB "brb"
#define YAHOO_STATUS_TYPE_BUSY "busy"
#define YAHOO_STATUS_TYPE_NOTATHOME "notathome"
#define YAHOO_STATUS_TYPE_NOTATDESK "notatdesk"
#define YAHOO_STATUS_TYPE_NOTINOFFICE "notinoffice"
#define YAHOO_STATUS_TYPE_ONPHONE "onphone"
#define YAHOO_STATUS_TYPE_ONVACATION "onvacation"
#define YAHOO_STATUS_TYPE_OUTTOLUNCH "outtolunch"
#define YAHOO_STATUS_TYPE_STEPPEDOUT "steppedout"
#define YAHOO_STATUS_TYPE_AWAY "away"
#define YAHOO_STATUS_TYPE_INVISIBLE "invisible"

enum yahoo_status {
	YAHOO_STATUS_AVAILABLE = 0,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE = 12,
	YAHOO_STATUS_CUSTOM = 99,
	YAHOO_STATUS_IDLE = 999,
	YAHOO_STATUS_WEBLOGIN = 0x5a55aa55,
	YAHOO_STATUS_OFFLINE = 0x5a55aa56, /* don't ask */
	YAHOO_STATUS_TYPING = 0x16
};

struct yahoo_buddy_icon_upload_data {
	GaimConnection *gc;
	GString *str;
	char *filename;
	int pos;
	int fd;
	guint watcher;
};

struct _YchtConn;

struct yahoo_data {
	int fd;
	guchar *rxqueue;
	int rxlen;
	GHashTable *friends;
	int current_status;
	gboolean logged_in;
	GString *tmp_serv_blist, *tmp_serv_ilist;
	GSList *confs;
	unsigned int conf_id; /* just a counter */
	gboolean chat_online;
	gboolean in_chat;
	char *chat_name;
	char *auth;
	char *cookie_y;
	char *cookie_t;
	int session_id;
	gboolean jp;
	gboolean wm;
	/* picture aka buddy icon stuff */
	char *picture_url;
	int picture_checksum;

	/* ew. we have to check the icon before we connect,
	 * but can't upload it til we're connected. */
	struct yahoo_buddy_icon_upload_data *picture_upload_todo;

	struct _YchtConn *ycht;
};

struct yahoo_pair {
	int key;
	char *value;
};

struct yahoo_packet {
	guint16 service;
	guint32 status;
	guint32 id;
	GSList *hash;
};


#define YAHOO_MAX_STATUS_MESSAGE_LENGTH (255)


#define YAHOO_WEBMESSENGER_PROTO_VER 0x0065
#define YAHOO_PROTO_VER 0x000c


#define YAHOO_PACKET_HDRLEN (4 + 2 + 2 + 2 + 2 + 4 + 4)

/* sometimes i wish prpls could #include things from other prpls. then i could just
 * use the routines from libfaim and not have to admit to knowing how they work. */
#define yahoo_put16(buf, data) ( \
		(*(buf) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+1) = (unsigned char)(data)&0xff),  \
		2)
#define yahoo_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define yahoo_put32(buf, data) ( \
		(*((buf)) = (unsigned char)((data)>>24)&0xff), \
		(*((buf)+1) = (unsigned char)((data)>>16)&0xff), \
		(*((buf)+2) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+3) = (unsigned char)(data)&0xff), \
		4)
#define yahoo_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))


struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, enum yahoo_status status, int id);
void yahoo_packet_hash(struct yahoo_packet *pkt, int key, const char *value);
int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt);
int yahoo_send_packet_special(int fd, struct yahoo_packet *pkt, int pad);
void yahoo_packet_write(struct yahoo_packet *pkt, guchar *data);
int yahoo_packet_length(struct yahoo_packet *pkt);
void yahoo_packet_free(struct yahoo_packet *pkt);

/* util.c */
void yahoo_init_colorht();
void yahoo_dest_colorht();
char *yahoo_codes_to_html(const char *x);
char *yahoo_html_to_codes(const char *src);

/**
 * Encode some text to send to the yahoo server.
 *
 * @param gc The connection handle.
 * @param str The null terminated utf8 string to encode.
 * @param utf8 If not @c NULL, whether utf8 is okay or not.
 *             Even if it is okay, we may not use it. If we
 *             used it, we set this to @c TRUE, else to
 *             @c FALSE. If @c NULL, false is assumed, and
 *             it is not dereferenced.
 * @return The g_malloced string in the appropriate encoding.
 */
char *yahoo_string_encode(GaimConnection *gc, const char *str, gboolean *utf8);

/**
 * Decode some text received from the server.
 *
 * @param gc The gc handle.
 * @param str The null terminated string to decode.
 * @param utf8 Did the server tell us it was supposed to be utf8?
 * @return The decoded, utf-8 string, which must be g_free()'d.
 */
char *yahoo_string_decode(GaimConnection *gc, const char *str, gboolean utf8);

/* previously-static functions, now needed for yahoo_profile.c */
char *yahoo_tooltip_text(GaimBuddy *b);

/* yahoo_profile.c */
void yahoo_get_info(GaimConnection *gc, const char *name);

#endif /* _YAHOO_H_ */
