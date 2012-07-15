#ifndef _GGP_PURPLEW_H
#define _GGP_PURPLEW_H

#include <internal.h>
#include <libgadu.h>

#define GGP_PURPLEW_GROUP_DEFAULT _("Buddies")

/**
 * Adds an input handler in purple event loop for http request.
 *
 * @see purple_input_add
 *
 * @param http_req  Http connection to watch.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle (will be greater than 0).
 */
guint ggp_purplew_http_input_add(struct gg_http *http_req,
	PurpleInputFunction func, gpointer user_data);

typedef void (*ggp_purplew_request_processing_cancel_cb)(PurpleConnection *gc,
	void *user_data);

typedef struct
{
	PurpleConnection *gc;
	ggp_purplew_request_processing_cancel_cb cancel_cb;
	void *request_handle;
	void *user_data;
} ggp_purplew_request_processing_handle;

ggp_purplew_request_processing_handle * ggp_purplew_request_processing(
	PurpleConnection *gc, const gchar *msg, void *user_data,
	ggp_purplew_request_processing_cancel_cb oncancel);

void ggp_purplew_request_processing_done(
	ggp_purplew_request_processing_handle *handle);

// ignores default group
PurpleGroup * ggp_purplew_buddy_get_group_only(PurpleBuddy *buddy);

GList * ggp_purplew_group_get_buddies(PurpleGroup *group, PurpleAccount *account);

// you must g_free returned list
GList * ggp_purplew_account_get_groups(PurpleAccount *account, gboolean exclusive);

#endif /* _GGP_PURPLEW_H */
