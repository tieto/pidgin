#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gaim_setup_imhtml(imhtml)
	Gtk::Widget imhtml

Gtk::Widget
gaim_gtk_create_imhtml(editable, imhtml_ret, toolbar_ret, sw_ret)
	gboolean editable
	GtkWidget ** imhtml_ret
	GtkWidget ** toolbar_ret
	GtkWidget ** sw_ret

void
gaim_gtk_toggle_sensitive(widget, to_toggle)
	Gtk::Widget widget
	Gtk::Widget to_toggle

void
gaim_gtk_set_sensitive_if_input(entry, dialog)
	Gtk::Widget entry
	Gtk::Widget dialog

void
gaim_gtk_toggle_sensitive_array(w, data)
	Gtk::Widget w
	GPtrArray data

void
gaim_gtk_toggle_showhide(widget, to_toggle)
	Gtk::Widget widget
	Gtk::Widget to_toggle

void
gaim_separator(menu)
	Gtk::Widget menu

Gtk::Widget
gaim_new_item(menu, str)
	Gtk::Widget menu
	const char * str

Gtk::Widget
gaim_new_check_item(menu, str, sf, data, checked)
	Gtk::Widget menu
	const char * str
	GtkSignalFunc sf
	gpointer data
	gboolean checked

Gtk::Widget
gaim_new_item_from_stock(menu, str, icon, sf, data, accel_key, accel_mods, mod)
	Gtk::Widget menu
	const char * str
	const char * icon
	GtkSignalFunc sf
	gpointer data
	guint accel_key
	guint accel_mods
	char * mod

Gtk::Widget
gaim_pixbuf_button_from_stock(text, icon, style)
	const char * text
	const char * icon
	Gaim::ButtonOrientation style

Gtk::Widget
gaim_gtk_make_frame(parent, title)
	Gtk::Widget parent
	const char * title

Gtk::Widget
gaim_gtk_protocol_option_menu_new(id, cb, user_data)
	const char * id
	GCallback cb
	gpointer user_data

Gtk::Widget
gaim_gtk_account_option_menu_new(default_account, show_all, cb, filter_func, user_data)
	Gaim::Account account
	gboolean show_all
	GCallback cb
	Gaim::Account::FilterFunc filter_func
	gpointer user_data

Gaim::Account
gaim_gtk_account_option_menu_get_selected(optmenu)
	Gtk::Widget optmenu

void
gaim_gtk_account_option_menu_set_selected(optmenu, account)
	Gtk::Widget optmenu
	Gaim::Account account

void
gaim_gtk_setup_screenname_autocomplete(entry, optmenu, all)
	Gtk::Widget entry
	Gtk::Widget optmenu
	gboolean all

gboolean
gaim_gtk_check_if_dir(path, filesel)
	const char * path
	Gtk::FileSelection filesel

void
gaim_gtk_setup_gtkspell(textview)
	Gtk::TextView textview

void
gaim_gtk_save_accels_cb(accel_group, arg1, arg2, arg3, data)
	Gtk::AccelGroup accel_group
	guint arg1
	Gdk::ModifierType arg2
	GClosure arg3
	gpointer data
*/

/* TODO This needs GaimAccount **
gboolean
gaim_gtk_parse_x_im_contact(msg, all_accounts, ret_account, ret_protocol, ret_username, ret_alias)
	const char * msg
	gboolean all_accounts
	Gaim::Account ret_account
	char ** ret_protocol
	char ** ret_username
	char ** ret_alias
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
void
gaim_set_accessible_lable(w, l)
	Gtk::Widget w
	Gtk::Widget l

void
gaim_gtk_treeview_popup_menu_position_func(menu, x, y, push_in, user_data)
	Gtk::Menu menu
	gint x
	gint y
	gboolean push_in
	gpointer user_data

void
gaim_dnd_file_manage(sd, account, who)
	Gtk::SelectionData sd
	Gaim::Account account
	const char * who

void
gaim_gtk_buddy_icon_get_scale_size(buf, spec, width, height)
	Gdk::Pixbuf buf
	Gaim::Buddy::Icon::Spec spec
	int width
	int height

Gdk::Pixbuf
gaim_gtk_create_prpl_icon(account, scale_factor)
	const Gaim::Account account
	double scale_factor

Gdk::Pixbuf
gaim_gtk_create_prpl_icon_with_status(account, status_type, scale_factor)
	const Gaim::Account account
	Gaim::StatusType status_type
	double scale_factor

Gdk::Pixbuf
gaim_gtk_create_gaim_icon_with_status(primitive, scale_factor)
	Gaim::StatusPrimitive primitive
	double scale_factor

void
gaim_gtk_append_menu_action(menu, act, gobject)
	Gtk::Widget menu
	Gaim::Menu::Action act
	gpointer gobject

void
gaim_gtk_set_cursor(widget, cursor_type)
	Gtk::Widget widget
	Gdk::CursorType cursor_type

void
gaim_gtk_clear_cursor(widget)
	Gtk::Widget widget
*/

MODULE = Gaim::Gtk::Utils  PACKAGE = Gaim::Gtk::Utils  PREFIX = gaim_gtk_utils_
PROTOTYPES: ENABLE

gboolean
gaim_gtk_save_accels(data)
	gpointer data

void
gaim_gtk_load_accels()

char *
gaim_gtk_convert_buddy_icon(plugin, path)
	Gaim::Plugin plugin
	const char * path
