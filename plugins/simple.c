#include "config.h"
#include "gaim.h"

static gboolean
plugin_load(GaimPlugin *plugin)
{
	debug_printf("simple plugin loaded.\n");

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	debug_printf("simple plugin unloaded.\n");

	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	NULL,                                             /**< id             */
	N_("Simple Plugin"),                              /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Tests to see that most things are working."),
	                                                  /**  description    */
	N_("Tests to see that most things are working."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(simple, __init_plugin, info);
