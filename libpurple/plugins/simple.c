#include "internal.h"
#include "debug.h"
#include "plugins.h"
#include "version.h"

/** Plugin id : type-author-name (to guarantee uniqueness) */
#define SIMPLE_PLUGIN_ID "core-ewarmenhoven-simple"

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Eric Warmenhoven <eric@warmenhoven.org>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           SIMPLE_PLUGIN_ID,
		"name",         N_("Simple Plugin"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Testing"),
		"summary",      N_("Tests to see that most things are working."),
		"description",  N_("Tests to see that most things are working."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_debug(PURPLE_DEBUG_INFO, "simple", "simple plugin loaded.\n");

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_debug(PURPLE_DEBUG_INFO, "simple", "simple plugin unloaded.\n");

	return TRUE;
}

PURPLE_PLUGIN_INIT(simple, plugin_query, plugin_load, plugin_unload);
