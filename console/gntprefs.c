#include <prefs.h>

#include "gntprefs.h"
#include "gntgaim.h"

void gg_prefs_init()
{
	gaim_prefs_add_none("/gaim");
	gaim_prefs_add_none("/gaim/gnt");

	gaim_prefs_add_none("/gaim/gnt/plugins");
	gaim_prefs_add_string_list("/gaim/gnt/plugins/loaded", NULL);
}

