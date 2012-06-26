#ifndef _GGP_IMAGE_H
#define _GGP_IMAGE_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	GList *pending_messages;
	GHashTable *pending_images;
} ggp_image_connection_data;

void ggp_image_setup(PurpleConnection *gc);
void ggp_image_free(PurpleConnection *gc);

const char * ggp_image_pending_placeholder(uint32_t id);

void ggp_image_got_im(PurpleConnection *gc, uin_t from, gchar *msg,
	time_t mtime);

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply);

#endif /* _GGP_IMAGE_H */
