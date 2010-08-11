#include "common.h"

PyObject *
create_pyobject_from_string_list(GList *list)
{
	PyObject *py_list;
	if (list == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if ((py_list = PyList_New(0)) == NULL) {
		g_list_foreach(list, (GFunc)g_free, NULL);
		g_list_free(list);
		return NULL;
	}
	while (list) {
		PyObject *obj = PyString_FromString(list->data);
		PyList_Append(py_list, obj);
		Py_DECREF(obj);
		g_free(list->data);
		list = g_list_delete_link(list, list);
	}
	return py_list;
}
