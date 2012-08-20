/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

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
