#include "gnt.h"
#include "gntfilesel.h"

static void
file_selected(GntFileSel *sel, const char *path, const char *filename)
{
	g_printerr("%s %s\n", path, filename);
}

int main()
{
	freopen(".error", "w", stderr);
	fprintf(stdout, "\x1b]1;\x07\x1b]2;TEST\x07");
	gnt_init();

	GntWidget *w = gnt_file_sel_new();
	gnt_file_sel_set_current_location(GNT_FILE_SEL(w), "asd/home/asdasd/qweqweasd");
	gnt_file_sel_set_dirs_only(GNT_FILE_SEL(w), TRUE);
	gnt_file_sel_set_multi_select(GNT_FILE_SEL(w), TRUE);
	gnt_widget_show(w);

	g_signal_connect(G_OBJECT(w), "file_selected", G_CALLBACK(file_selected), NULL);

	gnt_main();

	gnt_quit();
	return 0;
}

