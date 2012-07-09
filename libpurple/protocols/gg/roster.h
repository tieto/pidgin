#ifndef _GGP_ROSTER_H
#define _GGP_ROSTER_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	xmlnode *xml;
	int version;
} ggp_roster_session_data;

void ggp_roster_setup(PurpleConnection *gc);
void ggp_roster_cleanup(PurpleConnection *gc);

void ggp_roster_update(PurpleConnection *gc);
void ggp_roster_reply(PurpleConnection *gc, struct gg_event_userlist100_reply *reply);

#endif /* _GGP_ROSTER_H */
