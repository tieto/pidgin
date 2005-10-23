/*
 * Mono Plugin Loader
 *
 * -- Thanks to the perl plugin loader for all the great tips ;-)
 *
 * Eoin Coffey
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <string.h>
#include "mono-helper.h"
#include "mono-glue.h"
#include "value.h"

MonoClass* mono_loader_find_plugin_class(MonoImage *image)
{
	MonoClass *klass, *pklass = NULL;
	int i, total;

	total = mono_image_get_table_rows (image, MONO_TABLE_TYPEDEF);
	for (i = 1; i <= total; ++i) {
		klass = mono_class_get (image, MONO_TOKEN_TYPE_DEF | i);
		pklass = mono_class_get_parent(klass);
		if (pklass) 
			if (strcmp("GaimPlugin", mono_class_get_name(pklass)) == 0)
				return klass;
	}
	
	return NULL;
}

void mono_loader_set_prop_string(MonoObject *obj, char *field, char *data)
{
	MonoClass *klass;
	MonoProperty *prop;
	MonoString *str;
	gpointer args[1];
	
	klass = mono_object_get_class(obj);
	
	prop = mono_class_get_property_from_name(klass, field);
	
	str = mono_string_new(mono_loader_get_domain(), data);
	
	args[0] = str;
	
	mono_property_set_value(prop, obj, args, NULL);
}

gchar* mono_loader_get_prop_string(MonoObject *obj, char *field)
{
	MonoClass *klass;
	MonoProperty *prop;
	MonoString *str;
	
	klass = mono_object_get_class(obj);
	
	prop = mono_class_get_property_from_name(klass, field);
	
	str = (MonoString*)mono_property_get_value(prop, obj, NULL, NULL);
	
	return mono_string_to_utf8(str);
}

gboolean mono_loader_is_api_dll(MonoImage *image)
{	
	MonoClass *klass;
	int i, total;

	total = mono_image_get_table_rows (image, MONO_TABLE_TYPEDEF);
	for (i = 1; i <= total; ++i) {
		klass = mono_class_get (image, MONO_TOKEN_TYPE_DEF | i);
		if (strcmp(mono_class_get_name(klass), "Debug") == 0)
			if (strcmp(mono_class_get_namespace(klass), "Gaim") == 0) {
				mono_loader_set_api_image(image);
				return TRUE;
			}
	}
	
	return FALSE;
}

MonoObject* mono_loader_object_from_gaim_type(GaimType type, gpointer data)
{
	return NULL;
}

MonoObject* mono_loader_object_from_gaim_subtype(GaimSubType type, gpointer data)
{
	MonoObject *obj = NULL;
	
	switch (type) {
		case GAIM_SUBTYPE_BLIST_BUDDY:
			obj = gaim_blist_build_buddy_object(data);
		break;
		default:
		break;
	}
	
	return obj;
}

static MonoDomain *_domain;

MonoDomain* mono_loader_get_domain(void)
{
	return _domain;
}

void mono_loader_set_domain(MonoDomain *d)
{
	_domain = d;
}

static MonoImage *_api_image = NULL;

void mono_loader_set_api_image(MonoImage *image)
{
	_api_image = image;
}

MonoImage* mono_loader_get_api_image()
{
	return _api_image;
}

void mono_loader_init_internal_calls(void)
{
	mono_add_internal_call("Gaim.Debug::_debug", gaim_debug_glue);
	mono_add_internal_call("Gaim.Signal::_connect", gaim_signal_connect_glue);
	mono_add_internal_call("Gaim.BuddyList::_get_handle", gaim_blist_get_handle_glue);
}

static GHashTable *plugins_hash = NULL;

void mono_loader_add_plugin(GaimMonoPlugin *plugin)
{
	if (!plugins_hash)
		plugins_hash = g_hash_table_new(NULL, NULL);
		
	g_hash_table_insert(plugins_hash, plugin->klass, plugin);
}

gboolean mono_loader_remove_plugin(GaimMonoPlugin *plugin)
{
	return g_hash_table_remove(plugins_hash, plugin->klass);
}

gpointer mono_loader_find_plugin(GaimMonoPlugin *plugin)
{
	return g_hash_table_lookup(plugins_hash, plugin->klass);
}

gpointer mono_loader_find_plugin_by_class(MonoClass *klass)
{
	return g_hash_table_lookup(plugins_hash, klass);
}

GHashTable* mono_loader_get_plugin_hash()
{
	return plugins_hash;
}
