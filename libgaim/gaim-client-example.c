#define DBUS_API_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>

#include "gaim-client.h"

/*
   This example demonstrates how to use libgaim-client to communicate
   with gaim.  The names and signatures of functions provided by
   libgaim-client are the same as those in gaim.  However, all
   structures (such as GaimAccount) are opaque, that is, you can only
   use pointer to them.  In fact, these pointers DO NOT actually point
   to anything, they are just integer identifiers of assigned to these
   structures by gaim.  So NEVER try to dereference these pointers.
   Integer ids as disguised as pointers to provide type checking and
   prevent mistakes such as passing an id of GaimAccount when an id of
   GaimBuddy is expected.  According to glib manual, this technique is
   portable.
*/

int main (int argc, char **argv)
{
	GList *alist, *node;

	gaim_init();

	alist = gaim_accounts_get_all();
	for (node = alist; node != NULL; node = node->next)
	{
		GaimAccount *account = (GaimAccount*) node->data;
		char *name = gaim_account_get_username(account);
		g_print("Name: %s\n", name);
		g_free(name);
	}
	g_list_free(alist);

	return 0;
}
