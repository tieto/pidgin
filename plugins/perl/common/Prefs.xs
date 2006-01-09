#include "module.h"

MODULE = Gaim::Prefs  PACKAGE = Gaim::Prefs  PREFIX = gaim_prefs_
PROTOTYPES: ENABLE

void
gaim_prefs_add_bool(name, value)
	const char *name
	gboolean value

void
gaim_prefs_add_int(name, value)
	const char *name
	int value

void
gaim_prefs_add_none(name)
	const char *name

void
gaim_prefs_add_string(name, value)
	const char *name
	const char *value

void
gaim_prefs_add_string_list(name, value)
	const char *name
	SV *value
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(value));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(value), i, 0), t_sl));
	}
	gaim_prefs_add_string_list(name, t_GL);

void
gaim_prefs_destroy()

void
gaim_prefs_disconnect_by_handle(handle)
	void * handle

void
gaim_prefs_disconnect_callback(callback_id)
	guint callback_id

gboolean
gaim_prefs_exists(name)
	const char *name

gboolean
gaim_prefs_get_bool(name)
	const char *name

void *
gaim_prefs_get_handle()

int
gaim_prefs_get_int(name)
	const char *name

const char *
gaim_prefs_get_string(name)
	const char *name

void
gaim_prefs_get_string_list(name)
	const char *name
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_prefs_get_string_list(name); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::PrefValue")));
	}

Gaim::PrefType
gaim_prefs_get_type(name)
	const char *name

void
gaim_prefs_init()

gboolean
gaim_prefs_load()

void
gaim_prefs_remove(name)
	const char *name

void
gaim_prefs_rename(oldname, newname)
	const char *oldname
	const char *newname

void
gaim_prefs_rename_boolean_toggle(oldname, newname)
	const char *oldname
	const char *newname

void
gaim_prefs_set_bool(name, value)
	const char *name
	gboolean value

void
gaim_prefs_set_generic(name, value)
	const char *name
	gpointer value

void
gaim_prefs_set_int(name, value)
	const char *name
	int value

void
gaim_prefs_set_string(name, value)
	const char *name
	const char *value

void
gaim_prefs_set_string_list(name, value)
	const char *name
	SV *value
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(value));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(value), i, 0), t_sl));
	}
	gaim_prefs_set_string_list(name, t_GL);

void
gaim_prefs_trigger_callback(name)
	const char *name

void
gaim_prefs_uninit()

void
gaim_prefs_update_old()
