#ifndef _MSN_SLPLINK_H_
#define _MSN_SLPLINK_H_

typedef struct _MsnSlpLink MsnSlpLink;

#include "session.h"
#include "directconn.h"
#include "slpcall.h"
#include "slpmsg.h"

#include "ft.h"

struct _MsnSlpLink
{
	MsnSession *session;

	char *local_user;
	char *remote_user;

	int slp_seq_id;

	MsnDirectConn *directconn;

	GList *slp_calls;
	GList *slp_sessions;
	GList *slp_msgs;

	GQueue *slp_msg_queue;
};

MsnSlpLink *msn_slplink_new(MsnSession *session, const char *username);
void msn_slplink_destroy(MsnSlpLink *slplink);
MsnSlpLink *msn_session_find_slplink(MsnSession *session,
									   const char *who);
MsnSlpLink *msn_session_get_slplink(MsnSession *session, const char *username);
MsnSlpSession *msn_slplink_find_slp_session(MsnSlpLink *slplink,
											  long session_id);
MsnSlpCall *msn_slplink_find_slp_call(MsnSlpLink *slplink,
									   const char *id);
MsnSlpCall *msn_slplink_find_slp_call_with_session_id(MsnSlpLink *slplink, long id);
void msn_slplink_send_msg(MsnSlpLink *slplink, MsnMessage *msg);
void msn_slplink_release_msg(MsnSlpLink *slplink,
							  MsnSlpMessage *slpmsg);
void msn_slplink_queue_slpmsg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg);
void msn_slplink_send_slpmsg(MsnSlpLink *slplink,
							  MsnSlpMessage *slpmsg);
void msn_slplink_unleash(MsnSlpLink *slplink);
void msn_slplink_send_ack(MsnSlpLink *slplink, MsnMessage *msg);
void msn_slplink_process_msg(MsnSlpLink *slplink, MsnMessage *msg);
MsnSlpMessage *msn_slplink_message_find(MsnSlpLink *slplink, long id);
void msn_slplink_append_slp_msg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg);
void msn_slplink_remove_slp_msg(MsnSlpLink *slplink,
								 MsnSlpMessage *slpmsg);
void msn_slplink_request_ft(MsnSlpLink *slplink, GaimXfer *xfer);

void msn_slplink_request_object(MsnSlpLink *slplink,
								 const char *info,
								 MsnSlpCb cb,
								 const MsnObject *obj);

MsnSlpCall *msn_slp_process_msg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg);

#endif /* _MSN_SLPLINK_H_ */
