#include "status.h"
#include "mono-helper.h"

MonoObject* gaim_status_build_status_object(void* data)
{
	MonoObject *obj = NULL;
	MonoClass *klass = NULL;
			
	GaimStatus *status = (GaimStatus*)data;
	
	klass = mono_class_from_name(ml_get_api_image(), "Gaim", "Status");
	if (!klass) {
		gaim_debug(GAIM_DEBUG_FATAL, "mono", "couldn't build the class!\n");
	}
	
	obj = mono_object_new(ml_get_domain(), klass);
	if (!obj) {
		gaim_debug(GAIM_DEBUG_FATAL, "mono", "couldn't create the object!\n");
	}
	
	mono_runtime_object_init(obj);
	
	ml_set_prop_string(obj, "Id", (char*)gaim_status_get_id(status));
	
	return obj;
}
