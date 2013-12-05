#error "This is file is not a valid C code"

/* This file contains some of the macros from other header files as
   function declarations.  This does not make sense in C, but it
   provides type information for the dbus-analyze-functions.py
   program, which makes these macros callable by DBUS.  */

/* buddylist.h */
gboolean PURPLE_BUDDY_IS_ONLINE(PurpleBuddy *buddy);

/* connection.h */
gboolean PURPLE_CONNECTION_IS_CONNECTED(PurpleConnection *connection);
gboolean PURPLE_CONNECTION_IS_VALID(PurpleConnection *connection);

