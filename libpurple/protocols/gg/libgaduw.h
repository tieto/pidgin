#ifndef _GGP_LIBGADUW_H
#define _GGP_LIBGADUW_H

#include <internal.h>
#include <libgadu.h>

#include "purplew.h"

typedef void (*ggp_libgaduw_http_cb)(struct gg_http *h, gboolean success,
	gboolean cancelled, gpointer user_data);

typedef struct
{
	gpointer user_data;
	ggp_libgaduw_http_cb cb;
	
	gboolean cancelled;
	struct gg_http *h;
	ggp_purplew_request_processing_handle *processing;
	guint inpa;
} ggp_libgaduw_http_req;

ggp_libgaduw_http_req * ggp_libgaduw_http_watch(PurpleConnection *gc,
	struct gg_http *h, ggp_libgaduw_http_cb cb, gpointer user_data,
	gboolean show_processing);
void ggp_libgaduw_http_cancel(ggp_libgaduw_http_req *req);


#endif /* _GGP_LIBGADUW_H */
