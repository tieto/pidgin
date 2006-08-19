#ifndef _GNT_ACCOUNT_H
#define _GNT_ACCOUNT_H

#include "account.h"

GaimAccountUiOps *gg_accounts_get_ui_ops(void);

void gg_accounts_init(void);

void gg_accounts_uninit(void);

void gg_accounts_show_all(void);

#endif
