#ifndef _GGP_CHAT_H
#define _GGP_CHAT_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_chat_session_data ggp_chat_session_data;

void ggp_chat_setup(PurpleConnection *gc);
void ggp_chat_cleanup(PurpleConnection *gc);

void ggp_chat_got_event(PurpleConnection *gc, const struct gg_event *ev);

GList * ggp_chat_info(PurpleConnection *gc);
char * ggp_chat_get_name(GHashTable *components);
void ggp_chat_join(PurpleConnection *gc, GHashTable *components);
void ggp_chat_leave(PurpleConnection *gc, int local_id);
void ggp_chat_invite(PurpleConnection *gc, int local_id, const char *message,
	const char *who);
int ggp_chat_send(PurpleConnection *gc, int local_id, const char *message,
	PurpleMessageFlags flags);

void ggp_chat_got_message(PurpleConnection *gc, uint64_t chat_id,
	const char *message, time_t time, uin_t who);

#endif /* _GGP_CHAT_H */
