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

#ifndef PURPLE_MEMORY_POOL_H
#define PURPLE_MEMORY_POOL_H
/**
 * SECTION:memorypool
 * @include:memorypool.h
 * @section_id: libpurple-memorypool
 * @short_description: a container for a large number of small chunks of memory
 * @title: Memory pools
 *
 * A #PurpleMemoryPool allows allocating many small objects within a single
 * memory range and releasing them all at once using a single call. This
 * prevents memory fragmentation and improves performance when used properly.
 * It's purpose is to act as an internal storage for other object private
 * structures, like tree nodes, string chunks, list elements.
 *
 * Current implementation is not optimized for releasing individual objects,
 * so it may be extremely inefficient, when misused. On every memory allocation,
 * it checks if there is enough space in current block. If there is not enough
 * room here, it creates another block of memory. On pool destruction or calling
 * #purple_memory_pool_cleanup, the whole block chain will be freed, using only
 * one #g_free call for every block.
 */

#include <glib-object.h>

#define PURPLE_TYPE_MEMORY_POOL (purple_memory_pool_get_type())
#define PURPLE_MEMORY_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPool))
#define PURPLE_MEMORY_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPoolClass))
#define PURPLE_IS_MEMORY_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_MEMORY_POOL))
#define PURPLE_IS_MEMORY_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_MEMORY_POOL))
#define PURPLE_MEMORY_POOL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_MEMORY_POOL, PurpleMemoryPoolClass))

typedef struct _PurpleMemoryPool PurpleMemoryPool;
typedef struct _PurpleMemoryPoolClass PurpleMemoryPoolClass;

/**
 * PurpleMemoryPool:
 *
 * The memory pool object instance.
 */
struct _PurpleMemoryPool
{
	/*< private >*/
	GObject parent_instance;
};

/**
 * PurpleMemoryPoolClass:
 * @palloc: alloates memory for a specific memory pool subclass,
 *          see #purple_memory_pool_alloc.
 * @pfree: frees memory allocated within a pool, see #purple_memory_pool_free.
 *         May be %NULL.
 * @cleanup: frees (or marks as unused) all memory allocated within a pool.
 *           See #purple_memory_pool_cleanup.
 *
 * Base class for #PurpleMemoryPool objects.
 */
struct _PurpleMemoryPoolClass
{
	/*< private >*/
	GObjectClass parent_class;

	/*< public >*/
	gpointer (*palloc)(PurpleMemoryPool *pool, gsize size, guint alignment);
	gpointer (*pfree)(PurpleMemoryPool *pool, gpointer mem);
	void (*cleanup)(PurpleMemoryPool *pool);

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_memory_pool_get_type:
 *
 * Returns: the #GType for a #PurpleMemoryPool.
 */
GType
purple_memory_pool_get_type(void);

/**
 * purple_memory_pool_new:
 *
 * Creates a new memory pool.
 *
 * Returns: the new #PurpleMemoryPool.
 */
PurpleMemoryPool *
purple_memory_pool_new(void);

/**
 * purple_memory_pool_set_block_size:
 * @pool: the memory pool.
 * @block_size: the new default block size.
 *
 * Sets new default block size for a memory pool. You might want to call this
 * before any allocation, to have it applied to the every created block.
 */
void
purple_memory_pool_set_block_size(PurpleMemoryPool *pool, gulong block_size);

/**
 * purple_memory_pool_alloc:
 * @pool: the memory pool.
 * @size: the size of memory to be allocated.
 * @alignment: the alignment of memory block (should be a power of two).
 *
 * Allocates an aligned memory block within a pool.
 *
 * Returns: the pointer to a memory block. This should be freed with
 *          a call to #purple_memory_pool_free.
 */
gpointer
purple_memory_pool_alloc(PurpleMemoryPool *pool, gsize size, guint alignment);

/**
 * purple_memory_pool_alloc0:
 * @pool: the memory pool.
 * @size: the size of memory to be allocated.
 * @alignment: the alignment of memory block (should be a power of two).
 *
 * Allocates an aligned memory block within a pool and sets its contents to
 * zeros.
 *
 * Returns: the pointer to a memory block. This should be freed with
 *          a call to #purple_memory_pool_free.
 */
gpointer
purple_memory_pool_alloc0(PurpleMemoryPool *pool, gsize size, guint alignment);

/**
 * purple_memory_pool_free:
 * @pool: the memory pool.
 * @mem: the pointer to a memory block.
 *
 * Frees a memory allocated within a memory pool. This can be a no-op in certain
 * implementations. Thus, it don't need to be called in every case. Thus, the
 * freed memory is wasted until you call #purple_memory_pool_cleanup
 * or destroy the @pool.
 */
void
purple_memory_pool_free(PurpleMemoryPool *pool, gpointer mem);

/**
 * purple_memory_pool_cleanup:
 * @pool: the memory pool.
 *
 * Marks all memory allocated within a memory pool as not used. It may free
 * resources, but don't have to.
 */
void
purple_memory_pool_cleanup(PurpleMemoryPool *pool);

/**
 * purple_memory_pool_strdup:
 * @pool: the memory pool.
 * @str: the string to duplicate.
 *
 * Duplicates a string using a memory allocated within a memory pool. If @str is
 * %NULL, it returns %NULL. The returned string should be freed with g_free()
 * when no longer needed.
 *
 * Returns: a newly-allocated copy of @str.
 */
gchar *
purple_memory_pool_strdup(PurpleMemoryPool *pool, const gchar *str);

G_END_DECLS

#endif /* PURPLE_MEMORY_POOL_H */
