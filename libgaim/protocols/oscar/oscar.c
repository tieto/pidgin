/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Some code copyright (C) 1999-2001, Eric Warmenhoven
 * Some code copyright (C) 2001-2003, Sean Egan
 * Some code copyright (C) 2001-2005, Mark Doliner <thekingant@users.sourceforge.net>
 * Some code copyright (C) 2005, Jonathan Clark <ardentlygnarly@users.sourceforge.net>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
#include "cipher.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "imgstore.h"
#include "network.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "util.h"
#include "version.h"

#include "oscarcommon.h"
#include "oscar.h"
#include "peer.h"

#define OSCAR_STATUS_ID_INVISIBLE   "invisible"
#define OSCAR_STATUS_ID_OFFLINE     "offline"
#define OSCAR_STATUS_ID_AVAILABLE   "available"
#define OSCAR_STATUS_ID_AWAY        "away"
#define OSCAR_STATUS_ID_DND         "dnd"
#define OSCAR_STATUS_ID_NA          "na"
#define OSCAR_STATUS_ID_OCCUPIED    "occupied"
#define OSCAR_STATUS_ID_FREE4CHAT   "free4chat"
#define OSCAR_STATUS_ID_CUSTOM      "custom"

#define AIMHASHDATA "http://gaim.sourceforge.net/aim_data.php3"

#define OSCAR_CONNECT_STEPS 6

static OscarCapability gaim_caps = OSCAR_CAPABILITY_CHAT | OSCAR_CAPABILITY_BUDDYICON | OSCAR_CAPABILITY_DIRECTIM | OSCAR_CAPABILITY_SENDFILE | OSCAR_CAPABILITY_UNICODE | OSCAR_CAPABILITY_INTEROPERATE | OSCAR_CAPABILITY_ICHAT;

static guint8 features_aim[] = {0x01, 0x01, 0x01, 0x02};
static guint8 features_icq[] = {0x01, 0x06};
static guint8 features_icq_offline[] = {0x01};
static guint8 ck[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct create_room {
	char *name;
	int exchange;
};

struct oscar_ask_directim_data
{
	OscarData *od;
	char *who;
};

/*
 * Various PRPL-specific buddy info that we want to keep track of
 * Some other info is maintained by locate.c, and I'd like to move
 * the rest of this to libfaim, mostly im.c
 *
 * TODO: More of this should use the status API.
 */
struct buddyinfo {
	gboolean typingnot;
	guint32 ipaddr;

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
static int gaim_parse_auth_resp  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_login      (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_auth_securid_request(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_handle_redirect  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_info_change      (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_account_confirm  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_oncoming   (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_offgoing   (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_incoming_im(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_misses     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_clientauto (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_userinfo   (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_got_infoblock    (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_motd       (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_chatnav_info     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_conv_chat_join        (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_conv_chat_leave       (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_conv_chat_info_update (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_conv_chat_incoming_msg(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_email_parseupdate(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_icon_error       (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_icon_parseicon   (OscarData *, FlapConnection *, FlapFrame *, ...);
static int oscar_icon_req        (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_msgack     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_ratechange (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_evilnotify (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_searcherror(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_searchreply(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_bosrights        (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_connerr          (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_msgerr     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_mtn        (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_locaterights(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_buddyrights(OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_locerr     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_icbm_param_info  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_parse_genericerr (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_memrequest       (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_selfinfo         (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_offlinemsg       (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_offlinemsgdone   (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_icqalias         (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_icqinfo          (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_popup            (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_parseerr     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_parserights  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_parselist    (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_parseack     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_parseadd     (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_authgiven    (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_authrequest  (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_authreply    (OscarData *, FlapConnection *, FlapFrame *, ...);
static int gaim_ssi_gotadded     (OscarData *, FlapConnection *, FlapFrame *, ...);

static gboolean gaim_icon_timerfunc(gpointer data);

static void recent_buddies_cb(const char *name, GaimPrefType type, gconstpointer value, gpointer data);
void oscar_set_info(GaimConnection *gc, const char *info);
static void oscar_set_info_and_status(GaimAccount *account, gboolean setinfo, const char *rawinfo, gboolean setstatus, GaimStatus *status);
static void oscar_set_extendedstatus(GaimConnection *gc);
static gboolean gaim_ssi_rerequestdata(gpointer data);

static void oscar_free_name_data(struct name_data *data) {
	g_free(data->name);
	g_free(data->nick);
	g_free(data);
}

#ifdef _WIN32
const char *oscar_get_locale_charset(void) {
	static const char *charset = NULL;
	if (charset == NULL)
		g_get_charset(&charset);
	return charset;
}
#endif

/**
 * Determine how we can send this message.  Per the warnings elsewhere
 * in this file, these little checks determine the simplest encoding
 * we can use for a given message send using it.
 */
static guint32
oscar_charset_check(const char *utf8)
{
	int i = 0;
	int charset = AIM_CHARSET_ASCII;

	/*
	 * Can we get away with using our custom encoding?
	 */
	while (utf8[i])
	{
		if ((unsigned char)utf8[i] > 0x7f) {
			/* not ASCII! */
			charset = AIM_CHARSET_CUSTOM;
			break;
		}
		i++;
	}

	/*
	 * Must we send this message as UNICODE (in the UCS-2BE encoding)?
	 */
	while (utf8[i])
	{
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

/**
 * Take a string of the form charset="bleh" where bleh is
 * one of us-ascii, utf-8, iso-8859-1, or unicode-2-0, and
 * return a newly allocated string containing bleh.
 */
gchar *
oscar_encoding_extract(const char *encoding)
{
	gchar *ret = NULL;
	char *begin, *end;

	g_return_val_if_fail(encoding != NULL, NULL);

	/* Make sure encoding begins with charset= */
	if (strncmp(encoding, "text/aolrtf; charset=", 21) &&
		strncmp(encoding, "text/x-aolrtf; charset=", 23))
	{
		return NULL;
	}

	begin = strchr(encoding, '"');
	end = strrchr(encoding, '"');

	if ((begin == NULL) || (end == NULL) || (begin >= end))
		return NULL;

	ret = g_strndup(begin+1, (end-1) - begin);

	return ret;
}

gchar *
oscar_encoding_to_utf8(const char *encoding, const char *text, int textlen)
{
	gchar *utf8 = NULL;

	if ((encoding == NULL) || encoding[0] == '\0') {
		gaim_debug_info("oscar", "Empty encoding, assuming UTF-8\n");
	} else if (!strcasecmp(encoding, "iso-8859-1")) {
		utf8 = g_convert(text, textlen, "UTF-8", "iso-8859-1", NULL, NULL, NULL);
	} else if (!strcasecmp(encoding, "ISO-8859-1-Windows-3.1-Latin-1") ||
	           !strcasecmp(encoding, "us-ascii"))
	{
		utf8 = g_convert(text, textlen, "UTF-8", "Windows-1252", NULL, NULL, NULL);
	} else if (!strcasecmp(encoding, "unicode-2-0")) {
		utf8 = g_convert(text, textlen, "UTF-8", "UCS-2BE", NULL, NULL, NULL);
	} else if (strcasecmp(encoding, "utf-8")) {
		gaim_debug_warning("oscar", "Unrecognized character encoding \"%s\", "
						   "attempting to convert to UTF-8 anyway\n", encoding);
		utf8 = g_convert(text, textlen, "UTF-8", encoding, NULL, NULL, NULL);
	}

	/*
	 * If utf8 is still NULL then either the encoding is utf-8 or
	 * we have been unable to convert the text to utf-8 from the encoding
	 * that was specified.  So we check if the text is valid utf-8 then
	 * just copy it.
	 */
	if (utf8 == NULL) {
		if (textlen != 0 && *text != '\0'
				&& !g_utf8_validate(text, textlen, NULL))
			utf8 = g_strdup(_("(There was an error receiving this message.  The buddy you are speaking with is probably using a different encoding than expected.  If you know what encoding he is using, you can specify it in the advanced account options for your AIM/ICQ account.)"));
		else
			utf8 = g_strndup(text, textlen);
	}

	return utf8;
}

static gchar *
oscar_utf8_try_convert(GaimAccount *account, const gchar *msg)
{
	const char *charset = NULL;
	char *ret = NULL;

	if(aim_sn_is_icq(gaim_account_get_username(account)))
		charset = gaim_account_get_string(account, "encoding", NULL);

	if(charset && *charset)
		ret = g_convert(msg, -1, "UTF-8", charset, NULL, NULL, NULL);

	if(!ret)
		ret = gaim_utf8_try_convert(msg);

	return ret;
}

static gchar *
gaim_plugin_oscar_convert_to_utf8(const gchar *data, gsize datalen, const char *charsetstr, gboolean fallback)
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
			gaim_debug_warning("oscar", "Conversion from %s failed: %s.\n",
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

/**
 * This attemps to decode an incoming IM into a UTF8 string.
 *
 * We try decoding using two different character sets.  The charset
 * specified in the IM determines the order in which we attempt to
 * decode.  We do this because there are lots of broken ICQ clients
 * that don't correctly send non-ASCII messages.  And if Gaim isn't
 * able to deal with that crap, then people complain like banshees.
 * charsetstr1 is always set to what the correct encoding should be.
 */
gchar *
gaim_plugin_oscar_decode_im_part(GaimAccount *account, const char *sourcesn, guint16 charset, guint16 charsubset, const gchar *data, gsize datalen)
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
		if ((sourcesn != NULL) && isdigit(sourcesn[0]))
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
	if (ret == NULL) {
		char *str, *salvage, *tmp;

		str = g_malloc(datalen + 1);
		strncpy(str, data, datalen);
		str[datalen] = '\0';
		salvage = gaim_utf8_salvage(str);
		tmp = g_strdup_printf(_("(There was an error receiving this message.  Either you and %s have different encodings selected, or %s has a buggy client.)"),
					  sourcesn, sourcesn);
		ret = g_strdup_printf("%s %s", salvage, tmp);
		g_free(tmp);
		g_free(str);
		g_free(salvage);
	}

	return ret;
}

/**
 * Figure out what encoding to use when sending a given outgoing message.
 */
static void
gaim_plugin_oscar_convert_to_best_encoding(GaimConnection *gc,
				const char *destsn, const gchar *from,
				gchar **msg, int *msglen_int,
				guint16 *charset, guint16 *charsubset)
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
	 * If we're sending to an ICQ user, and they are in our
	 * buddy list, and they are advertising the Unicode
	 * capability, and they are online, then attempt to send
	 * as UCS-2BE.
	 */
	if ((destsn != NULL) && aim_sn_is_icq(destsn))
		userinfo = aim_locate_finduserinfo(od, destsn);

	if ((userinfo != NULL) && (userinfo->capabilities & OSCAR_CAPABILITY_UNICODE))
	{
		GaimBuddy *b;
		b = gaim_find_buddy(account, destsn);
		if ((b != NULL) && (GAIM_BUDDY_IS_ONLINE(b)))
		{
			*msg = g_convert(from, strlen(from), "UCS-2BE", "UTF-8", NULL, &msglen, NULL);
			if (*msg != NULL)
			{
				*charset = AIM_CHARSET_UNICODE;
				*charsubset = 0x0000;
				*msglen_int = msglen;
				return;
			}
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

/**
 * Looks for %n, %d, or %t in a string, and replaces them with the
 * specified name, date, and time, respectively.
 *
 * @param str  The string that may contain the special variables.
 * @param name The sender name.
 *
 * @return A newly allocated string where the special variables are
 *         expanded.  This should be g_free'd by the caller.
 */
static gchar *
gaim_str_sub_away_formatters(const char *str, const char *name)
{
	char *c;
	GString *cpy;
	time_t t;
	struct tm *tme;

	g_return_val_if_fail(str  != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Create an empty GString that is hopefully big enough for most messages */
	cpy = g_string_sized_new(1024);

	t = time(NULL);
	tme = localtime(&t);

	c = (char *)str;
	while (*c) {
		switch (*c) {
		case '%':
			if (*(c + 1)) {
				switch (*(c + 1)) {
				case 'n':
					/* append name */
					g_string_append(cpy, name);
					c++;
					break;
				case 'd':
					/* append date */
					g_string_append(cpy, gaim_date_format_short(tme));
					c++;
					break;
				case 't':
					/* append time */
					g_string_append(cpy, gaim_time_format(tme));
					c++;
					break;
				default:
					g_string_append_c(cpy, *c);
				}
			} else {
				g_string_append_c(cpy, *c);
			}
			break;
		default:
			g_string_append_c(cpy, *c);
		}
		c++;
	}

	return g_string_free(cpy, FALSE);
}

static gchar *oscar_caps_to_string(OscarCapability caps)
{
	GString *str;
	const gchar *tmp;
	guint bit = 1;

	str = g_string_new("");

	if (!caps) {
		return NULL;
	} else while (bit <= OSCAR_CAPABILITY_LAST) {
		if (bit & caps) {
			switch (bit) {
			case OSCAR_CAPABILITY_BUDDYICON:
				tmp = _("Buddy Icon");
				break;
			case OSCAR_CAPABILITY_TALK:
				tmp = _("Voice");
				break;
			case OSCAR_CAPABILITY_DIRECTIM:
				tmp = _("AIM Direct IM");
				break;
			case OSCAR_CAPABILITY_CHAT:
				tmp = _("Chat");
				break;
			case OSCAR_CAPABILITY_GETFILE:
				tmp = _("Get File");
				break;
			case OSCAR_CAPABILITY_SENDFILE:
				tmp = _("Send File");
				break;
			case OSCAR_CAPABILITY_GAMES:
			case OSCAR_CAPABILITY_GAMES2:
				tmp = _("Games");
				break;
			case OSCAR_CAPABILITY_ADDINS:
				tmp = _("Add-Ins");
				break;
			case OSCAR_CAPABILITY_SENDBUDDYLIST:
				tmp = _("Send Buddy List");
				break;
			case OSCAR_CAPABILITY_ICQ_DIRECT:
				tmp = _("ICQ Direct Connect");
				break;
			case OSCAR_CAPABILITY_APINFO:
				tmp = _("AP User");
				break;
			case OSCAR_CAPABILITY_ICQRTF:
				tmp = _("ICQ RTF");
				break;
			case OSCAR_CAPABILITY_EMPTY:
				tmp = _("Nihilist");
				break;
			case OSCAR_CAPABILITY_ICQSERVERRELAY:
				tmp = _("ICQ Server Relay");
				break;
			case OSCAR_CAPABILITY_UNICODEOLD:
				tmp = _("Old ICQ UTF8");
				break;
			case OSCAR_CAPABILITY_TRILLIANCRYPT:
				tmp = _("Trillian Encryption");
				break;
			case OSCAR_CAPABILITY_UNICODE:
				tmp = _("ICQ UTF8");
				break;
			case OSCAR_CAPABILITY_HIPTOP:
				tmp = _("Hiptop");
				break;
			case OSCAR_CAPABILITY_SECUREIM:
				tmp = _("Security Enabled");
				break;
			case OSCAR_CAPABILITY_VIDEO:
				tmp = _("Video Chat");
				break;
			/* Not actually sure about this one... WinAIM doesn't show anything */
			case OSCAR_CAPABILITY_ICHATAV:
				tmp = _("iChat AV");
				break;
			case OSCAR_CAPABILITY_LIVEVIDEO:
				tmp = _("Live Video");
				break;
			case OSCAR_CAPABILITY_CAMERA:
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
oscar_user_info_add_pair(GaimNotifyUserInfo *user_info, const char *name, const char *value)
{
	if (value && value[0]) {
		gaim_notify_user_info_add_pair(user_info, name, value);
	}
}

static void
oscar_user_info_convert_and_add_pair(GaimAccount *account, GaimNotifyUserInfo *user_info,
									 const char *name, const char *value)
{
	gchar *utf8;
	
	if (value && value[0] && (utf8 = oscar_utf8_try_convert(account, value))) {
		gaim_notify_user_info_add_pair(user_info, name, utf8);
		g_free(utf8);
	}
}

static void
oscar_string_convert_and_append(GaimAccount *account, GString *str, const char *newline,
					const char *name, const char *value)
{
	gchar *utf8;

	if (value && value[0] && (utf8 = oscar_utf8_try_convert(account, value))) {
		g_string_append_printf(str, "%s<b>%s:</b> %s", newline, name, utf8);
		g_free(utf8);
	}
}

static void
oscar_user_info_convert_and_add(GaimAccount *account, GaimNotifyUserInfo *user_info,
								const char *name, const char *value)
{
	gchar *utf8;
	
	if (value && value[0] && (utf8 = oscar_utf8_try_convert(account, value))) {
		gaim_notify_user_info_add_pair(user_info, name, value);
		g_free(utf8);
	}
}

static void oscar_string_append_info(GaimConnection *gc, GaimNotifyUserInfo *user_info, GaimBuddy *b, aim_userinfo_t *userinfo)
{
	OscarData *od;
	GaimAccount *account;
	GaimPresence *presence = NULL;
	GaimStatus *status = NULL;
	GaimGroup *g = NULL;
	struct buddyinfo *bi = NULL;
	char *tmp;

	od = gc->proto_data;
	account = gaim_connection_get_account(gc);

	if ((user_info == NULL) || ((b == NULL) && (userinfo == NULL)))
		return;

	if (userinfo == NULL)
		userinfo = aim_locate_finduserinfo(od, b->name);

	if (b == NULL)
		b = gaim_find_buddy(account, userinfo->sn);

	if (b != NULL) {
		g = gaim_buddy_get_group(b);
		presence = gaim_buddy_get_presence(b);
		status = gaim_presence_get_active_status(presence);
	}

	if (userinfo != NULL)
		bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, userinfo->sn));

	if (b != NULL) {
		if (gaim_presence_is_online(presence)) {
			if (aim_sn_is_icq(b->name)) {
				GaimStatus *status = gaim_presence_get_active_status(presence);
				oscar_user_info_add_pair(user_info, _("Status"),	gaim_status_get_name(status));
			}
		} else {
			tmp = aim_ssi_itemlist_findparentname(od->ssi.local, b->name);
			if (aim_ssi_waitingforauth(od->ssi.local, tmp, b->name))
				oscar_user_info_add_pair(user_info, _("Status"),	_("Not Authorized"));
			else
				oscar_user_info_add_pair(user_info, _("Status"),	_("Offline"));
		}
	}

	if ((bi != NULL) && (bi->ipaddr != 0)) {
		tmp =  g_strdup_printf("%hhu.%hhu.%hhu.%hhu",
						(bi->ipaddr & 0xff000000) >> 24,
						(bi->ipaddr & 0x00ff0000) >> 16,
						(bi->ipaddr & 0x0000ff00) >> 8,
						(bi->ipaddr & 0x000000ff));
		oscar_user_info_add_pair(user_info, _("IP Address"), tmp);
		g_free(tmp);
	}


	if ((userinfo != NULL) && (userinfo->warnlevel != 0)) {
		tmp = g_strdup_printf("%d", (int)(userinfo->warnlevel/10.0 + .5));
		oscar_user_info_add_pair(user_info, _("Warning Level"), tmp);
		g_free(tmp);
	}

	if ((b != NULL) && (b->name != NULL) && (g != NULL) && (g->name != NULL)) {
		tmp = aim_ssi_getcomment(od->ssi.local, g->name, b->name);
		if (tmp != NULL) {
			char *tmp2 = g_markup_escape_text(tmp, strlen(tmp));
			g_free(tmp);

			oscar_user_info_convert_and_add_pair(account, user_info, _("Buddy Comment"), tmp2);
			g_free(tmp2);
		}
	}
}

static char *extract_name(const char *name) {
	char *tmp, *x;
	int i, j;

	if (!name)
		return NULL;

	x = strchr(name, '-');
	if (!x)
		return NULL;

	x = strchr(x + 1, '-');
	if (!x)
		return NULL;

	tmp = g_strdup(++x);

	for (i = 0, j = 0; x[i]; i++) {
		char hex[3];
		if (x[i] != '%') {
			tmp[j++] = x[i];
			continue;
		}
		strncpy(hex, x + ++i, 2);
		hex[2] = 0;
		i++;
		tmp[j++] = strtol(hex, NULL, 16);
	}

	tmp[j] = 0;
	return tmp;
}

static struct chat_connection *
find_oscar_chat(GaimConnection *gc, int id)
{
	OscarData *od = (OscarData *)gc->proto_data;
	GSList *cur;
	struct chat_connection *cc;

	for (cur = od->oscar_chats; cur != NULL; cur = cur->next)
	{
		cc = (struct chat_connection *)cur->data;
		if (cc->id == id)
			return cc;
	}

	return NULL;
}

static struct chat_connection *
find_oscar_chat_by_conn(GaimConnection *gc, FlapConnection *conn)
{
	OscarData *od = (OscarData *)gc->proto_data;
	GSList *cur;
	struct chat_connection *cc;

	for (cur = od->oscar_chats; cur != NULL; cur = cur->next)
	{
		cc = (struct chat_connection *)cur->data;
		if (cc->conn == conn)
			return cc;
	}

	return NULL;
}

static struct chat_connection *
find_oscar_chat_by_conv(GaimConnection *gc, GaimConversation *conv)
{
	OscarData *od = (OscarData *)gc->proto_data;
	GSList *cur;
	struct chat_connection *cc;

	for (cur = od->oscar_chats; cur != NULL; cur = cur->next)
	{
		cc = (struct chat_connection *)cur->data;
		if (cc->conv == conv)
			return cc;
	}

	return NULL;
}

void
oscar_chat_destroy(struct chat_connection *cc)
{
	g_free(cc->name);
	g_free(cc->show);
	g_free(cc);
}

static void
oscar_chat_kill(GaimConnection *gc, struct chat_connection *cc)
{
	OscarData *od = (OscarData *)gc->proto_data;

	/* Notify the conversation window that we've left the chat */
	serv_got_chat_left(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(cc->conv)));

	/* Destroy the chat_connection */
	od->oscar_chats = g_slist_remove(od->oscar_chats, cc);
	flap_connection_schedule_destroy(cc->conn, OSCAR_DISCONNECT_DONE, NULL);
	oscar_chat_destroy(cc);
}

/**
 * This is the callback function anytime gaim_proxy_connect()
 * establishes a new TCP connection with an oscar host.  Depending
 * on the type of host, we do a few different things here.
 */
static void
connection_established_cb(gpointer data, gint source, const gchar *error_message)
{
	GaimConnection *gc;
	OscarData *od;
	GaimAccount *account;
	FlapConnection *conn;

	conn = data;
	od = conn->od;
	gc = od->gc;
	account = gaim_connection_get_account(gc);

	conn->connect_data = NULL;
	conn->fd = source;

	if (source < 0)
	{
		gaim_debug_error("oscar", "unable to connect FLAP server "
				"of type 0x%04hx\n", conn->type);
		if (conn->type == SNAC_FAMILY_AUTH)
		{
			gchar *msg;
			msg = g_strdup_printf(_("Could not connect to authentication server:\n%s"),
					error_message);
			gaim_connection_error(gc, msg);
			g_free(msg);
		}
		else if (conn->type == SNAC_FAMILY_LOCATE)
		{
			gchar *msg;
			msg = g_strdup_printf(_("Could not connect to BOS server:\n%s"),
					error_message);
			gaim_connection_error(gc, msg);
			g_free(msg);
		}
		else
		{
			/* Maybe we should call this for BOS connections, too? */
			flap_connection_schedule_destroy(conn,
					OSCAR_DISCONNECT_COULD_NOT_CONNECT, error_message);
		}
		return;
	}

	gaim_debug_info("oscar", "connected to FLAP server of type 0x%04hx\n",
			conn->type);
	conn->watcher_incoming = gaim_input_add(conn->fd,
			GAIM_INPUT_READ, flap_connection_recv_cb, conn);
	if (conn->cookie == NULL)
	{
		if (!aim_sn_is_icq(gaim_account_get_username(account)))
			/*
			 * We don't send this when authenticating an ICQ account
			 * because for some reason ICQ is still using the
			 * assy/insecure authentication procedure.
			 */
			flap_connection_send_version(od, conn);
	}
	else
	{
		flap_connection_send_version_with_cookie(od, conn,
				conn->cookielen, conn->cookie);
		g_free(conn->cookie);
		conn->cookie = NULL;
	}

	if (conn->type == SNAC_FAMILY_AUTH)
	{
		aim_request_login(od, conn, gaim_account_get_username(account));
		gaim_debug_info("oscar", "Screen name sent, waiting for response\n");
		gaim_connection_update_progress(gc, _("Screen name sent"), 1, OSCAR_CONNECT_STEPS);
		ck[1] = 0x65;
	}
	else if (conn->type == SNAC_FAMILY_LOCATE)
	{
		gaim_connection_update_progress(gc, _("Connection established, cookie sent"), 4, OSCAR_CONNECT_STEPS);
		ck[4] = 0x61;
	}
	else if (conn->type == SNAC_FAMILY_CHAT)
	{
		od->oscar_chats = g_slist_prepend(od->oscar_chats, conn->new_conn_data);
		conn->new_conn_data = NULL;
	}
}

static void
flap_connection_established_bos(OscarData *od, FlapConnection *conn)
{
	GaimConnection *gc = od->gc;

	aim_srv_reqpersonalinfo(od, conn);

	gaim_debug_info("oscar", "ssi: requesting rights and list\n");
	aim_ssi_reqrights(od);
	aim_ssi_reqdata(od);
	if (od->getblisttimer > 0)
		gaim_timeout_remove(od->getblisttimer);
	od->getblisttimer = gaim_timeout_add(30000, gaim_ssi_rerequestdata, od);

	aim_locate_reqrights(od);
	aim_buddylist_reqrights(od, conn);
	aim_im_reqparams(od);
	aim_bos_reqrights(od, conn); /* TODO: Don't call this with ssi */

	gaim_connection_update_progress(gc, _("Finalizing connection"), 5, OSCAR_CONNECT_STEPS);
}

static void
flap_connection_established_admin(OscarData *od, FlapConnection *conn)
{
	aim_clientready(od, conn);
	gaim_debug_info("oscar", "connected to admin\n");

	if (od->chpass) {
		gaim_debug_info("oscar", "changing password\n");
		aim_admin_changepasswd(od, conn, od->newp, od->oldp);
		g_free(od->oldp);
		od->oldp = NULL;
		g_free(od->newp);
		od->newp = NULL;
		od->chpass = FALSE;
	}
	if (od->setnick) {
		gaim_debug_info("oscar", "formatting screen name\n");
		aim_admin_setnick(od, conn, od->newsn);
		g_free(od->newsn);
		od->newsn = NULL;
		od->setnick = FALSE;
	}
	if (od->conf) {
		gaim_debug_info("oscar", "confirming account\n");
		aim_admin_reqconfirm(od, conn);
		od->conf = FALSE;
	}
	if (od->reqemail) {
		gaim_debug_info("oscar", "requesting e-mail address\n");
		aim_admin_getinfo(od, conn, 0x0011);
		od->reqemail = FALSE;
	}
	if (od->setemail) {
		gaim_debug_info("oscar", "setting e-mail address\n");
		aim_admin_setemail(od, conn, od->email);
		g_free(od->email);
		od->email = NULL;
		od->setemail = FALSE;
	}
}

static void
flap_connection_established_chat(OscarData *od, FlapConnection *conn)
{
	GaimConnection *gc = od->gc;
	struct chat_connection *chatcon;
	static int id = 1;

	aim_clientready(od, conn);

	chatcon = find_oscar_chat_by_conn(gc, conn);
	chatcon->id = id;
	chatcon->conv = serv_got_joined_chat(gc, id++, chatcon->show);
}

static void
flap_connection_established_chatnav(OscarData *od, FlapConnection *conn)
{
	aim_clientready(od, conn);
	aim_chatnav_reqrights(od, conn);
}

static void
flap_connection_established_alert(OscarData *od, FlapConnection *conn)
{
	aim_email_sendcookies(od);
	aim_email_activate(od);
	aim_clientready(od, conn);
}

static void
flap_connection_established_bart(OscarData *od, FlapConnection *conn)
{
	GaimConnection *gc = od->gc;

	aim_clientready(od, conn);

	od->iconconnecting = FALSE;

	if (od->icontimer == 0)
		od->icontimer = gaim_timeout_add(100, gaim_icon_timerfunc, gc);
}

static int
flap_connection_established(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	gaim_debug_info("oscar", "FLAP connection of type 0x%04hx is "
			"now fully connected\n", conn->type);
	if (conn->type == SNAC_FAMILY_LOCATE)
		flap_connection_established_bos(od, conn);
	else if (conn->type == SNAC_FAMILY_ADMIN)
		flap_connection_established_admin(od, conn);
	else if (conn->type == SNAC_FAMILY_CHAT)
		flap_connection_established_chat(od, conn);
	else if (conn->type == SNAC_FAMILY_CHATNAV)
		flap_connection_established_chatnav(od, conn);
	else if (conn->type == SNAC_FAMILY_ALERT)
		flap_connection_established_alert(od, conn);
	else if (conn->type == SNAC_FAMILY_BART)
		flap_connection_established_bart(od, conn);

	return 1;
}

void
oscar_login(GaimAccount *account)
{
	GaimConnection *gc;
	OscarData *od;
	FlapConnection *newconn;

	gc = gaim_account_get_connection(account);
	od = gc->proto_data = oscar_data_new();
	od->gc = gc;

	oscar_data_addhandler(od, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
	oscar_data_addhandler(od, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, flap_connection_established, 0);

	oscar_data_addhandler(od, SNAC_FAMILY_ADMIN, 0x0003, gaim_info_change, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ADMIN, 0x0005, gaim_info_change, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ADMIN, 0x0007, gaim_account_confirm, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ALERT, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ALERT, SNAC_SUBTYPE_ALERT_MAILSTATUS, gaim_email_parseupdate, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_AUTH, 0x0003, gaim_parse_auth_resp, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_AUTH, 0x0007, gaim_parse_login, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_AUTH, SNAC_SUBTYPE_AUTH_SECURID_REQUEST, gaim_parse_auth_securid_request, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BART, SNAC_SUBTYPE_BART_ERROR, gaim_icon_error, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BART, SNAC_SUBTYPE_BART_RESPONSE, gaim_icon_parseicon, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BOS, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BOS, 0x0003, gaim_bosrights, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BUDDY, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BUDDY, SNAC_SUBTYPE_BUDDY_RIGHTSINFO, gaim_parse_buddyrights, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BUDDY, SNAC_SUBTYPE_BUDDY_ONCOMING, gaim_parse_oncoming, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_BUDDY, SNAC_SUBTYPE_BUDDY_OFFGOING, gaim_parse_offgoing, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHAT, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHAT, SNAC_SUBTYPE_CHAT_USERJOIN, gaim_conv_chat_join, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHAT, SNAC_SUBTYPE_CHAT_USERLEAVE, gaim_conv_chat_leave, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHAT, SNAC_SUBTYPE_CHAT_ROOMINFOUPDATE, gaim_conv_chat_info_update, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHAT, SNAC_SUBTYPE_CHAT_INCOMINGMSG, gaim_conv_chat_incoming_msg, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHATNAV, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_CHATNAV, SNAC_SUBTYPE_CHATNAV_INFO, gaim_chatnav_info, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_ERROR, gaim_ssi_parseerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_RIGHTSINFO, gaim_ssi_parserights, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_LIST, gaim_ssi_parselist, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_SRVACK, gaim_ssi_parseack, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_ADD, gaim_ssi_parseadd, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_RECVAUTH, gaim_ssi_authgiven, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_RECVAUTHREQ, gaim_ssi_authrequest, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_RECVAUTHREP, gaim_ssi_authreply, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_FEEDBAG, SNAC_SUBTYPE_FEEDBAG_ADDED, gaim_ssi_gotadded, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, 0x0005, gaim_icbm_param_info, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_INCOMING, gaim_parse_incoming_im, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_MISSEDCALL, gaim_parse_misses, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_CLIENTAUTORESP, gaim_parse_clientauto, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_ERROR, gaim_parse_msgerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_MTN, gaim_parse_mtn, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICBM, SNAC_SUBTYPE_ICBM_ACK, gaim_parse_msgack, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_OFFLINEMSG, gaim_offlinemsg, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_OFFLINEMSGCOMPLETE, gaim_offlinemsgdone, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_ALIAS, gaim_icqalias, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_INFO, gaim_icqinfo, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_RIGHTSINFO, gaim_parse_locaterights, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_USERINFO, gaim_parse_userinfo, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_ERROR, gaim_parse_locerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_GOTINFOBLOCK, gaim_got_infoblock, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, 0x0001, gaim_parse_genericerr, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, 0x000f, gaim_selfinfo, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, 0x001f, gaim_memrequest, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, 0x0021, oscar_icon_req,0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, SNAC_SUBTYPE_OSERVICE_RATECHANGE, gaim_parse_ratechange, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, SNAC_SUBTYPE_OSERVICE_REDIRECT, gaim_handle_redirect, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, SNAC_SUBTYPE_OSERVICE_MOTD, gaim_parse_motd, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_OSERVICE, SNAC_SUBTYPE_OSERVICE_EVIL, gaim_parse_evilnotify, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_POPUP, 0x0002, gaim_popup, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_USERLOOKUP, SNAC_SUBTYPE_USERLOOKUP_ERROR, gaim_parse_searcherror, 0);
	oscar_data_addhandler(od, SNAC_FAMILY_USERLOOKUP, 0x0003, gaim_parse_searchreply, 0);

	gaim_debug_misc("oscar", "oscar_login: gc = %p\n", gc);

	if (!aim_snvalid(gaim_account_get_username(account))) {
		gchar *buf;
		buf = g_strdup_printf(_("Unable to login: Could not sign on as %s because the screen name is invalid.  Screen names must either start with a letter and contain only letters, numbers and spaces, or contain only numbers."), gaim_account_get_username(account));
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc, buf);
		g_free(buf);
	}

	if (aim_sn_is_icq((gaim_account_get_username(account)))) {
		od->icq = TRUE;
	} else {
		gc->flags |= GAIM_CONNECTION_HTML;
		gc->flags |= GAIM_CONNECTION_AUTO_RESP;
	}

	/* Connect to core Gaim signals */
	gaim_prefs_connect_callback(gc, "/plugins/prpl/oscar/recent_buddies", recent_buddies_cb, gc);

	newconn = flap_connection_new(od, SNAC_FAMILY_AUTH);
	newconn->connect_data = gaim_proxy_connect(NULL, account,
			gaim_account_get_string(account, "server", OSCAR_DEFAULT_LOGIN_SERVER),
			gaim_account_get_int(account, "port", OSCAR_DEFAULT_LOGIN_PORT),
			connection_established_cb, newconn);
	if (newconn->connect_data == NULL)
	{
		gaim_connection_error(gc, _("Couldn't connect to host"));
		return;
	}

	gaim_connection_update_progress(gc, _("Connecting"), 0, OSCAR_CONNECT_STEPS);
	ck[0] = 0x5a;
}

void
oscar_close(GaimConnection *gc)
{
	OscarData *od;

	od = (OscarData *)gc->proto_data;

	while (od->oscar_chats)
	{
		struct chat_connection *cc = od->oscar_chats->data;
		od->oscar_chats = g_slist_remove(od->oscar_chats, cc);
		oscar_chat_destroy(cc);
	}
	while (od->create_rooms)
	{
		struct create_room *cr = od->create_rooms->data;
		g_free(cr->name);
		od->create_rooms = g_slist_remove(od->create_rooms, cr);
		g_free(cr);
	}
	oscar_data_destroy(od);
	gc->proto_data = NULL;

	gaim_prefs_disconnect_by_handle(gc);

	gaim_debug_info("oscar", "Signed off.\n");
}

static int
gaim_parse_auth_resp(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gc->account;
	char *host; int port;
	int i;
	FlapConnection *newconn;
	va_list ap;
	struct aim_authresp_info *info;

	port = gaim_account_get_int(account, "port", OSCAR_DEFAULT_LOGIN_PORT);

	va_start(ap, fr);
	info = va_arg(ap, struct aim_authresp_info *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "inside auth_resp (Screen name: %s)\n", info->sn);

	if (info->errorcode || !info->bosip || !info->cookielen || !info->cookie) {
		char buf[256];
		switch (info->errorcode) {
		case 0x01:
			/* Unregistered screen name */
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("Invalid screen name."));
			break;
		case 0x05:
			/* Incorrect password */
			gc->wants_to_die = TRUE;
			if (!gaim_account_get_remember_password(account))
				gaim_account_set_password(account, NULL);
			gaim_connection_error(gc, _("Incorrect password."));
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
		gaim_debug_error("oscar", "Login Error Code 0x%04hx\n", info->errorcode);
		gaim_debug_error("oscar", "Error URL: %s\n", info->errorurl);
		od->killme = TRUE;
		return 1;
	}

	gaim_debug_misc("oscar", "Reg status: %hu\n", info->regstatus);
	gaim_debug_misc("oscar", "E-mail: %s\n",
					(info->email != NULL) ? info->email : "null");
	gaim_debug_misc("oscar", "BOSIP: %s\n", info->bosip);
	gaim_debug_info("oscar", "Closing auth connection...\n");
	flap_connection_schedule_destroy(conn, OSCAR_DISCONNECT_DONE, NULL);

	for (i = 0; i < strlen(info->bosip); i++) {
		if (info->bosip[i] == ':') {
			port = atoi(&(info->bosip[i+1]));
			break;
		}
	}
	host = g_strndup(info->bosip, i);
	newconn = flap_connection_new(od, SNAC_FAMILY_LOCATE);
	newconn->cookielen = info->cookielen;
	newconn->cookie = g_memdup(info->cookie, info->cookielen);
	newconn->connect_data = gaim_proxy_connect(NULL, account, host, port,
			connection_established_cb, newconn);
	g_free(host);
	if (newconn->connect_data == NULL)
	{
		gaim_connection_error(gc, _("Could Not Connect"));
		od->killme = TRUE;
		return 0;
	}

	gaim_connection_update_progress(gc, _("Received authorization"), 3, OSCAR_CONNECT_STEPS);
	ck[3] = 0x64;

	return 1;
}

static void
gaim_parse_auth_securid_request_yes_cb(gpointer user_data, const char *msg)
{
	GaimConnection *gc = user_data;
	OscarData *od = gc->proto_data;

	aim_auth_securid_send(od, msg);
}

static void
gaim_parse_auth_securid_request_no_cb(gpointer user_data, const char *value)
{
	GaimConnection *gc = user_data;
	OscarData *od = gc->proto_data;

	/* Disconnect */
	gc->wants_to_die = TRUE;
	gaim_connection_error(gc, _("The SecurID key entered is invalid."));
	od->killme = TRUE;
}

static int
gaim_parse_auth_securid_request(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	gchar *primary;

	gaim_debug_info("oscar", "Got SecurID request\n");

	primary = g_strdup_printf("Enter the SecurID key for %s.", gaim_account_get_username(account));
	gaim_request_input(gc, NULL, _("Enter SecurID"), primary,
					   _("Enter the 6 digit number from the digital display."),
					   FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(gaim_parse_auth_securid_request_yes_cb),
					   _("Cancel"), G_CALLBACK(gaim_parse_auth_securid_request_no_cb),
					   gc);
	g_free(primary);

	return 1;
}

/* XXX - Should use gaim_util_fetch_url for the below stuff */
struct pieceofcrap {
	GaimConnection *gc;
	unsigned long offset;
	unsigned long len;
	char *modname;
	int fd;
	FlapConnection *conn;
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
	if (read(pos->fd, m, 16) != 16)
	{
		gaim_debug_warning("oscar", "Could not read full AIM login hash "
				"from " AIMHASHDATA "--that's bad.\n");
	}
	m[16] = '\0';
	gaim_debug_misc("oscar", "Sending hash: ");
	for (x = 0; x < 16; x++)
		gaim_debug_misc(NULL, "%02hhx ", (unsigned char)m[x]);

	gaim_debug_misc(NULL, "\n");
	gaim_input_remove(pos->inpa);
	close(pos->fd);
	aim_sendmemblock(od, pos->conn, 0, 16, m, AIM_SENDMEMBLOCK_FLAG_ISHASH);
	g_free(pos);
}

static void
straight_to_hell(gpointer data, gint source, const gchar *error_message)
{
	struct pieceofcrap *pos = data;
	gchar *buf;

	if (!GAIM_CONNECTION_IS_VALID(pos->gc))
	{
		g_free(pos->modname);
		g_free(pos);
		return;
	}

	pos->fd = source;

	if (source < 0) {
		buf = g_strdup_printf(_("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), GAIM_WEBSITE);
		gaim_notify_warning(pos->gc, NULL,
							_("Gaim was unable to get a valid AIM login hash."),
							buf);
		g_free(buf);
		g_free(pos->modname);
		g_free(pos);
		return;
	}

	buf = g_strdup_printf("GET " AIMHASHDATA "?offset=%ld&len=%ld&modname=%s HTTP/1.0\n\n",
			pos->offset, pos->len, pos->modname ? pos->modname : "");
	write(pos->fd, buf, strlen(buf));
	g_free(buf);
	g_free(pos->modname);
	pos->inpa = gaim_input_add(pos->fd, GAIM_INPUT_READ, damn_you, pos);
	return;
}

/* size of icbmui.ocm, the largest module in AIM 3.5 */
#define AIM_MAX_FILE_SIZE 98304

int gaim_memrequest(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	struct pieceofcrap *pos;
	guint32 offset, len;
	char *modname;

	va_start(ap, fr);
	offset = va_arg(ap, guint32);
	len = va_arg(ap, guint32);
	modname = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_misc("oscar", "offset: %u, len: %u, file: %s\n",
					offset, len, (modname ? modname : "aim.exe"));

	if (len == 0) {
		gaim_debug_misc("oscar", "len is 0, hashing NULL\n");
		aim_sendmemblock(od, conn, offset, len, NULL,
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
		aim_sendmemblock(od, command->conn, offset, i, buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		g_free(buf);
		return 1;
	}
#endif

	pos = g_new0(struct pieceofcrap, 1);
	pos->gc = od->gc;
	pos->conn = conn;

	pos->offset = offset;
	pos->len = len;
	pos->modname = g_strdup(modname);

	/* TODO: Keep track of this return value. */
	if (gaim_proxy_connect(NULL, pos->gc->account, "gaim.sourceforge.net", 80,
			straight_to_hell, pos) == NULL)
	{
		char buf[256];
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  "
			"Check %s for updates."), GAIM_WEBSITE);
		gaim_notify_warning(pos->gc, NULL,
							_("Gaim was unable to get a valid login hash."),
							buf);
	}

	return 1;
}

static int
gaim_parse_login(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc;
	GaimAccount *account;
	ClientInfo info = CLIENTINFO_GAIM;
	va_list ap;
	char *key;

	gc = od->gc;
	account = gaim_connection_get_account(gc);

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(od, conn, gaim_account_get_username(account),
				   gaim_connection_get_password(gc), &info, key);

	gaim_connection_update_progress(gc, _("Password sent"), 2, OSCAR_CONNECT_STEPS);
	ck[2] = 0x6c;

	return 1;
}

static int
gaim_handle_redirect(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	char *host, *separator;
	int port;
	FlapConnection *newconn;
	va_list ap;
	struct aim_redirect_data *redir;

	va_start(ap, fr);
	redir = va_arg(ap, struct aim_redirect_data *);
	va_end(ap);

	port = gaim_account_get_int(account, "port", OSCAR_DEFAULT_LOGIN_PORT);
	separator = strchr(redir->ip, ':');
	if (separator != NULL)
	{
		host = g_strndup(redir->ip, separator - redir->ip);
		port = atoi(separator + 1);
	}
	else
		host = g_strdup(redir->ip);

	gaim_debug_info("oscar", "Connecting to FLAP server %s:%d of type 0x%04hx\n",
					host, port, redir->group);
	newconn = flap_connection_new(od, redir->group);
	newconn->cookielen = redir->cookielen;
	newconn->cookie = g_memdup(redir->cookie, redir->cookielen);
	if (newconn->type == SNAC_FAMILY_CHAT)
	{
		struct chat_connection *cc;
		cc = g_new0(struct chat_connection, 1);
		cc->conn = newconn;
		cc->gc = gc;
		cc->name = g_strdup(redir->chat.room);
		cc->exchange = redir->chat.exchange;
		cc->instance = redir->chat.instance;
		cc->show = extract_name(redir->chat.room);
		newconn->new_conn_data = cc;
		gaim_debug_info("oscar", "Connecting to chat room %s exchange %hu\n", cc->name, cc->exchange);
	}

	newconn->connect_data = gaim_proxy_connect(NULL, account, host, port,
			connection_established_cb, newconn);
	if (newconn->connect_data == NULL)
	{
		flap_connection_schedule_destroy(newconn,
				OSCAR_DISCONNECT_COULD_NOT_CONNECT,
				_("Unable to initialize connection"));
		gaim_debug_error("oscar", "Unable to connect to FLAP server "
				"of type 0x%04hx\n", redir->group);
	}
	g_free(host);

	return 1;
}

static int gaim_parse_oncoming(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc;
	GaimAccount *account;
	GaimPresence *presence;
	struct buddyinfo *bi;
	time_t time_idle = 0, signon = 0;
	int type = 0;
	gboolean buddy_is_away = FALSE;
	const char *status_id;
	gboolean have_status_message = FALSE;
	char *message = NULL;
	va_list ap;
	aim_userinfo_t *info;

	gc = od->gc;
	account = gaim_connection_get_account(gc);
	presence = gaim_account_get_presence(account);

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	g_return_val_if_fail(info != NULL, 1);
	g_return_val_if_fail(info->sn != NULL, 1);

	if (info->present & AIM_USERINFO_PRESENT_FLAGS) {
		if (info->flags & AIM_FLAG_AWAY)
			buddy_is_away = TRUE;
	}
	if (info->present & AIM_USERINFO_PRESENT_ICQEXTSTATUS) {
		type = info->icqinfo.status;
		if (!(info->icqinfo.status & AIM_ICQ_STATE_CHAT) &&
		      (info->icqinfo.status != AIM_ICQ_STATE_NORMAL)) {
			buddy_is_away = TRUE;
		}
	}

	if (aim_sn_is_icq(info->sn)) {
		if (type & AIM_ICQ_STATE_CHAT)
			status_id = OSCAR_STATUS_ID_FREE4CHAT;
		else if (type & AIM_ICQ_STATE_DND)
			status_id = OSCAR_STATUS_ID_DND;
		else if (type & AIM_ICQ_STATE_OUT)
			status_id = OSCAR_STATUS_ID_NA;
		else if (type & AIM_ICQ_STATE_BUSY)
			status_id = OSCAR_STATUS_ID_OCCUPIED;
		else if (type & AIM_ICQ_STATE_AWAY)
			status_id = OSCAR_STATUS_ID_AWAY;
		else if (type & AIM_ICQ_STATE_INVISIBLE)
			status_id = OSCAR_STATUS_ID_INVISIBLE;
		else
			status_id = OSCAR_STATUS_ID_AVAILABLE;
	} else {
		if (buddy_is_away)
			status_id = OSCAR_STATUS_ID_AWAY;
		else
			status_id = OSCAR_STATUS_ID_AVAILABLE;
	}

	/*
	 * Handle the available message.  If info->status is NULL then the user
	 * may or may not have an available message, so don't do anything.  If
	 * info->status is set to the empty string, then the user's client DOES
	 * support available messages and the user DOES NOT have one set.
	 * Otherwise info->status contains the available message.
	 */
	if (info->status != NULL)
	{
		have_status_message = TRUE;
		if (info->status[0] != '\0')
			message = oscar_encoding_to_utf8(info->status_encoding,
											 info->status, info->status_len);
	}

	if (have_status_message)
	{
		gaim_prpl_got_user_status(account, info->sn, status_id,
								  "message", message, NULL);
		g_free(message);
	}
	else
		gaim_prpl_got_user_status(account, info->sn, status_id, NULL);

	/* Login time stuff */
	if (info->present & AIM_USERINFO_PRESENT_ONLINESINCE)
		signon = info->onlinesince;
	else if (info->present & AIM_USERINFO_PRESENT_SESSIONLEN)
		signon = time(NULL) - info->sessionlen;
	if (!aim_sncmp(gaim_account_get_username(account), info->sn)) {
		gaim_connection_set_display_name(gc, info->sn);
		od->timeoffset = signon - gaim_presence_get_login_time(presence);
	}
	gaim_prpl_got_user_login_time(account, info->sn, signon - od->timeoffset);

	/* Idle time stuff */
	/* info->idletime is the number of minutes that this user has been idle */
	if (info->present & AIM_USERINFO_PRESENT_IDLE)
		time_idle = time(NULL) - info->idletime * 60;

	if (time_idle > 0)
		gaim_prpl_got_user_idle(account, info->sn, TRUE, time_idle);
	else
		gaim_prpl_got_user_idle(account, info->sn, FALSE, 0);

	/* Server stored icon stuff */
	bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, info->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(gaim_normalize(account, info->sn)), bi);
	}
	bi->typingnot = FALSE;
	bi->ico_informed = FALSE;
	bi->ipaddr = info->icqinfo.ipaddr;

	if (info->iconcsumlen) {
		const char *filename, *saved_b16 = NULL;
		char *b16 = NULL, *filepath = NULL;
		GaimBuddy *b = NULL;

		b16 = gaim_base16_encode(info->iconcsum, info->iconcsumlen);
		b = gaim_find_buddy(account, info->sn);
		/*
		 * If for some reason the checksum is valid, but cached file is not..
		 * we want to know.
		 */
		if (b != NULL)
			filename = gaim_blist_node_get_string((GaimBlistNode*)b, "buddy_icon");
		else
			filename = NULL;
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
				if (od->icontimer == 0)
					od->icontimer = gaim_timeout_add(500, gaim_icon_timerfunc, gc);
			}
		}
		g_free(b16);
	}

	return 1;
}

static void gaim_check_comment(OscarData *od, const char *str) {
	if ((str == NULL) || strcmp(str, (const char *)ck))
		aim_locate_setcaps(od, gaim_caps);
	else
		aim_locate_setcaps(od, gaim_caps | OSCAR_CAPABILITY_SECUREIM);
}

static int gaim_parse_offgoing(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
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

static int incomingim_chan1(OscarData *od, FlapConnection *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch1_args *args) {
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimMessageFlags flags = 0;
	struct buddyinfo *bi;
	char *iconfile;
	GString *message;
	gchar *tmp;
	aim_mpmsg_section_t *curpart;
	const char *start, *end;
	GData *attribs;

	gaim_debug_misc("oscar", "Received IM from %s with %d parts\n",
					userinfo->sn, args->mpmsg.numparts);

	if (args->mpmsg.numparts == 0)
		return 1;

	bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(account, userinfo->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(gaim_normalize(account, userinfo->sn)), bi);
	}

	if (args->icbmflags & AIM_IMFLAGS_AWAY)
		flags |= GAIM_MESSAGE_AUTO_RESP;

	if (args->icbmflags & AIM_IMFLAGS_TYPINGNOT)
		bi->typingnot = TRUE;
	else
		bi->typingnot = FALSE;

	if ((args->icbmflags & AIM_IMFLAGS_HASICON) && (args->iconlen) && (args->iconsum) && (args->iconstamp)) {
		gaim_debug_misc("oscar", "%s has an icon\n", userinfo->sn);
		if ((args->iconlen != bi->ico_len) || (args->iconsum != bi->ico_csum) || (args->iconstamp != bi->ico_time)) {
			bi->ico_need = TRUE;
			bi->ico_len = args->iconlen;
			bi->ico_csum = args->iconsum;
			bi->ico_time = args->iconstamp;
		}
	}

	iconfile = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(account));
	if ((iconfile != NULL) &&
	    (args->icbmflags & AIM_IMFLAGS_BUDDYREQ) && !bi->ico_sent && bi->ico_informed) {
		FILE *file;
		struct stat st;

		if (!g_stat(iconfile, &st)) {
			guchar *buf = g_malloc(st.st_size);
			file = g_fopen(iconfile, "rb");
			if (file) {
				/* XXX - Use g_file_get_contents() */
				/* g_file_get_contents(iconfile, &data, &len, NULL); */
				int len = fread(buf, 1, st.st_size, file);
				gaim_debug_info("oscar",
						   "Sending buddy icon to %s (%d bytes, "
						   "%lu reported)\n",
						   userinfo->sn, len, st.st_size);
				aim_im_sendch2_icon(od, userinfo->sn, buf, st.st_size,
					st.st_mtime, aimutil_iconsum(buf, st.st_size));
				fclose(file);
			} else
				gaim_debug_error("oscar", "Can't open buddy icon file!\n");
			g_free(buf);
		} else
			gaim_debug_error("oscar", "Can't stat buddy icon file!\n");
	}
	g_free(iconfile);

	message = g_string_new("");
	curpart = args->mpmsg.parts;
	while (curpart != NULL) {
		tmp = gaim_plugin_oscar_decode_im_part(account, userinfo->sn, curpart->charset,
				curpart->charsubset, curpart->data, curpart->datalen);
		if (tmp != NULL) {
			g_string_append(message, tmp);
			g_free(tmp);
		}

		curpart = curpart->next;
	}
	tmp = g_string_free(message, FALSE);

	/*
	 * If the message is from an ICQ user and to an ICQ user then escape any HTML,
	 * because HTML is not sent over ICQ as a means to format a message.
	 * So any HTML we receive is intended to be displayed.  Also, \r\n must be
	 * replaced with <br>
	 *
	 * Note: There *may* be some clients which send messages as HTML formatted -
	 *       they need to be special-cased somehow.
	 */
	if (aim_sn_is_icq(gaim_account_get_username(account)) && aim_sn_is_icq(userinfo->sn)) {
		/* being recevied by ICQ from ICQ - escape HTML so it is displayed as sent */
		gchar *tmp2 = g_markup_escape_text(tmp, -1);
		g_free(tmp);
		tmp = tmp2;
		tmp2 = gaim_strreplace(tmp, "\r\n", "<br>");
		g_free(tmp);
		tmp = tmp2;
	}

	/*
	 * Convert iChat color tags to normal font tags.
	 */
	if (gaim_markup_find_tag("body", tmp, &start, &end, &attribs))
	{
		const char *ichattextcolor, *ichatballooncolor;

		ichattextcolor = g_datalist_get_data(&attribs, "ichattextcolor");
		if (ichattextcolor != NULL)
		{
			gchar *tmp2;
			tmp2 = g_strdup_printf("<font color=\"%s\">%s</font>", ichattextcolor, tmp);
			g_free(tmp);
			tmp = tmp2;
		}

		ichatballooncolor = g_datalist_get_data(&attribs, "ichatballooncolor");
		if (ichatballooncolor != NULL)
		{
			gchar *tmp2;
			tmp2 = g_strdup_printf("<font back=\"%s\">%s</font>", ichatballooncolor, tmp);
			g_free(tmp);
			tmp = tmp2;
		}

		g_datalist_clear(&attribs);
	}

	serv_got_im(gc, userinfo->sn, tmp, flags, time(NULL));
	g_free(tmp);

	return 1;
}

static int
incomingim_chan2(OscarData *od, FlapConnection *conn, aim_userinfo_t *userinfo, IcbmArgsCh2 *args)
{
	GaimConnection *gc;
	GaimAccount *account;
	char *message = NULL;

	g_return_val_if_fail(od != NULL, 0);
	g_return_val_if_fail(od->gc != NULL, 0);

	gc = od->gc;
	account = gaim_connection_get_account(gc);
	od = gc->proto_data;

	if (args == NULL)
		return 0;

	gaim_debug_misc("oscar", "Incoming rendezvous message of type %u, "
			"user %s, status %hu\n", args->type, userinfo->sn, args->status);

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

	if (args->type & OSCAR_CAPABILITY_CHAT)
	{
		char *encoding, *utf8name, *tmp;
		GHashTable *components;

		if (!args->info.chat.roominfo.name || !args->info.chat.roominfo.exchange) {
			g_free(message);
			return 1;
		}
		encoding = args->encoding ? oscar_encoding_extract(args->encoding) : NULL;
		utf8name = oscar_encoding_to_utf8(encoding,
				args->info.chat.roominfo.name,
				args->info.chat.roominfo.namelen);
		g_free(encoding);

		tmp = extract_name(utf8name);
		if (tmp != NULL)
		{
			g_free(utf8name);
			utf8name = tmp;
		}

		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				g_free);
		g_hash_table_replace(components, g_strdup("room"), utf8name);
		g_hash_table_replace(components, g_strdup("exchange"),
				g_strdup_printf("%d", args->info.chat.roominfo.exchange));
		serv_got_chat_invite(gc,
				     utf8name,
				     userinfo->sn,
				     message,
				     components);
	}

	else if ((args->type & OSCAR_CAPABILITY_SENDFILE) ||
			 (args->type & OSCAR_CAPABILITY_DIRECTIM))
	{
		if (args->status == AIM_RENDEZVOUS_PROPOSE)
		{
			peer_connection_got_proposition(od, userinfo->sn, message, args);
		}
		else if (args->status == AIM_RENDEZVOUS_CANCEL)
		{
			/* The other user canceled a peer request */
			PeerConnection *conn;

			conn = peer_connection_find_by_cookie(od, userinfo->sn, args->cookie);
			/*
			 * If conn is NULL it means we haven't tried to create
			 * a connection with that user.  They may be trying to
			 * do something malicious.
			 */
			if (conn != NULL)
			{
				peer_connection_destroy(conn, OSCAR_DISCONNECT_REMOTE_CLOSED, NULL);
			}
		}
		else if (args->status == AIM_RENDEZVOUS_CONNECTED)
		{
			/* Remote user has accepted our peer request */
			PeerConnection *conn;

			conn = peer_connection_find_by_cookie(od, userinfo->sn, args->cookie);
			/*
			 * If conn is NULL it means we haven't tried to create
			 * a connection with that user.  They may be trying to
			 * do something malicious.
			 */
			if (conn != NULL)
			{
				if (conn->listenerfd != -1)
				{
					/*
					 * If they are connecting directly to us then
					 * continue the peer negotiation by
					 * accepting connections on our listener port.
					 */
					conn->watcher_incoming = gaim_input_add(conn->listenerfd,
							GAIM_INPUT_READ, peer_connection_listen_cb, conn);
				}
			}
		}
	}

	else if (args->type & OSCAR_CAPABILITY_GETFILE)
	{
	}

	else if (args->type & OSCAR_CAPABILITY_TALK)
	{
	}

	else if (args->type & OSCAR_CAPABILITY_BUDDYICON)
	{
		gaim_buddy_icons_set_for_user(account, userinfo->sn,
									  args->info.icon.icon,
									  args->info.icon.length);
	}

	else if (args->type & OSCAR_CAPABILITY_ICQSERVERRELAY)
	{
		gaim_debug_error("oscar", "Got an ICQ Server Relay message of "
				"type %d\n", args->info.rtfmsg.msgtype);
	}

	else
	{
		gaim_debug_error("oscar", "Unknown request class %hu\n",
				args->type);
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
static void
gaim_auth_request(struct name_data *data, char *msg)
{
	GaimConnection *gc;
	OscarData *od;
	GaimBuddy *buddy;
	GaimGroup *group;

	gc = data->gc;
	od = gc->proto_data;
	buddy = gaim_find_buddy(gaim_connection_get_account(gc), data->name);
	if (buddy != NULL)
		group = gaim_buddy_get_group(buddy);
	else
		group = NULL;

	if (group != NULL)
	{
		gaim_debug_info("oscar", "ssi: adding buddy %s to group %s\n",
				   buddy->name, group->name);
		aim_ssi_sendauthrequest(od, data->name, msg ? msg : _("Please authorize me so I can add you to my buddy list."));
		if (!aim_ssi_itemlist_finditem(od->ssi.local, group->name, buddy->name, AIM_SSI_TYPE_BUDDY))
			aim_ssi_addbuddy(od, buddy->name, group->name, gaim_buddy_get_alias_only(buddy), NULL, NULL, 1);
	}
}

static void
gaim_auth_dontrequest(struct name_data *data)
{
	GaimConnection *gc = data->gc;
	GaimBuddy *b = gaim_find_buddy(gaim_connection_get_account(gc), data->name);

	/* Remove from local list */
	gaim_blist_remove_buddy(b);

	oscar_free_name_data(data);
}


static void
gaim_auth_sendrequest(GaimConnection *gc, const char *name)
{
	struct name_data *data;

	data = g_new0(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(name);

	gaim_request_input(data->gc, NULL, _("Authorization Request Message:"),
					   NULL, _("Please authorize me!"), TRUE, FALSE, NULL,
					   _("OK"), G_CALLBACK(gaim_auth_request),
					   _("Cancel"), G_CALLBACK(gaim_auth_dontrequest),
					   data);
}


static void
gaim_auth_sendrequest_menu(GaimBlistNode *node, gpointer ignored)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);
	gaim_auth_sendrequest(gc, buddy->name);
}

/* When other people ask you for authorization */
static void
gaim_auth_grant(struct name_data *data)
{
	GaimConnection *gc = data->gc;
	OscarData *od = gc->proto_data;

	aim_ssi_sendauthreply(od, data->name, 0x01, NULL);

	oscar_free_name_data(data);
}

/* When other people ask you for authorization */
static void
gaim_auth_dontgrant(struct name_data *data, char *msg)
{
	GaimConnection *gc = data->gc;
	OscarData *od = gc->proto_data;

	aim_ssi_sendauthreply(od, data->name, 0x00, msg ? msg : _("No reason given."));
}

static void
gaim_auth_dontgrant_msgprompt(struct name_data *data)
{
	gaim_request_input(data->gc, NULL, _("Authorization Denied Message:"),
					   NULL, _("No reason given."), TRUE, FALSE, NULL,
					   _("OK"), G_CALLBACK(gaim_auth_dontgrant),
					   _("Cancel"), G_CALLBACK(oscar_free_name_data),
					   data);
}

/* When someone sends you buddies */
static void
gaim_icq_buddyadd(struct name_data *data)
{
	GaimConnection *gc = data->gc;

	gaim_blist_request_add_buddy(gaim_connection_get_account(gc), data->name, NULL, data->nick);

	oscar_free_name_data(data);
}

static int
incomingim_chan4(OscarData *od, FlapConnection *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch4_args *args, time_t t)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	gchar **msg1, **msg2;
	int i, numtoks;

	if (!args->type || !args->msg || !args->uin)
		return 1;

	gaim_debug_info("oscar",
					"Received a channel 4 message of type 0x%02hx.\n",
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
		gchar *uin = g_strdup_printf("%u", args->uin);

		gaim_str_strip_char(msg1[i], '\r');
		/* TODO: Should use an encoding other than ASCII? */
		msg2[i] = gaim_plugin_oscar_decode_im_part(account, uin, AIM_CHARSET_ASCII, 0x0000, msg1[i], strlen(msg1[i]));
		g_free(uin);
	}
	msg2[i] = NULL;

	switch (args->type) {
		case 0x01: { /* MacICQ message or basic offline message */
			if (i >= 1) {
				gchar *uin = g_strdup_printf("%u", args->uin);
				gchar *tmp;

				/* If the message came from an ICQ user then escape any HTML */
				tmp = g_markup_escape_text(msg2[0], -1);

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
				gchar *sn = g_strdup_printf("%u", args->uin);
				gchar *reason = NULL;

				if (msg2[5] != NULL)
					reason = gaim_plugin_oscar_decode_im_part(account, sn, AIM_CHARSET_CUSTOM, 0x0000, msg2[5], strlen(msg2[5]));

				gaim_debug_info("oscar",
						   "Received an authorization request from UIN %u\n",
						   args->uin);
				data->gc = gc;
				data->name = sn;
				data->nick = NULL;

				gaim_account_request_authorization(account, sn, NULL, NULL,
						reason, gaim_find_buddy(account, sn) != NULL,
						G_CALLBACK(gaim_auth_grant),
						G_CALLBACK(gaim_auth_dontgrant_msgprompt), data);
				g_free(reason);
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
				gchar *dialog_msg = g_strdup_printf(_("You have received an ICQ e-mail from %s [%s]\n\nMessage is:\n%s"), msg2[0], msg2[3], msg2[5]);
				gaim_notify_info(gc, NULL, "ICQ E-Mail", dialog_msg);
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
										_("_Decline"), G_CALLBACK(oscar_free_name_data));
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

static int gaim_parse_incoming_im(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	guint16 channel;
	int ret = 0;
	aim_userinfo_t *userinfo;
	va_list ap;

	va_start(ap, fr);
	channel = (guint16)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	switch (channel) {
		case 1: { /* standard message */
			struct aim_incomingim_ch1_args *args;
			args = va_arg(ap, struct aim_incomingim_ch1_args *);
			ret = incomingim_chan1(od, conn, userinfo, args);
		} break;

		case 2: { /* rendezvous */
			IcbmArgsCh2 *args;
			args = va_arg(ap, IcbmArgsCh2 *);
			ret = incomingim_chan2(od, conn, userinfo, args);
		} break;

		case 4: { /* ICQ */
			struct aim_incomingim_ch4_args *args;
			args = va_arg(ap, struct aim_incomingim_ch4_args *);
			ret = incomingim_chan4(od, conn, userinfo, args, 0);
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

static int gaim_parse_misses(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	char *buf;
	va_list ap;
	guint16 chan, nummissed, reason;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	chan = (guint16)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	nummissed = (guint16)va_arg(ap, unsigned int);
	reason = (guint16)va_arg(ap, unsigned int);
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
		gaim_notify_error(od->gc, NULL, buf, NULL);
	g_free(buf);

	return 1;
}

static int
gaim_parse_clientauto_ch2(OscarData *od, const char *who, guint16 reason, const guchar *cookie)
{
	if (reason == 0x0003)
	{
		/* Rendezvous was refused. */
		PeerConnection *conn;

		conn = peer_connection_find_by_cookie(od, who, cookie);

		if (conn == NULL)
		{
			gaim_debug_info("oscar", "Received a rendezvous cancel message "
					"for a nonexistant connection from %s.\n", who);
		}
		else
		{
			peer_connection_destroy(conn, OSCAR_DISCONNECT_REMOTE_REFUSED, NULL);
		}
	}
	else
	{
		gaim_debug_warning("oscar", "Received an unknown rendezvous "
				"message from %s.  Type 0x%04hx\n", who, reason);
	}

	return 0;
}

static int gaim_parse_clientauto_ch4(OscarData *od, char *who, guint16 reason, guint32 state, char *msg) {
	GaimConnection *gc = od->gc;

	switch(reason) {
		case 0x0003: { /* Reply from an ICQ status message request */
			char *statusmsg, **splitmsg;
			GaimNotifyUserInfo *user_info;

			/* Split at (carriage return/newline)'s, then rejoin later with BRs between. */
			statusmsg = oscar_icqstatus(state);
			splitmsg = g_strsplit(msg, "\r\n", 0);
			
			user_info = gaim_notify_user_info_new();
				
			gaim_notify_user_info_add_pair(user_info, _("UIN"), who);
			gaim_notify_user_info_add_pair(user_info, _("Status"), statusmsg);
			gaim_notify_user_info_add_section_break(user_info);
			gaim_notify_user_info_add_pair(user_info, NULL, g_strjoinv("<BR>", splitmsg));

			g_free(statusmsg);
			g_strfreev(splitmsg);

			gaim_notify_userinfo(gc, who, user_info, NULL, NULL);
			gaim_notify_user_info_destroy(user_info);

		} break;

		default: {
			gaim_debug_warning("oscar",
					   "Received an unknown client auto-response from %s.  "
					   "Type 0x%04hx\n", who, reason);
		} break;
	} /* end of switch */

	return 0;
}

static int gaim_parse_clientauto(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	guint16 chan, reason;
	char *who;

	va_start(ap, fr);
	chan = (guint16)va_arg(ap, unsigned int);
	who = va_arg(ap, char *);
	reason = (guint16)va_arg(ap, unsigned int);

	if (chan == 0x0002) { /* File transfer declined */
		guchar *cookie = va_arg(ap, guchar *);
		return gaim_parse_clientauto_ch2(od, who, reason, cookie);
	} else if (chan == 0x0004) { /* ICQ message */
		guint32 state = 0;
		char *msg = NULL;
		if (reason == 0x0003) {
			state = va_arg(ap, guint32);
			msg = va_arg(ap, char *);
		}
		return gaim_parse_clientauto_ch4(od, who, reason, state, msg);
	}

	va_end(ap);

	return 1;
}

static int gaim_parse_genericerr(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	guint16 reason;
	char *m;

	va_start(ap, fr);
	reason = (guint16) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_error("oscar",
			   "snac threw error (reason 0x%04hx: %s)\n", reason,
			   (reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown");

	m = g_strdup_printf(_("SNAC threw error: %s\n"),
			reason < msgerrreasonlen ? _(msgerrreason[reason]) : _("Unknown error"));
	gaim_notify_error(od->gc, NULL, m, NULL);
	g_free(m);

	return 1;
}

static int gaim_parse_msgerr(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
#ifdef TODOFT
	OscarData *od = gc->proto_data;
	GaimXfer *xfer;
#endif
	va_list ap;
	guint16 reason;
	char *data, *buf;

	va_start(ap, fr);
	reason = (guint16)va_arg(ap, unsigned int);
	data = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_error("oscar",
			   "Message error with data %s and reason %hu\n",
				(data != NULL ? data : ""), reason);

#ifdef TODOFT
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
		gaim_notify_error(od->gc, NULL, buf,
				  (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	}
	g_free(buf);

	return 1;
}

static int gaim_parse_mtn(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	guint16 type1, type2;
	char *sn;

	va_start(ap, fr);
	type1 = (guint16) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	type2 = (guint16) va_arg(ap, unsigned int);
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
			/*
			 * It looks like iChat sometimes sends typing notification
			 * with type1=0x0001 and type2=0x000f.  Not sure why.
			 */
			gaim_debug_info("oscar", "Received unknown typing notification message from %s.  Type1 is 0x%04x and type2 is 0x%04hx.\n", sn, type1, type2);
		} break;
	}

	return 1;
}

/*
 * We get this error when there was an error in the locate family.  This
 * happens when you request info of someone who is offline.
 */
static int gaim_parse_locerr(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	gchar *buf;
	va_list ap;
	guint16 reason;
	char *destn;

	va_start(ap, fr);
	reason = (guint16) va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	if (destn == NULL)
		return 1;

	buf = g_strdup_printf(_("User information not available: %s"), (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	if (!gaim_conv_present_error(destn, gaim_connection_get_account((GaimConnection*)od->gc), buf)) {
		g_free(buf);
		buf = g_strdup_printf(_("User information for %s unavailable:"), destn);
		gaim_notify_error(od->gc, NULL, buf, (reason < msgerrreasonlen) ? _(msgerrreason[reason]) : _("Unknown reason."));
	}
	g_free(buf);

	return 1;
}

static int gaim_parse_userinfo(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimNotifyUserInfo *user_info;
	gchar *tmp = NULL, *info_utf8 = NULL, *away_utf8 = NULL;
	va_list ap;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	user_info = gaim_notify_user_info_new();
	gaim_notify_user_info_add_pair(user_info, _("Screen Name"), userinfo->sn);

	tmp = g_strdup_printf("%d", (int)((userinfo->warnlevel/10.0) + 0.5));
	gaim_notify_user_info_add_pair(user_info, _("Warning Level"), tmp);
	g_free(tmp);

	if (userinfo->present & AIM_USERINFO_PRESENT_ONLINESINCE) {
		time_t t = userinfo->onlinesince - od->timeoffset;
		oscar_user_info_add_pair(user_info, _("Online Since"), gaim_date_format_full(localtime(&t)));
	}

	if (userinfo->present & AIM_USERINFO_PRESENT_MEMBERSINCE) {
		time_t t = userinfo->membersince - od->timeoffset;
		oscar_user_info_add_pair(user_info, _("Member Since"), gaim_date_format_full(localtime(&t)));
	}

	if (userinfo->capabilities != 0) {
		tmp = oscar_caps_to_string(userinfo->capabilities);
		oscar_user_info_add_pair(user_info, _("Capabilities"), tmp);
		g_free(tmp);
	}

	if (userinfo->present & AIM_USERINFO_PRESENT_IDLE) {
		tmp = gaim_str_seconds_to_string(userinfo->idletime*60);
		oscar_user_info_add_pair(user_info, _("Idle"), tmp);
		g_free(tmp);
	}

	oscar_string_append_info(gc, user_info, NULL, userinfo);

	/* Available message */
	if ((userinfo->status != NULL) && !(userinfo->flags & AIM_FLAG_AWAY))
	{
		if (userinfo->status[0] != '\0')
			tmp = oscar_encoding_to_utf8(userinfo->status_encoding,
											 userinfo->status, userinfo->status_len);
		oscar_user_info_add_pair(user_info, _("Available Message"), tmp);
		g_free(tmp);
	}

	/* Away message */
	if ((userinfo->flags & AIM_FLAG_AWAY) && (userinfo->away_len > 0) && (userinfo->away != NULL) && (userinfo->away_encoding != NULL)) {
		tmp = oscar_encoding_extract(userinfo->away_encoding);
		away_utf8 = oscar_encoding_to_utf8(tmp, userinfo->away, userinfo->away_len);
		g_free(tmp);
		if (away_utf8 != NULL) {
			tmp = gaim_str_sub_away_formatters(away_utf8, gaim_account_get_username(account));
			gaim_notify_user_info_add_section_break(user_info);
			oscar_user_info_add_pair(user_info, NULL, tmp);
			g_free(tmp);
			g_free(away_utf8);
		}
	}

	/* Info */
	if ((userinfo->info_len > 0) && (userinfo->info != NULL) && (userinfo->info_encoding != NULL)) {
		tmp = oscar_encoding_extract(userinfo->info_encoding);
		info_utf8 = oscar_encoding_to_utf8(tmp, userinfo->info, userinfo->info_len);
		g_free(tmp);
		if (info_utf8 != NULL) {
			tmp = gaim_str_sub_away_formatters(info_utf8, gaim_account_get_username(account));
			gaim_notify_user_info_add_section_break(user_info);
			oscar_user_info_add_pair(user_info, _("Profile"), tmp);
			g_free(tmp);
			g_free(info_utf8);
		}
	}

	gaim_notify_userinfo(gc, userinfo->sn, user_info, NULL, NULL);
	gaim_notify_user_info_destroy(user_info);

	return 1;
}

static int gaim_got_infoblock(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimBuddy *b;
	GaimPresence *presence;
	GaimStatus *status;
	gchar *message = NULL;

	va_list ap;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	b = gaim_find_buddy(gaim_connection_get_account(gc), userinfo->sn);
	if (b == NULL)
		return 1;

	if (!aim_sn_is_icq(userinfo->sn))
	{
		if (strcmp(gaim_buddy_get_name(b), userinfo->sn))
			serv_got_alias(gc, gaim_buddy_get_name(b), userinfo->sn);
		else
			serv_got_alias(gc, gaim_buddy_get_name(b), NULL);
	}

	presence = gaim_buddy_get_presence(b);
	status = gaim_presence_get_active_status(presence);

	if (!gaim_status_is_available(status) && gaim_status_is_online(status))
	{
		if ((userinfo->flags & AIM_FLAG_AWAY) &&
			(userinfo->away_len > 0) && (userinfo->away != NULL) && (userinfo->away_encoding != NULL)) {
			gchar *charset = oscar_encoding_extract(userinfo->away_encoding);
			message = oscar_encoding_to_utf8(charset, userinfo->away, userinfo->away_len);
			g_free(charset);
			gaim_status_set_attr_string(status, "message", message);
			g_free(message);
		}
		else
			/* Set an empty message so that we know not to show "pending" */
			gaim_status_set_attr_string(status, "message", "");

		gaim_blist_update_buddy_status(b, status);
	}

	return 1;
}

static int gaim_parse_motd(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	char *msg;
	guint16 id;
	va_list ap;

	va_start(ap, fr);
	id  = (guint16) va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "MOTD: %s (%hu)\n", msg ? msg : "Unknown", id);
	if (id < 4)
		gaim_notify_warning(od->gc, NULL,
							_("Your AIM connection may be lost."), NULL);

	return 1;
}

static int gaim_chatnav_info(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	guint16 type;

	va_start(ap, fr);
	type = (guint16) va_arg(ap, unsigned int);

	switch(type) {
		case 0x0002: {
			guint8 maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i;

			maxrooms = (guint8) va_arg(ap, unsigned int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);

			gaim_debug_misc("oscar", "chat info: Chat Rights:\n");
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
				aim_chatnav_createroom(od, conn, cr->name, cr->exchange);
				g_free(cr->name);
				od->create_rooms = g_slist_remove(od->create_rooms, cr);
				g_free(cr);
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			guint16 instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
			guint8 createperms;
			guint32 createtime;

			fqcn = va_arg(ap, char *);
			instance = (guint16)va_arg(ap, unsigned int);
			exchange = (guint16)va_arg(ap, unsigned int);
			flags = (guint16)va_arg(ap, unsigned int);
			createtime = va_arg(ap, guint32);
			maxmsglen = (guint16)va_arg(ap, unsigned int);
			maxoccupancy = (guint16)va_arg(ap, unsigned int);
			createperms = (guint8)va_arg(ap, unsigned int);
			unknown = (guint16)va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);

			gaim_debug_misc("oscar",
					"created room: %s %hu %hu %hu %u %hu %hu %hhu %hu %s %s\n",
					fqcn, exchange, instance, flags, createtime,
					maxmsglen, maxoccupancy, createperms, unknown,
					name, ck);
			aim_chat_join(od, exchange, ck, instance);
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

static int gaim_conv_chat_join(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	GaimConnection *gc = od->gc;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(gc, conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(c->conv), info[i].sn, NULL, GAIM_CBFLAGS_NONE, TRUE);

	return 1;
}

static int gaim_conv_chat_leave(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	GaimConnection *gc = od->gc;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(gc, conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(c->conv), info[i].sn, NULL);

	return 1;
}

static int gaim_conv_chat_info_update(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	aim_userinfo_t *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	guint16 unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	guint32 creationtime;
	GaimConnection *gc = od->gc;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, conn);

	if (!ccon)
		return 1;

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount= va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (guint16)va_arg(ap, unsigned int);
	creationtime = va_arg(ap, guint32);
	maxmsglen = (guint16)va_arg(ap, unsigned int);
	unknown_d2 = (guint16)va_arg(ap, unsigned int);
	unknown_d5 = (guint16)va_arg(ap, unsigned int);
	maxvisiblemsglen = (guint16)va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "inside chat_info_update (maxmsglen = %hu, maxvislen = %hu)\n",
			   maxmsglen, maxvisiblemsglen);

	ccon->maxlen = maxmsglen;
	ccon->maxvis = maxvisiblemsglen;

	return 1;
}

static int gaim_conv_chat_incoming_msg(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, conn);
	gchar *utf8;
	va_list ap;
	aim_userinfo_t *info;
	int len;
	char *msg;
	char *charset;

	if (!ccon)
		return 1;

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

static int gaim_email_parseupdate(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	GaimConnection *gc = od->gc;
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

static int gaim_icon_error(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	char *sn;

	sn = od->requesticon->data;
	gaim_debug_misc("oscar", "removing %s from hash table\n", sn);
	od->requesticon = g_slist_remove(od->requesticon, sn);
	g_free(sn);

	if (od->icontimer == 0)
		od->icontimer = gaim_timeout_add(500, gaim_icon_timerfunc, gc);

	return 1;
}

static int gaim_icon_parseicon(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	GSList *cur;
	va_list ap;
	char *sn;
	guint8 iconcsumtype, *iconcsum, *icon;
	guint16 iconcsumlen, iconlen;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	iconcsumtype = va_arg(ap, int);
	iconcsum = va_arg(ap, guint8 *);
	iconcsumlen = va_arg(ap, int);
	icon = va_arg(ap, guint8 *);
	iconlen = va_arg(ap, int);
	va_end(ap);

	/*
	 * Some AIM clients will send a blank GIF image with iconlen 90 when
	 * no icon is set.  Ignore these.
	 */
	if ((iconlen > 0) && (iconlen != 90)) {
		char *b16;
		GaimBuddy *b;
		gaim_buddy_icons_set_for_user(gaim_connection_get_account(gc),
									  sn, icon, iconlen);
		b16 = gaim_base16_encode(iconcsum, iconcsumlen);
		b = gaim_find_buddy(gc->account, sn);
		if ((b16 != NULL) && (b != NULL)) {
			gaim_blist_node_set_string((GaimBlistNode*)b, "icon_checksum", b16);
			g_free(b16);
		}
	}

	cur = od->requesticon;
	while (cur) {
		char *cursn = cur->data;
		if (!aim_sncmp(cursn, sn)) {
			od->requesticon = g_slist_remove(od->requesticon, cursn);
			g_free(cursn);
			cur = od->requesticon;
		} else
			cur = cur->next;
	}

	if (od->icontimer == 0)
		od->icontimer = gaim_timeout_add(250, gaim_icon_timerfunc, gc);

	return 1;
}

static gboolean gaim_icon_timerfunc(gpointer data) {
	GaimConnection *gc = data;
	OscarData *od = gc->proto_data;
	aim_userinfo_t *userinfo;
	FlapConnection *conn;

	od->icontimer = 0;

	conn = flap_connection_getbytype(od, SNAC_FAMILY_BART);
	if (!conn) {
		if (!od->iconconnecting) {
			aim_srv_requestnew(od, SNAC_FAMILY_BART);
			od->iconconnecting = TRUE;
		}
		return FALSE;
	}

	if (od->set_icon) {
		struct stat st;
		char *iconfile = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(gaim_connection_get_account(gc)));
		if (iconfile == NULL) {
			aim_ssi_delicon(od);
		} else if (!g_stat(iconfile, &st)) {
			guchar *buf = g_malloc(st.st_size);
			FILE *file = g_fopen(iconfile, "rb");
			if (file) {
				/* XXX - Use g_file_get_contents()? */
				fread(buf, 1, st.st_size, file);
				fclose(file);
				gaim_debug_info("oscar",
					   "Uploading icon to icon server\n");
				aim_bart_upload(od, buf, st.st_size);
			} else
				gaim_debug_error("oscar",
					   "Can't open buddy icon file!\n");
			g_free(buf);
		} else {
			gaim_debug_error("oscar",
				   "Can't stat buddy icon file!\n");
		}
		g_free(iconfile);
		od->set_icon = FALSE;
	}

	if (!od->requesticon) {
		gaim_debug_misc("oscar",
				   "no more icons to request\n");
		return FALSE;
	}

	userinfo = aim_locate_finduserinfo(od, (char *)od->requesticon->data);
	if ((userinfo != NULL) && (userinfo->iconcsumlen > 0)) {
		aim_bart_request(od, od->requesticon->data, userinfo->iconcsumtype, userinfo->iconcsum, userinfo->iconcsumlen);
		return FALSE;
	} else {
		gchar *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		g_free(sn);
	}

	od->icontimer = gaim_timeout_add(100, gaim_icon_timerfunc, gc);

	return FALSE;
}

/*
 * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
 */
static int gaim_parse_msgack(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	guint16 type;
	char *sn;

	va_start(ap, fr);
	type = (guint16) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar", "Sent message to %s.\n", sn);

	return 1;
}

static int gaim_parse_ratechange(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	static const char *codes[5] = {
		"invalid",
		"change",
		"warning",
		"limit",
		"limit cleared",
	};
	va_list ap;
	guint16 code, rateclass;
	guint32 windowsize, clear, alert, limit, disconnect, currentavg, maxavg;

	va_start(ap, fr);
	code = (guint16)va_arg(ap, unsigned int);
	rateclass= (guint16)va_arg(ap, unsigned int);
	windowsize = va_arg(ap, guint32);
	clear = va_arg(ap, guint32);
	alert = va_arg(ap, guint32);
	limit = va_arg(ap, guint32);
	disconnect = va_arg(ap, guint32);
	currentavg = va_arg(ap, guint32);
	maxavg = va_arg(ap, guint32);
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

	if (code == AIM_RATE_CODE_LIMIT)
	{
		gaim_notify_error(od->gc, NULL, _("Rate limiting error."),
						  _("The last action you attempted could not be "
							"performed because you are over the rate limit. "
							"Please wait 10 seconds and try again."));
	}

	return 1;
}

static int gaim_parse_evilnotify(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
#ifdef CRAZY_WARNING
	va_list ap;
	guint16 newevil;
	aim_userinfo_t *userinfo;

	va_start(ap, fr);
	newevil = (guint16) va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	gaim_prpl_got_account_warning_level(account, (userinfo && userinfo->sn) ? userinfo->sn : NULL, (newevil/10.0) + 0.5);
#endif

	return 1;
}

static int gaim_selfinfo(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	int warning_level;
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	/*
	 * What's with the + 0.5?
	 * The 0.5 is basically poor-man's rounding.  Normally
	 * casting "13.7" to an int will truncate to "13," but
	 * with 13.7 + 0.5 = 14.2, which becomes "14" when
	 * truncated.
	 */
	warning_level = info->warnlevel/10.0 + 0.5;

#ifdef CRAZY_WARNING
	gaim_presence_set_warning_level(presence, warning_level);
#endif

	return 1;
}

static int gaim_connerr(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	guint16 code;
	char *msg;

	va_start(ap, fr);
	code = (guint16)va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar", "Disconnected.  Code is 0x%04x and msg is %s\n",
					code, (msg != NULL ? msg : ""));

	g_return_val_if_fail(fr       != NULL, 1);
	g_return_val_if_fail(conn != NULL, 1);

	if (conn->type == SNAC_FAMILY_LOCATE) {
		if (code == 0x0001) {
			gc->wants_to_die = TRUE;
			gaim_connection_error(gc, _("You have signed on from another location."));
		} else {
			gaim_connection_error(gc, _("You have been signed off for an unknown reason."));
		}
		od->killme = TRUE;
	} else if (conn->type == SNAC_FAMILY_CHAT) {
		struct chat_connection *cc;
		GaimConversation *conv;

		cc = find_oscar_chat_by_conn(gc, conn);
		conv = gaim_find_chat(gc, cc->id);

		if (conv != NULL)
		{
			gchar *buf;
			buf = g_strdup_printf(_("You have been disconnected from chat "
									"room %s."), cc->name);
			gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_ERROR, time(NULL));
			g_free(buf);
		}
		oscar_chat_kill(gc, cc);
	}

	return 1;
}

static int gaim_icbm_param_info(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
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

	aim_im_setparams(od, params);

	return 1;
}

static int gaim_parse_locaterights(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	va_list ap;
	guint16 maxsiglen;

	va_start(ap, fr);
	maxsiglen = (guint16) va_arg(ap, int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "locate rights: max sig len = %d\n", maxsiglen);

	od->rights.maxsiglen = od->rights.maxawaymsglen = (guint)maxsiglen;

	aim_locate_setcaps(od, gaim_caps);
	oscar_set_info_and_status(account, TRUE, account->user_info, TRUE,
							  gaim_account_get_active_status(account));

	return 1;
}

static int gaim_parse_buddyrights(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	guint16 maxbuddies, maxwatchers;

	va_start(ap, fr);
	maxbuddies = (guint16) va_arg(ap, unsigned int);
	maxwatchers = (guint16) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "buddy list rights: Max buddies = %hu / Max watchers = %hu\n", maxbuddies, maxwatchers);

	od->rights.maxbuddies = (guint)maxbuddies;
	od->rights.maxwatchers = (guint)maxwatchers;

	return 1;
}

static int gaim_bosrights(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc;
	GaimAccount *account;
	GaimStatus *status;
	const char *message;
	char *tmp;
	va_list ap;
	guint16 maxpermits, maxdenies;

	gc = od->gc;
	od = (OscarData *)gc->proto_data;
	account = gaim_connection_get_account(gc);

	va_start(ap, fr);
	maxpermits = (guint16) va_arg(ap, unsigned int);
	maxdenies = (guint16) va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_misc("oscar",
			   "BOS rights: Max permit = %hu / Max deny = %hu\n", maxpermits, maxdenies);

	od->rights.maxpermits = (guint)maxpermits;
	od->rights.maxdenies = (guint)maxdenies;

	gaim_connection_set_state(gc, GAIM_CONNECTED);

	gaim_debug_info("oscar", "buddy list loaded\n");

	aim_clientready(od, conn);

	if (gaim_account_get_user_info(account) != NULL)
		serv_set_info(gc, gaim_account_get_user_info(account));

	/* Set our available message based on the current status */
	status = gaim_account_get_active_status(account);
	if (gaim_status_is_available(status))
		message = gaim_status_get_attr_string(status, "message");
	else
		message = NULL;
	tmp = gaim_markup_strip_html(message);
	aim_srv_setstatusmsg(od, tmp);
	g_free(tmp);

	aim_srv_setidle(od, 0);

	if (od->icq) {
		aim_icq_reqofflinemsgs(od);
		oscar_set_extendedstatus(gc);
		aim_icq_setsecurity(od,
			gaim_account_get_bool(account, "authorization", OSCAR_DEFAULT_AUTHORIZATION),
			gaim_account_get_bool(account, "web_aware", OSCAR_DEFAULT_WEB_AWARE));
	}

	aim_srv_requestnew(od, SNAC_FAMILY_CHATNAV);

	/*
	 * The "if" statement here is a pathetic attempt to not attempt to
	 * connect to the alerts servce (aka email notification) if this
	 * screen name does not support it.  I think mail notification
	 * works for @mac.com accounts but does not work for the newer
	 * @anythingelse.com accounts.  If that's true then this change
	 * breaks mail notification for @mac.com accounts, but it gets rid
	 * of an annoying error at signon for @anythingelse.com accounts.
	 */
	if ((od->authinfo->email != NULL) && ((strchr(gc->account->username, '@') == NULL)))
		aim_srv_requestnew(od, SNAC_FAMILY_ALERT);

	return 1;
}

static int gaim_offlinemsg(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
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
	incomingim_chan4(od, conn, NULL, &args, t);

	return 1;
}

static int gaim_offlinemsgdone(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	aim_icq_ackofflinemsgs(od);
	return 1;
}

static int gaim_icqinfo(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc;
	GaimAccount *account;
	GaimBuddy *buddy;
	struct buddyinfo *bi;
	gchar who[16];
	GaimNotifyUserInfo *user_info;
	GString *tmp;
	gchar *utf8;
	gchar *buf;
	const gchar *alias;
	va_list ap;
	struct aim_icq_info *info;

	gc = od->gc;
	account = gaim_connection_get_account(gc);

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (!info->uin)
		return 0;

	user_info = gaim_notify_user_info_new();
		
	g_snprintf(who, sizeof(who), "%u", info->uin);
	buddy = gaim_find_buddy(gaim_connection_get_account(gc), who);
	if (buddy != NULL)
		bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(buddy->account, buddy->name));
	else
		bi = NULL;

	gaim_notify_user_info_add_pair(user_info, _("UIN"), who);
	oscar_user_info_convert_and_add(account, user_info, _("Nick"), info->nick);
	if ((bi != NULL) && (bi->ipaddr != 0)) {
		char *tstr =  g_strdup_printf("%hhu.%hhu.%hhu.%hhu",
						(bi->ipaddr & 0xff000000) >> 24,
						(bi->ipaddr & 0x00ff0000) >> 16,
						(bi->ipaddr & 0x0000ff00) >> 8,
						(bi->ipaddr & 0x000000ff));
		gaim_notify_user_info_add_pair(user_info, _("IP Address"), tstr);
		g_free(tstr);
	}
	oscar_user_info_convert_and_add(account, user_info, _("First Name"), info->first);
	oscar_user_info_convert_and_add(account, user_info, _("Last Name"), info->last);
	if (info->email && info->email[0] && (utf8 = oscar_utf8_try_convert(gc->account, info->email))) {
		buf = g_strdup_printf("<a href=\"mailto:%s\">%s</a>", utf8, utf8);
		gaim_notify_user_info_add_pair(user_info, _("E-Mail Address"), buf);
		g_free(buf);
		g_free(utf8);
	}
	if (info->numaddresses && info->email2) {
		int i;
		for (i = 0; i < info->numaddresses; i++) {
			if (info->email2[i] && info->email2[i][0] && (utf8 = oscar_utf8_try_convert(gc->account, info->email2[i]))) {
				buf = g_strdup_printf("<a href=\"mailto:%s\">%s</a>", utf8, utf8);
				gaim_notify_user_info_add_pair(user_info, _("E-Mail Address"), buf);
				g_free(buf);
				g_free(utf8);
			}
		}
	}
	oscar_user_info_convert_and_add(account, user_info, _("Mobile Phone"), info->mobile);

	if (info->gender != 0)
		gaim_notify_user_info_add_pair(user_info, _("Gender"), (info->gender == 1 ? _("Female") : _("Male")));

	if ((info->birthyear > 1900) && (info->birthmonth > 0) && (info->birthday > 0)) {
		/* Initialize the struct properly or strftime() will crash
		 * under some conditions (e.g. Debian sarge w/ LANG=en_HK). */
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);

		tm->tm_mday = (int)info->birthday;
		tm->tm_mon  = (int)info->birthmonth - 1;
		tm->tm_year = (int)info->birthyear - 1900;

		/* To be 100% sure that the fields are re-normalized.
		 * If you're sure strftime() ALWAYS does this EVERYWHERE,
		 * feel free to remove it.  --rlaager */
		mktime(tm);

		oscar_user_info_convert_and_add(account, user_info, _("Birthday"), gaim_date_format_short(tm));
	}
	if ((info->age > 0) && (info->age < 255)) {
		char age[5];
		snprintf(age, sizeof(age), "%hhd", info->age);
		gaim_notify_user_info_add_pair(user_info,
													_("Age"), age);
	}
	if (info->personalwebpage && info->personalwebpage[0] && (utf8 = oscar_utf8_try_convert(gc->account, info->personalwebpage))) {
		buf = g_strdup_printf("<a href=\"%s\">%s</a>", utf8, utf8);
		gaim_notify_user_info_add_pair(user_info, _("Personal Web Page"), buf);
		g_free(buf);
		g_free(utf8);
	}
	
	oscar_user_info_convert_and_add(account, user_info, _("Additional Information"), info->info);
	gaim_notify_user_info_add_section_break(user_info);

	if ((info->homeaddr && (info->homeaddr[0])) || (info->homecity && info->homecity[0]) || (info->homestate && info->homestate[0]) || (info->homezip && info->homezip[0])) {
		tmp = g_string_sized_new(100);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Address"), info->homeaddr);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("City"), info->homecity);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("State"), info->homestate);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Zip Code"), info->homezip);
		
		gaim_notify_user_info_add_pair(user_info, _("Home Address"), tmp->str);
		gaim_notify_user_info_add_section_break(user_info);

		g_string_free(tmp, TRUE);
	}
	if ((info->workaddr && info->workaddr[0]) || (info->workcity && info->workcity[0]) || (info->workstate && info->workstate[0]) || (info->workzip && info->workzip[0])) {
		tmp = g_string_sized_new(100);

		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Address"), info->workaddr);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("City"), info->workcity);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("State"), info->workstate);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Zip Code"), info->workzip);

		gaim_notify_user_info_add_pair(user_info, _("Work Address"), tmp->str);
		gaim_notify_user_info_add_section_break(user_info);

		g_string_free(tmp, TRUE);
	}
	if ((info->workcompany && info->workcompany[0]) || (info->workdivision && info->workdivision[0]) || (info->workposition && info->workposition[0]) || (info->workwebpage && info->workwebpage[0])) {
		tmp = g_string_sized_new(100);
		
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Company"), info->workcompany);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Division"), info->workdivision);
		oscar_string_convert_and_append(account, tmp, "\n<br>", _("Position"), info->workposition);
		if (info->workwebpage && info->workwebpage[0] && (utf8 = oscar_utf8_try_convert(gc->account, info->workwebpage))) {
			g_string_append_printf(tmp, "\n<br><b>%s:</b> <a href=\"%s\">%s</a>", _("Web Page"), utf8, utf8);
			g_free(utf8);
		}
		gaim_notify_user_info_add_pair(user_info, _("Work Information"), tmp->str);
		g_string_free(tmp, TRUE);
	}

	if (buddy != NULL)
		alias = gaim_buddy_get_alias(buddy);
	else
		alias = who;
	gaim_notify_userinfo(gc, who, user_info, NULL, NULL);
	gaim_notify_user_info_destroy(user_info);

	return 1;
}

static int gaim_icqalias(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimAccount *account = gaim_connection_get_account(gc);
	gchar who[16], *utf8;
	GaimBuddy *b;
	va_list ap;
	struct aim_icq_info *info;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (info->uin && info->nick && info->nick[0] && (utf8 = oscar_utf8_try_convert(account, info->nick))) {
		g_snprintf(who, sizeof(who), "%u", info->uin);
		serv_got_alias(gc, who, utf8);
		if ((b = gaim_find_buddy(gc->account, who))) {
			gaim_blist_node_set_string((GaimBlistNode*)b, "servernick", utf8);
		}
		g_free(utf8);
	}

	return 1;
}

static int gaim_popup(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	gchar *text;
	va_list ap;
	char *msg, *url;
	guint16 wid, hei, delay;

	va_start(ap, fr);
	msg = va_arg(ap, char *);
	url = va_arg(ap, char *);
	wid = (guint16) va_arg(ap, int);
	hei = (guint16) va_arg(ap, int);
	delay = (guint16) va_arg(ap, int);
	va_end(ap);

	text = g_strdup_printf("%s<br><a href=\"%s\">%s</a>", msg, url, url);
	gaim_notify_formatted(gc, NULL, _("Pop-Up Message"), NULL, text, NULL, NULL);
	g_free(text);

	return 1;
}

static void oscar_searchresults_add_buddy_cb(GaimConnection *gc, GList *row, void *user_data)
{
	gaim_blist_request_add_buddy(gaim_connection_get_account(gc),
								 g_list_nth_data(row, 0), NULL, NULL);
}

static int gaim_parse_searchreply(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc = od->gc;
	GaimNotifySearchResults *results;
	GaimNotifySearchColumn *column;
	gchar *secondary;
	int i, num;
	va_list ap;
	char *email, *SNs;

	va_start(ap, fr);
	email = va_arg(ap, char *);
	num = va_arg(ap, int);
	SNs = va_arg(ap, char *);
	va_end(ap);

	results = gaim_notify_searchresults_new();

	if (results == NULL) {
		gaim_debug_error("oscar", "gaim_parse_searchreply: "
						 "Unable to display the search results.\n");
		gaim_notify_error(gc, NULL,
						  _("Unable to display the search results."),
						  NULL);
		return 1;
	}

	secondary = g_strdup_printf(
					ngettext("The following screen name is associated with %s",
						 "The following screen names are associated with %s",
						 num),
					email);

	column = gaim_notify_searchresults_column_new(_("Screen name"));
	gaim_notify_searchresults_column_add(results, column);

	for (i = 0; i < num; i++) {
		GList *row = NULL;
		row = g_list_append(row, g_strdup(&SNs[i * (MAXSNLEN + 1)]));
		gaim_notify_searchresults_row_add(results, row);
	}
	gaim_notify_searchresults_button_add(results, GAIM_NOTIFY_BUTTON_ADD,
										 oscar_searchresults_add_buddy_cb);
	gaim_notify_searchresults(gc, NULL, NULL, secondary, results, NULL, NULL);

	g_free(secondary);

	return 1;
}

static int gaim_parse_searcherror(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	va_list ap;
	char *email;
	char *buf;

	va_start(ap, fr);
	email = va_arg(ap, char *);
	va_end(ap);

	buf = g_strdup_printf(_("No results found for e-mail address %s"), email);
	gaim_notify_error(od->gc, NULL, buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_account_confirm(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	guint16 status;
	va_list ap;
	char msg[256];

	va_start(ap, fr);
	status = (guint16) va_arg(ap, unsigned int); /* status code of confirmation request */
	va_end(ap);

	gaim_debug_info("oscar",
			   "account confirmation returned status 0x%04x (%s)\n", status,
			status ? "unknown" : "e-mail sent");
	if (!status) {
		g_snprintf(msg, sizeof(msg), _("You should receive an e-mail asking to confirm %s."),
				gaim_account_get_username(gaim_connection_get_account(gc)));
		gaim_notify_info(gc, NULL, _("Account Confirmation Requested"), msg);
	}

	return 1;
}

static int gaim_info_change(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	guint16 perms, err;
	char *url, *sn, *email;
	int change;

	va_start(ap, fr);
	change = va_arg(ap, int);
	perms = (guint16) va_arg(ap, unsigned int);
	err = (guint16) va_arg(ap, unsigned int);
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
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because it is invalid."), err);
			} break;
			case 0x000b: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name is too long."), err);
			} break;
			case 0x001d: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change e-mail address because there is already a request pending for this screen name."), err);
			} break;
			case 0x0021: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change e-mail address because the given address has too many screen names associated with it."), err);
			} break;
			case 0x0023: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change e-mail address because the given address is invalid."), err);
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
		char *dialog_msg = g_strdup_printf(_("The e-mail address for %s is %s"),
						   gaim_account_get_username(gaim_connection_get_account(gc)), email);
		gaim_notify_info(gc, NULL, _("Account Info"), dialog_msg);
		g_free(dialog_msg);
	}

	return 1;
}

void
oscar_keepalive(GaimConnection *gc)
{
	OscarData *od;
	FlapConnection *conn;

	od = (OscarData *)gc->proto_data;
	conn = flap_connection_getbytype(od, SNAC_FAMILY_LOCATE);
	if (conn != NULL)
		flap_connection_send_keepalive(od, conn);
}

unsigned int
oscar_send_typing(GaimConnection *gc, const char *name, GaimTypingState state)
{
	OscarData *od;
	PeerConnection *conn;

	od = (OscarData *)gc->proto_data;
	conn = peer_connection_find_by_type(od, name, OSCAR_CAPABILITY_DIRECTIM);

	if ((conn != NULL) && (conn->ready))
	{
		peer_odc_send_typing(conn, state);
	}
	else {
		/* Don't send if this turkey is in our deny list */
		GSList *list;
		for (list=gc->account->deny; (list && aim_sncmp(name, list->data)); list=list->next);
		if (!list) {
			struct buddyinfo *bi = g_hash_table_lookup(od->buddyinfo, gaim_normalize(gc->account, name));
			if (bi && bi->typingnot) {
				if (state == GAIM_TYPING)
					aim_im_sendmtn(od, 0x0001, name, 0x0002);
				else if (state == GAIM_TYPED)
					aim_im_sendmtn(od, 0x0001, name, 0x0001);
				else
					aim_im_sendmtn(od, 0x0001, name, 0x0000);
			}
		}
	}
	return 0;
}

/* TODO: Move this into odc.c! */
static void
gaim_odc_send_im(PeerConnection *conn, const char *message, GaimMessageFlags imflags)
{
	GString *msg;
	GString *data;
	gchar *tmp;
	int tmplen;
	guint16 charset, charsubset;
	GData *attribs;
	const char *start, *end, *last;
	int oscar_id = 0;

	msg = g_string_new("<HTML><BODY>");
	data = g_string_new("<BINARY>");
	last = message;

	/* for each valid IMG tag... */
	while (last && *last && gaim_markup_find_tag("img", last, &start, &end, &attribs))
	{
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
			g_string_append_len(data, imgdata, size);
			g_string_append(data, "</DATA>");
		}
			/* If the tag is invalid, skip it, thus no else here */

		g_datalist_clear(&attribs);

		/* continue from the end of the tag */
		last = end + 1;
	}

	/* append any remaining message data */
	if (last && *last)
		g_string_append(msg, last);

	g_string_append(msg, "</BODY></HTML>");

	/* Convert the message to a good encoding */
	gaim_plugin_oscar_convert_to_best_encoding(conn->od->gc,
			conn->sn, msg->str, &tmp, &tmplen, &charset, &charsubset);
	g_string_free(msg, TRUE);
	msg = g_string_new_len(tmp, tmplen);

	/* Append any binary data that we may have */
	if (oscar_id) {
		msg = g_string_append_len(msg, data->str, data->len);
		msg = g_string_append(msg, "</BINARY>");
	}
	g_string_free(data, TRUE);

	peer_odc_send_im(conn, msg->str, msg->len, charset,
			imflags & GAIM_MESSAGE_AUTO_RESP);
	g_string_free(msg, TRUE);
}

int
oscar_send_im(GaimConnection *gc, const char *name, const char *message, GaimMessageFlags imflags)
{
	OscarData *od;
	GaimAccount *account;
	PeerConnection *conn;
	int ret;
	char *iconfile;
	char *tmp1, *tmp2;

	od = (OscarData *)gc->proto_data;
	account = gaim_connection_get_account(gc);
	ret = 0;
	iconfile = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(account));

	if (imflags & GAIM_MESSAGE_AUTO_RESP)
		tmp1 = gaim_str_sub_away_formatters(message, name);
	else
		tmp1 = g_strdup(message);

	conn = peer_connection_find_by_type(od, name, OSCAR_CAPABILITY_DIRECTIM);
	if ((conn != NULL) && (conn->ready))
	{
		/* If we're directly connected, send a direct IM */
		gaim_odc_send_im(conn, tmp1, imflags);
	} else {
		struct buddyinfo *bi;
		struct aim_sendimext_args args;
		struct stat st;
		gsize len;
		GaimConversation *conv;

		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, account);

		if (strstr(tmp1, "<IMG "))
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
			/* We have to present different "features" (whose meaning
			   is unclear and are merely a result of protocol inspection)
			   to offline ICQ buddies. Otherwise, the official
			   ICQ client doesn't treat those messages as being "ANSI-
			   encoded" (and instead, assumes them to be UTF-8).
			   For more details, see SF issue 1179452.
			*/
			GaimBuddy *buddy = gaim_find_buddy(gc->account, name);
			if (buddy && GAIM_BUDDY_IS_ONLINE(buddy)) {
				args.features = features_icq;
				args.featureslen = sizeof(features_icq);
			} else {
				args.features = features_icq_offline;
				args.featureslen = sizeof(features_icq_offline);
			}
			args.flags |= AIM_IMFLAGS_OFFLINE;
		} else {
			args.features = features_aim;
			args.featureslen = sizeof(features_aim);

			if (imflags & GAIM_MESSAGE_AUTO_RESP)
				args.flags |= AIM_IMFLAGS_AWAY;
		}

		if (bi->ico_need) {
			gaim_debug_info("oscar",
					   "Sending buddy icon request with message\n");
			args.flags |= AIM_IMFLAGS_BUDDYREQ;
			bi->ico_need = FALSE;
		}

		if (iconfile && !g_stat(iconfile, &st)) {
			FILE *file = g_fopen(iconfile, "rb");
			if (file) {
				guchar *buf = g_malloc(st.st_size);
				/* TODO: Use g_file_get_contents()? */
				fread(buf, 1, st.st_size, file);
				fclose(file);

				args.iconlen   = st.st_size;
				args.iconsum   = aimutil_iconsum(buf, st.st_size);
				args.iconstamp = st.st_mtime;

				if ((args.iconlen != bi->ico_me_len) || (args.iconsum != bi->ico_me_csum) || (args.iconstamp != bi->ico_me_time)) {
					bi->ico_informed = FALSE;
					bi->ico_sent     = FALSE;
				}

				/*
				 * TODO:
				 * For some reason sending our icon to people only works
				 * when we're the ones who initiated the conversation.  If
				 * the other person sends the first IM then they never get
				 * the icon.  We should fix that.
				 */
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
		g_free(iconfile);

		args.destsn = name;

		/*
		 * If we're IMing an SMS user or an ICQ user from an ICQ account, then strip HTML.
		 */
		if (aim_sn_is_sms(name)) {
			/* Messaging an SMS (mobile) user */
			tmp2 = gaim_unescape_html(tmp1);
		} else if (aim_sn_is_icq(gaim_account_get_username(account))) {
			if (aim_sn_is_icq(name))
				/* From ICQ to ICQ */
				tmp2 = gaim_unescape_html(tmp1);
			else
				/* From ICQ to AIM */
				tmp2 = g_strdup(tmp1);
		} else {
			/* From AIM to AIM and AIM to ICQ */
			tmp2 = g_strdup(tmp1);
		}
		g_free(tmp1);
		tmp1 = tmp2;
		len = strlen(tmp1);

		gaim_plugin_oscar_convert_to_best_encoding(gc, name, tmp1, (char **)&args.msg, &args.msglen, &args.charset, &args.charsubset);
		gaim_debug_info("oscar", "Sending IM, charset=0x%04hx, charsubset=0x%04hx, length=%d\n",
						args.charset, args.charsubset, args.msglen);
		ret = aim_im_sendch1_ext(od, &args);
		g_free((char *)args.msg);
	}

	g_free(tmp1);

	if (ret >= 0)
		return 1;

	return ret;
}

/*
 * As of 26 June 2006, ICQ users can request AIM info from
 * everyone, and can request ICQ info from ICQ users, and
 * AIM users can only request AIM info.
 */
void oscar_get_info(GaimConnection *gc, const char *name) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->icq && aim_sn_is_icq(name))
		aim_icq_getallinfo(od, name);
	else
		aim_locate_getinfoshort(od, name, 0x00000003);
}

#if 0
static void oscar_set_dir(GaimConnection *gc, const char *first, const char *middle, const char *last,
			  const char *maiden, const char *city, const char *state, const char *country, int web) {
	/* XXX - some of these things are wrong, but i'm lazy */
	OscarData *od = (OscarData *)gc->proto_data;
	aim_locate_setdirinfo(od, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
}
#endif

void oscar_set_idle(GaimConnection *gc, int time) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_srv_setidle(od, time);
}

static
gchar *gaim_prpl_oscar_convert_to_infotext(const gchar *str, gsize *ret_len, char **encoding)
{
	int charset = 0;
	char *encoded = NULL;

	charset = oscar_charset_check(str);
	if (charset == AIM_CHARSET_UNICODE) {
		encoded = g_convert(str, strlen(str), "UCS-2BE", "UTF-8", NULL, ret_len, NULL);
		*encoding = "unicode-2-0";
	} else if (charset == AIM_CHARSET_CUSTOM) {
		encoded = g_convert(str, strlen(str), "ISO-8859-1", "UTF-8", NULL, ret_len, NULL);
		*encoding = "iso-8859-1";
	} else {
		encoded = g_strdup(str);
		*ret_len = strlen(str);
		*encoding = "us-ascii";
	}

	return encoded;
}

void
oscar_set_info(GaimConnection *gc, const char *rawinfo)
{
	GaimAccount *account;
	GaimStatus *status;

	account = gaim_connection_get_account(gc);
	status = gaim_account_get_active_status(account);
	oscar_set_info_and_status(account, TRUE, rawinfo, FALSE, status);
}

static void
oscar_set_extendedstatus(GaimConnection *gc)
{
	OscarData *od;
	GaimAccount *account;
	GaimStatus *status;
	const gchar *status_id;
	guint32 data = 0x00000000;

	od = gc->proto_data;
	account = gaim_connection_get_account(gc);
	status = gaim_account_get_active_status(account);
	status_id = gaim_status_get_id(status);

	data |= AIM_ICQ_STATE_HIDEIP;
	if (gaim_account_get_bool(account, "web_aware", OSCAR_DEFAULT_WEB_AWARE))
		data |= AIM_ICQ_STATE_WEBAWARE;

	if (!strcmp(status_id, OSCAR_STATUS_ID_AVAILABLE) || !strcmp(status_id, OSCAR_STATUS_ID_AVAILABLE))
		data |= AIM_ICQ_STATE_NORMAL;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_AWAY))
		data |= AIM_ICQ_STATE_AWAY;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_DND))
		data |= AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_DND | AIM_ICQ_STATE_BUSY;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_NA))
		data |= AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_OCCUPIED))
		data |= AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_BUSY;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_FREE4CHAT))
		data |= AIM_ICQ_STATE_CHAT;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_INVISIBLE))
		data |= AIM_ICQ_STATE_INVISIBLE;
	else if (!strcmp(status_id, OSCAR_STATUS_ID_CUSTOM))
		data |= AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY;

	aim_srv_setextstatus(od, data);
}

static void
oscar_set_info_and_status(GaimAccount *account, gboolean setinfo, const char *rawinfo,
						  gboolean setstatus, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	OscarData *od = gc->proto_data;
	GaimPresence *presence;
	GaimStatusType *status_type;
	GaimStatusPrimitive primitive;
	gboolean invisible;

	char *htmlinfo;
	char *info_encoding = NULL;
	char *info = NULL;
	gsize infolen = 0;

	const char *htmlaway;
	char *away_encoding = NULL;
	char *away = NULL;
	gsize awaylen = 0;

	status_type = gaim_status_get_type(status);
	primitive = gaim_status_type_get_primitive(status_type);
	presence = gaim_account_get_presence(account);
	invisible = gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_INVISIBLE);

	if (!setinfo)
	{
		/* Do nothing! */
	}
	else if (od->rights.maxsiglen == 0)
	{
		gaim_notify_warning(gc, NULL, _("Unable to set AIM profile."),
							_("You have probably requested to set your "
							  "profile before the login procedure completed.  "
							  "Your profile remains unset; try setting it "
							  "again when you are fully connected."));
	}
	else if (rawinfo != NULL)
	{
		htmlinfo = gaim_strdup_withhtml(rawinfo);
		info = gaim_prpl_oscar_convert_to_infotext(htmlinfo, &infolen, &info_encoding);
		g_free(htmlinfo);

		if (infolen > od->rights.maxsiglen)
		{
			gchar *errstr;
			errstr = g_strdup_printf(ngettext("The maximum profile length of %d byte "
									 "has been exceeded.  Gaim has truncated it for you.",
									 "The maximum profile length of %d bytes "
									 "has been exceeded.  Gaim has truncated it for you.",
									 od->rights.maxsiglen), od->rights.maxsiglen);
			gaim_notify_warning(gc, NULL, _("Profile too long."), errstr);
			g_free(errstr);
		}
	}

	if (!setstatus)
	{
		/* Do nothing! */
	}
	else if (primitive == GAIM_STATUS_AVAILABLE)
	{
		const char *status_html;
		char *status_text = NULL;

		status_html = gaim_status_get_attr_string(status, "message");
		if (status_html != NULL)
		{
			status_text = gaim_markup_strip_html(status_html);
			/* If the status_text is longer than 60 character then truncate it */
			if (strlen(status_text) > 60)
			{
				char *tmp = g_utf8_find_prev_char(status_text, &status_text[58]);
				strcpy(tmp, "...");
			}
		}

		aim_srv_setstatusmsg(od, status_text);
		g_free(status_text);

		/* This is needed for us to un-set any previous away message. */
		away = g_strdup("");
	}
	else if ((primitive == GAIM_STATUS_AWAY) ||
			 (primitive == GAIM_STATUS_EXTENDED_AWAY))
	{
		htmlaway = gaim_status_get_attr_string(status, "message");
		if ((htmlaway == NULL) || (*htmlaway == '\0'))
			htmlaway = _("Away");
		away = gaim_prpl_oscar_convert_to_infotext(htmlaway, &awaylen, &away_encoding);

		if (awaylen > od->rights.maxawaymsglen)
		{
			gchar *errstr;

			errstr = g_strdup_printf(ngettext("The maximum away message length of %d byte "
									 "has been exceeded.  Gaim has truncated it for you.",
									 "The maximum away message length of %d bytes "
									 "has been exceeded.  Gaim has truncated it for you.",
									 od->rights.maxawaymsglen), od->rights.maxawaymsglen);
			gaim_notify_warning(gc, NULL, _("Away message too long."), errstr);
			g_free(errstr);
		}
	}

	if (setstatus)
		oscar_set_extendedstatus(gc);

	aim_locate_setprofile(od, info_encoding, info, MIN(infolen, od->rights.maxsiglen),
									away_encoding, away, MIN(awaylen, od->rights.maxawaymsglen));
	g_free(info);
	g_free(away);
}

static void
oscar_set_status_icq(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	OscarData *od = NULL;

	if (gc)
		od = (OscarData *)gc->proto_data;
	if (!od)
		return;

	if (gaim_status_type_get_primitive(gaim_status_get_type(status)) == GAIM_STATUS_INVISIBLE)
		account->perm_deny = GAIM_PRIVACY_ALLOW_USERS;
	else
		account->perm_deny = GAIM_PRIVACY_DENY_USERS;

	if ((od->ssi.received_data) && (aim_ssi_getpermdeny(od->ssi.local) != account->perm_deny))
		aim_ssi_setpermdeny(od, account->perm_deny, 0xffffffff);

	oscar_set_extendedstatus(gc);
}

void
oscar_set_status(GaimAccount *account, GaimStatus *status)
{
	gaim_debug_info("oscar", "Set status to %s\n", gaim_status_get_name(status));

	if (!gaim_status_is_active(status))
		return;

	if (!gaim_account_is_connected(account))
		return;

	/* Set the AIM-style away message for both AIM and ICQ accounts */
	oscar_set_info_and_status(account, FALSE, NULL, TRUE, status);

	/* Set the ICQ status for ICQ accounts only */
	if (aim_sn_is_icq(gaim_account_get_username(account)))
		oscar_set_status_icq(account, status);
}

#ifdef CRAZY_WARN
void
oscar_warn(GaimConnection *gc, const char *name, gboolean anonymous) {
	OscarData *od = (OscarData *)gc->proto_data;
	aim_im_warn(od, od->conn, name, anonymous ? AIM_WARN_ANON : 0);
}
#endif

void
oscar_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {
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

	if ((od->ssi.received_data) && !(aim_ssi_itemlist_finditem(od->ssi.local, group->name, buddy->name, AIM_SSI_TYPE_BUDDY))) {
		gaim_debug_info("oscar",
				   "ssi: adding buddy %s to group %s\n", buddy->name, group->name);
		aim_ssi_addbuddy(od, buddy->name, group->name, gaim_buddy_get_alias_only(buddy), NULL, NULL, 0);
	}

	/* XXX - Should this be done from AIM accounts, as well? */
	if (od->icq)
		aim_icq_getalias(od, buddy->name);
}

void oscar_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->ssi.received_data) {
		gaim_debug_info("oscar",
				   "ssi: deleting buddy %s from group %s\n", buddy->name, group->name);
		aim_ssi_delbuddy(od, buddy->name, group->name);
	}
}

void oscar_move_buddy(GaimConnection *gc, const char *name, const char *old_group, const char *new_group) {
	OscarData *od = (OscarData *)gc->proto_data;
	if (od->ssi.received_data && strcmp(old_group, new_group)) {
		gaim_debug_info("oscar",
				   "ssi: moving buddy %s from group %s to group %s\n", name, old_group, new_group);
		aim_ssi_movebuddy(od, old_group, new_group, name);
	}
}

void oscar_alias_buddy(GaimConnection *gc, const char *name, const char *alias) {
	OscarData *od = (OscarData *)gc->proto_data;
	if (od->ssi.received_data) {
		char *gname = aim_ssi_itemlist_findparentname(od->ssi.local, name);
		if (gname) {
			gaim_debug_info("oscar",
					   "ssi: changing the alias for buddy %s to %s\n", name, alias ? alias : "(none)");
			aim_ssi_aliasbuddy(od, gname, name, alias);
		}
	}
}

/*
 * FYI, the OSCAR SSI code removes empty groups automatically.
 */
void oscar_rename_group(GaimConnection *gc, const char *old_name, GaimGroup *group, GList *moved_buddies) {
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->ssi.received_data) {
		if (aim_ssi_itemlist_finditem(od->ssi.local, group->name, NULL, AIM_SSI_TYPE_GROUP)) {
			GList *cur, *groups = NULL;
			GaimAccount *account = gaim_connection_get_account(gc);

			/* Make a list of what the groups each buddy is in */
			for (cur = moved_buddies; cur != NULL; cur = cur->next) {
				GaimBlistNode *node = cur->data;
				/* node is GaimBuddy, parent is a GaimContact.
				 * We must go two levels up to get the Group */
				groups = g_list_append(groups,
						node->parent->parent);
			}

			gaim_account_remove_buddies(account, moved_buddies, groups);
			gaim_account_add_buddies(account, moved_buddies);
			g_list_free(groups);
			gaim_debug_info("oscar",
					   "ssi: moved all buddies from group %s to %s\n", old_name, group->name);
		} else {
			aim_ssi_rename_group(od, old_name, group->name);
			gaim_debug_info("oscar",
					   "ssi: renamed group %s to %s\n", old_name, group->name);
		}
	}
}

static gboolean gaim_ssi_rerequestdata(gpointer data) {
	OscarData *od = data;

	aim_ssi_reqdata(od);

	return TRUE;
}

static int gaim_ssi_parseerr(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	guint16 reason;

	va_start(ap, fr);
	reason = (guint16)va_arg(ap, unsigned int);
	va_end(ap);

	gaim_debug_error("oscar", "ssi: SNAC error %hu\n", reason);

	if (reason == 0x0005) {
		gaim_notify_error(gc, NULL, _("Unable To Retrieve Buddy List"),
						  _("Gaim was temporarily unable to retrieve your buddy list from the AIM servers.  Your buddy list is not lost, and will probably become available in a few hours."));
		if (od->getblisttimer > 0)
			gaim_timeout_remove(od->getblisttimer);
		od->getblisttimer = gaim_timeout_add(30000, gaim_ssi_rerequestdata, od);
	}

	oscar_set_extendedstatus(gc);

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	gaim_debug_info("oscar", "ssi: activating server-stored buddy list\n");
	aim_ssi_enable(od);

	return 1;
}

static int gaim_ssi_parserights(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	int i;
	va_list ap;
	int numtypes;
	guint16 *maxitems;

	va_start(ap, fr);
	numtypes = va_arg(ap, int);
	maxitems = va_arg(ap, guint16 *);
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

static int gaim_ssi_parselist(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...)
{
	GaimConnection *gc;
	GaimAccount *account;
	GaimGroup *g;
	GaimBuddy *b;
	struct aim_ssi_item *curitem;
	guint32 tmp;
	const char *icon_path, *cached_icon_path;
	va_list ap;
	guint16 fmtver, numitems;
	guint32 timestamp;

	gc = od->gc;
	od = gc->proto_data;
	account = gaim_connection_get_account(gc);

	va_start(ap, fr);
	fmtver = (guint16)va_arg(ap, int);
	numitems = (guint16)va_arg(ap, int);
	timestamp = va_arg(ap, guint32);
	va_end(ap);

	/* Don't attempt to re-request our buddy list later */
	if (od->getblisttimer != 0)
		gaim_timeout_remove(od->getblisttimer);
	od->getblisttimer = 0;

	gaim_debug_info("oscar",
			   "ssi: syncing local list and server list\n");

	if ((timestamp == 0) || (numitems == 0)) {
		gaim_debug_info("oscar", "Got AIM SSI with a 0 timestamp or 0 numitems--not syncing.  This probably means your buddy list is empty.", NULL);
		return 1;
	}

	/* Clean the buddy list */
	aim_ssi_cleanlist(od);

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
							if (aim_ssi_itemlist_exists(od->ssi.local, b->name)) {
								/* If the buddy is an ICQ user then load his nickname */
								const char *servernick = gaim_blist_node_get_string((GaimBlistNode*)b, "servernick");
								char *alias;
								if (servernick)
									serv_got_alias(gc, b->name, servernick);

								/* Store local alias on server */
								alias = aim_ssi_getalias(od->ssi.local, g->name, b->name);
								if (!alias && b->alias && strlen(b->alias))
									aim_ssi_aliasbuddy(od, g->name, b->name, b->alias);
								g_free(alias);
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
				if (!aim_ssi_itemlist_finditem(od->ssi.local, NULL, cur->data, AIM_SSI_TYPE_PERMIT)) {
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
				if (!aim_ssi_itemlist_finditem(od->ssi.local, NULL, cur->data, AIM_SSI_TYPE_DENY)) {
					gaim_debug_info("oscar",
							"ssi: removing deny %s from local list\n", (const char *)cur->data);
					gaim_privacy_deny_remove(account, cur->data, TRUE);
				}
			}
		}
		/* Presence settings (idle time visibility) */
		if ((tmp = aim_ssi_getpresence(od->ssi.local)) != 0xFFFFFFFF)
			if (!(tmp & 0x400))
				aim_ssi_setpresence(od, tmp | 0x400);
	} /* end pruning buddies from local list */

	/* Add from server list to local list */
	for (curitem=od->ssi.local; curitem; curitem=curitem->next) {
		if ((curitem->name == NULL) || (g_utf8_validate(curitem->name, -1, NULL)))
		switch (curitem->type) {
			case 0x0000: { /* Buddy */
				if (curitem->name) {
					char *gname = aim_ssi_itemlist_findparentname(od->ssi.local, curitem->name);
					char *gname_utf8 = gname ? oscar_utf8_try_convert(gc->account, gname) : NULL;
					char *alias = aim_ssi_getalias(od->ssi.local, gname, curitem->name);
					char *alias_utf8;

					if (alias != NULL)
					{
						if (g_utf8_validate(alias, -1, NULL))
							alias_utf8 = g_strdup(alias);
						else
							alias_utf8 = oscar_utf8_try_convert(account, alias);
					}
					else
						alias_utf8 = NULL;

					b = gaim_find_buddy(gc->account, curitem->name);
					/* Should gname be freed here? -- elb */
					/* Not with the current code, but that might be cleaner -- med */
					g_free(alias);
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
						char *comment = aim_ssi_getcomment(od->ssi.local, gname, curitem->name);
						if (comment != NULL)
						{
							gaim_check_comment(od, comment);
							g_free(comment);
						}
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
					guint8 permdeny;
					if ((permdeny = aim_ssi_getpermdeny(od->ssi.local)) && (permdeny != account->perm_deny)) {
						gaim_debug_info("oscar",
								   "ssi: changing permdeny from %d to %hhu\n", account->perm_deny, permdeny);
						account->perm_deny = permdeny;
						if (od->icq && account->perm_deny == GAIM_PRIVACY_ALLOW_USERS) {
							gaim_presence_set_status_active(account->presence, OSCAR_STATUS_ID_INVISIBLE, TRUE);
						}
					}
				}
			} break;

			case 0x0005: { /* Presence setting */
				/* We don't want to change Gaim's setting because it applies to all accounts */
			} break;
		} /* End of switch on curitem->type */
	} /* End of for loop */

	oscar_set_extendedstatus(gc);

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	gaim_debug_info("oscar",
			   "ssi: activating server-stored buddy list\n");
	aim_ssi_enable(od);

	/*
	 * Make sure our server-stored icon is updated correctly in
	 * the event that the local user set a new icon while this
	 * account was offline.
	 */
	icon_path = gaim_account_get_buddy_icon(account);
	cached_icon_path = gaim_buddy_icons_get_full_path(icon_path);
	oscar_set_icon(gc, cached_icon_path);

	return 1;
}

static int gaim_ssi_parseack(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
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
				if ((retval->action == SNAC_SUBTYPE_FEEDBAG_ADD) && (retval->name))
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

static int gaim_ssi_parseadd(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	char *gname, *gname_utf8, *alias, *alias_utf8;
	GaimBuddy *b;
	GaimGroup *g;
	va_list ap;
	guint16 type;
	const char *name;

	va_start(ap, fr);
	type = (guint16)va_arg(ap, int);
	name = va_arg(ap, char *);
	va_end(ap);

	if ((type != 0x0000) || (name == NULL))
		return 1;

	gname = aim_ssi_itemlist_findparentname(od->ssi.local, name);
	gname_utf8 = gname ? oscar_utf8_try_convert(gc->account, gname) : NULL;

	alias = aim_ssi_getalias(od->ssi.local, gname, name);
	if (alias != NULL)
	{
		if (g_utf8_validate(alias, -1, NULL))
			alias_utf8 = g_strdup(alias);
		else
			alias_utf8 = oscar_utf8_try_convert(gaim_connection_get_account(gc), alias);
	}
	else
		alias_utf8 = NULL;

	b = gaim_find_buddy(gc->account, name);
	g_free(alias);

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

static int gaim_ssi_authgiven(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
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

static int gaim_ssi_authrequest(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	char *sn;
	char *msg;
	GaimAccount *account = gaim_connection_get_account(gc);
	gchar *nombre;
	gchar *reason = NULL;
	struct name_data *data;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	gaim_debug_info("oscar",
			   "ssi: received authorization request from %s\n", sn);

	buddy = gaim_find_buddy(account, sn);
	if (buddy && (gaim_buddy_get_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_buddy_get_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	if (msg != NULL)
		reason = gaim_plugin_oscar_decode_im_part(account, sn, AIM_CHARSET_CUSTOM, 0x0000, msg, strlen(msg));

	data = g_new(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(sn);
	data->nick = NULL;

	gaim_account_request_authorization(account, nombre, NULL, NULL,
			reason, buddy != NULL, G_CALLBACK(gaim_auth_grant),
			G_CALLBACK(gaim_auth_dontgrant_msgprompt), data);
	g_free(nombre);
	g_free(reason);

	return 1;
}

static int gaim_ssi_authreply(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	guint8 reply;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	reply = (guint8)va_arg(ap, int);
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

static int gaim_ssi_gotadded(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	char *sn;
	GaimBuddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	va_end(ap);

	buddy = gaim_find_buddy(gc->account, sn);
	gaim_debug_info("oscar", "ssi: %s added you to their buddy list\n", sn);
	gaim_account_notify_added(gc->account, sn, NULL, (buddy ? gaim_buddy_get_alias_only(buddy) : NULL), NULL);

	return 1;
}

GList *oscar_chat_info(GaimConnection *gc) {
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Exchange:");
	pce->identifier = "exchange";
	pce->required = TRUE;
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

char *
oscar_get_chat_name(GHashTable *data)
{
	return g_strdup(g_hash_table_lookup(data, "room"));
}

void
oscar_join_chat(GaimConnection *gc, GHashTable *data)
{
	OscarData *od = (OscarData *)gc->proto_data;
	FlapConnection *conn;
	char *name, *exchange;

	name = g_hash_table_lookup(data, "room");
	exchange = g_hash_table_lookup(data, "exchange");

	if ((name == NULL) || (*name == '\0')) {
		gaim_notify_error(gc, NULL, _("Invalid chat name specified."), NULL);
		return;
	}

	gaim_debug_info("oscar", "Attempting to join chat room %s.\n", name);

	if ((conn = flap_connection_getbytype(od, SNAC_FAMILY_CHATNAV)))
	{
		gaim_debug_info("oscar", "chatnav exists, creating room\n");
		aim_chatnav_createroom(od, conn, name, atoi(exchange));
	} else {
		/* this gets tricky */
		struct create_room *cr = g_new0(struct create_room, 1);
		gaim_debug_info("oscar", "chatnav does not exist, opening chatnav\n");
		cr->exchange = atoi(exchange);
		cr->name = g_strdup(name);
		od->create_rooms = g_slist_prepend(od->create_rooms, cr);
		aim_srv_requestnew(od, SNAC_FAMILY_CHATNAV);
	}
}

void
oscar_chat_invite(GaimConnection *gc, int id, const char *message, const char *name)
{
	OscarData *od = (OscarData *)gc->proto_data;
	struct chat_connection *ccon = find_oscar_chat(gc, id);

	if (ccon == NULL)
		return;

	aim_im_sendch2_chatinvite(od, name, message ? message : "",
			ccon->exchange, ccon->name, 0x0);
}

void
oscar_chat_leave(GaimConnection *gc, int id)
{
	GaimConversation *conv;
	struct chat_connection *cc;

	conv = gaim_find_chat(gc, id);

	g_return_if_fail(conv != NULL);

	gaim_debug_info("oscar", "Leaving chat room %s\n", conv->name);

	cc = find_oscar_chat(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv)));
	oscar_chat_kill(gc, cc);
}

int oscar_send_chat(GaimConnection *gc, int id, const char *message, GaimMessageFlags flags) {
	OscarData *od = (OscarData *)gc->proto_data;
	GaimConversation *conv = NULL;
	struct chat_connection *c = NULL;
	char *buf, *buf2;
	guint16 charset, charsubset;
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
	/*
	 * Evan S. suggested that maxvis really does mean "number of
	 * visible characters" and not "number of bytes"
	 */
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
	aim_chat_send_im(od, c->conn, 0, buf2, len, charsetstr, "en");
	g_free(buf2);

	return 0;
}

const char *oscar_list_icon_icq(GaimAccount *a, GaimBuddy *b)
{
	if ((b == NULL) || (b->name == NULL) || aim_sn_is_sms(b->name))
	{
		if (a == NULL || aim_sn_is_icq(gaim_account_get_username(a)))
			return "icq";
		else
			return "aim";
	}

	if (aim_sn_is_icq(b->name))
		return "icq";
	return "aim";
}

const char *oscar_list_icon_aim(GaimAccount *a, GaimBuddy *b)
{
	if ((b == NULL) || (b->name == NULL) || aim_sn_is_sms(b->name))
	{
		if (a != NULL && aim_sn_is_icq(gaim_account_get_username(a)))
			return "icq";
		else
			return "aim";
	}

	if (aim_sn_is_icq(b->name))
		return "icq";
	return "aim";
}

void oscar_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
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

	account = b->account;
	if (account != NULL)
		gc = account->gc;
	if (gc != NULL)
		od = gc->proto_data;
	if (od != NULL)
		userinfo = aim_locate_finduserinfo(od, b->name);

	presence = gaim_buddy_get_presence(b);
	status = gaim_presence_get_active_status(presence);
	status_id = gaim_status_get_id(status);

	if (gaim_presence_is_online(presence) == FALSE) {
		char *gname;
		if ((b->name) && (od) && (od->ssi.received_data) &&
			(gname = aim_ssi_itemlist_findparentname(od->ssi.local, b->name)) &&
			(aim_ssi_waitingforauth(od->ssi.local, gname, b->name))) {
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
			emblems[i++] = "unavailable";
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
		if ((i < 4) && userinfo->flags & AIM_FLAG_ADMINISTRATOR)
			emblems[i++] = "admin";
		if ((i < 4) && userinfo->flags & AIM_FLAG_AOL)
			emblems[i++] = "aol";
		if ((i < 4) && userinfo->flags & AIM_FLAG_WIRELESS)
			emblems[i++] = "wireless";
		if ((i < 4) && userinfo->flags & AIM_FLAG_ACTIVEBUDDY)
			emblems[i++] = "activebuddy";

		if ((i < 4) && (userinfo->capabilities & OSCAR_CAPABILITY_HIPTOP))
			emblems[i++] = "hiptop";

		if ((i < 4) && (userinfo->capabilities & OSCAR_CAPABILITY_SECUREIM))
			emblems[i++] = "secure";
	}

	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

void oscar_tooltip_text(GaimBuddy *b, GaimNotifyUserInfo *user_info, gboolean full) {
	GaimConnection *gc = b->account->gc;
	OscarData *od = gc->proto_data;
	aim_userinfo_t *userinfo = aim_locate_finduserinfo(od, b->name);

	if (GAIM_BUDDY_IS_ONLINE(b)) {
		GaimPresence *presence;
		GaimStatus *status;
		const char *message;

		if (full)
			oscar_string_append_info(gc, user_info, b, userinfo);

		presence = gaim_buddy_get_presence(b);
		status = gaim_presence_get_active_status(presence);
		message = gaim_status_get_attr_string(status, "message");

		if (gaim_status_is_available(status))
		{
			if (message != NULL)
			{
				/* Available status messages are plain text */
				gchar *tmp;
				tmp = g_markup_escape_text(message, -1);
				gaim_notify_user_info_add_pair(user_info, _("Message"), tmp);
				g_free(tmp);
			}
		}
		else
		{
			if (message != NULL)
			{
				/* Away messages are HTML */
				gchar *tmp1, *tmp2;
				tmp2 = gaim_markup_strip_html(message);
				tmp1 = g_markup_escape_text(tmp2, -1);
				g_free(tmp2);
				tmp2 = gaim_str_sub_away_formatters(tmp1, gaim_account_get_username(gaim_connection_get_account(gc)));
				g_free(tmp1);
				gaim_notify_user_info_add_pair(user_info, _("Away Message"), tmp2);
				g_free(tmp2);
			}
			else
			{
				gaim_notify_user_info_add_pair(user_info, _("Away Message"), _("<i>(retrieving)</i>"));
			}
		}
	}
}

char *oscar_status_text(GaimBuddy *b)
{
	GaimConnection *gc;
	GaimAccount *account;
	OscarData *od;
	const GaimPresence *presence;
	const GaimStatus *status;
	const char *id;
	const char *message;
	gchar *ret = NULL;

	gc = gaim_account_get_connection(gaim_buddy_get_account(b));
	account = gaim_connection_get_account(gc);
	od = gc->proto_data;
	presence = gaim_buddy_get_presence(b);
	status = gaim_presence_get_active_status(presence);
	id = gaim_status_get_id(status);

	if (!gaim_presence_is_online(presence))
	{
		char *gname = aim_ssi_itemlist_findparentname(od->ssi.local, b->name);
		if (aim_ssi_waitingforauth(od->ssi.local, gname, b->name))
			ret = g_strdup(_("Not Authorized"));
		else
			ret = g_strdup(_("Offline"));
	}
	else if (gaim_status_is_available(status) && !strcmp(id, OSCAR_STATUS_ID_AVAILABLE))
	{
		/* Available */
		message = gaim_status_get_attr_string(status, "message");
		if (message != NULL)
		{
			ret = g_markup_escape_text(message, -1);
			gaim_util_chrreplace(ret, '\n', ' ');
		}
	}
	else if (!gaim_status_is_available(status) && !strcmp(id, OSCAR_STATUS_ID_AWAY))
	{
		/* Away */
		message = gaim_status_get_attr_string(status, "message");
		if (message != NULL)
		{
			gchar *tmp1, *tmp2;
			tmp1 = gaim_markup_strip_html(message);
			gaim_util_chrreplace(tmp1, '\n', ' ');
			tmp2 = g_markup_escape_text(tmp1, -1);
			ret = gaim_str_sub_away_formatters(tmp2, gaim_account_get_username(account));
			g_free(tmp1);
			g_free(tmp2);
		}
		else
		{
			ret = g_strdup(_("Away"));
		}
	}
	else
		ret = g_strdup(gaim_status_get_name(status));

	return ret;
}


static int oscar_icon_req(OscarData *od, FlapConnection *conn, FlapFrame *fr, ...) {
	GaimConnection *gc = od->gc;
	va_list ap;
	guint16 type;
	guint8 flags = 0, length = 0;
	guchar *md5 = NULL;

	va_start(ap, fr);
	type = va_arg(ap, int);

	switch(type) {
		case 0x0000:
		case 0x0001: {
			flags = va_arg(ap, int);
			length = va_arg(ap, int);
			md5 = va_arg(ap, guchar *);

			if (flags == 0x41) {
				if (!flap_connection_getbytype(od, SNAC_FAMILY_BART) && !od->iconconnecting) {
					od->iconconnecting = TRUE;
					od->set_icon = TRUE;
					aim_srv_requestnew(od, SNAC_FAMILY_BART);
				} else {
					struct stat st;
					char *iconfile = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(gaim_connection_get_account(gc)));
					if (iconfile == NULL) {
						aim_ssi_delicon(od);
					} else if (!g_stat(iconfile, &st)) {
						guchar *buf = g_malloc(st.st_size);
						FILE *file = g_fopen(iconfile, "rb");
						if (file) {
							/* XXX - Use g_file_get_contents()? */
							fread(buf, 1, st.st_size, file);
							fclose(file);
							gaim_debug_info("oscar",
											"Uploading icon to icon server\n");
							aim_bart_upload(od, buf, st.st_size);
						} else
							gaim_debug_error("oscar",
											 "Can't open buddy icon file!\n");
						g_free(buf);
					} else {
						gaim_debug_error("oscar",
										 "Can't stat buddy icon file!\n");
					}
					g_free(iconfile);
				}
			} else if (flags == 0x81) {
				char *iconfile = gaim_buddy_icons_get_full_path(gaim_account_get_buddy_icon(gaim_connection_get_account(gc)));
				if (iconfile == NULL)
					aim_ssi_delicon(od);
				else {
					aim_ssi_seticon(od, md5, length);
					g_free(iconfile);
				}
			}
		} break;

		case 0x0002: { /* We just set an "available" message? */
		} break;
	}

	va_end(ap);

	return 0;
}

void oscar_set_permit_deny(GaimConnection *gc) {
	GaimAccount *account = gaim_connection_get_account(gc);
	OscarData *od = (OscarData *)gc->proto_data;

	if (od->ssi.received_data) {
		switch (account->perm_deny) {
			case GAIM_PRIVACY_ALLOW_ALL:
				aim_ssi_setpermdeny(od, 0x01, 0xffffffff);
				break;
			case GAIM_PRIVACY_ALLOW_BUDDYLIST:
				aim_ssi_setpermdeny(od, 0x05, 0xffffffff);
				break;
			case GAIM_PRIVACY_ALLOW_USERS:
				aim_ssi_setpermdeny(od, 0x03, 0xffffffff);
				break;
			case GAIM_PRIVACY_DENY_ALL:
				aim_ssi_setpermdeny(od, 0x02, 0xffffffff);
				break;
			case GAIM_PRIVACY_DENY_USERS:
				aim_ssi_setpermdeny(od, 0x04, 0xffffffff);
				break;
			default:
				aim_ssi_setpermdeny(od, 0x01, 0xffffffff);
				break;
		}
	}
}

void oscar_add_permit(GaimConnection *gc, const char *who) {
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to add a permit\n");
	if (od->ssi.received_data)
		aim_ssi_addpermit(od, who);
}

void oscar_add_deny(GaimConnection *gc, const char *who) {
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to add a deny\n");
	if (od->ssi.received_data)
		aim_ssi_adddeny(od, who);
}

void oscar_rem_permit(GaimConnection *gc, const char *who) {
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to delete a permit\n");
	if (od->ssi.received_data)
		aim_ssi_delpermit(od, who);
}

void oscar_rem_deny(GaimConnection *gc, const char *who) {
	OscarData *od = (OscarData *)gc->proto_data;
	gaim_debug_info("oscar", "ssi: About to delete a deny\n");
	if (od->ssi.received_data)
		aim_ssi_deldeny(od, who);
}

GList *
oscar_status_types(GaimAccount *account)
{
	gboolean is_icq;
	GList *status_types = NULL;
	GaimStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	/* Used to flag some statuses as "user settable" or not */
	is_icq = aim_sn_is_icq(gaim_account_get_username(account));

	/* Common status types */
	/* Really the available message should only be settable for AIM accounts */
	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AVAILABLE,
										   OSCAR_STATUS_ID_AVAILABLE,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_AVAILABLE,
									 OSCAR_STATUS_ID_FREE4CHAT,
									 _("Free For Chat"), TRUE, is_icq, FALSE);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AWAY,
										   OSCAR_STATUS_ID_AWAY,
										   NULL, TRUE, TRUE, FALSE,
										   "message", _("Message"),
										   gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_INVISIBLE,
									 OSCAR_STATUS_ID_INVISIBLE,
									 NULL, TRUE, TRUE, FALSE);
	status_types = g_list_prepend(status_types, type);

	/* ICQ-specific status types */
	type = gaim_status_type_new_with_attrs(GAIM_STATUS_UNAVAILABLE,
				OSCAR_STATUS_ID_OCCUPIED,
				_("Occupied"), TRUE, is_icq, FALSE,
				"message", _("Message"),
				gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_EXTENDED_AWAY,
				OSCAR_STATUS_ID_DND,
				_("Do Not Disturb"), TRUE, is_icq, FALSE,
				"message", _("Message"),
				gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_EXTENDED_AWAY,
				OSCAR_STATUS_ID_NA,
				_("Not Available"), TRUE, is_icq, FALSE,
				"message", _("Message"),
				gaim_value_new(GAIM_TYPE_STRING), NULL);
	status_types = g_list_prepend(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
									 OSCAR_STATUS_ID_OFFLINE,
									 NULL, TRUE, TRUE, FALSE);
	status_types = g_list_prepend(status_types, type);

	status_types = g_list_reverse(status_types);

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

	if (!(g = gaim_buddy_get_group(b))) {
		oscar_free_name_data(data);
		return;
	}

	aim_ssi_editcomment(od, g->name, data->name, text);

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

	if (!(g = gaim_buddy_get_group(buddy)))
		return;
	comment = aim_ssi_getcomment(od->ssi.local, g->name, buddy->name);
	comment_utf8 = comment ? oscar_utf8_try_convert(gc->account, comment) : NULL;

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

	g_free(comment);
	g_free(comment_utf8);
}

static void
oscar_ask_directim_yes_cb(struct oscar_ask_directim_data *data)
{
	peer_connection_propose(data->od, OSCAR_CAPABILITY_DIRECTIM, data->who);
	g_free(data->who);
	g_free(data);
}

static void
oscar_ask_directim_no_cb(struct oscar_ask_directim_data *data)
{
	g_free(data->who);
	g_free(data);
}

/* This is called from right-click menu on a buddy node. */
static void
oscar_ask_directim(gpointer object, gpointer ignored)
{
	GaimBlistNode *node;
	GaimBuddy *buddy;
	GaimConnection *gc;
	gchar *buf;
	struct oscar_ask_directim_data *data;

	node = object;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *)node;
	gc = gaim_account_get_connection(buddy->account);

	data = g_new0(struct oscar_ask_directim_data, 1);
	data->who = g_strdup(buddy->name);
	data->od = gc->proto_data;
	buf = g_strdup_printf(_("You have selected to open a Direct IM connection with %s."),
			buddy->name);

	gaim_request_action(gc, NULL, buf,
			_("Because this reveals your IP address, it "
			  "may be considered a security risk.  Do you "
			  "wish to continue?"),
			0, data, 2,
			_("_Connect"), G_CALLBACK(oscar_ask_directim_yes_cb),
			_("Cancel"), G_CALLBACK(oscar_ask_directim_no_cb));
	g_free(buf);
}

static void
oscar_get_aim_info_cb(GaimBlistNode *node, gpointer ignore)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *)node;
	gc = gaim_account_get_connection(buddy->account);

	aim_locate_getinfoshort(gc->proto_data, gaim_buddy_get_name(buddy), 0x00000003);
}

static GList *
oscar_buddy_menu(GaimBuddy *buddy) {

	GaimConnection *gc;
	OscarData *od;
	GList *menu;
	GaimMenuAction *act;
	aim_userinfo_t *userinfo;

	gc = gaim_account_get_connection(buddy->account);
	od = gc->proto_data;
	userinfo = aim_locate_finduserinfo(od, buddy->name);
	menu = NULL;

	if (od->icq && aim_sn_is_icq(gaim_buddy_get_name(buddy)))
	{
		act = gaim_menu_action_new(_("Get AIM Info"),
								   GAIM_CALLBACK(oscar_get_aim_info_cb),
								   NULL, NULL);
		menu = g_list_prepend(menu, act);
	}

	act = gaim_menu_action_new(_("Edit Buddy Comment"),
	                           GAIM_CALLBACK(oscar_buddycb_edit_comment),
	                           NULL, NULL);
	menu = g_list_prepend(menu, act);

#if 0
	if (od->icq)
	{
		act = gaim_menu_action_new(_("Get Status Msg"),
		                           GAIM_CALLBACK(oscar_get_icqstatusmsg),
		                           NULL, NULL);
		menu = g_list_prepend(menu, act);
	}
#endif

	if (userinfo &&
		aim_sncmp(gaim_account_get_username(buddy->account), buddy->name) &&
		GAIM_BUDDY_IS_ONLINE(buddy))
	{
		if (userinfo->capabilities & OSCAR_CAPABILITY_DIRECTIM)
		{
			act = gaim_menu_action_new(_("Direct IM"),
			                           GAIM_CALLBACK(oscar_ask_directim),
			                           NULL, NULL);
			menu = g_list_prepend(menu, act);
		}
#if 0
		/* TODO: This menu item should be added by the core */
		if (userinfo->capabilities & OSCAR_CAPABILITY_GETFILE) {
			act = gaim_menu_action_new(_("Get File"),
			                           GAIM_CALLBACK(oscar_ask_getfile),
			                           NULL, NULL);
			menu = g_list_prepend(menu, act);
		}
#endif
	}

	if (od->ssi.received_data)
	{
		char *gname;
		gname = aim_ssi_itemlist_findparentname(od->ssi.local, buddy->name);
		if (gname && aim_ssi_waitingforauth(od->ssi.local, gname, buddy->name))
		{
			act = gaim_menu_action_new(_("Re-request Authorization"),
			                           GAIM_CALLBACK(gaim_auth_sendrequest_menu),
			                           NULL, NULL);
			menu = g_list_prepend(menu, act);
		}
	}

	menu = g_list_reverse(menu);

	return menu;
}


GList *oscar_blist_node_menu(GaimBlistNode *node) {
	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return oscar_buddy_menu((GaimBuddy *) node);
	} else {
		return NULL;
	}
}

static void
oscar_icq_privacy_opts(GaimConnection *gc, GaimRequestFields *fields)
{
	OscarData *od = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimRequestField *f;
	gboolean auth, web_aware;

	f = gaim_request_fields_get_field(fields, "authorization");
	auth = gaim_request_field_bool_get_value(f);

	f = gaim_request_fields_get_field(fields, "web_aware");
	web_aware = gaim_request_field_bool_get_value(f);

	gaim_account_set_bool(account, "authorization", auth);
	gaim_account_set_bool(account, "web_aware", web_aware);

	oscar_set_extendedstatus(gc);
	aim_icq_setsecurity(od, auth, web_aware);
}

static void
oscar_show_icq_privacy_opts(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimRequestFields *fields;
	GaimRequestFieldGroup *g;
	GaimRequestField *f;
	gboolean auth, web_aware;

	auth = gaim_account_get_bool(account, "authorization", OSCAR_DEFAULT_AUTHORIZATION);
	web_aware = gaim_account_get_bool(account, "web_aware", OSCAR_DEFAULT_WEB_AWARE);

	fields = gaim_request_fields_new();

	g = gaim_request_field_group_new(NULL);

	f = gaim_request_field_bool_new("authorization", _("Require authorization"), auth);
	gaim_request_field_group_add_field(g, f);

	f = gaim_request_field_bool_new("web_aware", _("Web aware (enabling this will cause you to receive SPAM!)"), web_aware);
	gaim_request_field_group_add_field(g, f);

	gaim_request_fields_add_group(fields, g);

	gaim_request_fields(gc, _("ICQ Privacy Options"), _("ICQ Privacy Options"),
						NULL, fields,
						_("OK"), G_CALLBACK(oscar_icq_privacy_opts),
						_("Cancel"), NULL, gc);
}

static void oscar_format_screenname(GaimConnection *gc, const char *nick) {
	OscarData *od = gc->proto_data;
	if (!aim_sncmp(gaim_account_get_username(gaim_connection_get_account(gc)), nick)) {
		if (!flap_connection_getbytype(od, SNAC_FAMILY_ADMIN)) {
			od->setnick = TRUE;
			od->newsn = g_strdup(nick);
			aim_srv_requestnew(od, SNAC_FAMILY_ADMIN);
		} else {
			aim_admin_setnick(od, flap_connection_getbytype(od, SNAC_FAMILY_ADMIN), nick);
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
	GaimConnection *gc;
	OscarData *od;
	FlapConnection *conn;

	gc = (GaimConnection *)action->context;
	od = gc->proto_data;

	conn = flap_connection_getbytype(od, SNAC_FAMILY_ADMIN);
	if (conn != NULL) {
		aim_admin_reqconfirm(od, conn);
	} else {
		od->conf = TRUE;
		aim_srv_requestnew(od, SNAC_FAMILY_ADMIN);
	}
}

static void oscar_show_email(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	OscarData *od = gc->proto_data;
	FlapConnection *conn = flap_connection_getbytype(od, SNAC_FAMILY_ADMIN);

	if (conn) {
		aim_admin_getinfo(od, conn, 0x11);
	} else {
		od->reqemail = TRUE;
		aim_srv_requestnew(od, SNAC_FAMILY_ADMIN);
	}
}

static void oscar_change_email(GaimConnection *gc, const char *email)
{
	OscarData *od = gc->proto_data;
	FlapConnection *conn = flap_connection_getbytype(od, SNAC_FAMILY_ADMIN);

	if (conn) {
		aim_admin_setemail(od, conn, email);
	} else {
		od->setemail = TRUE;
		od->email = g_strdup(email);
		aim_srv_requestnew(od, SNAC_FAMILY_ADMIN);
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
				if (buddy->account == gc->account && aim_ssi_waitingforauth(od->ssi.local, group->name, buddy->name)) {
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

	aim_search_address(od, email);
}

static void oscar_show_find_email(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_request_input(gc, _("Find Buddy by E-Mail"),
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
	gchar *substituted = gaim_strreplace(od->authinfo->chpassurl, "%s", gaim_account_get_username(gaim_connection_get_account(gc)));
	gaim_notify_uri(gc, substituted);
	g_free(substituted);
}

static void oscar_show_imforwardingurl(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_notify_uri(gc, "http://mymobile.aol.com/dbreg/register?action=imf&clientID=1");
}

void oscar_set_icon(GaimConnection *gc, const char *iconfile)
{
	OscarData *od = gc->proto_data;
	FILE *file;
	struct stat st;

	if (iconfile == NULL) {
		aim_ssi_delicon(od);
	} else if (!g_stat(iconfile, &st)) {
		guchar *buf = g_malloc(st.st_size);
		file = g_fopen(iconfile, "rb");
		if (file)
		{
			GaimCipher *cipher;
			GaimCipherContext *context;
			guchar md5[16];
			int len;

			/* XXX - Use g_file_get_contents()? */
			len = fread(buf, 1, st.st_size, file);
			fclose(file);

			cipher = gaim_ciphers_find_cipher("md5");
			context = gaim_cipher_context_new(cipher, NULL);
			gaim_cipher_context_append(context, buf, len);
			gaim_cipher_context_digest(context, 16, md5, NULL);
			gaim_cipher_context_destroy(context);

			aim_ssi_seticon(od, md5, 16);
		} else
			gaim_debug_error("oscar",
				   "Can't open buddy icon file!\n");
		g_free(buf);
	} else
		gaim_debug_error("oscar", "Can't stat buddy icon file!\n");
}

/**
 * Called by the Gaim core to determine whether or not we're
 * allowed to send a file to this user.
 */
gboolean
oscar_can_receive_file(GaimConnection *gc, const char *who)
{
	OscarData *od;
	GaimAccount *account;

	od = gc->proto_data;
	account = gaim_connection_get_account(gc);

	if (od != NULL)
	{
		aim_userinfo_t *userinfo;
		userinfo = aim_locate_finduserinfo(od, who);

		/*
		 * Don't allowing sending a file to a user that does not support
		 * file transfer, and don't allow sending to ourselves.
		 */
		if (((userinfo == NULL) ||
			(userinfo->capabilities & OSCAR_CAPABILITY_SENDFILE)) &&
			aim_sncmp(who, gaim_account_get_username(account)))
		{
			return TRUE;
		}
	}

	return FALSE;
}

GaimXfer *
oscar_new_xfer(GaimConnection *gc, const char *who)
{
	GaimXfer *xfer;
	OscarData *od;
	GaimAccount *account;
	PeerConnection *conn;

	od = gc->proto_data;
	account = gaim_connection_get_account(gc);

	xfer = gaim_xfer_new(account, GAIM_XFER_SEND, who);
	gaim_xfer_ref(xfer);
	gaim_xfer_set_init_fnc(xfer, peer_oft_sendcb_init);
	gaim_xfer_set_cancel_send_fnc(xfer, peer_oft_cb_generic_cancel);
	gaim_xfer_set_request_denied_fnc(xfer, peer_oft_cb_generic_cancel);
	gaim_xfer_set_ack_fnc(xfer, peer_oft_sendcb_ack);

	conn = peer_connection_new(od, OSCAR_CAPABILITY_SENDFILE, who);
	conn->flags |= PEER_CONNECTION_FLAG_INITIATED_BY_ME;
	conn->flags |= PEER_CONNECTION_FLAG_APPROVED;
	aim_icbm_makecookie(conn->cookie);
	conn->xfer = xfer;
	xfer->data = conn;

	return xfer;
}

/*
 * Called by the Gaim core when the user indicates that a
 * file is to be sent to a special someone.
 */
void
oscar_send_file(GaimConnection *gc, const char *who, const char *file)
{
	GaimXfer *xfer;

	xfer = oscar_new_xfer(gc, who);

	if (file != NULL)
		gaim_xfer_request_accepted(xfer, file);
	else
		gaim_xfer_request(xfer);
}

GList *
oscar_actions(GaimPlugin *plugin, gpointer context)
{
	GaimConnection *gc = (GaimConnection *) context;
	OscarData *od = gc->proto_data;
	GList *menu = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Set User Info..."),
			oscar_show_set_info);
	menu = g_list_prepend(menu, act);

	if (od->icq)
	{
		act = gaim_plugin_action_new(_("Set User Info (URL)..."),
				oscar_show_set_info_icqurl);
		menu = g_list_prepend(menu, act);
	}

	act = gaim_plugin_action_new(_("Change Password..."),
			oscar_change_pass);
	menu = g_list_prepend(menu, act);

	if (od->authinfo->chpassurl != NULL)
	{
		act = gaim_plugin_action_new(_("Change Password (URL)"),
				oscar_show_chpassurl);
		menu = g_list_prepend(menu, act);

		act = gaim_plugin_action_new(_("Configure IM Forwarding (URL)"),
				oscar_show_imforwardingurl);
		menu = g_list_prepend(menu, act);
	}

	menu = g_list_prepend(menu, NULL);

	if (od->icq)
	{
		/* ICQ actions */
		act = gaim_plugin_action_new(_("Set Privacy Options..."),
				oscar_show_icq_privacy_opts);
		menu = g_list_prepend(menu, act);
	}
	else
	{
		/* AIM actions */
		act = gaim_plugin_action_new(_("Format Screen Name..."),
				oscar_show_format_screenname);
		menu = g_list_prepend(menu, act);

		act = gaim_plugin_action_new(_("Confirm Account"),
				oscar_confirm_account);
		menu = g_list_prepend(menu, act);

		act = gaim_plugin_action_new(_("Display Currently Registered E-Mail Address"),
				oscar_show_email);
		menu = g_list_prepend(menu, act);

		act = gaim_plugin_action_new(_("Change Currently Registered E-Mail Address..."),
				oscar_show_change_email);
		menu = g_list_prepend(menu, act);
	}

	menu = g_list_prepend(menu, NULL);

	act = gaim_plugin_action_new(_("Show Buddies Awaiting Authorization"),
			oscar_show_awaitingauth);
	menu = g_list_prepend(menu, act);

	menu = g_list_prepend(menu, NULL);

	act = gaim_plugin_action_new(_("Search for Buddy by E-Mail Address..."),
			oscar_show_find_email);
	menu = g_list_prepend(menu, act);

#if 0
	act = gaim_plugin_action_new(_("Search for Buddy by Information"),
			show_find_info);
	menu = g_list_prepend(menu, act);
#endif

	menu = g_list_reverse(menu);

	return menu;
}

void oscar_change_passwd(GaimConnection *gc, const char *old, const char *new)
{
	OscarData *od = gc->proto_data;

	if (od->icq) {
		aim_icq_changepasswd(od, new);
	} else {
		FlapConnection *conn;
		conn = flap_connection_getbytype(od, SNAC_FAMILY_ADMIN);
		if (conn) {
			aim_admin_changepasswd(od, conn, new, old);
		} else {
			od->chpass = TRUE;
			od->oldp = g_strdup(old);
			od->newp = g_strdup(new);
			aim_srv_requestnew(od, SNAC_FAMILY_ADMIN);
		}
	}
}

void
oscar_convo_closed(GaimConnection *gc, const char *who)
{
	OscarData *od;
	PeerConnection *conn;

	od = gc->proto_data;
	conn = peer_connection_find_by_type(od, who, OSCAR_CAPABILITY_DIRECTIM);

	if (conn != NULL)
	{
		if (!conn->ready)
			aim_im_sendch2_cancel(conn);

		peer_connection_destroy(conn, OSCAR_DISCONNECT_LOCAL_CLOSED, NULL);
	}
}

static void
recent_buddies_cb(const char *name, GaimPrefType type,
				  gconstpointer value, gpointer data)
{
	GaimConnection *gc = data;
	OscarData *od = gc->proto_data;
	guint32 presence;

	presence = aim_ssi_getpresence(od->ssi.local);

	if (value) {
		presence &= ~AIM_SSI_PRESENCE_FLAG_NORECENTBUDDIES;
		aim_ssi_setpresence(od, presence);
	} else {
		presence |= AIM_SSI_PRESENCE_FLAG_NORECENTBUDDIES;
		aim_ssi_setpresence(od, presence);
	}
}

#ifdef USE_PRPL_PREFERENCES
	ppref = gaim_plugin_pref_new_with_name_and_label("/plugins/prpl/oscar/recent_buddies", _("Use recent buddies group"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label("/plugins/prpl/oscar/show_idle", _("Show how long you have been idle"));
	gaim_plugin_pref_frame_add(frame, ppref);
#endif

const char *
oscar_normalize(const GaimAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp1, *tmp2;
	int i, j;

	g_return_val_if_fail(str != NULL, NULL);

	strncpy(buf, str, BUF_LEN);
	for (i=0, j=0; buf[j]; i++, j++)
	{
		while (buf[j] == ' ')
			j++;
		buf[i] = buf[j];
	}
	buf[i] = '\0';

	tmp1 = g_utf8_strdown(buf, -1);
	tmp2 = g_utf8_normalize(tmp1, -1, G_NORMALIZE_DEFAULT);
	g_snprintf(buf, sizeof(buf), "%s", tmp2);
	g_free(tmp2);
	g_free(tmp1);

	return buf;
}

gboolean
oscar_offline_message(const GaimBuddy *buddy)
{
	OscarData *od;
	GaimAccount *account;
	GaimConnection *gc;

	account = gaim_buddy_get_account(buddy);
	gc = gaim_account_get_connection(account);
	od = (OscarData *)gc->proto_data;

	return (od->icq && aim_sn_is_icq(gaim_account_get_username(account)));
}

