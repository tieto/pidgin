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
#include "debug.h"

typedef GaimAccount *Gaim__Account;
typedef GaimConnection *Gaim__Connection;
typedef GaimPlugin *Gaim__Plugin;
