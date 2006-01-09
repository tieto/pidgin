#include "module.h"

MODULE = Gaim::Account::Option  PACKAGE = Gaim::Account::Option  PREFIX = gaim_account_option_
PROTOTYPES: ENABLE

void
gaim_account_option_destroy(option)
	Gaim::Account::Option option

const char *
gaim_account_option_get_default_string(option)
	Gaim::Account::Option option

void
gaim_account_option_add_list_item(option, key, value)
	Gaim::Account::Option option
	const char * key
	const char * value

void
gaim_account_option_set_default_string(option, value);
	Gaim::Account::Option option
	const char * value

void
gaim_account_option_set_default_int(option, value);
	Gaim::Account::Option option
	int value

void
gaim_account_option_set_default_bool(option, value);
	Gaim::Account::Option option
	gboolean value

Gaim::Account::Option
gaim_account_option_list_new(class, text, pref_name, values)
	const char * text
	const char * pref_name
	SV * values
PREINIT:
	GList *t_GL;
	int i, t_len;
CODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(values));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(values), i, 0), t_sl));
	}
	RETVAL  = gaim_account_option_list_new(text, pref_name, t_GL);
OUTPUT:
	RETVAL

Gaim::Account::Option
gaim_account_option_string_new(class, text, pref_name, default_value)
	const char * text
	const char * pref_name
	const char * default_value
    C_ARGS:
	text, pref_name, default_value

Gaim::Account::Option
gaim_account_option_int_new(class, text, pref_name, default_value)
	const char * text
	const char * pref_name
	gboolean default_value
    C_ARGS:
	text, pref_name, default_value

Gaim::Account::Option
gaim_account_option_bool_new(class, text, pref_name, default_value)
	const char * text
	const char * pref_name
	gboolean default_value
    C_ARGS:
	text, pref_name, default_value

Gaim::Account::Option
gaim_account_option_new(class, type, text, pref_name)
	Gaim::PrefType type
	const char * text
	const char * pref_name
    C_ARGS:
	type, text, pref_name

void
gaim_account_option_get_list(option)
	Gaim::Account::Option option
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_account_option_get_list(option); l != NULL; l = l->next) {
		/* XXX These are actually GaimKeyValuePairs but we don't have a
		 * type for that and even if we did I don't think there's
		 * anything perl could do with them, so I'm just going to
		 * leave this as a Gaim::ListEntry for now. */
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

Gaim::PrefType
gaim_account_option_get_type(option)
	Gaim::Account::Option option

gboolean
gaim_account_option_get_masked(option)
	Gaim::Account::Option option

int
gaim_account_option_get_default_int(option)
	Gaim::Account::Option option;

gboolean
gaim_account_option_get_default_bool(option)
	Gaim::Account::Option option;

const char *
gaim_account_option_get_setting(option)
	Gaim::Account::Option option

const char *
gaim_account_option_get_text(option)
	Gaim::Account::Option option

void
gaim_account_option_set_list(option, values)
	Gaim::Account::Option option
	SV * values
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(values));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(values), i, 0), t_sl));
	}
	gaim_account_option_set_list(option, t_GL);

void
gaim_account_option_set_masked(option, masked)
	Gaim::Account::Option option
	gboolean masked

MODULE = Gaim::Account::Option  PACKAGE = Gaim::Account::UserSplit  PREFIX = gaim_account_user_split_
PROTOTYPES: ENABLE

Gaim::Account::UserSplit
gaim_account_user_split_new(class, text, default_value, sep)
	const char * text
	const char * default_value
	char sep
    C_ARGS:
	text, default_value, sep

char
gaim_account_user_split_get_separator(split)
	Gaim::Account::UserSplit split

const char *
gaim_account_user_split_get_text(split)
	Gaim::Account::UserSplit split

void
gaim_account_user_split_destroy(split)
	Gaim::Account::UserSplit split
