#include <gnt.h>
#include <gntbox.h>
#include <gntentry.h>
#include <gntlabel.h>

static gboolean
print_keycode(GntEntry *entry, const char *text, gpointer null)
{
	char *s = g_strdup_printf("%s ", text);
	gnt_entry_set_text(entry, s);
	g_free(s);
	if (text[0] == 27)
		return FALSE;
	else
		return TRUE;
}

int main()
{
	GntWidget *window, *entry;

	gnt_init();

	freopen(".error", "w", stderr);

	window = gnt_hbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);

	gnt_box_add_widget(GNT_BOX(window), gnt_label_new("Press any key: "));

	entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(window), entry);
	g_signal_connect(G_OBJECT(entry), "key_pressed", G_CALLBACK(print_keycode), NULL);

	gnt_widget_set_position(window, getmaxx(stdscr) / 2 - 12, getmaxy(stdscr) / 2 - 3);
	gnt_widget_show(window);

	gnt_main();
	gnt_quit();
	return 0;
}

