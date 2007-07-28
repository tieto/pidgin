#include <pygobject.h>
 
/* {{{ Wrapper for gpointer */

typedef struct {
	PyObject_HEAD
	PyGPointer *data;
} mygpointer;

static PyObject *
mygpointer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	mygpointer *self = (mygpointer*)type->tp_alloc(type, 0);
	self->data = NULL;
	return (PyObject*)self;
}

static const PyMethodDef mygpointer_methods[] = {
	/*{"value", (PyCFunction)get_value, METH_NOARGS, NULL},*/
	{NULL, NULL, 0, NULL}
};

static int
mygpointer_init(mygpointer *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"data", NULL};
	PyObject *data = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, 
				&data))
		return -1; 

	Py_INCREF(data);
	Py_DECREF(self->data);
	self->data = data;

	return 0;
}

static PyTypeObject mygpointer_type = {
	PyObject_HEAD_INIT(&PyType_Type)
	.tp_name = "gpointer",
	.tp_basicsize = sizeof(mygpointer),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = "gpointer stuff",
	.tp_members = NULL,
	.tp_init = mygpointer_init,
	.tp_new = mygpointer_new,
	.tp_methods = mygpointer_methods
};

PyObject *create_mygpointer(gpointer data)
{
	mygpointer *p = mygpointer_new(&mygpointer_type, NULL, NULL);
	p->data = data;
	return (PyObject *)p;
}
/* }}} Wrapper for gpointer */

void gnt_register_classes (PyObject *d); 
extern PyMethodDef gnt_functions[];
 
DL_EXPORT(void)
initgnt(void)
{
    PyObject *m, *d;
 
    init_pygobject ();

	if (PyType_Ready(&mygpointer_type) < 0)
		return;

    m = Py_InitModule ("gnt", gnt_functions);
    d = PyModule_GetDict (m);
 
    gnt_register_classes (d);
    gnt_add_constants(m, "GNT_");
 
    if (PyErr_Occurred ()) {
        Py_FatalError ("can't initialise module sad");
    }
}

