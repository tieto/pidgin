#ifndef _GGP_ROSTER_H
#define _GGP_ROSTER_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	int version;
	
	guint timer;
	gpointer content;
	
	GList *sent_updates;
	GList *pending_updates;
	
	gboolean is_updating;
} ggp_roster_session_data;

// setup
gboolean ggp_roster_enabled(void);
void ggp_roster_setup(PurpleConnection *gc);
void ggp_roster_cleanup(PurpleConnection *gc);

// control
void ggp_roster_request_update(PurpleConnection *gc);

// libgadu callbacks
void ggp_roster_reply(PurpleConnection *gc, struct gg_event_userlist100_reply *reply);
void ggp_roster_version(PurpleConnection *gc, struct gg_event_userlist100_version *version);

// libpurple callbacks
void ggp_roster_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
void ggp_roster_group_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group);
void ggp_roster_rename_group(PurpleConnection *, const char *old_name, PurpleGroup *group, GList *moved_buddies);
void ggp_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group, const char *message);
void ggp_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);

#endif /* _GGP_ROSTER_H */
