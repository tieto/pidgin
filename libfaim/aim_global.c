/*
  aim_global.c

  These are things that are globally required, but don't fit the
  naming of the rest of the functions.  Namely, the queue ptrs and fds.

 */

#include "aim.h" 

/* the dreaded global variables... */

struct login_phase1_struct aim_logininfo;

/* queue (linked list) pointers */
struct command_tx_struct *aim_queue_outgoing = NULL; /* incoming commands */
struct command_rx_struct *aim_queue_incoming = NULL; /* outgoing commands */
