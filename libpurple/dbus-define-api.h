#error "This is file is not a valid C code"

/* This file contains some of the macros from other header files as
   function declarations.  This does not make sense in C, but it
   provides type information for the dbus-analyze-functions.py
   program, which makes these macros callable by DBUS.  */

/* buddylist.h */
gboolean PURPLE_IS_CHAT(PurpleBListNode *node);
gboolean PURPLE_IS_BUDDY(PurpleBListNode *node);
gboolean PURPLE_IS_CONTACT(PurpleBListNode *node);
gboolean PURPLE_IS_GROUP(PurpleBListNode *node);
gboolean PURPLE_IS_BUDDY_ONLINE(PurpleBuddy *buddy);
gboolean PURPLE_BLIST_NODE_HAS_FLAG(PurpleBListNode *node, int flags);
gboolean PURPLE_BLIST_NODE_SHOULD_SAVE(PurpleBListNode *node);

/* connection.h */
gboolean PURPLE_CONNECTION_IS_CONNECTED(PurpleConnection *connection);
gboolean PURPLE_CONNECTION_IS_VALID(PurpleConnection *connection);

