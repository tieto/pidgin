#ifndef _MSN_DIRECTCONN_H_
#define _MSN_DIRECTCONN_H_

typedef struct _MsnDirectConn MsnDirectConn;

#include "slplink.h"
#include "slp.h"
#include "msg.h"

struct _MsnDirectConn
{
	MsnSlpLink *slplink;
	MsnSlpCall *initial_call;

	gboolean acked;

	char *nonce;

	int fd;

	int port;
	int inpa;

	int c;
};

MsnDirectConn *msn_directconn_new(MsnSlpLink *slplink);
gboolean msn_directconn_connect(MsnDirectConn *directconn,
								const char *host, int port);
void msn_directconn_listen(MsnDirectConn *directconn);
void msn_directconn_send_msg(MsnDirectConn *directconn, MsnMessage *msg);
void msn_directconn_parse_nonce(MsnDirectConn *directconn, const char *nonce);
void msn_directconn_destroy(MsnDirectConn *directconn);
void msn_directconn_send_handshake(MsnDirectConn *directconn);

#endif /* _MSN_DIRECTCONN_H_ */
