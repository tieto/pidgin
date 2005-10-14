#include <string.h>
#include "blist.h"
#include "mono-helper.h"

MonoObject* gaim_blist_get_handle_glue(void)
{
	void *handle = gaim_blist_get_handle();
	
	return mono_value_box(mono_loader_get_domain(), mono_get_intptr_class(), &handle);
}

MonoObject* gaim_blist_build_buddy_object(void* data)
{
	MonoObject *obj = NULL;
	MonoClass *klass = NULL;
			
	GaimBuddy *buddy = (GaimBuddy*)data;
	
	klass = mono_class_from_name(mono_loader_get_api_image(), "Gaim", "Buddy");
	if (!klass) {
		gaim_debug(GAIM_DEBUG_FATAL, "mono", "couldn't build the class!\n");
	}
	
	obj = mono_object_new(mono_loader_get_domain(), klass);
	if (!obj) {
		gaim_debug(GAIM_DEBUG_FATAL, "mono", "couldn't create the object!\n");
	}
	
	mono_runtime_object_init(obj);
	
	mono_loader_set_prop_string(obj, "Name", (char*)gaim_buddy_get_name(buddy));
	mono_loader_set_prop_string(obj, "Alias", (char*)gaim_buddy_get_alias(buddy));
	
	return obj;
}
