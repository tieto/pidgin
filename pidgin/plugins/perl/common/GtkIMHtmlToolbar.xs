#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.

Gtk::Widget
gtk_imhtmltoolbar_new()

void
gtk_imhtmltoolbar_attach(toolbar, imhtml)
	Pidgin::IMHtmlToolbar toolbar
	Gtk::Widget imhtml
*/

MODULE = Pidgin::IMHtmlToolbar  PACKAGE = Pidgin::IMHtmlToolbar  PREFIX = gtk_imhtmltoolbar_
PROTOTYPES: ENABLE

void
gtk_imhtmltoolbar_associate_smileys(toolbar, proto_id)
	Pidgin::IMHtmlToolbar toolbar
	const char * proto_id
