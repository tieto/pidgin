#ifndef __CRAZYCHAT_H__
#define __CRAZYCHAT_H__

#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gtk/gtk.h>
#include "filter.h"
#include "gaim.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

struct crazychat;

/* --- type definitions --- */

typedef enum { INVITE = 0, ACCEPT, ACCEPTED, CONNECTED } CC_STATE;

/**
 * Finds the CrazyChat session with the handle.
 * @param cc		global crazychat data structure
 * @param handle	the peer name
 * @return		the cc_session if found, or NULL
 */
struct cc_session *cc_find_session(struct crazychat *cc, char *handle);

/**
 * Adds a new session with a peer, unless a peer session already exists.
 * Makes a deep copy of the handle.
 * @param cc		global crazychat data structure
 * @param handle	the peer name
 * @return		the new/old cc_session
 */
struct cc_session *cc_add_session(struct crazychat *cc, char *handle);

/**
 * Removes a crazychat session with a peer.
 * @param cc		global crazychat data structure
 * @param session	the cc_session to remove
 */
void cc_remove_session(struct crazychat *cc, struct cc_session *session);

#endif				/* __CRAZYCHAT_H__ */
