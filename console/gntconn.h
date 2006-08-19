#ifndef _GNT_CONN_H
#define _GNT_CONN_H

#include "connection.h"

GaimConnectionUiOps *gg_connections_get_ui_ops(void);

void gg_connections_init(void);

void gg_connections_uninit(void);

#endif
