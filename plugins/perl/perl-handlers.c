#include "perl-common.h"
#include "perl-handlers.h"

#include "debug.h"

static GList *timeout_handlers = NULL;

extern PerlInterpreter *my_perl;

static void
destroy_timeout_handler(GaimPerlTimeoutHandler *handler)
{
	timeout_handlers = g_list_remove(timeout_handlers, handler);

	g_free(handler->name);
	g_free(handler);
}

static int
perl_timeout_cb(gpointer data)
{
	void *atmp[2] = { NULL, NULL };
	GaimPerlTimeoutHandler *handler = (GaimPerlTimeoutHandler *)data;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);
	XPUSHs((SV *)handler->args);
	PUTBACK;
	call_pv(handler->name, G_EVAL | G_SCALAR);
	SPAGAIN;

	atmp[0] = handler->args;

	PUTBACK;
	FREETMPS;
	LEAVE;

	destroy_timeout_handler(handler);

	return 0;
}

void
gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, const char *func,
					  void *args)
{
	GaimPerlTimeoutHandler *handler;

	if (plugin == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Invalid handle in adding perl timeout handler.\n");
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
	while (timeout_handlers)
		destroy_timeout_handler(timeout_handlers->data);
}

