#include "message-prpl.h"

#include <debug.h>

#include "gg.h"
#include "chat.h"
#include "utils.h"
#include "html.h"

#define GGP_GG10_DEFAULT_FORMAT "<span style=\"color:#000000; " \
	"font-family:'MS Shell Dlg 2'; font-size:9pt; \">"
#define GGP_GG10_DEFAULT_FORMAT_REPLACEMENT "<span>"
#define GGP_GG11_FORCE_COMPAT FALSE

#define GGP_IMAGE_REPLACEMENT "<img id=\"gg-pending-image-" GGP_IMAGE_ID_FORMAT "\">"
#define GGP_IMAGE_DESTINATION "<img src=\"" PURPLE_STORED_IMAGE_PROTOCOL "%u\">"

typedef struct
{
	enum
	{
		GGP_MESSAGE_GOT_TYPE_IM,
		GGP_MESSAGE_GOT_TYPE_CHAT,
		GGP_MESSAGE_GOT_TYPE_MULTILOGON
	} type;

	uin_t user;
	gchar *text;
	time_t time;
	uint64_t chat_id;
	GList *pending_images;

	PurpleConnection *gc;
} ggp_message_got_data;

typedef struct
{
	GRegex *re_html_tag;
	GRegex *re_gg_img;
} ggp_message_global_data;

static ggp_message_global_data global_data;

struct _ggp_message_session_data
{
	GList *pending_messages;
};

typedef struct
{
	int size;
	gchar *face;
	int color, bgcolor;
	gboolean b, i, u, s;
} ggp_font;

static ggp_font * ggp_font_new(void);
static ggp_font * ggp_font_clone(ggp_font *font);
static void ggp_font_free(gpointer font);

static PurpleConversation * ggp_message_get_conv(PurpleConnection *gc,
	uin_t uin);
static void ggp_message_got_data_free(ggp_message_got_data *msg);
static gboolean ggp_message_request_images(PurpleConnection *gc,
	ggp_message_got_data *msg);
static void ggp_message_got_display(PurpleConnection *gc,
	ggp_message_got_data *msg);
static void ggp_message_format_from_gg(ggp_message_got_data *msg,
	const gchar *text);

/**************/

void ggp_message_setup_global(void)
{
	global_data.re_html_tag = g_regex_new(
		"<(/)?([a-zA-Z]+)( [^>]+)?>",
		G_REGEX_OPTIMIZE, 0, NULL);
	global_data.re_gg_img = g_regex_new(
		"<img name=\"([0-9a-fA-F]+)\"/?>",
		G_REGEX_OPTIMIZE, 0, NULL);
}

void ggp_message_cleanup_global(void)
{
	g_regex_unref(global_data.re_html_tag);
	g_regex_unref(global_data.re_gg_img);
}

static inline ggp_message_session_data *
ggp_message_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->message_data;
}

void ggp_message_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_message_session_data *sdata = g_new0(ggp_message_session_data, 1);

	accdata->message_data = sdata;
}

void ggp_message_cleanup(PurpleConnection *gc)
{
	ggp_message_session_data *sdata = ggp_message_get_sdata(gc);
	
	g_list_free_full(sdata->pending_messages,
		(GDestroyNotify)ggp_message_got_data_free);
	g_free(sdata);
}

static ggp_font * ggp_font_new(void)
{
	ggp_font *font;

	font = g_new0(ggp_font, 1);
	font->color = -1;
	font->bgcolor = -1;

	return font;
}

static ggp_font * ggp_font_clone(ggp_font * font)
{
	ggp_font *clone = g_new0(ggp_font, 1);

	*clone = *font;
	clone->face = g_strdup(font->face);

	return clone;
}

static void ggp_font_free(gpointer _font)
{
	ggp_font *font = _font;

	g_free(font->face);
	g_free(font);
}

/**/

static PurpleConversation * ggp_message_get_conv(PurpleConnection *gc,
	uin_t uin)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleConversation *conv;
	const gchar *who = ggp_uin_to_str(uin);

	conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_IM, who, account);
	if (conv)
		return conv;
	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, who);
	return conv;
}

static void ggp_message_got_data_free(ggp_message_got_data *msg)
{
	g_list_free_full(msg->pending_images, g_free);
	g_free(msg->text);
	g_free(msg);
}

static void ggp_message_request_images_got(PurpleConnection *gc, uint64_t id,
	int stored_id, gpointer _msg)
{
	ggp_message_session_data *sdata = ggp_message_get_sdata(gc);
	ggp_message_got_data *msg = _msg;
	GList *m_it, *i_it;
	gchar *tmp, *tag_search, *tag_replace;

	m_it = g_list_find(sdata->pending_messages, msg);
	if (!m_it)
	{
		purple_debug_error("gg", "ggp_message_request_images_got: "
			"message %p is not in queue\n", msg);
		return;
	}

	i_it = g_list_find_custom(msg->pending_images, &id, ggp_int64_compare);
	if (!i_it)
	{
		purple_debug_error("gg", "ggp_message_request_images_got: "
			"image " GGP_IMAGE_ID_FORMAT " is not present in this "
			"message\n", id);
		return;
	}

	tag_search = g_strdup_printf(GGP_IMAGE_REPLACEMENT, id);
	tag_replace = g_strdup_printf(GGP_IMAGE_DESTINATION, stored_id);

	tmp = msg->text;
	msg->text = purple_strreplace(msg->text, tag_search, tag_replace);
	g_free(tmp);

	g_free(tag_search);
	g_free(tag_replace);

	g_free(i_it->data);
	msg->pending_images = g_list_delete_link(msg->pending_images, i_it);
	if (msg->pending_images != NULL)
	{
		purple_debug_info("gg", "ggp_message_request_images_got: "
			"got image " GGP_IMAGE_ID_FORMAT ", but the message "
			"contains more of them\n", id);
		return;
	}

	ggp_message_got_display(gc, msg);
	ggp_message_got_data_free(msg);
	sdata->pending_messages = g_list_delete_link(sdata->pending_messages,
		m_it);
}

static gboolean ggp_message_request_images(PurpleConnection *gc,
	ggp_message_got_data *msg)
{
	ggp_message_session_data *sdata = ggp_message_get_sdata(gc);
	GList *it;
	if (msg->pending_images == NULL)
		return FALSE;
	
	it = msg->pending_images;
	while (it)
	{
		ggp_image_request(gc, msg->user, *(uint64_t*)it->data,
			ggp_message_request_images_got, msg);
		it = g_list_next(it);
	}

	sdata->pending_messages = g_list_append(sdata->pending_messages, msg);

	return TRUE;
}

void ggp_message_got(PurpleConnection *gc, const struct gg_event_msg *ev)
{
	ggp_message_got_data *msg = g_new0(ggp_message_got_data, 1);

	msg->gc = gc;
	msg->time = ev->time;
	msg->user = ev->sender;

	if (ev->chat_id != 0)
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_CHAT;
		msg->chat_id = ev->chat_id;
	}
	else
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_IM;
	}

	ggp_message_format_from_gg(msg, ev->xhtml_message);
	if (ggp_message_request_images(gc, msg))
		return;

	ggp_message_got_display(gc, msg);
	ggp_message_got_data_free(msg);
}

void ggp_message_got_multilogon(PurpleConnection *gc,
	const struct gg_event_msg *ev)
{
	ggp_message_got_data *msg = g_new0(ggp_message_got_data, 1);

	msg->gc = gc;
	msg->time = ev->time;
	msg->user = ev->sender; /* not really a sender*/

	if (ev->chat_id != 0)
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_CHAT;
		msg->chat_id = ev->chat_id;
	}
	else
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_MULTILOGON;
	}

	ggp_message_format_from_gg(msg, ev->xhtml_message);
	if (ggp_message_request_images(gc, msg))
		return;

	ggp_message_got_display(gc, msg);
	ggp_message_got_data_free(msg);
}

static void ggp_message_got_display(PurpleConnection *gc,
	ggp_message_got_data *msg)
{
	if (msg->type == GGP_MESSAGE_GOT_TYPE_IM)
	{
		serv_got_im(gc, ggp_uin_to_str(msg->user), msg->text,
			PURPLE_MESSAGE_RECV, msg->time);
	}
	else if (msg->type == GGP_MESSAGE_GOT_TYPE_CHAT)
	{
		ggp_chat_got_message(gc, msg->chat_id, msg->text, msg->time,
			msg->user);
	}
	else if (msg->type == GGP_MESSAGE_GOT_TYPE_MULTILOGON)
	{
		PurpleConversation *conv = ggp_message_get_conv(gc, msg->user);
		const gchar *me = purple_account_get_username(
			purple_connection_get_account(gc));

		purple_conversation_write(conv, me, msg->text,
			PURPLE_MESSAGE_SEND, msg->time);
	}
	else
		purple_debug_error("gg", "ggp_message_got_display: "
			"unexpected message type: %d\n", msg->type);
}

static gboolean ggp_message_format_from_gg_found_img(const GMatchInfo *info,
	GString *res, gpointer data)
{
	ggp_message_got_data *msg = data;
	gchar *name, *replacement;
	int64_t id;
	int stored_id;

	name = g_match_info_fetch(info, 1);
	if (sscanf(name, "%" G_GINT64_MODIFIER "x", &id) != 1)
		id = 0;
	g_free(name);
	if (!id)
	{
		g_string_append(res, "[");
		g_string_append(res, _("broken image"));
		g_string_append(res, "]");
		return FALSE;
	}
	
	stored_id = ggp_image_get_cached(msg->gc, id);
	
	if (stored_id > 0)
	{
		purple_debug_info("gg", "ggp_message_format_from_gg_found_img: "
			"getting image " GGP_IMAGE_ID_FORMAT " from cache\n",
			id);
		replacement = g_strdup_printf(GGP_IMAGE_DESTINATION, stored_id);
		g_string_append(res, replacement);
		g_free(replacement);
		return FALSE;
	}
	
	if (NULL == g_list_find_custom(msg->pending_images, &id,
		ggp_int64_compare))
	{
		msg->pending_images = g_list_append(msg->pending_images,
			ggp_uint64dup(id));
	}
	
	replacement = g_strdup_printf(GGP_IMAGE_REPLACEMENT, id);
	g_string_append(res, replacement);
	g_free(replacement);

	return FALSE;
}

static void ggp_message_format_from_gg(ggp_message_got_data *msg,
	const gchar *text)
{
	gchar *text_new, *tmp;

	if (text == NULL)
	{
		msg->text = g_strdup("");
		return;
	}

	text_new = g_strdup(text);
	purple_str_strip_char(text_new, '\r');

	tmp = text_new;
	text_new = purple_strreplace(text_new, GGP_GG10_DEFAULT_FORMAT,
		GGP_GG10_DEFAULT_FORMAT_REPLACEMENT);
	g_free(tmp);

	tmp = text_new;
	text_new = g_regex_replace_eval(global_data.re_gg_img, text_new, -1, 0,
		0, ggp_message_format_from_gg_found_img, msg, NULL);
	g_free(tmp);

	msg->text = text_new;
}

gchar * ggp_message_format_to_gg(PurpleConversation *conv, const gchar *text)
{
	gchar *text_new, *tmp;
	GList *rt = NULL; /* reformatted text */
	GMatchInfo *match;
	int pos = 0;
	GList *pending_objects = NULL;
	GList *font_stack = NULL;
	static int html_sizes_pt[7] = { 7, 8, 9, 10, 12, 14, 16 };

	ggp_font *font_new, *font_current, *font_base;
	gboolean font_changed = FALSE;
	gboolean in_any_tag = FALSE;

	/* TODO: verbose
	 * purple_debug_info("gg", "ggp formatting text: [%s]\n", text);
	 */

	/* default font */
	font_base = ggp_font_new();
	font_current = ggp_font_new();
	font_new = ggp_font_new();

	/* GG11 doesn't use nbsp, it just print spaces */
	text_new = purple_strreplace(text, "&nbsp;", " ");

	/* add end-of-message tag */
	if (strstr(text_new, "<eom>") != NULL)
	{
		tmp = text_new;
		text_new = purple_strreplace(text_new, "<eom>", "");
		g_free(tmp);
		purple_debug_warning("gg", "ggp_message_format_to_gg: "
			"unexpected <eom> tag\n");
	}
	tmp = text_new;
	text_new = g_strdup_printf("%s<eom></eom>", text_new);
	g_free(tmp);

	g_regex_match(global_data.re_html_tag, text_new, 0, &match);
	while (g_match_info_matches(match))
	{
		int m_start, m_end, m_pos;
		gboolean tag_close;
		gchar *tag_str, *attribs_str;
		ggp_html_tag tag;
		gboolean text_before;

		/* reading tag and its contents */
		g_match_info_fetch_pos(match, 0, &m_start, &m_end);
		g_assert(m_start >= 0 && m_end >= 0);
		text_before = (m_start > pos);
		g_match_info_fetch_pos(match, 1, &m_pos, NULL);
		tag_close = (m_pos >= 0);
		tag_str = g_match_info_fetch(match, 2);
		tag = ggp_html_parse_tag(tag_str);
		attribs_str = g_match_info_fetch(match, 3);
		g_match_info_next(match, NULL);

		if (tag == GGP_HTML_TAG_UNKNOWN)
			purple_debug_warning("gg", "ggp_message_format_to_gg: "
				"uknown tag %s\n", tag_str);

		/* closing *all* formatting-related tags (GG11 weirness)
		 * and adding pending objects */
		if ((text_before && (font_changed || pending_objects)) ||
			(tag == GGP_HTML_TAG_EOM && tag_close))
		{
			font_changed = FALSE;
			if (in_any_tag)
			{
				in_any_tag = FALSE;
				if (font_current->s && !GGP_GG11_FORCE_COMPAT)
					rt = g_list_prepend(rt,
						g_strdup("</s>"));
				if (font_current->u)
					rt = g_list_prepend(rt,
						g_strdup("</u>"));
				if (font_current->i)
					rt = g_list_prepend(rt,
						g_strdup("</i>"));
				if (font_current->b)
					rt = g_list_prepend(rt,
						g_strdup("</b>"));
				rt = g_list_prepend(rt, g_strdup("</span>"));
			}
			if (pending_objects)
			{
				rt = g_list_concat(pending_objects, rt);
				pending_objects = NULL;
			}
		}
		
		/* opening formatting-related tags again */
		if (text_before && !in_any_tag)
		{
			gchar *style;
			GList *styles = NULL;
			gboolean has_size = (font_new->size > 0 &&
				font_new->size <= 7 && font_new->size != 3);
			
			if (has_size)
				styles = g_list_append(styles, g_strdup_printf(
					"font-size:%dpt;",
					html_sizes_pt[font_new->size - 1]));
			if (font_new->face)
				styles = g_list_append(styles, g_strdup_printf(
					"font-family:%s;", font_new->face));
			if (font_new->bgcolor >= 0 && !GGP_GG11_FORCE_COMPAT)
				styles = g_list_append(styles, g_strdup_printf(
					"background-color:#%06x;",
					font_new->bgcolor));
			if (font_new->color >= 0)
				styles = g_list_append(styles, g_strdup_printf(
					"color:#%06x;", font_new->color));
			
			if (styles)
			{
				gchar *combined = ggp_strjoin_list(" ", styles);
				g_list_free_full(styles, g_free);
				style = g_strdup_printf(" style=\"%s\"",
					combined);
				g_free(combined);
			}
			else
				style = g_strdup("");
			rt = g_list_prepend(rt, g_strdup_printf("<span%s>",
				style));
			g_free(style);

			if (font_new->b)
				rt = g_list_prepend(rt, g_strdup("<b>"));
			if (font_new->i)
				rt = g_list_prepend(rt, g_strdup("<i>"));
			if (font_new->u)
				rt = g_list_prepend(rt, g_strdup("<u>"));
			if (font_new->s && !GGP_GG11_FORCE_COMPAT)
				rt = g_list_prepend(rt, g_strdup("<s>"));

			ggp_font_free(font_current);
			font_current = font_new;
			font_new = ggp_font_clone(font_current);

			in_any_tag = TRUE;
		}
		if (text_before)
		{
			rt = g_list_prepend(rt,
				g_strndup(text_new + pos, m_start - pos));
		}

		/* set formatting of a following text */
		if (tag == GGP_HTML_TAG_B)
		{
			font_changed |= (font_new->b != !tag_close);
			font_new->b = !tag_close;
		}
		else if (tag == GGP_HTML_TAG_I)
		{
			font_changed |= (font_new->i != !tag_close);
			font_new->i = !tag_close;
		}
		else if (tag == GGP_HTML_TAG_U)
		{
			font_changed |= (font_new->u != !tag_close);
			font_new->u = !tag_close;
		}
		else if (tag == GGP_HTML_TAG_S)
		{
			font_changed |= (font_new->s != !tag_close);
			font_new->s = !tag_close;
		}
		else if (tag == GGP_HTML_TAG_IMG && !tag_close)
		{
			GHashTable *attribs = ggp_html_tag_attribs(attribs_str);
			gchar *val = NULL;
			uint64_t id;
			int stored_id = -1;
			ggp_image_prepare_result res = -1;
		
			if ((val = g_hash_table_lookup(attribs, "src")) != NULL
				&& g_str_has_prefix(val,
				PURPLE_STORED_IMAGE_PROTOCOL))
			{
				val += strlen(PURPLE_STORED_IMAGE_PROTOCOL);
				if (sscanf(val, "%u", &stored_id) != 1)
					stored_id = -1;
			}
			
			if (stored_id >= 0)
				res = ggp_image_prepare(conv, stored_id, &id);
			
			if (res == GGP_IMAGE_PREPARE_OK)
			{
				pending_objects = g_list_prepend(
					pending_objects, g_strdup_printf(
					"<img name=\"" GGP_IMAGE_ID_FORMAT
					"\">", id));
			}
			else if (res == GGP_IMAGE_PREPARE_TOO_BIG)
			{
				purple_conversation_write(conv, "",
					_("Image is too large, please try "
					"smaller one."), PURPLE_MESSAGE_ERROR,
					time(NULL));
			}
			else
			{
				purple_conversation_write(conv, "",
					_("Image cannot be sent."),
					PURPLE_MESSAGE_ERROR, time(NULL));
			}
			
			g_hash_table_destroy(attribs);
		}
		else if (tag == GGP_HTML_TAG_FONT && !tag_close)
		{
			GHashTable *attribs = ggp_html_tag_attribs(attribs_str);
			gchar *val = NULL;

			font_stack = g_list_prepend(font_stack,
				ggp_font_clone(font_new));

			if ((val = g_hash_table_lookup(attribs, "size")) != NULL
				&& val[0] >= '1' && val[0] <= '7' &&
				val[1] == '\0')
			{
				int size = val[0] - '0';
				font_changed |= (font_new->size != size);
				font_new->size = size;
			}

			if ((val = g_hash_table_lookup(attribs, "face"))
				!= NULL)
			{
				font_changed |=
					(g_strcmp0(font_new->face, val) != 0);
				g_free(font_new->face);
				font_new->face = g_strdup(val);
			}

			if ((val = g_hash_table_lookup(attribs, "color"))
				!= NULL && val[0] == '#' && strlen(val) == 7)
			{
				int color = ggp_html_decode_color(val);
				font_changed |= (font_new->color != color);
				font_new->color = color;
			}

			g_hash_table_destroy(attribs);
		}
		else if ((tag == GGP_HTML_TAG_SPAN || tag == GGP_HTML_TAG_DIV)
			&& !tag_close)
		{
			GHashTable *attribs, *styles = NULL;
			gchar *style = NULL;
			gchar *val = NULL;

			attribs = ggp_html_tag_attribs(attribs_str);

			font_stack = g_list_prepend(font_stack,
				ggp_font_clone(font_new));
			if (tag == GGP_HTML_TAG_DIV)
				pending_objects = g_list_prepend(
					pending_objects, g_strdup("<br>"));

			style = g_hash_table_lookup(attribs, "style");
			if (style)
				styles = ggp_html_css_attribs(style);

			if ((val = g_hash_table_lookup(styles,
				"background-color")) != NULL)
			{
				int color = ggp_html_decode_color(val);
				font_changed |= (font_new->bgcolor != color);
				font_new->bgcolor = color;
			}

			if ((val = g_hash_table_lookup(styles,
				"color")) != NULL)
			{
				int color = ggp_html_decode_color(val);
				font_changed |= (font_new->color != color);
				font_new->color = color;
			}

			if (styles)
				g_hash_table_destroy(styles);
			g_hash_table_destroy(attribs);
		}
		else if ((tag == GGP_HTML_TAG_FONT || tag == GGP_HTML_TAG_SPAN
			|| tag == GGP_HTML_TAG_DIV) && tag_close)
		{
			font_changed = TRUE;
			
			ggp_font_free(font_new);
			if (font_stack)
			{
				font_new = (ggp_font*)font_stack->data;
				font_stack = g_list_delete_link(
					font_stack, font_stack);
			}
			else
				font_new = ggp_font_clone(font_base);
		}
		else if (tag == GGP_HTML_TAG_BR)
		{
			pending_objects = g_list_prepend(pending_objects,
				g_strdup("<br>"));
		}
		else if (tag == GGP_HTML_TAG_HR)
		{
			pending_objects = g_list_prepend(pending_objects,
				g_strdup("<br><span>---</span><br>"));
		}
		else if (tag == GGP_HTML_TAG_A || tag == GGP_HTML_TAG_EOM)
		{
			/* do nothing */
		}
		else if (tag == GGP_HTML_TAG_UNKNOWN)
		{
			purple_debug_warning("gg", "ggp_message_format_to_gg: "
				"uknown tag %s\n", tag_str);
		}
		else
		{
			purple_debug_error("gg", "ggp_message_format_to_gg: "
				"not handled tag %s\n", tag_str);
		}

		pos = m_end;
		g_free(tag_str);
		g_free(attribs_str);
	}
	g_match_info_free(match);

	if (pos < strlen(text_new) || in_any_tag)
	{
		purple_debug_fatal("gg", "ggp_message_format_to_gg: "
			"end of message not reached\n");
	}

	/* releasing fonts recources */
	ggp_font_free(font_new);
	ggp_font_free(font_current);
	ggp_font_free(font_base);
	g_list_free_full(font_stack, ggp_font_free);

	/* combining reformatted text info one string */
	rt = g_list_reverse(rt);
	g_free(text_new);
	text_new = ggp_strjoin_list("", rt);
	g_list_free_full(rt, g_free);

	/* TODO: verbose
	 * purple_debug_info("gg", "reformatted text: [%s]\n", text_new);
	 */

	return text_new;
}

int ggp_message_send_im(PurpleConnection *gc, const char *who,
	const char *message, PurpleMessageFlags flags)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	PurpleConversation *conv;
	ggp_buddy_data *buddy_data;
	gchar *gg_msg;
	gboolean succ;

	/* TODO: return -ENOTCONN, if not connected */

	if (message == NULL || message[0] == '\0')
		return 0;

	buddy_data = ggp_buddy_get_data(purple_find_buddy(
		purple_connection_get_account(gc), who));

	if (buddy_data->blocked)
		return -1;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
		who, purple_connection_get_account(gc));

	gg_msg = ggp_message_format_to_gg(conv, message);

	/* TODO: splitting messages */
	if (strlen(gg_msg) > GG_MSG_MAXSIZE)
	{
		g_free(gg_msg);
		return -E2BIG;
	}

	succ = (gg_send_message_html(info->session, GG_CLASS_CHAT,
		ggp_str_to_uin(who), (unsigned char *)gg_msg) >= 0);

	g_free(gg_msg);

	return succ ? 1 : -1;
}
