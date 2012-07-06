#include "libgadu-events.h"

#include <debug.h>

#include "avatar.h"

void ggp_events_user_data(PurpleConnection *gc, struct gg_event_user_data *data)
{
	int user_idx;
	gboolean is_update;
	
	purple_debug_info("gg", "GG_EVENT_USER_DATA [type=%d, user_count=%d]\n",
		data->type, data->user_count);
	
	/*
	type = 
		1, 3:	user information sent after connecting (divided by
			20 contacts; 3 - last one; 1 - rest of them)
		0: data update
	*/
	is_update = (data->type == 0);
	
	for (user_idx = 0; user_idx < data->user_count; user_idx++)
	{
		struct gg_event_user_data_user *data_user =
			&data->users[user_idx];
		uin_t uin = data_user->uin;
		int attr_idx;
		gboolean got_avatar = FALSE;
		for (attr_idx = 0; attr_idx < data_user->attr_count; attr_idx++)
		{
			struct gg_event_user_data_attr *data_attr =
				&data_user->attrs[attr_idx];
			if (strcmp(data_attr->key, "avatar") == 0)
			{
				time_t timestamp = atoi(data_attr->value);
				if (timestamp <= 0)
					continue;
				got_avatar = TRUE;
				ggp_avatar_buddy_update(gc, uin, timestamp);
			}
		}
		
		if (!is_update && !got_avatar)
			ggp_avatar_buddy_remove(gc, uin);
	}
}
