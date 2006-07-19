#include "gntui.h"

#include "gntaccount.h"
#include "gntblist.h"
#include "gntconn.h"
#include "gntconv.h"
#include "gntnotify.h"

void init_gnt_ui()
{
#ifdef STANDALONE
	gnt_init();
#endif
	/* Accounts */
	gg_accounts_init();
	gaim_accounts_set_ui_ops(gg_accounts_get_ui_ops());

	/* Connections */
	gg_connections_init();
	gaim_connections_set_ui_ops(gg_connections_get_ui_ops());

	/* Initialize the buddy list */
	gg_blist_init();
	gaim_blist_set_ui_ops(gg_blist_get_ui_ops());

	/* Now the conversations */
	gg_conversation_init();
	gaim_conversations_set_ui_ops(gg_conv_get_ui_ops());

	/* Notify */
	gg_notify_init();
	gaim_notify_set_ui_ops(gg_notify_get_ui_ops());

#ifdef STANDALONE
	gnt_main();

	gaim_accounts_set_ui_ops(NULL);
	gg_accounts_uninit();

	gaim_connections_set_ui_ops(NULL);
	gg_connections_uninit();

	gaim_blist_set_ui_ops(NULL);
	gg_blist_uninit();

	gaim_conversations_set_ui_ops(NULL);
	gg_conversation_uninit();

	gaim_notify_set_ui_ops(NULL);
	gg_notify_uninit();

	gnt_quit();
#endif
}

