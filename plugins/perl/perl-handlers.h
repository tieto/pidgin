#ifndef _GAIM_PERL_HANDLERS_H_
#define _GAIM_PERL_HANDLERS_H_

#include "plugin.h"

typedef struct
{
	char *name;
	char *args;
	GaimPlugin *plugin;
	int iotag;

} GaimPerlTimeoutHandler;

void gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, const char *func,
						   void *args);
void gaim_perl_timeout_clear_for_plugin(GaimPlugin *plugin);
void gaim_perl_timeout_clear(void);

#endif /* _GAIM_PERL_HANDLERS_H_ */
