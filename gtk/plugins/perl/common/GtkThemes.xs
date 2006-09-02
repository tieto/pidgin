#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gaim_gtkthemes_smiley_themeize(widget)
	Gtk::Widget * widget
*/

MODULE = Gaim::Gtk::Themes  PACKAGE = Gaim::Gtk::Themes  PREFIX = gaim_gtkthemes_
PROTOTYPES: ENABLE

void
gaim_gtkthemes_init()

gboolean
gaim_gtkthemes_smileys_disabled()

void
gaim_gtkthemes_smiley_theme_probe()

void
gaim_gtkthemes_load_smiley_theme(file, load)
	const char * file
	gboolean load

void
gaim_gtkthemes_get_proto_smileys(id)
	const char * id
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_gtkthemes_get_proto_smileys(id); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gtk::IMHtml::Smiley")));
	}
