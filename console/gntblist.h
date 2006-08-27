#ifndef _GNT_BLIST_H
#define _GNT_BLIST_H

#include "blist.h"

GaimBlistUiOps * gg_blist_get_ui_ops(void);

void gg_blist_init(void);

void gg_blist_uninit(void);

void gg_blist_show(void);

gboolean gg_blist_get_position(int *x, int *y);

void gg_blist_set_position(int x, int y);

gboolean gg_blist_get_size(int *width, int *height);

void gg_blist_set_size(int width, int height);

#endif
