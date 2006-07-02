#include "gntui.h"

#include "gntaccount.h"
#include "gntblist.h"
#include "gntconv.h"

void init_gnt_ui()
{
	gnt_init();

	/* Accounts */
	gg_accounts_init();
	gaim_accounts_set_ui_ops(gg_accounts_get_ui_ops());

	/* Initialize the buddy list */
	gg_blist_init();
	gaim_blist_set_ui_ops(gg_blist_get_ui_ops());

	/* Now the conversations */
	gg_conversation_init();
	gaim_conversations_set_ui_ops(gg_conv_get_ui_ops());

	gnt_main();

	gaim_accounts_set_ui_ops(NULL);
	gg_accounts_uninit();

	gaim_blist_set_ui_ops(NULL);
	gg_blist_uninit();

	gaim_conversations_set_ui_ops(NULL);
	gg_conversation_uninit();

	gnt_quit();
}

