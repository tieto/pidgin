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

#include "internal.h"
#include "glibcompat.h"

#include "debug.h"
#include "smiley.h"
#include "smiley-remote.h"

#define PURPLE_REMOTE_SMILEY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_REMOTE_SMILEY, PurpleRemoteSmileyPrivate))

typedef struct {
	GString *contents;
	PurpleStoredImage *image; /* it's not the same as in parent */

	gboolean failed;
} PurpleRemoteSmileyPrivate;

#if 0
enum
{
	PROP_0,
	PROP_LAST
};
#endif

enum
{
	SIG_FAILED,
	SIG_LAST
};

static GObjectClass *parent_class;

static guint signals[SIG_LAST];
#if 0
static GParamSpec *properties[PROP_LAST];
#endif

/******************************************************************************
 * API implementation
 ******************************************************************************/

void
purple_remote_smiley_write(PurpleRemoteSmiley *smiley, const guchar *data,
	gsize size)
{
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!priv->failed);
	g_return_if_fail(!purple_smiley_is_ready(PURPLE_SMILEY(smiley)));
	g_return_if_fail(data != NULL || size == 0);

	if (size == 0)
		return;

	g_string_append_len(priv->contents, (const gchar*)data, size);
}

void
purple_remote_smiley_close(PurpleRemoteSmiley *smiley)
{
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!priv->failed);
	g_return_if_fail(!purple_smiley_is_ready(PURPLE_SMILEY(smiley)));
	g_return_if_fail(priv->contents != NULL);
	g_return_if_fail(priv->image == NULL);

	if (priv->contents->len == 0) {
		purple_debug_error("smiley-remote", "Smiley is empty");
		purple_remote_smiley_failed(smiley);
		return;
	}

	priv->image = purple_imgstore_new(priv->contents->str,
		priv->contents->len, NULL);
	g_return_if_fail(priv->image != NULL);
	g_string_free(priv->contents, FALSE);
	priv->contents = NULL;

	g_object_set(smiley, "is-ready", TRUE, NULL);
	g_signal_emit_by_name(smiley, "ready");
}

void
purple_remote_smiley_failed(PurpleRemoteSmiley *smiley)
{
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(!purple_smiley_is_ready(PURPLE_SMILEY(smiley)));

	if (priv->failed)
		return;

	g_object_set(smiley, "failed", TRUE, NULL);
	g_signal_emit(smiley, signals[SIG_FAILED], 0);
}

static PurpleStoredImage *
purple_remote_smiley_get_image_impl(PurpleSmiley *smiley)
{
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	g_return_val_if_fail(priv != NULL, FALSE);

	if (purple_smiley_get_path(smiley))
		return PURPLE_SMILEY_CLASS(parent_class)->get_image(smiley);

	if (!priv->image) {
		purple_debug_error("smiley-remote",
			"Remote smiley is not ready yet");
		return NULL;
	}

	return priv->image;
}

/******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_remote_smiley_init(GTypeInstance *instance, gpointer klass)
{
	PurpleRemoteSmiley *smiley = PURPLE_REMOTE_SMILEY(instance);
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	priv->contents = g_string_new(NULL);
}

static void
purple_remote_smiley_finalize(GObject *obj)
{
	PurpleRemoteSmiley *smiley = PURPLE_REMOTE_SMILEY(obj);
	PurpleRemoteSmileyPrivate *priv =
		PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);

	if (priv->contents)
		g_string_free(priv->contents, TRUE);
	if (priv->image)
		purple_imgstore_unref(priv->image);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

#if 0
static void
purple_remote_smiley_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleRemoteSmiley *remote_smiley = PURPLE_REMOTE_SMILEY(object);
	PurpleRemoteSmileyPrivate *priv = PURPLE_REMOTE_SMILEY_GET_PRIVATE(remote_smiley);

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_remote_smiley_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleRemoteSmiley *remote_smiley = PURPLE_REMOTE_SMILEY(object);
	PurpleRemoteSmileyPrivate *priv = PURPLE_REMOTE_SMILEY_GET_PRIVATE(remote_smiley);

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}
#endif

static void
purple_remote_smiley_class_init(PurpleRemoteSmileyClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);
	PurpleSmileyClass *ps_class = PURPLE_SMILEY_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleRemoteSmileyPrivate));

	gobj_class->finalize = purple_remote_smiley_finalize;

	ps_class->get_image = purple_remote_smiley_get_image_impl;

#if 0
	gobj_class->get_property = purple_remote_smiley_get_property;
	gobj_class->set_property = purple_remote_smiley_set_property;

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
#endif

	signals[SIG_FAILED] = g_signal_new("failed", G_OBJECT_CLASS_TYPE(klass),
		0, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GType
purple_remote_smiley_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleRemoteSmileyClass),
			.class_init = (GClassInitFunc)purple_remote_smiley_class_init,
			.instance_size = sizeof(PurpleRemoteSmiley),
			.instance_init = purple_remote_smiley_init,
		};

		type = g_type_register_static(PURPLE_TYPE_SMILEY,
			"PurpleRemoteSmiley", &info, 0);
	}

	return type;
}
