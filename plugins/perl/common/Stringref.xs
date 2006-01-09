#include "module.h"

MODULE = Gaim::Stringref  PACKAGE = Gaim::Stringref  PREFIX = gaim_stringref_
PROTOTYPES: ENABLE

int
gaim_stringref_cmp(s1, s2)
	Gaim::Stringref s1
	Gaim::Stringref s2

size_t
gaim_stringref_len(stringref)
	Gaim::Stringref stringref

Gaim::Stringref
gaim_stringref_new(class, value)
	const char *value
    C_ARGS:
	value

Gaim::Stringref
gaim_stringref_new_noref(class, value)
	const char *value
    C_ARGS:
	value

Gaim::Stringref
gaim_stringref_ref(stringref)
	Gaim::Stringref stringref

void
gaim_stringref_unref(stringref)
	Gaim::Stringref stringref

const char *
gaim_stringref_value(stringref)
	Gaim::Stringref stringref
