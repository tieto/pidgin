#include "module.h"

MODULE = Gaim::ConvWindow  PACKAGE = Gaim::ConvWindow  PREFIX = gaim_conv_window_
PROTOTYPES: ENABLE

Gaim::ConvWindow
gaim_conv_window_new()

void
DESTROY(win)
	Gaim::ConvWindow win
CODE:
	gaim_conv_window_destroy(win);


void
gaim_conv_window_show(win)
	Gaim::ConvWindow win

void
gaim_conv_window_hide(win)
	Gaim::ConvWindow win

void
gaim_conv_window_raise(win)
	Gaim::ConvWindow win

void
gaim_conv_window_flash(win)
	Gaim::ConvWindow win

int
gaim_conv_window_add_conversation(win, conv)
	Gaim::ConvWindow win
	Gaim::Conversation conv

Gaim::Conversation
gaim_conv_window_remove_conversation(win, conv)
	Gaim::ConvWindow win
	Gaim::Conversation conv

size_t
gaim_conv_window_get_conversation_count(win)
	Gaim::ConvWindow win

void
gaim_conv_window_switch_conversation(win, conv)
	Gaim::ConvWindow win
	Gaim::Conversation conv

Gaim::Conversation
gaim_conv_window_get_active_conversation(win)
	Gaim::ConvWindow win

void
conversations(win)
	Gaim::ConvWindow win
PREINIT:
	GList *l;
CODE:
	for (l = gaim_conv_window_get_conversations(win); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data,
			"Gaim::Conversation")));
	}


MODULE = Gaim::ConvWindow  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

void
conv_windows()
PREINIT:
	GList *l;
CODE:
	for (l = gaim_get_windows(); l != NULL; l = l->next)
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ConvWindow")));
