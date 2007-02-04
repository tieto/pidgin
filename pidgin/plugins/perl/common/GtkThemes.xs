#include "gtkmodule.h"

MODULE = Pidgin::Themes  PACKAGE = Pidgin::Themes  PREFIX = pidginthemes_
PROTOTYPES: ENABLE

void
pidginthemes_init()

gboolean
pidginthemes_smileys_disabled()

void
pidginthemes_smiley_theme_probe()

void
pidginthemes_load_smiley_theme(file, load)
	const char * file
	gboolean load

void
pidginthemes_get_proto_smileys(id)
	const char * id
PREINIT:
	GSList *l;
PPCODE:
	for (l = pidginthemes_get_proto_smileys(id); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Pidgin::IMHtml::Smiley")));
	}
