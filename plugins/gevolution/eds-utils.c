/*
 * Evolution integration plugin for Gaim
 *
 * Copyright (C) 2004 Henry Jen.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "internal.h"
#include "gtkblist.h"
#include "gtkgaim.h"
#include "gtkutils.h"
#include "gtkimhtml.h"
#include "gaim-disclosure.h"

#include "debug.h"
#include "gevolution.h"

GtkTreeModel *
gevo_addrbooks_model_new()
{
	return GTK_TREE_MODEL(gtk_list_store_new(NUM_ADDRBOOK_COLUMNS,
											 G_TYPE_STRING, G_TYPE_STRING));
}

void
gevo_addrbooks_model_unref(GtkTreeModel *model)
{
	GtkTreeIter iter;

	g_return_if_fail(model != NULL);
	g_return_if_fail(GTK_IS_LIST_STORE(model));

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	g_object_unref(model);
}

void
gevo_addrbooks_model_populate(GtkTreeModel *model)
{
	ESourceList *addressbooks;
	GError *err;
	GSList *groups, *g;
	GtkTreeIter iter;
	GtkListStore *list;

	g_return_if_fail(model != NULL);
	g_return_if_fail(GTK_IS_LIST_STORE(model));

	list = GTK_LIST_STORE(model);

	if (!e_book_get_addressbooks(&addressbooks, &err))
	{
		gaim_debug_error("evolution",
						 "Unable to fetch list of address books.\n");

		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter,
						   ADDRBOOK_COLUMN_NAME, _("None"),
						   ADDRBOOK_COLUMN_URI,  NULL,
						   -1);

		return;
	}

	groups = e_source_list_peek_groups(addressbooks);

	if (groups == NULL)
	{
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter,
						   ADDRBOOK_COLUMN_NAME, _("None"),
						   ADDRBOOK_COLUMN_URI,  NULL,
						   -1);

		return;
	}

	for (g = groups; g != NULL; g = g->next)
	{
		GSList *sources, *s;

		sources = e_source_group_peek_sources(g->data);

		for (s = sources; s != NULL; s = s->next)
		{
			ESource *source = E_SOURCE(s->data);

			g_object_ref(source);

			gtk_list_store_append(list, &iter);
			gtk_list_store_set(list, &iter,
							   ADDRBOOK_COLUMN_NAME, e_source_peek_name(source),
							   ADDRBOOK_COLUMN_URI,  e_source_get_uri(source),
							   -1);
		}
	}

	g_object_unref(addressbooks);
}
