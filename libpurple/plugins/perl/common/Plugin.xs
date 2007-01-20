#include "module.h"

MODULE = Gaim::Plugin  PACKAGE = Gaim::Plugin  PREFIX = gaim_plugin_
PROTOTYPES: ENABLE

Gaim::Plugin
gaim_plugin_new(native, path)
	gboolean native
	const char *path

Gaim::Plugin
gaim_plugin_probe(filename)
	const char *filename

gboolean
gaim_plugin_register(plugin)
	Gaim::Plugin plugin

gboolean
gaim_plugin_load(plugin)
	Gaim::Plugin plugin

gboolean
gaim_plugin_unload(plugin)
	Gaim::Plugin plugin

gboolean
gaim_plugin_reload(plugin)
	Gaim::Plugin plugin

void
gaim_plugin_destroy(plugin)
	Gaim::Plugin plugin

gboolean
gaim_plugin_is_loaded(plugin)
	Gaim::Plugin plugin

gboolean
gaim_plugin_is_unloadable(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_id(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_name(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_version(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_summary(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_description(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_author(plugin)
	Gaim::Plugin plugin

const gchar *
gaim_plugin_get_homepage(plugin)
	Gaim::Plugin plugin

MODULE = Gaim::Plugin  PACKAGE = Gaim::Plugin::IPC  PREFIX = gaim_plugin_ipc_

void
gaim_plugin_ipc_unregister(plugin, command)
	Gaim::Plugin plugin
	const char *command

void
gaim_plugin_ipc_unregister_all(plugin)
	Gaim::Plugin plugin

MODULE = Gaim::Plugin  PACKAGE = Gaim::Plugins  PREFIX = gaim_plugins_
PROTOTYPES: ENABLE

void
gaim_plugins_add_search_path(path)
	const char *path

void
gaim_plugins_unload_all()

void
gaim_plugins_destroy_all()

void
gaim_plugins_load_saved(key)
	const char *key

void
gaim_plugins_probe(ext)
	const char *ext

gboolean
gaim_plugins_enabled()

Gaim::Plugin
gaim_plugins_find_with_name(name)
	const char *name

Gaim::Plugin
gaim_plugins_find_with_filename(filename)
	const char *filename

Gaim::Plugin
gaim_plugins_find_with_basename(basename)
	const char *basename

Gaim::Plugin
gaim_plugins_find_with_id(id)
	const char *id

void
gaim_plugins_get_loaded()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugins_get_loaded(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Plugin")));
	}

void
gaim_plugins_get_protocols()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Plugin")));
	}

void
gaim_plugins_get_all()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugins_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Plugin")));
	}

Gaim::Handle
gaim_plugins_get_handle()

void
gaim_plugins_init()

void
gaim_plugins_uninit()
