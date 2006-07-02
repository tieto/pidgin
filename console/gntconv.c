#include <string.h>
#include <util.h>

#include "gntgaim.h"
#include "gntconv.h"

#include "gnt.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gnttextview.h"

GHashTable *ggconvs;

typedef struct _GGConv GGConv;
typedef struct _GGConvChat GGConvChat;
typedef struct _GGConvIm GGConvIm;

struct _GGConv
{
	GaimConversation *conv;

	GntWidget *window;        /* the container */
	GntWidget *entry;         /* entry */
	GntWidget *tv;            /* text-view */

	union
	{
		GGConvChat *chat;
		GGConvIm *im;
	} u;
};

struct _GGConvChat
{
	GntWidget *userlist;       /* the userlist */
};

struct _GGConvIm
{
	void *nothing_for_now;
};

static gboolean
entry_key_pressed(GntWidget *w, const char *key, GGConv *ggconv)
{
	if (key[0] == '\r' && key[1] == 0)
	{
		const char *text = gnt_entry_get_text(GNT_ENTRY(ggconv->entry));
		switch (gaim_conversation_get_type(ggconv->conv))
		{
			case GAIM_CONV_TYPE_IM:
				gaim_conv_im_send_with_flags(GAIM_CONV_IM(ggconv->conv), text, GAIM_MESSAGE_SEND);
				break;
			case GAIM_CONV_TYPE_CHAT:
				gaim_conv_chat_send(GAIM_CONV_CHAT(ggconv->conv), text);
				break;
			default:
				g_return_val_if_reached(FALSE);
		}
		gnt_entry_clear(GNT_ENTRY(ggconv->entry));
		return TRUE;
	}
	else if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 1);
		else if (strcmp(key+1, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), -1);
		else if (strcmp(key+1, GNT_KEY_PGDOWN) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), ggconv->tv->priv.height - 2);
		else if (strcmp(key+1, GNT_KEY_PGUP) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), -(ggconv->tv->priv.height - 2));
		else
			return FALSE;
		return TRUE;
	}

	return FALSE;
}

static void
closing_window(GntWidget *window, GGConv *ggconv)
{
	ggconv->window = NULL;
	gaim_conversation_destroy(ggconv->conv);
}

static void
gg_create_conversation(GaimConversation *conv)
{
	GGConv *ggc = g_hash_table_lookup(ggconvs, conv);
	char *title;
	GaimConversationType type;

	if (ggc)
		return;

	ggc = g_new0(GGConv, 1);
	g_hash_table_insert(ggconvs, conv, ggc);

	ggc->conv = conv;

	type = gaim_conversation_get_type(conv);
	title = g_strdup_printf(_("%s"), gaim_conversation_get_name(conv));
	ggc->window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_title(GNT_BOX(ggc->window), title);
	gnt_box_set_toplevel(GNT_BOX(ggc->window), TRUE);
	gnt_widget_set_name(ggc->window, title);

	ggc->tv = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->tv);
	gnt_widget_set_name(ggc->tv, "conversation-window-textview");
	gnt_widget_set_size(ggc->tv, getmaxx(stdscr) - 40, getmaxy(stdscr) - 15);

	ggc->entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggc->window), ggc->entry);
	gnt_widget_set_name(ggc->entry, "conversation-window-entry");
	gnt_widget_set_size(ggc->entry, getmaxx(stdscr) - 40, 1);

	g_signal_connect(G_OBJECT(ggc->entry), "key_pressed", G_CALLBACK(entry_key_pressed), ggc);
	g_signal_connect(G_OBJECT(ggc->window), "destroy", G_CALLBACK(closing_window), ggc);

	gnt_widget_set_position(ggc->window, 32, 0);
	gnt_widget_show(ggc->window);

	g_free(title);
}

static void
gg_destroy_conversation(GaimConversation *conv)
{
	g_hash_table_remove(ggconvs, conv);
}

static void
gg_write_common(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	GGConv *ggconv = g_hash_table_lookup(ggconvs, conv);
	char *strip;
	GntTextViewFlags fl = 0;

	g_return_if_fail(ggconv != NULL);

	if (who && *who && (flags & (GAIM_MESSAGE_SEND | GAIM_MESSAGE_RECV)))
	{
		char * name = g_strdup_printf("%s: ", who);
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				name, GNT_TEXT_FLAG_BOLD);
		g_free(name);
	}
	else
		fl = GNT_TEXT_FLAG_DIM;

	if (flags & GAIM_MESSAGE_ERROR)
		fl |= GNT_TEXT_FLAG_BOLD;
	if (flags & GAIM_MESSAGE_NICK)
		fl |= GNT_TEXT_FLAG_UNDERLINE;

	strip = gaim_markup_strip_html(message);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(ggconv->tv),
				strip, fl);
	gnt_text_view_next_line(GNT_TEXT_VIEW(ggconv->tv));
	gnt_text_view_scroll(GNT_TEXT_VIEW(ggconv->tv), 0);

	g_free(strip);

	if (flags & (GAIM_MESSAGE_RECV | GAIM_MESSAGE_NICK | GAIM_MESSAGE_ERROR))
		gnt_widget_set_urgent(ggconv->tv);
}

static void
gg_write_chat(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	gg_write_common(conv, who, message, flags, mtime);
}

static void
gg_write_im(GaimConversation *conv, const char *who, const char *message,
		GaimMessageFlags flags, time_t mtime)
{
	if (flags & GAIM_MESSAGE_SEND)
	{
		who = gaim_connection_get_display_name(conv->account->gc);
		if (!who)
			who = gaim_account_get_alias(conv->account);
		if (!who)
			who = gaim_account_get_username(conv->account);
	}
	else if (flags & GAIM_MESSAGE_RECV)
		who = gaim_conversation_get_name(conv);

	gg_write_common(conv, who, message, flags, mtime);
}

static void
gg_write_conv(GaimConversation *conv, const char *who, const char *alias,
		const char *message, GaimMessageFlags flags, time_t mtime)
{
	const char *name;
	if (alias && *alias)
		name = alias;
	else if (who && *who)
		name = who;
	else
		name = NULL;

	gg_write_common(conv, name, message, flags, mtime);
}

static void
gg_chat_add_users(GaimConversation *conv, GList *users, GList *flags, GList *aliases, gboolean new_arrivals)
{}

static void
gg_chat_rename_user(GaimConversation *conv, const char *old, const char *new_n, const char *new_a)
{}

static void
gg_chat_remove_user(GaimConversation *conv, GList *list)
{}

static void
gg_chat_update_user(GaimConversation *conv, const char *user)
{}

static GaimConversationUiOps conv_ui_ops = 
{
	.create_conversation = gg_create_conversation,
	.destroy_conversation = gg_destroy_conversation,
	.write_chat = gg_write_chat,
	.write_im = gg_write_im,
	.write_conv = gg_write_conv,
	.chat_add_users = gg_chat_add_users,
	.chat_rename_user = gg_chat_rename_user,
	.chat_remove_users = gg_chat_remove_user,
	.chat_update_user = gg_chat_update_user,
	.present = NULL,
	.has_focus = NULL,
	.custom_smiley_add = NULL,
	.custom_smiley_write = NULL,
	.custom_smiley_close = NULL
};

static void
destroy_ggconv(gpointer data)
{
	GGConv *conv = data;
	gnt_widget_destroy(conv->window);
	g_free(conv);
}

GaimConversationUiOps *gg_conv_get_ui_ops()
{
	return &conv_ui_ops;
}


void gg_conversation_init()
{
	ggconvs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, destroy_ggconv);
}

void gg_conversation_uninit()
{
	g_hash_table_destroy(ggconvs);
	ggconvs = NULL;
}

