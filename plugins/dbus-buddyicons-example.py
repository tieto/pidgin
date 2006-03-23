#!/usr/bin/env python
#
# Print the aliases of buddies who have a buddy-icon set.
#
# Gaim is the legal property of its developers, whose names are too numerous
# to list here.  Please refer to the COPYRIGHT file distributed with this
# source distribution.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import dbus

bus = dbus.SessionBus()
obj = bus.get_object("net.sf.gaim.GaimService", "/net/sf/gaim/GaimObject")
gaim = dbus.Interface(obj, "net.sf.gaim.GaimInterface")

node = gaim.GaimBlistGetRoot()
while node != 0:
	if gaim.GaimBlistNodeIsBuddy(node):
		icon = gaim.GaimBuddyGetIcon(node)
		if icon != 0:
			print gaim.GaimBuddyGetAlias(node)
	node = gaim.GaimBlistNodeNext(node, 0)
