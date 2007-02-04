#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
pidgin_status_box_new()

Gtk::Widget
pidgin_status_box_new_with_account(account)
	Gaim::Account account

void
pidgin_status_box_add(status_box, type, pixbuf, text, sec_text, data)
	Pidgin::StatusBox status_box
	Pidgin::StatusBox::ItemType type
	GdkPixbuf pixbuf
	const char * text
	const char * sec_text
	gpointer data
*/

MODULE = Pidgin::StatusBox  PACKAGE = Pidgin::StatusBox  PREFIX = pidgin_status_box_
PROTOTYPES: ENABLE

void
pidgin_status_box_add_separator(status_box)
	Pidgin::StatusBox status_box

void
pidgin_status_box_set_connecting(status_box, connecting)
	Pidgin::StatusBox status_box
	gboolean connecting

void
pidgin_status_box_pulse_connecting(status_box)
	Pidgin::StatusBox status_box

void
pidgin_status_box_set_buddy_icon(status_box, filename)
	Pidgin::StatusBox status_box
	const char * filename

const char *
pidgin_status_box_get_buddy_icon(status_box)
	Pidgin::StatusBox status_box

gchar_own *
pidgin_status_box_get_message(status_box)
	Pidgin::StatusBox status_box
