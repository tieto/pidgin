#include "gtkmodule.h"

MODULE = Pidgin::Themes  PACKAGE = Pidgin::Themes  PREFIX = pidgin_themes_
PROTOTYPES: ENABLE

void
pidgin_themes_init()

gboolean
pidgin_themes_smileys_disabled()

void
pidgin_themes_smiley_theme_probe()

void
pidgin_themes_load_smiley_theme(file, load)
	const char * file
	gboolean load

void
pidgin_themes_get_proto_smileys(id)
	const char * id
PREINIT:
	GSList *l;
PPCODE:
	for (l = pidgin_themes_get_proto_smileys(id); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Pidgin::IMHtml::Smiley")));
	}
