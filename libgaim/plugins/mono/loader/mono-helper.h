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
	
	GList *signal_data;
} GaimMonoPlugin;

gboolean ml_init(void);

void ml_uninit(void);

MonoObject* ml_invoke(MonoMethod *method, void *obj, void **params);

MonoObject* ml_delegate_invoke(MonoObject *method, void **params);

MonoClass* ml_find_plugin_class(MonoImage *image);

gchar* ml_get_prop_string(MonoObject *obj, char *field);

void ml_set_prop_string(MonoObject *obj, char *field, char *data);

gboolean ml_is_api_dll(MonoImage *image);

MonoDomain* ml_get_domain(void);

void ml_set_domain(MonoDomain *d);

void ml_init_internal_calls(void);

MonoObject* ml_object_from_gaim_type(GaimType type, gpointer data);

MonoObject* ml_object_from_gaim_subtype(GaimSubType type, gpointer data);

MonoObject* ml_create_api_object(char *class_name);

void ml_set_api_image(MonoImage *image);

MonoImage* ml_get_api_image(void);

/* hash table stuff; probably don't need it anymore */

void ml_add_plugin(GaimMonoPlugin *plugin);

gboolean ml_remove_plugin(GaimMonoPlugin *plugin);

gpointer ml_find_plugin(GaimMonoPlugin *plugin);

gpointer ml_find_plugin_by_class(MonoClass *klass);

GHashTable* ml_get_plugin_hash(void);

#endif
