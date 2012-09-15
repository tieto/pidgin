/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "image.h"

#include <debug.h>
#include <glibcompat.h>

#include "gg.h"
#include "utils.h"

struct _ggp_image_session_data
{
	GHashTable *got_images;
	GHashTable *incoming_images;
	GHashTable *pending_images;
};

typedef struct
{
	int id;
	gchar *conv_name;
} ggp_image_pending_image;

typedef struct
{
	GList *listeners;
} ggp_image_requested;

typedef struct
{
	ggp_image_request_cb cb;
	gpointer user_data;
} ggp_image_requested_listener;

static void ggp_image_pending_image_free(gpointer data)
{
	ggp_image_pending_image *pending_image =
		(ggp_image_pending_image*)data;
	g_free(pending_image->conv_name);
	g_free(pending_image);
}

static void ggp_image_requested_free(gpointer data)
{
	ggp_image_requested *req = data;
	g_list_free_full(req->listeners, g_free);
	g_free(req);
}

static inline ggp_image_session_data *
ggp_image_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->image_data;
}

void ggp_image_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = g_new0(ggp_image_session_data, 1);
	
	accdata->image_data = sdata;
	
	sdata->got_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal, g_free, NULL);
	sdata->incoming_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal,
		g_free, ggp_image_requested_free);
	sdata->pending_images = g_hash_table_new_full(NULL, NULL, NULL,
		ggp_image_pending_image_free);
}

void ggp_image_cleanup(PurpleConnection *gc)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	
	g_hash_table_destroy(sdata->got_images);
	g_hash_table_destroy(sdata->incoming_images);
	g_hash_table_destroy(sdata->pending_images);
	g_free(sdata);
}

ggp_image_prepare_result ggp_image_prepare(PurpleConnection *gc, const int id,
	const char *conv_name, struct gg_msg_richtext_image *image_info)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	PurpleStoredImage *image = purple_imgstore_find_by_id(id);
	size_t image_size;
	gconstpointer image_data;
	const char *image_filename;
	uint32_t image_crc;
	ggp_image_pending_image *pending_image;
	
	if (!image)
	{
		purple_debug_error("gg", "ggp_image_prepare_to_send: image %d "
			"not found in image store\n", id);
		return GGP_IMAGE_PREPARE_FAILURE;
	}
	
	image_size = purple_imgstore_get_size(image);
	
	if (image_size > GGP_IMAGE_SIZE_MAX)
	{
		purple_debug_warning("gg", "ggp_image_prepare_to_send: image "
			"is too big (max bytes: %d)\n", GGP_IMAGE_SIZE_MAX);
		return GGP_IMAGE_PREPARE_TOO_BIG;
	}
	
	purple_imgstore_ref(image);
	image_data = purple_imgstore_get_data(image);
	image_filename = purple_imgstore_get_filename(image);
	image_crc = gg_crc32(0, image_data, image_size);
	
	purple_debug_info("gg", "ggp_image_prepare_to_send: image prepared "
		"[id=%d, crc=%u, size=%zu, filename=%s]\n",
		id, image_crc, image_size, image_filename);
	
	pending_image = g_new(ggp_image_pending_image, 1);
	pending_image->id = id;
	pending_image->conv_name = g_strdup(conv_name);
	g_hash_table_insert(sdata->pending_images, GINT_TO_POINTER(image_crc),
		pending_image);
	
	image_info->unknown1 = 0x0109;
	image_info->size = gg_fix32(image_size);
	image_info->crc32 = gg_fix32(image_crc);
	
	return GGP_IMAGE_PREPARE_OK;
}

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	int stored_id;
	ggp_image_requested *req;
	GList *it;
	uint64_t id;
	
	stored_id = purple_imgstore_add_with_id(
		g_memdup(image_reply->image, image_reply->size),
		image_reply->size,
		image_reply->filename);
	
	id = ((uint64_t)image_reply->crc32 << 32) | image_reply->size;
	
	purple_debug_info("gg", "ggp_image_recv: got image "
		"[stored_id=%d, crc=%u, size=%u, id=%016llx]\n",
		stored_id,
		image_reply->crc32,
		image_reply->size,
		id);

	g_hash_table_insert(sdata->got_images, ggp_uint64dup(id),
		GINT_TO_POINTER(stored_id));

	req = g_hash_table_lookup(sdata->incoming_images, &id);
	if (!req)
	{
		purple_debug_warning("gg", "ggp_image_recv: "
			"image %016llx wasn't requested\n", id);
		return;
	}

	it = g_list_first(req->listeners);
	while (it)
	{
		ggp_image_requested_listener *listener = it->data;
		it = g_list_next(it);
		
		listener->cb(gc, id, stored_id, listener->user_data);
	}
	g_hash_table_remove(sdata->incoming_images, &id);
}

void ggp_image_send(PurpleConnection *gc,
	const struct gg_event_image_request *image_request)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	ggp_image_pending_image *pending_image;
	PurpleStoredImage *image;
	PurpleConversation *conv;
	
	purple_debug_info("gg", "ggp_image_send: got image request "
		"[uin=%u, crc=%u, size=%u]\n",
		image_request->sender,
		image_request->crc32,
		image_request->size);
	
	pending_image = g_hash_table_lookup(sdata->pending_images,
		GINT_TO_POINTER(image_request->crc32));
	
	if (pending_image == NULL)
	{
		purple_debug_warning("gg", "ggp_image_send: requested image "
			"not found\n");
		return;
	}
	
	purple_debug_misc("gg", "ggp_image_send: requested image found "
		"[id=%d, conv=%s]\n",
		pending_image->id,
		pending_image->conv_name);
	
	image = purple_imgstore_find_by_id(pending_image->id);
	
	if (!image)
	{
		purple_debug_error("gg", "ggp_image_send: requested image "
			"found, but doesn't exists in image store\n");
		g_hash_table_remove(sdata->pending_images,
			GINT_TO_POINTER(image_request->crc32));
		return;
	}
	
	//TODO: check allowed recipients
	gg_image_reply(accdata->session, image_request->sender,
		purple_imgstore_get_filename(image),
		purple_imgstore_get_data(image),
		purple_imgstore_get_size(image));
	purple_imgstore_unref(image);
	
	conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_IM, pending_image->conv_name,
		purple_connection_get_account(gc));
	if (conv != NULL)
		purple_conversation_write(conv, "", _("Image delivered."),
			PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_NOTIFY,
			time(NULL));
	
	g_hash_table_remove(sdata->pending_images,
		GINT_TO_POINTER(image_request->crc32));
}

void ggp_image_request(PurpleConnection *gc, uin_t uin, uint64_t id,
	ggp_image_request_cb cb, gpointer user_data)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	ggp_image_requested *req;
	ggp_image_requested_listener *listener;
	uint32_t crc = id >> 32;
	uint32_t size = id;
	
	if (size > GGP_IMAGE_SIZE_MAX && crc <= GGP_IMAGE_SIZE_MAX)
	{
		uint32_t tmp;
		purple_debug_warning("gg", "ggp_image_request: "
			"crc and size are swapped!\n");
		tmp = crc;
		crc = size;
		size = tmp;
	}
	
	req = g_hash_table_lookup(sdata->incoming_images, &id);
	if (!req)
	{
		req = g_new0(ggp_image_requested, 1);
		g_hash_table_insert(sdata->incoming_images,
			ggp_uint64dup(id), req);
		purple_debug_info("gg", "ggp_image_request: "
			"requesting image %016llx\n", id);
		if (gg_image_request(accdata->session, uin, size, crc) != 0)
			purple_debug_error("gg", "ggp_image_request: failed\n");
	}
	
	listener = g_new0(ggp_image_requested_listener, 1);
	listener->cb = cb;
	listener->user_data = user_data;
	req->listeners = g_list_append(req->listeners, listener);
}

int ggp_image_get_cached(PurpleConnection *gc, uint64_t id)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);

	return GPOINTER_TO_INT(g_hash_table_lookup(sdata->got_images, &id));
}
