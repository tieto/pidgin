#include "module.h"

MODULE = Gaim::Account  PACKAGE = Gaim::Account  PREFIX = gaim_account_
PROTOTYPES: ENABLE

void
gaim_account_disconnect(account)
	Gaim::Account account

void
gaim_account_set_username(account, username)
	Gaim::Account account
	const char *username

void
gaim_account_set_password(account, password)
	Gaim::Account account
	const char *password

void
gaim_account_set_alias(account, alias)
	Gaim::Account account
	const char *alias

void
gaim_account_set_user_info(account, user_info)
	Gaim::Account account
	const char *user_info
CODE:
	gaim_account_set_user_info(account, user_info);
	serv_set_info(gaim_account_get_connection(account), user_info);

void
gaim_account_set_buddy_icon(account, buddy_icon)
	Gaim::Account account
	const char *buddy_icon

void
gaim_account_set_protocol_id(account, protocol_id)
	Gaim::Account account
	const char *protocol_id

void
gaim_account_set_remember_password(account, value)
	Gaim::Account account
	gboolean value

void
gaim_account_set_check_mail(account, value)
	Gaim::Account account
	gboolean value

gboolean
gaim_account_is_connected(account)
	Gaim::Account account

const char *
gaim_account_get_username(account)
	Gaim::Account account

const char *
gaim_account_get_password(account)
	Gaim::Account account

const char *
gaim_account_get_alias(account)
	Gaim::Account account

const char *
gaim_account_get_user_info(account)
	Gaim::Account account

const char *
gaim_account_get_buddy_icon(account)
	Gaim::Account account

const char *
gaim_account_get_protocol_id(account)
	Gaim::Account account

Gaim::Connection
gaim_account_get_connection(account)
	Gaim::Account account

gboolean
gaim_account_get_remember_password(account)
	Gaim::Account account

gboolean
gaim_account_get_check_mail(account)
	Gaim::Account account


MODULE = Gaim::Account  PACKAGE = Gaim::Accounts  PREFIX = gaim_accounts_

void
gaim_accounts_add(account)
	Gaim::Account account

void
gaim_accounts_remove(account)
	Gaim::Account account

Gaim::Account
find(name, protocol_id)
	const char *name
	const char *protocol_id
CODE:
	RETVAL = gaim_accounts_find(name, protocol_id);
OUTPUT:
	RETVAL

void *
handle()
CODE:
	RETVAL = gaim_accounts_get_handle();
OUTPUT:
	RETVAL


MODULE = Gaim::Account  PACKAGE = Gaim

void
accounts()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Account")));

