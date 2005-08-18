#include "module.h"

MODULE = Gaim::Roomlist  PACKAGE = Gaim::Roomlist  PREFIX = gaim_roomlist_
PROTOTYPES: ENABLE

void 
gaim_roomlist_cancel_get_list(list)
	Gaim::Roomlist list

void 
gaim_roomlist_expand_category(list, category)
	Gaim::Roomlist list
	Gaim::Roomlist::Room category

gboolean 
gaim_roomlist_get_in_progress(list)
	Gaim::Roomlist list

Gaim::Roomlist
gaim_roomlist_get_list(gc)
	Gaim::Connection gc

Gaim::Roomlist::UiOps
gaim_roomlist_get_ui_ops()
 

Gaim::Roomlist
gaim_roomlist_new(account)
	Gaim::Account account

void 
gaim_roomlist_ref(list)
	Gaim::Roomlist list

void 
gaim_roomlist_room_add(list, room)
	Gaim::Roomlist list
	Gaim::Roomlist::Room room

void 
gaim_roomlist_room_add_field(list, room, field)
	Gaim::Roomlist list
	Gaim::Roomlist::Room room
	gconstpointer field

void 
gaim_roomlist_room_join(list, room)
	Gaim::Roomlist list
	Gaim::Roomlist::Room room

void 
gaim_roomlist_set_fields(list, fields)
	Gaim::Roomlist list
	SV *fields
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(fields));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(fields), i, 0), t_sl));
	}
	gaim_roomlist_set_fields(list, t_GL);

void 
gaim_roomlist_set_in_progress(list, in_progress)
	Gaim::Roomlist list
	gboolean in_progress

void 
gaim_roomlist_set_ui_ops(ops)
	Gaim::Roomlist::UiOps ops

void 
gaim_roomlist_show_with_account(account)
	Gaim::Account account

void 
gaim_roomlist_unref(list)
	Gaim::Roomlist list

