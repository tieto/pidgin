/*
 * @file circbuffer.h Buffer Utility Functions
 * @ingroup core
 */

/* Purple is the legal property of its developers, whose names are too numerous
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

#include "circularbuffer.h"

#define DEFAULT_BUF_SIZE 256

#define PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CIRCULAR_BUFFER, PurpleCircularBufferPrivate))

/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *buffer;
	gsize growsize;
	gsize buflen;
	gsize bufused;
	gchar *input;
	gchar *output;
} PurpleCircularBufferPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_ZERO,
	PROP_GROW_SIZE,
	PROP_BUFFER_USED,
	PROP_INPUT,
	PROP_OUTPUT,
	PROP_LAST,
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Circular Buffer Implementation
 *****************************************************************************/
static void
purple_circular_buffer_real_grow(PurpleCircularBuffer *buffer, gsize len) {
	PurpleCircularBufferPrivate *priv = NULL;
	gsize in_offset = 0, out_offset = 0;
	gsize start_buflen;

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	start_buflen = priv->buflen;

	while((priv->buflen - priv->bufused) < len)
		priv->buflen += priv->growsize;

	if(priv->input != NULL) {
		in_offset = priv->input - priv->buffer;
		out_offset = priv->output - priv->buffer;
	}

	priv->buffer = g_realloc(priv->buffer, priv->buflen);

	/* adjust the fill and remove pointer locations */
	if(priv->input == NULL) {
		priv->input = priv->output = priv->buffer;
	} else {
		priv->input = priv->buffer + in_offset;
		priv->output = priv->buffer + out_offset;
	}

	/* If the fill pointer is wrapped to before the remove
	 * pointer, we need to shift the data */
	if(in_offset < out_offset
			|| (in_offset == out_offset && priv->bufused > 0))
	{
		gsize shift_n = MIN(priv->buflen - start_buflen, in_offset);
		memcpy(priv->buffer + start_buflen, priv->buffer, shift_n);

		/* If we couldn't fit the wrapped read buffer at the end */
		if (shift_n < in_offset) {
			memmove(priv->buffer, priv->buffer + shift_n, in_offset - shift_n);
			priv->input = priv->buffer + (in_offset - shift_n);
		} else {
			priv->input = priv->buffer + start_buflen + in_offset;
		}
	}
}

static void
purple_circular_buffer_real_append(PurpleCircularBuffer *buffer,
                                   gconstpointer src, gsize len)
{
	PurpleCircularBufferPrivate *priv = NULL;
	gsize len_stored;

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	/* Grow the buffer, if necessary */
	if((priv->buflen - priv->bufused) < len)
		purple_circular_buffer_grow(buffer, len);

	/* If there is not enough room to copy all of src before hitting
	 * the end of the buffer then we will need to do two copies.
	 * One copy from input to the end of the buffer, and the
	 * second copy from the start of the buffer to the end of src. */
	if(priv->input >= priv->output)
		len_stored = MIN(len, priv->buflen - (priv->input - priv->buffer));
	else
		len_stored = len;

	if(len_stored > 0)
		memcpy(priv->input, src, len_stored);

	if(len_stored < len) {
		memcpy(priv->buffer, (char*)src + len_stored, len - len_stored);
		priv->input = priv->buffer + (len - len_stored);
	} else {
		priv->input += len_stored;
	}

	priv->bufused += len;
}

static gsize
purple_circular_buffer_real_max_read_size(const PurpleCircularBuffer *buffer) {
	PurpleCircularBufferPrivate *priv = NULL;
	gsize max_read;

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	if(priv->bufused == 0)
		max_read = 0;
	else if((priv->output - priv->input) >= 0)
		max_read = priv->buflen - (priv->output - priv->buffer);
	else
		max_read = priv->input - priv->output;

	return max_read;
}

static gboolean
purple_circular_buffer_real_mark_read(PurpleCircularBuffer *buffer,
                                      gsize len)
{
	PurpleCircularBufferPrivate *priv = NULL;

	g_return_val_if_fail(purple_circular_buffer_get_max_read(buffer) >= len, FALSE);

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	priv->output += len;
	priv->bufused -= len;

	/* wrap to the start if we're at the end */
	if((gsize)(priv->output - priv->buffer) == priv->buflen)
		priv->output = priv->buffer;

	return TRUE;
}

/******************************************************************************
 * Private API
 *****************************************************************************/
static void
purple_circular_buffer_set_grow_size(PurpleCircularBuffer *buffer,
                                     gsize grow_size)
{
	PurpleCircularBufferPrivate *priv =
		PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	priv->growsize = (grow_size != 0) ? grow_size : DEFAULT_BUF_SIZE;

	g_object_notify(G_OBJECT(buffer), "grow-size");
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_circular_buffer_finalize(GObject *obj) {
	PurpleCircularBufferPrivate *priv =
		PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(obj);

	g_free(priv->buffer);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_circular_buffer_get_property(GObject *obj, guint param_id,
                                    GValue *value, GParamSpec *pspec)
{
	PurpleCircularBuffer *buffer = PURPLE_CIRCULAR_BUFFER(obj);

	switch(param_id) {
		case PROP_GROW_SIZE:
			g_value_set_ulong(value,
			                  purple_circular_buffer_get_grow_size(buffer));
			break;
		case PROP_BUFFER_USED:
			g_value_set_ulong(value,
			                  purple_circular_buffer_get_used(buffer));
			break;
		case PROP_INPUT:
			g_value_set_pointer(value,
			                    (void*) purple_circular_buffer_get_input(buffer));
			break;
		case PROP_OUTPUT:
			g_value_set_pointer(value,
			                    (void*) purple_circular_buffer_get_output(buffer));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_circular_buffer_set_property(GObject *obj, guint param_id,
                                    const GValue *value, GParamSpec *pspec)
{
	PurpleCircularBuffer *buffer = PURPLE_CIRCULAR_BUFFER(obj);

	switch(param_id) {
		case PROP_GROW_SIZE:
			purple_circular_buffer_set_grow_size(buffer,
			                                     g_value_get_ulong(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_circular_buffer_class_init(PurpleCircularBufferClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCircularBufferClass *buffer_class = PURPLE_CIRCULAR_BUFFER_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleCircularBufferPrivate));

	obj_class->finalize = purple_circular_buffer_finalize;
	obj_class->get_property = purple_circular_buffer_get_property;
	obj_class->set_property = purple_circular_buffer_set_property;

	buffer_class->grow = purple_circular_buffer_real_grow;
	buffer_class->append = purple_circular_buffer_real_append;
	buffer_class->max_read_size = purple_circular_buffer_real_max_read_size;
	buffer_class->mark_read = purple_circular_buffer_real_mark_read;

	/* using a ulong for the gsize properties since there is no
	 * g_param_spec_size, and the ulong should always work. --gk 3/21/11
	 */
	g_object_class_install_property(obj_class, PROP_GROW_SIZE,
		g_param_spec_ulong("grow-size", "grow-size",
		                   "The grow size of the buffer",
		                   0, G_MAXSIZE, 0,
		                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property(obj_class, PROP_BUFFER_USED,
		g_param_spec_ulong("buffer-used", "buffer-used",
		                   "The amount of the buffer used",
		                   0, G_MAXSIZE, 0,
		                   G_PARAM_READABLE));

	g_object_class_install_property(obj_class, PROP_INPUT,
		g_param_spec_pointer("input", "input",
		                     "The input pointer of the buffer",
		                     G_PARAM_READABLE));

	g_object_class_install_property(obj_class, PROP_OUTPUT,
		g_param_spec_pointer("output", "output",
		                     "The output pointer of the buffer",
		                     G_PARAM_READABLE));
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_circular_buffer_get_type(void) {
	static GType type = 0;

	if(G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleCircularBufferClass),
			.class_init = (GClassInitFunc)purple_circular_buffer_class_init,
			.instance_size = sizeof(PurpleCircularBuffer),
		};

		type = g_type_register_static(G_TYPE_OBJECT,
		                              "PurpleCircularBuffer",
		                              &info, 0);
	}

	return type;
}

PurpleCircularBuffer *
purple_circular_buffer_new(gsize growsize) {
	return g_object_new(PURPLE_TYPE_CIRCULAR_BUFFER,
	                    "grow-size", growsize ? growsize : DEFAULT_BUF_SIZE,
	                    NULL);
}

void
purple_circular_buffer_grow(PurpleCircularBuffer *buffer, gsize len) {
	PurpleCircularBufferClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer));

	klass = PURPLE_CIRCULAR_BUFFER_GET_CLASS(buffer);
	if(klass && klass->grow)
		klass->grow(buffer, len);
}

void
purple_circular_buffer_append(PurpleCircularBuffer *buffer, gconstpointer src,
                              gsize len)
{
	PurpleCircularBufferClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer));
	g_return_if_fail(src != NULL);

	klass = PURPLE_CIRCULAR_BUFFER_GET_CLASS(buffer);
	if(klass && klass->append)
		klass->append(buffer, src, len);
}

gsize
purple_circular_buffer_get_max_read(const PurpleCircularBuffer *buffer) {
	PurpleCircularBufferClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), 0);

	klass = PURPLE_CIRCULAR_BUFFER_GET_CLASS(buffer);
	if(klass && klass->max_read_size)
		return klass->max_read_size(buffer);

	return 0;
}

gboolean
purple_circular_buffer_mark_read(PurpleCircularBuffer *buffer, gsize len) {
	PurpleCircularBufferClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), FALSE);

	klass = PURPLE_CIRCULAR_BUFFER_CLASS(buffer);
	if(klass && klass->mark_read)
		return klass->mark_read(buffer, len);

	return FALSE;
}

gsize
purple_circular_buffer_get_grow_size(const PurpleCircularBuffer *buffer) {

	PurpleCircularBufferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), 0);

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	return priv->growsize;
}

gsize
purple_circular_buffer_get_used(const PurpleCircularBuffer *buffer) {
	PurpleCircularBufferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), 0);

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	return priv->bufused;
}

const gchar *
purple_circular_buffer_get_input(const PurpleCircularBuffer *buffer) {
	PurpleCircularBufferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), NULL);

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	return priv->input;
}

const gchar *
purple_circular_buffer_get_output(const PurpleCircularBuffer *buffer) {
	PurpleCircularBufferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer), NULL);

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	return priv->output;
}

void
purple_circular_buffer_reset(PurpleCircularBuffer *buffer) {
	PurpleCircularBufferPrivate *priv = NULL;
	GObject *obj = NULL;

	g_return_if_fail(PURPLE_IS_CIRCULAR_BUFFER(buffer));

	priv = PURPLE_CIRCULAR_BUFFER_GET_PRIVATE(buffer);

	priv->input = priv->buffer;
	priv->output = priv->buffer;

	obj = G_OBJECT(buffer);
	g_object_freeze_notify(obj);
	g_object_notify(obj, "input");
	g_object_notify(obj, "output");
	g_object_thaw_notify(obj);
}

