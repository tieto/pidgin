#include "module.h"

MODULE = Gaim::PluginPref  PACKAGE = Gaim::PluginPref::Frame  PREFIX = gaim_plugin_pref_frame_
PROTOTYPES: ENABLE

void
gaim_plugin_pref_frame_add(frame, pref)
	Gaim::PluginPref::Frame frame
	Gaim::PluginPref pref

void
gaim_plugin_pref_frame_destroy(frame)
	Gaim::PluginPref::Frame frame

void
gaim_plugin_pref_frame_get_prefs(frame)
	Gaim::PluginPref::Frame frame
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_plugin_pref_frame_get_prefs(frame); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::PluginPref")));
	}

Gaim::PluginPref::Frame
gaim_plugin_pref_frame_new(class)
    C_ARGS: /* void */

MODULE = Gaim::PluginPref  PACKAGE = Gaim::PluginPref  PREFIX = gaim_plugin_pref_
PROTOTYPES: ENABLE

void
gaim_plugin_pref_add_choice(pref, label, choice)
	Gaim::PluginPref pref
	const char *label
	gpointer choice

void
gaim_plugin_pref_destroy(pref)
	Gaim::PluginPref pref


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

const char *
gaim_plugin_pref_get_label(pref)
	Gaim::PluginPref pref

gboolean
gaim_plugin_pref_get_masked(pref)
	Gaim::PluginPref pref

unsigned int
gaim_plugin_pref_get_max_length(pref)
	Gaim::PluginPref pref

const char *
gaim_plugin_pref_get_name(pref)
	Gaim::PluginPref pref

Gaim::PluginPrefType
gaim_plugin_pref_get_type(pref)
	Gaim::PluginPref pref

Gaim::PluginPref
gaim_plugin_pref_new(class)
    C_ARGS: /* void */

Gaim::PluginPref
gaim_plugin_pref_new_with_label(class, label)
	const char *label
    C_ARGS:
	label

Gaim::PluginPref
gaim_plugin_pref_new_with_name(class, name)
	const char *name
    C_ARGS:
	name

Gaim::PluginPref
gaim_plugin_pref_new_with_name_and_label(class, name, label)
	const char *name
	const char *label
    C_ARGS:
	name, label

void
gaim_plugin_pref_set_bounds(pref, min, max)
	Gaim::PluginPref pref
	int min
	int max

void
gaim_plugin_pref_set_label(pref, label)
	Gaim::PluginPref pref
	const char *label

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
	const char *name

void
gaim_plugin_pref_set_type(pref, type)
	Gaim::PluginPref pref
	Gaim::PluginPrefType type
PREINIT:
	GaimPluginPrefType gpp_type;
CODE:
	gpp_type = GAIM_PLUGIN_PREF_NONE;

	if (type == 1) {
		gpp_type = GAIM_PLUGIN_PREF_CHOICE;
	} else if (type == 2) {
		gpp_type = GAIM_PLUGIN_PREF_INFO;
	}
	gaim_plugin_pref_set_type(pref, gpp_type);
