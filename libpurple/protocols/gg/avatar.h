#ifndef _GGP_AVATAR_H
#define _GGP_AVATAR_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	guint timer;
	GList *pending_updates;
	
	gpointer current_update;
	gpointer own_data;
} ggp_avatar_session_data;

void ggp_avatar_setup(PurpleConnection *gc);
void ggp_avatar_cleanup(PurpleConnection *gc);

void ggp_avatar_buddy_update(PurpleConnection *gc, uin_t uin, time_t timestamp);
void ggp_avatar_buddy_remove(PurpleConnection *gc, uin_t uin);

void ggp_avatar_own_set(PurpleConnection *gc, PurpleStoredImage *img);

#endif /* _GGP_AVATAR_H */
