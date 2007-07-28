#include <pygobject.h>
 
void gnt_register_classes (PyObject *d); 
extern PyMethodDef gnt_functions[];
 
DL_EXPORT(void)
initgnt(void)
{
    PyObject *m, *d;
 
    init_pygobject ();

    m = Py_InitModule ("gnt", gnt_functions);
    d = PyModule_GetDict (m);
 
    gnt_register_classes (d);
    gnt_add_constants(m, "GNT_");
 
    if (PyErr_Occurred ()) {
        Py_FatalError ("can't initialise module sad");
    }
}

