/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Family 0x0006 - This isn't really ever used by anyone anymore.
 *
 * Once upon a time, there used to be a menu item in AIM clients that
 * said something like "Invite a friend to use AIM..." and then it would
 * ask for an email address and it would sent a mail to them saying
 * how perfectly wonderful the AIM service is and why you should use it
 * and click here if you hate the person who sent this to you and want to
 * complain and yell at them in a small box with pretty fonts.
 *
 * I could've sworn libfaim had this implemented once, a long long time ago,
 * but I can't find it.
 *
 * I'm mainly adding this so that I can keep advertising that we support
 * group 6, even though we don't.
 *
 */

#include "oscar.h"

int invite_modfirst(OscarData *od, aim_module_t *mod)
{

	mod->family = 0x0006;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "invite", sizeof(mod->name));
	mod->snachandler = NULL;

	return 0;
}
