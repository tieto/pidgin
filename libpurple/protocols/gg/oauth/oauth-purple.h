#ifndef _GGP_OAUTH_PURPLE_H
#define _GGP_OAUTH_PURPLE_H

#include <internal.h>
#include <libgadu.h>

typedef void (*ggp_oauth_request_cb)(PurpleConnection *gc, const gchar *token,
	gpointer user_data);

void ggp_oauth_request(PurpleConnection *gc, ggp_oauth_request_cb callback,
	gpointer user_data, const gchar *sign_method, const gchar *sign_url);

#endif /* _GGP_OAUTH_PURPLE_H */
