/*
 * This might be fun to implement. Right? Maybe not. 
 */

#define FAIM_INTERNAL
#include <aim.h>

faim_internal int popups_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0008;
	mod->version = 0x0001;
	mod->toolid = 0x0104;
	mod->toolversion = 0x0001;
	mod->flags = 0;
	strncpy(mod->name, "popups", sizeof(mod->name));
	mod->snachandler = NULL;

	return 0;
}


