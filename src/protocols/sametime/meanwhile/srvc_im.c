
/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <glib.h>
#include <glib/glist.h>
#include <string.h>

#include "mw_channel.h"
#include "mw_debug.h"
#include "mw_error.h"
#include "mw_message.h"
#include "mw_service.h"
#include "mw_session.h"
#include "mw_srvc_im.h"
#include "mw_util.h"


#define PROTOCOL_TYPE  0x00001000
#define PROTOCOL_VER   0x00000003


/* data for the addtl blocks of channel creation */
#define mwImAddtlA_NORMAL  0x00000001

#define mwImAddtlB_NORMAL      0x00000001  /**< standard */
#define mwImAddtlB_PRECONF     0x00000019  /**< pre-conference chat */
#define mwImAddtlB_NOTESBUDDY  0x00033453  /**< notesbuddy */

#define mwImAddtlC_NORMAL  0x00000002


/* send-on-channel message type */
#define msg_MESSAGE  0x0064  /**< IM message */


/* which type of im? */
enum mwImType {
  mwIm_TEXT  = 0x00000001,  /**< text message */
  mwIm_DATA  = 0x00000002,  /**< status message (usually) */
};


/* which type of data im? */
enum mwImDataType {
  mwImData_TYPING   = 0x00000001,  /**< common use typing indicator */
  mwImData_SUBJECT  = 0x00000003,  /**< notesbuddy IM topic */
  mwImData_HTML     = 0x00000004,  /**< notesbuddy HTML message */
  mwImData_MIME     = 0x00000005,  /**< notesbuddy MIME message, w/image */

  mwImData_INVITE   = 0x0000000a,  /**< Places invitation */

  mwImData_MULTI_START  = 0x00001388,
  mwImData_MULTI_STOP   = 0x00001389,
};


/** @todo might be appropriate to make a couple of hashtables to
    reference conversations by channel and target */
struct mwServiceIm {
  struct mwService service;

  enum mwImClientType features;

  struct mwImHandler *handler;
  GList *convs;  /**< list of struct im_convo */
};


struct mwConversation {
  struct mwServiceIm *service;  /**< owning service */
  struct mwChannel *channel;    /**< channel */
  struct mwIdBlock target;      /**< conversation target */

  /** state of the conversation, based loosely on the state of its
      underlying channel */
  enum mwConversationState state;

  enum mwImClientType features;

  GString *multi;               /**< buffer for multi-chunk message */
  enum mwImSendType multi_type; /**< type of incoming multi-chunk message */

  struct mw_datum client_data;
};


/** momentarily places a mwLoginInfo into a mwIdBlock */
static void login_into_id(struct mwIdBlock *to, struct mwLoginInfo *from) {
  to->user = from->user_id;
  to->community = from->community;
}


static struct mwConversation *convo_find_by_user(struct mwServiceIm *srvc,
						 struct mwIdBlock *to) {
  GList *l;

  for(l = srvc->convs; l; l = l->next) {
    struct mwConversation *c = l->data;
    if(mwIdBlock_equal(&c->target, to))
       return c;
  }

  return NULL;
}


static const char *conv_state_str(enum mwConversationState state) {
  switch(state) {
  case mwConversation_CLOSED:
    return "closed";

  case mwConversation_OPEN:
    return "open";

  case mwConversation_PENDING:
    return "pending";

  case mwConversation_UNKNOWN:
  default:
    return "UNKNOWN";
  }
}


static void convo_set_state(struct mwConversation *conv,
			    enum mwConversationState state) {

  g_return_if_fail(conv != NULL);

  if(conv->state != state) {
    g_info("setting conversation (%s, %s) state: %s",
	   NSTR(conv->target.user), NSTR(conv->target.community),
	   conv_state_str(state));
    conv->state = state;
  }
}


struct mwConversation *mwServiceIm_findConversation(struct mwServiceIm *srvc,
						    struct mwIdBlock *to) {
  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(to != NULL, NULL);

  return convo_find_by_user(srvc, to);
}


struct mwConversation *mwServiceIm_getConversation(struct mwServiceIm *srvc,
						   struct mwIdBlock *to) {
  struct mwConversation *c;

  g_return_val_if_fail(srvc != NULL, NULL);
  g_return_val_if_fail(to != NULL, NULL);

  c = convo_find_by_user(srvc, to);
  if(! c) {
    c = g_new0(struct mwConversation, 1);
    c->service = srvc;
    mwIdBlock_clone(&c->target, to);
    c->state = mwConversation_CLOSED;
    c->features = srvc->features;

    srvc->convs = g_list_prepend(srvc->convs, c);
  }

  return c;
}


static void convo_create_chan(struct mwConversation *c) {
  struct mwSession *s;
  struct mwChannelSet *cs;
  struct mwChannel *chan;
  struct mwLoginInfo *login;
  struct mwPutBuffer *b;

  /* we only should be calling this if there isn't a channel already
     associated with the conversation */
  g_return_if_fail(c != NULL);
  g_return_if_fail(mwConversation_isPending(c));
  g_return_if_fail(c->channel == NULL);

  s = mwService_getSession(MW_SERVICE(c->service));
  cs = mwSession_getChannels(s);

  chan = mwChannel_newOutgoing(cs);
  mwChannel_setService(chan, MW_SERVICE(c->service));
  mwChannel_setProtoType(chan, PROTOCOL_TYPE);
  mwChannel_setProtoVer(chan, PROTOCOL_VER);

  /* offer all known ciphers */
  mwChannel_populateSupportedCipherInstances(chan);

  /* set the target */
  login = mwChannel_getUser(chan);
  login->user_id = g_strdup(c->target.user);
  login->community = g_strdup(c->target.community);

  /* compose the addtl create, with optional FANCY HTML! */
  b = mwPutBuffer_new();
  guint32_put(b, mwImAddtlA_NORMAL);
  guint32_put(b, c->features);
  mwPutBuffer_finalize(mwChannel_getAddtlCreate(chan), b);

  c->channel = mwChannel_create(chan)? NULL: chan;
  if(c->channel) {
    mwChannel_setServiceData(c->channel, c, NULL);
  }
}


void mwConversation_open(struct mwConversation *conv) {
  g_return_if_fail(conv != NULL);
  g_return_if_fail(mwConversation_isClosed(conv));

  convo_set_state(conv, mwConversation_PENDING);
  convo_create_chan(conv);
}


static void convo_opened(struct mwConversation *conv) {
  struct mwImHandler *h = NULL;

  g_return_if_fail(conv != NULL);
  g_return_if_fail(conv->service != NULL);

  convo_set_state(conv, mwConversation_OPEN);
  h = conv->service->handler;

  g_return_if_fail(h != NULL);

  if(h->conversation_opened)
    h->conversation_opened(conv);
}


static void convo_free(struct mwConversation *conv) {
  struct mwServiceIm *srvc;

  mwConversation_removeClientData(conv);

  srvc = conv->service;
  srvc->convs = g_list_remove(srvc->convs, conv);

  mwIdBlock_clear(&conv->target);
  g_free(conv);
}


static int send_accept(struct mwConversation *c) {

  struct mwChannel *chan = c->channel;
  struct mwSession *s = mwChannel_getSession(chan);
  struct mwUserStatus *stat = mwSession_getUserStatus(s);

  struct mwPutBuffer *b;
  struct mwOpaque *o;

  b = mwPutBuffer_new();
  guint32_put(b, mwImAddtlA_NORMAL);
  guint32_put(b, c->features);
  guint32_put(b, mwImAddtlC_NORMAL);
  mwUserStatus_put(b, stat);

  o = mwChannel_getAddtlAccept(chan);
  mwOpaque_clear(o);
  mwPutBuffer_finalize(o, b);

  return mwChannel_accept(chan);
}


static void recv_channelCreate(struct mwService *srvc,
			       struct mwChannel *chan,
			       struct mwMsgChannelCreate *msg) {

  /* - ensure it's the right service,proto,and proto ver
     - check the opaque for the right opaque junk
     - if not, close channel
     - compose & send a channel accept
  */

  struct mwServiceIm *srvc_im = (struct mwServiceIm *) srvc;
  struct mwImHandler *handler;
  struct mwSession *s;
  struct mwUserStatus *stat;
  guint32 x, y, z;
  struct mwGetBuffer *b;
  struct mwConversation *c = NULL;
  struct mwIdBlock idb;

  handler = srvc_im->handler;
  s = mwChannel_getSession(chan);
  stat = mwSession_getUserStatus(s);

  /* ensure the appropriate service/proto/ver */
  x = mwChannel_getServiceId(chan);
  y = mwChannel_getProtoType(chan);
  z = mwChannel_getProtoVer(chan);

  if( (x != SERVICE_IM) || (y != PROTOCOL_TYPE) || (z != PROTOCOL_VER) ) {
    g_warning("unacceptable service, proto, ver:"
	      " 0x%08x, 0x%08x, 0x%08x", x, y, z);
    mwChannel_destroy(chan, ERR_SERVICE_NO_SUPPORT, NULL);
    return;
  }

  /* act upon the values in the addtl block */
  b = mwGetBuffer_wrap(&msg->addtl);
  guint32_get(b, &x);
  guint32_get(b, &y);
  z = (guint) mwGetBuffer_error(b);
  mwGetBuffer_free(b);

  if(z /* buffer error, BOOM! */ ) {
    g_warning("bad/malformed addtl in IM service");
    mwChannel_destroy(chan, ERR_FAILURE, NULL);
    return;

  } else if(x != mwImAddtlA_NORMAL) {
    g_message("unknown params: 0x%08x, 0x%08x", x, y);
    mwChannel_destroy(chan, ERR_IM_NOT_REGISTERED, NULL);
    return;
    
  } else if(y == mwImAddtlB_PRECONF) {
    if(! handler->place_invite) {
      g_info("rejecting place-invite channel");
      mwChannel_destroy(chan, ERR_IM_NOT_REGISTERED, NULL);
      return;

    } else {
      g_info("accepting place-invite channel");
    }

  } else if(y != mwImClient_PLAIN && y != srvc_im->features) {
    /** reject what we do not understand */
    mwChannel_destroy(chan, ERR_IM_NOT_REGISTERED, NULL);
    return;

  } else if(stat->status == mwStatus_BUSY) {
    g_info("rejecting IM channel due to DND status");
    mwChannel_destroy(chan, ERR_CLIENT_USER_DND, NULL);
    return;
  }

  login_into_id(&idb, mwChannel_getUser(chan));

#if 0
  c = convo_find_by_user(srvc_im, &idb);
#endif

  if(! c) {
    c = g_new0(struct mwConversation, 1);
    c->service = srvc_im;
    srvc_im->convs = g_list_prepend(srvc_im->convs, c);
  }

#if 0
  /* we're going to re-associate any existing conversations with this
     new channel. That means closing any existing ones */
  if(c->channel) {
    g_info("closing existing IM channel 0x%08x", mwChannel_getId(c->channel));
    mwConversation_close(c, ERR_SUCCESS);
  }
#endif

  /* set up the conversation with this channel, target, and be fancy
     if the other side requested it */
  c->channel = chan;
  mwIdBlock_clone(&c->target, &idb);
  c->features = y;
  convo_set_state(c, mwConversation_PENDING);
  mwChannel_setServiceData(c->channel, c, NULL);

  if(send_accept(c)) {
    g_warning("sending IM channel accept failed");
    mwConversation_free(c);

  } else {
    convo_opened(c);
  }
}


static void recv_channelAccept(struct mwService *srvc, struct mwChannel *chan,
			       struct mwMsgChannelAccept *msg) {

  struct mwConversation *conv;

  conv = mwChannel_getServiceData(chan);
  if(! conv) {
    g_warning("received channel accept for non-existant conversation");
    mwChannel_destroy(chan, ERR_FAILURE, NULL);
    return;
  }

  convo_opened(conv);
}


static void recv_channelDestroy(struct mwService *srvc, struct mwChannel *chan,
				struct mwMsgChannelDestroy *msg) {

  struct mwConversation *c;

  c = mwChannel_getServiceData(chan);
  g_return_if_fail(c != NULL);

  c->channel = NULL;

  if(mwChannel_isState(chan, mwChannel_ERROR)) {

    /* checking for failure on the receiving end to accept html
       messages. Fail-over to a non-html format on a new channel for
       the convo */
    if(c->features != mwImClient_PLAIN
       && msg->reason == ERR_IM_NOT_REGISTERED) {

      g_debug("falling back on a plaintext conversation");
      c->features = mwImClient_PLAIN;
      convo_create_chan(c);
      return;
    }
  }

  mwConversation_close(c, msg->reason);
}


static void convo_recv(struct mwConversation *conv, enum mwImSendType type,
		       gconstpointer msg) {

  struct mwServiceIm *srvc;
  struct mwImHandler *handler;

  g_return_if_fail(conv != NULL);

  srvc = conv->service;
  g_return_if_fail(srvc != NULL);

  handler = srvc->handler;
  if(handler && handler->conversation_recv)
    handler->conversation_recv(conv, type, msg);
}


static void convo_multi_start(struct mwConversation *conv) {
  g_return_if_fail(conv->multi == NULL);
  conv->multi = g_string_new(NULL);
}


static void convo_multi_stop(struct mwConversation *conv) {

  g_return_if_fail(conv->multi != NULL);

  /* actually send it */
  convo_recv(conv, conv->multi_type, conv->multi->str);

  /* clear up the multi buffer */
  g_string_free(conv->multi, TRUE);
  conv->multi = NULL;
}


static void recv_text(struct mwServiceIm *srvc, struct mwChannel *chan,
		      struct mwGetBuffer *b) {

  struct mwConversation *c;
  char *text = NULL;

  mwString_get(b, &text);

  if(! text) return;

  c = mwChannel_getServiceData(chan);
  if(c) {
    if(c->multi) {
      g_string_append(c->multi, text);

    } else {
      convo_recv(c, mwImSend_PLAIN, text); 
    }
  }

  g_free(text);
}


static void convo_invite(struct mwConversation *conv,
			 struct mwOpaque *o) {

  struct mwServiceIm *srvc;
  struct mwImHandler *handler;

  struct mwGetBuffer *b;
  char *title, *name, *msg;

  g_info("convo_invite");

  srvc = conv->service;
  handler = srvc->handler;

  g_return_if_fail(handler != NULL);
  g_return_if_fail(handler->place_invite != NULL);

  b = mwGetBuffer_wrap(o);
  mwGetBuffer_advance(b, 4);
  mwString_get(b, &title);
  mwString_get(b, &msg);
  mwGetBuffer_advance(b, 19);
  mwString_get(b, &name);

  if(! mwGetBuffer_error(b)) {
    handler->place_invite(conv, msg, title, name);
  }

  mwGetBuffer_free(b);
  g_free(msg);
  g_free(title);
  g_free(name);
}


static void recv_data(struct mwServiceIm *srvc, struct mwChannel *chan,
		      struct mwGetBuffer *b) {

  struct mwConversation *conv;
  guint32 type, subtype;
  struct mwOpaque o = { 0, 0 };
  char *x;

  guint32_get(b, &type);
  guint32_get(b, &subtype);
  mwOpaque_get(b, &o);

  if(mwGetBuffer_error(b)) {
    mwOpaque_clear(&o);
    return;
  }

  conv = mwChannel_getServiceData(chan);
  if(! conv) return;

  switch(type) {
  case mwImData_TYPING:
    convo_recv(conv, mwImSend_TYPING, GINT_TO_POINTER(! subtype));
    break;

  case mwImData_HTML:
    if(o.len) {
      if(conv->multi) {
	g_string_append_len(conv->multi, o.data, o.len);
	conv->multi_type = mwImSend_HTML;

      } else {
	x = g_strndup(o.data, o.len);
	convo_recv(conv, mwImSend_HTML, x);
	g_free(x);
      }
    }
    break;

  case mwImData_SUBJECT:
    x = g_strndup(o.data, o.len);
    convo_recv(conv, mwImSend_SUBJECT, x);
    g_free(x);
    break;

  case mwImData_MIME:
    if(conv->multi) {
      g_string_append_len(conv->multi, o.data, o.len);
      conv->multi_type = mwImSend_MIME;

    } else {
      x = g_strndup(o.data, o.len);
      convo_recv(conv, mwImSend_MIME, x);
      g_free(x);
    }
    break;

  case mwImData_INVITE:
    convo_invite(conv, &o);
    break;

  case mwImData_MULTI_START:
    convo_multi_start(conv);
    break;

  case mwImData_MULTI_STOP:
    convo_multi_stop(conv);
    break;

  default:
    
    mw_debug_mailme(&o, "unknown data message type in IM service:"
		    " (0x%08x, 0x%08x)", type, subtype);
  }

  mwOpaque_clear(&o);
}


static void recv(struct mwService *srvc, struct mwChannel *chan,
		 guint16 type, struct mwOpaque *data) {

  /* - ensure message type is something we want
     - parse message type into either mwIMText or mwIMData
     - handle
  */

  struct mwGetBuffer *b;
  guint32 mt;

  g_return_if_fail(type == msg_MESSAGE);

  b = mwGetBuffer_wrap(data);
  guint32_get(b, &mt);

  if(mwGetBuffer_error(b)) {
    g_warning("failed to parse message for IM service");
    mwGetBuffer_free(b);
    return;
  }

  switch(mt) {
  case mwIm_TEXT:
    recv_text((struct mwServiceIm *) srvc, chan, b);
    break;

  case mwIm_DATA:
    recv_data((struct mwServiceIm *) srvc, chan, b);
    break;

  default:
    g_warning("unknown message type 0x%08x for IM service", mt);
  }

  if(mwGetBuffer_error(b))
    g_warning("failed to parse message type 0x%08x for IM service", mt);

  mwGetBuffer_free(b);
}


static void clear(struct mwServiceIm *srvc) {
  struct mwImHandler *h;

  while(srvc->convs)
    convo_free(srvc->convs->data);

  h = srvc->handler;
  if(h && h->clear)
    h->clear(srvc);
  srvc->handler = NULL;
}


static const char *name(struct mwService *srvc) {
  return "Instant Messaging";
}


static const char *desc(struct mwService *srvc) {
  return "IM service with Standard and NotesBuddy features";
}


static void start(struct mwService *srvc) {
  mwService_started(srvc);
}


static void stop(struct mwServiceIm *srvc) {

  while(srvc->convs)
    mwConversation_free(srvc->convs->data);

  mwService_stopped(MW_SERVICE(srvc));
}


struct mwServiceIm *mwServiceIm_new(struct mwSession *session,
				    struct mwImHandler *hndl) {

  struct mwServiceIm *srvc_im;
  struct mwService *srvc;

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(hndl != NULL, NULL);

  srvc_im = g_new0(struct mwServiceIm, 1);
  srvc = &srvc_im->service;

  mwService_init(srvc, session, SERVICE_IM);
  srvc->recv_create = recv_channelCreate;
  srvc->recv_accept = recv_channelAccept;
  srvc->recv_destroy = recv_channelDestroy;
  srvc->recv = recv;
  srvc->clear = (mwService_funcClear) clear;
  srvc->get_name = name;
  srvc->get_desc = desc;
  srvc->start = start;
  srvc->stop = (mwService_funcStop) stop;

  srvc_im->features = mwImClient_PLAIN;
  srvc_im->handler = hndl;

  return srvc_im;
}


struct mwImHandler *mwServiceIm_getHandler(struct mwServiceIm *srvc) {
  g_return_val_if_fail(srvc != NULL, NULL);
  return srvc->handler;
}


gboolean mwServiceIm_supports(struct mwServiceIm *srvc,
			      enum mwImSendType type) {

  g_return_val_if_fail(srvc != NULL, FALSE);

  switch(type) {
  case mwImSend_PLAIN:
  case mwImSend_TYPING:
    return TRUE;

  case mwImSend_SUBJECT:
  case mwImSend_HTML:
  case mwImSend_MIME:
    return srvc->features == mwImClient_NOTESBUDDY;

  default:
    return FALSE;
  }
}


void mwServiceIm_setClientType(struct mwServiceIm *srvc,
			       enum mwImClientType type) {

  g_return_if_fail(srvc != NULL);
  srvc->features = type;
}


enum mwImClientType mwServiceIm_getClientType(struct mwServiceIm *srvc) {
  g_return_val_if_fail(srvc != NULL, mwImClient_UNKNOWN);
  return srvc->features;
}


static int convo_sendText(struct mwConversation *conv, const char *text) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;
  
  b = mwPutBuffer_new();

  guint32_put(b, mwIm_TEXT);
  mwString_put(b, text);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conv->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static int convo_sendHtml(struct mwConversation *conv, const char *html) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;
  
  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_HTML);
  guint32_put(b, 0x00);

  /* use o first as a shell of an opaque for the text */
  o.len = strlen(html);
  o.data = (char *) html;
  mwOpaque_put(b, &o);

  /* use o again as the holder of the buffer's finalized data */
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conv->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static int convo_sendSubject(struct mwConversation *conv,
			     const char *subject) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_SUBJECT);
  guint32_put(b, 0x00);

  /* use o first as a shell of an opaque for the text */
  o.len = strlen(subject);
  o.data = (char *) subject;
  mwOpaque_put(b, &o);

  /* use o again as the holder of the buffer's finalized data */
  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conv->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static int convo_sendTyping(struct mwConversation *conv, gboolean typing) {
  struct mwPutBuffer *b;
  struct mwOpaque o = { 0, NULL };
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_TYPING);
  guint32_put(b, !typing);

  /* not to be confusing, but we're re-using o first as an empty
     opaque, and later as the contents of the finalized buffer */
  mwOpaque_put(b, &o);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conv->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


static int convo_sendMime(struct mwConversation *conv,
			  const char *mime) {
  struct mwPutBuffer *b;
  struct mwOpaque o;
  int ret;

  b = mwPutBuffer_new();

  guint32_put(b, mwIm_DATA);
  guint32_put(b, mwImData_MIME);
  guint32_put(b, 0x00);

  o.len = strlen(mime);
  o.data = (char *) mime;
  mwOpaque_put(b, &o);

  mwPutBuffer_finalize(&o, b);
  ret = mwChannel_send(conv->channel, msg_MESSAGE, &o);
  mwOpaque_clear(&o);

  return ret;
}


int mwConversation_send(struct mwConversation *conv, enum mwImSendType type,
			gconstpointer msg) {

  g_return_val_if_fail(conv != NULL, -1);
  g_return_val_if_fail(mwConversation_isOpen(conv), -1);
  g_return_val_if_fail(conv->channel != NULL, -1);

  switch(type) {
  case mwImSend_PLAIN:
    return convo_sendText(conv, msg);
  case mwImSend_TYPING:
    return convo_sendTyping(conv, GPOINTER_TO_INT(msg));
  case mwImSend_SUBJECT:
    return convo_sendSubject(conv, msg);
  case mwImSend_HTML:
    return convo_sendHtml(conv, msg);
  case mwImSend_MIME:
    return convo_sendMime(conv, msg);

  default:
    g_warning("unsupported IM Send Type, 0x%x", type);
    return -1;
  }
}


enum mwConversationState mwConversation_getState(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, mwConversation_UNKNOWN);
  return conv->state;
}


struct mwServiceIm *mwConversation_getService(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, NULL);
  return conv->service;
}


gboolean mwConversation_supports(struct mwConversation *conv,
				 enum mwImSendType type) {
  g_return_val_if_fail(conv != NULL, FALSE);

  switch(type) {
  case mwImSend_PLAIN:
  case mwImSend_TYPING:
    return TRUE;

  case mwImSend_SUBJECT:
  case mwImSend_HTML:
  case mwImSend_MIME:
    return conv->features == mwImClient_NOTESBUDDY;

  default:
    return FALSE;
  }
}


enum mwImClientType
mwConversation_getClientType(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, mwImClient_UNKNOWN);
  return conv->features;
}


struct mwIdBlock *mwConversation_getTarget(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, NULL);
  return &conv->target;
}


struct mwLoginInfo *mwConversation_getTargetInfo(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, NULL);
  g_return_val_if_fail(conv->channel != NULL, NULL);
  return mwChannel_getUser(conv->channel);
}


void mwConversation_setClientData(struct mwConversation *conv,
				  gpointer data, GDestroyNotify clean) {
  g_return_if_fail(conv != NULL);
  mw_datum_set(&conv->client_data, data, clean);
}


gpointer mwConversation_getClientData(struct mwConversation *conv) {
  g_return_val_if_fail(conv != NULL, NULL);
  return mw_datum_get(&conv->client_data);
}


void mwConversation_removeClientData(struct mwConversation *conv) {
  g_return_if_fail(conv != NULL);
  mw_datum_clear(&conv->client_data);
}


void mwConversation_close(struct mwConversation *conv, guint32 reason) {
  struct mwServiceIm *srvc;
  struct mwImHandler *h;

  g_return_if_fail(conv != NULL);

  convo_set_state(conv, mwConversation_CLOSED);

  srvc = conv->service;
  g_return_if_fail(srvc != NULL);

  h = srvc->handler;
  if(h && h->conversation_closed)
    h->conversation_closed(conv, reason);

  if(conv->channel) {
    mwChannel_destroy(conv->channel, reason, NULL);
    conv->channel = NULL;
  }
}


void mwConversation_free(struct mwConversation *conv) {
  g_return_if_fail(conv != NULL);

  if(! mwConversation_isClosed(conv))
    mwConversation_close(conv, ERR_SUCCESS);

  convo_free(conv);
}

