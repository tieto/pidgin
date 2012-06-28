#ifndef _GGP_IMAGE_H
#define _GGP_IMAGE_H

#include <internal.h>
#include <libgadu.h>

#define GGP_IMAGE_SIZE_MAX 255000

typedef struct
{
	GList *pending_messages;
	GHashTable *pending_images;
} ggp_image_connection_data;

typedef enum
{
	GGP_IMAGE_PREPARE_OK = 0,
	GGP_IMAGE_PREPARE_FAILURE,
	GGP_IMAGE_PREPARE_TOO_BIG
} ggp_image_prepare_result;

void ggp_image_setup(PurpleConnection *gc);
void ggp_image_free(PurpleConnection *gc);

const char * ggp_image_pending_placeholder(uint32_t id);

void ggp_image_got_im(PurpleConnection *gc, uin_t from, gchar *msg,
	time_t mtime);
ggp_image_prepare_result ggp_image_prepare(PurpleConnection *gc, const int id,
	const char *conv_name, struct gg_msg_richtext_image *image_info);

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply);
void ggp_image_send(PurpleConnection *gc,
	const struct gg_event_image_request *image_request);

#endif /* _GGP_IMAGE_H */
