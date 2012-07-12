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
} ggp_roster_session_data;

void ggp_roster_setup(PurpleConnection *gc);
void ggp_roster_cleanup(PurpleConnection *gc);

void ggp_roster_update(PurpleConnection *gc);
void ggp_roster_reply(PurpleConnection *gc, struct gg_event_userlist100_reply *reply);
void ggp_roster_version(PurpleConnection *gc, struct gg_event_userlist100_version *version);

void ggp_roster_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
void ggp_roster_group_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group);

#endif /* _GGP_ROSTER_H */
