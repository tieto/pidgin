#include "gntmarshal.h"

void gnt_closure_marshal_BOOLEAN__VOID(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef gboolean (*func) (gpointer data1, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;
	gboolean ret;

	g_return_if_fail(ret_value != NULL);
	g_return_if_fail(n_param_values == 1);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	ret = callback(data1, data2);
	g_value_set_boolean(ret_value, ret);
}

void gnt_closure_marshal_BOOLEAN__STRING(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef gboolean (*func) (gpointer data1, const char *arg1, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;
	gboolean ret;

	g_return_if_fail(ret_value != NULL);
	g_return_if_fail(n_param_values == 2);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	ret = callback(data1, g_value_get_string(param_values + 1) , data2);
	g_value_set_boolean(ret_value, ret);
}

void gnt_closure_marshal_VOID__INT_INT_INT_INT(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef void (*func) (gpointer data1, int, int, int, int, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;

	g_return_if_fail(n_param_values == 5);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	callback(data1,
			g_value_get_int(param_values + 1) ,
			g_value_get_int(param_values + 2) ,
			g_value_get_int(param_values + 3) ,
			g_value_get_int(param_values + 4) ,
			data2);
}

void gnt_closure_marshal_VOID__INT_INT(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef void (*func) (gpointer data1, int, int, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;

	g_return_if_fail(n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	callback(data1,
			g_value_get_int(param_values + 1) ,
			g_value_get_int(param_values + 2) ,
			data2);
}

void gnt_closure_marshal_VOID__POINTER_POINTER(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef void (*func) (gpointer data1, gpointer, gpointer, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;

	g_return_if_fail(n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	callback(data1,
			g_value_get_pointer(param_values + 1) ,
			g_value_get_pointer(param_values + 2) ,
			data2);
}

void gnt_closure_marshal_BOOLEAN__INT_INT(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef gboolean (*func) (gpointer data1, int, int, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;
	gboolean ret;

	g_return_if_fail(ret_value != NULL);
	g_return_if_fail(n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	ret = callback(data1,
			g_value_get_int(param_values + 1) ,
			g_value_get_int(param_values + 2) ,
			data2);
	g_value_set_boolean(ret_value, ret);
}


void gnt_closure_marshal_BOOLEAN__INT_INT_INT(GClosure *closure,
										GValue *ret_value,
										guint n_param_values,
										const GValue *param_values,
										gpointer invocation_hint,
										gpointer marshal_data)
{
	typedef gboolean (*func) (gpointer data1, int, int, int, gpointer data2);
	register func callback;
	register GCClosure *cc = (GCClosure*)closure;
	register gpointer data1, data2;
	gboolean ret;

	g_return_if_fail(ret_value != NULL);
	g_return_if_fail(n_param_values == 4);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (func) (marshal_data ? marshal_data : cc->callback);
	ret = callback(data1,
			g_value_get_int(param_values + 1) ,
			g_value_get_int(param_values + 2) ,
			g_value_get_int(param_values + 3) ,
			data2);
	g_value_set_boolean(ret_value, ret);
}


