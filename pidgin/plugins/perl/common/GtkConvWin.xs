#include "gtkmodule.h"

MODULE = Pidgin::Conversation::Window  PACKAGE = Pidgin::Conversation::Window  PREFIX = pidgin_conv_window_
PROTOTYPES: ENABLE

Pidgin::Conversation::Window
pidgin_conv_window_new(class)
    C_ARGS: /* void */

void
pidgin_conv_window_destroy(win)
	Pidgin::Conversation::Window win

void
pidgin_conv_window_show(win)
	Pidgin::Conversation::Window win

void
pidgin_conv_window_hide(win)
	Pidgin::Conversation::Window win

void
pidgin_conv_window_raise(win)
	Pidgin::Conversation::Window win

void
pidgin_conv_window_switch_gtkconv(win, gtkconv)
	Pidgin::Conversation::Window win
	Pidgin::Conversation gtkconv

void
pidgin_conv_window_add_gtkconv(win, gtkconv)
	Pidgin::Conversation::Window win
	Pidgin::Conversation gtkconv

void
pidgin_conv_window_remove_gtkconv(win, gtkconv)
	Pidgin::Conversation::Window win
	Pidgin::Conversation gtkconv

Pidgin::Conversation
pidgin_conv_window_get_gtkconv_at_index(win, index)
	Pidgin::Conversation::Window win
	int index

Pidgin::Conversation
pidgin_conv_window_get_active_gtkconv(win)
	Pidgin::Conversation::Window win

Purple::Conversation
pidgin_conv_window_get_active_conversation(win)
	Pidgin::Conversation::Window win

gboolean
pidgin_conv_window_is_active_conversation(conv)
	Purple::Conversation conv

gboolean
pidgin_conv_window_has_focus(win)
	Pidgin::Conversation::Window win

void
pidgin_conv_window_get_gtkconvs(win)
	Pidgin::Conversation::Window win
PREINIT:
	GList *l;
PPCODE:
	for (l = pidgin_conv_window_get_gtkconvs(win); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Pidgin::Conversation")));
	}

guint
pidgin_conv_window_get_gtkconv_count(win)
	Pidgin::Conversation::Window win

Pidgin::Conversation::Window
pidgin_conv_window_first_with_type(type)
	Purple::ConversationType type

Pidgin::Conversation::Window
pidgin_conv_window_last_with_type(type)
	Purple::ConversationType type

MODULE = Pidgin::Conversation::Window  PACKAGE = Pidgin::Conversation::Placement  PREFIX = pidgin_conv_placement_
PROTOTYPES: ENABLE

void
pidgin_conv_placement_get_options()
PREINIT:
	GList *l;
PPCODE:
	for (l = pidgin_conv_placement_get_options(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Pidgin::Conversation::Window")));
	}

void
pidgin_conv_placement_add_fnc(id, name, fnc)
	const char * id
	const char * name
	Pidgin::Conversation::PlacementFunc fnc

void
pidgin_conv_placement_remove_fnc(id)
	const char * id

const char *
pidgin_conv_placement_get_name(id)
	const char * id

Pidgin::Conversation::PlacementFunc
pidgin_conv_placement_get_fnc(id)
	const char * id

void
pidgin_conv_placement_set_current_func(func)
	Pidgin::Conversation::PlacementFunc func

Pidgin::Conversation::PlacementFunc
pidgin_conv_placement_get_current_func()

void
pidgin_conv_placement_place(gtkconv)
	Pidgin::Conversation gtkconv

MODULE = Pidgin::Conversation::Window  PACKAGE = Pidgin::Conversation::Windows  PREFIX = pidgin_conv_windows_
PROTOTYPES: ENABLE

void
pidgin_conv_windows_get_list()
PREINIT:
	GList *l;
PPCODE:
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Pidgin::Conversation::Window")));
	}
