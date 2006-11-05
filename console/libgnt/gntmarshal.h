
#ifndef __gnt_closure_marshal_MARSHAL_H__
#define __gnt_closure_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:VOID (/dev/stdin:1) */
extern void gnt_closure_marshal_BOOLEAN__VOID (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* BOOLEAN:STRING (/dev/stdin:2) */
extern void gnt_closure_marshal_BOOLEAN__STRING (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* VOID:INT,INT,INT,INT (/dev/stdin:3) */
extern void gnt_closure_marshal_VOID__INT_INT_INT_INT (GClosure     *closure,
                                                       GValue       *return_value,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      invocation_hint,
                                                       gpointer      marshal_data);

/* VOID:INT,INT (/dev/stdin:4) */
extern void gnt_closure_marshal_VOID__INT_INT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:POINTER,POINTER (/dev/stdin:5) */
extern void gnt_closure_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                                       GValue       *return_value,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      invocation_hint,
                                                       gpointer      marshal_data);

/* BOOLEAN:INT,INT (/dev/stdin:6) */
extern void gnt_closure_marshal_BOOLEAN__INT_INT (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* BOOLEAN:INT,INT,INT (/dev/stdin:7) */
extern void gnt_closure_marshal_BOOLEAN__INT_INT_INT (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* BOOLEAN:POINTER,POINTER,POINTER (/dev/stdin:8) */
extern void gnt_closure_marshal_BOOLEAN__POINTER_POINTER_POINTER (GClosure     *closure,
                                                                  GValue       *return_value,
                                                                  guint         n_param_values,
                                                                  const GValue *param_values,
                                                                  gpointer      invocation_hint,
                                                                  gpointer      marshal_data);

/* BOOLEAN:INT,INT,INT,POINTER (/dev/stdin:9) */
extern void gnt_closure_marshal_BOOLEAN__INT_INT_INT_POINTER (GClosure     *closure,
                                                              GValue       *return_value,
                                                              guint         n_param_values,
                                                              const GValue *param_values,
                                                              gpointer      invocation_hint,
                                                              gpointer      marshal_data);

G_END_DECLS

#endif /* __gnt_closure_marshal_MARSHAL_H__ */

gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint,
				  GValue                *return_accu,
				  const GValue          *handler_return,
				  gpointer               dummy);



