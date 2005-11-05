#include "mono-glue.h"
#include "mono-helper.h"
#include "debug.h"
#include "blist.h"
#include "signals.h"
#include "value.h"

typedef struct {
	MonoObject *func;
	char *signal;
	GaimValue **values;
	GaimValue *ret_value;
	int num_vals;
} SignalData;

static GaimCallback get_callback(SignalData *sig_data);

static gpointer dispatch_callback(SignalData *sig_data, int num_vals, ...)
{
	MonoArray *array;
	MonoObject *obj;
	int i;
	gpointer meth_args[1];
	gpointer gaim_obj;
	
	va_list args;
	
	va_start(args, num_vals);
	
	array = mono_array_new(ml_get_domain(), mono_get_object_class(), num_vals);
	
	for (i = 0; i < num_vals; i++) {
		if (gaim_value_get_type(sig_data->values[i]) == GAIM_TYPE_SUBTYPE) {
			gaim_obj = va_arg(args, gpointer);
			obj = ml_object_from_gaim_subtype(gaim_value_get_subtype(sig_data->values[i]), gaim_obj);
			mono_array_set(array, MonoObject*, i, obj);
		} else {
			gaim_obj = va_arg(args, gpointer);
			obj = ml_object_from_gaim_type(gaim_value_get_type(sig_data->values[i]), gaim_obj);
			mono_array_set(array, MonoObject*, i, obj);
		}
	}
	
	va_end(args);
	
	meth_args[0] = array;
	
	return ml_delegate_invoke(sig_data->func, meth_args);	
}

static void cb_void__pointer(void *arg1, void *data)
{
	dispatch_callback((SignalData*)data, ((SignalData*)data)->num_vals, arg1);	
}

static void cb_void__pointer_pointer_pointer(void *arg1, void *arg2, void *arg3, void *data)
{
	dispatch_callback((SignalData*)data, ((SignalData*)data)->num_vals, arg1, arg2, arg3);	
}


int gaim_signal_connect_glue(MonoObject* h, MonoObject *plugin, MonoString *signal, MonoObject *func)
{
	char *sig;
	void **instance = NULL;
	SignalData *sig_data;
	GaimMonoPlugin *mplug;
	MonoClass *klass;
		
	sig = mono_string_to_utf8(signal);
	gaim_debug(GAIM_DEBUG_INFO, "mono", "connecting signal: %s\n", sig);
	
	instance = (void*)mono_object_unbox(h);
	
	sig_data = g_new0(SignalData, 1);
	
	sig_data->func = func;
	sig_data->signal = sig;
	
	gaim_signal_get_values(*instance, sig, &sig_data->ret_value, &sig_data->num_vals, &sig_data->values);
	
	klass = mono_object_get_class(plugin);
	
	mplug = ml_find_plugin_by_class(klass);
	
	mplug->signal_data = g_list_append(mplug->signal_data, (gpointer)sig_data);

	return gaim_signal_connect(*instance, sig, (gpointer)klass, get_callback(sig_data), (gpointer)sig_data);
}

static int determine_index(GaimType type)
{
	switch (type) {
		case GAIM_TYPE_SUBTYPE:
		case GAIM_TYPE_STRING:
		case GAIM_TYPE_OBJECT:
		case GAIM_TYPE_POINTER:
		case GAIM_TYPE_BOXED:
			return 1;
		break;
		default:
			return type;
		break;
	}
}

static gpointer callbacks[]= { 
										NULL,
										cb_void__pointer,
										NULL,
										cb_void__pointer_pointer_pointer
									};

static int callbacks_array_size = sizeof(callbacks) / sizeof(GaimCallback);
	

static GaimCallback get_callback(SignalData *sig_data)
{
	int i, index = 0;

	if (sig_data->ret_value == NULL)
		index = 0;
	else
		index = determine_index(gaim_value_get_type(sig_data->ret_value));
	
	for (i = 0; i < sig_data->num_vals; i++) {
		index += determine_index(gaim_value_get_type(sig_data->values[i]));
	}
	
	gaim_debug(GAIM_DEBUG_INFO, "mono", "get_callback index = %d\n", index);
	
	if (index >= callbacks_array_size || callbacks[index] == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "mono", "couldn't find a callback function for signal: %s\n", sig_data->signal);
		return NULL;
	}
	
	gaim_debug(GAIM_DEBUG_MISC, "mono", "using callback at index: %d\n", index);
	return GAIM_CALLBACK(callbacks[index]);
}
