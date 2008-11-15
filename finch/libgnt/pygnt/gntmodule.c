#include <pygobject.h>
#include "gnt.h"
 
void gnt_register_classes (PyObject *d); 
extern PyMethodDef gnt_functions[];

static void
gnt_add_string_constants(PyObject *module)
{
#define define_key(x) if (GNT_KEY_##x && *(GNT_KEY_##x))  PyModule_AddStringConstant(module, "KEY_" #x, GNT_KEY_##x)

	define_key(POPUP);

	define_key(LEFT);
	define_key(RIGHT);
	define_key(UP);
	define_key(DOWN);

	define_key(CTRL_UP);
	define_key(CTRL_DOWN);
	define_key(CTRL_RIGHT);
	define_key(CTRL_LEFT);

	define_key(PGUP);
	define_key(PGDOWN);
	define_key(HOME);
	define_key(END);

	define_key(ENTER);

	define_key(BACKSPACE);
	define_key(DEL);
	define_key(INS);
	define_key(BACK_TAB);

	define_key(CTRL_A);
	define_key(CTRL_B);
	define_key(CTRL_D);
	define_key(CTRL_E);
	define_key(CTRL_F);
	define_key(CTRL_G);
	define_key(CTRL_H);
	define_key(CTRL_I);
	define_key(CTRL_J);
	define_key(CTRL_K);
	define_key(CTRL_L);
	define_key(CTRL_M);
	define_key(CTRL_N);
	define_key(CTRL_O);
	define_key(CTRL_P);
	define_key(CTRL_R);
	define_key(CTRL_T);
	define_key(CTRL_U);
	define_key(CTRL_V);
	define_key(CTRL_W);
	define_key(CTRL_X);
	define_key(CTRL_Y);

	define_key(F1);
	define_key(F2);
	define_key(F3);
	define_key(F4);
	define_key(F5);
	define_key(F6);
	define_key(F7);
	define_key(F8);
	define_key(F9);
	define_key(F10);
	define_key(F11);
	define_key(F12);
}
 
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

	gnt_init();
	gnt_add_string_constants(m);
}

