
#ifndef _MW_CLIENT_H
#define _MW_CLIENT_H


#include <glib.h>
#include <glib/glist.h>


/* place-holders */
struct mwChannel;
struct mwClient;
struct mwClientHandler;
struct mwConnection;
struct mwConnectionHandler;
struct mwMessage;
struct mwService;
struct mwSession;



/* @file mw_client.h
   
*/



/* @section Client

*/
/*@{*/


struct mwClient;


typedef (struct mwConnectionHandler *)(*mwClientConnect)
     (struct mwClient *c, const char *host);


struct mwClientHandler {
  mwClientConnect connect;
}


struct mwClient *
mwClient_new(struct mwClientHandler *h);


struct mwChannel *
mwClient_newChannel(struct mwClient *client,
		    struct mwService *srvc);


struct mwChannel *
mwClient_newMasterChannel(struct mwClient *client,
			  struct mwSession *session);


int mwClient_sendKeepAlive(struct mwClient *client);


void mwClient_setUsesCountByte(struct mwClient *client,
			      gboolean use);


gboolean mwClient_getUsesCountByte(struct mwClient *client);


void mwClient_destroy(struct mwClient *client);


/*@}*/



/* @section Connection

*/
/*{*/


struct mwConnection;


typedef (int)(*mwConnectionWrite)
     (struct mwConnection *c, const char *buf, gsize len);


typedef (void)(*mwConnectionClose)
     (struct mwConnection *c);


struct mwConnectionHandler {
  mwConnectionWrite write;
  mwConnectionClose close;
}


struct mwConnection *
mwConnection_new(struct mwConnectionHandler *h);


void mwConnection_recv(struct mwConnection *connection,
		       const char *buf,
		       gsize len);


void mwConnection_destroy(struct mwConnection *connection);


/*@}*/



/* @section Channel

*/
/*@{*/


struct mwChannel;


int mwChannel_sendMessage(struct mwChannel *channel,
			  struct mwMessage *msg);


int mwChannel_send(struct mwChannel *channel,
		   guint32 type,
		   guint32 options,
		   struct mwOpaque *data);


int mwChannel_destroy(struct mwChannel *channel,
		      guint32 reason,
		      struct mwOpaque *info);


gboolean mwChannel_isMasterChannel(struct mwChannel *channel);


guint32 mwChannel_getId(struct mwChannel *channel);


enum mwChannelState mwChannel_getState(struct mwChannel *channel);



/*@}*/



#endif
