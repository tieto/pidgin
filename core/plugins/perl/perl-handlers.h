#ifndef _GAIM_PERL_HANDLERS_H_
#define _GAIM_PERL_HANDLERS_H_

#include "cmds.h"
#include "plugin.h"
#include "prefs.h"
#include "pluginpref.h"
#include "gtkplugin.h"
#include "gtkutils.h"

typedef struct
{
	GaimCmdId id;
	SV *callback;
	SV *data;
	char *prpl_id;
	char *cmd;
	GaimPlugin *plugin;
} GaimPerlCmdHandler;

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
GList *gaim_perl_plugin_actions(GaimPlugin *plugin, gpointer context); 

GaimPluginPrefFrame *gaim_perl_get_plugin_frame(GaimPlugin *plugin);

GtkWidget *gaim_perl_gtk_get_plugin_frame(GaimPlugin *plugin);

void gaim_perl_timeout_add(GaimPlugin *plugin, int seconds, SV *callback,
                           SV *data);
void gaim_perl_timeout_clear_for_plugin(GaimPlugin *plugin);
void gaim_perl_timeout_clear(void);

void gaim_perl_signal_connect(GaimPlugin *plugin, void *instance,
                              const char *signal, SV *callback,
                              SV *data, int priority);
void gaim_perl_signal_disconnect(GaimPlugin *plugin, void *instance,
                                 const char *signal);
void gaim_perl_signal_clear_for_plugin(GaimPlugin *plugin);
void gaim_perl_signal_clear(void);

GaimCmdId gaim_perl_cmd_register(GaimPlugin *plugin, const gchar *cmd,
                                 const gchar *args, GaimCmdPriority priority,
                                 GaimCmdFlag flag, const gchar *prpl_id,
                                 SV *callback, const gchar *helpstr, SV *data);
void gaim_perl_cmd_unregister(GaimCmdId id);
void gaim_perl_cmd_clear_for_plugin(GaimPlugin *plugin);

#endif /* _GAIM_PERL_HANDLERS_H_ */
