#ifndef _GGP_MESSAGE_PRPL_H
#define _GGP_MESSAGE_PRPL_H

#include <internal.h>
#include <libgadu.h>

void ggp_message_got(PurpleConnection *gc, const struct gg_event_msg *ev);
void ggp_message_got_multilogon(PurpleConnection *gc,
	const struct gg_event_msg *ev);

#endif /* _GGP_MESSAGE_PRPL_H */
