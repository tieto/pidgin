#include "gntutils.h"

int main()
{
	GntWidget *win, *button;

	gnt_init();

	gnt_util_parse_widgets("<vwindow id='0' fill='0' align='2'><label>This is a test.</label><button id='1'>OK</button></vwindow>", 2, &win, &button);
	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), win);
	gnt_widget_show(win);

	gnt_main();

	gnt_quit();
	return 0;
}

