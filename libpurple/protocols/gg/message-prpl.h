#ifndef _GGP_MESSAGE_PRPL_H
#define _GGP_MESSAGE_PRPL_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_message_session_data ggp_message_session_data;

void ggp_message_setup_global(void);
void ggp_message_cleanup_global(void);
void ggp_message_setup(PurpleConnection *gc);
void ggp_message_cleanup(PurpleConnection *gc);

void ggp_message_got(PurpleConnection *gc, const struct gg_event_msg *ev);
void ggp_message_got_multilogon(PurpleConnection *gc,
	const struct gg_event_msg *ev);

int ggp_message_send_im(PurpleConnection *gc, const char *who,
	const char *message, PurpleMessageFlags flags);
gchar * ggp_message_format_to_gg(PurpleConversation *conv, const gchar *text);

#endif /* _GGP_MESSAGE_PRPL_H */
