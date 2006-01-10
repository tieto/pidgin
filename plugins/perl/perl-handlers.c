#include "perl-common.h"
#include "perl-handlers.h"

#include "debug.h"
#include "signals.h"

static GList *timeout_handlers = NULL;
static GList *signal_handlers = NULL;
static char *perl_plugin_pref_cb;
static char *perl_gtk_plugin_pref_cb;
extern PerlInterpreter *my_perl;

/* perl < 5.8.0 doesn't define PERL_MAGIC_ext */
#ifndef PERL_MAGIC_ext
#define PERL_MAGIC_ext '~'
#endif

/* For now a plugin can only have one action */
void
gaim_perl_plugin_action_cb(GaimPluginAction * gpa)
{
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	/* We put the plugin handle on the stack so it can pass it along    */
	/* to anything called from the callback.  It is supposed to pass    */
	/* the Action, but there is no way to access the plugin handle from */
	/* the GaimPluginAction in perl...yet.                              */

	XPUSHs(gaim_perl_bless_object(gpa->plugin, "Gaim::Plugin"));
	PUTBACK;

	/* gaim_perl_plugin_action_callback_sub defined in the header is set
	 * in perl.c during plugin probe by a PLUGIN_INFO hash value limiting
	 * us to only one action for right now even though the action member
	 * of GaimPluginInfo can take (does take) a GList. */
	call_pv(gaim_perl_plugin_action_callback_sub, G_EVAL | G_SCALAR);
	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;
}

GList *
gaim_perl_plugin_action(GaimPlugin *plugin, gpointer context)
{
	GaimPluginAction *act = NULL;
	GList *gl = NULL;

	/* TODO: Fix the way we create action handlers so we can have more
	 * than one action in perl.  Maybe there is a clever work around, but
	 * so far I have not figured it out.  There is no way to tie the perl
	 * sub's name to the callback function without these global variables
	 * and there is no way to create a callback on the fly so each would
	 * have to be hardcoded--more than one would just be arbitrary. */
	act = gaim_plugin_action_new(gaim_perl_plugin_action_label,
	                             gaim_perl_plugin_action_cb);
	gl = g_list_append(gl, act);

	return gl;
}


GaimGtkPluginUiInfo *
gaim_perl_gtk_plugin_pref(const char * frame_cb)
{
	GaimGtkPluginUiInfo *ui_info;

	ui_info = g_new0(GaimGtkPluginUiInfo, 1);
	perl_gtk_plugin_pref_cb = g_strdup(frame_cb);
	ui_info->get_config_frame = gaim_perl_gtk_get_plugin_frame;

	return ui_info;
}

GtkWidget *
gaim_perl_gtk_get_plugin_frame(GaimPlugin *plugin)
{
	SV * sv;
	GtkWidget *ret;
	MAGIC *mg;
	dSP;
	int count;

	ENTER;
	SAVETMPS;

	count = call_pv(perl_gtk_plugin_pref_cb, G_SCALAR | G_NOARGS);
	if (count != 1)
		croak("call_pv: Did not return the correct number of values.\n");

	/* the frame was created in a perl sub and is returned */
	SPAGAIN;

	/* We have a Gtk2::Frame on top of the stack */
	sv = POPs;

	/* The magic field hides the pointer to the actuale GtkWidget */
	mg = mg_find(SvRV(sv), PERL_MAGIC_ext);
	ret = (GtkWidget *)mg->mg_ptr;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret;
}

/* Called to create a pointer to GaimPluginUiInfo for the GaimPluginInfo */
/* It will then in turn create ui_info with the C function pointer       */
/* that will eventually do a call_pv to call a perl functions so users   */
/* can create their own frames in the prefs                              */
GaimPluginUiInfo *
gaim_perl_plugin_pref(const char * frame_cb)
{
	GaimPluginUiInfo *ui_info;

	ui_info = g_new0(GaimPluginUiInfo, 1);
	perl_plugin_pref_cb = g_strdup(frame_cb);
	ui_info->get_plugin_pref_frame = gaim_perl_get_plugin_frame;

	return ui_info;
}

GaimPluginPrefFrame *
gaim_perl_get_plugin_frame(GaimPlugin *plugin)
{
	/* Sets up the Perl Stack for our call back into the script to run the
	 * plugin_pref... sub */
	GaimPluginPrefFrame *ret_frame;
	dSP;
	int count;

	ENTER;
	SAVETMPS;
	/* Some perl magic to run perl_plugin_pref_frame_SV perl sub and
	 * return the frame */
	PUSHMARK(SP);
	PUTBACK;

	count = call_pv(perl_plugin_pref_cb, G_SCALAR | G_NOARGS);

	SPAGAIN;

	if (count != 1)
		croak("call_pv: Did not return the correct number of values.\n");
	/* the frame was created in a perl sub and is returned */
	ret_frame = (GaimPluginPrefFrame *)gaim_perl_ref_object(POPs);

	/* Tidy up the Perl stack */
	PUTBACK;
	FREETMPS;
	LEAVE;
	
	return ret_frame;
}

static void
destroy_timeout_handler(GaimPerlTimeoutHandler *handler)
{
	timeout_handlers = g_list_remove(timeout_handlers, handler);

	if (handler->callback != NULL)
		SvREFCNT_dec(handler->callback);

	if (handler->data != NULL)
		SvREFCNT_dec(handler->data);

	g_free(handler);
}

static void
destroy_signal_handler(GaimPerlSignalHandler *handler)
{
	signal_handlers = g_list_remove(signal_handlers, handler);

	if (handler->callback != NULL)
		SvREFCNT_dec(handler->callback);

	if (handler->data != NULL)
		SvREFCNT_dec(handler->data);

	g_free(handler->signal);
	g_free(handler);
}

static int
perl_timeout_cb(gpointer data)
{
	GaimPerlTimeoutHandler *handler = (GaimPerlTimeoutHandler *)data;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);
	XPUSHs((SV *)handler->data);
	PUTBACK;
	call_sv(handler->callback, G_EVAL | G_SCALAR);
	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;

	destroy_timeout_handler(handler);

	return 0;
}

typedef void *DATATYPE;

static void *
perl_signal_cb(va_list args, void *data)
{
	GaimPerlSignalHandler *handler = (GaimPerlSignalHandler *)data;
	void *ret_val = NULL;
	int i;
	int count;
	int value_count;
	GaimValue *ret_value, **values;
	SV **sv_args;
	DATATYPE **copy_args;
	STRLEN na;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	gaim_signal_get_values(handler->instance, handler->signal,
	                       &ret_value, &value_count, &values);

	sv_args   = g_new(SV *,    value_count);
	copy_args = g_new(void **, value_count);

	for (i = 0; i < value_count; i++) {
		sv_args[i] = gaim_perl_sv_from_vargs(values[i],
		                                     (va_list*)&args,
		                                     &copy_args[i]);

		XPUSHs(sv_args[i]);
	}

	XPUSHs((SV *)handler->data);

	PUTBACK;

	if (ret_value != NULL) {
		count = call_sv(handler->callback, G_EVAL | G_SCALAR);

		SPAGAIN;

		if (count != 1)
			croak("Uh oh! call_sv returned %i != 1", i);
		else
			ret_val = gaim_perl_data_from_sv(ret_value, POPs);
	} else {
		call_sv(handler->callback, G_SCALAR);

		SPAGAIN;
	}

	if (SvTRUE(ERRSV)) {
		gaim_debug_error("perl",
		                 "Perl function exited abnormally: %s\n",
		                 SvPV(ERRSV, na));
	}

	/* See if any parameters changed. */
	for (i = 0; i < value_count; i++) {
		if (gaim_value_is_outgoing(values[i])) {
			switch (gaim_value_get_type(values[i])) {
				case GAIM_TYPE_BOOLEAN:
					*((gboolean *)copy_args[i]) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_INT:
					*((int *)copy_args[i]) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_UINT:
					*((unsigned int *)copy_args[i]) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_LONG:
					*((long *)copy_args[i]) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_ULONG:
					*((unsigned long *)copy_args[i]) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_INT64:
					*((gint64 *)copy_args[i]) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_UINT64:
					*((guint64 *)copy_args[i]) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_STRING:
					if (strcmp(*((char **)copy_args[i]), SvPVX(sv_args[i]))) {
						g_free(*((char **)copy_args[i]));
						*((char **)copy_args[i]) =
							g_strdup(SvPV(sv_args[i], na));
					}
					break;

				case GAIM_TYPE_POINTER:
					*((void **)copy_args[i]) = (void *)SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_BOXED:
					*((void **)copy_args[i]) = (void *)SvIV(sv_args[i]);
					break;

				default:
					break;
			}

#if 0
			*((void **)copy_args[i]) = gaim_perl_data_from_sv(values[i],
															  sv_args[i]);
#endif
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	g_free(sv_args);
	g_free(copy_args);

	gaim_debug_misc("perl", "ret_val = %p\n", ret_val);

	return ret_val;
}

static GaimPerlSignalHandler *
find_signal_handler(GaimPlugin *plugin, void *instance, const char *signal)
{
	GaimPerlSignalHandler *handler;
	GList *l;

	for (l = signal_handlers; l != NULL; l = l->next) {
		handler = (GaimPerlSignalHandler *)l->data;

		if (handler->plugin == plugin &&
			handler->instance == instance &&
			!strcmp(handler->signal, signal)) {
			return handler;
		}
	}

	return NULL;
}

void
gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, SV *callback, SV *data)
{
	GaimPerlTimeoutHandler *handler;

	if (plugin == NULL) {
		croak("Invalid handle in adding perl timeout handler.\n");
		return;
	}

	handler = g_new0(GaimPerlTimeoutHandler, 1);

	handler->plugin   = plugin;
	handler->callback = (callback != NULL && callback != &PL_sv_undef
						 ? newSVsv(callback) : NULL);
	handler->data     = (data != NULL && data != &PL_sv_undef
						 ? newSVsv(data) : NULL);

	timeout_handlers = g_list_append(timeout_handlers, handler);

	handler->iotag = g_timeout_add(seconds * 1000, perl_timeout_cb, handler);
}

void
gaim_perl_timeout_clear_for_plugin(GaimPlugin *plugin)
{
	GaimPerlTimeoutHandler *handler;
	GList *l, *l_next;

	for (l = timeout_handlers; l != NULL; l = l_next) {
		l_next = l->next;

		handler = (GaimPerlTimeoutHandler *)l->data;

		if (handler->plugin == plugin)
			destroy_timeout_handler(handler);
	}
}

void
gaim_perl_timeout_clear(void)
{
	while (timeout_handlers != NULL)
		destroy_timeout_handler(timeout_handlers->data);
}

void
gaim_perl_signal_connect(GaimPlugin *plugin, void *instance,
                         const char *signal, SV *callback, SV *data)
{
	GaimPerlSignalHandler *handler;

	handler = g_new0(GaimPerlSignalHandler, 1);
	handler->plugin   = plugin;
	handler->instance = instance;
	handler->signal   = g_strdup(signal);
	handler->callback = (callback != NULL &&
	                     callback != &PL_sv_undef ? newSVsv(callback)
	                                              : NULL);
	handler->data     = (data != NULL &&
	                     data != &PL_sv_undef ? newSVsv(data) : NULL);

	signal_handlers = g_list_append(signal_handlers, handler);

	gaim_signal_connect_vargs(instance, signal, plugin,
	                          GAIM_CALLBACK(perl_signal_cb), handler);
}

void
gaim_perl_signal_disconnect(GaimPlugin *plugin, void *instance,
                            const char *signal)
{
	GaimPerlSignalHandler *handler;

	handler = find_signal_handler(plugin, instance, signal);

	if (handler == NULL) {
		croak("Invalid signal handler information in "
		      "disconnecting a perl signal handler.\n");
		return;
	}

	destroy_signal_handler(handler);
}

void
gaim_perl_signal_clear_for_plugin(GaimPlugin *plugin)
{
	GaimPerlSignalHandler *handler;
	GList *l, *l_next;

	for (l = signal_handlers; l != NULL; l = l_next) {
		l_next = l->next;

		handler = (GaimPerlSignalHandler *)l->data;

		if (handler->plugin == plugin)
			destroy_signal_handler(handler);
	}
}

void
gaim_perl_signal_clear(void)
{
	while (signal_handlers != NULL)
		destroy_signal_handler(signal_handlers->data);
}
