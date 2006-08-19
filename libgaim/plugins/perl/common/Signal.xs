#include "module.h"
#include "../perl-handlers.h"

MODULE = Gaim::Signal  PACKAGE = Gaim::Signal  PREFIX = gaim_signal_
PROTOTYPES: ENABLE

void
gaim_signal_connect_priority(instance, signal, plugin, callback, priority, data = 0)
	void *instance
	const char *signal
	Gaim::Plugin plugin
	SV *callback
	int priority
	SV *data
CODE:
	gaim_perl_signal_connect(plugin, instance, signal, callback, data, priority);

void
gaim_signal_connect(instance, signal, plugin, callback, data = 0)
	void *instance
	const char *signal
	Gaim::Plugin plugin
	SV *callback
	SV *data
CODE:
	gaim_perl_signal_connect(plugin, instance, signal, callback, data, GAIM_SIGNAL_PRIORITY_DEFAULT);

void
gaim_signal_disconnect(instance, signal, plugin)
	void *instance
	const char *signal
	Gaim::Plugin plugin
CODE:
	gaim_perl_signal_disconnect(plugin, instance, signal);
