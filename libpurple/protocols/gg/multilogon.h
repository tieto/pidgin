#ifndef _GGP_MULTILOGON_H
#define _GGP_MULTILOGON_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_multilogon_session_data ggp_multilogon_session_data;

void ggp_multilogon_setup(PurpleConnection *gc);
void ggp_multilogon_cleanup(PurpleConnection *gc);

void ggp_multilogon_msg(PurpleConnection *gc, struct gg_event_msg *msg);
void ggp_multilogon_info(PurpleConnection *gc,
	struct gg_event_multilogon_info *msg);

int ggp_multilogon_get_session_count(PurpleConnection *gc);

#endif /* _GGP_MULTILOGON_H */
