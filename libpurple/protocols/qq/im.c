/**
 * @file im.c
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

#include "conversation.h"
#include "debug.h"
#include "internal.h"
#include "notify.h"
#include "server.h"
#include "util.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "qq_define.h"
#include "im.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "send_file.h"
#include "utils.h"

#define QQ_MSG_IM_MAX               700	/* max length of IM */

enum {
	QQ_IM_TEXT = 0x01,
	QQ_IM_AUTO_REPLY = 0x02
};

enum
{
	QQ_NORMAL_IM_TEXT = 0x000b,
	QQ_NORMAL_IM_FILE_REQUEST_TCP = 0x0001,
	QQ_NORMAL_IM_FILE_APPROVE_TCP = 0x0003,
	QQ_NORMAL_IM_FILE_REJECT_TCP = 0x0005,
	QQ_NORMAL_IM_FILE_REQUEST_UDP = 0x0035,
	QQ_NORMAL_IM_FILE_APPROVE_UDP = 0x0037,
	QQ_NORMAL_IM_FILE_REJECT_UDP = 0x0039,
	QQ_NORMAL_IM_FILE_NOTIFY = 0x003b,
	QQ_NORMAL_IM_FILE_PASV = 0x003f,			/* are you behind a firewall? */
	QQ_NORMAL_IM_FILE_CANCEL = 0x0049,
	QQ_NORMAL_IM_FILE_EX_REQUEST_UDP = 0x81,
	QQ_NORMAL_IM_FILE_EX_REQUEST_ACCEPT = 0x83,
	QQ_NORMAL_IM_FILE_EX_REQUEST_CANCEL = 0x85,
	QQ_NORMAL_IM_FILE_EX_NOTIFY_IP = 0x87
};

typedef struct _qq_im_header qq_im_header;
struct _qq_im_header {
	/* this is the common part of normal_text */
	guint16 version_from;
	guint32 uid_from;
	guint32 uid_to;
	guint8 session_md5[QQ_KEY_LENGTH];
	guint16 im_type;
};

/* read the common parts of the normal_im,
 * returns the bytes read if succeed, or -1 if there is any error */
static gint get_im_header(qq_im_header *im_header, guint8 *data, gint len)
{
	gint bytes;
	g_return_val_if_fail(data != NULL && len > 0, -1);

	bytes = 0;
	bytes += qq_get16(&(im_header->version_from), data + bytes);
	bytes += qq_get32(&(im_header->uid_from), data + bytes);
	bytes += qq_get32(&(im_header->uid_to), data + bytes);
	bytes += qq_getdata(im_header->session_md5, QQ_KEY_LENGTH, data + bytes);
	bytes += qq_get16(&(im_header->im_type), data + bytes);
	return bytes;
}

typedef struct _qq_emoticon qq_emoticon;
struct _qq_emoticon {
	guint8 symbol;
	gchar *name;
};

static gboolean emoticons_is_sorted = FALSE;
/* Map for purple smiley convert to qq, need qsort */
static qq_emoticon emoticons_std[] = {
	{0x4f, "/:)"},      {0x4f, "/wx"},      {0x4f, "/small_smile"},
	{0x42, "/:~"},      {0x42, "/pz"},      {0x42, "/curl_lip"},
	{0x43, "/:*"},      {0x43, "/se"},      {0x43, "/desire"},
	{0x44, "/:|"},      {0x44, "/fd"},      {0x44, "/dazed"},
	{0x45, "/8-)"},     {0x45, "/dy"},      {0x45, "/revel"},
	{0x46, "/:<"},      {0x46, "/ll"},      {0x46, "/cry"},
	{0x47, "/:$"},      {0x47, "/hx"},      {0x47, "/bashful"},
	{0x48, "/:x"},      {0x48, "/bz"},      {0x48, "/shut_mouth"},
	{0x8f, "/|-)"},     {0x8f, "/kun"},     {0x8f, "/sleepy"},
	{0x49, "/:z"},      {0x49, "/shui"},    {0x49, "/sleep"},	/* after sleepy */
	{0x4a, "/:'"},      {0x4a, "/dk"},      {0x4a, "/weep"},
	{0x4b, "/:-|"},     {0x4b, "/gg"},      {0x4b, "/embarassed"},
	{0x4c, "/:@"},      {0x4c, "/fn"},      {0x4c, "/pissed_off"},
	{0x4d, "/:P"},      {0x4d, "/tp"},      {0x4d, "/act_up"},
	{0x4e, "/:D"},      {0x4e, "/cy"},      {0x4e, "/toothy_smile"},
	{0x41, "/:O"},      {0x41, "/jy"},      {0x41, "/surprised"},
	{0x73, "/:("},      {0x73, "/ng"},      {0x73, "/sad"},
	{0x74, "/:+"},      {0x74, "/kuk"},     {0x74, "/cool"},
	{0xa1, "/--b"},     {0xa1, "/lengh"},
	{0x76, "/:Q"},      {0x76, "/zk"},      {0x76, "/crazy"},
	{0x8a, "/;P"},      {0x8a, "/tx"},      {0x8a, "/titter"},
	{0x8b, "/;-D"},     {0x8b, "/ka"},      {0x8b, "/cute"},
	{0x8c, "/;d"},      {0x8c, "/by"},      {0x8c, "/disdain"},
	{0x8d, "/;o"},      {0x8d, "/am"},      {0x8d, "/arrogant"},
	{0x8e, "/:g"},      {0x8e, "/jie"},     {0x8e, "/starving"},
	{0x78, "/:!"},      {0x78, "/jk"},      {0x78, "/terror"},
	{0x79, "/:L"},      {0x79, "/lh"},      {0x79, "/sweat"},
	{0x7a, "/:>"},      {0x7a, "/hanx"},    {0x7a, "/smirk"},
	{0x7b, "/:;"},      {0x7b, "/db"},      {0x7b, "/soldier"},
	{0x90, "/;f"},      {0x90, "/fendou"},  {0x90, "/struggle"},
	{0x91, "/:-S"},     {0x91, "/zhm"},     {0x91, "/curse"},
	{0x92, "/?"},       {0x92, "/yiw"},     {0x92, "/question"},
	{0x93, "/;x"},      {0x93, "/xu"},      {0x93, "/shh"},
	{0x94, "/;@"},      {0x94, "/yun"},     {0x94, "/dizzy"},
	{0x95, "/:8"},      {0x95, "/zhem"},    {0x95, "/excrutiating"},
	{0x96, "/;!"},      {0x96, "/shuai"},   {0x96, "/freaked_out"},
	{0x97, "/!!!"},     {0x97, "/kl"},      {0x97, "/skeleton"},
	{0x98, "/xx"},      {0x98, "/qiao"},    {0x98, "/hammer"},
	{0x99, "/bye"},     {0x99, "/zj"},      {0x99, "/bye"},
	{0xa2, "/wipe"},    {0xa2, "/ch"},
	{0xa3, "/dig"},     {0xa3, "/kb"},
	{0xa4, "/handclap"},{0xa4, "/gz"},
	{0xa5, "/&-("},     {0xa5, "/qd"},
	{0xa6, "/B-)"},     {0xa6, "/huaix"},
	{0xa7, "/<@"},      {0xa7, "/zhh"},
	{0xa8, "/@>"},      {0xa8, "/yhh"},
	{0xa9, "/:-O"},     {0xa9, "/hq"},
	{0xaa, "/>-|"},     {0xaa, "/bs"},
	{0xab, "/P-("},     {0xab, "/wq"},
	{0xac, "/:'|"},     {0xac, "/kk"},
	{0xad, "/X-)"},     {0xad, "/yx"},
	{0xae, "/:*"},      {0xae, "/qq"},
	{0xaf, "/@x"},      {0xaf, "/xia"},
	{0xb0, "/8*"},      {0xb0, "/kel"},
	{0xb1, "/pd"},      {0xb1, "/cd"},
	{0x61, "/<W>"},     {0x61, "/xig"},     {0x61, "/watermelon"},
	{0xb2, "/beer"},    {0xb2, "/pj"},
	{0xb3, "/basketb"}, {0xb3, "/lq"},
	{0xb4, "/oo"},      {0xb4, "/pp"},
	{0x80, "/coffee"},  {0x80, "/kf"},
	{0x81, "/eat"},     {0x81, "/fan"},
	{0x62, "/rose"},    {0x62, "/mg"},
	{0x63, "/fade"},    {0x63, "/dx"},      {0x63, "/wilt"},
	{0xb5, "/showlove"},{0xb5, "/sa"},		/* after sad */
	{0x65, "/heart"},   {0x65, "/xin"},
	{0x66, "/break"},   {0x66, "/xs"},      {0x66, "/broken_heart"},
	{0x67, "/cake"},    {0x67, "/dg"},
	{0x9c, "/li"},      {0x9c, "/shd"},     {0x9c, "/lightning"},
	{0x9d, "/bome"},    {0x9d, "/zhd"},     {0x9d, "/bomb"},
	{0x9e, "/kn"},      {0x9e, "/dao"},     {0x9e, "/knife"},
	{0x5e, "/footb"},   {0x5e, "/zq"},      {0x5e, "/soccer"},
	{0xb6, "/ladybug"}, {0xb6, "/pc"},
	{0x89, "/shit"},    {0x89, "/bb"},
	{0x6e, "/moon"},    {0x6e, "/yl"},
	{0x6b, "/sun"},     {0x6b, "/ty"},
	{0x68, "/gift"},    {0x68, "/lw"},
	{0x7f, "/hug"},     {0x7f, "/yb"},
	{0x6f, "/strong"},  {0x6f, "/qiang"},   {0x6f, "/thumbs_up"},
	{0x70, "/weak"},    {0x70, "/ruo"},     {0x70, "/thumbs_down"},
	{0x88, "/share"},   {0x88, "/ws"},      {0x88, "/handshake"},
	{0xb7, "/@)"},      {0xb7, "/bq"},
	{0xb8, "/jj"},      {0xb8, "/gy"},
	{0xb9, "/@@"},      {0xb9, "/qt"},
	{0xba, "/bad"},     {0xba, "/cj"},
	{0xbb, "/loveu"},   {0xbb, "/aini"},
	{0xbc, "/no"},      {0xbc, "/bu"},
	{0xbd, "/ok"},      {0xbd, "/hd"},
	{0x5c, "/love"},    {0x5c, "/aiq"},		/* after loveu */
	{0x56, "/<L>"},     {0x56, "/fw"},      {0x56, "/blow_kiss"},
	{0x58, "/jump"},    {0x58, "/tiao"},
	{0x5a, "/shake"},   {0x5a, "/fad"},		/* after fade */
	{0x5b, "/<O>"},     {0x5b, "/oh"},      {0x5b, "/angry"},
	{0xbe, "/circle"},  {0xbe, "/zhq"},
	{0xbf, "/kotow"},   {0xbf, "/kt"},
	{0xc0, "/turn"},    {0xc0, "/ht"},
	{0x77, "/:t"},      {0x77, "/tu"},      {0x77, "/vomit"},		/* after turn */
	{0xa0, "/victory"}, {0xa0, "/shl"},     {0xa0, "/v"},			/* end of v */
	{0xc1, "/skip"},    {0xc1, "/tsh"},
	{0xc2, "/oY"},      {0xc2, "/hsh"},
	{0xc3, "/#-O"},     {0xc3, "/jd"},
	{0xc4, "/hiphop"},  {0xc4, "/jw"},
	{0xc5, "/kiss"},    {0xc5, "/xw"},
	{0xc6, "/<&"},      {0xc6, "/ztj"},
	{0x7c, "/pig"},     {0x7c, "/zt"},		/* after ztj */
	{0xc7, "/&>"},      {0xc7, "/ytj"},		/* must be end of "&" */
	{0x75, "/:#"},      {0x75, "/feid"},    {0x75, "/SARS"},
	{0x59, "/go"},      {0x59, "/shan"},
	{0x57, "/find"},    {0x57, "/zhao"},    {0x57, "/search"},
	{0x55, "/&"},       {0x55, "/mm"},      {0x55, "/beautiful_eyebrows"},
	{0x7d, "/cat"},     {0x7d, "/maom"},
	{0x7e, "/dog"},     {0x7e, "/xg"},
	{0x9a, "/$"},       {0x9a, "/qianc"},   {0x9a, "/money"},
	{0x9b, "/(!)"},     {0x9b, "/dp"},      {0x9b, "/lightbulb"},
	{0x60, "/cup"},     {0x60, "/bei"},
	{0x9f, "/music"},   {0x9f, "/yy"},
	{0x82, "/pill"},    {0x82, "/yw"},
	{0x64, "/kiss"},    {0x64, "/wen"},
	{0x83, "/meeting"}, {0x83, "/hy"},
	{0x84, "/phone"},   {0x84, "/dh"},
	{0x85, "/time"},    {0x85, "/sj"},
	{0x86, "/email"},   {0x86, "/yj"},
	{0x87, "/tv"},      {0x87, "/ds"},
	{0x50, "/<D>"},     {0x50, "/dd"},
	{0x51, "/<J>"},     {0x51,  "/mn"},     {0x51,  "/beauty"},
	{0x52, "/<H>"},     {0x52,  "/hl"},
	{0x53, "/<M>"},     {0x53,  "/mamao"},
	{0x54, "/<QQ>"},    {0x54,  "/qz"},     {0x54,  "/qq"},
	{0x5d, "/<B>"},     {0x5d,  "/bj"},     {0x5d,  "/baijiu"},
	{0x5f, "/<U>"},     {0x5f,  "/qsh"},    {0x5f,  "/soda"},
	{0x69, "/<!!>"},    {0x69,  "/xy"},     {0x69,  "/rain"},
	{0x6a, "/<~>"},     {0x6a,  "/duoy"},   {0x6a,  "/cloudy"},
	{0x6c, "/<Z>"},     {0x6c,  "/xr"},     {0x6c,  "/snowman"},
	{0x6d, "/<*>"},     {0x6d,  "/xixing"}, {0x6d,  "/star"},		/* after starving */
	{0x71, "/<00>"},    {0x71,  "/nv"},     {0x71,  "/woman"},
	{0x72, "/<11>"},    {0x72,  "/nan"},    {0x72,  "/man"},
	{0, NULL}
};
gint emoticons_std_num = sizeof(emoticons_std) / sizeof(qq_emoticon) - 1;

/* Map for purple smiley convert to qq, need qsort */
static qq_emoticon emoticons_ext[] = {
	{0xc7, "/&>"},		{0xa5, "/&-("},
	{0xbb, "/loveu"},
	{0x63, "/fade"},
	{0x8f, "/sleepy"},	{0x73, "/sad"},		{0x8e, "/starving"},
	{0xc0, "/turn"},
	{0xa0, "/victory"}, {0x77, "/vomit"},
	{0xc6, "/ztj"},
	{0, NULL}
};
gint emoticons_ext_num = sizeof(emoticons_ext) / sizeof(qq_emoticon) - 1;

/* Map for qq smiley convert to purple */
static qq_emoticon emoticons_sym[] = {
	{0x41, "/jy"},
	{0x42, "/pz"},
	{0x43, "/se"},
	{0x44, "/fd"},
	{0x45, "/dy"},
	{0x46, "/ll"},
	{0x47, "/hx"},
	{0x48, "/bz"},
	{0x49, "/shui"},
	{0x4a, "/dk"},
	{0x4b, "/gg"},
	{0x4c, "/fn"},
	{0x4d, "/tp"},
	{0x4e, "/cy"},
	{0x4f, "/wx"},
	{0x50, "/dd"},
	{0x51, "/mn"},
	{0x52, "/hl"},
	{0x53, "/mamao"},
	{0x54, "/qz"},
	{0x55, "/mm"},
	{0x56, "/fw"},
	{0x57, "/zhao"},
	{0x58, "/tiao"},
	{0x59, "/shan"},
	{0x5a, "/fad"},
	{0x5b, "/oh"},
	{0x5c, "/aiq"},
	{0x5d, "/bj"},
	{0x5e, "/zq"},
	{0x5f, "/qsh"},
	{0x60, "/bei"},
	{0x61, "/xig"},
	{0x62, "/mg"},
	{0x63, "/dx"},
	{0x64, "/wen"},
	{0x65, "/xin"},
	{0x66, "/xs"},
	{0x67, "/dg"},
	{0x68, "/lw"},
	{0x69, "/xy"},
	{0x6a, "/duoy"},
	{0x6b, "/ty"},
	{0x6c, "/xr"},
	{0x6d, "/xixing"},
	{0x6e, "/yl"},
	{0x6f, "/qiang"},
	{0x70, "/ruo"},
	{0x71, "/nv"},
	{0x72, "/nan"},
	{0x73, "/ng"},
	{0x74, "/kuk"},
	{0x75, "/feid"},
	{0x76, "/zk"},
	{0x77, "/tu"},
	{0x78, "/jk"},
	{0x79, "/sweat"},
	{0x7a, "/hanx"},
	{0x7b, "/db"},
	{0x7c, "/zt"},
	{0x7d, "/maom"},
	{0x7e, "/xg"},
	{0x7f, "/yb"},
	{0x80, "/coffee"},
	{0x81, "/fan"},
	{0x82, "/yw"},
	{0x83, "/hy"},
	{0x84, "/dh"},
	{0x85, "/sj"},
	{0x86, "/yj"},
	{0x87, "/ds"},
	{0x88, "/ws"},
	{0x89, "/bb"},
	{0x8a, "/tx"},
	{0x8b, "/ka"},
	{0x8c, "/by"},
	{0x8d, "/am"},
	{0x8e, "/jie"},
	{0x8f, "/kun"},
	{0x90, "/fendou"},
	{0x91, "/zhm"},
	{0x92, "/yiw"},
	{0x93, "/xu"},
	{0x94, "/yun"},
	{0x95, "/zhem"},
	{0x96, "/shuai"},
	{0x97, "/kl"},
	{0x98, "/qiao"},
	{0x99, "/zj"},
	{0x9a, "/qianc"},
	{0x9b, "/dp"},
	{0x9c, "/shd"},
	{0x9d, "/zhd"},
	{0x9e, "/dao"},
	{0x9f, "/yy"},
	{0xa0, "/shl"},
	{0xa1, "/lengh"},
	{0xa2, "/wipe"},
	{0xa3, "/kb"},
	{0xa4, "/gz"},
	{0xa5, "/qd"},
	{0xa6, "/huaix"},
	{0xa7, "/zhh"},
	{0xa8, "/yhh"},
	{0xa9, "/hq"},
	{0xaa, "/bs"},
	{0xab, "/wq"},
	{0xac, "/kk"},
	{0xad, "/yx"},
	{0xae, "/qq"},
	{0xaf, "/xia"},
	{0xb0, "/kel"},
	{0xb1, "/cd"},
	{0xb2, "/pj"},
	{0xb3, "/lq"},
	{0xb4, "/pp"},
	{0xb5, "/sa"},
	{0xb6, "/pc"},
	{0xb7, "/bq"},
	{0xb8, "/gy"},
	{0xb9, "/qt"},
	{0xba, "/cj"},
	{0xbb, "/aini"},
	{0xbc, "/bu"},
	{0xbd, "/hd"},
	{0xbe, "/zhq"},
	{0xbf, "/kt"},
	{0xc0, "/ht"},
	{0xc1, "/tsh"},
	{0xc2, "/hsh"},
	{0xc3, "/jd"},
	{0xc4, "/jw"},
	{0xc5, "/xw"},
	{0xc6, "/ztj"},
	{0xc7, "/ytj"},
	{0, NULL}
};
gint emoticons_sym_num = sizeof(emoticons_sym) / sizeof(qq_emoticon) - 1;;

static int emoticon_cmp(const void *k1, const void *k2)
{
	const qq_emoticon *e1 = (const qq_emoticon *) k1;
	const qq_emoticon *e2 = (const qq_emoticon *) k2;
	if (e1->symbol == 0) {
		/* purple_debug_info("QQ", "emoticon_cmp len %d\n", strlen(e2->name)); */
		return strncmp(e1->name, e2->name, strlen(e2->name));
	}
	if (e2->symbol == 0) {
		/* purple_debug_info("QQ", "emoticon_cmp len %d\n", strlen(e1->name)); */
		return strncmp(e1->name, e2->name, strlen(e1->name));
	}
	return strcmp(e1->name, e2->name);
}

static void emoticon_try_sort()
{
	if (emoticons_is_sorted)
		return;

	purple_debug_info("QQ", "qsort stand emoticons\n");
	qsort(emoticons_std, emoticons_std_num, sizeof(qq_emoticon), emoticon_cmp);
	purple_debug_info("QQ", "qsort extend emoticons\n");
	qsort(emoticons_ext, emoticons_ext_num, sizeof(qq_emoticon), emoticon_cmp);
	emoticons_is_sorted = TRUE;
}

static qq_emoticon *emoticon_find(gchar *name)
{
	qq_emoticon *ret = NULL;
	qq_emoticon key;

	g_return_val_if_fail(name != NULL, NULL);
	emoticon_try_sort();

	key.name = name;
	key.symbol = 0;

	/* purple_debug_info("QQ", "bsearch emoticon %.20s\n", name); */
	ret = (qq_emoticon *)bsearch(&key, emoticons_ext, emoticons_ext_num,
			sizeof(qq_emoticon), emoticon_cmp);
	if (ret != NULL) {
		return ret;
	}
	ret = (qq_emoticon *)bsearch(&key, emoticons_std, emoticons_std_num,
			sizeof(qq_emoticon), emoticon_cmp);
	return ret;
}

static gchar *emoticon_get(guint8 symbol)
{
	g_return_val_if_fail(symbol >= emoticons_sym[0].symbol, NULL);
	g_return_val_if_fail(symbol <= emoticons_sym[emoticons_sym_num - 2].symbol, NULL);

	return emoticons_sym[symbol - emoticons_sym[0].symbol].name;
}

/* convert qq emote icon to purple sytle
   Notice: text is in qq charset, GB18030 or utf8 */
gchar *qq_emoticon_to_purple(gchar *text)
{
	gchar *ret;
	GString *converted;
	gchar **segments;
	gboolean have_smiley;
	gchar *purple_smiley;
	gchar *cur;
	guint8 symbol;

	/* qq_show_packet("text", (guint8 *)text, strlen(text)); */
	g_return_val_if_fail(text != NULL && strlen(text) != 0, g_strdup(""));

	while ((cur = strchr(text, '\x14')) != NULL)
		*cur = '\x15';

	segments = g_strsplit(text, "\x15", 0);
	if(segments == NULL) {
		return g_strdup("");
	}

	converted = g_string_new("");
	have_smiley = FALSE;
	if (segments[0] != NULL) {
		g_string_append(converted, segments[0]);
	} else {
		purple_debug_info("QQ", "segments[0] is NULL\n");
	}
	while ((*(++segments)) != NULL) {
		have_smiley = TRUE;

		cur = *segments;
		if (cur == NULL) {
			purple_debug_info("QQ", "current segment is NULL\n");
			break;
		}
		if (strlen(cur) == 0) {
			purple_debug_info("QQ", "current segment length is 0\n");
			break;
		}
		symbol = (guint8)cur[0];

		purple_smiley = emoticon_get(symbol);
		if (purple_smiley == NULL) {
			purple_debug_info("QQ", "Not found smiley of 0x%02X\n", symbol);
			g_string_append(converted, "<IMG ID=\"0\">");
		} else {
			purple_debug_info("QQ", "Found 0x%02X smiley is %s\n", symbol, purple_smiley);
			g_string_append(converted, purple_smiley);
			g_string_append(converted, cur + 1);
		}
		/* purple_debug_info("QQ", "next segment\n"); */
	}

	/* purple_debug_info("QQ", "end of convert\n"); */
	if (!have_smiley) {
		g_string_prepend(converted, "<font sml=\"none\">");
		g_string_append(converted, "</font>");
	}
	ret = converted->str;
	g_string_free(converted, FALSE);
	return ret;
}

void qq_im_fmt_free(qq_im_format *fmt)
{
	g_return_if_fail(fmt != NULL);
	if (fmt->font)	g_free(fmt->font);
	g_free(fmt);
}

qq_im_format *qq_im_fmt_new(void)
{
	qq_im_format *fmt;
	const gchar simsun[] = { 0xcb, 0xce, 0xcc, 0xe5, 0};	/* simsun in Chinese */

	fmt = g_new0(qq_im_format, 1);
	memset(fmt, 0, sizeof(qq_im_format));
	fmt->font_len = strlen(simsun);
	fmt->font = g_strdup(simsun);
	fmt->attr = 10;
	/* encoding, 0x8602=GB, 0x0000=EN, define BIG5 support here */
	fmt->charset = 0x8602;

	return fmt;
}

qq_im_format *qq_im_fmt_new_by_purple(const gchar *msg)
{
	qq_im_format *fmt;
	const gchar *start, *end, *last;
	GData *attribs;
	gchar *tmp;
	unsigned char *rgb;

	g_return_val_if_fail(msg != NULL, NULL);

	fmt = qq_im_fmt_new();

	last = msg;
	while (purple_markup_find_tag("font", last, &start, &end, &attribs)) {
		tmp = g_datalist_get_data(&attribs, "face");
		if (tmp && strlen(tmp) > 0) {
			if (fmt->font)	g_free(fmt->font);
			fmt->font_len = strlen(tmp);
			fmt->font = g_strdup(tmp);
		}

		tmp = g_datalist_get_data(&attribs, "size");
		if (tmp) {
			fmt->attr = atoi(tmp) * 3 + 1;
			fmt->attr &= 0x0f;
		}

		tmp = g_datalist_get_data(&attribs, "color");
		if (tmp && strlen(tmp) > 1) {
			rgb = purple_base16_decode(tmp + 1, NULL);
			g_memmove(fmt->rgb, rgb, 3);
			g_free(rgb);
		}

		g_datalist_clear(&attribs);
		last = end + 1;
	}

	if (purple_markup_find_tag("b", msg, &start, &end, &attribs)) {
		fmt->attr |= 0x20;
		g_datalist_clear(&attribs);
	}

	if (purple_markup_find_tag("i", msg, &start, &end, &attribs)) {
		fmt->attr |= 0x40;
		g_datalist_clear(&attribs);
	}

	if (purple_markup_find_tag("u", msg, &start, &end, &attribs)) {
		fmt->attr |= 0x80;
		g_datalist_clear(&attribs);
	}

	return fmt;
}

/* convert qq format to purple
   Notice: text is in qq charset, GB18030 or utf8 */
gchar *qq_im_fmt_to_purple(qq_im_format *fmt, gchar *text)
{
	GString *converted, *tmp;
	gchar *ret;
	gint size;

	converted = g_string_new(text);
	tmp = g_string_new("");
	g_string_append_printf(tmp, "<font color=\"#%02x%02x%02x\">",
		fmt->rgb[0], fmt->rgb[1], fmt->rgb[2]);
	g_string_prepend(converted, tmp->str);
	g_string_set_size(tmp, 0);
	g_string_append(converted, "</font>");

	/* Fixme:
	 * check font face can be convert to utf8 or not?
	 * If failed, prepending font face cause msg display as "(NULL)" */
	if (fmt->font != NULL) {
		g_string_append_printf(tmp, "<font face=\"%s\">", fmt->font);
		g_string_prepend(converted, tmp->str);
		g_string_set_size(tmp, 0);
		g_string_append(converted, "</font>");
	}
	size = (fmt->attr & 0x1f) / 3;
	if (size >= 0) {
		g_string_append_printf(tmp, "<font size=\"%d\">", size);
		g_string_prepend(converted, tmp->str);
		g_string_set_size(tmp, 0);
		g_string_append(converted, "</font>");
	}
	if (fmt->attr & 0x20) {
		/* bold */
		g_string_prepend(converted, "<b>");
		g_string_append(converted, "</b>");
	}
	if (fmt->attr & 0x40) {
		/* italic */
		g_string_prepend(converted, "<i>");
		g_string_append(converted, "</i>");
	}
	if (fmt->attr & 0x80) {
		/* underline */
		g_string_prepend(converted, "<u>");
		g_string_append(converted, "</u>");
	}

	g_string_free(tmp, TRUE);
	ret = converted->str;
	g_string_free(converted, FALSE);
	return ret;
}

gint qq_put_im_tail(guint8 *buf, qq_im_format *fmt)
{
	gint bytes;

	g_return_val_if_fail(buf != NULL && fmt != NULL, 0);

	bytes = 0;
	bytes += qq_put8(buf + bytes, 0);
	bytes += qq_put8(buf + bytes, fmt->attr);
	bytes += qq_putdata(buf + bytes, fmt->rgb, sizeof(fmt->rgb));
	bytes += qq_put8(buf + bytes, 0);
	bytes += qq_put16(buf + bytes, fmt->charset);
	if (fmt->font != NULL && fmt->font_len > 0) {
		bytes += qq_putdata(buf + bytes, (guint8 *)fmt->font, fmt->font_len);
	} else {
		purple_debug_warning("QQ", "Font name is empty\n");
	}
	bytes += qq_put8(buf + bytes, bytes + 1);
	/* qq_show_packet("IM tail", buf, bytes); */
	return bytes;
}

/* data includes text msg and font attr*/
gint qq_get_im_tail(qq_im_format *fmt, guint8 *data, gint data_len)
{
	gint bytes, text_len;
	guint8 tail_len;
	guint8 font_len;

	g_return_val_if_fail(fmt != NULL && data != NULL, 0);
	g_return_val_if_fail(data_len > 1, 0);
	tail_len = data[data_len - 1];
	g_return_val_if_fail(tail_len > 2, 0);
	text_len = data_len - tail_len;
	g_return_val_if_fail(text_len >= 0, 0);

	bytes = text_len;
	/* qq_show_packet("IM tail", data + bytes, tail_len); */
	bytes += 1;		/* skip 0x00 */
	bytes += qq_get8(&fmt->attr, data + bytes);
	bytes += qq_getdata(fmt->rgb, sizeof(fmt->rgb), data + bytes);	/* red,green,blue */
 	bytes += 1;	/* skip 0x00 */
	bytes += qq_get16(&fmt->charset, data + bytes);

	font_len = data_len - bytes - 1;
	g_return_val_if_fail(font_len > 0, bytes + 1);

	fmt->font_len = font_len;
	if (fmt->font != NULL)	g_free(fmt->font);
	fmt->font = g_strndup((gchar *)data + bytes, fmt->font_len);
	return tail_len;
}

void qq_got_message(PurpleConnection *gc, const gchar *msg)
{
	qq_data *qd;
	gchar *from;
	time_t now = time(NULL);

	g_return_if_fail(gc != NULL  && gc->proto_data != NULL);
	qd = gc->proto_data;

	g_return_if_fail(qd->uid > 0);

	qq_buddy_find_or_new(gc, qd->uid);

	from = uid_to_purple_name(qd->uid);
	serv_got_im(gc, from, msg, PURPLE_MESSAGE_SYSTEM, now);
	g_free(from);
}

/* process received normal text IM */
static void process_im_text(PurpleConnection *gc, guint8 *data, gint len, qq_im_header *im_header)
{
	qq_data *qd;
	guint16 purple_msg_type;
	gchar *who;
	gchar *msg_smiley, *msg_fmt, *msg_utf8;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	gint bytes, tail_len;
	qq_im_format *fmt = NULL;

	struct {
		/* now comes the part for text only */
		guint16 msg_seq;
		guint32 send_time;
		guint16 sender_icon;
		guint8 unknown1[3];
		guint8 has_font_attr;
		guint8 fragment_count;
		guint8 fragment_index;
		guint8 msg_id;
		guint8 unknown2;
		guint8 msg_type;
		gchar *msg;		/* no fixed length, ends with 0x00 */
	} im_text;

	g_return_if_fail (data != NULL && len > 0);
	g_return_if_fail(im_header != NULL);

	qd = (qq_data *) gc->proto_data;
	memset(&im_text, 0, sizeof(im_text));

	/* qq_show_packet("IM text", data, len); */
	bytes = 0;
	bytes += qq_get16(&(im_text.msg_seq), data + bytes);
	bytes += qq_get32(&(im_text.send_time), data + bytes);
	bytes += qq_get16(&(im_text.sender_icon), data + bytes);
	bytes += qq_getdata(im_text.unknown1, sizeof(im_text.unknown1), data + bytes); /* 0x(00 00 00)*/
	bytes += qq_get8(&(im_text.has_font_attr), data + bytes);
	bytes += qq_get8(&(im_text.fragment_count), data + bytes);
	bytes += qq_get8(&(im_text.fragment_index), data + bytes);
	bytes += qq_get8(&(im_text.msg_id), data + bytes);
	bytes += 1; 	/* skip 0x00 */
	bytes += qq_get8(&(im_text.msg_type), data + bytes);
	purple_debug_info("QQ", "IM Seq %u, id %04X, fragment %d-%d, type %d, %s\n",
			im_text.msg_seq, im_text.msg_id,
			im_text.fragment_count, im_text.fragment_index,
			im_text.msg_type,
			im_text.has_font_attr ? "exist font atrr" : "");

	if (im_text.has_font_attr) {
		fmt = qq_im_fmt_new();
		tail_len = qq_get_im_tail(fmt, data + bytes, len - bytes);
		im_text.msg = g_strndup((gchar *)(data + bytes), len - tail_len);
	} else	{
		im_text.msg = g_strndup((gchar *)(data + bytes), len - bytes);
	}
	/* qq_show_packet("IM text", (guint8 *)im_text.msg , strlen(im_text.msg) ); */

	who = uid_to_purple_name(im_header->uid_from);
	buddy = purple_find_buddy(gc->account, who);
	if (buddy == NULL) {
		/* create no-auth buddy */
		buddy = qq_buddy_new(gc, im_header->uid_from);
	}
	bd = (buddy == NULL) ? NULL : (qq_buddy_data *) buddy->proto_data;
	if (bd != NULL) {
		bd->client_tag = im_header->version_from;
		bd->face = im_text.sender_icon;
		qq_update_buddy_icon(gc->account, who, bd->face);
	}

	purple_msg_type = (im_text.msg_type == QQ_IM_AUTO_REPLY)
		? PURPLE_MESSAGE_AUTO_RESP : 0;

	msg_smiley = qq_emoticon_to_purple(im_text.msg);
	if (fmt != NULL) {
		msg_fmt = qq_im_fmt_to_purple(fmt, msg_smiley);
		msg_utf8 =  qq_to_utf8(msg_fmt, QQ_CHARSET_DEFAULT);
		g_free(msg_fmt);
		qq_im_fmt_free(fmt);
	} else {
		msg_utf8 =  qq_to_utf8(msg_smiley, QQ_CHARSET_DEFAULT);
	}
	g_free(msg_smiley);

	/* send encoded to purple, note that we use im_text.send_time,
	 * not the time we receive the message
	 * as it may have been delayed when I am not online. */
	purple_debug_info("QQ", "IM from %u: %s\n", im_header->uid_from,msg_utf8);
	serv_got_im(gc, who, msg_utf8, purple_msg_type, (time_t) im_text.send_time);

	g_free(msg_utf8);
	g_free(who);
	g_free(im_text.msg);
}

/* process received extended (2007) text IM */
static void process_extend_im_text(PurpleConnection *gc, guint8 *data, gint len, qq_im_header *im_header)
{
	qq_data *qd;
	guint16 purple_msg_type;
	gchar *who;
	gchar *msg_smiley, *msg_fmt, *msg_utf8;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	gint bytes, tail_len;
	qq_im_format *fmt = NULL;

	struct {
		/* now comes the part for text only */
		guint16 msg_seq;
		guint32 send_time;
		guint16 sender_icon;
		guint32 has_font_attr;
		guint8 unknown1[8];
		guint8 fragment_count;
		guint8 fragment_index;
		guint8 msg_id;
		guint8 unknown2;
		guint8 msg_type;
		gchar *msg;		/* no fixed length, ends with 0x00 */
		guint8 fromMobileQQ;
	} im_text;

	g_return_if_fail (data != NULL && len > 0);
	g_return_if_fail(im_header != NULL);

	qd = (qq_data *) gc->proto_data;
	memset(&im_text, 0, sizeof(im_text));

	/* qq_show_packet("Extend IM text", data, len); */
	bytes = 0;
	bytes += qq_get16(&(im_text.msg_seq), data + bytes);
	bytes += qq_get32(&(im_text.send_time), data + bytes);
	bytes += qq_get16(&(im_text.sender_icon), data + bytes);
	bytes += qq_get32(&(im_text.has_font_attr), data + bytes);
	bytes += qq_getdata(im_text.unknown1, sizeof(im_text.unknown1), data + bytes);
	bytes += qq_get8(&(im_text.fragment_count), data + bytes);
	bytes += qq_get8(&(im_text.fragment_index), data + bytes);
	bytes += qq_get8(&(im_text.msg_id), data + bytes);
	bytes += 1; 	/* skip 0x00 */
	bytes += qq_get8(&(im_text.msg_type), data + bytes);
	purple_debug_info("QQ", "IM Seq %u, id %04X, fragment %d-%d, type %d, %s\n",
			im_text.msg_seq, im_text.msg_id,
			im_text.fragment_count, im_text.fragment_index,
			im_text.msg_type,
			im_text.has_font_attr ? "exist font atrr" : "");

	if (im_text.has_font_attr) {
		fmt = qq_im_fmt_new();
		tail_len = qq_get_im_tail(fmt, data + bytes, len - bytes);
		im_text.msg = g_strndup((gchar *)(data + bytes), len - tail_len);
	} else	{
		im_text.msg = g_strndup((gchar *)(data + bytes), len - bytes);
	}
	/* qq_show_packet("IM text", (guint8 *)im_text.msg , strlen(im_text.msg)); */

	if(im_text.fragment_count == 0) 	im_text.fragment_count = 1;

	who = uid_to_purple_name(im_header->uid_from);
	buddy = purple_find_buddy(gc->account, who);
	if (buddy == NULL) {
		/* create no-auth buddy */
		buddy = qq_buddy_new(gc, im_header->uid_from);
	}
	bd = (buddy == NULL) ? NULL : (qq_buddy_data *) buddy->proto_data;
	if (bd != NULL) {
		bd->client_tag = im_header->version_from;
		bd->face = im_text.sender_icon;
		qq_update_buddy_icon(gc->account, who, bd->face);
	}

	purple_msg_type = 0;

	msg_smiley = qq_emoticon_to_purple(im_text.msg);
	if (fmt != NULL) {
		msg_fmt = qq_im_fmt_to_purple(fmt, msg_smiley);
		msg_utf8 =  qq_to_utf8(msg_fmt, QQ_CHARSET_DEFAULT);
		g_free(msg_fmt);
		qq_im_fmt_free(fmt);
	} else {
		msg_utf8 =  qq_to_utf8(msg_smiley, QQ_CHARSET_DEFAULT);
	}
	g_free(msg_smiley);

	/* send encoded to purple, note that we use im_text.send_time,
	 * not the time we receive the message
	 * as it may have been delayed when I am not online. */
	serv_got_im(gc, who, msg_utf8, purple_msg_type, (time_t) im_text.send_time);

	g_free(msg_utf8);
	g_free(who);
	g_free(im_text.msg);
}

/* it is a normal IM, maybe text or video request */
void qq_process_im(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes = 0;
	qq_im_header im_header;

	g_return_if_fail (data != NULL && len > 0);

	bytes = get_im_header(&im_header, data, len);
	if (bytes < 0) {
		purple_debug_error("QQ", "Fail read im header, len %d\n", len);
		qq_show_packet ("IM Header", data, len);
		return;
	}
	purple_debug_info("QQ",
			"Got IM to %u, type: %02X from: %u ver: %s (%04X)\n",
			im_header.uid_to, im_header.im_type, im_header.uid_from,
			qq_get_ver_desc(im_header.version_from), im_header.version_from);

	switch (im_header.im_type) {
		case QQ_NORMAL_IM_TEXT:
			if (bytes >= len - 1) {
				purple_debug_warning("QQ", "Received normal IM text is empty\n");
				return;
			}
			process_im_text(gc, data + bytes, len - bytes, &im_header);
			break;
		case QQ_NORMAL_IM_FILE_REJECT_UDP:
			qq_process_recv_file_reject(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_APPROVE_UDP:
			qq_process_recv_file_accept(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_UDP:
			qq_process_recv_file_request(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_CANCEL:
			qq_process_recv_file_cancel(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_NOTIFY:
			qq_process_recv_file_notify(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_TCP:
			/* Check ReceivedFileIM::parseContents in eva*/
			/* some client use this function for detect invisable buddy*/
		case QQ_NORMAL_IM_FILE_APPROVE_TCP:
		case QQ_NORMAL_IM_FILE_REJECT_TCP:
		case QQ_NORMAL_IM_FILE_PASV:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_UDP:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_ACCEPT:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_CANCEL:
		case QQ_NORMAL_IM_FILE_EX_NOTIFY_IP:
			qq_show_packet ("Not support", data, len);
			break;
		default:
			/* a simple process here, maybe more later */
			qq_show_packet ("Unknow", data + bytes, len - bytes);
			return;
	}
}

/* it is a extended IM, maybe text or video request */
void qq_process_extend_im(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes;
	qq_im_header im_header;

	g_return_if_fail (data != NULL && len > 0);

	bytes = get_im_header(&im_header, data, len);
	if (bytes < 0) {
		purple_debug_error("QQ", "Fail read im header, len %d\n", len);
		qq_show_packet ("IM Header", data, len);
		return;
	}
	purple_debug_info("QQ",
			"Got Extend IM to %u, type: %02X from: %u ver: %s (%04X)\n",
			im_header.uid_to, im_header.im_type, im_header.uid_from,
			qq_get_ver_desc(im_header.version_from), im_header.version_from);

	switch (im_header.im_type) {
		case QQ_NORMAL_IM_TEXT:
			process_extend_im_text(gc, data + bytes, len - bytes, &im_header);
			break;
		case QQ_NORMAL_IM_FILE_REJECT_UDP:
			qq_process_recv_file_reject (data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_APPROVE_UDP:
			qq_process_recv_file_accept (data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_UDP:
			qq_process_recv_file_request (data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_CANCEL:
			qq_process_recv_file_cancel (data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_NOTIFY:
			qq_process_recv_file_notify (data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_TCP:
			/* Check ReceivedFileIM::parseContents in eva*/
			/* some client use this function for detect invisable buddy*/
		case QQ_NORMAL_IM_FILE_APPROVE_TCP:
		case QQ_NORMAL_IM_FILE_REJECT_TCP:
		case QQ_NORMAL_IM_FILE_PASV:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_UDP:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_ACCEPT:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_CANCEL:
		case QQ_NORMAL_IM_FILE_EX_NOTIFY_IP:
			qq_show_packet ("Not support", data, len);
			break;
		default:
			/* a simple process here, maybe more later */
			qq_show_packet ("Unknow", data + bytes, len - bytes);
			break;
	}
}

/* send an IM to uid_to */
static void request_send_im(PurpleConnection *gc, guint32 uid_to, gint type,
	qq_im_format *fmt, gchar *msg, guint8 id, guint8 frag_count, guint8 frag_index)
{
	qq_data *qd;
	guint8 raw_data[MAX_PACKET_SIZE - 16];
	guint16 im_type;
	gint bytes;
	time_t now;

	qd = (qq_data *) gc->proto_data;
	im_type = QQ_NORMAL_IM_TEXT;

	/* purple_debug_info("QQ", "Send IM %d-%d\n", frag_count, frag_index); */
	bytes = 0;
	/* 000-003: receiver uid */
	bytes += qq_put32(raw_data + bytes, qd->uid);
	/* 004-007: sender uid */
	bytes += qq_put32(raw_data + bytes, uid_to);
	/* 008-009: sender client version */
	bytes += qq_put16(raw_data + bytes, qd->client_tag);
	/* 010-013: receiver uid */
	bytes += qq_put32(raw_data + bytes, qd->uid);
	/* 014-017: sender uid */
	bytes += qq_put32(raw_data + bytes, uid_to);
	/* 018-033: md5 of (uid+session_key) */
	bytes += qq_putdata(raw_data + bytes, qd->session_md5, 16);
	/* 034-035: message type */
	bytes += qq_put16(raw_data + bytes, QQ_NORMAL_IM_TEXT);
	/* 036-037: sequence number */
	bytes += qq_put16(raw_data + bytes, qd->send_seq);
	/* 038-041: send time */
	now = time(NULL);
	bytes += qq_put32(raw_data + bytes, (guint32) now);
	/* 042-043: sender icon */
	bytes += qq_put16(raw_data + bytes, qd->my_icon);
	/* 044-046: always 0x00 */
	bytes += qq_put16(raw_data + bytes, 0x0000);
	bytes += qq_put8(raw_data + bytes, 0x00);
	/* 047-047: always use font attr */
	bytes += qq_put8(raw_data + bytes, 0x01);
	/* 048-051: always 0x00 */
	/* Fixme: frag_count, frag_index not working now */
	bytes += qq_put8(raw_data + bytes, frag_count);
	bytes += qq_put8(raw_data + bytes, frag_index);
	bytes += qq_put8(raw_data + bytes, id);
	bytes += qq_put8(raw_data + bytes, 0);
	/* 052-052: text message type (normal/auto-reply) */
	bytes += qq_put8(raw_data + bytes, type);
	/* 053-   : msg ends with 0x00 */
	bytes += qq_putdata(raw_data + bytes, (guint8 *)msg, strlen(msg));
	if (frag_count == frag_index + 1) {
		bytes += qq_put8(raw_data + bytes, 0x20);	/* add extra SPACE */
	}
	bytes += qq_put_im_tail(raw_data + bytes, fmt);

	/* qq_show_packet("QQ_CMD_SEND_IM", raw_data, bytes); */
	qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
}

static void im_convert_and_merge(GString *dest, GString *append)
{
	gchar *converted;
	g_return_if_fail(dest != NULL && append != NULL);

	if (append->str == NULL || append->len <= 0) {
		return;
	}
	/* purple_debug_info("QQ", "Append:\n%s\n", append->str); */
	converted = utf8_to_qq(append->str, QQ_CHARSET_DEFAULT);
	g_string_append(dest, converted);
	g_string_set_size(append, 0);
	g_free(converted);
}

GSList *qq_im_get_segments(gchar *msg_stripped, gboolean is_smiley_none)
{
	GSList *string_list = NULL;
	GString *new_string;
	GString *append_utf8;
	gchar *start, *p;
	gint count, len;
	qq_emoticon *emoticon;

	g_return_val_if_fail(msg_stripped != NULL, NULL);

	start = msg_stripped;
	count = 0;
	new_string = g_string_new("");
	append_utf8 = g_string_new("");
	while (*start) {
		p = start;

		/* Convert emoticon */
		if (!is_smiley_none && *p == '/') {
			if (new_string->len + append_utf8->len + 2 > QQ_MSG_IM_MAX) {
				/* enough chars to send */
				im_convert_and_merge(new_string, append_utf8);
				string_list = g_slist_append(string_list, strdup(new_string->str));
				g_string_set_size(new_string, 0);
				continue;
			}
			emoticon = emoticon_find(p);
			if (emoticon != NULL) {
				purple_debug_info("QQ", "found emoticon %s as 0x%02X\n",
						emoticon->name, emoticon->symbol);
				/* QQ emoticon code prevent converting from utf8 to QQ charset
				 * convert append_utf8 to QQ charset
				 * merge the result to dest
				 * append qq QQ emoticon code to dest */
				im_convert_and_merge(new_string, append_utf8);
				g_string_append_c(new_string, 0x14);
				g_string_append_c(new_string, emoticon->symbol);
				start += strlen(emoticon->name);
				continue;
			} else {
				purple_debug_info("QQ", "Not found emoticon %.20s\n", p);
			}
		}

		/* Get next char */
		start = g_utf8_next_char(p);
		len = start - p;
		if (new_string->len + append_utf8->len + len > QQ_MSG_IM_MAX) {
			/* enough chars to send */
			im_convert_and_merge(new_string, append_utf8);
			string_list = g_slist_append(string_list, strdup(new_string->str));
			g_string_set_size(new_string, 0);
		}
		g_string_append_len(append_utf8, p, len);
	}

	if (new_string->len + append_utf8->len > 0) {
		im_convert_and_merge(new_string, append_utf8);
		string_list = g_slist_append(string_list, strdup(new_string->str));
	}
	g_string_free(new_string, TRUE);
	g_string_free(append_utf8, TRUE);
	return string_list;
}

gboolean qq_im_smiley_none(const gchar *msg)
{
	const gchar *start, *end, *last;
	GData *attribs;
	gchar *tmp;
	gboolean ret = FALSE;

	g_return_val_if_fail(msg != NULL, TRUE);

	last = msg;
	while (purple_markup_find_tag("font", last, &start, &end, &attribs)) {
		tmp = g_datalist_get_data(&attribs, "sml");
		if (tmp && strlen(tmp) > 0) {
			if (strcmp(tmp, "none") == 0) {
				ret = TRUE;
				break;
			}
		}
		g_datalist_clear(&attribs);
		last = end + 1;
	}
	return ret;
}

/* Grab custom emote icons
static GSList*  qq_grab_emoticons(const char *msg, const char*username)
{
	GSList *list;
	GList *smileys;
	PurpleSmiley *smiley;
	const char *smiley_shortcut;
	char *ptr;
	int length;
	PurpleStoredImage *img;

	smileys = purple_smileys_get_all();
	length = strlen(msg);

	for (; smileys; smileys = g_list_delete_link(smileys, smileys)) {
		smiley = smileys->data;
		smiley_shortcut = purple_smiley_get_shortcut(smiley);
		purple_debug_info("QQ", "Smiley shortcut [%s]\n", smiley_shortcut);

		ptr = g_strstr_len(msg, length, smiley_shortcut);

		if (!ptr)
			continue;

		purple_debug_info("QQ", "Found Smiley shortcut [%s]\n", smiley_shortcut);

		img = purple_smiley_get_stored_image(smiley);

		emoticon = g_new0(MsnEmoticon, 1);
		emoticon->smile = g_strdup(purple_smiley_get_shortcut(smiley));
		emoticon->obj = msn_object_new_from_image(img,
				purple_imgstore_get_filename(img),
				username, MSN_OBJECT_EMOTICON);

 		purple_imgstore_unref(img);
		list = g_slist_prepend(list, emoticon);
	}
	return list;
}
*/

gint qq_send_im(PurpleConnection *gc, const gchar *who, const gchar *what, PurpleMessageFlags flags)
{
	qq_data *qd;
	guint32 uid_to;
	gint type;
	qq_im_format *fmt;
	gchar *msg_stripped, *tmp;
	GSList *segments, *it;
	gint msg_len;
	const gchar *start_invalid;
	gboolean is_smiley_none;
	guint8 frag_count, frag_index;
	guint8 msg_id;

	g_return_val_if_fail(NULL != gc && NULL != gc->proto_data, -1);
	g_return_val_if_fail(who != NULL && what != NULL, -1);

	qd = (qq_data *) gc->proto_data;
	purple_debug_info("QQ", "Send IM to %s, len %" G_GSIZE_FORMAT ":\n%s\n", who, strlen(what), what);

	uid_to = purple_name_to_uid(who);
	if (uid_to == qd->uid) {
		/* if msg is to myself, bypass the network */
		serv_got_im(gc, who, what, flags, time(NULL));
		return 1;
	}

	type = (flags == PURPLE_MESSAGE_AUTO_RESP ? QQ_IM_AUTO_REPLY : QQ_IM_TEXT);
	/* qq_show_packet("IM UTF8", (guint8 *)what, strlen(what)); */

	msg_stripped = purple_markup_strip_html(what);
	g_return_val_if_fail(msg_stripped != NULL, -1);
	/* qq_show_packet("IM Stripped", (guint8 *)what, strlen(what)); */

	/* Check and valid utf8 string */
	msg_len = strlen(msg_stripped);
	g_return_val_if_fail(msg_len > 0, -1);
	if (!g_utf8_validate(msg_stripped, msg_len, &start_invalid)) {
		if (start_invalid > msg_stripped) {
			tmp = g_strndup(msg_stripped, start_invalid - msg_stripped);
			g_free(msg_stripped);
			msg_stripped = g_strconcat(tmp, _("(Invalid UTF-8 string)"), NULL);
			g_free(tmp);
		} else {
			g_free(msg_stripped);
			msg_stripped = g_strdup(_("(Invalid UTF-8 string)"));
		}
	}

	is_smiley_none = qq_im_smiley_none(what);
	segments = qq_im_get_segments(msg_stripped, is_smiley_none);
	g_free(msg_stripped);

	if (segments == NULL) {
		return -1;
	}

	qd->send_im_id++;
	msg_id = (guint8)(qd->send_im_id && 0xFF);
	fmt = qq_im_fmt_new_by_purple(what);
	frag_count = g_slist_length(segments);
	frag_index = 0;
	for (it = segments; it; it = it->next) {
		/*
		request_send_im(gc, uid_to, type, fmt, (gchar *)it->data,
			msg_id, frag_count, frag_index);
		*/
		request_send_im(gc, uid_to, type, fmt, (gchar *)it->data, 0, 0, 0);
		g_free(it->data);
		frag_index++;
	}
	g_slist_free(segments);
	qq_im_fmt_free(fmt);
	return 1;
}
