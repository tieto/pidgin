#include "debug.h"

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

/*
  2003/02/06: execute_perl modified by Mark Doliner <mark@kingant.net>
		Pass parameters by pushing them onto the stack rather than
		passing an array of strings.  This way, perl scripts can
		modify the parameters and we can get the changed values
		and then shoot ourselves.  I mean, uh, use them.

  2001/06/14: execute_perl replaced by Martin Persson <mep@passagen.se>
		previous use of perl_eval leaked memory, replaced with
		a version that uses perl_call instead

  30/11/2002: execute_perl modified by Eric Timme <timothy@voidnet.com>
		args changed to char** so that we can have preparsed
  		arguments again, and many headaches ensued! This essentially
		means we replaced one hacked method with a messier hacked
		method out of perceived necessity. Formerly execute_perl
		required a single char_ptr, and it would insert it into an
		array of character pointers and NULL terminate the new array.
		Now we have to pass in pre-terminated character pointer arrays
		to accomodate functions that want to pass in multiple arguments.

		Previously arguments were preparsed because an argument list
		was constructed in the form 'arg one','arg two' and was
		executed via a call like &funcname(arglist) (see .59.x), so
		the arglist was magically pre-parsed because of the method.
		With Martin Persson's change to perl_call we now need to
		use a null terminated list of character pointers for arguments
		if we wish them to be parsed. Lacking a better way to allow
		for both single arguments and many I created a NULL terminated
		array in every function that called execute_perl and passed
		that list into the function.  In the former version a single
		character pointer was passed in, and was placed into an array
		of character pointers with two elements, with a NULL element
		tacked onto the back, but this method no longer seemed prudent.

		Enhancements in the future might be to get rid of pre-declaring
		the array sizes?  I am not comfortable enough with this
		subject to attempt it myself and hope it to stand the test
		of time.
*/
int
execute_perl(const char *function, int argc, char **args)
{
	int count = 0, i, ret_value = 1;
	SV *sv_args[argc];
	STRLEN na;

	/*
	 * Set up the perl environment, push arguments onto the
	 * perl stack, then call the given function
	 */
	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	for (i = 0; i < argc; i++) {
		if (args[i]) {
			sv_args[i] = sv_2mortal(newSVpv(args[i], 0));
			XPUSHs(sv_args[i]);
		}
	}

	PUTBACK;
	count = call_pv(function, G_EVAL | G_SCALAR);
	SPAGAIN;

	/*
	 * Check for "die," make sure we have 1 argument, and set our
	 * return value.
	 */
	if (SvTRUE(ERRSV)) {
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Perl function %s exited abnormally: %s\n",
				   function, SvPV(ERRSV, na));
		POPs;
	}
	else if (count != 1) {
		/*
		 * This should NEVER happen.  G_SCALAR ensures that we WILL
		 * have 1 parameter.
		 */
		gaim_debug(GAIM_DEBUG_ERROR, "perl",
				   "Perl error from %s: expected 1 return value, "
				   "but got %d\n", function, count);
	}
	else
		ret_value = POPi;

	/* Check for changed arguments */
	for (i = 0; i < argc; i++) {
		if (args[i] && strcmp(args[i], SvPVX(sv_args[i]))) {
			/*
			 * Shizzel.  So the perl script changed one of the parameters,
			 * and we want this change to affect the original parameters.
			 * args[i] is just a tempory little list of pointers.  We don't
			 * want to free args[i] here because the new parameter doesn't
			 * overwrite the data that args[i] points to.  That is done by
			 * the function that called execute_perl.  I'm not explaining this
			 * very well.  See, it's aggregate...  Oh, but if 2 perl scripts
			 * both modify the data, _that's_ a memleak.  This is really kind
			 * of hackish.  I should fix it.  Look how long this comment is.
			 * Holy crap.
			 */
			args[i] = g_strdup(SvPV(sv_args[i], na));
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ret_value;
}


