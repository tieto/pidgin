#ifndef MSN_SBCONN_H
#define MSN_SBCONN_H

#include "msg.h"

#define MSN_SBCONN_MAX_SIZE 1202

void msn_sbconn_msg_ack(MsnMessage *msg, void *data);

void msn_sbconn_msg_nak(MsnMessage *msg, void *data);
#endif /* MSN_SBCONN_H */
