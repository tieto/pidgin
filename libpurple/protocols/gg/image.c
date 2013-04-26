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

#define GGP_PENDING_IMAGE_ID_PREFIX "gg-pending-image-"

typedef struct
{
	uin_t from;
	gchar *text;
	time_t mtime;
} ggp_image_pending_message;

typedef struct
{
	int id;
	gchar *conv_name;
} ggp_image_pending_image;

static void ggp_image_pending_message_free(gpointer data)
{
	ggp_image_pending_message *pending_message =
		(ggp_image_pending_message*)data;
	g_free(pending_message->text);
	g_free(pending_message);
}

static void ggp_image_pending_image_free(gpointer data)
{
	ggp_image_pending_image *pending_image =
		(ggp_image_pending_image*)data;
	g_free(pending_image->conv_name);
	g_free(pending_image);
}

static inline ggp_image_connection_data *
ggp_image_get_imgdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return &accdata->image_data;
}

void ggp_image_setup(PurpleConnection *gc)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	
	imgdata->pending_messages = NULL;
	imgdata->pending_images = g_hash_table_new_full(NULL, NULL, NULL,
		ggp_image_pending_image_free);
}

void ggp_image_cleanup(PurpleConnection *gc)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	
	g_list_free_full(imgdata->pending_messages,
		&ggp_image_pending_message_free);
	g_hash_table_destroy(imgdata->pending_images);
}

const char * ggp_image_pending_placeholder(uint32_t id)
{
	static char buff[50];
	
	g_snprintf(buff, 50, "<img id=\"" GGP_PENDING_IMAGE_ID_PREFIX
		"%u\">", id);
	
	return buff;
}

void ggp_image_got_im(PurpleConnection *gc, uin_t from, gchar *text,
	time_t mtime)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	ggp_image_pending_message *pending_message =
		g_new(ggp_image_pending_message, 1);
	
	purple_debug_info("gg", "ggp_image_got_im: received message with "
		"images from %u: %s\n", from, text);
	
	pending_message->from = from;
	pending_message->text = text;
	pending_message->mtime = mtime;
	
	imgdata->pending_messages = g_list_append(imgdata->pending_messages,
		pending_message);
}

ggp_image_prepare_result ggp_image_prepare(PurpleConnection *gc, const int id,
	const char *conv_name, struct gg_msg_richtext_image *image_info)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
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
		"[id=%d, crc=%u, size=%" G_GSIZE_FORMAT ", filename=%s]\n",
		id, image_crc, image_size, image_filename);
	
	pending_image = g_new(ggp_image_pending_image, 1);
	pending_image->id = id;
	pending_image->conv_name = g_strdup(conv_name);
	g_hash_table_insert(imgdata->pending_images, GINT_TO_POINTER(image_crc),
		pending_image);
	
	image_info->unknown1 = 0x0109;
	image_info->size = gg_fix32(image_size);
	image_info->crc32 = gg_fix32(image_crc);
	
	return GGP_IMAGE_PREPARE_OK;
}

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	int stored_id;
	const char *imgtag_search;
	gchar *imgtag_replace;
	GList *pending_messages_it;
	
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
	
	purple_debug_info("gg", "ggp_image_recv: got image "
		"[id=%d, crc=%u, size=%u, filename=\"%s\"]\n",
		stored_id,
		image_reply->crc32,
		image_reply->size,
		image_reply->filename);

	imgtag_search = ggp_image_pending_placeholder(image_reply->crc32);
	imgtag_replace = g_strdup_printf("<img src=\""
		PURPLE_STORED_IMAGE_PROTOCOL "%u\">", stored_id);

	pending_messages_it = g_list_first(imgdata->pending_messages);
	while (pending_messages_it)
	{
		ggp_image_pending_message *pending_message =
			(ggp_image_pending_message*)pending_messages_it->data;
		gchar *newText;
		
		if (strstr(pending_message->text, imgtag_search) == NULL)
		{
			pending_messages_it = g_list_next(pending_messages_it);
			continue;
		}
		
		purple_debug_misc("gg", "ggp_image_recv: found message "
			"containing image: %s\n", pending_message->text);
		
		newText = purple_strreplace(pending_message->text,
			imgtag_search, imgtag_replace);
		g_free(pending_message->text);
		pending_message->text = newText;

		if (strstr(pending_message->text,
			"<img id=\"" GGP_PENDING_IMAGE_ID_PREFIX) == NULL)
		{
			purple_debug_info("gg", "ggp_image_recv: "
				"message is ready to display\n");
			serv_got_im(gc, ggp_uin_to_str(pending_message->from),
				pending_message->text,
				PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_IMAGES,
				pending_message->mtime);
			
			ggp_image_pending_message_free(pending_message);
			imgdata->pending_messages = g_list_remove(
				imgdata->pending_messages, pending_message);
		}
		
		pending_messages_it = g_list_next(pending_messages_it);
	}
	g_free(imgtag_replace);
}

void ggp_image_send(PurpleConnection *gc,
	const struct gg_event_image_request *image_request)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	ggp_image_pending_image *pending_image;
	PurpleStoredImage *image;
	PurpleConversation *conv;
	
	purple_debug_info("gg", "ggp_image_send: got image request "
		"[uin=%u, crc=%u, size=%u]\n",
		image_request->sender,
		image_request->crc32,
		image_request->size);
	
	pending_image = g_hash_table_lookup(imgdata->pending_images,
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
		g_hash_table_remove(imgdata->pending_images,
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
	
	g_hash_table_remove(imgdata->pending_images,
		GINT_TO_POINTER(image_request->crc32));
}
