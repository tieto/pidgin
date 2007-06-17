
#ifndef __gtksourceview_marshal_MARSHAL_H__
#define __gtksourceview_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (gtksourceview-marshal.list:1) */
#define gtksourceview_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:BOOLEAN (gtksourceview-marshal.list:2) */
#define gtksourceview_marshal_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN

/* VOID:BOXED (gtksourceview-marshal.list:3) */
#define gtksourceview_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

/* VOID:BOXED,BOXED (gtksourceview-marshal.list:4) */
extern void gtksourceview_marshal_VOID__BOXED_BOXED (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* VOID:STRING (gtksourceview-marshal.list:5) */
#define gtksourceview_marshal_VOID__STRING	g_cclosure_marshal_VOID__STRING

G_END_DECLS

#endif /* __gtksourceview_marshal_MARSHAL_H__ */

