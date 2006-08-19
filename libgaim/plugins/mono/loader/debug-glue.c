#include "mono-glue.h"
#include "debug.h"

void gaim_debug_glue(int type, MonoString *cat, MonoString *str)
{
	char *ccat;
	char *cstr;
	
	ccat = mono_string_to_utf8(cat);
	cstr = mono_string_to_utf8(str);
	
	gaim_debug(type, ccat, cstr);
	
	g_free(ccat);
	g_free(cstr);
}
