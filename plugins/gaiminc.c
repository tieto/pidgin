#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include "gaim.h"

void gaim_plugin_init() {
}

void write_to_conv(struct conversation *c, char *what, int flags) {
	printf("this got called\n");
}
