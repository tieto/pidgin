/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _MULTI_H_
#define _MULTI_H_

#include "account.h"
#include "plugin.h"

struct proto_actions_menu {
	char *label;
	void (*callback)(GaimConnection *);
	GaimConnection *gc;
};

struct proto_buddy_menu {
	char *label;
	void (*callback)(GaimConnection *, const char *);
	GaimConnection *gc;
};

struct proto_chat_entry {
	char *label;
	char *identifier;
	char *def;
	gboolean is_int;
	int min;
	int max;
};

#endif /* _MULTI_H_ */
