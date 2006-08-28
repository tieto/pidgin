#include "perl-common.h"
#include "perl-handlers.h"

#include "debug.h"
#include "signals.h"

extern PerlInterpreter *my_perl;
static GList *cmd_handlers = NULL;
static GList *signal_handlers = NULL;
static GList *timeout_handlers = NULL;

/* perl < 5.8.0 doesn't define PERL_MAGIC_ext */
#ifndef PERL_MAGIC_ext
#define PERL_MAGIC_ext '~'
#endif

void
gaim_perl_plugin_action_cb(GaimPluginAction *action)
{
	SV **callback;
	HV *hv = NULL;
	gchar *hvname;
	GaimPlugin *plugin;
	GaimPerlScript *gps;
	dSP;

	plugin = action->plugin;
	gps = (GaimPerlScript *)plugin->info->extra_info;
	hvname = g_strdup_printf("%s::plugin_actions", gps->package);
	hv = get_hv(hvname, FALSE);
	g_free(hvname);

	if (hv == NULL)
		croak("No plugin_actions hash found in \"%s\" plugin.", gaim_plugin_get_name(plugin));

	ENTER;
	SAVETMPS;

	callback = hv_fetch(hv, action->label, strlen(action->label), 0);

	if (callback == NULL || *callback == NULL)
		croak("No plugin_action function named \"%s\" in \"%s\" plugin.", action->label, gaim_plugin_get_name(plugin));

	PUSHMARK(sp);
	XPUSHs(gaim_perl_bless_object(gps->plugin, "Gaim::Plugin"));
	PUTBACK;

	call_sv(*callback, G_VOID | G_DISCARD);
	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;
}

GList *
gaim_perl_plugin_actions(GaimPlugin *plugin, gpointer context)
{
	GList *l = NULL;
	GaimPerlScript *gps;
	int i = 0, count = 0;
	dSP;

	gps = (GaimPerlScript *)plugin->info->extra_info;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(gaim_perl_bless_object(plugin, "Gaim::Plugin")));
	/* XXX This *will* cease working correctly if context gets changed to
	 * ever be able to hold anything other than a GaimConnection */
	if (context != NULL)
		XPUSHs(sv_2mortal(gaim_perl_bless_object(context, "Gaim::Connection")));
	else
		XPUSHs(&PL_sv_undef);
	PUTBACK;

	count = call_pv(gps->plugin_action_sub, G_ARRAY);

	SPAGAIN;

	if (count == 0)
		croak("The plugin_actions sub didn't return anything.\n");

	for (i = 0; i < count; i++) {
		SV *sv;
		gchar *label;
		GaimPluginAction *act = NULL;

		sv = POPs;
		label = SvPV_nolen(sv);
		/* XXX I think this leaks, but doing it without the strdup
		 * just showed garbage */
		act = gaim_plugin_action_new(g_strdup(label), gaim_perl_plugin_action_cb);
		l = g_list_prepend(l, act);
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return l;
}

#ifdef GAIM_GTKPERL
GtkWidget *
gaim_perl_gtk_get_plugin_frame(GaimPlugin *plugin)
{
	SV * sv;
	int count;
	MAGIC *mg;
	GtkWidget *ret;
	GaimPerlScript *gps;
	dSP;

	gps = (GaimPerlScript *)plugin->info->extra_info;

	ENTER;
	SAVETMPS;

	count = call_pv(gps->gtk_prefs_sub, G_SCALAR | G_NOARGS);
	if (count != 1)
		croak("call_pv: Did not return the correct number of values.\n");

	/* the frame was created in a perl sub and is returned */
	SPAGAIN;

	/* We have a Gtk2::Frame on top of the stack */
	sv = POPs;

	/* The magic field hides the pointer to the actual GtkWidget */
	mg = mg_find(SvRV(sv), PERL_MAGIC_ext);
	ret = (GtkWidget *)mg->mg_ptr;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret;
}
#endif

GaimPluginPrefFrame *
gaim_perl_get_plugin_frame(GaimPlugin *plugin)
{
	/* Sets up the Perl Stack for our call back into the script to run the
	 * plugin_pref... sub */
	int count;
	GaimPerlScript *gps;
	GaimPluginPrefFrame *ret_frame;
	dSP;

	gps = (GaimPerlScript *)plugin->info->extra_info;

	ENTER;
	SAVETMPS;
	/* Some perl magic to run perl_plugin_pref_frame_SV perl sub and
	 * return the frame */
	PUSHMARK(SP);
	PUTBACK;

	count = call_pv(gps->prefs_sub, G_SCALAR | G_NOARGS);

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
                         const char *signal, SV *callback, SV *data,
                         int priority)
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

	gaim_signal_connect_priority_vargs(instance, signal, plugin,
	                                   GAIM_CALLBACK(perl_signal_cb),
	                                   handler, priority);
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

static GaimCmdRet
perl_cmd_cb(GaimConversation *conv, const gchar *command,
            gchar **args, gchar **error, void *data)
{
	int i = 0, count, ret_value = GAIM_CMD_RET_OK;
	SV *cmdSV, *tmpSV, *convSV;
	GaimPerlCmdHandler *handler = (GaimPerlCmdHandler *)data;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	/* Push the conversation onto the perl stack */
	convSV = sv_2mortal(gaim_perl_bless_object(conv, "Gaim::Conversation"));
	XPUSHs(convSV);

	/* Push the command string onto the perl stack */
	cmdSV = newSVpv(command, 0);
	cmdSV = sv_2mortal(cmdSV);
	XPUSHs(cmdSV);

	/* Push the data onto the perl stack */
	XPUSHs((SV *)handler->data);

	/* Push any arguments we may have */
	for (i = 0; args[i] != NULL; i++) {
		/* XXX The mortality of these created SV's should prevent
		 * memory issues, if I read/understood everything correctly...
		 */
		tmpSV = newSVpv(args[i], 0);
		tmpSV = sv_2mortal(tmpSV);
		XPUSHs(tmpSV);
	}

	PUTBACK;
	count = call_sv(handler->callback, G_EVAL|G_SCALAR);

	if (count != 1)
		croak("call_sv: Did not return the correct number of values.\n");

	SPAGAIN;

	ret_value = POPi;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret_value;
}

GaimCmdId
gaim_perl_cmd_register(GaimPlugin *plugin, const gchar *command,
                       const gchar *args, GaimCmdPriority priority,
                       GaimCmdFlag flag, const gchar *prpl_id, SV *callback,
                       const gchar *helpstr, SV *data)
{
	GaimPerlCmdHandler *handler;

	handler          = g_new0(GaimPerlCmdHandler, 1);
	handler->plugin  = plugin;
	handler->cmd     = g_strdup(command);
	handler->prpl_id = g_strdup(prpl_id);

	if (callback != NULL && callback != &PL_sv_undef)
		handler->callback = newSVsv(callback);
	else
		handler->callback = NULL;

	if (data != NULL && data != &PL_sv_undef)
		handler->data = newSVsv(data);
	else
		handler->data = NULL;

	cmd_handlers = g_list_append(cmd_handlers, handler);

	handler->id = gaim_cmd_register(command, args, priority, flag, prpl_id,
	                                GAIM_CMD_FUNC(perl_cmd_cb), helpstr,
	                                handler);

	return handler->id;
}

static void
destroy_cmd_handler(GaimPerlCmdHandler *handler)
{
	cmd_handlers = g_list_remove(cmd_handlers, handler);

	if (handler->callback != NULL)
		SvREFCNT_dec(handler->callback);

	if (handler->data != NULL)
		SvREFCNT_dec(handler->data);

	g_free(handler->cmd);
	g_free(handler->prpl_id);
	g_free(handler);
}

void
gaim_perl_cmd_clear_for_plugin(GaimPlugin *plugin)
{
	GList *l, *l_next;

	for (l = cmd_handlers; l != NULL; l = l_next) {
		GaimPerlCmdHandler *handler = (GaimPerlCmdHandler *)l->data;

		l_next = l->next;

		if (handler->plugin == plugin)
			destroy_cmd_handler(handler);
	}
}

static GaimPerlCmdHandler *
find_cmd_handler(GaimCmdId id)
{
	GList *l;

	for (l = cmd_handlers; l != NULL; l = l->next) {
		GaimPerlCmdHandler *handler = (GaimPerlCmdHandler *)l->data;

		if (handler->id == id)
			return handler;
	}

	return NULL;
}

void
gaim_perl_cmd_unregister(GaimCmdId id)
{
	GaimPerlCmdHandler *handler;

	handler = find_cmd_handler(id);

	if (handler == NULL) {
		croak("Invalid command id in removing a perl command handler.\n");
		return;
	}

	gaim_cmd_unregister(id);
	destroy_cmd_handler(handler);
}
