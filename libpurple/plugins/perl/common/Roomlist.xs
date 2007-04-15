#include "module.h"

MODULE = Purple::Roomlist  PACKAGE = Purple::Roomlist  PREFIX = purple_roomlist_
PROTOTYPES: ENABLE

void 
purple_roomlist_cancel_get_list(list)
	Purple::Roomlist list

void 
purple_roomlist_expand_category(list, category)
	Purple::Roomlist list
	Purple::Roomlist::Room category

gboolean 
purple_roomlist_get_in_progress(list)
	Purple::Roomlist list

Purple::Roomlist
purple_roomlist_get_list(gc)
	Purple::Connection gc

Purple::Roomlist::UiOps
purple_roomlist_get_ui_ops()
 

Purple::Roomlist
purple_roomlist_new(account)
	Purple::Account account

void 
purple_roomlist_ref(list)
	Purple::Roomlist list

void 
purple_roomlist_room_add(list, room)
	Purple::Roomlist list
	Purple::Roomlist::Room room

void 
purple_roomlist_room_add_field(list, room, field)
	Purple::Roomlist list
	Purple::Roomlist::Room room
	gconstpointer field

void 
purple_roomlist_room_join(list, room)
	Purple::Roomlist list
	Purple::Roomlist::Room room

void 
purple_roomlist_set_fields(list, fields)
	Purple::Roomlist list
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
	purple_roomlist_set_fields(list, t_GL);

void 
purple_roomlist_set_in_progress(list, in_progress)
	Purple::Roomlist list
	gboolean in_progress

void 
purple_roomlist_set_ui_ops(ops)
	Purple::Roomlist::UiOps ops

void 
purple_roomlist_show_with_account(account)
	Purple::Account account

void 
purple_roomlist_unref(list)
	Purple::Roomlist list

