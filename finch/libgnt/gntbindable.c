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

#include <string.h>

#include "gntbindable.h"
#include "gntstyle.h"
#include "gnt.h"
#include "gntutils.h"
#include "gnttextview.h"
#include "gnttree.h"
#include "gntbox.h"
#include "gntbutton.h"
#include "gntwindow.h"
#include "gntlabel.h"

static GObjectClass *parent_class = NULL;

static struct
{
	char * okeys;         /* Old keystrokes */
	char * keys;          /* New Keystrokes being bound to the action */
	GntBindableClass * klass; /* Class of the object that's getting keys rebound */
	char * name;          /* The name of the action */
	GList * params;       /* The list of paramaters */
} rebind_info;

static void 
gnt_bindable_free_rebind_info(void)
{
	g_free(rebind_info.name);
	g_free(rebind_info.keys);
	g_free(rebind_info.okeys);
}

static void
gnt_bindable_rebinding_cancel(GntWidget *button, gpointer data)
{
	gnt_bindable_free_rebind_info();
	gnt_widget_destroy(GNT_WIDGET(data));
}

static void
gnt_bindable_rebinding_rebind(GntWidget *button, gpointer data)
{
	if (rebind_info.keys) {
		gnt_bindable_register_binding(rebind_info.klass,
				NULL,
				rebind_info.okeys,
				rebind_info.params);
		gnt_bindable_register_binding(rebind_info.klass,
				rebind_info.name,
				rebind_info.keys,
				rebind_info.params);
	}
	gnt_bindable_free_rebind_info();
	gnt_widget_destroy(GNT_WIDGET(data));
}

static gboolean
gnt_bindable_rebinding_grab_key(GntBindable *bindable, const char *text, gpointer data)
{
	GntTextView *textview = GNT_TEXT_VIEW(data);
	char *new_text;
	const char *tmp;

	if (text && *text) {
		/* Rebinding tab or enter for something is probably not that great an idea */
		if (!strcmp(text, GNT_KEY_CTRL_I) || !strcmp(text, GNT_KEY_ENTER)) {
			return FALSE;
		}
		
		tmp = gnt_key_lookup(text);
		new_text = g_strdup_printf("KEY: \"%s\"", tmp);
		gnt_text_view_clear(textview);
		gnt_text_view_append_text_with_flags(textview, new_text, GNT_TEXT_FLAG_NORMAL);
		g_free(new_text);

		g_free(rebind_info.keys);
		rebind_info.keys = g_strdup(text);

		return TRUE;
	}
	return FALSE;
} 
static void
gnt_bindable_rebinding_activate(GntBindable *data, gpointer bindable)
{
	const char *widget_name = g_type_name(G_OBJECT_TYPE(bindable));
	char *keys;
	GntWidget *key_textview;
	GntWidget *label;
	GntWidget *bind_button, *cancel_button;
	GntWidget *button_box;
	GList *current_row_data;
	char *tmp;
	GntWidget *win = gnt_window_new();
	GntTree *tree = GNT_TREE(data);
	GntWidget *vbox = gnt_box_new(FALSE, TRUE);

	rebind_info.klass = GNT_BINDABLE_GET_CLASS(bindable);

	current_row_data = gnt_tree_get_selection_text_list(tree);
	rebind_info.name = g_strdup(g_list_nth_data(current_row_data, 1));

	keys = gnt_tree_get_selection_data(tree);
	rebind_info.okeys = g_strdup(gnt_key_translate(keys));

	rebind_info.params = NULL;

	g_list_foreach(current_row_data, (GFunc)g_free, NULL);
	g_list_free(current_row_data);

	gnt_box_set_alignment(GNT_BOX(vbox), GNT_ALIGN_MID);

	gnt_box_set_title(GNT_BOX(win), "Key Capture");

	tmp = g_strdup_printf("Type the new bindings for %s in a %s.", rebind_info.name, widget_name);
	label = gnt_label_new(tmp);
	g_free(tmp);
	gnt_box_add_widget(GNT_BOX(vbox), label);

	tmp = g_strdup_printf("KEY: \"%s\"", keys);
	key_textview = gnt_text_view_new();
	gnt_widget_set_size(key_textview, key_textview->priv.x, 2);
	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(key_textview), tmp, GNT_TEXT_FLAG_NORMAL);
	g_free(tmp);
	gnt_widget_set_name(key_textview, "keystroke");
	gnt_box_add_widget(GNT_BOX(vbox), key_textview);

	g_signal_connect(G_OBJECT(win), "key_pressed", G_CALLBACK(gnt_bindable_rebinding_grab_key), key_textview);

	button_box = gnt_box_new(FALSE, FALSE);
	
	bind_button = gnt_button_new("BIND");
	gnt_widget_set_name(bind_button, "bind");
	gnt_box_add_widget(GNT_BOX(button_box), bind_button);
	
	cancel_button = gnt_button_new("Cancel");
	gnt_widget_set_name(cancel_button, "cancel");
	gnt_box_add_widget(GNT_BOX(button_box), cancel_button);
	
	g_signal_connect(G_OBJECT(bind_button), "activate", G_CALLBACK(gnt_bindable_rebinding_rebind), win);
	g_signal_connect(G_OBJECT(cancel_button), "activate", G_CALLBACK(gnt_bindable_rebinding_cancel), win);
	
	gnt_box_add_widget(GNT_BOX(vbox), button_box);

	gnt_box_add_widget(GNT_BOX(win), vbox);
	gnt_widget_show(win);
}

typedef struct
{
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

static void
gnt_bindable_class_init(GntBindableClass *klass)
{
	parent_class = g_type_class_peek_parent(klass);

	klass->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)gnt_bindable_action_free);
	klass->bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify)gnt_bindable_action_param_free);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
}

static gpointer
bindable_clone(GntBindableAction *action)
{
	GntBindableAction *ret = g_new0(GntBindableAction, 1);
	ret->name = g_strdup(action->name);
	ret->u = action->u;
	return ret;
}

static gpointer
binding_clone(GntBindableActionParam *param)
{
	GntBindableActionParam *p = g_new0(GntBindableActionParam, 1);
	p->list = g_list_copy(param->list);
	p->action = param->action;
	return p;
}

static void
duplicate_hashes(GntBindableClass *klass)
{
	/* Duplicate the bindings from parent class */
	if (klass->actions) {
		klass->actions = g_hash_table_duplicate(klass->actions, g_str_hash,
					g_str_equal, g_free, (GDestroyNotify)gnt_bindable_action_free,
					(GDupFunc)g_strdup, (GDupFunc)bindable_clone);
		klass->bindings = g_hash_table_duplicate(klass->bindings, g_str_hash,
					g_str_equal, g_free, (GDestroyNotify)gnt_bindable_action_param_free,
					(GDupFunc)g_strdup, (GDupFunc)binding_clone);
	} else {
		klass->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					(GDestroyNotify)gnt_bindable_action_free);
		klass->bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					(GDestroyNotify)gnt_bindable_action_param_free);
	}

	GNTDEBUG;
}

/******************************************************************************
 * GntBindable API
 *****************************************************************************/
GType
gnt_bindable_get_gtype(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(GntBindableClass),
			(GBaseInitFunc)duplicate_hashes,	/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_bindable_class_init,
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntBindable),
			0,						/* n_preallocs		*/
			NULL,					/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntBindable",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**
 * Key Remaps
 */
const char *
gnt_bindable_remap_keys(GntBindable *bindable, const char *text)
{
	const char *remap = NULL;
	GType type = G_OBJECT_TYPE(bindable);
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));

	if (klass->remaps == NULL)
	{
		klass->remaps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		gnt_styles_get_keyremaps(type, klass->remaps);
	}

	remap = g_hash_table_lookup(klass->remaps, text);

	return (remap ? remap : text);
}

/**
 * Actions and Bindings
 */
gboolean
gnt_bindable_perform_action_named(GntBindable *bindable, const char *name, ...)
{
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));
	GList *list = NULL;
	va_list args;
	GntBindableAction *action;
	void *p;

	va_start(args, name);
	while ((p = va_arg(args, void *)) != NULL)
		list = g_list_append(list, p);
	va_end(args);
	
	action = g_hash_table_lookup(klass->actions, name);
	if (action && action->u.action) {
		return action->u.action(bindable, list);
	}
	return FALSE;
}

gboolean
gnt_bindable_perform_action_key(GntBindable *bindable, const char *keys)
{
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bindable));
	GntBindableActionParam *param = g_hash_table_lookup(klass->bindings, keys);

	if (param && param->action) {
		if (param->list)
			return param->action->u.action(bindable, param->list);
		else
			return param->action->u.action_noparam(bindable);
	}
	return FALSE;
}

static void
register_binding(GntBindableClass *klass, const char *name, const char *trigger, GList *list)
{
	GntBindableActionParam *param;
	GntBindableAction *action;

	if (name == NULL || *name == '\0') {
		g_hash_table_remove(klass->bindings, (char*)trigger);
		gnt_keys_del_combination(trigger);
		return;
	}

	action = g_hash_table_lookup(klass->actions, name);
	if (!action) {
		g_printerr("GntBindable: Invalid action name %s for %s\n",
				name, g_type_name(G_OBJECT_CLASS_TYPE(klass)));
		if (list)
			g_list_free(list);
		return;
	}

	param = g_new0(GntBindableActionParam, 1);
	param->action = action;
	param->list = list;
	g_hash_table_replace(klass->bindings, g_strdup(trigger), param);
	gnt_keys_add_combination(trigger);
}

void gnt_bindable_register_binding(GntBindableClass *klass, const char *name,
			const char *trigger, ...)
{
	GList *list = NULL;
	va_list args;
	void *data;

	va_start(args, trigger);
	while ((data = va_arg(args, void *))) {
		list = g_list_append(list, data);
	}
	va_end(args);

	register_binding(klass, name, trigger, list);
}

void gnt_bindable_class_register_action(GntBindableClass *klass, const char *name,
			GntBindableActionCallback callback, const char *trigger, ...)
{
	void *data;
	va_list args;
	GntBindableAction *action = g_new0(GntBindableAction, 1);
	GList *list;

	action->name = g_strdup(name);
	action->u.action = callback;

	g_hash_table_replace(klass->actions, g_strdup(name), action);

	if (trigger && *trigger) {
		list = NULL;
		va_start(args, trigger);
		while ((data = va_arg(args, void *))) {
			list = g_list_append(list, data);
		}
		va_end(args);

		register_binding(klass, name, trigger, list);
	}
}

void gnt_bindable_action_free(GntBindableAction *action)
{
	g_free(action->name);
	g_free(action);
}

void gnt_bindable_action_param_free(GntBindableActionParam *param)
{
	g_list_free(param->list);   /* XXX: There may be a leak here for string parameters */
	g_free(param);
}

GntBindable * gnt_bindable_bindings_view(GntBindable *bind)
{
	GntBindable *tree = GNT_BINDABLE(gnt_tree_new_with_columns(2));
	GntBindableClass *klass = GNT_BINDABLE_CLASS(GNT_BINDABLE_GET_CLASS(bind));
	GHashTable *hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	BindingView bv = {hash, GNT_TREE(tree)};

	gnt_tree_set_compare_func(bv.tree, (GCompareFunc)g_utf8_collate);
	g_hash_table_foreach(klass->actions, add_action, &bv);
	g_hash_table_foreach(klass->bindings, add_binding, &bv);
	if (GNT_TREE(tree)->list == NULL) {
		gnt_widget_destroy(GNT_WIDGET(tree));
		tree = NULL;
	} else
		gnt_tree_adjust_columns(bv.tree);
	g_hash_table_destroy(hash);

	return tree;
}

static void
reset_binding_window(GntBindableClass *window, gpointer k)
{
	GntBindableClass *klass = GNT_BINDABLE_CLASS(k);
	klass->help_window = NULL;
}

gboolean
gnt_bindable_build_help_window(GntBindable *bindable)
{
	GntWidget *tree;
	GntBindableClass *klass = GNT_BINDABLE_GET_CLASS(bindable);
	char *title;

	tree = GNT_WIDGET(gnt_bindable_bindings_view(bindable));

	klass->help_window = GNT_BINDABLE(gnt_window_new());
	title = g_strdup_printf("Bindings for %s", g_type_name(G_OBJECT_TYPE(bindable)));
	gnt_box_set_title(GNT_BOX(klass->help_window), title);
	if (tree) {
		g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(gnt_bindable_rebinding_activate), bindable);
		gnt_box_add_widget(GNT_BOX(klass->help_window), tree);
	} else
		gnt_box_add_widget(GNT_BOX(klass->help_window), gnt_label_new("This widget has no customizable bindings."));

	g_signal_connect(G_OBJECT(klass->help_window), "destroy", G_CALLBACK(reset_binding_window), klass);
	gnt_widget_show(GNT_WIDGET(klass->help_window));
	g_free(title);

	return TRUE;
}

