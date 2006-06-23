#include "gntui.h"

void init_gnt_ui()
{
	gnt_init();

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(stdscr);
	wrefresh(stdscr);

	/* Initialize the buddy list */
	gg_blist_init();
	gaim_blist_set_ui_ops(gg_blist_get_ui_ops());

	gnt_main();
}

