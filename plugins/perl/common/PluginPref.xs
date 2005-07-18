
#include "module.h"

/* TODO


*/


MODULE = Gaim::Pref  PACKAGE = Gaim::Pref  PREFIX = gaim_plugin_pref_
PROTOTYPES: ENABLE

void 
gaim_plugin_pref_add_choice(pref, label, choice)
	Gaim::PluginPref pref
	char *label
	gpointer choice

void 
gaim_plugin_pref_destroy(pref)
	Gaim::PluginPref pref

void 
gaim_plugin_pref_frame_add(frame, pref)
	Gaim::PluginPrefFrame frame
	Gaim::PluginPref pref

void 
gaim_plugin_pref_frame_destroy(frame)
	Gaim::PluginPrefFrame frame

void
gaim_plugin_pref_frame_get_prefs(frame)
	Gaim::PluginPrefFrame frame
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugin_pref_frame_get_prefs(frame); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
	}
	
Gaim::PluginPrefFrame
gaim_plugin_pref_frame_new()


void 
gaim_plugin_pref_get_bounds(pref, min, max)
	Gaim::PluginPref pref
	int *min
	int *max

void
gaim_plugin_pref_get_choices(pref)
	Gaim::PluginPref pref
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugin_pref_get_choices(pref); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
	}
	
char *
gaim_plugin_pref_get_label(pref)
	Gaim::PluginPref pref

gboolean 
gaim_plugin_pref_get_masked(pref)
	Gaim::PluginPref pref

unsigned int 
gaim_plugin_pref_get_max_length(pref)
	Gaim::PluginPref pref

char *
gaim_plugin_pref_get_name(pref)
	Gaim::PluginPref pref

Gaim::PluginPrefType
gaim_plugin_pref_get_type(pref)
	Gaim::PluginPref pref

Gaim::PluginPref
gaim_plugin_pref_new()


Gaim::PluginPref
gaim_plugin_pref_new_with_label(label)
	char *label

Gaim::PluginPref
gaim_plugin_pref_new_with_name(name)
	char *name

Gaim::PluginPref
gaim_plugin_pref_new_with_name_and_label(name, label)
	char *name
	char *label

void 
gaim_plugin_pref_set_bounds(pref, min, max)
	Gaim::PluginPref pref
	int min
	int max

void 
gaim_plugin_pref_set_label(pref, label)
	Gaim::PluginPref pref
	char *label

void 
gaim_plugin_pref_set_masked(pref, mask)
	Gaim::PluginPref pref
	gboolean mask

void 
gaim_plugin_pref_set_max_length(pref, max_length)
	Gaim::PluginPref pref
	unsigned int max_length

void 
gaim_plugin_pref_set_name(pref, name)
	Gaim::PluginPref pref
	char *name

void 
gaim_plugin_pref_set_type(pref, type)
	Gaim::PluginPref pref
	Gaim::PluginPrefType type
CODE:
	printf("gaim_plugin_pref_set_type(): %d\n", (int)GAIM_PLUGIN_PREF_CHOICE);
	gaim_plugin_pref_set_type(pref, GAIM_PLUGIN_PREF_CHOICE);

