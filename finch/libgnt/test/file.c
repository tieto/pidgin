#include "gnt.h"
#include "gntfilesel.h"

int main()
{
	freopen(".error", "w", stderr);
	gnt_init();

	GntWidget *w = gnt_file_sel_new();
	gnt_file_sel_set_current_location(GNT_FILE_SEL(w), "/home/");
	gnt_widget_show(w);

	gnt_main();

	gnt_quit();
	return 0;
}

