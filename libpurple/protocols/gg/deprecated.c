#include "deprecated.h"

#include <libgadu.h>

gboolean ggp_deprecated_setup_proxy(PurpleConnection *gc)
{
	PurpleProxyInfo *gpi = purple_proxy_get_setup(purple_connection_get_account(gc));
	
	if ((purple_proxy_info_get_type(gpi) != PURPLE_PROXY_NONE) &&
		(purple_proxy_info_get_host(gpi) == NULL ||
		purple_proxy_info_get_port(gpi) <= 0))
	{
		gg_proxy_enabled = 0;
		purple_notify_error(NULL, NULL, _("Invalid proxy settings"),
			_("Either the host name or port number specified for your given proxy type is invalid."));
		return FALSE;
	}
	
	if (purple_proxy_info_get_type(gpi) == PURPLE_PROXY_NONE)
	{
		gg_proxy_enabled = 0;
		return TRUE;
	}

	gg_proxy_enabled = 1;
	//TODO: memleak
	gg_proxy_host = g_strdup(purple_proxy_info_get_host(gpi));
	gg_proxy_port = purple_proxy_info_get_port(gpi);
	gg_proxy_username = g_strdup(purple_proxy_info_get_username(gpi));
	gg_proxy_password = g_strdup(purple_proxy_info_get_password(gpi));
	
	return TRUE;
}
