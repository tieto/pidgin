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
	GHashTable *sent_images;
};

typedef struct
{
	int id;
	gchar *conv_name; /* TODO: callback */
} ggp_image_sent;

typedef struct
{
	GList *listeners;
} ggp_image_requested;

typedef struct
{
	ggp_image_request_cb cb;
	gpointer user_data;
} ggp_image_requested_listener;

static void ggp_image_got_free(gpointer data)
{
	int id = GPOINTER_TO_INT(data);
	purple_imgstore_unref_by_id(id);
}

static void ggp_image_sent_free(gpointer data)
{
	ggp_image_sent *sent_image = (ggp_image_sent*)data;
	purple_imgstore_unref_by_id(sent_image->id);
	g_free(sent_image->conv_name);
	g_free(sent_image);
}

static void ggp_image_requested_free(gpointer data)
{
	ggp_image_requested *req = data;
	g_list_free_full(req->listeners, g_free);
	g_free(req);
}

static uint64_t ggp_image_params_to_id(uint32_t crc32, uint32_t size)
{
	return ((uint64_t)crc32 << 32) | size;
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
		g_int64_hash, g_int64_equal, g_free,
		ggp_image_got_free);
	sdata->incoming_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal, g_free,
		ggp_image_requested_free);
	sdata->sent_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal, g_free,
		ggp_image_sent_free);
}

void ggp_image_cleanup(PurpleConnection *gc)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	
	g_hash_table_destroy(sdata->got_images);
	g_hash_table_destroy(sdata->incoming_images);
	g_hash_table_destroy(sdata->sent_images);
	g_free(sdata);
}

ggp_image_prepare_result ggp_image_prepare(PurpleConversation *conv,
	const int stored_id, uint64_t *id)
{
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	PurpleStoredImage *image = purple_imgstore_find_by_id(stored_id);
	size_t image_size;
	gconstpointer image_data;
	uint32_t image_crc;
	ggp_image_sent *sent_image;
	
	if (!image)
	{
		purple_debug_error("gg", "ggp_image_prepare: image %d "
			"not found in image store\n", stored_id);
		return GGP_IMAGE_PREPARE_FAILURE;
	}
	
	image_size = purple_imgstore_get_size(image);
	
	if (image_size > GGP_IMAGE_SIZE_MAX)
	{
		purple_debug_warning("gg", "ggp_image_prepare: image "
			"is too big (max bytes: %d)\n", GGP_IMAGE_SIZE_MAX);
		return GGP_IMAGE_PREPARE_TOO_BIG;
	}
	
	purple_imgstore_ref(image);
	image_data = purple_imgstore_get_data(image);
	image_crc = gg_crc32(0, image_data, image_size);
	
	purple_debug_info("gg", "ggp_image_prepare: image prepared "
		"[id=%d, crc=%u, size=%" G_GSIZE_FORMAT "]\n",
		stored_id, image_crc, image_size);
	
	*id = ggp_image_params_to_id(image_crc, image_size);

	sent_image = g_new(ggp_image_sent, 1);
	sent_image->id = stored_id;
	sent_image->conv_name = g_strdup(purple_conversation_get_name(conv));
	g_hash_table_insert(sdata->sent_images, ggp_uint64dup(*id),
		sent_image);
	
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
	
	/* TODO: This PurpleStoredImage will be rendered within the IM window
	   and right-clicking the image will allow the user to save the image
	   to disk.  The default filename used in this dialog is the filename
	   that we pass to purple_imgstore_new_with_id(), so we should call
	   g_path_get_basename() and purple_escape_filename() on it before
	   passing it in.  This is easy, but it's not clear if there might be
	   other implications because this filename is used elsewhere within
	   this PRPL. */
	stored_id = purple_imgstore_new_with_id(
		g_memdup(image_reply->image, image_reply->size),
		image_reply->size,
		image_reply->filename);
	
	id = ggp_image_params_to_id(image_reply->crc32, image_reply->size);
	
	purple_debug_info("gg", "ggp_image_recv: got image "
		"[stored_id=%d, crc=%u, size=%u, filename=%s, id="
		GGP_IMAGE_ID_FORMAT "]\n",
		stored_id,
		image_reply->crc32,
		image_reply->size,
		image_reply->filename,
		id);

	g_hash_table_insert(sdata->got_images, ggp_uint64dup(id),
		GINT_TO_POINTER(stored_id));

	req = g_hash_table_lookup(sdata->incoming_images, &id);
	if (!req)
	{
		purple_debug_warning("gg", "ggp_image_recv: "
			"image " GGP_IMAGE_ID_FORMAT " wasn't requested\n",
			id);
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
	ggp_image_sent *sent_image;
	PurpleStoredImage *image;
	PurpleConversation *conv;
	uint64_t id;
	gchar *gg_filename;
	
	purple_debug_info("gg", "ggp_image_send: got image request "
		"[uin=%u, crc=%u, size=%u]\n",
		image_request->sender,
		image_request->crc32,
		image_request->size);
	
	id = ggp_image_params_to_id(image_request->crc32, image_request->size);
	
	sent_image = g_hash_table_lookup(sdata->sent_images, &id);
	
	if (sent_image == NULL && image_request->sender == ggp_str_to_uin(
		purple_account_get_username(purple_connection_get_account(gc))))
	{
		purple_debug_misc("gg", "ggp_image_send: requested image "
			"not found, but this may be another session request\n");
		return;
	}
	if (sent_image == NULL)
	{
		purple_debug_warning("gg", "ggp_image_send: requested image "
			"not found\n");
		return;
	}
	
	purple_debug_misc("gg", "ggp_image_send: requested image found "
		"[id=" GGP_IMAGE_ID_FORMAT ", stored id=%d, conv=%s]\n",
		id,
		sent_image->id,
		sent_image->conv_name);
	
	image = purple_imgstore_find_by_id(sent_image->id);
	
	if (!image)
	{
		purple_debug_error("gg", "ggp_image_send: requested image "
			"found, but doesn't exists in image store\n");
		g_hash_table_remove(sdata->sent_images,
			GINT_TO_POINTER(image_request->crc32));
		return;
	}
	
	//TODO: check allowed recipients
	gg_filename = g_strdup_printf(GGP_IMAGE_ID_FORMAT, id);
	gg_image_reply(accdata->session, image_request->sender,
		gg_filename,
		purple_imgstore_get_data(image),
		purple_imgstore_get_size(image));
	g_free(gg_filename);
	
	conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_ANY, sent_image->conv_name,
		purple_connection_get_account(gc));
	if (conv != NULL)
	{
		gchar *msg = g_strdup_printf(_("Image delivered to %u."),
			image_request->sender);
		purple_conversation_write(conv, "", msg,
			PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_NOTIFY,
			time(NULL));
		g_free(msg);
	}
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
			"requesting image " GGP_IMAGE_ID_FORMAT "\n", id);
		if (gg_image_request(accdata->session, uin, size, crc) != 0)
			purple_debug_error("gg", "ggp_image_request: failed\n");
	}
	else
	{
		purple_debug_info("gg", "ggp_image_request: "
			"image " GGP_IMAGE_ID_FORMAT " already requested\n",
			id);
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
