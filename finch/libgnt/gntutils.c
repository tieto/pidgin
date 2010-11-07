/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
 */

#include "gntbutton.h"
#include "gntcheckbox.h"
#include "gntcombobox.h"
#include "gntentry.h"
#include "gntlabel.h"
#include "gntline.h"
#include "gnttextview.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef NO_LIBXML
#include <libxml/parser.h>
#include <libxml/tree.h>
#endif

#include "config.h"

void gnt_util_get_text_bound(const char *text, int *width, int *height)
{
	const char *s = text, *last;
	int count = 1, max = 0;
	int len;

	/* XXX: ew ... everyone look away */
	last = s;
	if (s)
	{
		while (*s)
		{
			if (*s == '\n' || *s == '\r')
			{
				count++;
				len = gnt_util_onscreen_width(last, s);
				if (max < len)
					max = len;
				last = s + 1;
			}
			s = g_utf8_next_char(s);
		}

		len = gnt_util_onscreen_width(last, s);
		if (max < len)
			max = len;
	}

	if (height)
		*height = count;
	if (width)
		*width = max + (count > 1);
}

int gnt_util_onscreen_width(const char *start, const char *end)
{
	int width = 0;

	if (end == NULL)
		end = start + strlen(start);

	while (start < end) {
		width += g_unichar_iswide(g_utf8_get_char(start)) ? 2 : 1;
		start = g_utf8_next_char(start);
	}
	return width;
}

const char *gnt_util_onscreen_width_to_pointer(const char *string, int len, int *w)
{
	int size;
	int width = 0;
	const char *str = string;

	if (len <= 0) {
		len = gnt_util_onscreen_width(string, NULL);
	}

	while (width < len && *str) {
		size = g_unichar_iswide(g_utf8_get_char(str)) ? 2 : 1;
		if (width + size > len)
			break;
		str = g_utf8_next_char(str);
		width += size;
	}
	if (w)
		*w = width;
	return str;
}

char *gnt_util_onscreen_fit_string(const char *string, int maxw)
{
	const char *start, *end;
	GString *str;

	if (maxw <= 0)
		maxw = getmaxx(stdscr) - 4;

	start = string;
	str = g_string_new(NULL);

	while (*start) {
		if ((end = strchr(start, '\n')) != NULL ||
			(end = strchr(start, '\r')) != NULL) {
			if (gnt_util_onscreen_width(start, end) > maxw)
				end = NULL;
		}
		if (end == NULL)
			end = gnt_util_onscreen_width_to_pointer(start, maxw, NULL);
		str = g_string_append_len(str, start, end - start);
		if (*end) {
			str = g_string_append_c(str, '\n');
			if (*end == '\n' || *end == '\r')
				end++;
		}
		start = end;
	}
	return g_string_free(str, FALSE);
}

struct duplicate_fns
{
	GDupFunc key_dup;
	GDupFunc value_dup;
	GHashTable *table;
};

static void
duplicate_values(gpointer key, gpointer value, gpointer data)
{
	struct duplicate_fns *fns = data;
	g_hash_table_insert(fns->table, fns->key_dup ? fns->key_dup(key) : key,
			fns->value_dup ? fns->value_dup(value) : value);
}

GHashTable *g_hash_table_duplicate(GHashTable *src, GHashFunc hash,
		GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d,
		GDupFunc key_dup, GDupFunc value_dup)
{
	GHashTable *dest = g_hash_table_new_full(hash, equal, key_d, value_d);
	struct duplicate_fns fns = {key_dup, value_dup, dest};
	g_hash_table_foreach(src, duplicate_values, &fns);
	return dest;
}

gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint,
				  GValue                *return_accu,
				  const GValue          *handler_return,
				  gpointer               dummy)
{
	gboolean continue_emission;
	gboolean signal_handled;

	signal_handled = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, signal_handled);
	continue_emission = !signal_handled;

	return continue_emission;
}

typedef struct {
	GHashTable *hash;
	GntTree *tree;
} BindingView;

static void
add_binding(gpointer key, gpointer value, gpointer data)
{
	BindingView *bv = data;
	GntBindableActionParam *act = value;
	const char *name = g_hash_table_lookup(bv->hash, act->action);
	if (name && *name) {
		const char *k = gnt_key_lookup(key);
		if (!k)
			k = key;
		gnt_tree_add_row_after(bv->tree, (gpointer)k,
				gnt_tree_create_row(bv->tree, k, name), NULL, NULL);
	}
}

static void
add_action(gpointer key, gpointer value, gpointer data)
{
	BindingView *bv = data;
	g_hash_table_insert(bv->hash, value, key);
}

GntWidget *gnt_widget_bindings_view(GntWidget *widget)
{
	GntBindable *bind = GNT_BINDABLE(widget);
	GntWidget *tree = gnt_tree_new_with_columns(2);
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bind));
	GHashTable *hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	BindingView bv = {hash, GNT_TREE(tree)};

	gnt_tree_set_compare_func(bv.tree, (GCompareFunc)g_utf8_collate);
	g_hash_table_foreach(klass->actions, add_action, &bv);
	g_hash_table_foreach(klass->bindings, add_binding, &bv);
	if (GNT_TREE(tree)->list == NULL) {
		gnt_widget_destroy(tree);
		tree = NULL;
	} else
		gnt_tree_adjust_columns(bv.tree);
	g_hash_table_destroy(hash);

	return tree;
}

#ifndef NO_LIBXML
static GntWidget *
gnt_widget_from_xmlnode(xmlNode *node, GntWidget **data[], int max)
{
	GntWidget *widget = NULL;
	char *name;
	char *id, *prop, *content;
	int val;

	if (node == NULL || node->name == NULL || node->type != XML_ELEMENT_NODE)
		return NULL;

	name = (char*)node->name;
	content = (char*)xmlNodeGetContent(node);
	if (strcmp(name + 1, "window") == 0 || strcmp(name + 1, "box") == 0) {
		xmlNode *ch;
		char *title;
		gboolean vert = (*name == 'v');

		if (name[1] == 'w')
			widget = gnt_window_box_new(FALSE, vert);
		else
			widget = gnt_box_new(FALSE, vert);

		title = (char*)xmlGetProp(node, (xmlChar*)"title");
		if (title) {
			gnt_box_set_title(GNT_BOX(widget), title);
			xmlFree(title);
		}

		prop = (char*)xmlGetProp(node, (xmlChar*)"fill");
		if (prop) {
			if (sscanf(prop, "%d", &val) == 1)
				gnt_box_set_fill(GNT_BOX(widget), !!val);
			xmlFree(prop);
		}

		prop = (char*)xmlGetProp(node, (xmlChar*)"align");
		if (prop) {
			if (sscanf(prop, "%d", &val) == 1)
				gnt_box_set_alignment(GNT_BOX(widget), val);
			xmlFree(prop);
		}

		prop = (char*)xmlGetProp(node, (xmlChar*)"pad");
		if (prop) {
			if (sscanf(prop, "%d", &val) == 1)
				gnt_box_set_pad(GNT_BOX(widget), val);
			xmlFree(prop);
		}

		for (ch = node->children; ch; ch=ch->next)
			gnt_box_add_widget(GNT_BOX(widget), gnt_widget_from_xmlnode(ch, data, max));
	} else if (strcmp(name, "button") == 0) {
		widget = gnt_button_new(content);
	} else if (strcmp(name, "label") == 0) {
		widget = gnt_label_new(content);
	} else if (strcmp(name, "entry") == 0) {
		widget = gnt_entry_new(content);
	} else if (strcmp(name, "combobox") == 0) {
		widget = gnt_combo_box_new();
	} else if (strcmp(name, "checkbox") == 0) {
		widget = gnt_check_box_new(content);
	} else if (strcmp(name, "tree") == 0) {
		widget = gnt_tree_new();
	} else if (strcmp(name, "textview") == 0) {
		widget = gnt_text_view_new();
	} else if (strcmp(name + 1, "line") == 0) {
		widget = gnt_line_new(*name == 'v');
	}

	xmlFree(content);

	if (widget == NULL) {
		g_printerr("Invalid widget name %s\n", name);
		return NULL;
	}

	id = (char*)xmlGetProp(node, (xmlChar*)"id");
	if (id) {
		int i;
		if (sscanf(id, "%d", &i) == 1 && i >= 0 && i < max) {
			*data[i] = widget;
			xmlFree(id);
		}
	}

	prop = (char*)xmlGetProp(node, (xmlChar*)"border");
	if (prop) {
		int val;
		if (sscanf(prop, "%d", &val) == 1) {
			if (val)
				GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER);
			else
				GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER);
		}
		xmlFree(prop);
	}

	prop = (char*)xmlGetProp(node, (xmlChar*)"shadow");
	if (prop) {
		int val;
		if (sscanf(prop, "%d", &val) == 1) {
			if (val)
				GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER);
			else
				GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER);
		}
		xmlFree(prop);
	}

	return widget;
}
#endif

void gnt_util_parse_widgets(const char *string, int num, ...)
{
#ifndef NO_LIBXML
	xmlParserCtxtPtr ctxt;
	xmlDocPtr doc;
	xmlNodePtr node;
	va_list list;
	GntWidget ***data;
	int id;

	ctxt = xmlNewParserCtxt();
	doc = xmlCtxtReadDoc(ctxt, (xmlChar*)string, NULL, NULL, XML_PARSE_NOBLANKS);

	data = g_new0(GntWidget **, num);

	va_start(list, num);
	for (id = 0; id < num; id++)
		data[id] = va_arg(list, gpointer);

	node = xmlDocGetRootElement(doc);
	gnt_widget_from_xmlnode(node, data, num);

	xmlFreeDoc(doc);
	xmlCleanupParser();
	va_end(list);
	g_free(data);
#endif
}

#ifndef NO_LIBXML
static void
util_parse_html_to_tv(xmlNode *node, GntTextView *tv, GntTextFormatFlags flag)
{
	const char *name;
	char *content;
	xmlNode *ch;
	gboolean processed = FALSE;
	char *url = NULL;
	gboolean insert_nl_s = FALSE, insert_nl_e = FALSE;

	if (node == NULL || node->name == NULL || node->type != XML_ELEMENT_NODE)
		return;

	name = (char*)node->name;
	if (g_ascii_strcasecmp(name, "b") == 0 ||
		g_ascii_strcasecmp(name, "strong") == 0 ||
		g_ascii_strcasecmp(name, "i") == 0 ||
		g_ascii_strcasecmp(name, "blockquote") == 0) {
		flag |= GNT_TEXT_FLAG_BOLD;
	} else if (g_ascii_strcasecmp(name, "u") == 0) {
		flag |= GNT_TEXT_FLAG_UNDERLINE;
	} else if (g_ascii_strcasecmp(name, "br") == 0) {
		insert_nl_e = TRUE;
	} else if (g_ascii_strcasecmp(name, "a") == 0) {
		flag |= GNT_TEXT_FLAG_UNDERLINE;
		url = (char *)xmlGetProp(node, (xmlChar*)"href");
	} else if (g_ascii_strcasecmp(name, "h1") == 0 ||
			g_ascii_strcasecmp(name, "h2") == 0 ||
			g_ascii_strcasecmp(name, "h3") == 0 ||
			g_ascii_strcasecmp(name, "h4") == 0 ||
			g_ascii_strcasecmp(name, "h5") == 0 ||
			g_ascii_strcasecmp(name, "h6") == 0) {
		insert_nl_s = TRUE;
		insert_nl_e = TRUE;
	} else if (g_ascii_strcasecmp(name, "title") == 0) {
		insert_nl_s = TRUE;
		insert_nl_e = TRUE;
		flag |= GNT_TEXT_FLAG_BOLD | GNT_TEXT_FLAG_UNDERLINE;
	} else {
		/* XXX: Process other possible tags */
	}

	if (insert_nl_s)
		gnt_text_view_append_text_with_flags(tv, "\n", flag);

	for (ch = node->children; ch; ch = ch->next) {
		if (ch->type == XML_ELEMENT_NODE) {
			processed = TRUE;
			util_parse_html_to_tv(ch, tv, flag);
		}
	}

	if (!processed) {
		content = (char*)xmlNodeGetContent(node);
		gnt_text_view_append_text_with_flags(tv, content, flag);
		xmlFree(content);
	}

	if (url) {
		char *href = g_strdup_printf(" (%s)", url);
		gnt_text_view_append_text_with_flags(tv, href, flag);
		g_free(href);
		xmlFree(url);
	}

	if (insert_nl_e)
		gnt_text_view_append_text_with_flags(tv, "\n", flag);
}
#endif

gboolean gnt_util_parse_xhtml_to_textview(const char *string, GntTextView *tv)
{
#ifdef NO_LIBXML
	return FALSE;
#else
	xmlParserCtxtPtr ctxt;
	xmlDocPtr doc;
	xmlNodePtr node;
	GntTextFormatFlags flag = GNT_TEXT_FLAG_NORMAL;
	gboolean ret = FALSE;

	ctxt = xmlNewParserCtxt();
	doc = xmlCtxtReadDoc(ctxt, (xmlChar*)string, NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_RECOVER);
	if (doc) {
		node = xmlDocGetRootElement(doc);
		util_parse_html_to_tv(node, tv, flag);
		xmlFreeDoc(doc);
		ret = TRUE;
	}
	xmlCleanupParser();
	return ret;
#endif
}

/* Setup trigger widget */
typedef struct {
	char *text;
	GntWidget *button;
} TriggerButton;

static void
free_trigger_button(TriggerButton *b)
{
	g_free(b->text);
	g_free(b);
}

static gboolean
key_pressed(GntWidget *widget, const char *text, TriggerButton *trig)
{
	if (text && trig->text &&
			strcmp(text, trig->text) == 0) {
		gnt_widget_activate(trig->button);
		return TRUE;
	}
	return FALSE;
}

void gnt_util_set_trigger_widget(GntWidget *wid, const char *text, GntWidget *button)
{
	TriggerButton *tb = g_new0(TriggerButton, 1);
	tb->text = g_strdup(text);
	tb->button = button;
	g_signal_connect(G_OBJECT(wid), "key_pressed", G_CALLBACK(key_pressed), tb);
	g_signal_connect_swapped(G_OBJECT(button), "destroy", G_CALLBACK(free_trigger_button), tb);
}
