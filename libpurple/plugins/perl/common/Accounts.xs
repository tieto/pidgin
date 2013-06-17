#include "module.h"
#include "../perl-handlers.h"

MODULE = Purple::Accounts  PACKAGE = Purple::Accounts  PREFIX = purple_accounts_
PROTOTYPES: ENABLE

void
purple_accounts_add(account)
    Purple::Account account

void
purple_accounts_remove(account)
    Purple::Account account

void
purple_accounts_delete(account)
    Purple::Account account

void
purple_accounts_reorder(account, new_index)
    Purple::Account account
    size_t new_index

void
purple_accounts_get_all()
PREINIT:
    GList *l;
PPCODE:
    for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
        XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Account")));
    }

void
purple_accounts_get_all_active()
PREINIT:
    GList *list, *iter;
PPCODE:
    list = purple_accounts_get_all_active();
    for (iter = list; iter != NULL; iter = iter->next) {
        XPUSHs(sv_2mortal(purple_perl_bless_object(iter->data, "Purple::Account")));
    }
    g_list_free(list);

void
purple_accounts_restore_current_statuses()

Purple::Account
purple_accounts_find(name, protocol)
    const char * name
    const char * protocol

Purple::Handle
purple_accounts_get_handle()
