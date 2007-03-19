#include "module.h"

MODULE = Purple::Prefs  PACKAGE = Purple::Prefs  PREFIX = purple_prefs_
PROTOTYPES: ENABLE

void
purple_prefs_add_bool(name, value)
	const char *name
	gboolean value

void
purple_prefs_add_int(name, value)
	const char *name
	int value

void
purple_prefs_add_none(name)
	const char *name

void
purple_prefs_add_string(name, value)
	const char *name
	const char *value

void
purple_prefs_add_string_list(name, value)
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
	purple_prefs_add_string_list(name, t_GL);

void
purple_prefs_destroy()

void
purple_prefs_disconnect_by_handle(handle)
	void * handle

void
purple_prefs_disconnect_callback(callback_id)
	guint callback_id

gboolean
purple_prefs_exists(name)
	const char *name

gboolean
purple_prefs_get_bool(name)
	const char *name

Purple::Handle
purple_prefs_get_handle()

int
purple_prefs_get_int(name)
	const char *name

const char *
purple_prefs_get_string(name)
	const char *name

void
purple_prefs_get_string_list(name)
	const char *name
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_prefs_get_string_list(name); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::PrefValue")));
	}

Purple::PrefType
purple_prefs_get_type(name)
	const char *name

void
purple_prefs_init()

gboolean
purple_prefs_load()

void
purple_prefs_remove(name)
	const char *name

void
purple_prefs_rename(oldname, newname)
	const char *oldname
	const char *newname

void
purple_prefs_rename_boolean_toggle(oldname, newname)
	const char *oldname
	const char *newname

void
purple_prefs_set_bool(name, value)
	const char *name
	gboolean value

void
purple_prefs_set_generic(name, value)
	const char *name
	gpointer value

void
purple_prefs_set_int(name, value)
	const char *name
	int value

void
purple_prefs_set_string(name, value)
	const char *name
	const char *value

void
purple_prefs_set_string_list(name, value)
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
	purple_prefs_set_string_list(name, t_GL);

void
purple_prefs_trigger_callback(name)
	const char *name

void
purple_prefs_uninit()

void
purple_prefs_update_old()
