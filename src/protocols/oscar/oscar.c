/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Some code copyright (C) 1999-2001, Eric Warmenhoven
 * Some code copyright (C) 2001-2003, Sean Egan
 * Some code copyright (C) 2001-2004, Mark Doliner <thekingant@users.sourceforge.net>
 *
 * Most libfaim code copyright (C) 1998-2001 Adam Fritzler <afritz@auk.cx>
 * Some libfaim code copyright (C) 2001-2004 Mark Doliner <thekingant@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "buddyicon.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "ft.h"
#include "imgstore.h"
#include "network.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "util.h"
#include "version.h"

#include "aim.h"
#include "md5.h"

#define OSCAR_STATUS_ID_INVISIBLE	"invisible"
#define OSCAR_STATUS_ID_OFFLINE		"offline"
#define OSCAR_STATUS_ID_ONLINE		"online"
#define OSCAR_STATUS_ID_AVAILABLE	"available"
#define OSCAR_STATUS_ID_AWAY		"away"
#define OSCAR_STATUS_ID_DND			"dnd"
#define OSCAR_STATUS_ID_NA			"na"
#define OSCAR_STATUS_ID_OCCUPIED	"occupied"
#define OSCAR_STATUS_ID_FREE4CHAT	"free4chat"
#define OSCAR_STATUS_ID_CUSTOM		"custom"

#define AIMHASHDATA "http://gaim.sourceforge.net/aim_data.php3"

#define OSCAR_CONNECT_STEPS 6
#define OSCAR_DEFAULT_CUSTOM_ENCODING "ISO-8859-1"

#define FAIM_DEBUG_LEVEL 0

static GaimPlugin *my_protocol = NULL;

static int caps_aim = AIM_CAPS_CHAT | AIM_CAPS_BUDDYICON | AIM_CAPS_DIRECTIM | AIM_CAPS_SENDFILE | AIM_CAPS_INTEROPERATE | AIM_CAPS_ICHAT;
static int caps_icq = AIM_CAPS_BUDDYICON | AIM_CAPS_DIRECTIM | AIM_CAPS_SENDFILE | AIM_CAPS_ICQUTF8 | AIM_CAPS_INTEROPERATE | AIM_CAPS_ICHAT;

static fu8_t features_aim[] = {0x01, 0x01, 0x01, 0x02};
static fu8_t features_icq[] = {0x01, 0x06};
static fu8_t ck[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

typedef struct _OscarData OscarData;
struct _OscarData {
	aim_session_t *sess;
	aim_conn_t *conn;

	guint cnpa;
	guint paspa;
	guint emlpa;
	guint icopa;

	gboolean iconconnecting;
	gboolean set_icon;

	GSList *create_rooms;

	gboolean conf;
	gboolean reqemail;
	gboolean setemail;
	char *email;
	gboolean setnick;
	char *newsn;
	gboolean chpass;
	char *oldp;
	char *newp;

	GSList *oscar_chats;
	GSList *direct_ims;
	GSList *file_transfers;
	GHashTable *buddyinfo;
	GSList *requesticon;

	gboolean killme;
	gboolean icq;
	guint icontimer;
	guint getblisttimer;
	guint getinfotimer;

	struct {
		guint maxwatchers; /* max users who can watch you */
		guint maxbuddies; /* max users you can watch */
		guint maxgroups; /* max groups in server list */
		guint maxpermits; /* max users on permit list */
		guint maxdenies; /* max users on deny list */
		guint maxsiglen; /* max size (bytes) of profile */
		guint maxawaymsglen; /* max size (bytes) of posted away message */
	} rights;
};

struct create_room {
	char *name;
	int exchange;
};

struct chat_connection {
	char *name;
	char *show; /* AOL did something funny to us */
	fu16_t exchange;
	fu16_t instance;
	int fd; /* this is redundant since we have the conn below */
	aim_conn_t *conn;
	int inpa;
	int id;
	GaimConnection *gc; /* i hate this. */
	GaimConversation *conv; /* bah. */
	int maxlen;
	int maxvis;
};

struct oscar_direct_im {
	GaimConnection *gc;
	char name[80];
	int watcher;
	aim_conn_t *conn;
	gboolean connected;
	gboolean gpc_pend;
	gboolean killme;
	gboolean donttryagain;
};

struct ask_direct {
	GaimConnection *gc;
	char *sn;
	char ip[64];
	fu8_t cookie[8];
	gboolean donttryagain;
};

/*
 * Various PRPL-specific buddy info that we want to keep track of
 * Some other info is maintained by locate.c, and I'd like to move 
 * the rest of this to libfaim, mostly im.c
 */
struct buddyinfo {
	gboolean typingnot;
	gchar *availmsg;
	fu32_t ipaddr;

	unsigned long ico_me_len;
	unsigned long ico_me_csum;
	time_t ico_me_time;
	gboolean ico_informed;

	unsigned long ico_len;
	unsigned long ico_csum;
	time_t ico_time;
	gboolean ico_need;
	gboolean ico_sent;
};

struct name_data {
	GaimConnection *gc;
	gchar *name;
	gchar *nick;
};

static char *msgerrreason[] = {
	N_("Invalid error"),
	N_("Invalid SNAC"),
	N_("Rate to host"),
	N_("Rate to client"),
	N_("Not logged in"),
	N_("Service unavailable"),
	N_("Service not defined"),
	N_("Obsolete SNAC"),
	N_("Not supported by host"),
	N_("Not supported by client"),
	N_("Refused by client"),
	N_("Reply too big"),
	N_("Responses lost"),
	N_("Request denied"),
	N_("Busted SNAC payload"),
	N_("Insufficient rights"),
	N_("In local permit/deny"),
	N_("Too evil (sender)"),
	N_("Too evil (receiver)"),
	N_("User temporarily unavailable"),
	N_("No match"),
	N_("List overflow"),
	N_("Request ambiguous"),
	N_("Queue full"),
	N_("Not while on AOL")
};
static int msgerrreasonlen = 25;

/* All the libfaim->gaim callback functions */
static int gaim_parse_auth_resp  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_login      (aim_session_t *, aim_frame_t *, ...);
static int gaim_handle_redirect  (aim_session_t *, aim_frame_t *, ...);
static int gaim_info_change      (aim_session_t *, aim_frame_t *, ...);
static int gaim_account_confirm  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_oncoming   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_offgoing   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_incoming_im(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_misses     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_clientauto (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_userinfo   (aim_session_t *, aim_frame_t *, ...);
static int gaim_reqinfo_timeout  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_motd       (aim_session_t *, aim_frame_t *, ...);
static int gaim_chatnav_info     (aim_session_t *, aim_frame_t *, ...);
static int gaim_conv_chat_join        (aim_session_t *, aim_frame_t *, ...);
static int gaim_conv_chat_leave       (aim_session_t *, aim_frame_t *, ...);
static int gaim_conv_chat_info_update (aim_session_t *, aim_frame_t *, ...);
static int gaim_conv_chat_incoming_msg(aim_session_t *, aim_frame_t *, ...);
static int gaim_email_parseupdate(aim_session_t *, aim_frame_t *, ...);
static int gaim_icon_error       (aim_session_t *, aim_frame_t *, ...);
static int gaim_icon_parseicon   (aim_session_t *, aim_frame_t *, ...);
static int oscar_icon_req        (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgack     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_ratechange (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_evilnotify (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searcherror(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searchreply(aim_session_t *, aim_frame_t *, ...);
static int gaim_bosrights        (aim_session_t *, aim_frame_t *, ...);
static int gaim_connerr          (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_admin    (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_bos      (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_chatnav  (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_chat     (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_email    (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_icon     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_mtn        (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_locaterights(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_buddyrights(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_locerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_icbm_param_info  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_genericerr (aim_session_t *, aim_frame_t *, ...);
static int gaim_memrequest       (aim_session_t *, aim_frame_t *, ...);
static int gaim_selfinfo         (aim_session_t *, aim_frame_t *, ...);
static int gaim_offlinemsg       (aim_session_t *, aim_frame_t *, ...);
static int gaim_offlinemsgdone   (aim_session_t *, aim_frame_t *, ...);
static int gaim_icqalias         (aim_session_t *, aim_frame_t *, ...);
static int gaim_icqinfo          (aim_session_t *, aim_frame_t *, ...);
static int gaim_popup            (aim_session_t *, aim_frame_t *, ...);
#ifndef NOSSI
static int gaim_ssi_parseerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parserights  (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parselist    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parseack     (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parseadd     (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authgiven    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authrequest  (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authreply    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_gotadded     (aim_session_t *, aim_frame_t *, ...);
#endif

/* for DirectIM/image transfer */
static int gaim_odc_initiate     (aim_session_t *, aim_frame_t *, ...);
static int gaim_odc_incoming     (aim_session_t *, aim_frame_t *, ...);
static int gaim_odc_typing       (aim_session_t *, aim_frame_t *, ...);
static int gaim_odc_update_ui    (aim_session_t *, aim_frame_t *, ...);

/* for file transfer */
static int oscar_sendfile_estblsh(aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_prompt (aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_ack    (aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_done   (aim_session_t *, aim_frame_t *, ...);

static gboolean gaim_icon_timerfunc(gpointer data);
static void oscar_callback(gpointer data, gint source, GaimInputCondition condition);
static void oscar_direct_im_initiate(GaimConnection *gc, const char *who, const char *cookie);
static void oscar_set_info(GaimConnection *gc, const char *text);
static void recent_buddies_cb(const char *name, GaimPrefType type, gpointer value, gpointer data);

static void oscar_free_name_data(struct name_data *data) {
	g_free(data->name);
	g_free(data->nick);
	g_free(data);
}

static void oscar_free_buddyinfo(void *data) {
	struct buddyinfo *bi = data;
	g_free(bi->availmsg);
	g_free(bi);
}

static fu32_t oscar_charset_check(const char *utf8)
{
	int i = 0;
	int charset = AIM_CHARSET_ASCII;

	/* Determine how we can send this message.  Per the warnings elsewhere 
	 * in this file, these little checks determine the simplest encoding 
	 * we can use for a given message send using it. */
	while (utf8[i]) {
		if ((unsigned char)utf8[i] > 0x7f) {
			/* not ASCII! */
			charset = AIM_CHARSET_CUSTOM;
			break;
		}
		i++;
	}
	while (utf8[i]) {
		/* ISO-8859-1 is 0x00-0xbf in the first byte
		 * followed by 0xc0-0xc3 in the second */
		if ((unsigned char)utf8[i] < 0x80) {
			i++;
			continue;
		} else if (((unsigned char)utf8[i] & 0xfc) == 0xc0 &&
			   ((unsigned char)utf8[i + 1] & 0xc0) == 0x80) {
			i += 2;
			continue;
		}
		charset = AIM_CHARSET_UNICODE;
		break;
	}

	return charset;
}

/*
 * Take a string of the form charset="bleh" where bleh is
 * one of us-ascii, utf-8, iso-8859-1, or unicode-2-0, and 
 * return a newly allocated string containing bleh.
 */
static gchar *oscar_encoding_extract(const char *encoding)
{
	gchar *ret = NULL;
	char *begin, *end;

	g_return_val_if_fail(encoding != NULL, NULL);

	/* Make sure encoding begins with charset= */
	if (strncmp(encoding, "text/aolrtf; charset=", 21))
		return NULL;

	begin = strchr(encoding, '"');
	end = strrchr(encoding, '"');

	if ((begin == NULL) || (end == NULL) || (begin >= end))
		return NULL;

	ret = g_strndup(begin+1, (end-1) - begin);

	return ret;
}

static gchar *
oscar_encoding_to_utf8(const char *encoding, const char *text, int textlen)
{
	gchar *utf8 = NULL;

	if ((encoding == NULL) || encoding[0] == '\0') {
		gaim_debug_info("oscar", "Empty encoding, assuming UTF-8\n");
	} else if (!strcasecmp(encoding, "iso-8859-1")) {
		utf8 = g_convert(text, textlen, "UTF-8", "iso-8859-1", NULL, NULL, NULL);
	} else if (!strcasecmp(encoding, "ISO-8859-1-Windows-3.1-Latin-1")) {
		utf8 = g_convert(text, textlen, "UTF-8", "Windows-1252", NULL, NULL, NULL);
	} else if (!strcasecmp(encoding, "unicode-2-0")) {
		utf8 = g_convert(text, textlen, "UTF-8", "UCS-2BE", NULL, NULL, NULL);
	} else if (strcasecmp(encoding, "us-ascii") && strcmp(encoding, "utf-8")) {
		gaim_debug_warning("oscar", "Unrecognized character encoding \"%s\", "
						   "attempting to convert to UTF-8 anyway\n", encoding);
		utf8 = g_convert(text, textlen, "UTF-8", encoding, NULL, NULL, NULL);
	}

	/*
	 * If utf8 is still NULL then either the encoding is us-ascii/utf-8 or
	 * we have been unable to convert the text to utf-8 from the encoding
	 * that was specified.  So we check if the text is valid utf-8 then
	 * just copy it.
	 */
	if (utf8 == NULL) {
		if (textlen != 0 && *text != '\0'
                    && !g_utf8_validate(text, textlen, NULL))
			utf8 = g_strdup(_("(There was an error receiving this message.  The buddy you are speaking to most likely has a buggy client.)"));
		else
			utf8 = g_strndup(text, textlen);
	}

	return utf8;
}

static gchar *
gaim_plugin_oscar_convert_to_utf8(const fu8_t *data, fu16_t datalen, const char *charsetstr, gboolean fallback)
{
	gchar *ret = NULL;
	GError *err = NULL;

	if ((charsetstr == NULL) || (*charsetstr == '\0'))
		return NULL;

	if (strcasecmp("UTF-8", charsetstr)) {
		if (fallback)
			ret = g_convert_with_fallback(data, datalen, "UTF-8", charsetstr, "?", NULL, NULL, &err);
		else
			ret = g_convert(data, datalen, "UTF-8", charsetstr, NULL, NULL, &err);
		if (err != NULL) {
			gaim_debug_warning("oscar", "Conversation from %s failed: %s.\n",
							   charsetstr, err->message);
			g_error_free(err);
		}
	} else {
		if (g_utf8_validate(data, datalen, NULL))
			ret = g_strndup(data, datalen);
		else
			gaim_debug_warning("oscar", "String is not valid UTF-8.\n");
	}

	return ret;
}

/*
 * We try decoding using two different character sets.  The charset
 * specified in the IM determines the order in which we attempt to
 * decode.  We do this because there are lots of broken ICQ clients
 * that don't correctly send non-ASCII messages.  And if Gaim isn't
 * able to deal with that crap, then people complain like banshees.
 * charsetstr1 is always set to what the correct encoding should be.
 */
static gchar *
gaim_plugin_oscar_decode_im_part(GaimAccount *account, const char *sourcesn, fu16_t charset, fu16_t charsubset, fu8_t *data, fu16_t datalen)
{
	gchar *ret = NULL;
	const gchar *charsetstr1, *charsetstr2;

	gaim_debug_info("oscar", "Parsing IM part, charset=0x%04hx, charsubset=0x%04hx, datalen=%hd\n", charset, charsubset, datalen);

	if ((datalen == 0) || (data == NULL))
		return NULL;

	if (charset == AIM_CHARSET_UNICODE) {
		charsetstr1 = "UCS-2BE";
		charsetstr2 = "UTF-8";
	} else if (charset == AIM_CHARSET_CUSTOM) {
		if ((sourcesn != NULL) && aim_sn_is_icq(sourcesn))
			charsetstr1 = gaim_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
		else
			charsetstr1 = "ISO-8859-1";
		charsetstr2 = "UTF-8";
	} else if (charset == AIM_CHARSET_ASCII) {
		/* Should just be "ASCII" */
		charsetstr1 = "ASCII";
		charsetstr2 = gaim_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	} else if (charset == 0x000d) {
		/* Mobile AIM client on a Nokia 3100 and an LG VX6000 */
		charsetstr1 = "ISO-8859-1";
		charsetstr2 = gaim_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	} else {
		/* Unknown, hope for valid UTF-8... */
		charsetstr1 = "UTF-8";
		charsetstr2 = gaim_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	}

	ret = gaim_plugin_oscar_convert_to_utf8(data, datalen, charsetstr1, FALSE);
	if (ret == NULL)
		ret = gaim_plugin_oscar_convert_to_utf8(data, datalen, charsetstr2, TRUE);
	if (ret == NULL)
		ret = g_strdup(_("(There was an error receiving this message.  The buddy you are speaking to most likely has a buggy client.)"));

	return ret;
}

static void
gaim_plugin_oscar_convert_to_best_encoding(GaimConnection *gc, const char *destsn, const gchar *from,
										   gchar **msg, int *msglen_int,
										   fu16_t *charset, fu16_t *charsubset)
{
	OscarData *od = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GError *err = NULL;
	aim_userinfo_t *userinfo = NULL;
	const gchar *charsetstr;
	gsize msglen;

	/* Attempt to send as ASCII */
	if (oscar_charset_check(from) == AIM_CHARSET_ASCII) {
		*msg = g_convert(from, strlen(from), "ASCII", "UTF-8", NULL, &msglen, NULL);
		*charset = AIM_CHARSET_ASCII;
		*charsubset = 0x0000;
		*msglen_int = msglen;
		return;
	}

	/*
	 * If we're sending to an ICQ user, and they are advertising the
	 * Unicode capability, then attempt to send as UCS-2BE.
	 */
	if ((destsn != NULL) && aim_sn_is_icq(destsn))
		userinfo = aim_locate_finduserinfo(od->sess, destsn);

	if ((userinfo != NULL) && (userinfo->capabilities & AIM_CAPS_ICQUTF8)) {
		*msg = g_convert(from, strlen(from), "UCS-2BE", "UTF-8", NULL, &msglen, NULL);
		if (*msg != NULL) {
			*charset = AIM_CHARSET_UNICODE;
			*charsubset = 0x0000;
			*msglen_int = msglen;
			return;
		}
	}

	/*
	 * If this is AIM then attempt to send as ISO-8859-1.  If this is
	 * ICQ then attempt to send as the user specified character encoding.
	 */
	charsetstr = "ISO-8859-1";
	if ((destsn != NULL) && aim_sn_is_icq(destsn))
		charsetstr = gaim_account_get_string(account, "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);

	/*
	 * XXX - We need a way to only attempt to convert if we KNOW "from"
	 * can be converted to "charsetstr"
	 */
	*msg = g_convert(from, strlen(from), charsetstr, "UTF-8", NULL, &msglen, NULL);
	if (*msg != NULL) {
		*charset = AIM_CHARSET_CUSTOM;
		*charsubset = 0x0000;
		*msglen_int = msglen;
		return;
	}

	/*
	 * Nothing else worked, so send as UCS-2BE.
	 */
	*msg = g_convert(from, strlen(from), "UCS-2BE", "UTF-8", NULL, &msglen, &err);
	if (*msg != NULL) {
		*charset = AIM_CHARSET_UNICODE;
		*charsubset = 0x0000;
		*msglen_int = msglen;
		return;
	}

	gaim_debug_error("oscar", "Error converting a Unicode message: %s\n", err->message);
	g_error_free(err);

	gaim_debug_error("oscar", "This should NEVER happen!  Sending UTF-8 text flagged as ASCII.\n");
	*msg = g_strdup(from);
	*msglen_int = strlen(*msg);
	*charset = AIM_CHARSET_ASCII;
	*charsubset = 0x0000;
	return;
}

gchar *oscar_caps_to_string(guint caps)
{
	GString *str;
	gchar *tmp;
	guint bit = 1;

	str = g_string_new("");

	if (!caps) {
		return NULL;
	} else while (bit <= AIM_CAPS_LAST) {
		if (bit & caps) {
			switch (bit) {
			case AIM_CAPS_BUDDYICON:
				tmp = _("Buddy Icon");
				break;
			case AIM_CAPS_TALK:
				tmp = _("Voice");
				break;
			case AIM_CAPS_DIRECTIM:
				tmp = _("AIM Direct IM");
				break;
			case AIM_CAPS_CHAT:
				tmp = _("Chat");
				break;
			case AIM_CAPS_GETFILE:
				tmp = _("Get File");
				break;
			case AIM_CAPS_SENDFILE:
				tmp = _("Send File");
				break;
			case AIM_CAPS_GAMES:
			case AIM_CAPS_GAMES2:
				tmp = _("Games");
				break;
			case AIM_CAPS_ADDINS:
				tmp = _("Add-Ins");
				break;
			case AIM_CAPS_SENDBUDDYLIST:
				tmp = _("Send Buddy List");
				break;
			case AIM_CAPS_ICQ_DIRECT:
				tmp = _("ICQ Direct Connect");
				break;
			case AIM_CAPS_APINFO:
				tmp = _("AP User");
				break;
			case AIM_CAPS_ICQRTF:
				tmp = _("ICQ RTF");
				break;
			case AIM_CAPS_EMPTY:
				tmp = _("Nihilist");
				break;
			case AIM_CAPS_ICQSERVERRELAY:
				tmp = _("ICQ Server Relay");
				break;
			case AIM_CAPS_ICQUTF8OLD:
				tmp = _("Old ICQ UTF8");
				break;
			case AIM_CAPS_TRILLIANCRYPT:
				tmp = _("Trillian Encryption");
				break;
			case AIM_CAPS_ICQUTF8:
				tmp = _("ICQ UTF8");
				break;
			case AIM_CAPS_HIPTOP:
				tmp = _("Hiptop");
				break;
			case AIM_CAPS_SECUREIM:
				tmp = _("Security Enabled");
				break;
			case AIM_CAPS_VIDEO:
				tmp = _("Video Chat");
				break;
			/* Not actually sure about this one... WinAIM doesn't show anything */
			case AIM_CAPS_ICHATAV:
				tmp = _("iChat AV");
				break;
			case AIM_CAPS_LIVEVIDEO:
				tmp = _("Live Video");
				break;
			case AIM_CAPS_CAMERA:
				tmp = _("Camera");
				break;
			default:
				tmp = NULL;
				break;
			}
			if (tmp)
				g_string_append_printf(str, "%s%s", (*(str->str) == '\0' ? "" : ", "), tmp);
		}
		bit <<= 1;
	}

	return g_string_free(str, FALSE);
}

static char *oscar_icqstatus(int state) {
	/* Make a cute little string that shows the status of the dude or dudet */
	if (state & AIM_ICQ_STATE_CHAT)
		return g_strdup_printf(_("Free For Chat"));
	else if (state & AIM_ICQ_STATE_DND)
		return g_strdup_printf(_("Do Not Disturb"));
	else if (state & AIM_ICQ_STATE_OUT)
		return g_strdup_printf(_("Not Available"));
	else if (state & AIM_ICQ_STATE_BUSY)
		return g_strdup_printf(_("Occupied"));
	else if (state & AIM_ICQ_STATE_AWAY)
		return g_strdup_printf(_("Away"));
	else if (state & AIM_ICQ_STATE_WEBAWARE)
		return g_strdup_printf(_("Web Aware"));
	else if (state & AIM_ICQ_STATE_INVISIBLE)
		return g_strdup_printf(_("Invisible"));
	else
		return g_strdup_printf(_("Online"));
}

static void
oscar_string_append(GString *str, const char *newline, const char *name, const char *value)
{
	gchar *utf8;

	if (value && value[0] && (utf8 = gaim_utf8_try_convert(value))) {
		g_string_append_printf(str, "%s<b>%s:</b> %s", newline, name, utf8);
		g_free(utf8);
	}
}

static void oscar_string_append_info(GaimConnection *gc, GString *str, const char *newline, GaimBuddy *b, aim_userinfo_t *userinfo)
{
	OscarData *od;
	GaimAccount *account;
	GaimPresence *presence;
	GaimPresence *buddy_presence;
	GaimGroup *g = NULL;
	struct buddyinfo *bi = NULL;
	char *tmp;

	od = gc->proto_data;
	account = gaim_connection_get_account(gc);
	presence = gaim_account_get_presence(account);
	buddy_presence = gaim_buddy_get_presence(b);

	if ((str == NULL) || (newline == NULL) || ((b == NULL) && (userinfo == NULL)))
		return;

	if (userinfo == NULL)
		userinfo = aim_locate_finduserinfo(od->sess, b->name);

	if (b == NULL)
		b = gaim_find_buddy(account, userinfo->sn);

	if (b != NULL)
		g = gaim_find_buddys_group(b);

	if (userinfo != NULL)
		bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, userinfo->sn));

	if (b != NULL) {
		if (gaim_presence_is_online(buddy_presence)) {
			if (aim_sn_is_icq(b->name)) {
				tmp = oscar_icqstatus((b->uc & 0xffff0000) >> 16);
				oscar_string_append(str, newline, _("Status"), tmp);
				g_free(tmp);
			}
		} else {
			tmp = aim_ssi_itemlist_findparentname(od->sess->ssi.local, b->name);
			if (aim_ssi_waitingforauth(od->sess->ssi.local, tmp, b->name))
				oscar_string_append(str, newline, _("Status"), _("Not Authorized"));
			else
				oscar_string_append(str, newline, _("Status"), _("Offline"));
		}
	}

	if ((bi != NULL) && (bi->ipaddr != 0)) {
		tmp =  g_strdup_printf("%hhu.%hhu.%hhu.%hhu",
						(bi->ipaddr & 0xff000000) >> 24,
						(bi->ipaddr & 0x00ff0000) >> 16,
						(bi->ipaddr & 0x0000ff00) >> 8,
						(bi->ipaddr & 0x000000ff));
		oscar_string_append(str, newline, _("IP Address"), tmp);
		g_free(tmp);
	}

	if ((userinfo != NULL) && (userinfo->capabilities != 0)) {
		tmp = oscar_caps_to_string(userinfo->capabilities);
		oscar_string_append(str, newline, _("Capabilities"), tmp);
		g_free(tmp);
	}

	if ((b != NULL) && (b->name != NULL) && (g != NULL) && (g->name != NULL)) {
		tmp = aim_ssi_getcomment(od->sess->ssi.local, g->name, b->name);
		if (tmp != NULL) {
			char *tmp2 = g_markup_escape_text(tmp, strlen(tmp));
			g_free(tmp);
			oscar_string_append(str, newline, _("Buddy Comment"), tmp2);
			g_free(tmp2);
		}
	}

	if ((bi != NULL) && (bi->availmsg != NULL) && gaim_presence_is_available(presence)) {
		tmp = g_markup_escape_text(bi->availmsg, strlen(bi->availmsg));
		oscar_string_append(str, newline, _("Available"), tmp);
		g_free(tmp);
	}
}

static char *extract_name(const char *name) {
	char *tmp, *x;
	int i, j;

	if (!name)
		return NULL;

	x = strchr(name, '-');

	if (!x) return NULL;
	x = strchr(++x, '-');
	if (!x) return NULL;
	tmp = g_strdup(++x);

	for (i = 0, j = 0; x[i]; i++) {
		char hex[3];
		if (x[i] != '%') {
			tmp[j++] = x[i];
			continue;
		}
		strncpy(hex, x + ++i, 2); hex[2] = 0;
		i++;
		tmp[j++] = strtol(hex, NULL, 16);
	}

	tmp[j] = 0;
	return tmp;
}

static struct chat_connection *find_oscar_chat(GaimConnection *gc, int id) {
	GSList *g = ((OscarData *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->id == id)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct chat_connection *find_oscar_chat_by_conn(GaimConnection *gc,
							aim_conn_t *conn) {
	GSList *g = ((OscarData *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->conn == conn)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct chat_connection *find_oscar_chat_by_conv(GaimConnection *gc,
							GaimConversation *conv) {
	GSList *g = ((OscarData *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->conv == conv)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

/*****************************************************************************
 * Begin scary direct im stuff
 *****************************************************************************/

static struct oscar_direct_im *oscar_direct_im_find(OscarData *od, const char *who) {
	GSList *d = od->direct_ims;
	struct oscar_direct_im *m = NULL;

	while (d) {
		m = (struct oscar_direct_im *)d->data;
		if (!aim_sncmp(who, m->name))
			return m;
		d = d->next;
	}

	return NULL;
}

static void oscar_direct_im_destroy(OscarData *od, struct oscar_direct_im *dim)
{
	gaim_debug_info("oscar",
			   "destroying Direct IM for %s.\n", dim->name);

	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	if (dim->gpc_pend) {
		dim->killme = TRUE;
		return;
	}
	if (dim->watcher)
	gaim_input_remove(dim->watcher);
	if (dim->conn) {
		aim_conn_close(dim->conn);
		aim_conn_kill(od->sess, &dim->conn);
	}
	g_free(dim);
}

/* the only difference between this and destroy is this writes a conv message */
static void oscar_direct_im_disconnect(OscarData *od, struct oscar_direct_im *dim)
{
	GaimConversation *conv;
	char buf[256];

	gaim_debug_info("oscar",
			   "%s disconnected Direct IM.\n", dim->name);

	if (dim->connected)
		g_snprintf(buf, sizeof buf, _("Direct IM with %s closed"), dim->name);
	else
		g_snprintf(buf, sizeof buf, _("Direct IM with %s failed"), dim->name);

	conv = gaim_find_conversation_with_account(GAIM_CONV_IM, dim->name,
											   gaim_connection_get_account(dim->gc));
	if (conv) {
		gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_SYSTEM, time(NULL));
		gaim_conversation_update_progress(conv, 0);
	} else {
		gaim_notify_error(dim->gc, NULL, _("Direct Connect failed"), buf);
	}

	oscar_direct_im_destroy(od, dim);

	return;
}

/* oops i made two of these. this one just calls the other one. */
static void gaim_odc_disconnect(aim_session_t *sess, aim_conn_t *conn)
{
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	struct oscar_direct_im *dim;
	char *sn;

	sn = g_strdup(aim_odc_getsn(conn));
	dim = oscar_direct_im_find(od, sn);
	oscar_direct_im_disconnect(od, dim);
	g_free(sn);
}

static void destroy_direct_im_request(struct ask_direct *d) {
	gaim_debug_info("oscar", "Freeing DirectIM prompts.\n");

	g_free(d->sn);
	g_free(d);
}

/* this is just a gaim_proxy_connect cb that sets up the rest of the cbs */
static void oscar_odc_callback(gpointer data, gint source, GaimInputCondition condition) {
	struct oscar_direct_im *dim = data;
	GaimConnection *gc = dim->gc;
	OscarData *od = gc->proto_data;
	GaimConversation *conv;
	char buf[256];
	struct sockaddr name;
	socklen_t name_len = 1;

	g_return_if_fail(gc != NULL);

	dim->gpc_pend = FALSE;
	if (dim->killme) {
		oscar_direct_im_destroy(od, dim);
		return;
	}

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		oscar_direct_im_destroy(od, dim);
		return;
	}

	if (source < 0) {
		if (dim->donttryagain) {
			oscar_direct_im_disconnect(od, dim);
			return;
		} else {
			fu8_t cookie[8];
			char *who = g_strdup(dim->name);
			const char *tmp = aim_odc_getcookie(dim->conn);

			memcpy(cookie, tmp, 8);
			oscar_direct_im_destroy(od, dim);
			oscar_direct_im_initiate(gc, who, cookie);
			gaim_debug_info("oscar", "asking direct im initiator to connect to us\n");
			g_free(who);
			return;
		}
	}

	dim->conn->fd = source;
	aim_conn_completeconnect(od->sess, dim->conn);
	conv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, dim->name);

	/* This is the best way to see if we're connected or not */
	/* Is this really needed? */
	if (getpeername(source, &name, &name_len) == 0) {
		g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), dim->name);
		dim->connected = TRUE;
		gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_SYSTEM, time(NULL));
		dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ, oscar_callback, dim->conn);
	} else {
		if (dim->donttryagain) {
			oscar_direct_im_disconnect(od, dim);
			return;
		} else {
			fu8_t cookie[8];
			char *who = g_strdup(dim->name);
			const char *tmp = aim_odc_getcookie(dim->conn);

			memcpy(cookie, tmp, 8);
			oscar_direct_im_destroy(od, dim);
			oscar_direct_im_initiate(gc, who, cookie);
			gaim_debug_info("oscar", "asking direct im initiator to connect to us\n");
			g_free(who);
			return;
		}
	}


}

static void accept_direct_im_request(struct ask_direct *d) {
	GaimConnection *gc = d->gc;
	OscarData *od;
	struct oscar_direct_im *dim;
	char *host; int port = 5190;
	int i, rc;
	char *tmp;
	GaimConversation *conv;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		destroy_direct_im_request(d);
		return;
	}

	od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "Accepted DirectIM.\n");

	dim = oscar_direct_im_find(od, d->sn);
	if (dim && dim->connected) {
		destroy_direct_im_request(d); /* 40 */ /* what does that 40 mean? */
		gaim_debug_info("oscar", "Wait, we're already connected, ignoring DirectIM.\n");
		return;
	}
	dim = g_new0(struct oscar_direct_im, 1);
	dim->gc = d->gc;
	dim->donttryagain = d->donttryagain;
	g_snprintf(dim->name, sizeof dim->name, "%s", d->sn);

	dim->conn = aim_odc_connect(od->sess, d->sn, NULL, d->cookie);
	od->direct_ims = g_slist_append(od->direct_ims, dim);
	if (!dim->conn) {
		oscar_direct_im_disconnect(od, dim);
		destroy_direct_im_request(d);
		return;
	}

	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_odc_incoming, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_odc_typing, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER,
			        gaim_odc_update_ui, 0);

	gaim_debug_info("oscar", "ip is %s.\n", d->ip);
	for (i = 0; i < (int)strlen(d->ip); i++) {
		if (d->ip[i] == ':') {
			port = atoi(&(d->ip[i+1]));
			break;
		}
	}
	host = g_strndup(d->ip, i);
	dim->conn->status |= AIM_CONN_STATUS_INPROGRESS;
	dim->gpc_pend = TRUE;
	rc = gaim_proxy_connect(gc->account, host, port, oscar_odc_callback, dim);

	conv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, d->sn);
	tmp = g_strdup_printf(_("Attempting to connect to %s at %s:%hu for Direct IM."), d->sn, host,
	                      port);
	gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	g_free(tmp);

	g_free(host);
	if (rc < 0) {
		dim->gpc_pend = FALSE;
		oscar_direct_im_disconnect(od, dim);
		destroy_direct_im_request(d);
		return;
	}

	destroy_direct_im_request(d);

	return;
}

/*
 * We have just established a socket with the other dude, so set up some handlers.
 */
static int gaim_odc_initiate(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	GaimConversation *conv;
	struct oscar_direct_im *dim;
	char buf[256];
	char *sn;
	va_list ap;
	aim_conn_t *newconn, *listenerconn;

	va_start(ap, fr);
	newconn = va_arg(ap, aim_conn_t *);
	listenerconn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	aim_conn_close(listenerconn);
	aim_conn_kill(sess, &listenerconn);

	sn = g_strdup(aim_odc_getsn(newconn));

	gaim_debug_info("oscar",
			   "DirectIM: initiate success to %s\n", sn);
	dim = oscar_direct_im_find(od, sn);

	conv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, sn);
	gaim_input_remove(dim->watcher);
	dim->conn = newconn;
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ, oscar_callback, dim->conn);
	dim->connected = TRUE;
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), sn);
	g_free(sn);
	gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_SYSTEM, time(NULL));

	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING, gaim_odc_incoming, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING, gaim_odc_typing, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER, gaim_odc_update_ui, 0);

	return 1;
}

/*
 * This is called when each chunk of an image is received.  It can be used to
 * update a progress bar, or to eat lots of dry cat food.  Wet cat food is
 * nasty, you sicko.
 */
static int gaim_odc_update_ui(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *sn;
	double percent;
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	GaimConversation *c;
	struct oscar_direct_im *dim;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	percent = va_arg(ap, double);
	va_end(ap);

	if (!sn || !(dim = oscar_direct_im_find(od, sn)))
		return 1;
	if (dim->watcher) {
		gaim_input_remove(dim->watcher);   /* Otherwise, the callback will callback */
		/* The callback will callback? I don't get how that would happen here. */
		dim->watcher = 0;
	}

	c = gaim_find_conversation_with_account(GAIM_CONV_IM, sn,
											gaim_connection_get_account(gc));
	if (c != NULL)
		gaim_conversation_update_progress(c, percent);
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
				      oscar_callback, dim->conn);

	return 1;
}

/*
 * This is called after a direct IM has been received in its entirety.  This
 * function is passed a long chunk of data which contains the IM with any
 * data chunks (images) appended to it.
 *
 * This function rips out all the data chunks and creates an imgstore for
 * each one.  In order to do this, it first goes through the IM and takes
 * out all the IMG tags.  When doing so, it rewrites the original IMG tag
 * with one compatible with the imgstore Gaim core code. For each one, we
 * then read in chunks of data from the end of the message and actually
 * create the img store using the given data.
 *
 * For somewhat easy reference, here's a sample message
 * (without the whitespace and asterisks):
 *
 * <HTML><BODY BGCOLOR="#ffffff">
 *     <FONT LANG="0">
 *     This is a really stupid picture:<BR>
 *     <IMG SRC="Sample.jpg" ID="1" WIDTH="283" HEIGHT="212" DATASIZE="9894"><BR>
 *     Yeah it is<BR>
 *     Here is another one:<BR>
 *     <IMG SRC="Soap Bubbles.bmp" ID="2" WIDTH="256" HEIGHT="256" DATASIZE="65978">
 *     </FONT>
 * </BODY></HTML>
 * <BINARY>
 *     <DATA ID="1" SIZE="9894">datadatadatadata</DATA>
 *     <DATA ID="2" SIZE="65978">datadatadatadata</DATA>
 * </BINARY>
 */
static int gaim_odc_incoming(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimConvImFlags imflags = 0;
	gchar *utf8;
	GString *newmsg = g_string_new("");
	GSList *images = NULL;
	va_list ap;
	const char *sn, *msg, *msgend, *binary;
	size_t len;
	int encoding, isawaymsg;

	va_start(ap, fr);
	sn = va_arg(ap, const char *);
	msg = va_arg(ap, const char *);
	len = va_arg(ap, size_t);
	encoding = va_arg(ap, int);
	isawaymsg = va_arg(ap, int);
	va_end(ap);
	msgend = msg + len;

	gaim_debug_info("oscar",
			   "Got DirectIM message from %s\n", sn);

	if (isawaymsg)
		imflags |= GAIM_CONV_IM_AUTO_RESP;

	/* message has a binary trailer */
	if ((binary = gaim_strcasestr(msg, "<binary>"))) {
		GData *attribs;
		const char *tmp, *start, *end, *last = NULL;

		tmp = msg;

		/* for each valid image tag... */
		while (gaim_markup_find_tag("img", tmp, &start, &end, &attribs)) {
			const char *id, *src, *datasize;
			const char *tag = NULL, *data = NULL;
			size_t size;
			int imgid = 0;

			/* update the location of the last img tag */
			last = end;

			/* grab attributes */
			id       = g_datalist_get_data(&attribs, "id");
			src      = g_datalist_get_data(&attribs, "src");
			datasize = g_datalist_get_data(&attribs, "datasize");

			/* if we have id & datasize, build the data tag */
			if (id && datasize)
				tag = g_strdup_printf("<data id=\"%s\" size=\"%s\">", id, datasize);

			/* if we have a tag, find the start of the data */
			if (tag && (data = gaim_strcasestr(binary, tag)))
				data += strlen(tag);

			/* check the data is here and store it */
			if (data + (size = atoi(datasize)) <= msgend)
				imgid = gaim_imgstore_add(data, size, src);

			/*
			 * XXX - The code below contains some calls to oscar_encoding_to_utf8
			 * The hardcoded "us-ascii" value REALLY needs to be removed.
			 */
			/* if we have a stored image... */
			if (imgid) {
				/* append the message up to the tag */
				utf8 = oscar_encoding_to_utf8("us-ascii", tmp, start - tmp);
				if (utf8 != NULL) {
					newmsg = g_string_append(newmsg, utf8);
					g_free(utf8);
				}

				/* write the new image tag */
				g_string_append_printf(newmsg, "<IMG ID=\"%d\">", imgid);

				/* and record the image number */
				images = g_slist_append(images, GINT_TO_POINTER(imgid));
			} else {
				/* otherwise, copy up to the end of the tag */
				utf8 = oscar_encoding_to_utf8("us-ascii", tmp, (end + 1) - tmp);
				if (utf8 != NULL) {
					newmsg = g_string_append(newmsg, utf8);
					g_free(utf8);
				}
			}

			/* clear the attribute list */
			g_datalist_clear(&attribs);

			/* continue from the end of the tag */
			tmp = end + 1;
		}

		/* append any remaining message data (without the > :-) */
		if (last++ && (last < binary))
			newmsg = g_string_append_len(newmsg, last, binary - last);

		/* set the flag if we caught any images */
		if (images)
			imflags |= GAIM_CONV_IM_IMAGES;
	} else {
		g_string_append_len(newmsg, msg, len);
	}

	/* Convert to UTF8 */
	/* (This hasn't been tested very much) */
	utf8 = gaim_plugin_oscar_decode_im_part(gc->account, sn, encoding, 0x0000, newmsg->str, len);
	if (utf8 != NULL) {
		serv_got_im(gc, sn, utf8, imflags, time(NULL));
		g_free(utf8);
	}

	/* free the message */
	g_string_free(newmsg, TRUE);

	/* unref any images we allocated */
	if (images) {
		GSList *tmp;
		int id;

		for (tmp = images; tmp != NULL; tmp = tmp->next) {
			id = GPOINTER_TO_INT(tmp->data);
			gaim_imgstore_unref(id);
		}

		g_slist_free(images);
	}

	return 1;
}

static int gaim_odc_typing(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *sn;
	int typing;
	GaimConnection *gc = sess->aux_data;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	typing = va_arg(ap, int);
	va_end(ap);

	if (typing == 0x0002) {
		/* I had to leave this. It's just too funny. It reminds me of my sister. */
		gaim_debug_info("oscar",
				   "ohmigod! %s has started typing (DirectIM). He's going to send you a message! *squeal*\n", sn);
		serv_got_typing(gc, sn, 0, GAIM_TYPING);
	} else if (typing == 0x0001)
		serv_got_typing(gc, sn, 0, GAIM_TYPED);
	else
		serv_got_typing_stopped(gc, sn);
	return 1;
}

static int gaim_odc_send_im(aim_session_t *sess, aim_conn_t *conn, const char *message, GaimConvImFlags imflags) {
	char *buf;
	size_t len;
	int ret;
	GString *msg = g_string_new("<HTML><BODY>");
	GString *data = g_string_new("</BODY></HTML><BINARY>");
	GData *attribs;
	const char *start, *end, *last;
	int oscar_id = 0;

	last = message;

	/* for each valid IMG tag... */
	while (last && *last && gaim_markup_find_tag("img", last, &start, &end, &attribs)) {
		GaimStoredImage *image = NULL;
		const char *id;

		if (start - last) {
			g_string_append_len(msg, last, start - last);
		}

		id = g_datalist_get_data(&attribs, "id");

		/* ... if it refers to a valid gaim image ... */
		if (id && (image = gaim_imgstore_get(atoi(id)))) {
			/* ... append the message from start to the tag ... */
			unsigned long size = gaim_imgstore_get_size(image);
			const char *filename = gaim_imgstore_get_filename(image);
			gpointer imgdata = gaim_imgstore_get_data(image);

			oscar_id++;

			/* ... insert a new img tag with the oscar id ... */
			if (filename)
				g_string_append_printf(msg,
					"<IMG SRC=\"%s\" ID=\"%d\" DATASIZE=\"%lu\">",
					filename, oscar_id, size);
			else
				g_string_append_printf(msg,
					"<IMG ID=\"%d\" DATASIZE=\"%lu\">",
					oscar_id, size);

			/* ... and append the data to the binary section ... */
			g_string_append_printf(data, "<DATA ID=\"%d\" SIZE=\"%lu\">",
				oscar_id, size);
			data = g_string_append_len(data, imgdata, size);
			data = g_string_append(data, "</DATA>");
		}
			/* If the tag is invalid, skip it, thus no else here */

		g_datalist_clear(&attribs);

		/* continue from the end of the tag */
		last = end + 1;
	}

	/* append any remaining message data (without the > :-) */
	if (last && *last)
		msg = g_string_append(msg, last);

	/* if we inserted any images in the binary section, append it */
	if (oscar_id) {
		msg = g_string_append_len(msg, data->str, data->len);
		msg = g_string_append(msg, "</BINARY>");
	}

	len = msg->len;
	buf = msg->str;
	g_string_free(msg, FALSE);
	g_string_free(data, TRUE);


	/* XXX - The last parameter below is the encoding.  Let Paco-Paco do something with it. */
	if (imflags & GAIM_CONV_IM_AUTO_RESP)
		ret =  aim_odc_send_im(sess, conn, buf, len, 0, 1);
	else
		ret =  aim_odc_send_im(sess, conn, buf, len, 0, 0);

	g_free(buf);

	return ret;
}

struct ask_do_dir_im {
	char *who;
	GaimConnection *gc;
};

static void oscar_cancel_direct_im(struct ask_do_dir_im *data) {
	g_free(data->who);
	g_free(data);
}

/* this function is used to initiate a direct im session with someone.
 * we start listening on a port and send a request. they either connect
 * or send some kind of reply. If they can't connect, they ask us to
 * connect to them, and so we do that.
 *
 * this function will also get called if the other side initiate's a direct
 * im and we try to connect and fail. in that case cookie will not be null.
 *
 * note that cookie is an 8 byte string that isn't NULL terminated
 */
static void oscar_direct_im_initiate(GaimConnection *gc, const char *who, const char *cookie) {
	OscarData *od;
	struct oscar_direct_im *dim;
	int listenfd;
	const char *ip;

	od = (OscarData *)gc->proto_data;

	dim = oscar_direct_im_find(od, who);
	if (dim) {
		if (!(dim->connected)) {  /* We'll free the old, unconnected dim, and start over */
			oscar_direct_im_disconnect(od, dim);
			gaim_debug_info("oscar",
					   "Gave up on old direct IM, trying again\n");
		} else {
			gaim_notify_error(gc, NULL, "DirectIM already open.", NULL);
			return;
		}
	}
	dim = g_new0(struct oscar_direct_im, 1);
	dim->gc = gc;
	g_snprintf(dim->name, sizeof dim->name, "%s", who);

	listenfd = gaim_network_listen_range(5190, 5199);
	ip = gaim_network_get_my_ip(od->conn ? od->conn->fd : -1);
	if (listenfd >= 0)
		dim->conn = aim_odc_initiate(od->sess, who, listenfd, gaim_network_ip_atoi(ip), gaim_network_get_port_from_fd(listenfd), cookie);
	if (dim->conn != NULL) {
		char *tmp;
		GaimConversation *conv;

		od->direct_ims = g_slist_append(od->direct_ims, dim);
		dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
						oscar_callback, dim->conn);
		aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIM_ESTABLISHED,
					gaim_odc_initiate, 0);

		conv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, who);
		tmp = g_strdup_printf(_("Asking %s to connect to us at %s:%hu for Direct IM."), who, ip,
		                      gaim_network_get_port_from_fd(listenfd));
		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(tmp);
	} else {
		gaim_notify_error(gc, NULL, _("Unable to open Direct IM"), NULL);
		oscar_direct_im_destroy(od, dim);
	}
}

static void oscar_direct_im(struct ask_do_dir_im *data) {
	GaimConnection *gc = data->gc;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		g_free(data->who);
		g_free(data);
		return;
	}

	oscar_direct_im_initiate(gc, data->who, NULL);
	g_free(data->who);
	g_free(data);
}

/* this is the right click menu cb thingy */
static void oscar_ask_direct_im(GaimBlistNode *node, gpointer ignored) {

	GaimBuddy *buddy;
	GaimConnection *gc;
	gchar *buf;
	struct ask_do_dir_im *data;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	data = g_new0(struct ask_do_dir_im, 1);
	data->who = g_strdup(buddy->name);
	data->gc = gc;
	buf = g_strdup_printf(_("You have selected to open a Direct IM connection with %s."),
			buddy->name);

	gaim_request_action(gc, NULL, buf,
			_("Because this reveals your IP address, it "
			  "may be considered a privacy risk.  Do you "
			  "wish to continue?"),
			0, data, 2,
			_("Connect"), G_CALLBACK(oscar_direct_im),
			_("Cancel"), G_CALLBACK(oscar_cancel_direct_im));
	g_free(buf);
}

/*****************************************************************************
 * End scary direct im stuff
 *****************************************************************************/

static void oscar_callback(gpointer data, gint source, GaimInputCondition condition) {
	aim_conn_t *conn = (aim_conn_t *)data;
	aim_session_t *sess = aim_conn_getsess(conn);
	GaimConnection *gc = sess ? sess->aux_data : NULL;
	OscarData *od;

	if (!gc) {
		gaim_debug_info("oscar",
				   "oscar callback for closed connection (1).\n");
		return;
	}
      
	od = (OscarData *)gc->proto_data;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		/* oh boy. this is probably bad. i guess the only thing we 
		 * can really do is return? */
		gaim_debug_info("oscar",
				   "oscar callback for closed connection (2).\n");
		gaim_debug_misc("oscar", "gc = %p\n", gc);
		return;
	}

	if (condition & GAIM_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_LISTENER) {
			gaim_debug_info("oscar",
					   "got information on rendezvous listener\n");
			if (aim_handlerendconnect(od->sess, conn) < 0) {
				gaim_debug_error("oscar",
						   "connection error (rendezvous listener)\n");
				aim_conn_kill(od->sess, &conn);
				/* AAA - Don't we need to gaim_xfer_cancel here? --marv */
			}
		} else {
			if (aim_get_command(od->sess, conn) >= 0) {
				aim_rxdispatch(od->sess);
				if (od->killme) {
					gaim_debug_error("oscar", "Waiting to be destroyed\n");
					return;
				}
			} else {
				if ((conn->type == AIM_CONN_TYPE_BOS) ||
					   !(aim_getconn_type(od->sess, AIM_CONN_TYPE_BOS))) {
					gaim_debug_error("oscar",
							   "major connection error\n");
					gaim_connection_error(gc, _("Disconnected."));
				} else if (conn->type == AIM_CONN_TYPE_CHAT) {
					struct chat_connection *c = find_oscar_chat_by_conn(gc, conn);
					GaimConversation *conv = gaim_find_chat(gc, c->id);
					char *buf;
					gaim_debug_info("oscar",
							   "disconnected from chat room %s\n", c->name);
					c->conn = NULL;
					if (c->inpa > 0)
						gaim_input_remove(c->inpa);
					c->inpa = 0;
					c->fd = -1;
					aim_conn_kill(od->sess, &conn);
					buf = g_strdup_printf(_("You have been disconnected from chat room %s."), c->name);
					if (conv) 
						gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_ERROR, time(NULL));
					else
						gaim_notify_error(gc, NULL, buf, NULL);
					g_free(buf);
				} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
					if (od->cnpa > 0)
						gaim_input_remove(od->cnpa);
					od->cnpa = 0;
					gaim_debug_info("oscar",
							   "removing chatnav input watcher\n");
					while (od->create_rooms) {
						struct create_room *cr = od->create_rooms->data;
						g_free(cr->name);
						od->create_rooms =
							g_slist_remove(od->create_rooms, cr);
						g_free(cr);
						gaim_notify_error(gc, NULL,
										  _("Chat is currently unavailable"),
										  NULL);
					}
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_AUTH) {
					if (od->paspa > 0)
						gaim_input_remove(od->paspa);
					od->paspa = 0;
					gaim_debug_info("oscar",
							   "removing authconn input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_EMAIL) {
					if (od->emlpa > 0)
						gaim_input_remove(od->emlpa);
					od->emlpa = 0;
					gaim_debug_info("oscar",
							   "removing email input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_ICON) {
					if (od->icopa > 0)
						gaim_input_remove(od->icopa);
					od->icopa = 0;
					gaim_debug_info("oscar",
							   "removing icon input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
					if (conn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM)
						gaim_odc_disconnect(od->sess, conn);
					aim_conn_kill(od->sess, &conn);
				} else {
					gaim_debug_error("oscar",
							   "holy crap! generic connection error! %hu\n",
							   conn->type);
					aim_conn_kill(od->sess, &conn);
				}
			}
		}
	}
}

static void oscar_debug(aim_session_t *sess, int level, const char *format, va_list va) {
	GaimConnection *gc = sess->aux_data;
	gchar *s = g_strdup_vprintf(format, va);
	gchar *buf;

	buf = g_strdup_printf("%s %d: %s", gaim_account_get_username(gaim_connection_get_account(gc)), level, s);
	gaim_debug_info("oscar", buf);
	if (buf[strlen(buf)-1] != '\n')
		gaim_debug_info(NULL, "\n");
	g_free(buf);
	g_free(s);
}

static void oscar_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *conn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	conn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);
	conn->fd = source;

	if (source < 0) {
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	aim_conn_completeconnect(sess, conn);
	gc->inpa = gaim_input_add(conn->fd, GAIM_INPUT_READ, oscar_callback, conn);
	aim_request_login(sess, conn, gaim_account_get_username(gaim_connection_get_account(gc)));

	gaim_debug_info("oscar",
			   "Screen name sent, waiting for response\n");
	gaim_connection_update_progress(gc, _("Screen name sent"), 1, OSCAR_CONNECT_STEPS);
	ck[1] = 0x65;
}

static void oscar_login(GaimAccount *account, GaimStatus *status) {
	aim_session_t *sess;
	aim_conn_t *conn;
	GaimConnection *gc = gaim_account_get_connection(account);
	OscarData *od = gc->proto_data = g_new0(OscarData, 1);
	GaimStatusType *status_type;
	GaimStatusPrimitive primitive;

	status_type = gaim_status_get_type(status);
	primitive = gaim_status_type_get_primitive(status_type);

	gaim_debug_misc("oscar", "oscar_login: gc = %p\n", gc);
	
	if (primitive == GAIM_STATUS_OFFLINE)
		return;

	if (!aim_snvalid(gaim_account_get_username(account))) {
		gchar *buf;
		buf = g_strdup_printf(_("Unable to login: Could not sign on as %s because the screen name is invalid.  Screen names must either start with a letter and contain only letters, numbers and spaces, or contain only numbers."), gaim_account_get_username(account));
		gaim_connection_error(gc, buf);
		g_free(buf);
	}

	if (aim_sn_is_icq((gaim_account_get_username(account)))) {
		od->icq = TRUE;
	} else {
		gc->flags |= GAIM_CONNECTION_HTML;
		gc->flags |= GAIM_CONNECTION_AUTO_RESP;
	}
	od->buddyinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, oscar_free_buddyinfo);

	sess = g_new0(aim_session_t, 1);
	aim_session_init(sess, TRUE, FAIM_DEBUG_LEVEL);
	aim_setdebuggingcb(sess, oscar_debug);
	/*
	 * We need an immediate queue because we don't use a while-loop 
	 * to see if things need to be sent.
	 */
	aim_tx_setenqueue(sess, AIM_TX_IMMEDIATE, NULL);
	od->sess = sess;
	sess->aux_data = gc;

	/* Connect to core Gaim signals */
	gaim_prefs_connect_callback(gc, "/plugins/prpl/oscar/recent_buddies", recent_buddies_cb, gc);

	conn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
	if (conn == NULL) {
		gaim_debug_error("oscar",
				   "internal connection error\n");
		gaim_connection_error(gc, _("Unable to login to AIM"));
		return;
	}

	aim_conn_addhandler(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0007, gaim_parse_login, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0003, gaim_parse_auth_resp, 0);

	conn->status |= AIM_CONN_STATUS_INPROGRESS;
	if (gaim_proxy_connect(account, gaim_account_get_string(account, "server", FAIM_LOGIN_SERVER),
			  gaim_account_get_int(account, "port", FAIM_LOGIN_PORT),
			  oscar_login_connect, gc) < 0) {
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	gaim_connection_update_progress(gc, _("Connecting"), 0, OSCAR_CONNECT_STEPS);
	ck[0] = 0x5a;
}

static void oscar_close(GaimConnection *gc) {
	OscarData *od = (OscarData *)gc->proto_data;

	while (od->oscar_chats) {
		struct chat_connection *n = od->oscar_chats->data;
		if (n->inpa > 0)
			gaim_input_remove(n->inpa);
		g_free(n->name);
		g_free(n->show);
		od->oscar_chats = g_slist_remove(od->oscar_chats, n);
		g_free(n);
	}
	while (od->direct_ims) {
		struct oscar_direct_im *n = od->direct_ims->data;
		oscar_direct_im_destroy(od, n);
	}
/* BBB */
	while (od->file_transfers) {
		GaimXfer *xfer;
		xfer = (GaimXfer *)od->file_transfers->data;
		gaim_xfer_cancel_local(xfer);
	}
	while (od->requesticon) {
		char *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		free(sn);
	}
	g_hash_table_destroy(od->buddyinfo);
	while (od->create_rooms) {
		struct create_room *cr = od->create_rooms->data;
		g_free(cr->name);
		od->create_rooms = g_slist_remove(od->create_rooms, cr);
		g_free(cr);
	}
	if (od->email)
		g_free(od->email);
	if (od->newp)
		g_free(od->newp);
	if (od->oldp)
		g_free(od->oldp);
	if (gc->inpa > 0)
		gaim_input_remove(gc->inpa);
	if (od->cnpa > 0)
		gaim_input_remove(od->cnpa);
	if (od->paspa > 0)
		gaim_input_remove(od->paspa);
	if (od->emlpa > 0)
		gaim_input_remove(od->emlpa);
	if (od->icopa > 0)
		gaim_input_remove(od->icopa);
	if (od->icontimer > 0)
		gaim_timeout_remove(od->icontimer);
	if (od->getblisttimer > 0)
		gaim_timeout_remove(od->getblisttimer);
	if (od->getinfotimer > 0)
		gaim_timeout_remove(od->getinfotimer);
	gaim_prefs_disconnect_by_handle(gc);

	aim_session_kill(od->sess);
	g_free(od->sess);
	od->sess = NULL;
	g_free(gc->proto_data);
	gc->proto_data = NULL;
	gaim_debug_info("oscar", "Signed off.\n");
}

static void oscar_bos_connect(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *bosconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	bosconn = od->conn;	
	bosconn->fd = source;

	if (source < 0) {
		gaim_connection_error(gc, _("Could Not Connect"));
		return;
	}

	aim_conn_completeconnect(sess, bosconn);
	gc->inpa = gaim_input_add(bosconn->fd, GAIM_INPUT_READ, oscar_callback, bosconn);

	gaim_connection_update_progress(gc,
			_("Connection established, cookie sent"), 4, OSCAR_CONNECT_STEPS);
	ck[4] = 0x61;
}

/* BBB */
/*
 * This little area in oscar.c is the nexus of file transfer code, 
 * so I wrote a little explanation of what happens.  I am such a 
 * ninja.
 *
 * The series of events for a file send is:
 *  -Create xfer and call gaim_xfer_request (this happens in oscar_ask_sendfile)
 *  -User chooses a file and oscar_xfer_init is called.  It establishes a 
 *   listening socket, then asks the remote user to connect to us (and 
 *   gives them the file name, port, IP, etc.)
 *  -They connect to us and we send them an AIM_CB_OFT_PROMPT (this happens 
 *   in oscar_sendfile_estblsh)
 *  -They send us an AIM_CB_OFT_ACK and then we start sending data
 *  -When we finish, they send us an AIM_CB_OFT_DONE and they close the 
 *   connection.
 *  -We get drunk because file transfer kicks ass.
 *
 * The series of events for a file receive is:
 *  -Create xfer and call gaim_xfer request (this happens in incomingim_chan2)
 *  -Gaim user selects file to name and location to save file to and 
 *   oscar_xfer_init is called
 *  -It connects to the remote user using the IP they gave us earlier
 *  -After connecting, they send us an AIM_CB_OFT_PROMPT.  In reply, we send 
 *   them an AIM_CB_OFT_ACK.
 *  -They begin to send us lots of raw data.
 *  -When they finish sending data we send an AIM_CB_OFT_DONE and then close 
 *   the connection.
 */
static void oscar_sendfile_connected(gpointer data, gint source, GaimInputCondition condition);

/*
 * Miscellaneous xfer functions
 */
static GaimXfer *oscar_find_xfer_by_cookie(GSList *fts, const fu8_t *ck)
{
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;

	while (fts) {
		xfer = fts->data;
		oft_info = xfer->data;

		if (oft_info && !memcmp(ck, oft_info->cookie, 8))
			return xfer;

		fts = g_slist_next(fts);
	}

	return NULL;
}

static GaimXfer *oscar_find_xfer_by_conn(GSList *fts, aim_conn_t *conn)
{
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;

	while (fts) {
		xfer = fts->data;
		oft_info = xfer->data;

		if (oft_info && (conn == oft_info->conn))
			return xfer;

		fts = g_slist_next(fts);
	}

	return NULL;
}

static void oscar_xfer_end(GaimXfer *xfer)
{
	struct aim_oft_info *oft_info = xfer->data;
	GaimConnection *gc = oft_info->sess->aux_data;
	OscarData *od = gc->proto_data;

	gaim_debug_info("oscar", "AAA - in oscar_xfer_end\n");

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		oft_info->fh.nrecvd = gaim_xfer_get_bytes_sent(xfer);
		aim_oft_sendheader(oft_info->sess, AIM_CB_OFT_DONE, oft_info);
	}

	aim_conn_kill(oft_info->sess, &oft_info->conn);
	aim_oft_destroyinfo(oft_info);
	xfer->data = NULL;
	od->file_transfers = g_slist_remove(od->file_transfers, xfer);
}

/*
 * xfer functions used when receiving files
 */

static void oscar_xfer_init_recv(GaimXfer *xfer)
{
	struct aim_oft_info *oft_info = xfer->data;
	GaimConnection *gc = oft_info->sess->aux_data;
	OscarData *od = gc->proto_data;

	gaim_debug_info("oscar", "AAA - in oscar_xfer_recv_init\n");

	oft_info->conn = aim_newconn(od->sess, AIM_CONN_TYPE_RENDEZVOUS, NULL);
	if (oft_info->conn) {
		oft_info->conn->subtype = AIM_CONN_SUBTYPE_OFT_SENDFILE;
		aim_conn_addhandler(od->sess, oft_info->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_PROMPT, oscar_sendfile_prompt, 0);
		oft_info->conn->fd = xfer->fd = gaim_proxy_connect(gaim_connection_get_account(gc),
					xfer->remote_ip, xfer->remote_port,	oscar_sendfile_connected, xfer);
		if (xfer->fd == -1) {
			gaim_xfer_error(GAIM_XFER_RECEIVE, xfer->who,
							_("Unable to establish file descriptor."));
			gaim_xfer_cancel_local(xfer);
		}
	} else {
		gaim_xfer_error(GAIM_XFER_RECEIVE, xfer->who,
						_("Unable to create new connection."));
		gaim_xfer_cancel_local(xfer);
		/* Try a different port? Ask them to connect to us? /join #gaim and whine? */
	}

}

static void oscar_xfer_cancel_recv(GaimXfer *xfer)
{
	struct aim_oft_info *oft_info;
	GaimConnection *gc;
	OscarData *od;

	g_return_if_fail(xfer != NULL);
	g_return_if_fail(xfer->data != NULL);

	oft_info = xfer->data;
	gc = oft_info->sess->aux_data;
	od = gc->proto_data;

	gaim_debug_info("oscar", "AAA - in oscar_xfer_cancel_recv\n");

	if (gaim_xfer_get_status(xfer) != GAIM_XFER_STATUS_CANCEL_REMOTE)
		aim_im_sendch2_sendfile_cancel(oft_info->sess, oft_info);

	aim_conn_kill(oft_info->sess, &oft_info->conn);
	aim_oft_destroyinfo(oft_info);
	xfer->data = NULL;
	od->file_transfers = g_slist_remove(od->file_transfers, xfer);
}

static void oscar_xfer_ack_recv(GaimXfer *xfer, const char *buffer, size_t size)
{
	struct aim_oft_info *oft_info = xfer->data;

	/* Update our rolling checksum.  Like Walmart, yo. */
	oft_info->fh.recvcsum = aim_oft_checksum_chunk(buffer, size, oft_info->fh.recvcsum);
}

/*
 * xfer functions used when sending files
 */

static void oscar_xfer_init_send(GaimXfer *xfer)
{
	struct aim_oft_info *oft_info = xfer->data;
	GaimConnection *gc = oft_info->sess->aux_data;
	OscarData *od = gc->proto_data;
	int listenfd;

	gaim_debug_info("oscar", "AAA - in oscar_xfer_send_init\n");

	xfer->filename = g_path_get_basename(xfer->local_filename);
	strncpy(oft_info->fh.name, xfer->filename, 64);
	oft_info->fh.name[63] = '\0';
	oft_info->fh.totsize = gaim_xfer_get_size(xfer);
	oft_info->fh.size = gaim_xfer_get_size(xfer);
	oft_info->fh.checksum = aim_oft_checksum_file(xfer->local_filename);

	/* Create a listening socket and an associated libfaim conn */
	if ((listenfd = gaim_network_listen_range(5190, 5199)) < 0) {
		gaim_xfer_cancel_local(xfer);
		return;
	}
	xfer->local_port = gaim_network_get_port_from_fd(listenfd);
	oft_info->port = xfer->local_port;
	if (aim_sendfile_listen(od->sess, oft_info, listenfd) != 0) {
		gaim_xfer_cancel_local(xfer);
		return;
	}
	gaim_debug_misc("oscar",
			   "port is %hu, ip is %s\n",
			   xfer->local_port, oft_info->clientip);
	if (oft_info->conn) {
		xfer->watcher = gaim_input_add(oft_info->conn->fd, GAIM_INPUT_READ, oscar_callback, oft_info->conn);
		aim_im_sendch2_sendfile_ask(od->sess, oft_info);
		aim_conn_addhandler(od->sess, oft_info->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_ESTABLISHED, oscar_sendfile_estblsh, 0);
	} else {
		gaim_xfer_error(GAIM_XFER_SEND, xfer->who,
						_("Unable to establish listener socket."));
		gaim_xfer_cancel_local(xfer);
	}
}

static void oscar_xfer_cancel_send(GaimXfer *xfer)
{
	struct aim_oft_info *oft_info = xfer->data;
	GaimConnection *gc = oft_info->sess->aux_data;
	OscarData *od = gc->proto_data;

	gaim_debug_info("oscar", "AAA - in oscar_xfer_cancel_send\n");

	if (gaim_xfer_get_status(xfer) != GAIM_XFER_STATUS_CANCEL_REMOTE)
		aim_im_sendch2_sendfile_cancel(oft_info->sess, oft_info);

	aim_conn_kill(oft_info->sess, &oft_info->conn);
	aim_oft_destroyinfo(oft_info);
	xfer->data = NULL;
	od->file_transfers = g_slist_remove(od->file_transfers, xfer);
}

static void oscar_xfer_ack_send(GaimXfer *xfer, const char *buffer, size_t size)
{
	struct aim_oft_info *oft_info = xfer->data;

	/* I'm not sure I like how we do this. --marv
	 * I do.  AIM file transfers aren't really meant to be thought
	 * of as a transferring just a single file.  The rendezvous
	 * establishes a connection between two computers, and then
	 * those computers can use the same connection for transferring
	 * multiple files.  So we don't want the Gaim core up and closing
	 * the socket all willy-nilly.  We want to do that in the oscar
	 * prpl, whenever one side or the other says they're finished
	 * using the connection.  There might be a better way to intercept
	 * the socket from the core, however...  --KingAnt
	 */

	/*
	 * If we're done sending, intercept the socket from the core ft code
	 * and wait for the other guy to send the "done" OFT packet.
	 */
	if (gaim_xfer_get_bytes_remaining(xfer) <= 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = gaim_input_add(xfer->fd, GAIM_INPUT_READ, oscar_callback, oft_info->conn);
		xfer->fd = 0;
		gaim_xfer_set_completed(xfer, TRUE);
	}
}

static gboolean oscar_can_receive_file(GaimConnection *gc, const char *who) {
	gboolean can_receive = FALSE;
	OscarData *od = gc->proto_data;

	if (!od->icq) {
		aim_userinfo_t *userinfo;
		userinfo = aim_locate_finduserinfo(od->sess, who);
		if (userinfo && userinfo->capabilities & AIM_CAPS_SENDFILE)
			can_receive = TRUE;
	}

	return can_receive;
}

static void oscar_send_file(GaimConnection *gc, const char *who, const char *file) {

	OscarData *od;
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;
	const char *ip;

	od = (OscarData *)gc->proto_data;

	/* You want to send a file to someone else, you're so generous */

	/* Build the file transfer handle */
	xfer = gaim_xfer_new(gc->account, GAIM_XFER_SEND, who);

	/* Create the oscar-specific data */
	ip = gaim_network_get_my_ip(od->conn ? od->conn->fd : -1);
	oft_info = aim_oft_createinfo(od->sess, NULL, who, ip, 0, 0, 0, NULL);
	xfer->data = oft_info;

	 /* Setup our I/O op functions */
	gaim_xfer_set_init_fnc(xfer, oscar_xfer_init_send);
	gaim_xfer_set_end_fnc(xfer, oscar_xfer_end);
	gaim_xfer_set_cancel_send_fnc(xfer, oscar_xfer_cancel_send);
	gaim_xfer_set_request_denied_fnc(xfer, oscar_xfer_cancel_send);
	gaim_xfer_set_ack_fnc(xfer, oscar_xfer_ack_send);

	/* Keep track of this transfer for later */
	od->file_transfers = g_slist_append(od->file_transfers, xfer);

	/* Now perform the request */
	if (file)
		gaim_xfer_request_accepted(xfer, file);
	else
		gaim_xfer_request(xfer);
}

static int gaim_parse_auth_resp(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimAccount *account = gc->account;
	aim_conn_t *bosconn;
	char *host; int port;
	int i, rc;
	va_list ap;
	struct aim_authresp_info *info;

	port = gaim_account_get_int(account, "port", FAIM_LOGIN_PORT);

	va_start(ap, fr);
	info = va_arg(ap, struct aim_authresp_info *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "inside auth_resp (Screen name: %s)\n", info->sn);

	if (info->errorcode || !info->bosip || !info->cookielen || !info->cookie) {
		char buf[256];
		switch (info->errorcode) {
		case 0x05:
			/* Incorrect nick/password */
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("Incorrect nickname or password."));
			break;
		case 0x11:
			/* Suspended account */
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("Your account is currently suspended."));
			break;
		case 0x14:
			/* service temporarily unavailable */
			gaim_connection_error(gc, _("The AOL Instant Messenger service is temporarily unavailable."));
			break;
		case 0x18:
			/* connecting too frequently */
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer."));
			break;
		case 0x1c:
			/* client too old */
			gc->wants_to_die = TRUE;
			g_snprintf(buf, sizeof(buf), _("The client version you are using is too old. Please upgrade at %s"), GAIM_WEBSITE);
			gaim_connection_error(gc, buf);
			break;
		default:
			gaim_connection_error(gc, _("Authentication failed"));
			break;
		}
		gaim_debug_error("oscar",
				   "Login Error Code 0x%04hx\n", info->errorcode);
		gaim_debug_error("oscar",
				   "Error URL: %s\n", info->errorurl);
		od->killme = TRUE;
		return 1;
	}


	gaim_debug_misc("oscar", "Reg status: %hu\n", info->regstatus);

	if (info->email) {
		gaim_debug_misc("oscar", "Email: %s\n", info->email);
	} else {
		gaim_debug_misc("oscar", "Email is NULL\n");
	}

	gaim_debug_misc("oscar", "BOSIP: %s\n", info->bosip);
	gaim_debug_info("oscar",
			   "Closing auth connection...\n");
	aim_conn_kill(sess, &fr->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, NULL);
	if (bosconn == NULL) {
		gaim_connection_error(gc, _("Internal Error"));
		od->killme = TRUE;
		return 0;
	}

	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_bos, 0);
	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, gaim_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_RIGHTSINFO, gaim_parse_locaterights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, gaim_parse_buddyrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, gaim_parse_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, gaim_parse_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, gaim_parse_incoming_im, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, gaim_parse_locerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_CLIENTAUTORESP, gaim_parse_clientauto, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, gaim_parse_ratechange, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_EVIL, gaim_parse_evilnotify, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, AIM_CB_LOK_ERROR, gaim_parse_searcherror, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, 0x0003, gaim_parse_searchreply, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, gaim_parse_msgerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MTN, gaim_parse_mtn, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, gaim_parse_userinfo, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_REQUESTINFOTIMEOUT, gaim_reqinfo_timeout, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ACK, gaim_parse_msgack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, gaim_parse_motd, 0);
	aim_conn_addhandler(sess, bosconn, 0x0004, 0x0005, gaim_icbm_param_info, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0003, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x001f, gaim_memrequest, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x000f, gaim_selfinfo, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0021, oscar_icon_req,0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSG, gaim_offlinemsg, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSGCOMPLETE, gaim_offlinemsgdone, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_POP, 0x0002, gaim_popup, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_ALIAS, gaim_icqalias, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_INFO, gaim_icqinfo, 0);
#ifndef NOSSI
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_ERROR, gaim_ssi_parseerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RIGHTSINFO, gaim_ssi_parserights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_LIST, gaim_ssi_parselist, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_NOLIST, gaim_ssi_parselist, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_SRVACK, gaim_ssi_parseack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_ADD, gaim_ssi_parseadd, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTH, gaim_ssi_authgiven, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTHREQ, gaim_ssi_authrequest, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTHREP, gaim_ssi_authreply, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_ADDED, gaim_ssi_gotadded, 0);
#endif

	od->conn = bosconn;
	for (i = 0; i < (int)strlen(info->bosip); i++) {
		if (info->bosip[i] == ':') {
			port = atoi(&(info->bosip[i+1]));
			break;
		}
	}
	host = g_strndup(info->bosip, i);
	bosconn->status |= AIM_CONN_STATUS_INPROGRESS;
	rc = gaim_proxy_connect(gc->account, host, port, oscar_bos_connect, gc);
	g_free(host);
	if (rc < 0) {
		gaim_connection_error(gc, _("Could Not Connect"));
		od->killme = TRUE;
		return 0;
	}
	aim_sendcookie(sess, bosconn, info->cookielen, info->cookie);
	gaim_input_remove(gc->inpa);

	gaim_connection_update_progress(gc, _("Received authorization"), 3, OSCAR_CONNECT_STEPS);
	ck[3] = 0x64;

	return 1;
}

/* XXX - Should use gaim_url_fetch for the below stuff */
struct pieceofcrap {
	GaimConnection *gc;
	unsigned long offset;
	unsigned long len;
	char *modname;
	int fd;
	aim_conn_t *conn;
	unsigned int inpa;
};

static void damn_you(gpointer data, gint source, GaimInputCondition c)
{
	struct pieceofcrap *pos = data;
	OscarData *od = pos->gc->proto_data;
	char in = '\0';
	int x = 0;
	unsigned char m[17];

	while (read(pos->fd, &in, 1) == 1) {
		if (in == '\n')
			x++;
		else if (in != '\r')
			x = 0;
		if (x == 2)
			break;
		in = '\0';
	}
	if (in != '\n') {
		char buf[256];
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), GAIM_WEBSITE);
		gaim_notify_warning(pos->gc, NULL,
							_("Gaim was unable to get a valid AIM login hash."),
							buf);
		gaim_input_remove(pos->inpa);
		close(pos->fd);
		g_free(pos);
		return;
	}
	read(pos->fd, m, 16);
	m[16] = '\0';
	gaim_debug_misc("oscar", "Sending hash: ");
	for (x = 0; x < 16; x++)
		gaim_debug_misc(NULL, "%02hhx ", (unsigned char)m[x]);

	gaim_debug_misc(NULL, "\n");
	gaim_input_remove(pos->inpa);
	close(pos->fd);
	aim_sendmemblock(od->sess, pos->conn, 0, 16, m, AIM_SENDMEMBLOCK_FLAG_ISHASH);
	g_free(pos);
}

static void straight_to_hell(gpointer data, gint source, GaimInputCondition cond) {
	struct pieceofcrap *pos = data;
	gchar *buf;

	pos->fd = source;

	if (source < 0) {
		buf = g_strdup_printf(_("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), GAIM_WEBSITE);
		gaim_notify_warning(pos->gc, NULL,
							_("Gaim was unable to get a valid AIM login hash."),
							buf);
		g_free(buf);
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		return;
	}

	buf = g_strdup_printf("GET " AIMHASHDATA "?offset=%ld&len=%ld&modname=%s HTTP/1.0\n\n",
			pos->offset, pos->len, pos->modname ? pos->modname : "");
	write(pos->fd, buf, strlen(buf));
	g_free(buf);
	if (pos->modname)
		g_free(pos->modname);
	pos->inpa = gaim_input_add(pos->fd, GAIM_INPUT_READ, damn_you, pos);
	return;
}

/* size of icbmui.ocm, the largest module in AIM 3.5 */
#define AIM_MAX_FILE_SIZE 98304

int gaim_memrequest(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct pieceofcrap *pos;
	fu32_t offset, len;
	char *modname;

	va_start(ap, fr);
	offset = va_arg(ap, fu32_t);
	len = va_arg(ap, fu32_t);
	modname = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "offset: %u, len: %u, file: %s\n",
			   offset, len, (modname ? modname : "aim.exe"));

	if (len == 0) {
		gaim_debug_misc("oscar", "len is 0, hashing NULL\n");
		aim_sendmemblock(sess, fr->conn, offset, len, NULL,
				AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		return 1;
	}
	/* uncomment this when you're convinced it's right. remember, it's been wrong before. */
#if 0
	if (offset > AIM_MAX_FILE_SIZE || len > AIM_MAX_FILE_SIZE) {
		char *buf;
		int i = 8;
		if (modname)
			i += strlen(modname);
		buf = g_malloc(i);
		i = 0;
		if (modname) {
			memcpy(buf, modname, strlen(modname));
			i += strlen(modname);
		}
		buf[i++] = offset & 0xff;
		buf[i++] = (offset >> 8) & 0xff;
		buf[i++] = (offset >> 16) & 0xff;
		buf[i++] = (offset >> 24) & 0xff;
		buf[i++] = len & 0xff;
		buf[i++] = (len >> 8) & 0xff;
		buf[i++] = (len >> 16) & 0xff;
		buf[i++] = (len >> 24) & 0xff;
		gaim_debug_misc("oscar", "len + offset is invalid, "
		           "hashing request\n");
		aim_sendmemblock(sess, command->conn, offset, i, buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		g_free(buf);
		return 1;
	}
#endif

	pos = g_new0(struct pieceofcrap, 1);
	pos->gc = sess->aux_data;
	pos->conn = fr->conn;

	pos->offset = offset;
	pos->len = len;
	pos->modname = modname ? g_strdup(modname) : NULL;

	if (gaim_proxy_connect(pos->gc->account, "gaim.sourceforge.net", 80, straight_to_hell, pos) != 0) {
		char buf[256];
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), GAIM_WEBSITE);
		gaim_notify_warning(pos->gc, NULL,
							_("Gaim was unable to get a valid login hash."),
							buf);
	}

	return 1;
}

static int gaim_parse_login(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimAccount *ac = gaim_connection_get_account(gc);
#if 0
	struct client_info_s info = {"gaim", 7, 3, 2003, "us", "en", 0x0004, 0x0000, 0x04b};
#endif
	va_list ap;
	char *key;

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	if (od->icq) {
		struct client_info_s info = CLIENTINFO_ICQ_KNOWNGOOD;
		aim_send_login(sess, fr->conn, gaim_account_get_username(ac),
					   gaim_account_get_password(account), &info, key);
	} else {
		struct client_info_s info = CLIENTINFO_AIM_KNOWNGOOD;
		aim_send_login(sess, fr->conn, gaim_account_get_username(ac),
					   gaim_account_get_password(account), &info, key);
	}

	gaim_connection_update_progress(gc, _("Password sent"), 2, OSCAR_CONNECT_STEPS);
	ck[2] = 0x6c;

	return 1;
}

static int conninitdone_chat(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	struct chat_connection *chatcon;
	static int id = 1;

	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_conv_chat_join, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_conv_chat_leave, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_conv_chat_info_update, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_conv_chat_incoming_msg, 0);

	aim_clientready(sess, fr->conn);

	chatcon = find_oscar_chat_by_conn(gc, fr->conn);
	chatcon->id = id;
	chatcon->conv = serv_got_joined_chat(gc, id++, chatcon->show);

	return 1;
}

static int conninitdone_chatnav(aim_session_t *sess, aim_frame_t *fr, ...) {

	aim_conn_addhandler(sess, fr->conn, 0x000d, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);

	aim_clientready(sess, fr->conn);

	aim_chatnav_reqrights(sess, fr->conn);

	return 1;
}

static int conninitdone_email(aim_session_t *sess, aim_frame_t *fr, ...) {

	aim_conn_addhandler(sess, fr->conn, 0x0018, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_EML, AIM_CB_EML_MAILSTATUS, gaim_email_parseupdate, 0);

	aim_email_sendcookies(sess);
	aim_email_activate(sess);
	aim_clientready(sess, fr->conn);

	return 1;
}

static int conninitdone_icon(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;

	aim_conn_addhandler(sess, fr->conn, 0x0018, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ICO, AIM_CB_ICO_ERROR, gaim_icon_error, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ICO, AIM_CB_ICO_RESPONSE, gaim_icon_parseicon, 0);

	aim_clientready(sess, fr->conn);

	od->iconconnecting = FALSE;

	if (od->icontimer)
		gaim_timeout_remove(od->icontimer);
	od->icontimer = gaim_timeout_add(100, gaim_icon_timerfunc, gc);

	return 1;
}

static void oscar_chatnav_connect(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_CHATNAV);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		gaim_debug_error("oscar",
				   "unable to connect to chatnav server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->cnpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	gaim_debug_info("oscar", "chatnav: connected\n");
}

static void oscar_auth_connect(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		gaim_debug_error("oscar",
				   "unable to connect to authorizer\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->paspa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	gaim_debug_info("oscar", "admin: connected\n");
}

static void oscar_chat_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct chat_connection *ccon = data;
	GaimConnection *gc = ccon->gc;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = ccon->conn;
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	aim_conn_completeconnect(sess, ccon->conn);
	ccon->inpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	od->oscar_chats = g_slist_append(od->oscar_chats, ccon);
}

static void oscar_email_connect(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_EMAIL);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		gaim_debug_error("oscar",
				   "unable to connect to email server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->emlpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	gaim_debug_info("oscar",
			   "email: connected\n");
}

static void oscar_icon_connect(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	OscarData *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_ICON);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		gaim_debug_error("oscar",
				   "unable to connect to icon server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->icopa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	gaim_debug_info("oscar", "icon: connected\n");
}

/* Hrmph. I don't know how to make this look better. --mid */
static int gaim_handle_redirect(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	aim_conn_t *tstconn;
	int i;
	char *host;
	int port;
	va_list ap;
	struct aim_redirect_data *redir;

	port = gaim_account_get_int(account, "port", FAIM_LOGIN_PORT);

	va_start(ap, fr);
	redir = va_arg(ap, struct aim_redirect_data *);
	va_end(ap);

	for (i = 0; i < (int)strlen(redir->ip); i++) {
		if (redir->ip[i] == ':') {
			port = atoi(&(redir->ip[i+1]));
			break;
		}
	}
	host = g_strndup(redir->ip, i);

	switch(redir->group) {
	case 0x7: /* Authorizer */
		gaim_debug_info("oscar",
				   "Reconnecting with authorizor...\n");
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
		if (tstconn == NULL) {
			gaim_debug_error("oscar",
					   "unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_admin, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (gaim_proxy_connect(account, host, port, oscar_auth_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			gaim_debug_error("oscar",
					   "unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	break;

	case 0xd: /* ChatNav */
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, NULL);
		if (tstconn == NULL) {
			gaim_debug_error("oscar",
					   "unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_chatnav, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (gaim_proxy_connect(account, host, port, oscar_chatnav_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			gaim_debug_error("oscar",
					   "unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	break;

	case 0xe: { /* Chat */
		struct chat_connection *ccon;

		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, NULL);
		if (tstconn == NULL) {
			gaim_debug_error("oscar",
					   "unable to connect to chat server\n");
			g_free(host);
			return 1;
		}

		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_chat, 0);

		ccon = g_new0(struct chat_connection, 1);
		ccon->conn = tstconn;
		ccon->gc = gc;
		ccon->fd = -1;
		ccon->name = g_strdup(redir->chat.room);
		ccon->exchange = redir->chat.exchange;
		ccon->instance = redir->chat.instance;
		ccon->show = extract_name(redir->chat.room);

		ccon->conn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (gaim_proxy_connect(account, host, port, oscar_chat_connect, ccon) != 0) {
			aim_conn_kill(sess, &tstconn);
			gaim_debug_error("oscar",
					   "unable to connect to chat server\n");
			g_free(host);
			g_free(ccon->show);
			g_free(ccon->name);
			g_free(ccon);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
		gaim_debug_info("oscar",
				   "Connected to chat room %s exchange %hu\n",
				   ccon->name, ccon->exchange);
	} break;

	case 0x0010: { /* icon */
		if (!(tstconn = aim_newconn(sess, AIM_CONN_TYPE_ICON, NULL))) {
			gaim_debug_error("oscar",
					   "unable to connect to icon server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_icon, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (gaim_proxy_connect(account, host, port, oscar_icon_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			gaim_debug_error("oscar",
					   "unable to connect to icon server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	} break;

	case 0x0018: { /* email */
		if (!(tstconn = aim_newconn(sess, AIM_CONN_TYPE_EMAIL, NULL))) {
			gaim_debug_error("oscar",
					   "unable to connect to email server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_email, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (gaim_proxy_connect(account, host, port, oscar_email_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			gaim_debug_error("oscar",
					   "unable to connect to email server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	} break;

	default: /* huh? */
		gaim_debug_warning("oscar",
				   "got redirect for unknown service 0x%04hx\n",
				   redir->group);
		break;
	}

	g_free(host);
	return 1;
}

static int gaim_parse_oncoming(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc;
	GaimAccount *account;
	OscarData *od;
	struct buddyinfo *bi;
	time_t time_idle = 0, signon = 0;
	int type = 0;
	int caps = 0;
	va_list ap;
	aim_userinfo_t *info;
	gboolean buddy_is_away = FALSE;

	gc = sess->aux_data;
	account = gaim_connection_get_account(gc);
	od = gc->proto_data;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	g_return_val_if_fail(info != NULL, 1);
	g_return_val_if_fail(info->sn != NULL, 1);

	if (info->present & AIM_USERINFO_PRESENT_CAPABILITIES)
		caps = info->capabilities;

	if (info->present & AIM_USERINFO_PRESENT_FLAGS) {
		if (info->flags & AIM_FLAG_AWAY)
		  buddy_is_away = TRUE;
	}

	if (info->present & AIM_USERINFO_PRESENT_ICQEXTSTATUS) {
		type = (info->icqinfo.status << 16);
		if (!(info->icqinfo.status & AIM_ICQ_STATE_CHAT) &&
		      (info->icqinfo.status != AIM_ICQ_STATE_NORMAL)) {
			buddy_is_away = TRUE;
		}
	}

	if (caps & AIM_CAPS_ICQ_DIRECT)
		caps ^= AIM_CAPS_ICQ_DIRECT;

	if (info->present & AIM_USERINFO_PRESENT_IDLE) {
		time(&time_idle);
		time_idle -= info->idletime*60;
		/* time_idle should be the seconds since epoch at which the user became idle */
	}

	if (info->present & AIM_USERINFO_PRESENT_ONLINESINCE)
		signon = info->onlinesince;
	else if (info->present & AIM_USERINFO_PRESENT_SESSIONLEN)
		signon = time(NULL) - info->sessionlen;

	if (!aim_sncmp(gaim_account_get_username(account), info->sn))
		gaim_connection_set_display_name(gc, info->sn);

	bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, info->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(gaim_normalize(account, info->sn)), bi);
	}
	bi->typingnot = FALSE;
	bi->ico_informed = FALSE;
	bi->ipaddr = info->icqinfo.ipaddr;

	/* Available message stuff */
	free(bi->availmsg);
	if (info->avail != NULL)
		bi->availmsg = oscar_encoding_to_utf8(info->avail_encoding, info->avail, info->avail_len);
	else
		bi->availmsg = NULL;

	/* Server stored icon stuff */
	if (info->iconcsumlen) {
		const char *filename = NULL, *saved_b16 = NULL;
		char *b16 = NULL, *filepath = NULL;
		GaimBuddy *b = NULL;

		b16 = gaim_base16_encode(info->iconcsum, info->iconcsumlen);
		b = gaim_find_buddy(account, info->sn);
		/*
		 * If for some reason the checksum is valid, but cached file is not..
		 * we want to know.
		 */
		filename = gaim_blist_node_get_string((GaimBlistNode*)b, "buddy_icon");
		if (filename != NULL) {
			if (g_file_test(filename, G_FILE_TEST_EXISTS))
				saved_b16 = gaim_blist_node_get_string((GaimBlistNode*)b,
						"icon_checksum");
			else {
				filepath = g_build_filename(gaim_buddy_icons_get_cache_dir(),
											filename, NULL);
				if (g_file_test(filepath, G_FILE_TEST_EXISTS))
					saved_b16 = gaim_blist_node_get_string((GaimBlistNode*)b,
															"icon_checksum");
				g_free(filepath);
			}
		} else
			saved_b16 = NULL;

		if (!b16 || !saved_b16 || strcmp(b16, saved_b16)) {
			GSList *cur = od->requesticon;
			while (cur && aim_sncmp((char *)cur->data, info->sn))
				cur = cur->next;
			if (!cur) {
				od->requesticon = g_slist_append(od->requesticon, g_strdup(gaim_normalize(account, info->sn)));
				if (od->icontimer)
					gaim_timeout_remove(od->icontimer);
				od->icontimer = gaim_timeout_add(500, gaim_icon_timerfunc, gc);
			}
		}
		g_free(b16);
	}

	/* XXX - Represent other ICQ statuses */
	if (buddy_is_away == TRUE)
		gaim_prpl_got_user_status(account, info->sn, OSCAR_STATUS_ID_AWAY, NULL);
	else
		gaim_prpl_got_user_status(account, info->sn, OSCAR_STATUS_ID_AVAILABLE, NULL);
	gaim_prpl_got_user_login_time(account, info->sn, signon);
	gaim_prpl_got_user_warning_level(account, info->sn, info->warnlevel/10.0 + 0.5);

	if (time_idle > 0)
		gaim_prpl_got_user_idle(account, info->sn, TRUE, time_idle);
	else
		gaim_prpl_got_user_idle(account, info->sn, FALSE, 0);

	return 1;
}

static void gaim_check_comment(OscarData *od, const char *str) {
	if ((str == NULL) || strcmp(str, ck))
		aim_locate_setcaps(od->sess, caps_aim);
	else
		aim_locate_setcaps(od->sess, caps_aim | AIM_CAPS_SECUREIM);
}

static int gaim_parse_offgoing(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	gaim_prpl_got_user_status(account, info->sn, OSCAR_STATUS_ID_OFFLINE, NULL);

	g_hash_table_remove(od->buddyinfo, gaim_normalize(gc->account, info->sn));

	return 1;
}

/* BBB */
/*
 * This is called after a remote AIM user has connected to us.  We 
 * want to do some voodoo with the socket file descriptors, add a 
 * callback or two, and then send the AIM_CB_OFT_PROMPT.
 */
static int oscar_sendfile_estblsh(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;
	va_list ap;
	aim_conn_t *conn, *listenerconn;

	gaim_debug_info("oscar",
			   "AAA - in oscar_sendfile_estblsh\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	listenerconn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, listenerconn)))
		return 1;

	if (!(oft_info = xfer->data))
		return 1;

	/* Stop watching listener conn; watch transfer conn instead */
	gaim_input_remove(xfer->watcher);
	aim_conn_kill(sess, &listenerconn);

	oft_info->conn = conn;
	xfer->fd = oft_info->conn->fd;

	aim_conn_addhandler(sess, oft_info->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_ACK, oscar_sendfile_ack, 0);
	aim_conn_addhandler(sess, oft_info->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DONE, oscar_sendfile_done, 0);
	xfer->watcher = gaim_input_add(oft_info->conn->fd, GAIM_INPUT_READ, oscar_callback, oft_info->conn);

	/* Inform the other user that we are connected and ready to transfer */
	aim_oft_sendheader(sess, AIM_CB_OFT_PROMPT, oft_info);

	return 0;
}

/*
 * This is the gaim callback passed to gaim_proxy_connect when connecting to another AIM 
 * user in order to transfer a file.
 */
static void oscar_sendfile_connected(gpointer data, gint source, GaimInputCondition condition) {
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;

	gaim_debug_info("oscar",
			   "AAA - in oscar_sendfile_connected\n");
	if (!(xfer = data))
		return;
	if (!(oft_info = xfer->data))
		return;
	if (source < 0) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	xfer->fd = source;
	oft_info->conn->fd = source;

	aim_conn_completeconnect(oft_info->sess, oft_info->conn);
	xfer->watcher = gaim_input_add(xfer->fd, GAIM_INPUT_READ, oscar_callback, oft_info->conn);

	/* Inform the other user that we are connected and ready to transfer */
	aim_im_sendch2_sendfile_accept(oft_info->sess, oft_info);

	return;
}

/*
 * This is called when a buddy sends us some file info.  This happens when they 
 * are sending a file to you, and you have just established a connection to them.
 * You should send them the exact same info except use the real cookie.  We also 
 * get like totally ready to like, receive the file, kay?
 */
static int oscar_sendfile_prompt(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimXfer *xfer;
	struct aim_oft_info *oft_info;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	gaim_debug_info("oscar",
			   "AAA - in oscar_sendfile_prompt\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, conn)))
		return 1;

	if (!(oft_info = xfer->data))
		return 1;

	/* We want to stop listening with a normal thingy */
	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	/* They sent us some information about the file they're sending */
	memcpy(&oft_info->fh, fh, sizeof(*fh));

	/* Fill in the cookie */
	memcpy(&oft_info->fh.bcookie, oft_info->cookie, 8);

	/* XXX - convert the name from UTF-8 to UCS-2 if necessary, and pass the encoding to the call below */
	aim_oft_sendheader(oft_info->sess, AIM_CB_OFT_ACK, oft_info);
	gaim_xfer_start(xfer, xfer->fd, NULL, 0);

	return 0;
}

/*
 * We are sending a file to someone else.  They have just acknowledged our 
 * prompt, so we want to start sending data like there's no tomorrow.
 */
static int oscar_sendfile_ack(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimXfer *xfer;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	gaim_debug_info("oscar", "AAA - in oscar_sendfile_ack\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_cookie(od->file_transfers, cookie)))
		return 1;

	/* We want to stop listening with a normal thingy */
	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	gaim_xfer_start(xfer, xfer->fd, NULL, 0);

	return 0;
}

/*
 * We just sent a file to someone.  They said they got it and everything, 
 * so we can close our direct connection and what not.
 */
static int oscar_sendfile_done(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimXfer *xfer;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	gaim_debug_info("oscar", "AAA - in oscar_sendfile_done\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, conn)))
		return 1;

	xfer->fd = conn->fd;
	gaim_xfer_end(xfer);

	return 0;
}

static int incomingim_chan1(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch1_args *args) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimConvImFlags flags = 0;
	struct buddyinfo *bi;
	const char *iconfile;
	GString *message;
	gchar *tmp;
	aim_mpmsg_section_t *curpart;

	gaim_debug_misc("oscar", "Recived IM from %s with %d parts\n",
					userinfo->sn, args->mpmsg.numparts);

	bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, userinfo->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(gaim_normalize(account, userinfo->sn)), bi);
	}

	if (args->icbmflags & AIM_IMFLAGS_AWAY)
		flags |= GAIM_CONV_IM_AUTO_RESP;

	if (args->icbmflags & AIM_IMFLAGS_TYPINGNOT)
		bi->typingnot = TRUE;
	else
		bi->typingnot = FALSE;

	if ((args->icbmflags & AIM_IMFLAGS_HASICON) && (args->iconlen) && (args->iconsum) && (args->iconstamp)) {
		gaim_debug_misc("oscar",
				   "%s has an icon\n", userinfo->sn);
		if ((args->iconlen != bi->ico_len) || (args->iconsum != bi->ico_csum) || (args->iconstamp != bi->ico_time)) {
			bi->ico_need = TRUE;
			bi->ico_len = args->iconlen;
			bi->ico_csum = args->iconsum;
			bi->ico_time = args->iconstamp;
		}
	}

	if ((iconfile = gaim_account_get_buddy_icon(account)) && 
	    (args->icbmflags & AIM_IMFLAGS_BUDDYREQ) && !bi->ico_sent && bi->ico_informed) {
		FILE *file;
		struct stat st;

		if (!stat(iconfile, &st)) {
			char *buf = g_malloc(st.st_size);
			file = fopen(iconfile, "rb");
			if (file) {
				/* XXX - Use g_file_get_contents() */
				int len = fread(buf, 1, st.st_size, file);
				gaim_debug_info("oscar",
						   "Sending buddy icon to %s (%d bytes, "
						   "%lu reported)\n",
						   userinfo->sn, len, st.st_size);
				aim_im_sendch2_icon(sess, userinfo->sn, buf, st.st_size,
					st.st_mtime, aimutil_iconsum(buf, st.st_size));
				fclose(file);
			} else
				gaim_debug_error("oscar",
						   "Can't open buddy icon file!\n");
			g_free(buf);
		} else
			gaim_debug_error("oscar",
					   "Can't stat buddy icon file!\n");
	}

	message = g_string_new("");
	curpart = args->mpmsg.parts;
	while (curpart != NULL) {
		tmp = gaim_plugin_oscar_decode_im_part(account, userinfo->sn, curpart->charset, curpart->charsubset,
											curpart->data, curpart->datalen);
		if (tmp != NULL) {
			g_string_append(message, tmp);
			g_free(tmp);
		}

		curpart = curpart->next;
	}
	tmp = g_string_free(message, FALSE);

	/*
	 * If the message is being received by an ICQ user then escape any HTML,
	 * because HTML is not sent over ICQ as a means to format a message.
	 * so any HTML we receive is intended to be displayed
	 *
	 * Note: There *may* be some clients which send messages as HTML formatted -
	 *       they need to be special-cased somehow.
	 */
	if (aim_sn_is_icq(gaim_account_get_username(account)) && aim_sn_is_icq(userinfo->sn)) {
		/* being recevied by ICQ from ICQ - escape HTML so it is displayed as sent */
		gchar *tmp2 = gaim_escape_html(tmp);
		g_free(tmp);
		tmp = tmp2;
	}

	serv_got_im(gc, userinfo->sn, tmp, flags, time(NULL));
	g_free(tmp);

	return 1;
}

static int incomingim_chan2(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch2_args *args) {
	GaimConnection *gc;
	GaimAccount *account;
	OscarData *od;
	const char *username = NULL;
	char *message = NULL;

	g_return_val_if_fail(sess != NULL, 0);
	g_return_val_if_fail(sess->aux_data != NULL, 0);

	gc = sess->aux_data;
	account = gaim_connection_get_account(gc);
	od = gc->proto_data;
	username = gaim_account_get_username(account);

	if (args == NULL)
		return 0;

	gaim_debug_misc("oscar", "rendezvous with %s, status is %hu\n",
					userinfo->sn, args->status);

	if (args->msg != NULL)
	{
		if (args->encoding != NULL)
		{
			char *encoding = NULL;
			encoding = oscar_encoding_extract(args->encoding);
			message = oscar_encoding_to_utf8(encoding, args->msg, args->msglen);
			g_free(encoding);
		} else {
			if (g_utf8_validate(args->msg, args->msglen, NULL))
				message = g_strdup(args->msg);
		}
	}

	if (args->reqclass & AIM_CAPS_CHAT) {
		char *name;
		GHashTable *components;

		if (!args->info.chat.roominfo.name || !args->info.chat.roominfo.exchange) {
			g_free(message);
			return 1;
		}
		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				g_free);
		name = extract_name(args->info.chat.roominfo.name);
		g_hash_table_replace(components, g_strdup("room"), g_strdup(name ? name : args->info.chat.roominfo.name));
		g_hash_table_replace(components, g_strdup("exchange"), g_strdup_printf("%d", args->info.chat.roominfo.exchange));
		serv_got_chat_invite(gc,
				     name ? name : args->info.chat.roominfo.name,
				     userinfo->sn,
				     message,
				     components);
		if (name)
			g_free(name);
	} else if (args->reqclass & AIM_CAPS_SENDFILE) {
/* BBB */
		if (args->status == AIM_RENDEZVOUS_PROPOSE) {
			/* Someone wants to send a file (or files) to us */
			GaimXfer *xfer;
			struct aim_oft_info *oft_info;

			if (!args->cookie || !args->port || !args->verifiedip || 
			    !args->info.sendfile.filename || !args->info.sendfile.totsize || 
			    !args->info.sendfile.totfiles || !args->reqclass) {
				gaim_debug_warning("oscar",
						   "%s tried to send you a file with incomplete "
						   "information.\n", userinfo->sn);
				if (args->proxyip)
					gaim_debug_warning("oscar",
							   "IP for a proxy server was given.  Gaim "
							   "does not support this yet.\n");
				g_free(message);
				return 1;
			}

			if (args->info.sendfile.subtype == AIM_OFT_SUBTYPE_SEND_DIR) {
				/* last char of the ft req is a star, they are sending us a
				 * directory -- remove the star and trailing slash so we don't save
				 * directories that look like 'dirname\*'  -- arl */
				char *tmp = strrchr(args->info.sendfile.filename, '\\');
				if (tmp && (tmp[1] == '*')) {
					tmp[0] = '\0';
				}
				gaim_debug_warning("oscar",
						   "We're receiving a whole directory! What fun! "
						   "Especially since we don't support that!\n");
			}

			/* Build the file transfer handle */
			xfer = gaim_xfer_new(gc->account, GAIM_XFER_RECEIVE, userinfo->sn);
			xfer->remote_ip = g_strdup(args->verifiedip);
			xfer->remote_port = args->port;
			gaim_xfer_set_filename(xfer, args->info.sendfile.filename);
			gaim_xfer_set_size(xfer, args->info.sendfile.totsize);
			gaim_xfer_set_message(xfer, message);

			/* Create the oscar-specific data */
			oft_info = aim_oft_createinfo(od->sess, args->cookie, userinfo->sn, args->clientip, xfer->remote_port, 0, 0, NULL);
			if (args->proxyip)
				oft_info->proxyip = g_strdup(args->proxyip);
			if (args->verifiedip)
				oft_info->verifiedip = g_strdup(args->verifiedip);
			xfer->data = oft_info;

			 /* Setup our I/O op functions */
			gaim_xfer_set_init_fnc(xfer, oscar_xfer_init_recv);
			gaim_xfer_set_end_fnc(xfer, oscar_xfer_end);
			gaim_xfer_set_request_denied_fnc(xfer, oscar_xfer_cancel_recv);
			gaim_xfer_set_cancel_recv_fnc(xfer, oscar_xfer_cancel_recv);
			gaim_xfer_set_ack_fnc(xfer, oscar_xfer_ack_recv);

			/* Keep track of this transfer for later */
			od->file_transfers = g_slist_append(od->file_transfers, xfer);

			/* Now perform the request */
			gaim_xfer_request(xfer);
		} else if (args->status == AIM_RENDEZVOUS_CANCEL) {
			/* The other user wants to cancel a file transfer */
			GaimXfer *xfer;
			gaim_debug_info("oscar",
					   "AAA - File transfer canceled by remote user\n");
			if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, args->cookie)))
				gaim_xfer_cancel_remote(xfer);
		} else if (args->status == AIM_RENDEZVOUS_ACCEPT) {
			/*
			 * This gets sent by the receiver of a file 
			 * as they connect directly to us.  If we don't 
			 * get this, then maybe a third party connected 
			 * to us, and we shouldn't send them anything.
			 */
		} else {
			gaim_debug_error("oscar",
					   "unknown rendezvous status!\n");
		}
	} else if (args->reqclass & AIM_CAPS_GETFILE) {
	} else if (args->reqclass & AIM_CAPS_TALK) {
	} else if (args->reqclass & AIM_CAPS_BUDDYICON) {
		gaim_buddy_icons_set_for_user(account, userinfo->sn,
									  args->info.icon.icon,
									  args->info.icon.length);
	} else if (args->reqclass & AIM_CAPS_DIRECTIM) {
		/* Consider moving all this into a helper func in the direct im block way up there */
		struct ask_direct *d = g_new0(struct ask_direct, 1);
		struct oscar_direct_im *dim = oscar_direct_im_find(od, userinfo->sn);
		char buf[256];

		if (!args->verifiedip) {
			/* TODO: do something about this, after figuring out what it means */
			gaim_debug_info("oscar",
					   "directim kill blocked (%s)\n", userinfo->sn);
			g_free(message);
			return 1;
		}

		gaim_debug_info("oscar",
				   "%s received direct im request from %s (%s)\n",
				   username, userinfo->sn, args->clientip);

		d->gc = gc;
		d->sn = g_strdup(userinfo->sn);
		/* Let's use the clientip here, because I think that's what AIM does.
		 * Besides, if the clientip is wrong, we'll probably timeout faster,
		 * and then ask them to connect to us. */
		snprintf(d->ip, sizeof(d->ip), "%s:%d", args->clientip, args->port?args->port:5190);
		memcpy(d->cookie, args->cookie, 8);
		if (dim && !dim->connected && aim_odc_getcookie(dim->conn) && args->cookie &&
		    (!memcmp(aim_odc_getcookie(dim->conn), args->cookie, 8))) {

			oscar_direct_im_destroy(od, dim);
			d->donttryagain = TRUE;
			accept_direct_im_request(d);
		} else {
			if (dim && !dim->connected)
				gaim_debug_warning("oscar", "DirectIM: received direct im request while "
				                   "already connected to that buddy!");
		g_snprintf(buf, sizeof buf, _("%s has just asked to directly connect to %s"), userinfo->sn, username);

		gaim_request_action(gc, NULL, buf,
							_("This requires a direct connection between "
							  "the two computers and is necessary for IM "
							  "Images.  Because your IP address will be "
							  "revealed, this may be considered a privacy "
							  "risk."),
							GAIM_DEFAULT_ACTION_NONE, d, 2,
							_("Connect"), G_CALLBACK(accept_direct_im_request),
							_("Cancel"), G_CALLBACK(destroy_direct_im_request));
							/* FIXME: we should actually send a packet on cancel */
		}
	} else if (args->reqclass & AIM_CAPS_ICQSERVERRELAY) {
		gaim_debug_error("oscar", "Got an ICQ Server Relay message of type %d\n", args->info.rtfmsg.msgtype);
	} else {
		gaim_debug_error("oscar",
				   "Unknown reqclass %hu\n", args->reqclass);
	}

	g_free(message);

	return 1;
}

/*
 * Authorization Functions
 * Most of these are callbacks from dialogs.  They're used by both 
 * methods of authorization (SSI and old-school channel 4 ICBM)
 */
/* When you ask other people for authorization */
static void gaim_auth_request(struct name_data *data, char *msg) {
	GaimConnection *gc = data->gc;

	if (g_list_find(gaim_connections_get_all(), gc)) {
		OscarData *od = gc->proto_data;
		GaimBuddy *buddy = gaim_find_buddy(gc->account, data->name);
		GaimGroup *group = gaim_find_buddys_group(buddy);
		if (buddy && group) {
			gaim_debug_info("oscar",
					   "ssi: adding buddy %s to group %s\n",
					   buddy->name, group->name);
			aim_ssi_sendauthrequest(od->sess, data->name, msg ? msg : _("Please authorize me so I can add you to my buddy list."));
			if (!aim_ssi_itemlist_finditem(od->sess->ssi.local, group->name, buddy->name, AIM_SSI_TYPE_BUDDY))
				aim_ssi_addbuddy(od->sess, buddy->name, group->name, gaim_buddy_get_alias_only(buddy), NULL, NULL, 1);
		}
	}
}

static void gaim_auth_request_msgprompt(struct name_data *data) {
	gaim_request_input(data->gc, NULL, _("Authorization Request Message:"),
					   NULL, _("Please authorize me!"), TRUE, FALSE, NULL,
					   _("OK"), G_CALLBACK(gaim_auth_request),
					   _("Cancel"), G_CALLBACK(oscar_free_name_data),
					   data);
}

static void gaim_auth_dontrequest(struct name_data *data) {
	GaimConnection *gc = data->gc;

	if (g_list_find(gaim_connections_get_all(), gc)) {
		/* Remove from local list */
		GaimBuddy *b = gaim_find_buddy(gaim_connection_get_account(gc), data->name);
		gaim_blist_remove_buddy(b);
	}

	oscar_free_name_data(data);
}


static void gaim_auth_sendrequest(GaimConnection *gc, char *name) {
	struct name_data *data = g_new(struct name_data, 1);
	GaimBuddy *buddy;
	gchar *dialog_msg, *nombre;

	buddy = gaim_find_buddy(gc->account, name);
	if (buddy && (gaim_buddy_get_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", name, gaim_buddy_get_alias_only(buddy));
	else
		nombre = NULL;

	dialog_msg = g_strdup_printf(_("The user %s requires authorization before being added to a buddy list.  Do you want to send an authorization request?"), (nombre ? nombre : name));
	data->gc = gc;
	data->name = g_strdup(name);
	data->nick = NULL;

	gaim_request_action(gc, NULL, _("Request Authorization"), dialog_msg,
						0, data, 2,
						_("Request Authorization"),
						G_CALLBACK(gaim_auth_request_msgprompt),
						_("Cancel"), G_CALLBACK(gaim_auth_dontrequest));

	g_free(dialog_msg);
	g_free(nombre);
}


static void gaim_auth_sendrequest_menu(GaimBlistNode *node, gpointer ignored) {
	GaimBuddy *buddy;
	GaimConnection *gc;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	gaim_auth_sendrequest(gc, buddy->name);
}

/* When other people ask you for authorization */
static void gaim_auth_grant(struct name_data *data) {
	GaimConnection *gc = data->gc;

	if (g_list_find(gaim_connections_get_all(), gc)) {
		OscarData *od = gc->proto_data;
#ifdef NOSSI
		GaimBuddy *buddy;
		gchar message;
		message = 0;
		buddy = gaim_find_buddy(gc->account, data->name);
		aim_im_sendch4(od->sess, data->name, AIM_ICQMSG_AUTHGRANTED, &message);
		gaim_account_notify_added(gc->account, NULL, data->name, (buddy ? gaim_buddy_get_alias_only(buddy) : NULL), NULL);
#else
		aim_ssi_sendauthreply(od->sess, data->name, 0x01, NULL);
#endif
	}

	oscar_free_name_data(data);
}

/* When other people ask you for authorization */
static void gaim_auth_dontgrant(struct name_data *data, char *msg) {
	GaimConnection *gc = data->gc;

	if (g_list_find(gaim_connections_get_all(), gc)) {
		OscarData *od = gc->proto_data;
#ifdef NOSSI
		aim_im_sendch4(od->sess, data->name, AIM_ICQMSG_AUTHDENIED, msg ? msg : _("No reason given."));
#else
		aim_ssi_sendauthreply(od->sess, data->name, 0x00, msg ? msg : _("No reason given."));
#endif
	}
}

static void gaim_auth_dontgrant_msgprompt(struct name_data *data) {
	gaim_request_input(data->gc, NULL, _("Authorization Denied Message:"),
					   NULL, _("No reason given."), TRUE, FALSE, NULL,
					   _("OK"), G_CALLBACK(gaim_auth_dontgrant),
					   _("Cancel"), G_CALLBACK(oscar_free_name_data),
					   data);
}

/* When someone sends you buddies */
static void gaim_icq_buddyadd(struct name_data *data) {
	GaimConnection *gc = data->gc;

	if (g_list_find(gaim_connections_get_all(), gc)) {
		gaim_blist_request_add_buddy(gaim_connection_get_account(gc), data->name, NULL, data->nick);
	}

	oscar_free_name_data(data);
}

static int incomingim_chan4(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch4_args *args, time_t t) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	gchar **msg1, **msg2;
	int i, numtoks;

	if (!args->type || !args->msg || !args->uin)
		return 1;

	gaim_debug_info("oscar",
					"Received a channel 4 message of type 0x%02hhx.\n",
					args->type);

	/*
	 * Split up the message at the delimeter character, then convert each
	 * string to UTF-8.  Unless, of course, this is a type 1 message.  If
	 * this is a type 1 message, then the delimiter 0xfe could be a valid
	 * character in whatever encoding the message was sent in.  Type 1
	 * messages are always made up of only one part, so we can easily account
	 * for this suck-ass part of the protocol by splitting the string into at
	 * most 1 baby string.
	 */
	msg1 = g_strsplit(args->msg, "\376", (args->type == 0x01 ? 1 : 0));
	for (numtoks=0; msg1[numtoks]; numtoks++);
	msg2 = (gchar **)g_malloc((numtoks+1)*sizeof(gchar *));
	for (i=0; msg1[i]; i++) {
		gaim_str_strip_cr(msg1[i]);
		msg2[i] = gaim_plugin_oscar_decode_im_part(account, "1", AIM_CHARSET_ASCII, 0x0000, msg1[i], strlen(msg1[i]));
	}
	msg2[i] = NULL;

	switch (args->type) {
		case 0x01: { /* MacICQ message or basic offline message */
			if (i >= 1) {
				gchar *uin = g_strdup_printf("%u", args->uin);
				gchar *tmp;

				/* If the message came from an ICQ user then escape any HTML */
				tmp = gaim_escape_html(msg2[0]);

				if (t) { /* This is an offline message */
					/* The timestamp is UTC-ish, so we need to get the offset */
#ifdef HAVE_TM_GMTOFF
					time_t now;
					struct tm *tm;
					now = time(NULL);
					tm = localtime(&now);
					t += tm->tm_gmtoff;
#else
#	ifdef HAVE_TIMEZONE
					tzset();
					t -= timezone;
#	endif
#endif
					serv_got_im(gc, uin, tmp, 0, t);
				} else { /* This is a message from MacICQ/Miranda */
					serv_got_im(gc, uin, tmp, 0, time(NULL));
				}
				g_free(uin);
				g_free(tmp);
			}
		} break;

		case 0x04: { /* Someone sent you a URL */
			if (i >= 2) {
				if (msg2[1] != NULL) {
					gchar *uin = g_strdup_printf("%u", args->uin);
					gchar *message = g_strdup_printf("<A HREF=\"%s\">%s</A>",
													 msg2[1],
													 (msg2[0] && msg2[0][0]) ? msg2[0] : msg2[1]);
					serv_got_im(gc, uin, message, 0, time(NULL));
					g_free(uin);
					g_free(message);
				}
			}
		} break;

		case 0x06: { /* Someone requested authorization */
			if (i >= 6) {
				struct name_data *data = g_new(struct name_data, 1);
				gchar *dialog_msg = g_strdup_printf(_("The user %u wants to add you to their buddy list for the following reason:\n%s"), args->uin, msg2[5] ? msg2[5] : _("No reason given."));
				gaim_debug_info("oscar",
						   "Received an authorization request from UIN %u\n",
						   args->uin);
				data->gc = gc;
				data->name = g_strdup_printf("%u", args->uin);
				data->nick = NULL;

				gaim_request_action(gc, NULL, _("Authorization Request"),
									dialog_msg, GAIM_DEFAULT_ACTION_NONE, data,
									2, _("Authorize"),
									G_CALLBACK(gaim_auth_grant),
									_("Deny"),
									G_CALLBACK(gaim_auth_dontgrant_msgprompt));
				g_free(dialog_msg);
			}
		} break;

		case 0x07: { /* Someone has denied you authorization */
			if (i >= 1) {
				gchar *dialog_msg = g_strdup_printf(_("The user %u has denied your request to add them to your buddy list for the following reason:\n%s"), args->uin, msg2[0] ? msg2[0] : _("No reason given."));
				gaim_notify_info(gc, NULL, _("ICQ authorization denied."),
								 dialog_msg);
				g_free(dialog_msg);
			}
		} break;

		case 0x08: { /* Someone has granted you authorization */
			gchar *dialog_msg = g_strdup_printf(_("The user %u has granted your request to add them to your buddy list."), args->uin);
			gaim_notify_info(gc, NULL, "ICQ authorization accepted.",
							 dialog_msg);
			g_free(dialog_msg);
		} break;

		case 0x09: { /* Message from the Godly ICQ server itself, I think */
			if (i >= 5) {
				gchar *dialog_msg = g_strdup_printf(_("You have received a special message\n\nFrom: %s [%s]\n%s"), msg2[0], msg2[3], msg2[5]);
				gaim_notify_info(gc, NULL, "ICQ Server Message", dialog_msg);
				g_free(dialog_msg);
			}
		} break;

		case 0x0d: { /* Someone has sent you a pager message from http://www.icq.com/your_uin */
			if (i >= 6) {
				gchar *dialog_msg = g_strdup_printf(_("You have received an ICQ page\n\nFrom: %s [%s]\n%s"), msg2[0], msg2[3], msg2[5]);
				gaim_notify_info(gc, NULL, "ICQ Page", dialog_msg);
				g_free(dialog_msg);
			}
		} break;

		case 0x0e: { /* Someone has emailed you at your_uin@pager.icq.com */
			if (i >= 6) {
				gchar *dialog_msg = g_strdup_printf(_("You have received an ICQ email from %s [%s]\n\nMessage is:\n%s"), msg2[0], msg2[3], msg2[5]);
				gaim_notify_info(gc, NULL, "ICQ Email", dialog_msg);
				g_free(dialog_msg);
			}
		} break;

		case 0x12: {
			/* Ack for authorizing/denying someone.  Or possibly an ack for sending any system notice */
			/* Someone added you to their buddy list? */
		} break;

		case 0x13: { /* Someone has sent you some ICQ buddies */
			guint i, num;
			gchar **text;
			text = g_strsplit(args->msg, "\376", 0);
			if (text) {
				num = 0;
				for (i=0; i<strlen(text[0]); i++)
					num = num*10 + text[0][i]-48;
				for (i=0; i<num; i++) {
					struct name_data *data = g_new(struct name_data, 1);
					gchar *message = g_strdup_printf(_("ICQ user %u has sent you a buddy: %s (%s)"), args->uin, text[i*2+2], text[i*2+1]);
					data->gc = gc;
					data->name = g_strdup(text[i*2+1]);
					data->nick = g_strdup(text[i*2+2]);

					gaim_request_action(gc, NULL, message,
										_("Do you want to add this buddy "
										  "to your buddy list?"),
										GAIM_DEFAULT_ACTION_NONE, data, 2,
										_("Add"), G_CALLBACK(gaim_icq_buddyadd),
										_("Decline"), G_CALLBACK(oscar_free_name_data));
					g_free(message);
				}
				g_strfreev(text);
			}
		} break;

		case 0x1a: { /* Someone has sent you a greeting card or requested buddies? */
			/* This is boring and silly. */
		} break;

		default: {
			gaim_debug_info("oscar",
					   "Received a channel 4 message of unknown type "
					   "(type 0x%02hhx).\n", args->type);
		} break;
	}

	g_strfreev(msg1);
	g_strfreev(msg2);

	return 1;
}

static int gaim_parse_incoming_im(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t channel;
	int ret = 0;
	aim_userinfo_t *userinfo;
	va_list ap;

	va_start(ap, fr);
	channel = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	switch (channel) {
		case 1: { /* standard message */
			struct aim_incomingim_ch1_args *args;
			args = va_arg(ap, struct aim_incomingim_ch1_args *);
			ret = incomingim_chan1(sess, fr->conn, userinfo, args);
		} break;

		case 2: { /* rendezvous */
			struct aim_incomingim_ch2_args *args;
			args = va_arg(ap, struct aim_incomingim_ch2_args *);
			ret = incomingim_chan2(sess, fr->conn, userinfo, args);
		} break;

		case 4: { /* ICQ */
			struct aim_incomingim_ch4_args *args;
			args = va_arg(ap, struct aim_incomingim_ch4_args *);
			ret = incomingim_chan4(sess, fr->conn, userinfo, args, 0);
		} break;

		default: {
			gaim_debug_warning("oscar",
					   "ICBM received on unsupported channel (channel "
					   "0x%04hx).", channel);
		} break;
	}

	va_end(ap);

	return ret;
}

static int gaim_parse_misses(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	char *buf;
	va_list ap;
	fu16_t chan, nummissed, reason;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	chan = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	nummissed = (fu16_t)va_arg(ap, unsigned int);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	switch(reason) {
		case 0: /* Invalid (0) */
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s because it was invalid.",
				   "You missed %hu messages from %s because they were invalid.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 1: /* Message too large */
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s because it was too large.",
				   "You missed %hu messages from %s because they were too large.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 2: /* Rate exceeded */
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s because the rate limit has been exceeded.",
				   "You missed %hu messages from %s because the rate limit has been exceeded.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 3: /* Evil Sender */
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s because he/she was too evil.",
				   "You missed %hu messages from %s because he/she was too evil.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 4: /* Evil Receiver */
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s because you are too evil.",
				   "You missed %hu messages from %s because you are too evil.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		default:
			buf = g_strdup_printf(
				   ngettext(
				   "You missed %hu message from %s for an unknown reason.",
				   "You missed %hu messages from %s for an unknown reason.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
	}

	if (!gaim_conv_present_error(userinfo->sn, account, buf))
		gaim_notify_error(sess->aux_data, NULL, buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_parse_clientauto_ch2(aim_session_t *sess, const char *who, fu16_t reason, const char *cookie) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;

/* BBB */
	switch (reason) {
		case 3: { /* Decline sendfile. */
			GaimXfer *xfer;
			struct oscar_direct_im *dim;

			gaim_debug_info("oscar",
					   "AAA - Other user declined some sort of direct "
					   "connect attempt (automaticly?)\n");
			if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, cookie)))
				gaim_xfer_cancel_remote(xfer);
			else if ((dim = oscar_direct_im_find(od, who))) {
				/* AAA should use find by cookie or something here */
				oscar_direct_im_disconnect(od, dim);
			}
		} break;

		default: {
			gaim_debug_warning("oscar",
					   "Received an unknown rendezvous client auto-response "
					   "from %s.  Type 0x%04hx\n", who, reason);
		}

	}

	return 0;
}

static int gaim_parse_clientauto_ch4(aim_session_t *sess, char *who, fu16_t reason, fu32_t state, char *msg) {
	GaimConnection *gc = sess->aux_data;

	switch(reason) {
		case 0x0003: { /* Reply from an ICQ status message request */
			char *title, *statusmsg, **splitmsg, *dialogmsg;

			title = g_strdup_printf(_("Info for %s"), who);

			/* Split at (carriage return/newline)'s, then rejoin later with BRs between. */
			statusmsg = oscar_icqstatus(state);
			splitmsg = g_strsplit(msg, "\r\n", 0);
			dialogmsg = g_strdup_printf(_("<B>UIN:</B> %s<BR><B>Status:</B> %s<HR>%s"), who, statusmsg, g_strjoinv("<BR>", splitmsg));
			g_free(statusmsg);
			g_strfreev(splitmsg);

			gaim_notify_userinfo(gc, who, title, _("Buddy Information"), NULL, dialogmsg, NULL, NULL);

			g_free(title);
			g_free(dialogmsg);
		} break;

		default: {
			gaim_debug_warning("oscar",
					   "Received an unknown client auto-response from %s.  "
					   "Type 0x%04hx\n", who, reason);
		} break;
	} /* end of switch */

	return 0;
}

static int gaim_parse_clientauto(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t chan, reason;
	char *who;

	va_start(ap, fr);
	chan = (fu16_t)va_arg(ap, unsigned int);
	who = va_arg(ap, char *);
	reason = (fu16_t)va_arg(ap, unsigned int);

	if (chan == 0x0002) { /* File transfer declined */
		char *cookie = va_arg(ap, char *);
		return gaim_parse_clientauto_ch2(sess, who, reason, cookie);
	} else if (chan == 0x0004) { /* ICQ message */
		fu32_t state = 0;
		char *msg = NULL;
		if (reason == 0x0003) {
			state = va_arg(ap, fu32_t);
			msg = va_arg(ap, char *);
		}
		return gaim_parse_clientauto_ch4(sess, who, reason, state, msg);
	}

	va_end(ap);

	return 1;
}

static int gaim_parse_genericerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t reason;
	char *m;

	va_start(ap, fr);
	reason = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_error("oscar",
			   "snac threw error (reason 0x%04hx: %s)\n", reason,
			   (reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown");

	m = g_strdup_printf(_("SNAC threw error: %s\n"),
			reason < msgerrreasonlen ? _(msgerrreason[reason]) : _("Unknown error"));
	gaim_notify_error(sess->aux_data, NULL, m, NULL);
	g_free(m);

	return 1;
}

static int gaim_parse_msgerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
#if 0
	OscarData *od = gc->proto_data;
	GaimXfer *xfer;
#endif
	va_list ap;
	fu16_t reason;
	char *data, *buf;

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	data = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_error("oscar",
			   "Message error with data %s and reason %hu\n",
				(data != NULL ? data : ""), reason);

/* BBB */
#if 0
	/* If this was a file transfer request, data is a cookie */
	if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, data))) {
		gaim_xfer_cancel_remote(xfer);
		return 1;
	}
#endif

	/* Data is assumed to be the destination sn */
	buf = g_strdup_printf(_("Unable to send message: %s"), (reason < msgerrreasonlen) ? msgerrreason[reason] : _("Unknown reason."));
	if (!gaim_conv_present_error(data, gaim_connection_get_account(gc), buf)) {
		g_free(buf);
		buf = g_strdup_printf(_("Unable to send message to %s:"), data ? data : "(unknown)");
		gaim_notify_error(sess->aux_data, NULL, buf,
				  (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	}
	g_free(buf);

	return 1;
}

static int gaim_parse_mtn(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	fu16_t type1, type2;
	char *sn;

	va_start(ap, fr);
	type1 = (fu16_t) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	type2 = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	switch (type2) {
		case 0x0000: { /* Text has been cleared */
			serv_got_typing_stopped(gc, sn);
		} break;

		case 0x0001: { /* Paused typing */
			serv_got_typing(gc, sn, 0, GAIM_TYPED);
		} break;

		case 0x0002: { /* Typing */
			serv_got_typing(gc, sn, 0, GAIM_TYPING);
		} break;

		default: {
			gaim_debug_error("oscar", "Received unknown typing notification message from %s.  Type1 is 0x%04x and type2 is 0x%04hx.\n", sn, type1, type2);
		} break;
	}

	return 1;
}

/*
 * We get this error when there was an error in the locate family.  This 
 * happens when you request info of someone who is offline.
 */
static int gaim_parse_locerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	gchar *buf;
	va_list ap;
	fu16_t reason;
	char *destn;

	va_start(ap, fr);
	reason = (fu16_t) va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	if (destn == NULL)
		return 1;
	
	buf = g_strdup_printf(_("User information not available: %s"), (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	if (!gaim_conv_present_error(destn, gaim_connection_get_account((GaimConnection*)sess->aux_data), buf)) {
		g_free(buf);
		buf = g_strdup_printf(_("User information for %s unavailable:"), destn);
		gaim_notify_error(sess->aux_data, NULL, buf, (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	}
	g_free(buf);

	return 1;
}

static int gaim_parse_userinfo(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GString *str;
	gchar *tmp = NULL, *info_utf8 = NULL, *away_utf8 = NULL, *title = NULL;
	va_list ap;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	str = g_string_new("");
	g_string_append_printf(str, "<b>%s:</b> %s", _("Screen Name"), userinfo->sn);
	g_string_append_printf(str, "\n<br><b>%s</b>: %d%%", _("Warning Level"), (int)((userinfo->warnlevel/10.0) + 0.5));

	if (userinfo->present & AIM_USERINFO_PRESENT_ONLINESINCE) {
		time_t t = userinfo->onlinesince;
		oscar_string_append(str, "\n<br>", _("Online Since"), ctime(&t));
	}

	if (userinfo->present & AIM_USERINFO_PRESENT_MEMBERSINCE) {
		time_t t = userinfo->membersince;
		oscar_string_append(str, "\n<br>", _("Member Since"), ctime(&t));
	}

	if (userinfo->present & AIM_USERINFO_PRESENT_IDLE) {
		tmp = gaim_str_seconds_to_string(userinfo->idletime*60);
		oscar_string_append(str, "\n<br>", _("Idle"), tmp);
		g_free(tmp);
	}

	oscar_string_append_info(gc, str, "\n<br>", NULL, userinfo);

	if ((userinfo->flags & AIM_FLAG_AWAY) && (userinfo->away_len > 0) && (userinfo->away != NULL) && (userinfo->away_encoding != NULL)) {
		tmp = oscar_encoding_extract(userinfo->away_encoding);
		away_utf8 = oscar_encoding_to_utf8(tmp, userinfo->away, userinfo->away_len);
		g_free(tmp);
		if (away_utf8 != NULL) {
			g_string_append_printf(str, "\n<hr>%s", away_utf8);
			g_free(away_utf8);
		}
	}

	if ((userinfo->info_len > 0) && (userinfo->info != NULL) && (userinfo->info_encoding != NULL)) {
		tmp = oscar_encoding_extract(userinfo->info_encoding);
		info_utf8 = oscar_encoding_to_utf8(tmp, userinfo->info, userinfo->info_len);
		g_free(tmp);
		if (info_utf8 != NULL) {
			g_string_append_printf(str, "\n<hr>%s", info_utf8);
			g_free(info_utf8);
		}
	}

	tmp = gaim_str_sub_away_formatters(str->str, gaim_account_get_username(account));
	g_string_free(str, TRUE);
	title = g_strdup_printf(_("Info for %s"), userinfo->sn);
	gaim_notify_userinfo(gc, userinfo->sn, title, _("Buddy Information"), NULL, tmp, NULL, NULL);
	g_free(title);
	g_free(tmp);

	return 1;
}

static gboolean gaim_reqinfo_timeout_cb(void *data)
{
	aim_session_t *sess = data;
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;

	aim_locate_dorequest(data);
	od->getinfotimer = 0;

	return FALSE;
}

static int gaim_reqinfo_timeout(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;

	/*
	 * Wait a little while then call aim_locate_dorequest(sess).  This keeps
	 * us from hitting the rate limit due to request away messages and info
	 * too quickly.
	 */
	if (od->getinfotimer == 0)
		od->getinfotimer = gaim_timeout_add(1500, gaim_reqinfo_timeout_cb, sess);

	return 1;
}

static int gaim_parse_motd(aim_session_t *sess, aim_frame_t *fr, ...)
{
	char *msg;
	fu16_t id;
	va_list ap;

	va_start(ap, fr);
	id  = (fu16_t) va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "MOTD: %s (%hu)\n", msg ? msg : "Unknown", id);
	if (id < 4)
		gaim_notify_warning(sess->aux_data, NULL,
							_("Your AIM connection may be lost."), NULL);

	return 1;
}

static int gaim_chatnav_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;

	va_start(ap, fr);
	type = (fu16_t) va_arg(ap, unsigned int);

	switch(type) {
		case 0x0002: {
			fu8_t maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i;

			maxrooms = (fu8_t) va_arg(ap, unsigned int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);

			gaim_debug_misc("oscar",
					   "chat info: Chat Rights:\n");
			gaim_debug_misc("oscar",
					   "chat info: \tMax Concurrent Rooms: %hhd\n", maxrooms);
			gaim_debug_misc("oscar",
					   "chat info: \tExchange List: (%d total)\n", exchangecount);
			for (i = 0; i < exchangecount; i++)
				gaim_debug_misc("oscar",
						   "chat info: \t\t%hu    %s\n",
						   exchanges[i].number, exchanges[i].name ? exchanges[i].name : "");
			while (od->create_rooms) {
				struct create_room *cr = od->create_rooms->data;
				gaim_debug_info("oscar",
						   "creating room %s\n", cr->name);
				aim_chatnav_createroom(sess, fr->conn, cr->name, cr->exchange);
				g_free(cr->name);
				od->create_rooms = g_slist_remove(od->create_rooms, cr);
				g_free(cr);
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			fu16_t instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
			fu8_t createperms;
			fu32_t createtime;

			fqcn = va_arg(ap, char *);
			instance = (fu16_t)va_arg(ap, unsigned int);
			exchange = (fu16_t)va_arg(ap, unsigned int);
			flags = (fu16_t)va_arg(ap, unsigned int);
			createtime = va_arg(ap, fu32_t);
			maxmsglen = (fu16_t)va_arg(ap, unsigned int);
			maxoccupancy = (fu16_t)va_arg(ap, unsigned int);
			createperms = (fu8_t)va_arg(ap, unsigned int);
			unknown = (fu16_t)va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);

			gaim_debug_misc("oscar",
					   "created room: %s %hu %hu %hu %u %hu %hu %hhu %hu %s %s\n",
					fqcn,
					exchange, instance, flags,
					createtime,
					maxmsglen, maxoccupancy, createperms, unknown,
					name, ck);
			aim_chat_join(od->sess, od->conn, exchange, ck, instance);
			}
			break;
		default:
			gaim_debug_warning("oscar",
					   "chatnav info: unknown type (%04hx)\n", type);
			break;
	}

	va_end(ap);

	return 1;
}

static int gaim_conv_chat_join(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	GaimConnection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(c->conv), info[i].sn, NULL, GAIM_CBFLAGS_NONE, TRUE);

	return 1;
}

static int gaim_conv_chat_leave(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	GaimConnection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(c->conv), info[i].sn, NULL);

	return 1;
}

static int gaim_conv_chat_info_update(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	aim_userinfo_t *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	fu16_t unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	fu32_t creationtime;
	GaimConnection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount= va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (fu16_t)va_arg(ap, unsigned int);
	creationtime = va_arg(ap, fu32_t);
	maxmsglen = (fu16_t)va_arg(ap, unsigned int);
	unknown_d2 = (fu16_t)va_arg(ap, unsigned int);
	unknown_d5 = (fu16_t)va_arg(ap, unsigned int);
	maxvisiblemsglen = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "inside chat_info_update (maxmsglen = %hu, maxvislen = %hu)\n",
			   maxmsglen, maxvisiblemsglen);

	ccon->maxlen = maxmsglen;
	ccon->maxvis = maxvisiblemsglen;

	return 1;
}

static int gaim_conv_chat_incoming_msg(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);
	gchar *utf8;
	va_list ap;
	aim_userinfo_t *info;
	int len;
	char *msg;
	char *charset;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	len = va_arg(ap, int);
	msg = va_arg(ap, char *);
	charset = va_arg(ap, char *);
	va_end(ap);

	utf8 = oscar_encoding_to_utf8(charset, msg, len);
	if (utf8 == NULL)
		/* The conversion failed! */
		utf8 = g_strdup(_("[Unable to display a message from this user because it contained invalid characters.]"));
	serv_got_chat_in(gc, ccon->id, info->sn, 0, utf8, time((time_t)NULL));
	g_free(utf8);

	return 1;
}

static int gaim_email_parseupdate(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	GaimConnection *gc = sess->aux_data;
	struct aim_emailinfo *emailinfo;
	int havenewmail;
	char *alertitle, *alerturl;

	va_start(ap, fr);
	emailinfo = va_arg(ap, struct aim_emailinfo *);
	havenewmail = va_arg(ap, int);
	alertitle = va_arg(ap, char *);
	alerturl  = va_arg(ap, char *);
	va_end(ap);

	if ((emailinfo != NULL) && gaim_account_get_check_mail(gc->account)) {
		gchar *to = g_strdup_printf("%s%s%s", gaim_account_get_username(gaim_connection_get_account(gc)),
									emailinfo->domain ? "@" : "",
									emailinfo->domain ? emailinfo->domain : "");
		if (emailinfo->unread && havenewmail)
			gaim_notify_emails(gc, emailinfo->nummsgs, FALSE, NULL, NULL, (const char **)&to, (const char **)&emailinfo->url, NULL, NULL);
		g_free(to);
	}

	if (alertitle)
		gaim_debug_misc("oscar", "Got an alert '%s' %s\n", alertitle, alerturl ? alerturl : "");

	return 1;
}

static int gaim_icon_error(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	char *sn;

	sn = od->requesticon->data;
	gaim_debug_misc("oscar",
			   "removing %s from hash table\n", sn);
	od->requesticon = g_slist_remove(od->requesticon, sn);
	free(sn);

	if (od->icontimer)
		gaim_timeout_remove(od->icontimer);
	od->icontimer = gaim_timeout_add(500, gaim_icon_timerfunc, gc);

	return 1;
}

static int gaim_icon_parseicon(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	GSList *cur;
	va_list ap;
	char *sn;
	fu8_t *iconcsum, *icon;
	fu16_t iconcsumlen, iconlen;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	iconcsum = va_arg(ap, fu8_t *);
	iconcsumlen = va_arg(ap, int);
	icon = va_arg(ap, fu8_t *);
	iconlen = va_arg(ap, int);
	va_end(ap);

	if (iconlen > 0) {
		char *b16;
		GaimBuddy *b = gaim_find_buddy(gc->account, sn);
		gaim_buddy_icons_set_for_user(gaim_connection_get_account(gc),
									  sn, icon, iconlen);
		b16 = gaim_base16_encode(iconcsum, iconcsumlen);
		if (b16) {
			gaim_blist_node_set_string((GaimBlistNode*)b, "icon_checksum", b16);
			g_free(b16);
		}
	}

	cur = od->requesticon;
	while (cur) {
		char *cursn = cur->data;
		if (!aim_sncmp(cursn, sn)) {
			od->requesticon = g_slist_remove(od->requesticon, cursn);
			free(cursn);
			cur = od->requesticon;
		} else
			cur = cur->next;
	}

	if (od->icontimer)
		gaim_timeout_remove(od->icontimer);
	od->icontimer = gaim_timeout_add(250, gaim_icon_timerfunc, gc);

	return 1;
}

static gboolean gaim_icon_timerfunc(gpointer data) {
	GaimConnection *gc = data;
	OscarData *od = gc->proto_data;
	aim_userinfo_t *userinfo;
	aim_conn_t *conn;

	conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_ICON);
	if (!conn) {
		if (!od->iconconnecting) {
			aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_ICON);
			od->iconconnecting = TRUE;
		}
		return FALSE;
	}

	if (od->set_icon) {
		struct stat st;
		const char *iconfile = gaim_account_get_buddy_icon(gaim_connection_get_account(gc));
		if (iconfile == NULL) {
			aim_ssi_delicon(od->sess);
		} else if (!stat(iconfile, &st)) {
			char *buf = g_malloc(st.st_size);
			FILE *file = fopen(iconfile, "rb");
			if (file) {
				/* XXX - Use g_file_get_contents()? */
				fread(buf, 1, st.st_size, file);
				fclose(file);
				gaim_debug_info("oscar",
					   "Uploading icon to icon server\n");
				aim_bart_upload(od->sess, buf, st.st_size);
			} else
				gaim_debug_error("oscar",
					   "Can't open buddy icon file!\n");
			g_free(buf);
		} else {
			gaim_debug_error("oscar",
				   "Can't stat buddy icon file!\n");
		}
		od->set_icon = FALSE;
	}

	if (!od->requesticon) {
		gaim_debug_misc("oscar",
				   "no more icons to request\n");
		return FALSE;
	}

	userinfo = aim_locate_finduserinfo(od->sess, (char *)od->requesticon->data);
	if ((userinfo != NULL) && (userinfo->iconcsumlen > 0)) {
		aim_bart_request(od->sess, od->requesticon->data, userinfo->iconcsum, userinfo->iconcsumlen);
		return FALSE;
	} else {
		char *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		free(sn);
	}

	return TRUE;
}

/*
 * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
 */
static int gaim_parse_msgack(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	char *sn;

	va_start(ap, fr);
	type = (fu16_t) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar", "Sent message to %s.\n", sn);

	return 1;
}

static int gaim_parse_ratechange(aim_session_t *sess, aim_frame_t *fr, ...) {
	static const char *codes[5] = {
		"invalid",
		"change",
		"warning",
		"limit",
		"limit cleared",
	};
	va_list ap;
	fu16_t code, rateclass;
	fu32_t windowsize, clear, alert, limit, disconnect, currentavg, maxavg;

	va_start(ap, fr); 
	code = (fu16_t)va_arg(ap, unsigned int);
	rateclass= (fu16_t)va_arg(ap, unsigned int);
	windowsize = va_arg(ap, fu32_t);
	clear = va_arg(ap, fu32_t);
	alert = va_arg(ap, fu32_t);
	limit = va_arg(ap, fu32_t);
	disconnect = va_arg(ap, fu32_t);
	currentavg = va_arg(ap, fu32_t);
	maxavg = va_arg(ap, fu32_t);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "rate %s (param ID 0x%04hx): curavg = %u, maxavg = %u, alert at %u, "
		     "clear warning at %u, limit at %u, disconnect at %u (window size = %u)\n",
		     (code < 5) ? codes[code] : codes[0],
		     rateclass,
		     currentavg, maxavg,
		     alert, clear,
		     limit, disconnect,
		     windowsize);

	/* XXX fix these values */
	if (code == AIM_RATE_CODE_CHANGE) {
		if (currentavg >= clear)
			aim_conn_setlatency(fr->conn, 0);
	} else if (code == AIM_RATE_CODE_WARNING) {
		aim_conn_setlatency(fr->conn, windowsize/4);
	} else if (code == AIM_RATE_CODE_LIMIT) {
		gaim_notify_error(sess->aux_data, NULL, _("Rate limiting error."),
						  _("The last action you attempted could not be "
							"performed because you are over the rate limit. "
							"Please wait 10 seconds and try again."));
		aim_conn_setlatency(fr->conn, windowsize/2);
	} else if (code == AIM_RATE_CODE_CLEARLIMIT) {
		aim_conn_setlatency(fr->conn, 0);
	}

	return 1;
}

static int gaim_parse_evilnotify(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	va_list ap;
	fu16_t newevil;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	newevil = (fu16_t) va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	/* XXX - What's with the + 0.5? */
	gaim_prpl_got_account_warning_level(account, (userinfo && userinfo->sn) ? userinfo->sn : NULL, (newevil/10.0) + 0.5);

	return 1;
}

static int gaim_selfinfo(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimPresence *presence = gaim_account_get_presence(account);
	int warning_level;
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	warning_level = info->warnlevel/10.0 + 0.5;

	gaim_presence_set_warning_level(presence, warning_level);

	return 1;
}

static int gaim_connerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	va_list ap;
	fu16_t code;
	char *msg;

	va_start(ap, fr);
	code = (fu16_t)va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "Disconnected.  Code is 0x%04x and msg is %s\n", code,
				(msg != NULL ? msg : ""));

	if ((fr) && (fr->conn) && (fr->conn->type == AIM_CONN_TYPE_BOS)) {
		if (code == 0x0001) {
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("You have been disconnected because you have signed on with this screen name at another location."));
		} else {
			gaim_connection_error(gc, _("You have been signed off for an unknown reason."));
		}
		od->killme = TRUE;
	}

	return 1;
}

static int conninitdone_bos(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;

	aim_reqpersonalinfo(sess, fr->conn);

#ifndef NOSSI
	gaim_debug_info("oscar", "ssi: requesting rights and list\n");
	aim_ssi_reqrights(sess);
	aim_ssi_reqdata(sess);
#endif

	aim_locate_reqrights(sess);
	aim_buddylist_reqrights(sess, fr->conn);
	aim_im_reqparams(sess);
	aim_bos_reqrights(sess, fr->conn); /* XXX - Don't call this with ssi */

#ifdef NOSSI
	gaim_debug_info("oscar", "bos: requesting rights\n");
	aim_bos_reqrights(sess, fr->conn);
	aim_bos_setgroupperm(sess, fr->conn, AIM_FLAG_ALLUSERS);
	aim_bos_setprivacyflags(sess, fr->conn, AIM_PRIVFLAGS_ALLOWIDLE | AIM_PRIVFLAGS_ALLOWMEMBERSINCE);
#endif

	gaim_connection_update_progress(gc, _("Finalizing connection"), 5, OSCAR_CONNECT_STEPS);

	return 1;
}

static int conninitdone_admin(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;

	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ADM, 0x0003, gaim_info_change, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ADM, 0x0005, gaim_info_change, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ADM, 0x0007, gaim_account_confirm, 0);

	aim_clientready(sess, fr->conn);
	gaim_debug_info("oscar", "connected to admin\n");

	if (od->chpass) {
		gaim_debug_info("oscar", "changing password\n");
		aim_admin_changepasswd(sess, fr->conn, od->newp, od->oldp);
		g_free(od->oldp);
		od->oldp = NULL;
		g_free(od->newp);
		od->newp = NULL;
		od->chpass = FALSE;
	}
	if (od->setnick) {
		gaim_debug_info("oscar", "formatting screen name\n");
		aim_admin_setnick(sess, fr->conn, od->newsn);
		g_free(od->newsn);
		od->newsn = NULL;
		od->setnick = FALSE;
	}
	if (od->conf) {
		gaim_debug_info("oscar", "confirming account\n");
		aim_admin_reqconfirm(sess, fr->conn);
		od->conf = FALSE;
	}
	if (od->reqemail) {
		gaim_debug_info("oscar", "requesting email\n");
		aim_admin_getinfo(sess, fr->conn, 0x0011);
		od->reqemail = FALSE;
	}
	if (od->setemail) {
		gaim_debug_info("oscar", "setting email\n");
		aim_admin_setemail(sess, fr->conn, od->email);
		g_free(od->email);
		od->email = NULL;
		od->setemail = FALSE;
	}

	return 1;
}

static int gaim_icbm_param_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_icbmparameters *params;
	va_list ap;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	/* XXX - evidently this crashes on solaris. i have no clue why
	gaim_debug_misc("oscar", "ICBM Parameters: maxchannel = %hu, default flags = 0x%08lx, max msg len = %hu, "
			"max sender evil = %f, max receiver evil = %f, min msg interval = %u\n",
			params->maxchan, params->flags, params->maxmsglen,
			((float)params->maxsenderwarn)/10.0, ((float)params->maxrecverwarn)/10.0,
			params->minmsginterval);
	*/

	/* Maybe senderwarn and recverwarn should be user preferences... */
	params->flags = 0x0000000b;
	params->maxmsglen = 8000;
	params->minmsginterval = 0;

	aim_im_setparams(sess, params);

	return 1;
}

static int gaim_parse_locaterights(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	va_list ap;
	fu16_t maxsiglen;

	va_start(ap, fr);
	maxsiglen = (fu16_t) va_arg(ap, int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "locate rights: max sig len = %d\n", maxsiglen);

	od->rights.maxsiglen = od->rights.maxawaymsglen = (guint)maxsiglen;

	if (od->icq)
		aim_locate_setcaps(od->sess, caps_icq);
	else
		aim_locate_setcaps(od->sess, caps_aim);
	oscar_set_info(gc, gc->account->user_info);

	return 1;
}

static int gaim_parse_buddyrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t maxbuddies, maxwatchers;
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;

	va_start(ap, fr);
	maxbuddies = (fu16_t) va_arg(ap, unsigned int);
	maxwatchers = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "buddy list rights: Max buddies = %hu / Max watchers = %hu\n", maxbuddies, maxwatchers);

	od->rights.maxbuddies = (guint)maxbuddies;
	od->rights.maxwatchers = (guint)maxwatchers;

	return 1;
}

static int gaim_bosrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	va_list ap;
	fu16_t maxpermits, maxdenies;

	va_start(ap, fr);
	maxpermits = (fu16_t) va_arg(ap, unsigned int);
	maxdenies = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "BOS rights: Max permit = %hu / Max deny = %hu\n", maxpermits, maxdenies);

	od->rights.maxpermits = (guint)maxpermits;
	od->rights.maxdenies = (guint)maxdenies;

	gaim_connection_set_state(gc, GAIM_CONNECTED);
	serv_finish_login(gc);

	gaim_debug_info("oscar", "buddy list loaded\n");

	aim_clientready(sess, fr->conn);
	aim_srv_setavailmsg(sess, NULL);
	aim_srv_setidle(sess, 0);

	if (od->icq) {
		aim_icq_reqofflinemsgs(sess);
		aim_icq_hideip(sess);
	}

	aim_reqservice(sess, fr->conn, AIM_CONN_TYPE_CHATNAV);
	if (sess->authinfo->email)
		aim_reqservice(sess, fr->conn, AIM_CONN_TYPE_EMAIL);

	return 1;
}

static int gaim_offlinemsg(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct aim_icq_offlinemsg *msg;
	struct aim_incomingim_ch4_args args;
	time_t t;

	va_start(ap, fr);
	msg = va_arg(ap, struct aim_icq_offlinemsg *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "Received offline message.  Converting to channel 4 ICBM...\n");
	args.uin = msg->sender;
	args.type = msg->type;
	args.flags = msg->flags;
	args.msglen = msg->msglen;
	args.msg = msg->msg;
	t = gaim_time_build(msg->year, msg->month, msg->day, msg->hour, msg->minute, 0);
	incomingim_chan4(sess, fr->conn, NULL, &args, t);

	return 1;
}

static int gaim_offlinemsgdone(aim_session_t *sess, aim_frame_t *fr, ...)
{
	aim_icq_ackofflinemsgs(sess);
	return 1;
}

#if 0
/*
 * Update, 2003-11-09:
 * Joseph S. Myers, a gcc dude, fixed this for gcc 3.4!  Rock on!
 *
 * It may not be my place to do this, but...
 * I feel pretty strongly that the "last 2 digits" warning is ridiculously 
 * stupid, and should not exist for % switches (%x in our case) that request 
 * a year in the preferred representation for the current locale.  For that 
 * reason I've chosen to not use this workaround (n., see kluge).
 *
 * I have a date.  I want to show it to the user in the "preferred" way.  
 * Whether that displays a 2 digit year is perfectly fine--after all, it's 
 * what the locale wanted.
 * 
 * If I have a necessity for a full representation of the year in the current 
 * locale, then I'll use a switch that returns a full representation of the 
 * year.
 *
 * If you think the preferred locale should show 4 digits instead of 2 digits 
 * (because you're anal, or whatever), then change the f***ing locale.
 *
 * I guess the bottom line is--I'm trying to show a date to the user how they 
 * prefer to see it, why the hell does gcc want me to change that?
 *
 * See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=3190
 * See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=8714
 */

/*
 * This function was recommended by the STRFTIME(3) man page to remove the
 * "last 2 digits" warning.
 */
static size_t my_strftime(char *s, size_t max, const char  *fmt,
			const struct tm *tm)
{
	return strftime(s, max, fmt, tm);
}
#endif

static int gaim_icqinfo(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	GaimBuddy *buddy;
	struct buddyinfo *bi = NULL;
	gchar who[16];
	GString *str;
	gchar *primary, *utf8;
	const gchar *alias;
	va_list ap;
	struct aim_icq_info *info;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (!info->uin)
		return 0;

	str = g_string_sized_new(100);
	g_snprintf(who, sizeof(who), "%u", info->uin);
	buddy = gaim_find_buddy(gaim_connection_get_account(gc), who);
	if (buddy != NULL)
		bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(buddy->account, buddy->name));

	g_string_append_printf(str, "<b>%s:</b> %s", _("UIN"), who);
	oscar_string_append(str, "\n<br>", _("Nick"), info->nick);
	if ((bi != NULL) && (bi->ipaddr != 0)) {
		char *tstr =  g_strdup_printf("%hhu.%hhu.%hhu.%hhu",
						(bi->ipaddr & 0xff000000) >> 24,
						(bi->ipaddr & 0x00ff0000) >> 16,
						(bi->ipaddr & 0x0000ff00) >> 8,
						(bi->ipaddr & 0x000000ff));
		oscar_string_append(str, "\n<br>", _("IP Address"), tstr);
		g_free(tstr);
	}
	oscar_string_append(str, "\n<br>", _("First Name"), info->first);
	oscar_string_append(str, "\n<br>", _("Last Name"), info->last);
	if (info->email && info->email[0] && (utf8 = gaim_utf8_try_convert(info->email))) {
		g_string_append_printf(str, "\n<br><b>%s:</b> <a href=\"mailto:%s\">%s</a>", _("Email Address"), utf8, utf8);
		g_free(utf8);
	}
	if (info->numaddresses && info->email2) {
		int i;
		for (i = 0; i < info->numaddresses; i++) {
			if (info->email2[i] && info->email2[i][0] && (utf8 = gaim_utf8_try_convert(info->email2[i]))) {
				g_string_append_printf(str, "\n<br><b>%s:</b> <a href=\"mailto%s\">%s</a>", _("Email Address"), utf8, utf8);
				g_free(utf8);
			}
		}
	}
	oscar_string_append(str, "\n<br>", _("Mobile Phone"), info->mobile);
	if (info->gender != 0)
		oscar_string_append(str, "\n<br>", _("Gender"), info->gender == 1 ? _("Female") : _("Male"));
	if (info->birthyear || info->birthmonth || info->birthday) {
		char date[30];
		struct tm tm;
		tm.tm_mday = (int)info->birthday;
		tm.tm_mon = (int)info->birthmonth-1;
		tm.tm_year = (int)info->birthyear-1900;
		strftime(date, sizeof(date), "%x", &tm);
		oscar_string_append(str, "\n<br>", _("Birthday"), date);
	}
	if (info->age) {
		char age[5];
		snprintf(age, sizeof(age), "%hhd", info->age);
		oscar_string_append(str, "\n<br>", _("Age"), age);
	}
	if (info->personalwebpage && info->personalwebpage[0] && (utf8 = gaim_utf8_try_convert(info->personalwebpage))) {
		g_string_append_printf(str, "\n<br><b>%s:</b> <a href=\"%s\">%s</a>", _("Personal Web Page"), utf8, utf8);
		g_free(utf8);
	}
	if (info->info && info->info[0] && (utf8 = gaim_utf8_try_convert(info->info))) {
		g_string_append_printf(str, "<hr><b>%s:</b><br>%s", _("Additional Information"), utf8);
		g_free(utf8);
	}
	g_string_append_printf(str, "<hr>\n");
	if ((info->homeaddr && (info->homeaddr[0])) || (info->homecity && info->homecity[0]) || (info->homestate && info->homestate[0]) || (info->homezip && info->homezip[0])) {
		g_string_append_printf(str, "<b>%s:</b>", _("Home Address"));
		oscar_string_append(str, "\n<br>", _("Address"), info->homeaddr);
		oscar_string_append(str, "\n<br>", _("City"), info->homecity);
		oscar_string_append(str, "\n<br>", _("State"), info->homestate);
		oscar_string_append(str, "\n<br>", _("Zip Code"), info->homezip);
		g_string_append_printf(str, "\n<hr>\n");
	}
	if ((info->workaddr && info->workaddr[0]) || (info->workcity && info->workcity[0]) || (info->workstate && info->workstate[0]) || (info->workzip && info->workzip[0])) {
		g_string_append_printf(str, "<b>%s:</b>", _("Work Address"));
		oscar_string_append(str, "\n<br>", _("Address"), info->workaddr);
		oscar_string_append(str, "\n<br>", _("City"), info->workcity);
		oscar_string_append(str, "\n<br>", _("State"), info->workstate);
		oscar_string_append(str, "\n<br>", _("Zip Code"), info->workzip);
		g_string_append_printf(str, "\n<hr>\n");
	}
	if ((info->workcompany && info->workcompany[0]) || (info->workdivision && info->workdivision[0]) || (info->workposition && info->workposition[0]) || (info->workwebpage && info->workwebpage[0])) {
		g_string_append_printf(str, "<b>%s:</b>", _("Work Information"));
		oscar_string_append(str, "\n<br>", _("Company"), info->workcompany);
		oscar_string_append(str, "\n<br>", _("Division"), info->workdivision);
		oscar_string_append(str, "\n<br>", _("Position"), info->workposition);
		if (info->workwebpage && info->workwebpage[0] && (utf8 = gaim_utf8_try_convert(info->workwebpage))) {
			g_string_append_printf(str, "\n<br><b>%s:</b> <a href=\"%s\">%s</a>", _("Web Page"), utf8, utf8);
			g_free(utf8);
		}
		g_string_append_printf(str, "\n<hr>\n");
	}

	if (buddy != NULL)
		alias = gaim_buddy_get_alias(buddy);
	else
		alias = who;
	primary = g_strdup_printf(_("ICQ Info for %s"), alias);
	gaim_notify_userinfo(gc, who, NULL, primary, NULL, str->str, NULL, NULL);
	g_free(primary);
	g_string_free(str, TRUE);

	return 1;
}

static int gaim_icqalias(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	gchar who[16], *utf8;
	GaimBuddy *b;
	va_list ap;
	struct aim_icq_info *info;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (info->uin && info->nick && info->nick[0] && (utf8 = gaim_utf8_try_convert(info->nick))) {
		g_snprintf(who, sizeof(who), "%u", info->uin);
		serv_got_alias(gc, who, utf8);
		if ((b = gaim_find_buddy(gc->account, who))) {
			gaim_blist_node_set_string((GaimBlistNode*)b, "servernick", utf8);
		}
		g_free(utf8);
	}

	return 1;
}

static int gaim_popup(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	gchar *text;
	va_list ap;
	char *msg, *url;
	fu16_t wid, hei, delay;

	va_start(ap, fr);
	msg = va_arg(ap, char *);
	url = va_arg(ap, char *);
	wid = (fu16_t) va_arg(ap, int);
	hei = (fu16_t) va_arg(ap, int);
	delay = (fu16_t) va_arg(ap, int);
	va_end(ap);

	text = g_strdup_printf("%s<br><a href=\"%s\">%s</a>", msg, url, url);
	gaim_notify_formatted(gc, NULL, _("Pop-Up Message"), NULL, text, NULL, NULL);
	g_free(text);

	return 1;
}

static int gaim_parse_searchreply(aim_session_t *sess, aim_frame_t *fr, ...)
{
	GaimConnection *gc = sess->aux_data;
	gchar *secondary;
	GString *text;
	int i, num;
	va_list ap;
	char *email, *SNs;

	va_start(ap, fr);
	email = va_arg(ap, char *);
	num = va_arg(ap, int);
	SNs = va_arg(ap, char *);
	va_end(ap);

	secondary = g_strdup_printf(_("The following screen names are associated with %s"), email);
	text = g_string_new("");
	for (i = 0; i < num; i++)
		g_string_append_printf(text, "%s<br>", &SNs[i * (MAXSNLEN + 1)]);
	gaim_notify_formatted(gc, NULL, _("Search Results"), secondary, text->str, NULL, NULL);

	g_free(secondary);
	g_string_free(text, TRUE);

	return 1;
}

static int gaim_parse_searcherror(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *email;
	char *buf;

	va_start(ap, fr);
	email = va_arg(ap, char *);
	va_end(ap);

	buf = g_strdup_printf(_("No results found for email address %s"), email);
	gaim_notify_error(sess->aux_data, NULL, buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_account_confirm(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	fu16_t status;
	va_list ap;
	char msg[256];

	va_start(ap, fr);
	status = (fu16_t) va_arg(ap, unsigned int); /* status code of confirmation request */
	va_end(ap);

	gaim_debug_info("oscar",
			   "account confirmation returned status 0x%04x (%s)\n", status,
			status ? "unknown" : "email sent");
	if (!status) {
		g_snprintf(msg, sizeof(msg), _("You should receive an email asking to confirm %s."),
				gaim_account_get_username(gaim_connection_get_account(gc)));
		gaim_notify_info(gc, NULL, _("Account Confirmation Requested"), msg);
	}

	return 1;
}

static int gaim_info_change(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	fu16_t perms, err;
	char *url, *sn, *email;
	int change;

	va_start(ap, fr);
	change = va_arg(ap, int);
	perms = (fu16_t) va_arg(ap, unsigned int);
	err = (fu16_t) va_arg(ap, unsigned int);
	url = va_arg(ap, char *);
	sn = va_arg(ap, char *);
	email = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_misc("oscar",
					"account info: because of %s, perms=0x%04x, err=0x%04x, url=%s, sn=%s, email=%s\n",
					change ? "change" : "request", perms, err,
					(url != NULL) ? url : "(null)",
					(sn != NULL) ? sn : "(null)",
					(email != NULL) ? email : "(null)");

	if ((err > 0) && (url != NULL)) {
		char *dialog_msg;
		char *dialog_top = g_strdup_printf(_("Error Changing Account Info"));
		switch (err) {
			case 0x0001: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name differs from the original."), err);
			} break;
			case 0x0006: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name ends in a space."), err);
			} break;
			case 0x000b: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name is too long."), err);
			} break;
			case 0x001d: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because there is already a request pending for this screen name."), err);
			} break;
			case 0x0021: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because the given address has too many screen names associated with it."), err);
			} break;
			case 0x0023: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because the given address is invalid."), err);
			} break;
			default: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unknown error."), err);
			} break;
		}
		gaim_notify_error(gc, NULL, dialog_top, dialog_msg);
		g_free(dialog_top);
		g_free(dialog_msg);
		return 1;
	}

	if (sn != NULL) {
		char *dialog_msg = g_strdup_printf(_("Your screen name is currently formatted as follows:\n%s"), sn);
		gaim_notify_info(gc, NULL, _("Account Info"), dialog_msg);
		g_free(dialog_msg);
	}

	if (email != NULL) {
		char *dialog_msg = g_strdup_printf(_("The email address for %s is %s"), 
						   gaim_account_get_username(gaim_connection_get_account(gc)), email);
		gaim_notify_info(gc, NULL, _("Account Info"), dialog_msg);
		g_free(dialog_msg);
	}

	return 1;
}

static void oscar_keepalive(GaimConnection *gc) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_flap_nop(od->sess, od->conn);
}

static int oscar_send_typing(GaimConnection *gc, const char *name, int typing) {
	OscarData *od = (OscarData *)gc->proto_data;
	struct oscar_direct_im *dim = oscar_direct_im_find(od, name);
	if (dim && dim->connected)
		if (typing == GAIM_TYPING)
			aim_odc_send_typing(od->sess, dim->conn, 0x0002);
		else if (typing == GAIM_TYPED)
			aim_odc_send_typing(od->sess, dim->conn, 0x0001);
		else
			aim_odc_send_typing(od->sess, dim->conn, 0x0000);
	else {
		/* Don't send if this turkey is in our deny list */
		GSList *list;
		for (list=gc->account->deny; (list && aim_sncmp(name, list->data)); list=list->next);
		if (!list) {
			struct buddyinfo *bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(gc->account, name));
			if (bi && bi->typingnot) {
				if (typing == GAIM_TYPING)
					aim_im_sendmtn(od->sess, 0x0001, name, 0x0002);
				else if (typing == GAIM_TYPED)
					aim_im_sendmtn(od->sess, 0x0001, name, 0x0001);
				else
					aim_im_sendmtn(od->sess, 0x0001, name, 0x0000);
			}
		}
	}
	return 0;
}

static int gaim_odc_send_im(aim_session_t *, aim_conn_t *, const char *, GaimConvImFlags);

static int oscar_send_im(GaimConnection *gc, const char *name, const char *message, GaimConvImFlags imflags) {
	OscarData *od = (OscarData *)gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct oscar_direct_im *dim = oscar_direct_im_find(od, name);
	int ret = 0;
	const char *iconfile = gaim_account_get_buddy_icon(account);
	char *tmpmsg = NULL, *tmpmsg2 = NULL;

	if (dim && dim->connected) {
		/* If we're directly connected, send a direct IM */
		ret = gaim_odc_send_im(od->sess, dim->conn, message, imflags);
	} else {
		struct buddyinfo *bi;
		struct aim_sendimext_args args;
		struct stat st;
		gsize len;
		GaimConversation *conv;

		conv = gaim_find_conversation_with_account(GAIM_CONV_IM, name, account);

		if (strstr(message, "<IMG "))
			gaim_conversation_write(conv, "",
			                        _("Your IM Image was not sent. "
			                        "You must be Direct Connected to send IM Images."),
			                        GAIM_MESSAGE_ERROR, time(NULL));

		bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, name));
		if (!bi) {
			bi = g_new0(struct buddyinfo, 1);
			g_hash_table_insert(od->buddyinfo, g_strdup(gaim_normalize(account, name)), bi);
		}

		args.flags = AIM_IMFLAGS_ACK | AIM_IMFLAGS_CUSTOMFEATURES;
		if (od->icq) {
			args.features = features_icq;
			args.featureslen = sizeof(features_icq);
			args.flags |= AIM_IMFLAGS_OFFLINE;
		} else {
			args.features = features_aim;
			args.featureslen = sizeof(features_aim);

			if (imflags & GAIM_CONV_IM_AUTO_RESP)
				args.flags |= AIM_IMFLAGS_AWAY;
		}

		if (bi->ico_need) {
			gaim_debug_info("oscar",
					   "Sending buddy icon request with message\n");
			args.flags |= AIM_IMFLAGS_BUDDYREQ;
			bi->ico_need = FALSE;
		}

		if (iconfile && !stat(iconfile, &st)) {
			FILE *file = fopen(iconfile, "rb");
			if (file) {
				char *buf = g_malloc(st.st_size);
				/* XXX - Use g_file_get_contents()? */
				fread(buf, 1, st.st_size, file);
				fclose(file);

				args.iconlen   = st.st_size;
				args.iconsum   = aimutil_iconsum(buf, st.st_size);
				args.iconstamp = st.st_mtime;

				if ((args.iconlen != bi->ico_me_len) || (args.iconsum != bi->ico_me_csum) || (args.iconstamp != bi->ico_me_time)) {
					bi->ico_informed = FALSE;
					bi->ico_sent     = FALSE;
				}

				if (!bi->ico_informed) {
					gaim_debug_info("oscar",
							   "Claiming to have a buddy icon\n");
					args.flags |= AIM_IMFLAGS_HASICON;
					bi->ico_me_len = args.iconlen;
					bi->ico_me_csum = args.iconsum;
					bi->ico_me_time = args.iconstamp;
					bi->ico_informed = TRUE;
				}

				g_free(buf);
			}
		}

		args.destsn = name;

		/*
		 * If we're IMing an ICQ user then send newlines as CR/LF and
		 * strip all HTML
		 */
		if (aim_sn_is_icq(name) ) {
			/* being sent to an ICQ user */
			if (!aim_sn_is_icq(gaim_account_get_username(account))) {
				/* from an AIM user - ICQ receiving from AIM *expects the messsage to be HTML formatted* */
				tmpmsg = gaim_str_add_cr(message);
			} else {
				/* from an ICQ user - do nothing */
				tmpmsg = g_strdup(message);
			}
		} else {
			/* being sent to an AIM user */
			if (aim_sn_is_icq(gaim_account_get_username(account))) {
				/* from an ICQ user */
				tmpmsg2 = gaim_strdup_withhtml(message);
				tmpmsg = gaim_escape_html(tmpmsg2);
				g_free(tmpmsg2);
			} else
				tmpmsg = gaim_strdup_withhtml(message);
		}
		len = strlen(tmpmsg);

		gaim_plugin_oscar_convert_to_best_encoding(gc, name, tmpmsg, (char **)&args.msg, &args.msglen, &args.charset, &args.charsubset);
		gaim_debug_info("oscar", "Sending IM, charset=0x%04hx, charsubset=0x%04hx, length=%d\n",
						args.charset, args.charsubset, args.msglen);
		ret = aim_im_sendch1_ext(od->sess, &args);
		g_free((char *)args.msg);
	}

	g_free(tmpmsg);

	if (ret >= 0)
		return 1;

	return ret;
}

static void oscar_get_info(GaimConnection *gc, const char *name) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->icq && aim_sn_is_icq(name))
		aim_icq_getallinfo(od->sess, name);
	else
		aim_locate_getinfoshort(od->sess, name, 0x00000003);
}

#if 0
static void oscar_set_dir(GaimConnection *gc, const char *first, const char *middle, const char *last,
			  const char *maiden, const char *city, const char *state, const char *country, int web) {
	/* XXX - some of these things are wrong, but i'm lazy */
	OscarData *od = (OscarData *)gc->proto_data;
	aim_locate_setdirinfo(od->sess, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
}
#endif

static void oscar_set_idle(GaimConnection *gc, int time) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_srv_setidle(od->sess, time);
}

static void oscar_set_info(GaimConnection *gc, const char *text) {
	OscarData *od = (OscarData *)gc->proto_data;
	int charset = 0;
	char *text_html = NULL;
	char *msg = NULL;
	gsize msglen = 0;

	if (od->rights.maxsiglen == 0)
		gaim_notify_warning(gc, NULL, _("Unable to set AIM profile."),
							_("You have probably requested to set your "
							  "profile before the login procedure completed.  "
							  "Your profile remains unset; try setting it "
							  "again when you are fully connected."));

	if (!text) {
		aim_locate_setprofile(od->sess, NULL, "", 0, NULL, NULL, 0);
		return;
	}

	text_html = gaim_strdup_withhtml(text);
	charset = oscar_charset_check(text_html);
	if (charset == AIM_CHARSET_UNICODE) {
		msg = g_convert(text_html, strlen(text_html), "UCS-2BE", "UTF-8", NULL, &msglen, NULL);
		aim_locate_setprofile(od->sess, "unicode-2-0", msg, (msglen > od->rights.maxsiglen ? od->rights.maxsiglen : msglen), NULL, NULL, 0);
		g_free(msg);
	} else if (charset == AIM_CHARSET_CUSTOM) {
		msg = g_convert(text_html, strlen(text_html), "ISO-8859-1", "UTF-8", NULL, &msglen, NULL);
		aim_locate_setprofile(od->sess, "iso-8859-1", msg, (msglen > od->rights.maxsiglen ? od->rights.maxsiglen : msglen), NULL, NULL, 0);
		g_free(msg);
	} else {
		msglen = strlen(text_html);
		aim_locate_setprofile(od->sess, "us-ascii", text_html, (msglen > od->rights.maxsiglen ? od->rights.maxsiglen : msglen), NULL, NULL, 0);
	}

	if (msglen > od->rights.maxsiglen) {
		gchar *errstr;
		errstr = g_strdup_printf(ngettext("The maximum profile length of %d byte "
								 "has been exceeded.  Gaim has truncated it for you.",
								 "The maximum profile length of %d bytes "
								 "has been exceeded.  Gaim has truncated it for you.",
								 od->rights.maxsiglen), od->rights.maxsiglen);
		gaim_notify_warning(gc, NULL, _("Profile too long."), errstr);
		g_free(errstr);
	}

	g_free(text_html);

	return;
}

static void
oscar_set_status_aim(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	OscarData *od = NULL;
	GaimStatusType *status_type;
	GaimStatusPrimitive primitive;
	GaimPresence *presence;
	const gchar *status_id;
	int charset = 0;
	const gchar *text_html = NULL;
	char *msg = NULL;
	gsize msglen = 0;

	status_type = gaim_status_get_type(status);
	primitive = gaim_status_type_get_primitive(status_type);
	status_id = gaim_status_get_id(status);
	presence = gaim_account_get_presence(account);

	if (!gaim_status_is_active(status)) /* Is this right?  I'm confused. */
		return;

	gaim_debug_info("oscar", "Setting status to %s\n", status_id);

	if (gc)
		od = (OscarData *)gc->proto_data;

	if ((od == NULL) || (od->rights.maxawaymsglen == 0)) {
		gaim_notify_warning(gc, NULL, _("Unable to set AIM away message."),
				    _("You have probably requested to set your "
				      "away message before the login procedure "
				      "completed.  You remain in a \"present\" "
				      "state; try setting it again when you are "
				      "fully connected."));
		return;
	}

	if (primitive == GAIM_STATUS_AVAILABLE) {
		aim_setextstatus(od->sess, AIM_ICQ_STATE_NORMAL);

		aim_locate_setprofile(od->sess, NULL, NULL, 0, NULL, "", 0);

		text_html = gaim_status_get_attr_string(status, "message");
		if (text_html != NULL) {
			aim_srv_setavailmsg(od->sess, text_html);
		}

	} else if (primitive == GAIM_STATUS_AWAY) {
		aim_setextstatus(od->sess, AIM_ICQ_STATE_NORMAL);

		text_html = gaim_status_get_attr_string(status, "message");

		if (text_html == NULL) {
			text_html = _("Away");
		}

		charset = oscar_charset_check(text_html);
		if (charset == AIM_CHARSET_UNICODE) {
			msg = g_convert(text_html, strlen(text_html), "UCS-2BE", "UTF-8", NULL, &msglen, NULL);
			aim_locate_setprofile(od->sess, NULL, NULL, 0, "unicode-2-0", msg,
				(msglen > od->rights.maxawaymsglen ? od->rights.maxawaymsglen : msglen));
			g_free(msg);
		} else if (charset == AIM_CHARSET_CUSTOM) {
			msg = g_convert(text_html, strlen(text_html), "ISO-8859-1", "UTF-8", NULL, &msglen, NULL);
			aim_locate_setprofile(od->sess, NULL, NULL, 0, "iso-8859-1", msg,
				(msglen > od->rights.maxawaymsglen ? od->rights.maxawaymsglen : msglen));
			g_free(msg);
		} else {
			msglen = strlen(text_html);
			aim_locate_setprofile(od->sess, NULL, NULL, 0, "us-ascii", text_html,
				(msglen > od->rights.maxawaymsglen ? od->rights.maxawaymsglen : msglen));
		}

		if (msglen > od->rights.maxawaymsglen) {
			gchar *errstr;

			errstr = g_strdup_printf(ngettext("The maximum away message length of %d byte "
									 "has been exceeded.  Gaim has truncated it for you.",
									 "The maximum away message length of %d bytes "
									 "has been exceeded.  Gaim has truncated it for you.",
									 od->rights.maxawaymsglen), od->rights.maxawaymsglen);
			gaim_notify_warning(gc, NULL, _("Away message too long."), errstr);
			g_free(errstr);
		}

	} else if (primitive == GAIM_STATUS_HIDDEN) {
		aim_setextstatus(od->sess, AIM_ICQ_STATE_INVISIBLE);

	} else {
		gaim_debug_info("oscar", "Don't know what to do for this status\n");

	}
}

static void
oscar_set_status_icq(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	OscarData *od = (OscarData *)gc->proto_data;
	const gchar *status_id = gaim_status_get_id(status);

	if (gaim_status_type_get_primitive(gaim_status_get_type(status)) == GAIM_STATUS_HIDDEN)
		account->perm_deny = 4;
	else
		account->perm_deny = 3;

	if ((od->sess->ssi.received_data) && (aim_ssi_getpermdeny(od->sess->ssi.local) != account->perm_deny))
		aim_ssi_setpermdeny(od->sess, account->perm_deny, 0xffffffff);

	if (!strcmp(status_id, OSCAR_STATUS_ID_ONLINE))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_NORMAL);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_AWAY))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_AWAY);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_DND))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_DND | AIM_ICQ_STATE_BUSY);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_NA))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_OCCUPIED))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_BUSY);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_FREE4CHAT))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_CHAT);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_INVISIBLE))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_INVISIBLE);

	else if (!strcmp(status_id, OSCAR_STATUS_ID_CUSTOM))
		aim_setextstatus(od->sess, AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY);

	return;
}

static void
oscar_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimStatusType *type = gaim_status_get_type(status);
	int primitive = gaim_status_type_get_primitive(type);

	if (primitive == !GAIM_STATUS_OFFLINE && !gc) {
		gaim_account_connect(account, status);
	} else if (primitive == GAIM_STATUS_OFFLINE && gc) {
		gaim_account_disconnect(account);
	} else {

		if (aim_sn_is_icq(gaim_account_get_username(account)))
			oscar_set_status_icq(account, status);
		else
			oscar_set_status_aim(account, status);
	}
	return;
}

static void
oscar_warn(GaimConnection *gc, const char *name, gboolean anonymous) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_im_warn(od->sess, od->conn, name, anonymous ? AIM_WARN_ANON : 0);
}

static void oscar_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (!aim_snvalid(buddy->name)) {
		gchar *buf;
		buf = g_strdup_printf(_("Could not add the buddy %s because the screen name is invalid.  Screen names must either start with a letter and contain only letters, numbers and spaces, or contain only numbers."), buddy->name);
		if (!gaim_conv_present_error(buddy->name, gaim_connection_get_account(gc), buf))
			gaim_notify_error(gc, NULL, _("Unable To Add"), buf);
		g_free(buf);

		/* Remove from local list */
		gaim_blist_remove_buddy(buddy);

		return;
	}

#ifdef NOSSI
	aim_buddylist_addbuddy(od->sess, od->conn, buddy->name);
#else
	if ((od->sess->ssi.received_data) && !(aim_ssi_itemlist_finditem(od->sess->ssi.local, group->name, buddy->name, AIM_SSI_TYPE_BUDDY))) {
		if (buddy && group) {
			gaim_debug_info("oscar",
					   "ssi: adding buddy %s to group %s\n", buddy->name, group->name);
			aim_ssi_addbuddy(od->sess, buddy->name, group->name, gaim_buddy_get_alias_only(buddy), NULL, NULL, 0);
		}
	}
#endif

	/* XXX - Should this be done from AIM accounts, as well? */
	if (od->icq)
		aim_icq_getalias(od->sess, buddy->name);
}

static void oscar_add_buddies(GaimConnection *gc, GList *buddies, GList *groups) {
	OscarData *od = (OscarData *)gc->proto_data;
#ifdef NOSSI
	char buf[MSG_LEN];
	int n=0;

	while (buddies) {
		GaimBuddy *buddy = buddies->data;
		if (n > MSG_LEN - 18) {
			aim_buddylist_set(od->sess, od->conn, buf);
			n = 0;
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, "%s&", buddy->name);
		buddies = buddies->next;
	}
	aim_buddylist_set(od->sess, od->conn, buf);
#else

	if (od->sess->ssi.received_data) {
		GList *curb = buddies;
		GList *curg = groups;
		while ((curb != NULL) && (curg != NULL)) {
			GaimBuddy *buddy = curb->data;
			GaimGroup *group = curg->data;
			oscar_add_buddy(gc, buddy, group);
			curb = curb->next;
			curg = curg->next;
		}
	}
#endif
}

static void oscar_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {
	OscarData *od = (OscarData *)gc->proto_data;

#ifdef NOSSI
	aim_buddylist_removebuddy(od->sess, od->conn, buddy->name);
#else
	if (od->sess->ssi.received_data) {
		gaim_debug_info("oscar",
				   "ssi: deleting buddy %s from group %s\n", buddy->name, group->name);
		aim_ssi_delbuddy(od->sess, buddy->name, group->name);
	}
#endif
}

static void oscar_remove_buddies(GaimConnection *gc, GList *buddies, GList *groups) {
	OscarData *od = (OscarData *)gc->proto_data;

#ifdef NOSSI
	for (cur = buddies; cur != NULL; cur = cur->next) {
		GaimBuddy *buddy = cur->data;
		aim_buddylist_removebuddy(od->sess, od->conn, buddy->name);
	}
#else
	if (od->sess->ssi.received_data) {
		GList *curb = buddies;
		GList *curg = groups;
		while ((curb != NULL) && (curg != NULL)) {
			GaimBuddy *buddy = curb->data;
			GaimGroup *group = curg->data;
			oscar_remove_buddy(gc, buddy, group);
			curb = curb->next;
			curg = curg->next;
		}
	}
#endif
}

#ifndef NOSSI
static void oscar_move_buddy(GaimConnection *gc, const char *name, const char *old_group, const char *new_group) {
	OscarData *od = (OscarData *)gc->proto_data;
	if (od->sess->ssi.received_data && strcmp(old_group, new_group)) {
		gaim_debug_info("oscar",
				   "ssi: moving buddy %s from group %s to group %s\n", name, old_group, new_group);
		aim_ssi_movebuddy(od->sess, old_group, new_group, name);
	}
}

static void oscar_alias_buddy(GaimConnection *gc, const char *name, const char *alias) {
	OscarData *od = (OscarData *)gc->proto_data;
	if (od->sess->ssi.received_data) {
		char *gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, name);
		if (gname) {
			gaim_debug_info("oscar",
					   "ssi: changing the alias for buddy %s to %s\n", name, alias ? alias : "(none)");
			aim_ssi_aliasbuddy(od->sess, gname, name, alias);
		}
	}
}

static void oscar_rename_group(GaimConnection *gc, const char *old_name, GaimGroup *group, GList *moved_buddies) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->sess->ssi.received_data) {
		if (aim_ssi_itemlist_finditem(od->sess->ssi.local, group->name, NULL, AIM_SSI_TYPE_GROUP)) {
			GList *cur, *groups = NULL;

			/* Make a list of what the groups each buddy is in */
			for (cur = moved_buddies; cur != NULL; cur = cur->next) {
				GaimBlistNode *node = cur->data;
				groups = g_list_append(groups, node->parent);
			}

			oscar_remove_buddies(gc, moved_buddies, groups);
			oscar_add_buddies(gc, moved_buddies, groups);
			g_list_free(groups);
			gaim_debug_info("oscar",
					   "ssi: moved all buddies from group %s to %s\n", old_name, group->name);
		} else {
			aim_ssi_rename_group(od->sess, old_name, group->name);
			gaim_debug_info("oscar",
					   "ssi: renamed group %s to %s\n", old_name, group->name);
		}
	}
}

static gboolean gaim_ssi_rerequestdata(gpointer data) {
	aim_session_t *sess = data;
	aim_ssi_reqdata(sess);
	return FALSE;
}

static int gaim_ssi_parseerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	va_list ap;
	fu16_t reason;

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_error("oscar", "ssi: SNAC error %hu\n", reason);

	if (reason == 0x0005) {
		gaim_notify_error(gc, NULL, _("Unable To Retrieve Buddy List"),
						  _("Gaim was temporarily unable to retrieve your buddy list from the AIM servers.  Your buddy list is not lost, and will probably become available in a few hours."));
		od->getblisttimer = gaim_timeout_add(300000, gaim_ssi_rerequestdata, od->sess);
	}

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	gaim_debug_info("oscar", "ssi: activating server-stored buddy list\n");
	aim_ssi_enable(od->sess);

	return 1;
}

static int gaim_ssi_parserights(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = (OscarData *)gc->proto_data;
	int i;
	va_list ap;
	int numtypes;
	fu16_t *maxitems;

	va_start(ap, fr);
	numtypes = va_arg(ap, int);
	maxitems = va_arg(ap, fu16_t *);
	va_end(ap);

	gaim_debug_misc("oscar", "ssi rights:");

	for (i=0; i<numtypes; i++)
		gaim_debug_misc(NULL, " max type 0x%04x=%hd,",
				   i, maxitems[i]);

	gaim_debug_misc(NULL, "\n");

	if (numtypes >= 0)
		od->rights.maxbuddies = maxitems[0];
	if (numtypes >= 1)
		od->rights.maxgroups = maxitems[1];
	if (numtypes >= 2)
		od->rights.maxpermits = maxitems[2];
	if (numtypes >= 3)
		od->rights.maxdenies = maxitems[3];

	return 1;
}

static int gaim_ssi_parselist(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimStatus *status;
	OscarData *od = (OscarData *)gc->proto_data;
	GaimGroup *g;
	GaimBuddy *b;
	struct aim_ssi_item *curitem;
	int tmp;
	va_list ap;
	fu16_t fmtver, numitems;
	struct aim_ssi_item *items;
	fu32_t timestamp;

	va_start(ap, fr);
	fmtver = (fu16_t)va_arg(ap, int);
	numitems = (fu16_t)va_arg(ap, int);
	items = va_arg(ap, struct aim_ssi_item *);
	timestamp = va_arg(ap, fu32_t);
	va_end(ap);

	gaim_debug_info("oscar",
			   "ssi: syncing local list and server list\n");

	if ((timestamp == 0) || (numitems == 0)) {
		gaim_debug_info("oscar", "Got AIM SSI with a 0 timestamp or 0 numitems--not syncing.  This probably means your buddy list is empty.", NULL);
		return 1;
	}

	/* Clean the buddy list */
	aim_ssi_cleanlist(sess);

	{ /* If not in server list then prune from local list */
		GaimBlistNode *gnode, *cnode, *bnode;
		GaimBuddyList *blist;
		GSList *cur, *next;

		/* Buddies */
		cur = NULL;
		if ((blist = gaim_get_blist()) != NULL) {
			for (gnode = blist->root; gnode; gnode = gnode->next) {
				if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
					continue;
				g = (GaimGroup *)gnode;
				for (cnode = gnode->child; cnode; cnode = cnode->next) {
					if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
						continue;
					for (bnode = cnode->child; bnode; bnode = bnode->next) {
						if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
							continue;
						b = (GaimBuddy *)bnode;
						if (b->account == gc->account) {
							if (aim_ssi_itemlist_exists(sess->ssi.local, b->name)) {
								/* If the buddy is an ICQ user then load his nickname */
								const char *servernick = gaim_blist_node_get_string((GaimBlistNode*)b, "servernick");
								char *alias;
								if (servernick)
									serv_got_alias(gc, b->name, servernick);

								/* Store local alias on server */
								alias = aim_ssi_getalias(sess->ssi.local, g->name, b->name);
								if (!alias && b->alias && strlen(b->alias))
									aim_ssi_aliasbuddy(sess, g->name, b->name, b->alias);
								free(alias);
							} else {
								gaim_debug_info("oscar",
										"ssi: removing buddy %s from local list\n", b->name);
								/* We can't actually remove now because it will screw up our looping */
								cur = g_slist_prepend(cur, b);
							}
						}
					}
				}
			}
		}

		while (cur != NULL) {
			b = cur->data;
			cur = g_slist_remove(cur, b);
			gaim_blist_remove_buddy(b);
		}

		/* Permit list */
		if (gc->account->permit) {
			next = gc->account->permit;
			while (next != NULL) {
				cur = next;
				next = next->next;
				if (!aim_ssi_itemlist_finditem(sess->ssi.local, NULL, cur->data, AIM_SSI_TYPE_PERMIT)) {
					gaim_debug_info("oscar",
							"ssi: removing permit %s from local list\n", (const char *)cur->data);
					gaim_privacy_permit_remove(account, cur->data, TRUE);
				}
			}
		}

		/* Deny list */
		if (gc->account->deny) {
			next = gc->account->deny;
			while (next != NULL) {
				cur = next;
				next = next->next;
				if (!aim_ssi_itemlist_finditem(sess->ssi.local, NULL, cur->data, AIM_SSI_TYPE_DENY)) {
					gaim_debug_info("oscar",
							"ssi: removing deny %s from local list\n", (const char *)cur->data);
					gaim_privacy_deny_remove(account, cur->data, TRUE);
				}
			}
		}
		/* Presence settings (idle time visibility) */
		if ((tmp = aim_ssi_getpresence(sess->ssi.local)) != 0xFFFFFFFF)
			if (!(tmp & 0x400))
				aim_ssi_setpresence(sess, tmp | 0x400);
	} /* end pruning buddies from local list */

	/* Add from server list to local list */
	for (curitem=sess->ssi.local; curitem; curitem=curitem->next) {
		if ((curitem->name == NULL) || (g_utf8_validate(curitem->name, -1, NULL)))
		switch (curitem->type) {
			case 0x0000: { /* Buddy */
				if (curitem->name) {
					char *gname = aim_ssi_itemlist_findparentname(sess->ssi.local, curitem->name);
					char *gname_utf8 = gname ? gaim_utf8_try_convert(gname) : NULL;
					char *alias = aim_ssi_getalias(sess->ssi.local, gname, curitem->name);
					char *alias_utf8 = alias ? gaim_utf8_try_convert(alias) : NULL;
					b = gaim_find_buddy(gc->account, curitem->name);
					/* Should gname be freed here? -- elb */
					/* Not with the current code, but that might be cleaner -- med */
					free(alias);
					if (b) {
						/* Get server stored alias */
						if (alias_utf8) {
							g_free(b->alias);
							b->alias = g_strdup(alias_utf8);
						}
					} else {
						b = gaim_buddy_new(gc->account, curitem->name, alias_utf8);

						if (!(g = gaim_find_group(gname_utf8 ? gname_utf8 : _("Orphans")))) {
							g = gaim_group_new(gname_utf8 ? gname_utf8 : _("Orphans"));
							gaim_blist_add_group(g, NULL);
						}

						gaim_debug_info("oscar",
								   "ssi: adding buddy %s to group %s to local list\n", curitem->name, gname_utf8 ? gname_utf8 : _("Orphans"));
						gaim_blist_add_buddy(b, NULL, g, NULL);
					}
					if (!aim_sncmp(curitem->name, account->username)) {
						char *comment = aim_ssi_getcomment(sess->ssi.local, gname, curitem->name);
						gaim_check_comment(od, comment);
						free(comment);
					}
					g_free(gname_utf8);
					g_free(alias_utf8);
				}
			} break;

			case 0x0001: { /* Group */
				/* Shouldn't add empty groups */
			} break;

			case 0x0002: { /* Permit buddy */
				if (curitem->name) {
					/* if (!find_permdeny_by_name(gc->permit, curitem->name)) { AAA */
					GSList *list;
					for (list=account->permit; (list && aim_sncmp(curitem->name, list->data)); list=list->next);
					if (!list) {
						gaim_debug_info("oscar",
								   "ssi: adding permit buddy %s to local list\n", curitem->name);
						gaim_privacy_permit_add(account, curitem->name, TRUE);
					}
				}
			} break;

			case 0x0003: { /* Deny buddy */
				if (curitem->name) {
					GSList *list;
					for (list=account->deny; (list && aim_sncmp(curitem->name, list->data)); list=list->next);
					if (!list) {
						gaim_debug_info("oscar",
								   "ssi: adding deny buddy %s to local list\n", curitem->name);
						gaim_privacy_deny_add(account, curitem->name, TRUE);
					}
				}
			} break;

			case 0x0004: { /* Permit/deny setting */
				if (curitem->data) {
					fu8_t permdeny;
					if ((permdeny = aim_ssi_getpermdeny(sess->ssi.local)) && (permdeny != account->perm_deny)) {
						gaim_debug_info("oscar",
								   "ssi: changing permdeny from %d to %hhu\n", account->perm_deny, permdeny);
						account->perm_deny = permdeny;
						if (od->icq && account->perm_deny == 0x03) {
							gaim_presence_switch_status(account->presence, OSCAR_STATUS_ID_INVISIBLE);
						}
					}
				}
			} break;

			case 0x0005: { /* Presence setting */
				/* We don't want to change Gaim's setting because it applies to all accounts */
			} break;
		} /* End of switch on curitem->type */
	} /* End of for loop */

	/*
	 * XXX - STATUS - Set our ICQ status.  We probably don't want to do
	 * this.  We probably want the SSI status setting to override the local
	 * setting.
	 */
	status = gaim_presence_get_active_status(account->presence);
	if (gaim_status_is_available(status))
		aim_setextstatus(sess, AIM_ICQ_STATE_NORMAL);

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	gaim_debug_info("oscar",
			   "ssi: activating server-stored buddy list\n");
	aim_ssi_enable(sess);

	return 1;
}

static int gaim_ssi_parseack(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	struct aim_ssi_tmp *retval;

	va_start(ap, fr);
	retval = va_arg(ap, struct aim_ssi_tmp *);
	va_end(ap);

	while (retval) {
		gaim_debug_misc("oscar",
				   "ssi: status is 0x%04hx for a 0x%04hx action with name %s\n", retval->ack,  retval->action, retval->item ? (retval->item->name ? retval->item->name : "no name") : "no item");

		if (retval->ack != 0xffff)
		switch (retval->ack) {
			case 0x0000: { /* added successfully */
			} break;

			case 0x000c: { /* you are over the limit, the cheat is to the limit, come on fhqwhgads */
				gchar *buf;
				buf = g_strdup_printf(_("Could not add the buddy %s because you have too many buddies in your buddy list.  Please remove one and try again."), (retval->name ? retval->name : _("(no name)")));
				if ((retval->name != NULL) && !gaim_conv_present_error(retval->name, gaim_connection_get_account(gc), buf))
					gaim_notify_error(gc, NULL, _("Unable To Add"), buf);
				g_free(buf);
			}

			case 0x000e: { /* buddy requires authorization */
				if ((retval->action == AIM_CB_SSI_ADD) && (retval->name))
					gaim_auth_sendrequest(gc, retval->name);
			} break;

			default: { /* La la la */
				gchar *buf;
				gaim_debug_error("oscar", "ssi: Action 0x%04hx was unsuccessful with error 0x%04hx\n", retval->action, retval->ack);
				buf = g_strdup_printf(_("Could not add the buddy %s for an unknown reason.  The most common reason for this is that you have the maximum number of allowed buddies in your buddy list."), (retval->name ? retval->name : _("(no name)")));
				if ((retval->name != NULL) && !gaim_conv_present_error(retval->name, gaim_connection_get_account(gc), buf))
					gaim_notify_error(gc, NULL, _("Unable To Add"), buf);
				g_free(buf);
			} break;
		}

		retval = retval->next;
	}

	return 1;
}

static int gaim_ssi_parseadd(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	char *gname, *gname_utf8, *alias, *alias_utf8;
	GaimBuddy *b;
	GaimGroup *g;
	va_list ap;
	fu16_t type;
	const char *name;

	va_start(ap, fr);
	type = (fu16_t)va_arg(ap, int);
	name = va_arg(ap, char *);
	va_end(ap);

	if ((type != 0x0000) || (name == NULL))
		return 1;

	gname = aim_ssi_itemlist_findparentname(sess->ssi.local, name);
	gname_utf8 = gname ? gaim_utf8_try_convert(gname) : NULL;
	alias = aim_ssi_getalias(sess->ssi.local, gname, name);
	alias_utf8 = alias ? gaim_utf8_try_convert(alias) : NULL;
	b = gaim_find_buddy(gc->account, name);
	free(alias);

	if (b) {
		/* Get server stored alias */
		if (alias_utf8) {
			g_free(b->alias);
			b->alias = g_strdup(alias_utf8);
		}
	} else {
		b = gaim_buddy_new(gc->account, name, alias_utf8);

		if (!(g = gaim_find_group(gname_utf8 ? gname_utf8 : _("Orphans")))) {
			g = gaim_group_new(gname_utf8 ? gname_utf8 : _("Orphans"));
			gaim_blist_add_group(g, NULL);
		}

		gaim_debug_info("oscar",
				   "ssi: adding buddy %s to group %s to local list\n", name, gname_utf8 ? gname_utf8 : _("Orphans"));
		gaim_blist_add_buddy(b, NULL, g, NULL);
	}
	g_free(gname_utf8);
	g_free(alias_utf8);

	return 1;
}

static int gaim_ssi_authgiven(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	struct name_data *data;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "ssi: %s has given you permission to add him to your buddy list\n", sn);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_buddy_get_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_buddy_get_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	dialog_msg = g_strdup_printf(_("The user %s has given you permission to add you to their buddy list.  Do you want to add them?"), nombre);
	data = g_new(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(sn);
	data->nick = NULL;

	gaim_request_yes_no(gc, NULL, _("Authorization Given"), dialog_msg,
						GAIM_DEFAULT_ACTION_NONE, data,
						G_CALLBACK(gaim_icq_buddyadd),
						G_CALLBACK(oscar_free_name_data));

	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_authrequest(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	struct name_data *data;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "ssi: received authorization request from %s\n", sn);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_buddy_get_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_buddy_get_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	dialog_msg = g_strdup_printf(_("The user %s wants to add you to their buddy list for the following reason:\n%s"), nombre, msg ? msg : _("No reason given."));
	data = g_new(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(sn);
	data->nick = NULL;

	gaim_request_action(gc, NULL, _("Authorization Request"), dialog_msg,
						GAIM_DEFAULT_ACTION_NONE, data, 2,
						_("Authorize"), G_CALLBACK(gaim_auth_grant),
						_("Deny"), G_CALLBACK(gaim_auth_dontgrant_msgprompt));

	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_authreply(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	fu8_t reply;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	reply = (fu8_t)va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "ssi: received authorization reply from %s.  Reply is 0x%04hhx\n", sn, reply);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_buddy_get_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_buddy_get_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	if (reply) {
		/* Granted */
		dialog_msg = g_strdup_printf(_("The user %s has granted your request to add them to your buddy list."), nombre);
		gaim_notify_info(gc, NULL, _("Authorization Granted"), dialog_msg);
	} else {
		/* Denied */
		dialog_msg = g_strdup_printf(_("The user %s has denied your request to add them to your buddy list for the following reason:\n%s"), nombre, msg ? msg : _("No reason given."));
		gaim_notify_info(gc, NULL, _("Authorization Denied"), dialog_msg);
	}
	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_gotadded(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	va_list ap;
	char *sn;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	va_end(ap);

	buddy = gaim_find_buddy(gc->account, sn);
	gaim_debug_info("oscar",
			   "ssi: %s added you to their buddy list\n", sn);
	gaim_account_notify_added(gc->account, NULL, sn, (buddy ? gaim_buddy_get_alias_only(buddy) : NULL), NULL);

	return 1;
}
#endif

static GList *oscar_chat_info(GaimConnection *gc) {
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Exchange:");
	pce->identifier = "exchange";
	pce->is_int = TRUE;
	pce->min = 4;
	pce->max = 20;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *oscar_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "room", g_strdup(chat_name));

	return defaults;
}

static char *oscar_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "room"));
}

static void oscar_join_chat(GaimConnection *gc, GHashTable *data) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_conn_t *cur;
	char *name, *exchange;

	name = g_hash_table_lookup(data, "room");
	exchange = g_hash_table_lookup(data, "exchange");

	gaim_debug_info("oscar",
			   "Attempting to join chat room %s.\n", name);

	if ((name == NULL) || (*name == '\0')) {
		gaim_notify_error(gc, NULL, _("Invalid chat name specified."), NULL);
		return;
	}

	if ((cur = aim_getconn_type(od->sess, AIM_CONN_TYPE_CHATNAV))) {
		gaim_debug_info("oscar",
				   "chatnav exists, creating room\n");
		aim_chatnav_createroom(od->sess, cur, name, atoi(exchange));
	} else {
		/* this gets tricky */
		struct create_room *cr = g_new0(struct create_room, 1);
		gaim_debug_info("oscar",
				   "chatnav does not exist, opening chatnav\n");
		cr->exchange = atoi(exchange);
		cr->name = g_strdup(name);
		od->create_rooms = g_slist_append(od->create_rooms, cr);
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_CHATNAV);
	}
}

static void oscar_chat_invite(GaimConnection *gc, int id, const char *message, const char *name) {
	OscarData *od = (OscarData *)gc->proto_data;
	struct chat_connection *ccon = find_oscar_chat(gc, id);

	if (ccon == NULL)
		return;

	aim_im_sendch2_chatinvite(od->sess, name, message ? message : "",
			ccon->exchange, ccon->name, 0x0);
}

static void oscar_chat_leave(GaimConnection *gc, int id) {
	OscarData *od = gc ? (OscarData *)gc->proto_data : NULL;
	GSList *bcs = gc->buddy_chats;
	GaimConversation *b = NULL;
	struct chat_connection *c = NULL;
	int count = 0;

	while (bcs) {
		count++;
		b = (GaimConversation *)bcs->data;
		if (id == gaim_conv_chat_get_id(GAIM_CONV_CHAT(b)))
			break;
		bcs = bcs->next;
		b = NULL;
	}

	if (!b)
		return;

	gaim_debug_info("oscar",
			   "Attempting to leave room %s (currently in %d rooms)\n", b->name, count);

	c = find_oscar_chat(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(b)));
	if (c != NULL) {
		if (od)
			od->oscar_chats = g_slist_remove(od->oscar_chats, c);
		if (c->inpa > 0)
			gaim_input_remove(c->inpa);
		if (gc && od->sess)
			aim_conn_kill(od->sess, &c->conn);
		g_free(c->name);
		g_free(c->show);
		g_free(c);
	}
	/* we do this because with Oscar it doesn't tell us we left */
	serv_got_chat_left(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(b)));
}

static int oscar_send_chat(GaimConnection *gc, int id, const char *message) {
	OscarData *od = (OscarData *)gc->proto_data;
	GaimConversation *conv = NULL;
	struct chat_connection *c = NULL;
	char *buf, *buf2;
	fu16_t charset, charsubset;
	char *charsetstr = NULL;
	int len;

	if (!(conv = gaim_find_chat(gc, id)))
		return -EINVAL;

	if (!(c = find_oscar_chat_by_conv(gc, conv)))
		return -EINVAL;

	buf = gaim_strdup_withhtml(message);
	len = strlen(buf);

	if (strstr(buf, "<IMG "))
		gaim_conversation_write(conv, "",
		                        _("Your IM Image was not sent. "
		                        "You cannot send IM Images in AIM chats."),
		                        GAIM_MESSAGE_ERROR, time(NULL));

	gaim_plugin_oscar_convert_to_best_encoding(gc, NULL, buf, &buf2, &len, &charset, &charsubset);
	if ((len > c->maxlen) || (len > c->maxvis)) {
		g_free(buf2);
		return -E2BIG;
	}

	if (charset == AIM_CHARSET_ASCII)
		charsetstr = "us-ascii";
	else if (charset == AIM_CHARSET_UNICODE)
		charsetstr = "unicode-2-0";
	else if (charset == AIM_CHARSET_CUSTOM)
		charsetstr = "iso-8859-1";
	aim_chat_send_im(od->sess, c->conn, 0, buf2, len, charsetstr, "en");
	g_free(buf2);

	return 0;
}

static const char *oscar_list_icon(GaimAccount *a, GaimBuddy *b)
{
	if (!b || (b && b->name && b->name[0] == '+')) {
		if (a != NULL && aim_sn_is_icq(gaim_account_get_username(a)))
			return "icq";
		else
			return "aim";
	}

	if (b != NULL && aim_sn_is_icq(b->name))
		return "icq";
	return "aim";
}

static void oscar_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
{
	GaimConnection *gc = NULL;
	OscarData *od = NULL;
	GaimAccount *account = NULL;
	GaimPresence *presence;
	GaimStatus *status;
	const char *status_id;
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	int i = 0;
	aim_userinfo_t *userinfo = NULL;

	if (b != NULL)
		account = b->account;
	if (account != NULL)
		gc = account->gc;
	if (gc != NULL)
		od = gc->proto_data;
	if (od != NULL)
		userinfo = aim_locate_finduserinfo(od->sess, b->name);

	presence = gaim_buddy_get_presence(b);
	status = gaim_presence_get_active_status(presence);
	status_id = gaim_status_get_id(status);

	if (gaim_presence_is_online(presence) == FALSE) {
		char *gname;
		if ((b->name) && (od) && (od->sess->ssi.received_data) && 
			(gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, b->name)) && 
			(aim_ssi_waitingforauth(od->sess->ssi.local, gname, b->name))) {
			emblems[i++] = "notauthorized";
		} else {
			emblems[i++] = "offline";
		}
	}

	if (b->name && aim_sn_is_icq(b->name)) {
		if (!strcmp(status_id, OSCAR_STATUS_ID_INVISIBLE))
				emblems[i++] = "invisible";
		else if (!strcmp(status_id, OSCAR_STATUS_ID_FREE4CHAT))
			emblems[i++] = "freeforchat";
		else if (!strcmp(status_id, OSCAR_STATUS_ID_DND))
			emblems[i++] = "dnd";
		else if (!strcmp(status_id, OSCAR_STATUS_ID_NA))
			emblems[i++] = "na";
		else if (!strcmp(status_id, OSCAR_STATUS_ID_OCCUPIED))
			emblems[i++] = "occupied";
		else if (!strcmp(status_id, OSCAR_STATUS_ID_AWAY))
			emblems[i++] = "away";
	} else if (!strcmp(status_id, OSCAR_STATUS_ID_AWAY)) {
		emblems[i++] = "away";
	}

	if (userinfo != NULL ) {
	/*  if (userinfo->flags & AIM_FLAG_UNCONFIRMED)
			emblems[i++] = "unconfirmed"; */
		if (userinfo->flags & AIM_FLAG_ADMINISTRATOR)
			emblems[i++] = "admin";
		if (userinfo->flags & AIM_FLAG_AOL)
			emblems[i++] = "aol";
		if (userinfo->flags & AIM_FLAG_WIRELESS)
			emblems[i++] = "wireless";
		if (userinfo->flags & AIM_FLAG_ACTIVEBUDDY)
			emblems[i++] = "activebuddy";

		if ((i < 4) && (userinfo->capabilities & AIM_CAPS_HIPTOP))
			emblems[i++] = "hiptop";

		if ((i < 4) && (userinfo->capabilities & AIM_CAPS_SECUREIM))
			emblems[i++] = "secure";
	}

	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *oscar_tooltip_text(GaimBuddy *b) {
	GaimConnection *gc = b->account->gc;
	OscarData *od = gc->proto_data;
	aim_userinfo_t *userinfo = aim_locate_finduserinfo(od->sess, b->name);
	GString *str = g_string_new("");

	if (GAIM_BUDDY_IS_ONLINE(b)) {
		oscar_string_append_info(gc, str, "\n", b, userinfo);

		if ((userinfo != NULL) && (userinfo->flags & AIM_FLAG_AWAY) && (userinfo->away_len > 0) && (userinfo->away != NULL) && (userinfo->away_encoding != NULL)) {
			gchar *charset = oscar_encoding_extract(userinfo->away_encoding);
			gchar *away_utf8 = oscar_encoding_to_utf8(charset, userinfo->away, userinfo->away_len);
			g_free(charset);
			if (away_utf8 != NULL) {
				gchar *tmp1, *tmp2;
				tmp2 = gaim_markup_strip_html(away_utf8);
				g_free(away_utf8);
				tmp1 = gaim_escape_html(tmp2);
				g_free(tmp2);
				tmp2 = gaim_str_sub_away_formatters(tmp1, gaim_account_get_username(gaim_connection_get_account(gc)));
				g_free(tmp1);
				g_string_append_printf(str, "\n<b>%s:</b> %s", _("Away Message"), tmp2);
				g_free(tmp2);
			}
		}
	}

	return g_string_free(str, FALSE);
}

static char *oscar_status_text(GaimBuddy *b)
{
	GaimConnection *gc;
	OscarData *od;
	GaimStatus *status;
	gchar *ret = NULL;

	gc = gaim_account_get_connection(gaim_buddy_get_account(b));
	od = gc->proto_data;
	status = gaim_presence_get_active_status(gaim_buddy_get_presence(b));

	if (gaim_status_is_available(status) == FALSE || (((b->uc & 0xffff0000) >> 16) & AIM_ICQ_STATE_CHAT)) {
		if (aim_sn_is_icq(b->name))
			ret = oscar_icqstatus((b->uc & 0xffff0000) >> 16);
		else
			ret = g_strdup(_("Away"));
	} else if (GAIM_BUDDY_IS_ONLINE(b)) {
		struct buddyinfo *bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(b->account, b->name));
		if ((bi != NULL) && (bi->availmsg != NULL))
			ret = g_markup_escape_text(bi->availmsg, strlen(bi->availmsg));
	} else {
		char *gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, b->name);
		if (aim_ssi_waitingforauth(od->sess->ssi.local, gname, b->name))
			ret = g_strdup(_("Not Authorized"));
		else
			ret = g_strdup(_("Offline"));
	}

	return ret;
}


static int oscar_icon_req(aim_session_t *sess, aim_frame_t *fr, ...) {
	GaimConnection *gc = sess->aux_data;
	OscarData *od = gc->proto_data;
	va_list ap;
	fu16_t type;
	fu8_t flags = 0, length = 0;
	char *md5 = NULL;


	va_start(ap, fr);
	type = va_arg(ap, int);

	switch(type) {
		case 0x0000:
		case 0x0001: {
			flags = va_arg(ap, int);
			length = va_arg(ap, int);
			md5 = va_arg(ap, char *);

			if (flags == 0x41) {
				if (!aim_getconn_type(od->sess, AIM_CONN_TYPE_ICON) && !od->iconconnecting) {
					od->iconconnecting = TRUE;
					od->set_icon = TRUE;
					aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_ICON);
				} else {
					struct stat st;
					const char *iconfile = gaim_account_get_buddy_icon(gaim_connection_get_account(gc));
					if (iconfile == NULL) {
						aim_ssi_delicon(od->sess);
					} else if (!stat(iconfile, &st)) {
						char *buf = g_malloc(st.st_size);
						FILE *file = fopen(iconfile, "rb");
						if (file) {
							/* XXX - Use g_file_get_contents()? */
							fread(buf, 1, st.st_size, file);
							fclose(file);
							gaim_debug_info("oscar",
											"Uploading icon to icon server\n");
							aim_bart_upload(od->sess, buf, st.st_size);
						} else
							gaim_debug_error("oscar",
											 "Can't open buddy icon file!\n");
						g_free(buf);
					} else {
						gaim_debug_error("oscar",
										 "Can't stat buddy icon file!\n");
					}
				}
			} else if (flags == 0x81) {
				const char *iconfile = gaim_account_get_buddy_icon(gaim_connection_get_account(gc));
				if (iconfile == NULL)
					aim_ssi_delicon(od->sess);
				else
					aim_ssi_seticon(od->sess, md5, length);
			}
		} break;

		case 0x0002: { /* We just set an "available" message? */
		} break;
	}

	va_end(ap);

	return 0;
}

static void oscar_set_permit_deny(GaimConnection *gc) {
	GaimAccount *account = gaim_connection_get_account(gc);
	OscarData *od = (OscarData *)gc->proto_data;
#ifdef NOSSI
	GSList *list;
	char buf[MAXMSGLEN];
	int at;

	switch(account->perm_deny) {
	case GAIM_PRIVACY_ALLOW_ALL: 
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, gaim_account_get_username(account));
		break;
	case GAIM_PRIVACY_DENY_ALL:
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, gaim_account_get_username(account));
		break;
	case GAIM_PRIVACY_ALLOW_USERS:
		list = account->permit;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, buf);
		break;
	case GAIM_PRIVACY_DENY_USERS:
		list = account->deny;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, buf);
		break;
	default:
		break;
	}
#else
	if (od->sess->ssi.received_data) {
		switch (account->perm_deny) {
			case GAIM_PRIVACY_ALLOW_ALL:
				aim_ssi_setpermdeny(od->sess, 0x01, 0xffffffff);
				break;
			case GAIM_PRIVACY_ALLOW_BUDDYLIST:
				aim_ssi_setpermdeny(od->sess, 0x05, 0xffffffff);
				break;
			case GAIM_PRIVACY_ALLOW_USERS:
				aim_ssi_setpermdeny(od->sess, 0x03, 0xffffffff);
				break;
			case GAIM_PRIVACY_DENY_ALL:
				aim_ssi_setpermdeny(od->sess, 0x02, 0xffffffff);
				break;
			case GAIM_PRIVACY_DENY_USERS:
				aim_ssi_setpermdeny(od->sess, 0x04, 0xffffffff);
				break;
			default:
				aim_ssi_setpermdeny(od->sess, 0x01, 0xffffffff);
				break;
		}
	}
#endif
}

static void oscar_add_permit(GaimConnection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->perm_deny == 3)
		oscar_set_permit_deny(gc);
#else
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to add a permit\n");
	if (od->sess->ssi.received_data)
		aim_ssi_addpermit(od->sess, who);
#endif
}

static void oscar_add_deny(GaimConnection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->perm_deny == 4)
		oscar_set_permit_deny(gc);
#else
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to add a deny\n");
	if (od->sess->ssi.received_data)
		aim_ssi_adddeny(od->sess, who);
#endif
}

static void oscar_rem_permit(GaimConnection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->perm_deny == 3)
		oscar_set_permit_deny(gc);
#else
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to delete a permit\n");
	if (od->sess->ssi.received_data)
		aim_ssi_delpermit(od->sess, who);
#endif
}

static void oscar_rem_deny(GaimConnection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->perm_deny == 4)
		oscar_set_permit_deny(gc);
#else
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to delete a deny\n");
	if (od->sess->ssi.received_data)
		aim_ssi_deldeny(od->sess, who);
#endif
}

static GList *
oscar_status_types(GaimAccount *account)
{
	GList *status_types = NULL;
	GaimStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	/* Oscar-common status types */
	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
									 OSCAR_STATUS_ID_OFFLINE,
									 _("Offline"), FALSE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_ONLINE,
									 OSCAR_STATUS_ID_ONLINE,
									 _("Online"), FALSE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AVAILABLE,
										   OSCAR_STATUS_ID_AVAILABLE,
										   _("Available"), TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AWAY,
										   OSCAR_STATUS_ID_AWAY,
										   _("Away"), TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_HIDDEN,
									 OSCAR_STATUS_ID_INVISIBLE,
									 _("Invisible"), TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	if (aim_sn_is_icq(gaim_account_get_username(account)) == FALSE )
		return status_types;

	/* ICQ-specific status types */

	type = gaim_status_type_new_full(GAIM_STATUS_AVAILABLE,
									 OSCAR_STATUS_ID_FREE4CHAT,
									 _("Free For Chat"), TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_UNAVAILABLE,
									 OSCAR_STATUS_ID_OCCUPIED,
									 _("Occupied"), TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_EXTENDED_AWAY,
									 OSCAR_STATUS_ID_DND,
									 _("Do Not Disturb"), TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_EXTENDED_AWAY,
									 OSCAR_STATUS_ID_NA,
									 _("Not Available"), TRUE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	return status_types;
}

static void oscar_ssi_editcomment(struct name_data *data, const char *text) {
	GaimConnection *gc = data->gc;
	OscarData *od = gc->proto_data;
	GaimBuddy *b;
	GaimGroup *g;

	if (!(b = gaim_find_buddy(gaim_connection_get_account(data->gc), data->name))) {
		oscar_free_name_data(data);
		return;
	}

	if (!(g = gaim_find_buddys_group(b))) {
		oscar_free_name_data(data);
		return;
	}

	aim_ssi_editcomment(od->sess, g->name, data->name, text);

	if (!aim_sncmp(data->name, gc->account->username))
		gaim_check_comment(od, text);

	oscar_free_name_data(data);
}

static void oscar_buddycb_edit_comment(GaimBlistNode *node, gpointer ignore) {

	GaimBuddy *buddy;
	GaimConnection *gc;
	OscarData *od;
	struct name_data *data;
	GaimGroup *g;
	char *comment;
	gchar *comment_utf8;
	gchar *title;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	od = gc->proto_data;

	data = g_new(struct name_data, 1);

	if (!(g = gaim_find_buddys_group(buddy)))
		return;
	comment = aim_ssi_getcomment(od->sess->ssi.local, g->name, buddy->name);
	comment_utf8 = comment ? gaim_utf8_try_convert(comment) : NULL;

	data->gc = gc;
	data->name = g_strdup(buddy->name);
	data->nick = NULL;

	title = g_strdup_printf(_("Buddy Comment for %s"), data->name);
	gaim_request_input(gc, title, _("Buddy Comment:"), NULL,
					   comment_utf8, TRUE, FALSE, NULL,
					   _("OK"), G_CALLBACK(oscar_ssi_editcomment),
					   _("Cancel"), G_CALLBACK(oscar_free_name_data),
					   data);
	g_free(title);

	free(comment);
	g_free(comment_utf8);
}

static GList *oscar_buddy_menu(GaimBuddy *buddy) {

	GaimConnection *gc = gaim_account_get_connection(buddy->account);
	OscarData *od = gc->proto_data;

	GList *m = NULL;
	GaimBlistNodeAction *act;

	act = gaim_blist_node_action_new(_("Edit Buddy Comment"),
			oscar_buddycb_edit_comment, NULL);
	m = g_list_append(m, act);

	if (od->icq) {
#if 0
		act = gaim_blist_node_action_new(_("Get Status Msg"),
				oscar_get_icqstatusmsg, NULL);
		m = g_list_append(m, act);
#endif
	} else {
		aim_userinfo_t *userinfo;
		userinfo = aim_locate_finduserinfo(od->sess, buddy->name);

		if (userinfo && aim_sncmp(gaim_account_get_username(buddy->account), buddy->name) &&
				GAIM_BUDDY_IS_ONLINE(buddy)) {

			if (userinfo->capabilities & AIM_CAPS_DIRECTIM) {
				act = gaim_blist_node_action_new(_("Direct IM"),
						oscar_ask_direct_im, NULL);
				m = g_list_append(m, act);
			}
#if 0
			if (userinfo->capabilities & AIM_CAPS_GETFILE) {
				act = gaim_blist_node_action_new(_("Get File"),
						oscar_ask_getfile, NULL);
				m = g_list_append(m, act);
			}
#endif
		}
	}

	if (od->sess->ssi.received_data) {
		char *gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, buddy->name);
		if (gname && aim_ssi_waitingforauth(od->sess->ssi.local, gname, buddy->name)) {
			act = gaim_blist_node_action_new(_("Re-request Authorization"),
					gaim_auth_sendrequest_menu, NULL);
			m = g_list_append(m, act);
		}
	}

	return m;
}


static GList *oscar_blist_node_menu(GaimBlistNode *node) {
	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return oscar_buddy_menu((GaimBuddy *) node);
	} else {
		return NULL;
	}
}


static void oscar_format_screenname(GaimConnection *gc, const char *nick) {
	OscarData *od = gc->proto_data;
	if (!aim_sncmp(gaim_account_get_username(gaim_connection_get_account(gc)), nick)) {
		if (!aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH)) {
			od->setnick = TRUE;
			od->newsn = g_strdup(nick);
			aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		} else {
			aim_admin_setnick(od->sess, aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH), nick);
		}
	} else {
		gaim_notify_error(gc, NULL, _("The new formatting is invalid."),
						  _("Screen name formatting can change only capitalization and whitespace."));
	}
}

static void oscar_show_format_screenname(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, NULL, _("New screen name formatting:"), NULL,
					   gaim_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(oscar_format_screenname),
					   _("Cancel"), NULL,
					   gc);
}

static void oscar_confirm_account(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	OscarData *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_reqconfirm(od->sess, conn);
	} else {
		od->conf = TRUE;
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_show_email(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	OscarData *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_getinfo(od->sess, conn, 0x11);
	} else {
		od->reqemail = TRUE;
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_change_email(GaimConnection *gc, const char *email)
{
	OscarData *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_setemail(od->sess, conn, email);
	} else {
		od->setemail = TRUE;
		od->email = g_strdup(email);
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_show_change_email(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, NULL, _("Change Address To:"), NULL, NULL,
					   FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(oscar_change_email),
					   _("Cancel"), NULL,
					   gc);
}

static void oscar_show_awaitingauth(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	OscarData *od = gc->proto_data;
	gchar *nombre, *text, *tmp;
	GaimBlistNode *gnode, *cnode, *bnode;
	int num=0;

	text = g_strdup("");

	for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		GaimGroup *group = (GaimGroup *)gnode;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *buddy = (GaimBuddy *)bnode;
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				if (buddy->account == gc->account && aim_ssi_waitingforauth(od->sess->ssi.local, group->name, buddy->name)) {
					if (gaim_buddy_get_alias_only(buddy))
						nombre = g_strdup_printf(" %s (%s)", buddy->name, gaim_buddy_get_alias_only(buddy));
					else
						nombre = g_strdup_printf(" %s", buddy->name);
					tmp = g_strdup_printf("%s%s<br>", text, nombre);
					g_free(text);
					text = tmp;
					g_free(nombre);
					num++;
				}
			}
		}
	}

	if (!num) {
		g_free(text);
		text = g_strdup(_("<i>you are not waiting for authorization</i>"));
	}

	gaim_notify_formatted(gc, NULL, _("You are awaiting authorization from "
						  "the following buddies"),	_("You can re-request "
						  "authorization from these buddies by "
						  "right-clicking on them and selecting "
						  "\"Re-request Authorization.\""), text, NULL, NULL);
	g_free(text);
}

static void search_by_email_cb(GaimConnection *gc, const char *email)
{
	OscarData *od = (OscarData *)gc->proto_data;

	aim_search_address(od->sess, od->conn, email);
}

static void oscar_show_find_email(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, _("Find Buddy by E-mail"),
					   _("Search for a buddy by e-mail address"),
					   _("Type the e-mail address of the buddy you are "
						 "searching for."),
					   NULL, FALSE, FALSE, NULL,
					   _("Search"), G_CALLBACK(search_by_email_cb),
					   _("Cancel"), NULL, gc);
}

static void oscar_show_set_info(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_account_request_change_user_info(gaim_connection_get_account(gc));
}

static void oscar_show_set_info_icqurl(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_notify_uri(gc, "http://www.icq.com/whitepages/user_details.php");
}

static void oscar_change_pass(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_account_request_change_password(gaim_connection_get_account(gc));
}

static void oscar_show_chpassurl(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	OscarData *od = gc->proto_data;
	gchar *substituted = gaim_strreplace(od->sess->authinfo->chpassurl, "%s", gaim_account_get_username(gaim_connection_get_account(gc)));
	gaim_notify_uri(gc, substituted);
	g_free(substituted);
}

static void oscar_show_imforwardingurl(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_notify_uri(gc, "http://mymobile.aol.com/dbreg/register?action=imf&clientID=1");
}

static void oscar_set_icon(GaimConnection *gc, const char *iconfile)
{
	OscarData *od = gc->proto_data;
	aim_session_t *sess = od->sess;
	FILE *file;
	struct stat st;

	if (iconfile == NULL) {
		aim_ssi_delicon(od->sess);
	} else if (!stat(iconfile, &st)) {
		char *buf = g_malloc(st.st_size);
		file = fopen(iconfile, "rb");
		if (file) {
			md5_state_t *state;
			char md5[16];
			/* XXX - Use g_file_get_contents()? */
			int len = fread(buf, 1, st.st_size, file);
			fclose(file);
			state = g_malloc(sizeof(md5_state_t));
			md5_init(state);
			md5_append(state, buf, len);
			md5_finish(state, md5);
			g_free(state);
			aim_ssi_seticon(sess, md5, 16);
		} else
			gaim_debug_error("oscar",
				   "Can't open buddy icon file!\n");
		g_free(buf);
	} else
		gaim_debug_error("oscar",
			   "Can't stat buddy icon file!\n");
}


static GList *oscar_actions(GaimPlugin *plugin, gpointer context)
{
	GaimConnection *gc = (GaimConnection *) context;
	OscarData *od = gc->proto_data;
	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Set User Info..."),
			oscar_show_set_info);
	m = g_list_append(m, act);

	if (od->icq) {
		act = gaim_plugin_action_new(_("Set User Info (URL)..."),
				oscar_show_set_info_icqurl);
		m = g_list_append(m, act);
	}

	act = gaim_plugin_action_new(_("Change Password..."),
			oscar_change_pass);
	m = g_list_append(m, act);

	if (od->sess->authinfo->chpassurl) {
		act = gaim_plugin_action_new(_("Change Password (URL)"),
				oscar_show_chpassurl);
		m = g_list_append(m, act);

		act = gaim_plugin_action_new(_("Configure IM Forwarding (URL)"),
				oscar_show_imforwardingurl);
		m = g_list_append(m, act);
	}

	if (!od->icq) {
		/* AIM actions */
		m = g_list_append(m, NULL);

		act = gaim_plugin_action_new(_("Format Screen Name..."),
				oscar_show_format_screenname);
		m = g_list_append(m, act);

		act = gaim_plugin_action_new(_("Confirm Account"),
				oscar_confirm_account);
		m = g_list_append(m, act);

		act = gaim_plugin_action_new(_("Display Currently Registered Address"),
				oscar_show_email);
		m = g_list_append(m, act);

		act = gaim_plugin_action_new(_("Change Currently Registered Address..."),
				oscar_show_change_email);
		m = g_list_append(m, act);
	}

	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Show Buddies Awaiting Authorization"),
			oscar_show_awaitingauth);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Search for Buddy by Email..."),
			oscar_show_find_email);
	m = g_list_append(m, act);

#if 0
	act = gaim_plugin_action_new(_("Search for Buddy by Information"),
			show_find_info);
	m = g_list_append(m, act);
#endif

	return m;
}

static void oscar_change_passwd(GaimConnection *gc, const char *old, const char *new)
{
	OscarData *od = gc->proto_data;

	if (od->icq) {
		aim_icq_changepasswd(od->sess, new);
	} else {
		aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);
		if (conn) {
			aim_admin_changepasswd(od->sess, conn, new, old);
		} else {
			od->chpass = TRUE;
			od->oldp = g_strdup(old);
			od->newp = g_strdup(new);
			aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		}
	}
}

static void oscar_convo_closed(GaimConnection *gc, const char *who)
{
	OscarData *od = gc->proto_data;
	struct oscar_direct_im *dim = oscar_direct_im_find(od, who);

	if (!dim)
		return;

	oscar_direct_im_destroy(od, dim);
}

static void
recent_buddies_cb(const char *name, GaimPrefType type, gpointer value, gpointer data)
{
	GaimConnection *gc = data;
	OscarData *od = gc->proto_data;
	aim_session_t *sess = od->sess;
	fu32_t presence;

	presence = aim_ssi_getpresence(sess->ssi.local);

	if (value) {
		/* Based on the packet capture I thought it was the first one */
		/* Stu thinks it's the second one. */
		/* presence |= 0x00400000; */
		presence &= ~0x00020000;
		aim_ssi_setpresence(sess, presence);
	} else {
		/* presence &= ~0x00400000; */
		presence |= 0x00020000;
		aim_ssi_setpresence(sess, presence);
	}
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_name_and_label("/plugins/prpl/oscar/recent_buddies", _("Use recent buddies group"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label("/plugins/prpl/oscar/show_idle", _("Show how long you have been idle"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_IM_IMAGE,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"jpeg,gif,bmp,ico", 48, 48, 50, 50, GAIM_ICON_SCALE_DISPLAY},	/* icon_spec */
	oscar_list_icon,		/* list_icon */
	oscar_list_emblems,		/* list_emblems */
	oscar_status_text,		/* status_text */
	oscar_tooltip_text,		/* tooltip_text */
	oscar_status_types,		/* status_types */
	oscar_blist_node_menu,	/* blist_node_menu */
	oscar_chat_info,		/* chat_info */
	oscar_chat_info_defaults, /* chat_info_defaults */
	oscar_login,			/* login */
	oscar_close,			/* close */
	oscar_send_im,			/* send_im */
	oscar_set_info,			/* set_info */
	oscar_send_typing,		/* send_typing */
	oscar_get_info,			/* get_info */
	oscar_set_status,		/* set_status */
	oscar_set_idle,			/* set_idle */
	oscar_change_passwd,	/* change_passwd */
	oscar_add_buddy,		/* add_buddy */
	oscar_add_buddies,		/* add_buddies */
	oscar_remove_buddy,		/* remove_buddy */
	oscar_remove_buddies,	/* remove_buddies */
	oscar_add_permit,		/* add_permit */
	oscar_add_deny,			/* add_deny */
	oscar_rem_permit,		/* rem_permit */
	oscar_rem_deny,			/* rem_deny */
	oscar_set_permit_deny,	/* set_permit_deny */
	oscar_warn,				/* warn */
	oscar_join_chat,		/* join_chat */
	NULL,					/* reject_chat */
	oscar_get_chat_name,	/* get_chat_name */
	oscar_chat_invite,		/* chat_invite */
	oscar_chat_leave,		/* chat_leave */
	NULL,					/* chat_whisper */
	oscar_send_chat,		/* chat_send */
	oscar_keepalive,		/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
#ifndef NOSSI
	oscar_alias_buddy,		/* alias_buddy */
	oscar_move_buddy,		/* group_buddy */
	oscar_rename_group,		/* rename_group */
#else
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
#endif
	NULL,					/* buddy_free */
	oscar_convo_closed,		/* convo_closed */
	NULL,					/* normalize */
	oscar_set_icon,			/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	oscar_can_receive_file,	/* can_receive_file */
	oscar_send_file			/* send_file */
};

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-oscar",                                     /**< id             */
	"AIM/ICQ",                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("AIM/ICQ Protocol Plugin"),
	                                                  /**  description    */
	N_("AIM/ICQ Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	oscar_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Auth host"), "server", FAIM_LOGIN_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Auth port"), "port", FAIM_LOGIN_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_string_new(_("Encoding"), "encoding", OSCAR_DEFAULT_CUSTOM_ENCODING);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;

	gaim_prefs_add_none("/plugins/prpl/oscar");
	gaim_prefs_add_bool("/plugins/prpl/oscar/recent_buddies", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/oscar/show_idle", FALSE);
}

GAIM_INIT_PLUGIN(oscar, init_plugin, info);
