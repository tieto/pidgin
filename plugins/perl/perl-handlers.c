#include "perl-common.h"
#include "perl-handlers.h"

#include "debug.h"
#include "signals.h"

static GList *timeout_handlers = NULL;
static GList *signal_handlers = NULL;

extern PerlInterpreter *my_perl;

static void
destroy_timeout_handler(GaimPerlTimeoutHandler *handler)
{
	timeout_handlers = g_list_remove(timeout_handlers, handler);

	g_free(handler->name);
	g_free(handler);
}

static void
destroy_signal_handler(GaimPerlSignalHandler *handler)
{
	signal_handlers = g_list_remove(signal_handlers, handler);

	g_free(handler->signal);
	g_free(handler->func);
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
	XPUSHs((SV *)handler->args);
	PUTBACK;
	call_pv(handler->name, G_EVAL | G_SCALAR);
	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;

	destroy_timeout_handler(handler);

	return 0;
}

static void *
perl_signal_cb(va_list args, void *data)
{
	GaimPerlSignalHandler *handler = (GaimPerlSignalHandler *)data;
	void *arg;
	void *ret_val = NULL;
	int count;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	while ((arg = va_arg(args, void *)) != NULL)
		XPUSHs((SV *)arg);

	XPUSHs((SV *)handler->data);

	PUTBACK;
	count = call_pv(handler->func, G_EVAL | G_SCALAR);
	SPAGAIN;

	if (count > 1)
		ret_val = POPp;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret_val;
}

static GaimPerlSignalHandler *
find_signal_handler(GaimPlugin *plugin, void *instance,
					const char *signal, const char *func)
{
	GaimPerlSignalHandler *handler;
	GList *l;

	for (l = signal_handlers; l != NULL; l = l->next)
	{
		handler = (GaimPerlSignalHandler *)l->data;

		if (handler->plugin == plugin &&
			handler->instance == instance &&
			!strcmp(handler->signal, signal) &&
			!strcmp(handler->func, func))
		{
			return handler;
		}
	}

	return NULL;
}

void
gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, const char *func,
					  void *args)
{
	GaimPerlTimeoutHandler *handler;

	if (plugin == NULL)
	{
		croak("Invalid handle in adding perl timeout handler.\n");
		return;
	}

	handler = g_new0(GaimPerlTimeoutHandler, 1);

	handler->plugin = plugin;
	handler->name = g_strdup(func);
	handler->args = args;

	timeout_handlers = g_list_append(timeout_handlers, handler);
	handler->iotag = g_timeout_add(seconds * 1000, perl_timeout_cb, handler);
}

void
gaim_perl_timeout_clear_for_plugin(GaimPlugin *plugin)
{
	GaimPerlTimeoutHandler *handler;
	GList *l, *l_next;

	for (l = timeout_handlers; l != NULL; l = l_next)
	{
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
						 const char *signal, const char *func, void *data)
{
	GaimPerlSignalHandler *handler;

	handler = g_new0(GaimPerlSignalHandler, 1);
	handler->plugin   = plugin;
	handler->instance = instance;
	handler->signal   = g_strdup(signal);
	handler->func     = g_strdup(func);
	handler->data     = data;

	signal_handlers = g_list_append(signal_handlers, handler);

	gaim_debug(GAIM_DEBUG_MISC, "perl", "plugin = %p\n", plugin);
	gaim_signal_connect_vargs(instance, signal,
							  plugin, GAIM_CALLBACK(perl_signal_cb), handler);
}

void
gaim_perl_signal_disconnect(GaimPlugin *plugin, void *instance,
							const char *signal, const char *func)
{
	GaimPerlSignalHandler *handler;

	handler = find_signal_handler(plugin, instance, signal, func);

	if (handler == NULL)
	{
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

	for (l = signal_handlers; l != NULL; l = l_next)
	{
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

