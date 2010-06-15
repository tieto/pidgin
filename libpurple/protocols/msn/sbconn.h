#ifndef MSN_SBCONN_H
#define MSN_SBCONN_H

#include "msg.h"

void msn_sbconn_msg_ack(MsnMessage *msg, void *data);

void msn_sbconn_msg_nak(MsnMessage *msg, void *data);
#endif /* MSN_SBCONN_H */
