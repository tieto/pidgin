#ifndef _GAIM_PERL_COMMON_H_
#define _GAIM_PERL_COMMON_H_

#include <XSUB.h>
#include <EXTERN.h>
#include <perl.h>
#include <glib.h>


//#define plain_bless(object, stash) \
//	sv_bless(sv_setref_pv(newRV((object))))

//#define plain_bless(object, stash) \
//	((object) == NULL ? &PL_sv_undef : \
//	gaim_perl_bless_plain((stash), (object)))

#define is_hvref(o) \
	((o) && SvROK(o) && SvRV(o) && (SvTYPE(SvRV(o)) == SVt_PVHV))

#define hvref(o) \
	(is_hvref(o) ? (HV *)SvRV(o) : NULL);

#define GAIM_PERL_BOOT(x) \
	{ \
		extern void boot_Gaim__##x(pTHX_ CV *cv); \
		gaim_perl_callXS(boot_Gaim__##x, cv, mark); \
	}

void gaim_perl_callXS(void (*subaddr)(pTHX_ CV *cv), CV *cv, SV **mark);
void gaim_perl_bless_plain(const char *stash, void *object);
SV *gaim_perl_bless_object(void *object, const char *stash);
gboolean gaim_perl_is_ref_object(SV *o);
void *gaim_perl_ref_object(SV *o);

int execute_perl(const char *function, int argc, char **args);

#endif /* _GAIM_PERL_COMMON_H_ */
