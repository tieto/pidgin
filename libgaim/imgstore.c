/**
 * @file imgstore.h IM Image Store API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
struct _GaimStoredImage
{
	char *data;		/**< The image data.		*/
	size_t size;		/**< The image data's size.	*/
	char *filename;		/**< The filename (for the UI)	*/
};

typedef struct
{
	int id;
	int refcount;
	GaimStoredImage *img;
} GaimStoredImagePriv;

/* private functions */

static GaimStoredImagePriv *gaim_imgstore_get_priv(int id) {
	GSList *tmp = imgstore;
	GaimStoredImagePriv *priv = NULL;

	g_return_val_if_fail(id > 0, NULL);

	while (tmp && !priv) {
		GaimStoredImagePriv *tmp_priv = tmp->data;

		if (tmp_priv->id == id)
			priv = tmp_priv;

		tmp = tmp->next;
	}

	if (!priv)
		gaim_debug(GAIM_DEBUG_ERROR, "imgstore", "failed to find image id %d\n", id);

	return priv;
}

static void gaim_imgstore_free_priv(GaimStoredImagePriv *priv) {
	GaimStoredImage *img = NULL;

	g_return_if_fail(priv != NULL);

	img = priv->img;
	if (img) {
		g_free(img->data);
		g_free(img->filename);
		g_free(img);
	}

	gaim_debug(GAIM_DEBUG_INFO, "imgstore", "freed image id %d\n", priv->id);

	g_free(priv);

	imgstore = g_slist_remove(imgstore, priv);
}

/* public functions */

int gaim_imgstore_add(const void *data, size_t size, const char *filename) {
	GaimStoredImagePriv *priv;
	GaimStoredImage *img;

	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(size > 0, 0);

	img = g_new0(GaimStoredImage, 1);
	img->data = g_memdup(data, size);
	img->size = size;
	img->filename = g_strdup(filename);

	priv = g_new0(GaimStoredImagePriv, 1);
	priv->id = ++nextid;
	priv->refcount = 1;
	priv->img = img;

	imgstore = g_slist_append(imgstore, priv);
	gaim_debug(GAIM_DEBUG_INFO, "imgstore", "added image id %d\n", priv->id);

	return priv->id;
}

GaimStoredImage *gaim_imgstore_get(int id) {
	GaimStoredImagePriv *priv = gaim_imgstore_get_priv(id);

	g_return_val_if_fail(priv != NULL, NULL);

	gaim_debug(GAIM_DEBUG_INFO, "imgstore", "retrieved image id %d\n", priv->id);

	return priv->img;
}

gpointer gaim_imgstore_get_data(GaimStoredImage *i) {
	return i->data;
}

size_t gaim_imgstore_get_size(GaimStoredImage *i) {
	return i->size;
}

const char *gaim_imgstore_get_filename(GaimStoredImage *i) {
	return i->filename;
}

void gaim_imgstore_ref(int id) {
	GaimStoredImagePriv *priv = gaim_imgstore_get_priv(id);

	g_return_if_fail(priv != NULL);

	(priv->refcount)++;

	gaim_debug(GAIM_DEBUG_INFO, "imgstore", "referenced image id %d (count now %d)\n", priv->id, priv->refcount);
}

void gaim_imgstore_unref(int id) {
	GaimStoredImagePriv *priv = gaim_imgstore_get_priv(id);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->refcount > 0);

	(priv->refcount)--;

	gaim_debug(GAIM_DEBUG_INFO, "imgstore", "unreferenced image id %d (count now %d)\n", priv->id, priv->refcount);

	if (priv->refcount == 0)
		gaim_imgstore_free_priv(priv);
}
