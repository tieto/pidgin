#ifndef _GAIM_PERL_HANDLERS_H_
#define _GAIM_PERL_HANDLERS_H_

#include "plugin.h"

typedef struct
{
	char *name;
	void *args;
	GaimPlugin *plugin;
	int iotag;

} GaimPerlTimeoutHandler;

typedef struct
{
	char *signal;
	SV *callback;
	SV *data;
	void *instance;
	GaimPlugin *plugin;

} GaimPerlSignalHandler;


void gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, const char *func,
						   void *args);
void gaim_perl_timeout_clear_for_plugin(GaimPlugin *plugin);
void gaim_perl_timeout_clear(void);

void gaim_perl_signal_connect(GaimPlugin *plugin, void *instance,
							  const char *signal, SV *callback,
							  SV *data);
void gaim_perl_signal_disconnect(GaimPlugin *plugin, void *instance,
								 const char *signal);
void gaim_perl_signal_clear_for_plugin(GaimPlugin *plugin);
void gaim_perl_signal_clear(void);

#endif /* _GAIM_PERL_HANDLERS_H_ */
