#include "image.h"

#include <debug.h>

#include "gg.h"
#include "gg-utils.h"

#define GGP_PENDING_IMAGE_ID_PREFIX "gg-pending-image-"

typedef struct
{
	uin_t from;
	gchar *text;
	time_t mtime;
} ggp_image_pending_message;

static void ggp_image_pending_message_free(gpointer data)
{
	ggp_image_pending_message *pending_message =
		(ggp_image_pending_message*)data;
	g_free(pending_message->text);
	g_free(pending_message);
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
	imgdata->pending_images =
		g_hash_table_new(g_direct_hash, g_direct_equal);
}

void ggp_image_free(PurpleConnection *gc)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	
	g_list_free_full(imgdata->pending_messages,
		&ggp_image_pending_message_free);
	g_hash_table_destroy(imgdata->pending_images); // TODO: remove content
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

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply)
{
	ggp_image_connection_data *imgdata = ggp_image_get_imgdata(gc);
	int stored_id;
	const char *imgtag_search;
	gchar *imgtag_replace;
	GList *pending_messages_it;
	
	stored_id = purple_imgstore_add_with_id(
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
				pending_message->text, PURPLE_MESSAGE_IMAGES,
				pending_message->mtime);
			
			ggp_image_pending_message_free(pending_message);
			imgdata->pending_messages = g_list_remove(
				imgdata->pending_messages, pending_message);
		}
		
		pending_messages_it = g_list_next(pending_messages_it);
	}
	g_free(imgtag_replace);
}
