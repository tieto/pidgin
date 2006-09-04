#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gaim_gtk_plugin_pref_create_frame(frame)
	Gaim::PluginPref::Frame frame
*/

MODULE = Gaim::Gtk::PluginPref  PACKAGE = Gaim::Gtk::PluginPref  PREFIX = gaim_gtk_plugin_pref_
PROTOTYPES: ENABLE
