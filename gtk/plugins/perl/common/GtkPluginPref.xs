#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gaim_gtk_plugin_pref_create_frame(frame)
	Gaim::PluginPref::Frame frame

Gtk::Widget
gaim_gtk_prefs_checkbox(title, key, page)
	const char * title
	const char * key
	Gtk::Widget page

Gtk::Widget
gaim_gtk_prefs_labeled_spin_button(page, title, key, min, max, sg)
	Gtk::Widget page
	const gchar * title
	const char * key
	int min
	int max
	Gtk::Size::Group sg

Gtk::Widget
gaim_gtk_prefs_labeled_entry(page, title, key, sg)
	Gtk::Widget page
	const gchar * title
	const char * key
	Gtk::Size::Group sg
*/

/* TODO I don't know how to handle this in XS
Gtk::Widget
gaim_gtk_prefs_dropdown(page, title, type, key, ...)
	Gtk::Widget page
	const gchar * title
	Gaim::Pref::Type type
	const char * key

*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gaim_gtk_prefs_dropdown_from_list(page, title, type, key, menuitems)
	Gtk::Widget page
	const gchar * title
	Gaim::Pref::Type type
	const char * key
	SV *menuitems
PREINIT:
	GList *t_GL;
	int i, t_len;
CODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(menuitems));

	for ( i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(menuitems), i, 0), t_sl));
	RETVAL = gaim_gtk_prefs_dropdown_from_list(page, title, type, key, t_GL);
OUTPUT:
	RETVAL
*/

MODULE = Gaim::Gtk::PluginPref  PACKAGE = Gaim::Gtk::PluginPref  PREFIX = gaim_gtk_plugin_pref_
PROTOTYPES: ENABLE
