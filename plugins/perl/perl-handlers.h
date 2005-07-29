#ifndef _GAIM_PERL_HANDLERS_H_
#define _GAIM_PERL_HANDLERS_H_

#include "plugin.h"
#include "prefs.h"
#include "pluginpref.h"
#include "gtkplugin.h"
#include "gtkutils.h"

/* TODO: Find a better way to access the perl names from the plugin prober	*/
/* and store them for gaim_perl_plugin_action_* functions.			*/
char * gaim_perl_plugin_action_callback_sub;
char * gaim_perl_plugin_action_label;

typedef struct
{
	SV *callback;
	SV *data;
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

void gaim_perl_plugin_action_cb(GaimPluginAction * gpa);
GList *gaim_perl_plugin_action(GaimPlugin *plugin, gpointer context); 

GaimPluginUiInfo *gaim_perl_plugin_pref(const char * frame_cb);
GaimPluginPrefFrame *gaim_perl_get_plugin_frame(GaimPlugin *plugin);

GaimGtkPluginUiInfo *gaim_perl_gtk_plugin_pref(const char * frame_cb);
GtkWidget *gaim_perl_gtk_get_plugin_frame(GaimPlugin *plugin);

void gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, SV *callback,
						   SV *data);
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
