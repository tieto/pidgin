#include "mono-glue.h"
#include "mono-helper.h"
#include "debug.h"
#include "buddylist.h"
#include "signals.h"

typedef struct {
	MonoObject *func;
	char *signal;
	GType *types;
	GType ret_type;
	int num_vals;
} SignalData;

static PurpleCallback get_callback(SignalData *sig_data);

static gpointer dispatch_callback(SignalData *sig_data, int num_vals, ...)
{
	MonoArray *array;
	MonoObject *obj;
	int i;
	gpointer meth_args[1];
	gpointer purple_obj;

	va_list args;

	va_start(args, num_vals);

	array = mono_array_new(ml_get_domain(), mono_get_object_class(), num_vals);

	for (i = 0; i < num_vals; i++) {
		purple_obj = va_arg(args, gpointer);
		obj = ml_object_from_purple_type(sig_data->types[i], purple_obj);
		mono_array_set(array, MonoObject*, i, obj);
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


int purple_signal_connect_glue(MonoObject* h, MonoObject *plugin, MonoString *signal, MonoObject *func)
{
	char *sig;
	void **instance = NULL;
	SignalData *sig_data;
	PurpleMonoPlugin *mplug;
	MonoClass *klass;

	sig = mono_string_to_utf8(signal);
	purple_debug(PURPLE_DEBUG_INFO, "mono", "connecting signal: %s\n", sig);

	instance = (void*)mono_object_unbox(h);

	sig_data = g_new0(SignalData, 1);

	sig_data->func = func;
	sig_data->signal = sig;

	purple_signal_get_types(*instance, sig, &sig_data->ret_type, &sig_data->num_vals, &sig_data->types);

	klass = mono_object_get_class(plugin);

	mplug = ml_find_plugin_by_class(klass);

	mplug->signal_data = g_list_append(mplug->signal_data, (gpointer)sig_data);

	return purple_signal_connect(*instance, sig, (gpointer)klass, get_callback(sig_data), (gpointer)sig_data);
}

static int determine_index(GType type)
{
	switch (type) {
		case G_TYPE_STRING:
		case G_TYPE_POINTER:
			return 1;
		break;
		default:
			if (G_TYPE_IS_OBJECT(type) || G_TYPE_IS_BOXED(type))
				return 1;
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

static int callbacks_array_size = sizeof(callbacks) / sizeof(PurpleCallback);


static PurpleCallback get_callback(SignalData *sig_data)
{
	int i, index = 0;

	if (sig_data->ret_type == NULL)
		index = 0;
	else
		index = determine_index(sig_data->ret_type);

	for (i = 0; i < sig_data->num_vals; i++) {
		index += determine_index(sig_data->types[i]);
	}

	purple_debug(PURPLE_DEBUG_INFO, "mono", "get_callback index = %d\n", index);

	if (index >= callbacks_array_size || callbacks[index] == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "mono", "couldn't find a callback function for signal: %s\n", sig_data->signal);
		return NULL;
	}

	purple_debug(PURPLE_DEBUG_MISC, "mono", "using callback at index: %d\n", index);
	return PURPLE_CALLBACK(callbacks[index]);
}
