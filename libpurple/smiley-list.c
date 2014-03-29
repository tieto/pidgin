/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "smiley-list.h"

#include "dbus-maybe.h"

#define PURPLE_SMILEY_LIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY_LIST, \
	PurpleSmileyListPrivate))

typedef struct {
	int x;
} PurpleSmileyListPrivate;

enum
{
	PROP_0,
	PROP_LAST
};

static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_smiley_list_init(GTypeInstance *instance, gpointer klass)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(instance);
	PURPLE_DBUS_REGISTER_POINTER(sl, PurpleSmileyList);
}

static void
purple_smiley_list_finalize(GObject *obj)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(obj);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	(void)priv;

	PURPLE_DBUS_UNREGISTER_POINTER(sl);
}

static void
purple_smiley_list_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(object);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	(void)priv;

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_list_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(object);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	(void)priv;

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_list_class_init(PurpleSmileyListClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleSmileyListPrivate));

	gobj_class->get_property = purple_smiley_list_get_property;
	gobj_class->set_property = purple_smiley_list_set_property;
	gobj_class->finalize = purple_smiley_list_finalize;

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
}

GType
purple_smiley_list_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyListClass),
			.class_init = (GClassInitFunc)purple_smiley_list_class_init,
			.instance_size = sizeof(PurpleSmileyList),
			.instance_init = purple_smiley_list_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmileyList", &info, 0);
	}

	return type;
}
