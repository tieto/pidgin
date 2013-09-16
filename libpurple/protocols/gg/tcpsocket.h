#ifndef _GGP_TCPSOCKET_H
#define _GGP_TCPSOCKET_H

#include <internal.h>
#include <libgadu.h>

void
ggp_tcpsocket_setup(PurpleConnection *gc, struct gg_login_params *glp);

PurpleInputCondition
ggp_tcpsocket_inputcond_gg_to_purple(enum gg_check_t check);

#endif /* _GGP_TCPSOCKET_H */
