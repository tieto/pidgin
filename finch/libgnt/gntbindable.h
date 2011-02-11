/**
 * @file gntbindable.h Bindable API
 * @ingroup gnt
 */
/*
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

#ifndef GNT_BINDABLE_H
#define GNT_BINDABLE_H

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <ncurses.h>

#define GNT_TYPE_BINDABLE				(gnt_bindable_get_gtype())
#define GNT_BINDABLE(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_BINDABLE, GntBindable))
#define GNT_BINDABLE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_BINDABLE, GntBindableClass))
#define GNT_IS_BINDABLE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_BINDABLE))
#define GNT_IS_BINDABLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_BINDABLE))
#define GNT_BINDABLE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_BINDABLE, GntBindableClass))

#define	GNTDEBUG

typedef struct _GntBindable			GntBindable;
typedef struct _GntBindableClass		GntBindableClass;

struct _GntBindable
{
	GObject inherit;
};

struct _GntBindableClass
{
	GObjectClass parent;

	GHashTable *remaps;   /* Key remaps */
	GHashTable *actions;  /* name -> Action */
	GHashTable *bindings; /* key -> ActionParam */

	GntBindable * help_window;

	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 *
 *
 * @return
 */
GType gnt_bindable_get_gtype(void);

/******************/
/*   Key Remaps   */
/******************/
const char * gnt_bindable_remap_keys(GntBindable *bindable, const char *text);

/******************/
/* Bindable Actions */
/******************/
typedef gboolean (*GntBindableActionCallback) (GntBindable *bindable, GList *params);
typedef gboolean (*GntBindableActionCallbackNoParam)(GntBindable *bindable);

typedef struct _GntBindableAction GntBindableAction;
typedef struct _GntBindableActionParam GntBindableActionParam;

struct _GntBindableAction
{
	char *name;        /* The name of the action */
	union {
		gboolean (*action)(GntBindable *bindable, GList *params);
		gboolean (*action_noparam)(GntBindable *bindable);
	} u;
};

struct _GntBindableActionParam
{
	GntBindableAction *action;
	GList *list;
};

/*GntBindableAction *gnt_bindable_action_parse(const char *name);*/

/**
 * Free a bindable action.
 *
 * @param action The bindable action.
 */
void gnt_bindable_action_free(GntBindableAction *action);

/**
 * Free a GntBindableActionParam.
 *
 * @param param  The GntBindableActionParam to free.
 */
void gnt_bindable_action_param_free(GntBindableActionParam *param);

/**
 * Register a bindable action for a class.
 *
 * @param klass      The class the binding is for.
 * @param name       The name of the binding.
 * @param callback   The callback  for the binding.
 * @param trigger    The default trigger for the binding, or @c NULL, followed by a NULL-terminated
 *                   list of default parameters.
 */
void gnt_bindable_class_register_action(GntBindableClass *klass, const char *name, GntBindableActionCallback callback, const char *trigger, ...);

/**
 * Register a key-binding to an existing action.
 *
 * @param klass     The class the binding is for.
 * @param name      The name of the binding.
 * @param trigger   A new trigger for the binding, followed by a @c NULL-terminated list of parameters for the callback.
 */
void gnt_bindable_register_binding(GntBindableClass *klass, const char *name, const char *trigger, ...);

/**
 * Perform an action from a keybinding.
 *
 * @param bindable  The bindable object.
 * @param keys      The key to trigger the action.
 *
 * @return  @c TRUE if the action was performed successfully, @c FALSE otherwise.
 */
gboolean gnt_bindable_perform_action_key(GntBindable *bindable, const char *keys);

/**
 * Discover if a key is bound.
 *
 * @param bindable  The bindable object.
 * @param keys      The key to check for.
 *
 * @return  @c TRUE if the the key has an action associated with it.
 */
gboolean gnt_bindable_check_key(GntBindable *bindable, const char *keys);

/**
 * Perform an action on a bindable object.
 *
 * @param bindable  The bindable object.
 * @param name      The action to perform, followed by a @c NULL-terminated list of parameters.
 *
 * @return  @c TRUE if the action was performed successfully, @c FALSE otherwise.
 */
gboolean gnt_bindable_perform_action_named(GntBindable *bindable, const char *name, ...) G_GNUC_NULL_TERMINATED;

/**
 * Returns a GntTree populated with "key" -> "binding" for the widget.
 *
 * @param bind  The object to list the bindings for.
 *
 * @return   The GntTree.
 */
GntBindable * gnt_bindable_bindings_view(GntBindable *bind);

/**
 * Builds a window that list the key bindings for a GntBindable object.
 * From this window a user can select a listing to rebind a new key for the given action.
 *
 * @param bindable   The object to list the bindings for.
 *
 * @return  @c TRUE
 */

gboolean gnt_bindable_build_help_window(GntBindable *bindable);

G_END_DECLS

#endif /* GNT_BINDABLE_H */

