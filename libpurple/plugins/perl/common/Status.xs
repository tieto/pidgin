#include "module.h"

/* TODO

void
purple_status_type_add_attrs(status_type, id, name, value, purple_status_type_add_attrs)
	Purple::StatusType status_type
	const char *id
	const char *name
	Purple::Value value
	...

Purple::StatusType
purple_status_type_new_with_attrs(primitive, id, name, saveable, user_settable, independent, attr_id, attr_name, attr_value, purple_status_type_new_with_attrs)
	Purple::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean saveable
	gboolean user_settable
	gboolean independent
	const char *attr_id
	const char *attr_name
	Purple::Value attr_value
	...

*/

/* These break on faceprint's amd64 box
void
purple_status_type_add_attrs_vargs(status_type, args)
	Purple::StatusType status_type
	va_list args

void
purple_status_set_active_with_attrs(status, active, args)
	Purple::Status status
	gboolean active
	va_list args

	*/

MODULE = Purple::Status  PACKAGE = Purple::Primitive  PREFIX = purple_primitive_
PROTOTYPES: ENABLE

BOOT:
{
	HV *primitive_stash = gv_stashpv("Purple::Status::Primitive", 1);

	static const constiv *civ, primitive_const_iv[] = {
#undef const_iv
#define const_iv(name) {#name, (IV)PURPLE_STATUS_##name}
		const_iv(UNSET),
		const_iv(OFFLINE),
		const_iv(AVAILABLE),
		const_iv(UNAVAILABLE),
		const_iv(INVISIBLE),
		const_iv(AWAY),
		const_iv(EXTENDED_AWAY),
		const_iv(MOBILE),
	};

	for (civ = primitive_const_iv + sizeof(primitive_const_iv) / sizeof(primitive_const_iv[0]); civ-- > primitive_const_iv; )
		newCONSTSUB(primitive_stash, (char *)civ->name, newSViv(civ->iv));
}

const char *
purple_primitive_get_id_from_type(type)
	Purple::StatusPrimitive type

const char *
purple_primitive_get_name_from_type(type)
	Purple::StatusPrimitive type

Purple::StatusPrimitive
purple_primitive_get_type_from_id(id)
	const char *id

MODULE = Purple::Status  PACKAGE = Purple::StatusAttr PREFIX = purple_status_attr_
PROTOTYPES: ENABLE

void
purple_status_attr_destroy(attr)
	Purple::StatusAttr attr

const char *
purple_status_attr_get_id(attr)
	Purple::StatusAttr attr

const char *
purple_status_attr_get_name(attr)
	Purple::StatusAttr attr

GValue *
purple_status_attr_get_value(attr)
	Purple::StatusAttr attr

Purple::StatusAttr
purple_status_attr_new(id, name, value_type)
	const char *id
	const char *name
	GValue *value_type

MODULE = Purple::Status  PACKAGE = Purple::Status  PREFIX = purple_status_
PROTOTYPES: ENABLE

gint
purple_status_compare(status1, status2)
	Purple::Status status1
	Purple::Status status2

void
purple_status_destroy(status)
	Purple::Status status

gboolean
purple_status_get_attr_boolean(status, id)
	Purple::Status status
	const char *id

int
purple_status_get_attr_int(status, id)
	Purple::Status status
	const char *id

const char *
purple_status_get_attr_string(status, id)
	Purple::Status status
	const char *id

GValue *
purple_status_get_attr_value(status, id)
	Purple::Status status
	const char *id

Purple::Handle
purple_status_get_handle()

const char *
purple_status_get_id(status)
	Purple::Status status

const char *
purple_status_get_name(status)
	Purple::Status status

Purple::Presence
purple_status_get_presence(status)
	Purple::Status status

Purple::StatusType
purple_status_get_type(status)
	Purple::Status status

gboolean
purple_status_is_active(status)
	Purple::Status status

gboolean
purple_status_is_available(status)
	Purple::Status status

gboolean
purple_status_is_exclusive(status)
	Purple::Status status

gboolean
purple_status_is_independent(status)
	Purple::Status status

gboolean
purple_status_is_online(status)
	Purple::Status status

Purple::Status
purple_status_new(status_type, presence)
	Purple::StatusType status_type
	Purple::Presence presence

void
purple_status_set_active(status, active)
	Purple::Status status
	gboolean active

MODULE = Purple::Status  PACKAGE = Purple::StatusType  PREFIX = purple_status_type_
PROTOTYPES: ENABLE

void
purple_status_type_destroy(status_type)
	Purple::StatusType status_type

Purple::StatusAttr
purple_status_type_get_attr(status_type, id)
	Purple::StatusType status_type
	const char *id

void
purple_status_type_get_attrs(status_type)
	Purple::StatusType status_type
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_status_type_get_attrs(status_type); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::StatusAttr")));
	}

Purple::StatusType
purple_status_type_find_with_id(status_types, id)
	SV *status_types
	const char *id
PREINIT:
	GList *t_GL;
	int i, t_len;
CODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(status_types));

	for (i = 0; i <= t_len; i++) {
		t_GL = g_list_append(t_GL, SvPVutf8_nolen(*av_fetch((AV *)SvRV(status_types), i, 0)));
	}
	RETVAL = (PurpleStatusType *)purple_status_type_find_with_id(t_GL, id);
	g_list_free(t_GL);
OUTPUT:
	RETVAL

const char *
purple_status_type_get_id(status_type)
	Purple::StatusType status_type

const char *
purple_status_type_get_name(status_type)
	Purple::StatusType status_type

Purple::StatusPrimitive
purple_status_type_get_primitive(status_type)
	Purple::StatusType status_type

gboolean
purple_status_type_is_available(status_type)
	Purple::StatusType status_type

gboolean
purple_status_type_is_exclusive(status_type)
	Purple::StatusType status_type

gboolean
purple_status_type_is_independent(status_type)
	Purple::StatusType status_type

gboolean
purple_status_type_is_saveable(status_type)
	Purple::StatusType status_type

gboolean
purple_status_type_is_user_settable(status_type)
	Purple::StatusType status_type

Purple::StatusType
purple_status_type_new(primitive, id, name, user_settable)
	Purple::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean user_settable

Purple::StatusType
purple_status_type_new_full(primitive, id, name, saveable, user_settable, independent)
	Purple::StatusPrimitive primitive
	const char *id
	const char *name
	gboolean saveable
	gboolean user_settable
	gboolean independent
