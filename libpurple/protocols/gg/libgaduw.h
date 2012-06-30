#ifndef _GGP_LIBGADUW_H
#define _GGP_LIBGADUW_H

#include <internal.h>
#include <libgadu.h>

typedef void (*ggp_libgaduw_http_cb)(struct gg_http *h, gboolean success, gpointer user_data);

typedef struct
{
	gpointer user_data;
	ggp_libgaduw_http_cb cb;
	
	struct gg_http *h;
	guint inpa;
} ggp_libgaduw_http_req;

ggp_libgaduw_http_req * ggp_libgaduw_http_watch(struct gg_http *h,
	ggp_libgaduw_http_cb cb, gpointer user_data);
void ggp_libgaduw_http_cancel(ggp_libgaduw_http_req *req);


#endif /* _GGP_LIBGADUW_H */
