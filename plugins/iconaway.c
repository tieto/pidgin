#include "gtkinternal.h"

#include "conversation.h"
#include "signals.h"

#include "gtkconv.h"
#include "gtkplugin.h"

#define ICONAWAY_PLUGIN_ID "gtk-iconaway"

#ifdef _WIN32
__declspec(dllimport) GtkWidget *imaway;
#else
G_MODULE_IMPORT GtkWidget *imaway;
#endif

#ifdef USE_APPLET
extern void applet_destroy_buddy();
#endif

static void
iconify_windows(GaimAccount *account, char *state, char *message, void *data)
{
	GaimConvWindow *win;
	GList *windows;
	GaimConnection *gc;

	gc = gaim_account_get_connection(account);

	if (!imaway || !gc->away)
		return;

	gtk_window_iconify(GTK_WINDOW(imaway));
	gaim_blist_set_visible(FALSE);

	for (windows = gaim_get_windows();
		 windows != NULL;
		 windows = windows->next) {

		win = (GaimConvWindow *)windows->data;

		if (GAIM_IS_GTK_WINDOW(win)) {
			GaimGtkWindow *gtkwin;

			gtkwin = GAIM_GTK_WINDOW(win);

			gtk_window_iconify(GTK_WINDOW(gtkwin->window));
		}
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_accounts_get_handle(), "account-away",
						plugin, GAIM_CALLBACK(iconify_windows), NULL);

	return TRUE;
}

static GaimGtkPluginUiInfo ui_info =
{
	NULL                                            /**< get_config_frame */
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	ICONAWAY_PLUGIN_ID,                               /**< id             */
	N_("Iconify on Away"),                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Iconifies the buddy list and your conversations when you go away."),
	                                                  /**  description    */
	N_("Iconifies the buddy list and your conversations when you go away."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(iconaway, init_plugin, info)
