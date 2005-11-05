#ifndef _GAIM_MONO_LOADER_GLUE_H_
#define _GAIM_MONO_LOADER_GLUE_H_

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/assembly.h>

void gaim_debug_glue(int type, MonoString *cat, MonoString *str);

int gaim_signal_connect_glue(MonoObject *h, MonoObject *plugin, MonoString *signal, MonoObject *func);

MonoObject* gaim_blist_get_handle_glue(void);

MonoObject* gaim_blist_build_buddy_object(void* buddy);

MonoObject* gaim_status_build_status_object(void* data);

#endif
