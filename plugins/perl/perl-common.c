#include <XSUB.h>
#include <EXTERN.h>
#include <perl.h>
#include <glib.h>

#include "perl-common.h"

extern PerlInterpreter *my_perl;

static GHashTable *object_stashes = NULL;

static int
magic_free_object(pTHX_ SV *sv, MAGIC *mg)
{
	sv_setiv(sv, 0);

	return 0;
}

static MGVTBL vtbl_free_object =
{
	NULL, NULL, NULL, NULL, magic_free_object
};

static SV *
create_sv_ptr(void *object)
{
	SV *sv;

	sv = newSViv((IV)object);

	sv_magic(sv, NULL, '~', NULL, 0);

	SvMAGIC(sv)->mg_private = 0x1551; /* HF */
	SvMAGIC(sv)->mg_virtual = &vtbl_free_object;

	return sv;
}

SV *
gaim_perl_bless_object(void *object, const char *stash_name)
{
	HV *stash;
	HV *hv;
	void *hash;

	if (object_stashes == NULL)
	{
		object_stashes = g_hash_table_new(g_direct_hash, g_direct_equal);
	}

	stash = gv_stashpv(stash_name, 1);

	hv = newHV();
	hv_store(hv, "_gaim", 5, create_sv_ptr(object), 0);

	return sv_bless(newRV_noinc((SV *)hv), stash);

//	return sv_bless(create_sv_ptr(object), gv_stashpv(stash, 1));
//	return create_sv_ptr(object);

//	dXSARGS;

//	return sv_setref_pv(ST(0), "Gaim::Account", create_sv_ptr(object));
}

gboolean
gaim_perl_is_ref_object(SV *o)
{
	SV **sv;
	HV *hv;

	hv = hvref(o);

	if (hv != NULL)
	{
		sv = hv_fetch(hv, "_gaim", 5, 0);

		if (sv != NULL)
			return TRUE;
	}

	return FALSE;
}

void *
gaim_perl_ref_object(SV *o)
{
	SV **sv;
	HV *hv;
	void *p;

	hv = hvref(o);

	if (hv == NULL)
		return NULL;

	sv = hv_fetch(hv, "_gaim", 5, 0);

	if (sv == NULL)
		croak("variable is damaged");

	p = GINT_TO_POINTER(SvIV(*sv));

	return p;
}

