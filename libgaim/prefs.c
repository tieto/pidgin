/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include "internal.h"
#include "prefs.h"
#include "debug.h"
#include "util.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

struct pref_cb {
	GaimPrefCallback func;
	gpointer data;
	guint id;
	void *handle;
};

/* TODO: This should use GaimValues? */
struct gaim_pref {
	GaimPrefType type;
	char *name;
	union {
		gpointer generic;
		gboolean boolean;
		int integer;
		char *string;
		GList *stringlist;
	} value;
	GSList *callbacks;
	struct gaim_pref *parent;
	struct gaim_pref *sibling;
	struct gaim_pref *first_child;
};


static struct gaim_pref prefs = {
	GAIM_PREF_NONE,
	NULL,
	{ NULL },
	NULL,
	NULL,
	NULL,
	NULL
};

static GHashTable *prefs_hash = NULL;
static guint       save_timer = 0;
static gboolean    prefs_loaded = FALSE;


/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

static struct
gaim_pref *find_pref(const char *name)
{
	if (!name || name[0] != '/')
		return NULL;
	else if (name[1] == '\0')
		return &prefs;
	else
		return g_hash_table_lookup(prefs_hash, name);
}


/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

/*
 * This function recursively creates the xmlnode tree from the prefs
 * tree structure.  Yay recursion!
 */
static void
pref_to_xmlnode(xmlnode *parent, struct gaim_pref *pref)
{
	xmlnode *node, *childnode;
	struct gaim_pref *child;
	char buf[20];
	GList *cur;

	/* Create a new node */
	node = xmlnode_new_child(parent, "pref");
	xmlnode_set_attrib(node, "name", pref->name);

	/* Set the type of this node (if type == GAIM_PREF_NONE then do nothing) */
	if (pref->type == GAIM_PREF_INT) {
		xmlnode_set_attrib(node, "type", "int");
		snprintf(buf, sizeof(buf), "%d", pref->value.integer);
		xmlnode_set_attrib(node, "value", buf);
	}
	else if (pref->type == GAIM_PREF_STRING) {
		xmlnode_set_attrib(node, "type", "string");
		xmlnode_set_attrib(node, "value", pref->value.string ? pref->value.string : "");
	}
	else if (pref->type == GAIM_PREF_STRING_LIST) {
		xmlnode_set_attrib(node, "type", "stringlist");
		for (cur = pref->value.stringlist; cur != NULL; cur = cur->next)
		{
			childnode = xmlnode_new_child(node, "item");
			xmlnode_set_attrib(childnode, "value", cur->data ? cur->data : "");
		}
	}
	else if (pref->type == GAIM_PREF_BOOLEAN) {
		xmlnode_set_attrib(node, "type", "bool");
		snprintf(buf, sizeof(buf), "%d", pref->value.boolean);
		xmlnode_set_attrib(node, "value", buf);
	}

	/* All My Children */
	for (child = pref->first_child; child != NULL; child = child->sibling)
		pref_to_xmlnode(node, child);
}

static xmlnode *
prefs_to_xmlnode(void)
{
	xmlnode *node;
	struct gaim_pref *pref, *child;

	pref = &prefs;

	/* Create the root preference node */
	node = xmlnode_new("pref");
	xmlnode_set_attrib(node, "version", "1");
	xmlnode_set_attrib(node, "name", "/");

	/* All My Children */
	for (child = pref->first_child; child != NULL; child = child->sibling)
		pref_to_xmlnode(node, child);

	return node;
}

static void
sync_prefs(void)
{
	xmlnode *node;
	char *data;

	if (!prefs_loaded)
	{
		/*
		 * TODO: Call schedule_prefs_save()?  Ideally we wouldn't need to.
		 * (prefs.xml should be loaded when gaim_prefs_init is called)
		 */
		gaim_debug_error("prefs", "Attempted to save prefs before "
						 "they were read!\n");
		return;
	}

	node = prefs_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	gaim_util_write_data_to_file("prefs.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	sync_prefs();
	save_timer = 0;
	return FALSE;
}

static void
schedule_prefs_save(void)
{
	if (save_timer == 0)
		save_timer = gaim_timeout_add(5000, save_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static GList *prefs_stack = NULL;

static void
prefs_start_element_handler (GMarkupParseContext *context,
		const gchar *element_name,
		const gchar **attribute_names,
		const gchar **attribute_values,
		gpointer user_data,
		GError **error)
{
	GaimPrefType pref_type = GAIM_PREF_NONE;
	int i;
	const char *pref_name = NULL, *pref_value = NULL;
	GString *pref_name_full;
	GList *tmp;

	if(strcmp(element_name, "pref") && strcmp(element_name, "item"))
		return;

	for(i = 0; attribute_names[i]; i++) {
		if(!strcmp(attribute_names[i], "name")) {
			pref_name = attribute_values[i];
		} else if(!strcmp(attribute_names[i], "type")) {
			if(!strcmp(attribute_values[i], "bool"))
				pref_type = GAIM_PREF_BOOLEAN;
			else if(!strcmp(attribute_values[i], "int"))
				pref_type = GAIM_PREF_INT;
			else if(!strcmp(attribute_values[i], "string"))
				pref_type = GAIM_PREF_STRING;
			else if(!strcmp(attribute_values[i], "stringlist"))
				pref_type = GAIM_PREF_STRING_LIST;
			else
				return;
		} else if(!strcmp(attribute_names[i], "value")) {
			pref_value = attribute_values[i];
		}
	}

	if(!strcmp(element_name, "item")) {
		struct gaim_pref *pref;

		pref_name_full = g_string_new("");

		for(tmp = prefs_stack; tmp; tmp = tmp->next) {
			pref_name_full = g_string_prepend(pref_name_full, tmp->data);
			pref_name_full = g_string_prepend_c(pref_name_full, '/');
		}

		pref = find_pref(pref_name_full->str);

		if(pref) {
			pref->value.stringlist = g_list_append(pref->value.stringlist,
					g_strdup(pref_value));
		}
	} else {
		if(!pref_name || !strcmp(pref_name, "/"))
			return;

		pref_name_full = g_string_new(pref_name);

		for(tmp = prefs_stack; tmp; tmp = tmp->next) {
			pref_name_full = g_string_prepend_c(pref_name_full, '/');
			pref_name_full = g_string_prepend(pref_name_full, tmp->data);
		}

		pref_name_full = g_string_prepend_c(pref_name_full, '/');

		switch(pref_type) {
			case GAIM_PREF_NONE:
				gaim_prefs_add_none(pref_name_full->str);
				break;
			case GAIM_PREF_BOOLEAN:
				gaim_prefs_set_bool(pref_name_full->str, atoi(pref_value));
				break;
			case GAIM_PREF_INT:
				gaim_prefs_set_int(pref_name_full->str, atoi(pref_value));
				break;
			case GAIM_PREF_STRING:
				gaim_prefs_set_string(pref_name_full->str, pref_value);
				break;
			case GAIM_PREF_STRING_LIST:
				gaim_prefs_set_string_list(pref_name_full->str, NULL);
				break;
		}
		prefs_stack = g_list_prepend(prefs_stack, g_strdup(pref_name));
		g_string_free(pref_name_full, TRUE);
	}
}

static void
prefs_end_element_handler(GMarkupParseContext *context,
						  const gchar *element_name,
						  gpointer user_data, GError **error)
{
	if(prefs_stack && !strcmp(element_name, "pref")) {
		g_free(prefs_stack->data);
		prefs_stack = g_list_delete_link(prefs_stack, prefs_stack);
	}
}

static GMarkupParser prefs_parser = {
	prefs_start_element_handler,
	prefs_end_element_handler,
	NULL,
	NULL,
	NULL
};

gboolean
gaim_prefs_load()
{
	gchar *filename = g_build_filename(gaim_user_dir(), "prefs.xml", NULL);
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;

	if (!filename) {
		prefs_loaded = TRUE;
		return FALSE;
	}

	gaim_debug_info("prefs", "Reading %s\n", filename);

	if(!g_file_get_contents(filename, &contents, &length, &error)) {
#ifndef _WIN32
		g_free(filename);
		g_error_free(error);

		error = NULL;

		filename = g_build_filename(SYSCONFDIR, "gaim", "prefs.xml", NULL);

		gaim_debug_info("prefs", "Reading %s\n", filename);

		if (!g_file_get_contents(filename, &contents, &length, &error)) {
			gaim_debug_error("prefs", "Error reading prefs: %s\n",
					error->message);
			g_error_free(error);
			g_free(filename);
			prefs_loaded = TRUE;

			return FALSE;
		}
#else /* _WIN32 */
		gaim_debug_error("prefs", "Error reading prefs: %s\n",
				error->message);
		g_error_free(error);
		g_free(filename);
		prefs_loaded = TRUE;

		return FALSE;
#endif /* _WIN32 */
	}

	context = g_markup_parse_context_new(&prefs_parser, 0, NULL, NULL);

	if(!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		prefs_loaded = TRUE;

		return FALSE;
	}

	if(!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug_error("prefs", "Error parsing %s\n", filename);
		g_markup_parse_context_free(context);
		g_free(contents);
		g_free(filename);
		prefs_loaded = TRUE;

		return FALSE;
	}

	gaim_debug_info("prefs", "Finished reading %s\n", filename);
	g_markup_parse_context_free(context);
	g_free(contents);
	g_free(filename);
	prefs_loaded = TRUE;

	/* I introduced a bug in 2.0.0beta2.  This fixes the broken
	 * scores on upgrade.  This can be removed sometime shortly
	 * after 2.0.0 final is released. -- rlaager */
	if (gaim_prefs_get_int("/core/status/scores/offline") == -500 &&
	    gaim_prefs_get_int("/core/status/scores/available") == 100 &&
	    gaim_prefs_get_int("/core/status/scores/invisible") == -50 &&
	    gaim_prefs_get_int("/core/status/scores/away") == -100 &&
	    gaim_prefs_get_int("/core/status/scores/extended_away") == -200 &&
	    gaim_prefs_get_int("/core/status/scores/idle") == -400)
	{
		gaim_prefs_set_int("/core/status/scores/idle", -10);
	}

	return TRUE;
}



static void
prefs_save_cb(const char *name, GaimPrefType type, gconstpointer val,
			  gpointer user_data)
{

	if(!prefs_loaded)
		return;

	gaim_debug_misc("prefs", "%s changed, scheduling save.\n", name);

	schedule_prefs_save();
}

static char *
get_path_dirname(const char *name)
{
	char *c, *str;

	str = g_strdup(name);

	if ((c = strrchr(str, '/')) != NULL) {
		*c = '\0';

		if (*str == '\0') {
			g_free(str);

			str = g_strdup("/");
		}
	}
	else {
		g_free(str);

		str = g_strdup(".");
	}

	return str;
}

static char *
get_path_basename(const char *name)
{
	const char *c;

	if ((c = strrchr(name, '/')) != NULL)
		return g_strdup(c + 1);

	return g_strdup(name);
}

static char *
pref_full_name(struct gaim_pref *pref)
{
	GString *name;
	struct gaim_pref *parent;

	if(!pref)
		return NULL;

	if(pref == &prefs)
		return g_strdup("/");

	name = g_string_new(pref->name);
	parent = pref->parent;

	for(parent = pref->parent; parent && parent->name; parent = parent->parent) {
		name = g_string_prepend_c(name, '/');
		name = g_string_prepend(name, parent->name);
	}
	name = g_string_prepend_c(name, '/');
	return g_string_free(name, FALSE);
}

static struct gaim_pref *
find_pref_parent(const char *name)
{
	char *parent_name = get_path_dirname(name);
	struct gaim_pref *ret = &prefs;

	if(strcmp(parent_name, "/")) {
		ret = find_pref(parent_name);
	}

	g_free(parent_name);
	return ret;
}

static void
free_pref_value(struct gaim_pref *pref)
{
	switch(pref->type) {
		case GAIM_PREF_BOOLEAN:
			pref->value.boolean = FALSE;
			break;
		case GAIM_PREF_INT:
			pref->value.integer = 0;
			break;
		case GAIM_PREF_STRING:
			g_free(pref->value.string);
			pref->value.string = NULL;
			break;
		case GAIM_PREF_STRING_LIST:
			{
				g_list_foreach(pref->value.stringlist, (GFunc)g_free, NULL);
				g_list_free(pref->value.stringlist);
			} break;
		case GAIM_PREF_NONE:
			break;
	}
}

static struct gaim_pref *
add_pref(GaimPrefType type, const char *name)
{
	struct gaim_pref *parent;
	struct gaim_pref *me;
	struct gaim_pref *sibling;
	char *my_name;

	parent = find_pref_parent(name);

	if(!parent)
		return NULL;

	my_name = get_path_basename(name);

	for(sibling = parent->first_child; sibling; sibling = sibling->sibling) {
		if(!strcmp(sibling->name, my_name)) {
			g_free(my_name);
			return NULL;
		}
	}

	me = g_new0(struct gaim_pref, 1);
	me->type = type;
	me->name = my_name;

	me->parent = parent;
	if(parent->first_child) {
		/* blatant abuse of a for loop */
		for(sibling = parent->first_child; sibling->sibling;
				sibling = sibling->sibling);
		sibling->sibling = me;
	} else {
		parent->first_child = me;
	}

	g_hash_table_insert(prefs_hash, g_strdup(name), (gpointer)me);

	return me;
}

void
gaim_prefs_add_none(const char *name)
{
	add_pref(GAIM_PREF_NONE, name);
}

void
gaim_prefs_add_bool(const char *name, gboolean value)
{
	struct gaim_pref *pref = add_pref(GAIM_PREF_BOOLEAN, name);

	if(!pref)
		return;

	pref->value.boolean = value;
}

void
gaim_prefs_add_int(const char *name, int value)
{
	struct gaim_pref *pref = add_pref(GAIM_PREF_INT, name);

	if(!pref)
		return;

	pref->value.integer = value;
}

void
gaim_prefs_add_string(const char *name, const char *value)
{
	struct gaim_pref *pref = add_pref(GAIM_PREF_STRING, name);

	if(!pref)
		return;

	pref->value.string = g_strdup(value);
}

void
gaim_prefs_add_string_list(const char *name, GList *value)
{
	struct gaim_pref *pref = add_pref(GAIM_PREF_STRING_LIST, name);
	GList *tmp;

	if(!pref)
		return;

	for(tmp = value; tmp; tmp = tmp->next)
		pref->value.stringlist = g_list_append(pref->value.stringlist,
				g_strdup(tmp->data));
}

static void
remove_pref(struct gaim_pref *pref)
{
	char *name;
	GSList *l;

	if(!pref || pref == &prefs)
		return;

	while(pref->first_child)
		remove_pref(pref->first_child);

	if(pref->parent->first_child == pref) {
		pref->parent->first_child = pref->sibling;
	} else {
		struct gaim_pref *sib = pref->parent->first_child;
		while(sib && sib->sibling != pref)
			sib = sib->sibling;
		if(sib)
			sib->sibling = pref->sibling;
	}

	name = pref_full_name(pref);

	gaim_debug_info("prefs", "removing pref %s\n", name);

	g_hash_table_remove(prefs_hash, name);
	g_free(name);

	free_pref_value(pref);

	while((l = pref->callbacks) != NULL) {
		pref->callbacks = pref->callbacks->next;
		g_free(l->data);
		g_slist_free_1(l);
	}
	g_free(pref->name);
	g_free(pref);
}

void
gaim_prefs_remove(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref)
		return;

	remove_pref(pref);
}

void
gaim_prefs_destroy()
{
	gaim_prefs_remove("/");
}

static void
do_callbacks(const char* name, struct gaim_pref *pref)
{
	GSList *cbs;
	struct gaim_pref *cb_pref;
	for(cb_pref = pref; cb_pref; cb_pref = cb_pref->parent) {
		for(cbs = cb_pref->callbacks; cbs; cbs = cbs->next) {
			struct pref_cb *cb = cbs->data;
			cb->func(name, pref->type, pref->value.generic, cb->data);
		}
	}
}

void
gaim_prefs_trigger_callback(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_trigger_callback: Unknown pref %s\n", name);
		return;
	}

	do_callbacks(name, pref);
}

void
gaim_prefs_set_generic(const char *name, gpointer value)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_set_generic: Unknown pref %s\n", name);
		return;
	}

	pref->value.generic = value;
	do_callbacks(name, pref);
}

void
gaim_prefs_set_bool(const char *name, gboolean value)
{
	struct gaim_pref *pref = find_pref(name);

	if(pref) {
		if(pref->type != GAIM_PREF_BOOLEAN) {
			gaim_debug_error("prefs",
					"gaim_prefs_set_bool: %s not a boolean pref\n", name);
			return;
		}

		if(pref->value.boolean != value) {
			pref->value.boolean = value;
			do_callbacks(name, pref);
		}
	} else {
		gaim_prefs_add_bool(name, value);
	}
}

void
gaim_prefs_set_int(const char *name, int value)
{
	struct gaim_pref *pref = find_pref(name);

	if(pref) {
		if(pref->type != GAIM_PREF_INT) {
			gaim_debug_error("prefs",
					"gaim_prefs_set_int: %s not an integer pref\n", name);
			return;
		}

		if(pref->value.integer != value) {
			pref->value.integer = value;
			do_callbacks(name, pref);
		}
	} else {
		gaim_prefs_add_int(name, value);
	}
}

void
gaim_prefs_set_string(const char *name, const char *value)
{
	struct gaim_pref *pref = find_pref(name);

	if(pref) {
		if(pref->type != GAIM_PREF_STRING) {
			gaim_debug_error("prefs",
					"gaim_prefs_set_string: %s not a string pref\n", name);
			return;
		}

		if((value && !pref->value.string) ||
				(!value && pref->value.string) ||
				(value && pref->value.string &&
				 strcmp(pref->value.string, value))) {
			g_free(pref->value.string);
			pref->value.string = g_strdup(value);
			do_callbacks(name, pref);
		}
	} else {
		gaim_prefs_add_string(name, value);
	}
}

void
gaim_prefs_set_string_list(const char *name, GList *value)
{
	struct gaim_pref *pref = find_pref(name);
	if(pref) {
		GList *tmp;

		if(pref->type != GAIM_PREF_STRING_LIST) {
			gaim_debug_error("prefs",
					"gaim_prefs_set_string_list: %s not a string list pref\n",
					name);
			return;
		}

		g_list_foreach(pref->value.stringlist, (GFunc)g_free, NULL);
		g_list_free(pref->value.stringlist);
		pref->value.stringlist = NULL;

		for(tmp = value; tmp; tmp = tmp->next)
			pref->value.stringlist = g_list_prepend(pref->value.stringlist,
					g_strdup(tmp->data));
		pref->value.stringlist = g_list_reverse(pref->value.stringlist);

		do_callbacks(name, pref);

	} else {
		gaim_prefs_add_string_list(name, value);
	}
}

gboolean
gaim_prefs_exists(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if (pref != NULL)
		return TRUE;

	return FALSE;
}

GaimPrefType
gaim_prefs_get_type(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if (pref == NULL)
		return GAIM_PREF_NONE;

	return (pref->type);
}

gboolean
gaim_prefs_get_bool(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_bool: Unknown pref %s\n", name);
		return FALSE;
	} else if(pref->type != GAIM_PREF_BOOLEAN) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_bool: %s not a boolean pref\n", name);
		return FALSE;
	}

	return pref->value.boolean;
}

int
gaim_prefs_get_int(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_int: Unknown pref %s\n", name);
		return 0;
	} else if(pref->type != GAIM_PREF_INT) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_int: %s not an integer pref\n", name);
		return 0;
	}

	return pref->value.integer;
}

const char *
gaim_prefs_get_string(const char *name)
{
	struct gaim_pref *pref = find_pref(name);

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_string: Unknown pref %s\n", name);
		return NULL;
	} else if(pref->type != GAIM_PREF_STRING) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_string: %s not a string pref\n", name);
		return NULL;
	}

	return pref->value.string;
}

GList *
gaim_prefs_get_string_list(const char *name)
{
	struct gaim_pref *pref = find_pref(name);
	GList *ret = NULL, *tmp;

	if(!pref) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_string_list: Unknown pref %s\n", name);
		return NULL;
	} else if(pref->type != GAIM_PREF_STRING_LIST) {
		gaim_debug_error("prefs",
				"gaim_prefs_get_string_list: %s not a string list pref\n", name);
		return NULL;
	}

	for(tmp = pref->value.stringlist; tmp; tmp = tmp->next)
		ret = g_list_prepend(ret, g_strdup(tmp->data));
	ret = g_list_reverse(ret);

	return ret;
}

void
gaim_prefs_rename(const char *oldname, const char *newname)
{
	struct gaim_pref *oldpref, *newpref;

	oldpref = find_pref(oldname);

	/* it's already been renamed, call off the dogs */
	if(!oldpref)
		return;

	if (oldpref->first_child != NULL) /* can't rename parents */
	{
		gaim_debug_error("prefs", "Unable to rename %s to %s: can't rename parents\n", oldname, newname);
		return;
	}


	newpref = find_pref(newname);

	if (newpref == NULL)
	{
		gaim_debug_error("prefs", "Unable to rename %s to %s: new pref not created\n", oldname, newname);
		return;
	}

	if (oldpref->type != newpref->type)
	{
		gaim_debug_error("prefs", "Unable to rename %s to %s: differing types\n", oldname, newname);
		return;
	}

	gaim_debug_info("prefs", "Renaming %s to %s\n", oldname, newname);

	switch(oldpref->type) {
		case GAIM_PREF_NONE:
			break;
		case GAIM_PREF_BOOLEAN:
			gaim_prefs_set_bool(newname, oldpref->value.boolean);
			break;
		case GAIM_PREF_INT:
			gaim_prefs_set_int(newname, oldpref->value.integer);
			break;
		case GAIM_PREF_STRING:
			gaim_prefs_set_string(newname, oldpref->value.string);
			break;
		case GAIM_PREF_STRING_LIST:
			gaim_prefs_set_string_list(newname, oldpref->value.stringlist);
			break;
	}

	remove_pref(oldpref);
}

void
gaim_prefs_rename_boolean_toggle(const char *oldname, const char *newname)
{
		struct gaim_pref *oldpref, *newpref;

		oldpref = find_pref(oldname);

		/* it's already been renamed, call off the cats */
		if(!oldpref)
			return;

		if (oldpref->type != GAIM_PREF_BOOLEAN)
		{
			gaim_debug_error("prefs", "Unable to rename %s to %s: old pref not a boolean\n", oldname, newname);
			return;
		}

		if (oldpref->first_child != NULL) /* can't rename parents */
		{
			gaim_debug_error("prefs", "Unable to rename %s to %s: can't rename parents\n", oldname, newname);
			return;
		}


		newpref = find_pref(newname);

		if (newpref == NULL)
		{
			gaim_debug_error("prefs", "Unable to rename %s to %s: new pref not created\n", oldname, newname);
			return;
		}

		if (oldpref->type != newpref->type)
		{
			gaim_debug_error("prefs", "Unable to rename %s to %s: differing types\n", oldname, newname);
			return;
		}

		gaim_debug_info("prefs", "Renaming and toggling %s to %s\n", oldname, newname);
		gaim_prefs_set_bool(newname, !(oldpref->value.boolean));

		remove_pref(oldpref);
}

guint
gaim_prefs_connect_callback(void *handle, const char *name, GaimPrefCallback func, gpointer data)
{
	struct gaim_pref *pref;
	struct pref_cb *cb;
	static guint cb_id = 0;

	g_return_val_if_fail(name != NULL, 0);
	g_return_val_if_fail(func != NULL, 0);

	pref = find_pref(name);
	if (pref == NULL)
		return 0;

	cb = g_new0(struct pref_cb, 1);

	cb->func = func;
	cb->data = data;
	cb->id = ++cb_id;
	cb->handle = handle;

	pref->callbacks = g_slist_append(pref->callbacks, cb);

	return cb->id;
}

static gboolean
disco_callback_helper(struct gaim_pref *pref, guint callback_id)
{
	GSList *cbs;
	struct gaim_pref *child;

	if(!pref)
		return FALSE;

	for(cbs = pref->callbacks; cbs; cbs = cbs->next) {
		struct pref_cb *cb = cbs->data;
		if(cb->id == callback_id) {
			pref->callbacks = g_slist_delete_link(pref->callbacks, cbs);
			g_free(cb);
			return TRUE;
		}
	}

	for(child = pref->first_child; child; child = child->sibling) {
		if(disco_callback_helper(child, callback_id))
			return TRUE;
	}

	return FALSE;
}

void
gaim_prefs_disconnect_callback(guint callback_id)
{
	disco_callback_helper(&prefs, callback_id);
}

static void
disco_callback_helper_handle(struct gaim_pref *pref, void *handle)
{
	GSList *cbs;
	struct gaim_pref *child;

	if(!pref)
		return;

	cbs = pref->callbacks;
	while (cbs != NULL) {
		struct pref_cb *cb = cbs->data;
		if(cb->handle == handle) {
			pref->callbacks = g_slist_delete_link(pref->callbacks, cbs);
			g_free(cb);
			cbs = pref->callbacks;
		} else
			cbs = cbs->next;
	}

	for(child = pref->first_child; child; child = child->sibling)
		disco_callback_helper_handle(child, handle);
}

void
gaim_prefs_disconnect_by_handle(void *handle)
{
	g_return_if_fail(handle != NULL);

	disco_callback_helper_handle(&prefs, handle);
}

void
gaim_prefs_update_old()
{
	/* Remove some no-longer-used prefs */
	gaim_prefs_remove("/core/away/auto_response/enabled");
	gaim_prefs_remove("/core/away/auto_response/idle_only");
	gaim_prefs_remove("/core/away/auto_response/in_active_conv");
	gaim_prefs_remove("/core/away/auto_response/sec_before_resend");
	gaim_prefs_remove("/core/away/auto_response");
	gaim_prefs_remove("/core/away/default_message");
	gaim_prefs_remove("/core/buddies/use_server_alias");
	gaim_prefs_remove("/core/conversations/away_back_on_send");
	gaim_prefs_remove("/core/conversations/send_urls_as_links");
	gaim_prefs_remove("/core/conversations/im/show_login");
	gaim_prefs_remove("/core/conversations/chat/show_join");
	gaim_prefs_remove("/core/conversations/chat/show_leave");
	gaim_prefs_remove("/core/conversations/combine_chat_im");
	gaim_prefs_remove("/core/conversations/use_alias_for_title");
	gaim_prefs_remove("/core/logging/log_signon_signoff");
	gaim_prefs_remove("/core/logging/log_idle_state");
	gaim_prefs_remove("/core/logging/log_away_state");
	gaim_prefs_remove("/core/logging/log_own_states");
	gaim_prefs_remove("/core/status/scores/hidden");
	gaim_prefs_remove("/plugins/core/autorecon/hide_connected_error");
	gaim_prefs_remove("/plugins/core/autorecon/hide_connecting_error");
	gaim_prefs_remove("/plugins/core/autorecon/hide_reconnecting_dialog");
	gaim_prefs_remove("/plugins/core/autorecon/restore_state");
	gaim_prefs_remove("/plugins/core/autorecon");

	/* Convert old sounds while_away pref to new 3-way pref. */
	if (gaim_prefs_exists("/core/sound/while_away") &&
	    gaim_prefs_get_bool("/core/sound/while_away"))
	{
		gaim_prefs_set_int("/core/sound/while_status", 3);
	}
	gaim_prefs_remove("/core/sound/while_away");
}

void *
gaim_prefs_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_prefs_init(void)
{
	void *handle = gaim_prefs_get_handle();

	prefs_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	gaim_prefs_connect_callback(handle, "/", prefs_save_cb, NULL);

	gaim_prefs_add_none("/core");
	gaim_prefs_add_none("/plugins");
	gaim_prefs_add_none("/plugins/core");
	gaim_prefs_add_none("/plugins/lopl");
	gaim_prefs_add_none("/plugins/prpl");

	/* Away */
	gaim_prefs_add_none("/core/away");
	gaim_prefs_add_string("/core/away/idle_reporting", "system");
	gaim_prefs_add_bool("/core/away/away_when_idle", TRUE);
	gaim_prefs_add_int("/core/away/mins_before_away", 5);

	/* Away -> Auto-Reply */
	if (!gaim_prefs_exists("/core/away/auto_response/enabled") ||
	    !gaim_prefs_exists("/core/away/auto_response/idle_only"))
	{
		gaim_prefs_add_string("/core/away/auto_reply", "awayidle");
	}
	else
	{
		if (!gaim_prefs_get_bool("/core/away/auto_response/enabled"))
		{
			gaim_prefs_add_string("/core/away/auto_reply", "never");
		}
		else
		{
			if (gaim_prefs_get_bool("/core/away/auto_response/idle_only"))
			{
				gaim_prefs_add_string("/core/away/auto_reply", "awayidle");
			}
			else
			{
				gaim_prefs_add_string("/core/away/auto_reply", "away");
			}
		}
	}

	/* Buddies */
	gaim_prefs_add_none("/core/buddies");

	/* Contact Priority Settings */
	gaim_prefs_add_none("/core/contact");
	gaim_prefs_add_bool("/core/contact/last_match", FALSE);
	gaim_prefs_remove("/core/contact/offline_score");
	gaim_prefs_remove("/core/contact/away_score");
	gaim_prefs_remove("/core/contact/idle_score");
}

void
gaim_prefs_uninit()
{
	if (save_timer != 0)
	{
		gaim_timeout_remove(save_timer);
		save_timer = 0;
		sync_prefs();
	}

	gaim_prefs_disconnect_by_handle(gaim_prefs_get_handle());
}
