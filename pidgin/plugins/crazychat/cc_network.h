#ifndef __CC_NETWORK_H__
#define __CC_NETWORK_H__

#include "account.h"
#include "conversation.h"
#include "crazychat.h"

/* --- begin constant definition --- */

#define DEFAULT_CC_PORT		6543

#define CRAZYCHAT_INVITE_CODE	"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
				"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define CRAZYCHAT_ACCEPT_CODE	"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
				"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
				"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
				"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
				"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" \
				"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
#define CRAZYCHAT_READY_CODE	"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" \
				"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" \
				"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" \
				"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" \
				"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" \
				"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"

/* --- begin function declarations --- */

/**
 * Creates a new CrazyChat session if one doesn't exist and sends the invite.
 * @param cc		global crazychat data structure
 * @param name		the peer name
 * @param account	the purple account
 */
void cc_net_send_invite(struct crazychat *cc, char *name, PurpleAccount *account);

/**
 * Pops up the CrazyChat invitation accept window if a CrazyChat session does
 * not exist yet for this peer.
 * @param account	purple account
 * @param cc		global crazychat data structure
 * @param name		the peer name
 * @param peer_ip	the peer's ip address
 * @param peer_port	the peer's tcp port
 */
void cc_net_recv_invite(PurpleAccount *account, struct crazychat *cc, char *name,
		const char *peer_ip, const char *peer_port);

/**
 * Accepts the CrazyChat invitation and sends the response.
 * @param session	the CrazyChat session
 */
void cc_net_send_accept(struct cc_session *session);

/**
 * Receives a CrazyChat accept message, and if appropriate, creates a server
 * socket and sends the ready message.
 * @param account	the purple account which received the message
 * @param cc		global crazychat data structure
 * @param name		the peer name
 * @param peer_ip	the peer's ip address
 */
void cc_net_recv_accept(PurpleAccount *account, struct crazychat *cc, char *name,
		const char *peer_ip);

/**
 * Receives a CrazyChat ready message, and if appropriate, connects to peer
 * @param account	the purple account which received the message
 * @param cc		global crazychat data structure
 * @param name		the peer name
 */
void cc_net_recv_ready(PurpleAccount *account, struct crazychat *cc, char *name);

#endif
