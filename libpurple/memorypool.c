/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "memorypool.h"

#include <string.h>

#define PURPLE_MEMORY_POOL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPoolPrivate))
#define PURPLE_MEMORY_POOL_BLOCK_PADDING (sizeof(guint64))
#define PURPLE_MEMORY_POINTER_SHIFT(pointer, value) \
	(gpointer)((guintptr)(pointer) + (value))
#define PURPLE_MEMORY_PADDED(pointer, padding) \
	(gpointer)((((guintptr)(pointer) - 1) / (padding) + 1) * padding)

#define PURPLE_MEMORY_POOL_DEFAULT_BLOCK_SIZE 1024
#define PURPLE_MEMORY_POOL_DISABLED FALSE

typedef struct _PurpleMemoryPoolBlock PurpleMemoryPoolBlock;

typedef struct
{
	gboolean disabled;
	gulong block_size;

	PurpleMemoryPoolBlock *first_block;
	PurpleMemoryPoolBlock *last_block;
} PurpleMemoryPoolPrivate;

struct _PurpleMemoryPoolBlock
{
	gpointer available_ptr;
	gpointer end_ptr;
	PurpleMemoryPoolBlock *next;
};

enum
{
	PROP_ZERO,
	PROP_BLOCK_SIZE,
	PROP_LAST
};

static GObjectClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];


/*******************************************************************************
 * Memory allocation/deallocation
 ******************************************************************************/

static PurpleMemoryPoolBlock *
purple_memory_pool_block_new(gulong block_size)
{
	gpointer block_raw;
	PurpleMemoryPoolBlock *block;
	gsize total_size;

	/* ceil block struct size to the multipy of align */
	total_size = ((sizeof(PurpleMemoryPoolBlock) - 1) /
		PURPLE_MEMORY_POOL_BLOCK_PADDING + 1) *
		sizeof(PurpleMemoryPoolBlock);
	g_return_val_if_fail(block_size < G_MAXSIZE - total_size, NULL);
	total_size += block_size;

	block_raw = g_try_malloc(total_size);
	g_return_val_if_fail(block_raw != NULL, NULL);
	block = block_raw;

	/* in fact, we don't set available_ptr padded to
	 * PURPLE_MEMORY_POOL_BLOCK_PADDING, but we guarantee, there is at least
	 * block_size long block if padded to that value. */
	block->available_ptr = PURPLE_MEMORY_POINTER_SHIFT(block_raw,
		sizeof(PurpleMemoryPoolBlock));
	block->end_ptr = PURPLE_MEMORY_POINTER_SHIFT(block_raw, total_size);
	block->next = NULL;

	return block;
}

static gpointer
purple_memory_pool_alloc_impl(PurpleMemoryPool *pool, gsize size, guint alignment)
{
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);
	PurpleMemoryPoolBlock *blk;
	gpointer mem = NULL;

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->disabled) {
		/* XXX: this may cause some leaks */
		return g_try_malloc(size);
	}

	g_return_val_if_fail(alignment <= PURPLE_MEMORY_POOL_BLOCK_PADDING, NULL);
	g_warn_if_fail(alignment >= 1);
	if (alignment < 1)
		alignment = 1;

	blk = priv->last_block;

	if (blk) {
		mem = PURPLE_MEMORY_PADDED(blk->available_ptr, alignment);
		if (mem >= blk->end_ptr)
			mem = NULL;
		else if (mem < blk->available_ptr) /* gpointer overflow */
			mem = NULL;
		else if (PURPLE_MEMORY_POINTER_SHIFT(mem, size) >= blk->end_ptr)
			mem = NULL;
	}

	if (mem == NULL) {
		gsize real_size = priv->block_size;
		if (real_size < size)
			real_size = size;
		blk = purple_memory_pool_block_new(real_size);
		g_return_val_if_fail(blk != NULL, NULL);

		g_assert((priv->first_block == NULL) ==
			(priv->last_block == NULL));

		if (priv->first_block == NULL) {
			priv->first_block = blk;
			priv->last_block = blk;
		} else {
			priv->last_block->next = blk;
			priv->last_block = blk;
		}

		mem = PURPLE_MEMORY_PADDED(blk->available_ptr, alignment);
		g_assert((guintptr)mem + size < (guintptr)blk->end_ptr);
		g_assert(mem >= blk->available_ptr); /* gpointer overflow */
	}

	g_assert(blk != NULL);
	g_assert(mem != NULL);

	blk->available_ptr = PURPLE_MEMORY_POINTER_SHIFT(mem, size);
	g_assert(blk->available_ptr <= blk->end_ptr);

	return mem;
}

static void
purple_memory_pool_cleanup_impl(PurpleMemoryPool *pool)
{
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);
	PurpleMemoryPoolBlock *blk;

	g_return_if_fail(priv != NULL);

	blk = priv->first_block;
	priv->first_block = NULL;
	priv->last_block = NULL;
	while (blk) {
		PurpleMemoryPoolBlock *next = blk->next;
		g_free(blk);
		blk = next;
	}
}


/*******************************************************************************
 * API implementation
 ******************************************************************************/

void
purple_memory_pool_set_block_size(PurpleMemoryPool *pool, gulong block_size)
{
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);

	g_return_if_fail(priv != NULL);

	priv->block_size = block_size;
	g_object_notify_by_pspec(G_OBJECT(pool), properties[PROP_BLOCK_SIZE]);
}

gpointer
purple_memory_pool_alloc(PurpleMemoryPool *pool, gsize size, guint alignment)
{
	PurpleMemoryPoolClass *klass;

	if (size == 0)
		return NULL;

	g_return_val_if_fail(PURPLE_IS_MEMORY_POOL(pool), NULL);

	klass = PURPLE_MEMORY_POOL_GET_CLASS(pool);
	g_return_val_if_fail(klass != NULL, NULL);
	g_return_val_if_fail(klass->palloc != NULL, NULL);

	return klass->palloc(pool, size, alignment);
}

gpointer
purple_memory_pool_alloc0(PurpleMemoryPool *pool, gsize size, guint alignment)
{
	gpointer mem;

	if (size == 0)
		return NULL;

	mem = purple_memory_pool_alloc(pool, size, alignment);
	g_return_val_if_fail(mem != NULL, NULL);

	memset(mem, 0, size);

	return mem;
}

void
purple_memory_pool_free(PurpleMemoryPool *pool, gpointer mem)
{
	PurpleMemoryPoolClass *klass;

	if (mem == NULL)
		return;

	g_return_if_fail(PURPLE_IS_MEMORY_POOL(pool));

	klass = PURPLE_MEMORY_POOL_GET_CLASS(pool);
	g_return_if_fail(klass != NULL);

	if (klass->pfree)
		klass->pfree(pool, mem);
}

void
purple_memory_pool_cleanup(PurpleMemoryPool *pool)
{
	PurpleMemoryPoolClass *klass;

	g_return_if_fail(PURPLE_IS_MEMORY_POOL(pool));

	klass = PURPLE_MEMORY_POOL_GET_CLASS(pool);
	g_return_if_fail(klass != NULL);

	klass->cleanup(pool);
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

PurpleMemoryPool *
purple_memory_pool_new(void)
{
	return g_object_new(PURPLE_TYPE_MEMORY_POOL, NULL);
}

static void
purple_memory_pool_init(GTypeInstance *instance, gpointer klass)
{
	PurpleMemoryPool *pool = PURPLE_MEMORY_POOL(instance);
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);

	priv->disabled = PURPLE_MEMORY_POOL_DISABLED;
}

static void
purple_memory_pool_finalize(GObject *obj)
{
	purple_memory_pool_cleanup(PURPLE_MEMORY_POOL(obj));

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_memory_pool_get_property(GObject *obj, guint param_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleMemoryPool *pool = PURPLE_MEMORY_POOL(obj);
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);

	switch (param_id) {
		case PROP_BLOCK_SIZE:
			g_value_set_ulong(value, priv->block_size);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
	}
}

static void
purple_memory_pool_set_property(GObject *obj, guint param_id,
	const GValue *value, GParamSpec *pspec)
{
	PurpleMemoryPool *pool = PURPLE_MEMORY_POOL(obj);
	PurpleMemoryPoolPrivate *priv = PURPLE_MEMORY_POOL_GET_PRIVATE(pool);

	switch (param_id) {
		case PROP_BLOCK_SIZE:
			priv->block_size = g_value_get_ulong(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
	}
}

static void
purple_memory_pool_class_init(PurpleMemoryPoolClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleMemoryPoolPrivate));

	obj_class->finalize = purple_memory_pool_finalize;
	obj_class->get_property = purple_memory_pool_get_property;
	obj_class->set_property = purple_memory_pool_set_property;

	klass->palloc = purple_memory_pool_alloc_impl;
	klass->cleanup = purple_memory_pool_cleanup_impl;

	properties[PROP_BLOCK_SIZE] = g_param_spec_ulong("block-size",
		"Block size", "The default size of each block of pool memory.",
		0, G_MAXULONG, PURPLE_MEMORY_POOL_DEFAULT_BLOCK_SIZE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_memory_pool_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleMemoryPoolClass),
			.class_init = (GClassInitFunc)purple_memory_pool_class_init,
			.instance_size = sizeof(PurpleMemoryPool),
			.instance_init = (GInstanceInitFunc)purple_memory_pool_init
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleMemoryPool", &info, 0);
	}

	return type;
}

gchar *
purple_memory_pool_strdup(PurpleMemoryPool *pool, const gchar *str)
{
	gsize str_len;
	gchar *str_dup;

	if (str == NULL)
		return NULL;

	g_return_val_if_fail(PURPLE_IS_MEMORY_POOL(pool), NULL);

	str_len = strlen(str);
	str_dup = purple_memory_pool_alloc(pool, str_len + 1, sizeof(gchar));
	g_return_val_if_fail(str_dup != NULL, NULL);

	memcpy(str_dup, str, str_len);
	str_dup[str_len] = '\0';

	return str_dup;
}
