#ifndef _GGP_SERVCONN_H
#define _GGP_SERVCONN_H

#include <internal.h>
#include <accountopt.h>

void ggp_servconn_setup(PurpleAccountOption *server_option);
void ggp_servconn_cleanup(void);

void ggp_servconn_add_server(const gchar *server);
GSList * ggp_servconn_get_servers(void);

#endif /* _GGP_SERVCONN_H */
