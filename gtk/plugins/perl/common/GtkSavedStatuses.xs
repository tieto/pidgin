#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
 * This also has a GCallback issue that I have no idea how to deal with,
 * though the core-perl typemap has a mapping for them.
Gtk::Widget
gaim_gtk_status_menu(status, callback)
	Gaim::SavedStatus status
	GCallback callback
*/

MODULE = Gaim::Gtk::Status  PACKAGE = Gaim::Gtk::Status  PREFIX = gaim_gtk_status_
PROTOTYPES: ENABLE

void *
gaim_gtk_status_get_handle()

MODULE = Gaim::Gtk::Status  PACKAGE = Gaim::Gtk::Status::Editor  PREFIX = gaim_gtk_status_editor_
PROTOTYPES: ENABLE

void
gaim_gtk_status_editor_show(edit, status)
	gboolean edit
	Gaim::SavedStatus status

MODULE = Gaim::Gtk::Status  PACKAGE = Gaim::Gtk::Status::Window  PREFIX = gaim_gtk_status_window_
PROTOTYPES: ENABLE

void
gaim_gtk_status_window_show()

void
gaim_gtk_status_window_hide()
