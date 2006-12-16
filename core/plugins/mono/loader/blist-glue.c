#include <string.h>
#include "blist.h"
#include "mono-helper.h"
#include "mono-glue.h"

MonoObject* gaim_blist_get_handle_glue(void)
{
	void *handle = gaim_blist_get_handle();
	
	return mono_value_box(ml_get_domain(), mono_get_intptr_class(), &handle);
}

MonoObject* gaim_blist_build_buddy_object(void* data)
{
	MonoObject *obj = NULL;
			
	GaimBuddy *buddy = (GaimBuddy*)data;
	
	obj = ml_create_api_object("Buddy");
	g_return_val_if_fail(obj != NULL, NULL);
		
	ml_set_prop_string(obj, "Name", (char*)gaim_buddy_get_name(buddy));
	ml_set_prop_string(obj, "Alias", (char*)gaim_buddy_get_alias(buddy));
	
	return obj;
}
