#include "gtkmodule.h"

MODULE = Gaim::Gtk::Pounce  PACKAGE = Gaim::Gtk::Pounce  PREFIX = gaim_gtk_pounce_
PROTOTYPES: ENABLE

void
gaim_gtk_pounce_editor_show(account, name, cur_pounce)
	Gaim::Account account
	const char * name
	Gaim::Pounce cur_pounce

MODULE = Gaim::Gtk::Pounce  PACKAGE = Gaim::Gtk::Pounces  PREFIX = gaim_gtk_pounces_
PROTOTYPES: ENABLE

void *
gaim_gtk_pounces_get_handle()

MODULE = Gaim::Gtk::Pounce  PACKAGE = Gaim::Gtk::Pounces::Manager  PREFIX = gaim_gtk_pounces_manager_
PROTOTYPES: ENABLE

void
gaim_gtk_pounces_manager_show()

void
gaim_gtk_pounces_manager_hide()
