#ifndef MSN_SBCONN_H
#define MSN_SBCONN_H

#include "msg.h"
#include "slplink.h"

#define MSN_SBCONN_MAX_SIZE 1202

void msn_sbconn_send_part(MsnSlpLink *slplink, MsnSlpMessagePart *part);

void msn_switchboard_send_msg(MsnSwitchBoard *swboard, MsnMessage *msg,
						 gboolean queue);

void
msn_sbconn_process_queue(MsnSwitchBoard *swboard);

#endif /* MSN_SBCONN_H */
