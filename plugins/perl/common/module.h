typedef struct group *Gaim__Group;

#define group perl_group

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include <glib.h>

#undef group

#include "../perl-common.h"

#include "account.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "server.h"

typedef GaimAccount *     Gaim__Account;
typedef struct buddy *    Gaim__BuddyList__Buddy;
typedef struct chat *     Gaim__BuddyList__Chat;
typedef struct group *    Gaim__BuddyList__Group;
typedef GaimConnection *  Gaim__Connection;
typedef GaimConversation *Gaim__Conversation;
typedef GaimChat *        Gaim__Conversation__Chat;
typedef GaimIm *          Gaim__Conversation__IM;
typedef GaimWindow *      Gaim__ConvWindow;
typedef GaimPlugin *      Gaim__Plugin;

typedef GaimDebugLevel Gaim__DebugLevel;
