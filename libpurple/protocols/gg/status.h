#ifndef _GGP_STATUS_H
#define _GGP_STATUS_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_status_session_data ggp_status_session_data;

void ggp_status_setup(PurpleConnection *gc);
void ggp_status_cleanup(PurpleConnection *gc);

GList * ggp_status_types(PurpleAccount *account);
int ggp_status_from_purplestatus(PurpleStatus *status, gchar **message);
const gchar * ggp_status_to_purplestatus(int status);
const gchar * ggp_status_get_name(const gchar *purple_status);

// own status

void ggp_status_set_initial(PurpleConnection *gc, struct gg_login_params *glp);

gboolean ggp_status_set(PurpleAccount *account, int status, const gchar* msg);
void ggp_status_set_purplestatus(PurpleAccount *account, PurpleStatus *status);
void ggp_status_set_disconnected(PurpleAccount *account);
void ggp_status_fake_to_self(PurpleConnection *gc);

gboolean ggp_status_get_status_broadcasting(PurpleConnection *gc);
void ggp_status_set_status_broadcasting(PurpleConnection *gc,
	gboolean broadcasting);
void ggp_status_broadcasting_dialog(PurpleConnection *gc);

// buddy status

void ggp_status_got_others(PurpleConnection *gc, struct gg_event *ev);
char * ggp_status_buddy_text(PurpleBuddy *buddy);

#endif /* _GGP_STATUS_H */
