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

static EContact * 
gevo_run_query_in_uri(const gchar *uri, EBookQuery *query)
{
	EBook *book;
	gboolean status;
	GList *cards;

	if (!gevo_load_addressbook(uri, &book, NULL))
	{
		gaim_debug_error("evolution",
						 "Error retrieving addressbook\n");
		return NULL;
	}

	status = e_book_get_contacts(book, query, &cards, NULL);
	if (!status)
	{
		gaim_debug_error("evolution", "Error %d in getting card list\n",
						 status);
		g_object_unref(book);
		return NULL;
	}
	g_object_unref(book);

	if (cards != NULL)
	{
		EContact *contact = E_CONTACT(cards->data);
		GList *cards2 = cards->next;

		if (cards2 != NULL)
		{
			/* Break off the first contact and free the rest. */
			cards->next = NULL;
			cards2->prev = NULL;
			g_list_foreach(cards2, (GFunc)g_object_unref, NULL);
		}

		/* Free the whole list. */
		g_list_free(cards);

		return contact;
	}

	return NULL;
}

/*
 * Search for a buddy in the Evolution contacts.
 *
 * @param buddy The buddy to search for.
 * @param query An optional query. This function takes ownership of @a query,
 *              so callers must e_book_query_ref() it in advance (to obtain a
 *              second reference) if they want to reuse @a query.
 */
EContact * 
gevo_search_buddy_in_contacts(GaimBuddy *buddy, EBookQuery *query)
{
	ESourceList *addressbooks;
	GError *err;
	EBookQuery *full_query;
	GSList *groups, *g;
	EContact *result;
	EContactField protocol_field = gevo_prpl_get_field(buddy->account, buddy);

	if (protocol_field == 0)
		return NULL;

	if (query != NULL)
	{
		EBookQuery *queries[2];

		queries[0] = query;
		queries[1] = e_book_query_field_test(protocol_field, E_BOOK_QUERY_IS, buddy->name);
		if (queries[1] == NULL)
		{
			gaim_debug_error("evolution", "Error in creating protocol query\n");
			e_book_query_unref(query);
			return NULL;
		}

		full_query = e_book_query_and(2, queries, TRUE);
	}
	else
	{
		full_query = e_book_query_field_test(protocol_field, E_BOOK_QUERY_IS, buddy->name);
		if (full_query == NULL)
		{
			gaim_debug_error("evolution", "Error in creating protocol query\n");
			return NULL;
		}
	}

	if (!e_book_get_addressbooks(&addressbooks, &err))
	{
		gaim_debug_error("evolution",
						 "Unable to fetch list of address books.\n");
		e_book_query_unref(full_query);
		if (err != NULL)
			g_error_free(err);
		return NULL;
	}

	groups = e_source_list_peek_groups(addressbooks);
	if (groups == NULL)
	{
		g_object_unref(addressbooks);
		e_book_query_unref(full_query);
		return NULL;
	}

	for (g = groups; g != NULL; g = g->next)
	{
		GSList *sources, *s;
		sources = e_source_group_peek_sources(g->data);
		for (s = sources; s != NULL; s = s->next)
		{
			result = gevo_run_query_in_uri(e_source_get_uri(E_SOURCE(s->data)), full_query);
			if (result != NULL) {
			    g_object_unref(addressbooks);
				e_book_query_unref(full_query);
			    return result;
			}
		}
	}

	g_object_unref(addressbooks);
	e_book_query_unref(full_query);
	return NULL;
}
