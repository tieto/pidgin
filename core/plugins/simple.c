#include "internal.h"
#include "debug.h"
#include "plugin.h"
#include "version.h"

/** Plugin id : type-author-name (to guarantee uniqueness) */
#define SIMPLE_PLUGIN_ID "core-ewarmenhoven-simple"

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_debug(GAIM_DEBUG_INFO, "simple", "simple plugin loaded.\n");

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_debug(GAIM_DEBUG_INFO, "simple", "simple plugin unloaded.\n");

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	SIMPLE_PLUGIN_ID,                                 /**< id             */
	N_("Simple Plugin"),                              /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Tests to see that most things are working."),
	                                                  /**  description    */
	N_("Tests to see that most things are working."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(simple, init_plugin, info)
