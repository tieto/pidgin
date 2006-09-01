#include "gntui.h"

#include "gntaccount.h"
#include "gntblist.h"
#include "gntconn.h"
#include "gntconv.h"
#include "gntdebug.h"
#include "gntnotify.h"
#include "gntplugin.h"
#include "gntprefs.h"
#include "gntrequest.h"
#include "gntstatus.h"

#include "internal.h"

#include <prefs.h>

void init_gnt_ui()
{
#ifdef STANDALONE
	gnt_init();
#endif

	gaim_prefs_add_none("/gaim/gnt");
	
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

	gg_request_init();
	gaim_request_set_ui_ops(gg_request_get_ui_ops());

	gnt_register_action(_("Accounts"), gg_accounts_show_all);
	gnt_register_action(_("Buddy List"), gg_blist_show);
	gnt_register_action(_("Debug Window"), gg_debug_window_show);
	gnt_register_action(_("Plugins"), gg_plugins_show_all);
	gnt_register_action(_("Preferences"), gg_prefs_show_all);
	gnt_register_action(_("Statuses"), gg_savedstatus_show_all);

#ifdef STANDALONE

	gg_plugins_save_loaded();

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

	gaim_request_set_ui_ops(NULL);
	gg_request_uninit();

	gnt_quit();
#endif
}

