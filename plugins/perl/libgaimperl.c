#include <gmodule.h>
void __attribute__ ((constructor)) my_init(void);

void __attribute__ ((constructor)) my_init() {
	/* Very evil hack...puts perl.so's symbols in the global table
	 * but does not create a circular dependancy because g_module_open
	 * will only open the library once. */
	g_module_open("perl.so", 0);
}
