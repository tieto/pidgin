#error "This is file is not a valid C code"

/* This file contains some of the macros from other header files as
   function declarations.  This does not make sense in C, but it
   provides type information for the dbus-analyze-functions.py
   program, which makes these macros callable by DBUS.  */

/* blist.h */
gboolean GAIM_BLIST_NODE_IS_CHAT(GaimBlistNode *node);
gboolean GAIM_BLIST_NODE_IS_BUDDY(GaimBlistNode *node);
gboolean GAIM_BLIST_NODE_IS_CONTACT(GaimBlistNode *node);
gboolean GAIM_BLIST_NODE_IS_GROUP(GaimBlistNode *node);
gboolean GAIM_BUDDY_IS_ONLINE(GaimBuddy *buddy);
gboolean GAIM_BLIST_NODE_HAS_FLAG(GaimBlistNode *node, int flags);
gboolean GAIM_BLIST_NODE_SHOULD_SAVE(GaimBlistNode *node);

/* connection.h */
gboolean GAIM_CONNECTION_IS_CONNECTED(GaimConnection *connection);
gboolean GAIM_CONNECTION_IS_VALID(GaimConnection *connection);

/* conversation.h */
GaimConvIm *GAIM_CONV_IM(const GaimConversation *conversation);
GaimConvIm *GAIM_CONV_CHAT(const GaimConversation *conversation);


