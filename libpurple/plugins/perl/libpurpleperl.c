#include <gmodule.h>

#ifdef  __SUNPRO_C
#pragma init (my_init)
void my_init(void);

void my_init() {
#else
void __attribute__ ((constructor)) my_init(void);

void __attribute__ ((constructor)) my_init() {
#endif

	/* Very evil hack...puts perl.so's symbols in the global table
	 * but does not create a circular dependancy because g_module_open
	 * will only open the library once. */
	g_module_open("perl.so", 0);
}
