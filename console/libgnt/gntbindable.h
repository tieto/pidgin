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

#define	GNTDEBUG	fprintf(stderr, "%s\n", __FUNCTION__)

typedef struct _GnBindable			GntBindable;
typedef struct _GnBindableClass		GntBindableClass;

struct _GnBindable
{
	GObject inherit;
};

struct _GnBindableClass
{
	GObjectClass parent;

	GHashTable *remaps;   /* Key remaps */
	GHashTable *actions;  /* name -> Action */
	GHashTable *bindings; /* key -> ActionParam */

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

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

typedef struct _GnBindableAction GntBindableAction;
typedef struct _GnBindableActionParam GntBindableActionParam;

struct _GnBindableAction
{
	char *name;        /* The name of the action */
	union {
		gboolean (*action)(GntBindable *bindable, GList *params);
		gboolean (*action_noparam)(GntBindable *bindable);
	} u;
};

struct _GnBindableActionParam
{
	GntBindableAction *action;
	GList *list;
};


/*GntBindableAction *gnt_bindable_action_parse(const char *name);*/

void gnt_bindable_action_free(GntBindableAction *action);
void gnt_bindable_action_param_free(GntBindableActionParam *param);

void gnt_bindable_class_register_action(GntBindableClass *klass, const char *name,
			GntBindableActionCallback callback, const char *trigger, ...);
void gnt_bindable_register_binding(GntBindableClass *klass, const char *name,
			const char *trigger, ...);

gboolean gnt_bindable_perform_action_key(GntBindable *bindable, const char *keys);
gboolean gnt_bindable_perform_action_named(GntBindable *bindable, const char *name, ...);

G_END_DECLS

#endif /* GNT_BINDABLE_H */

