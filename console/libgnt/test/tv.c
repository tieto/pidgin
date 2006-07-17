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
				GNT_TEXT_FLAG_HIGHLIGHT);
		gnt_text_view_next_line(GNT_TEXT_VIEW(view));
		gnt_entry_clear(GNT_ENTRY(w));
		gnt_text_view_scroll(GNT_TEXT_VIEW(view), 0);

		return TRUE;
	}
	else if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_UP))
			gnt_text_view_scroll(GNT_TEXT_VIEW(view), 1);
		else if (strcmp(key+1, GNT_KEY_DOWN))
			gnt_text_view_scroll(GNT_TEXT_VIEW(view), -1);
	}
		
	return FALSE;
}

int main()
{
	GntWidget *hbox, *entry, *view;

	freopen(".error", "w", stderr);

	gnt_init();

	box(stdscr, 0, 0);
	wrefresh(stdscr);

	hbox = gnt_box_new(FALSE, TRUE);
	gnt_widget_set_name(hbox, "hbox");
	gnt_box_set_toplevel(GNT_BOX(hbox), TRUE);
	gnt_box_set_title(GNT_BOX(hbox), "Textview test");

	entry = gnt_entry_new(NULL);
	gnt_widget_set_name(entry, "entry");
	GNT_WIDGET_SET_FLAGS(entry, GNT_WIDGET_CAN_TAKE_FOCUS);

	view = gnt_text_view_new();
	gnt_widget_set_name(view, "view");

	gnt_widget_set_size(view, 20, 15);
	gnt_widget_set_size(entry, 20, 1);

	gnt_box_add_widget(GNT_BOX(hbox), view);
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_button_new("OK"));

	gnt_widget_show(hbox);

	g_signal_connect(G_OBJECT(entry), "key_pressed", G_CALLBACK(key_pressed), view);

	gnt_main();

	gnt_quit();

	return 0;
}

