/**
 * @file imgstore.h IM Image Store API
 * @ingroup core
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <glib.h>
#include "debug.h"
#include "imgstore.h"

static GSList *imgstore = NULL;
static int nextid = 0;

/**
 * Stored image
 *
 * Represents a single IM image awaiting display and/or transmission.
 * Now that this type is basicly private too, these two structs could
 * probably be combined.
 */
struct _PurpleStoredImage
{
	char *data;		/**< The image data.		*/
	size_t size;		/**< The image data's size.	*/
	char *filename;		/**< The filename (for the UI)	*/
};

typedef struct
{
	int id;
	int refcount;
	PurpleStoredImage *img;
} PurpleStoredImagePriv;

/* private functions */

static PurpleStoredImagePriv *purple_imgstore_get_priv(int id) {
	GSList *tmp = imgstore;
	PurpleStoredImagePriv *priv = NULL;

	g_return_val_if_fail(id > 0, NULL);

	while (tmp && !priv) {
		PurpleStoredImagePriv *tmp_priv = tmp->data;

		if (tmp_priv->id == id)
			priv = tmp_priv;

		tmp = tmp->next;
	}

	if (!priv)
		purple_debug(PURPLE_DEBUG_ERROR, "imgstore", "failed to find image id %d\n", id);

	return priv;
}

static void purple_imgstore_free_priv(PurpleStoredImagePriv *priv) {
	PurpleStoredImage *img = NULL;

	g_return_if_fail(priv != NULL);

	img = priv->img;
	if (img) {
		g_free(img->data);
		g_free(img->filename);
		g_free(img);
	}

	purple_debug(PURPLE_DEBUG_INFO, "imgstore", "freed image id %d\n", priv->id);

	g_free(priv);

	imgstore = g_slist_remove(imgstore, priv);
}

/* public functions */

int purple_imgstore_add(const void *data, size_t size, const char *filename) {
	PurpleStoredImagePriv *priv;
	PurpleStoredImage *img;

	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(size > 0, 0);

	img = g_new0(PurpleStoredImage, 1);
	img->data = g_memdup(data, size);
	img->size = size;
	img->filename = g_strdup(filename);

	priv = g_new0(PurpleStoredImagePriv, 1);
	priv->id = ++nextid;
	priv->refcount = 1;
	priv->img = img;

	imgstore = g_slist_append(imgstore, priv);
	purple_debug(PURPLE_DEBUG_INFO, "imgstore", "added image id %d\n", priv->id);

	return priv->id;
}

PurpleStoredImage *purple_imgstore_get(int id) {
	PurpleStoredImagePriv *priv = purple_imgstore_get_priv(id);

	g_return_val_if_fail(priv != NULL, NULL);

	purple_debug(PURPLE_DEBUG_INFO, "imgstore", "retrieved image id %d\n", priv->id);

	return priv->img;
}

gpointer purple_imgstore_get_data(PurpleStoredImage *i) {
	return i->data;
}

size_t purple_imgstore_get_size(PurpleStoredImage *i) {
	return i->size;
}

const char *purple_imgstore_get_filename(PurpleStoredImage *i) {
	return i->filename;
}

void purple_imgstore_ref(int id) {
	PurpleStoredImagePriv *priv = purple_imgstore_get_priv(id);

	g_return_if_fail(priv != NULL);

	(priv->refcount)++;

	purple_debug(PURPLE_DEBUG_INFO, "imgstore", "referenced image id %d (count now %d)\n", priv->id, priv->refcount);
}

void purple_imgstore_unref(int id) {
	PurpleStoredImagePriv *priv = purple_imgstore_get_priv(id);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->refcount > 0);

	(priv->refcount)--;

	purple_debug(PURPLE_DEBUG_INFO, "imgstore", "unreferenced image id %d (count now %d)\n", priv->id, priv->refcount);

	if (priv->refcount == 0)
		purple_imgstore_free_priv(priv);
}
