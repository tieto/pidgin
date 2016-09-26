/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#ifndef DBUS_API_SUBJECT_TO_CHANGE
#define DBUS_API_SUBJECT_TO_CHANGE
#endif

#include <stdio.h>
#include <stdlib.h>

#include "purple-client.h"

/*
   This example demonstrates how to use libpurple-client to communicate
   with purple.  The names and signatures of functions provided by
   libpurple-client are the same as those in purple.  However, all
   structures (such as PurpleAccount) are opaque, that is, you can only
   use pointer to them.  In fact, these pointers DO NOT actually point
   to anything, they are just integer identifiers of assigned to these
   structures by purple.  So NEVER try to dereference these pointers.
   Integer ids as disguised as pointers to provide type checking and
   prevent mistakes such as passing an id of PurpleAccount when an id of
   PurpleBuddy is expected.  According to glib manual, this technique is
   portable.
*/

int main (int argc, char **argv)
{
	GList *alist, *node;

	purple_init();

	alist = purple_accounts_get_all();
	for (node = alist; node != NULL; node = node->next)
	{
		PurpleAccount *account = (PurpleAccount*) node->data;
		char *name = purple_account_get_username(account);
		g_print("Name: %s\n", name);
		g_free(name);
	}
	g_list_free(alist);

	return 0;
}
