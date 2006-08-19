#ifndef _GNT_DEBUG_H
#define _GNT_DEBUG_H

#include "debug.h"

GaimDebugUiOps *gg_debug_get_ui_ops(void);

void gg_debug_init(void);

void gg_debug_uninit(void);

void gg_debug_window_show(void);

#endif
