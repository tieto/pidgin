#include "gntbutton.h"
#include "gnt.h"
#include "gntkeys.h"
#include "gnttree.h"
#include "gntbox.h"
#include "gntentry.h"
#include "gnttextview.h"

static gboolean
key_pressed(GntWidget *w, const char *key, GntWidget *view)
{
	if (key[0] == '\r' && key[1] == 0)
	{
		gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view),
				gnt_entry_get_text(GNT_ENTRY(w)),
				GNT_TEXT_FLAG_UNDERLINE | GNT_TEXT_FLAG_HIGHLIGHT);
		gnt_entry_add_to_history(GNT_ENTRY(w), gnt_entry_get_text(GNT_ENTRY(w)));
		gnt_text_view_next_line(GNT_TEXT_VIEW(view));
		gnt_entry_clear(GNT_ENTRY(w));
		if (gnt_text_view_get_lines_below(GNT_TEXT_VIEW(view)) <= 1)
			gnt_text_view_scroll(GNT_TEXT_VIEW(view), 0);
		gnt_entry_remove_suggest(GNT_ENTRY(w), "acb");

		return TRUE;
	}
	else if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(view), -1);
		else if (strcmp(key+1, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(GNT_TEXT_VIEW(view), 1);
		else
			return FALSE;
		return TRUE;
	}
		
	return FALSE;
}

int main()
{
	GntWidget *hbox, *entry, *view;

#ifdef STANDALONE
	freopen(".error", "w", stderr);

	gnt_init();
#endif

	hbox = gnt_box_new(FALSE, TRUE);
	gnt_widget_set_name(hbox, "hbox");
	gnt_box_set_toplevel(GNT_BOX(hbox), TRUE);
	gnt_box_set_fill(GNT_BOX(hbox), FALSE);
	gnt_box_set_title(GNT_BOX(hbox), "Textview test");
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);

	entry = gnt_entry_new(NULL);
	gnt_widget_set_name(entry, "entry");
	GNT_WIDGET_SET_FLAGS(entry, GNT_WIDGET_CAN_TAKE_FOCUS);

	gnt_entry_set_word_suggest(GNT_ENTRY(entry), TRUE);
	gnt_entry_set_always_suggest(GNT_ENTRY(entry), FALSE);
	gnt_entry_add_suggest(GNT_ENTRY(entry), "a");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "ab");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "abe");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "abc");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "abcde");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "abcd");
	gnt_entry_add_suggest(GNT_ENTRY(entry), "acb");

	view = gnt_text_view_new();
	gnt_widget_set_name(view, "view");

	gnt_widget_set_size(view, 20, 15);
	gnt_widget_set_size(entry, 20, 1);

	gnt_box_add_widget(GNT_BOX(hbox), view);
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_button_new("OK"));

	gnt_widget_show(hbox);

	gnt_entry_set_history_length(GNT_ENTRY(entry), -1);
	g_signal_connect_after(G_OBJECT(entry), "key_pressed", G_CALLBACK(key_pressed), view);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "plugins: ", GNT_TEXT_FLAG_BOLD);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "this is the 1st line\n", GNT_TEXT_FLAG_NORMAL);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "plugins: ", GNT_TEXT_FLAG_BOLD);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "this is the 2nd line\n", GNT_TEXT_FLAG_NORMAL);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "plugins: ", GNT_TEXT_FLAG_BOLD);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "this is the 3rd line\n", GNT_TEXT_FLAG_NORMAL);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "plugins: ", GNT_TEXT_FLAG_BOLD);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(view), "this is the 4th line\n", GNT_TEXT_FLAG_NORMAL);

#ifdef STANDALONE
	gnt_main();

	gnt_quit();
#endif

	return 0;
}

