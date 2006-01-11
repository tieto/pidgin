#include "module.h"

/* TODO

void
gaim_status_type_add_attrs(status_type, id, name, value, gaim_status_type_add_attrs)
	Gaim::StatusType status_type
	const char *id
	const char *name
	Gaim::Value value
	...

Gaim::StatusType
gaim_status_type_new_with_attrs(primitive, id, name, saveable, user_settable, independent, attr_id, attr_name, attr_value, gaim_status_type_new_with_attrs)
	Gaim::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean saveable
	gboolean user_settable
	gboolean independent
	const char *attr_id
	const char *attr_name
	Gaim::Value attr_value
	...

*/

/* These break on faceprint's amd64 box
void
gaim_status_type_add_attrs_vargs(status_type, args)
	Gaim::StatusType status_type
	va_list args

void
gaim_status_set_active_with_attrs(status, active, args)
	Gaim::Status status
	gboolean active
	va_list args

	*/

MODULE = Gaim::Status  PACKAGE = Gaim::Presence  PREFIX = gaim_presence_
PROTOTYPES: ENABLE

void
gaim_presence_add_list(presence, source_list)
	Gaim::Presence presence
	SV *source_list
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(source_list));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(source_list), i, 0), t_sl));
	}
	gaim_presence_add_list(presence, t_GL);

void
gaim_presence_add_status(presence, status)
	Gaim::Presence presence
	Gaim::Status status

gint
gaim_presence_compare(presence1, presence2)
	Gaim::Presence presence1
	Gaim::Presence presence2

void
gaim_presence_destroy(presence)
	Gaim::Presence presence

Gaim::Account
gaim_presence_get_account(presence)
	Gaim::Presence presence

Gaim::Status
gaim_presence_get_active_status(presence)
	Gaim::Presence presence

void
gaim_presence_get_buddies(presence)
	Gaim::Presence presence
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_presence_get_buddies(presence); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::BuddyList::Buddy")));
	}

const char *
gaim_presence_get_chat_user(presence)
	Gaim::Presence presence

Gaim::PresenceContext
gaim_presence_get_context(presence)
	Gaim::Presence presence

Gaim::Conversation
gaim_presence_get_conversation(presence)
	Gaim::Presence presence

time_t
gaim_presence_get_idle_time(presence)
	Gaim::Presence presence

time_t
gaim_presence_get_login_time(presence)
	Gaim::Presence presence

Gaim::Status
gaim_presence_get_status(presence, status_id)
	Gaim::Presence presence
	const char *status_id

void
gaim_presence_get_statuses(presence)
	Gaim::Presence presence
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_presence_get_statuses(presence); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Status")));
	}

gboolean
gaim_presence_is_available(presence)
	Gaim::Presence presence

gboolean
gaim_presence_is_idle(presence)
	Gaim::Presence presence

gboolean
gaim_presence_is_online(presence)
	Gaim::Presence presence

gboolean
gaim_presence_is_status_active(presence, status_id)
	Gaim::Presence presence
	const char *status_id

gboolean
gaim_presence_is_status_primitive_active(presence, primitive)
	Gaim::Presence presence
	Gaim::StatusPrimitive primitive

Gaim::Presence
gaim_presence_new(context)
	Gaim::PresenceContext context

Gaim::Presence
gaim_presence_new_for_account(account)
	Gaim::Account account

Gaim::Presence
gaim_presence_new_for_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

Gaim::Presence
gaim_presence_new_for_conv(conv)
	Gaim::Conversation conv

void
gaim_presence_remove_buddy(presence, buddy)
	Gaim::Presence presence
	Gaim::BuddyList::Buddy buddy

void
gaim_presence_set_idle(presence, idle, idle_time)
	Gaim::Presence presence
	gboolean idle
	time_t idle_time

void
gaim_presence_set_login_time(presence, login_time)
	Gaim::Presence presence
	time_t login_time

void
gaim_presence_set_status_active(presence, status_id, active)
	Gaim::Presence presence
	const char *status_id
	gboolean active

void
gaim_presence_switch_status(presence, status_id)
	Gaim::Presence presence
	const char *status_id

MODULE = Gaim::Status  PACKAGE = Gaim::Primitive  PREFIX = gaim_primitive_
PROTOTYPES: ENABLE

const char *
gaim_primitive_get_id_from_type(type)
	Gaim::StatusPrimitive type

const char *
gaim_primitive_get_name_from_type(type)
	Gaim::StatusPrimitive type

Gaim::StatusPrimitive
gaim_primitive_get_type_from_id(id)
	const char *id

MODULE = Gaim::Status  PACKAGE = Gaim::StatusAttr PREFIX = gaim_status_attr_
PROTOTYPES: ENABLE

void
gaim_status_attr_destroy(attr)
	Gaim::StatusAttr attr

const char *
gaim_status_attr_get_id(attr)
	Gaim::StatusAttr attr

const char *
gaim_status_attr_get_name(attr)
	Gaim::StatusAttr attr

Gaim::Value
gaim_status_attr_get_value(attr)
	Gaim::StatusAttr attr

Gaim::StatusAttr
gaim_status_attr_new(id, name, value_type)
	const char *id
	const char *name
	Gaim::Value value_type

MODULE = Gaim::Status  PACKAGE = Gaim::Status  PREFIX = gaim_status_
PROTOTYPES: ENABLE

gint
gaim_status_compare(status1, status2)
	Gaim::Status status1
	Gaim::Status status2

void
gaim_status_destroy(status)
	Gaim::Status status

gboolean
gaim_status_get_attr_boolean(status, id)
	Gaim::Status status
	const char *id

int
gaim_status_get_attr_int(status, id)
	Gaim::Status status
	const char *id

const char *
gaim_status_get_attr_string(status, id)
	Gaim::Status status
	const char *id

Gaim::Value
gaim_status_get_attr_value(status, id)
	Gaim::Status status
	const char *id

void *
gaim_status_get_handle()

const char *
gaim_status_get_id(status)
	Gaim::Status status

const char *
gaim_status_get_name(status)
	Gaim::Status status

Gaim::Presence
gaim_status_get_presence(status)
	Gaim::Status status

Gaim::StatusType
gaim_status_get_type(status)
	Gaim::Status status

gboolean
gaim_status_is_active(status)
	Gaim::Status status

gboolean
gaim_status_is_available(status)
	Gaim::Status status

gboolean
gaim_status_is_exclusive(status)
	Gaim::Status status

gboolean
gaim_status_is_independent(status)
	Gaim::Status status

gboolean
gaim_status_is_online(status)
	Gaim::Status status

Gaim::Status
gaim_status_new(status_type, presence)
	Gaim::StatusType status_type
	Gaim::Presence presence

void
gaim_status_set_active(status, active)
	Gaim::Status status
	gboolean active

void
gaim_status_set_attr_boolean(status, id, value)
	Gaim::Status status
	const char *id
	gboolean value

void
gaim_status_set_attr_string(status, id, value)
	Gaim::Status status
	const char *id
	const char *value

void
gaim_status_init()

void
gaim_status_uninit()

MODULE = Gaim::Status  PACKAGE = Gaim::StatusType  PREFIX = gaim_status_type_
PROTOTYPES: ENABLE

void
gaim_status_type_add_attr(status_type, id, name, value)
	Gaim::StatusType status_type
	const char *id
	const char *name
	Gaim::Value value

void
gaim_status_type_destroy(status_type)
	Gaim::StatusType status_type

Gaim::StatusType
gaim_status_type_find_with_id(status_types, id)
	SV *status_types
	const char *id
PREINIT:
/* XXX Check that this function actually works, I think it might need a */
/* status_type as it's first argument to work as $status_type->find_with_id */
/* properly. */
	GList *t_GL;
	int i, t_len;
CODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(status_types));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(status_types), i, 0), t_sl));
	}
	RETVAL = (GaimStatusType *)gaim_status_type_find_with_id(t_GL, id);
OUTPUT:
	RETVAL

Gaim::StatusAttr
gaim_status_type_get_attr(status_type, id)
	Gaim::StatusType status_type
	const char *id

void
gaim_status_type_get_attrs(status_type)
	Gaim::StatusType status_type
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_status_type_get_attrs(status_type); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::StatusAttr")));
	}

const char *
gaim_status_type_get_id(status_type)
	Gaim::StatusType status_type

const char *
gaim_status_type_get_name(status_type)
	Gaim::StatusType status_type

const char *
gaim_status_type_get_primary_attr(status_type)
	Gaim::StatusType status_type

Gaim::StatusPrimitive
gaim_status_type_get_primitive(status_type)
	Gaim::StatusType status_type

gboolean
gaim_status_type_is_available(status_type)
	Gaim::StatusType status_type

gboolean
gaim_status_type_is_exclusive(status_type)
	Gaim::StatusType status_type

gboolean
gaim_status_type_is_independent(status_type)
	Gaim::StatusType status_type

gboolean
gaim_status_type_is_saveable(status_type)
	Gaim::StatusType status_type

gboolean
gaim_status_type_is_user_settable(status_type)
	Gaim::StatusType status_type

Gaim::StatusType
gaim_status_type_new(primitive, id, name, user_settable)
	Gaim::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean user_settable

Gaim::StatusType
gaim_status_type_new_full(primitive, id, name, saveable, user_settable, independent)
	Gaim::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean saveable
	gboolean user_settable
	gboolean independent

void
gaim_status_type_set_primary_attr(status_type, attr_id)
	Gaim::StatusType status_type
	const char *attr_id
