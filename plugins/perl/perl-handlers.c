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
	STRLEN na;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	gaim_signal_get_values(handler->instance, handler->signal,
						   &ret_value, &value_count, &values);

	sv_args = g_new(SV *, value_count);

	for (i = 0; i < value_count; i++)
	{
		SV *sv = gaim_perl_sv_from_vargs(values[i], args);

		sv_args[i] = sv;

		XPUSHs(sv);
	}

	XPUSHs((SV *)handler->data);

	PUTBACK;

	if (ret_value != NULL)
	{
		count = call_sv(handler->callback, G_SCALAR);

		SPAGAIN;

		if (count != 1)
			croak("Uh oh! call_sv returned %i != 1", i);
		else
		{
			SV *temp_ret_val = POPs;

			switch (gaim_value_get_type(ret_value))
			{
				case GAIM_TYPE_BOOLEAN:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				case GAIM_TYPE_INT:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				case GAIM_TYPE_UINT:
					ret_val = (void *)SvUV(temp_ret_val);
					break;

				case GAIM_TYPE_LONG:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				case GAIM_TYPE_ULONG:
					ret_val = (void *)SvUV(temp_ret_val);
					break;

				case GAIM_TYPE_INT64:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				case GAIM_TYPE_UINT64:
					ret_val = (void *)SvUV(temp_ret_val);
					break;

				case GAIM_TYPE_STRING:
					ret_val = (void *)SvPV(temp_ret_val, na);
					break;

				case GAIM_TYPE_POINTER:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				case GAIM_TYPE_BOXED:
					ret_val = (void *)SvIV(temp_ret_val);
					break;

				default:
					ret_val = NULL;
			}
		}
	}
	else
		call_sv(handler->callback, G_SCALAR);

	/* See if any parameters changed. */
	for (i = 0; i < value_count; i++)
	{
		if (gaim_value_is_outgoing(values[i]))
		{
			switch (gaim_value_get_type(values[i]))
			{
				case GAIM_TYPE_BOOLEAN:
					*va_arg(args, gboolean *) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_INT:
					*va_arg(args, int *) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_UINT:
					*va_arg(args, unsigned int *) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_LONG:
					*va_arg(args, long *) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_ULONG:
					*va_arg(args, unsigned long *) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_INT64:
					*va_arg(args, gint64 *) = SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_UINT64:
					*va_arg(args, guint64 *) = SvUV(sv_args[i]);
					break;

				case GAIM_TYPE_STRING:
					/* XXX Memory leak! */
					*va_arg(args, char **) = SvPV(sv_args[i], na);
					break;

				case GAIM_TYPE_POINTER:
					/* XXX Possible memory leak! */
					*va_arg(args, void **) = (void *)SvIV(sv_args[i]);
					break;

				case GAIM_TYPE_BOXED:
					/* Uh.. I dunno. Try this? Likely won't work. Heh. */
					/* XXX Possible memory leak! */
					*va_arg(args, void **) = (void *)SvIV(sv_args[i]);
					break;

				default:
					return FALSE;
			}
		}
	}

	FREETMPS;
	LEAVE;

	g_free(sv_args);

	gaim_debug_misc("perl", "ret_val = %p\n", ret_val);

	return ret_val;
}

static GaimPerlSignalHandler *
find_signal_handler(GaimPlugin *plugin, void *instance, const char *signal)
{
	GaimPerlSignalHandler *handler;
	GList *l;

	for (l = signal_handlers; l != NULL; l = l->next)
	{
		handler = (GaimPerlSignalHandler *)l->data;

		if (handler->plugin == plugin &&
			handler->instance == instance &&
			!strcmp(handler->signal, signal))
		{
			return handler;
		}
	}

	return NULL;
}

void
gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, SV *callback, SV *data)
{
	GaimPerlTimeoutHandler *handler;

	if (plugin == NULL)
	{
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
						 const char *signal, SV *callback, SV *data)
{
	GaimPerlSignalHandler *handler;

	handler = g_new0(GaimPerlSignalHandler, 1);
	handler->plugin   = plugin;
	handler->instance = instance;
	handler->signal   = g_strdup(signal);
	handler->callback = (callback != NULL && callback != &PL_sv_undef
						 ? newSVsv(callback) : NULL);
	handler->data     = (data != NULL && data != &PL_sv_undef
						 ? newSVsv(data) : NULL);

	signal_handlers = g_list_append(signal_handlers, handler);

	gaim_signal_connect_vargs(instance, signal,
							  plugin, GAIM_CALLBACK(perl_signal_cb), handler);
}

void
gaim_perl_signal_disconnect(GaimPlugin *plugin, void *instance,
							const char *signal)
{
	GaimPerlSignalHandler *handler;

	handler = find_signal_handler(plugin, instance, signal);

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

