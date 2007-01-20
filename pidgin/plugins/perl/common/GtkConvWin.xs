#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Conversation::Window  PACKAGE = Gaim::GtkUI::Conversation::Window  PREFIX = gaim_gtk_conv_window_
PROTOTYPES: ENABLE

Gaim::GtkUI::Conversation::Window
gaim_gtk_conv_window_new(class)
    C_ARGS: /* void */

void
gaim_gtk_conv_window_destroy(win)
	Gaim::GtkUI::Conversation::Window win

void
gaim_gtk_conv_window_show(win)
	Gaim::GtkUI::Conversation::Window win

void
gaim_gtk_conv_window_hide(win)
	Gaim::GtkUI::Conversation::Window win

void
gaim_gtk_conv_window_raise(win)
	Gaim::GtkUI::Conversation::Window win

void
gaim_gtk_conv_window_switch_gtkconv(win, gtkconv)
	Gaim::GtkUI::Conversation::Window win
	Gaim::GtkUI::Conversation gtkconv

void
gaim_gtk_conv_window_add_gtkconv(win, gtkconv)
	Gaim::GtkUI::Conversation::Window win
	Gaim::GtkUI::Conversation gtkconv

void
gaim_gtk_conv_window_remove_gtkconv(win, gtkconv)
	Gaim::GtkUI::Conversation::Window win
	Gaim::GtkUI::Conversation gtkconv

Gaim::GtkUI::Conversation
gaim_gtk_conv_window_get_gtkconv_at_index(win, index)
	Gaim::GtkUI::Conversation::Window win
	int index

Gaim::GtkUI::Conversation
gaim_gtk_conv_window_get_active_gtkconv(win)
	Gaim::GtkUI::Conversation::Window win

Gaim::Conversation
gaim_gtk_conv_window_get_active_conversation(win)
	Gaim::GtkUI::Conversation::Window win

gboolean
gaim_gtk_conv_window_is_active_conversation(conv)
	Gaim::Conversation conv

gboolean
gaim_gtk_conv_window_has_focus(win)
	Gaim::GtkUI::Conversation::Window win

Gaim::GtkUI::Conversation::Window
gaim_gtk_conv_window_get_at_xy(x, y)
	int x
	int y

void
gaim_gtk_conv_window_get_gtkconvs(win)
	Gaim::GtkUI::Conversation::Window win
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_gtk_conv_window_get_gtkconvs(win); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::GtkUI::Conversation")));
	}

guint
gaim_gtk_conv_window_get_gtkconv_count(win)
	Gaim::GtkUI::Conversation::Window win

Gaim::GtkUI::Conversation::Window
gaim_gtk_conv_window_first_with_type(type)
	Gaim::ConversationType type

Gaim::GtkUI::Conversation::Window
gaim_gtk_conv_window_last_with_type(type)
	Gaim::ConversationType type

MODULE = Gaim::GtkUI::Conversation::Window  PACKAGE = Gaim::GtkUI::Conversation::Placement  PREFIX = gaim_gtkconv_placement_
PROTOTYPES: ENABLE

void
gaim_gtkconv_placement_get_options()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_gtkconv_placement_get_options(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::GtkUI::Conversation::Window")));
	}

void
gaim_gtkconv_placement_add_fnc(id, name, fnc)
	const char * id
	const char * name
	Gaim::Conversation::PlacementFunc fnc

void
gaim_gtkconv_placement_remove_fnc(id)
	const char * id

const char *
gaim_gtkconv_placement_get_name(id)
	const char * id

Gaim::Conversation::PlacementFunc
gaim_gtkconv_placement_get_fnc(id)
	const char * id

void
gaim_gtkconv_placement_set_current_func(func)
	Gaim::Conversation::PlacementFunc func

Gaim::Conversation::PlacementFunc
gaim_gtkconv_placement_get_current_func()

void
gaim_gtkconv_placement_place(gtkconv)
	Gaim::GtkUI::Conversation gtkconv

MODULE = Gaim::GtkUI::Conversation::Window  PACKAGE = Gaim::GtkUI::Conversation::Windows  PREFIX = gaim_gtk_conv_windows_
PROTOTYPES: ENABLE

void
gaim_gtk_conv_windows_get_list()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_gtk_conv_windows_get_list(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::GtkUI::Conversation::Window")));
	}
