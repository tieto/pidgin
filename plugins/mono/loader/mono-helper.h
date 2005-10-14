#ifndef _GAIM_MONO_LOADER_MONO_HELPER_H_
#define _GAIM_MONO_LOADER_MONO_HELPER_H_

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/tokentype.h>
#include "plugin.h"
#include "value.h"
#include "debug.h"

typedef struct {
	GaimPlugin *plugin;
	MonoAssembly *assm;
	MonoClass *klass;
	MonoObject *obj;	
	MonoMethod *init;
	MonoMethod *load;
	MonoMethod *unload;
	MonoMethod *destroy;
} GaimMonoPlugin;

MonoClass* mono_loader_find_plugin_class(MonoImage *image);

gchar* mono_loader_get_prop_string(MonoObject *obj, char *field);

void mono_loader_set_prop_string(MonoObject *obj, char *field, char *data);

gboolean mono_loader_is_api_dll(MonoImage *image);

MonoDomain* mono_loader_get_domain(void);

void mono_loader_set_domain(MonoDomain *d);

void mono_loader_init_internal_calls(void);

MonoObject* mono_loader_object_from_gaim_type(GaimType type, gpointer data);

MonoObject* mono_loader_object_from_gaim_subtype(GaimSubType type, gpointer data);

void mono_loader_set_api_image(MonoImage *image);

MonoImage* mono_loader_get_api_image();

/* hash table stuff; probably don't need it anymore */

void mono_loader_add_plugin(GaimMonoPlugin *plugin);

gboolean mono_loader_remove_plugin(GaimMonoPlugin *plugin);

gpointer mono_loader_find_plugin(GaimMonoPlugin *plugin);

gpointer mono_loader_find_plugin_by_class(MonoClass *klass);

GHashTable* mono_loader_get_plugin_hash();

#endif
