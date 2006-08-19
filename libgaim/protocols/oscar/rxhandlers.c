/*
 * Gaim's oscar protocol plugin
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

#include "oscar.h"
#include "peer.h"

aim_module_t *aim__findmodulebygroup(OscarData *od, guint16 group)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)od->modlistv; cur; cur = cur->next) {
		if (cur->family == group)
			return cur;
	}

	return NULL;
}

aim_module_t *aim__findmodule(OscarData *od, const char *name)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)od->modlistv; cur; cur = cur->next) {
		if (strcmp(name, cur->name) == 0)
			return cur;
	}

	return NULL;
}

int aim__registermodule(OscarData *od, int (*modfirst)(OscarData *, aim_module_t *))
{
	aim_module_t *mod;

	if (!od || !modfirst)
		return -1;

	mod = g_new0(aim_module_t, 1);

	if (modfirst(od, mod) == -1) {
		free(mod);
		return -1;
	}

	if (aim__findmodule(od, mod->name)) {
		if (mod->shutdown)
			mod->shutdown(od, mod);
		free(mod);
		return -1;
	}

	mod->next = (aim_module_t *)od->modlistv;
	od->modlistv = mod;

	gaim_debug_misc("oscar", "registered module %s (family 0x%04x, version = 0x%04x, tool 0x%04x, tool version 0x%04x)\n", mod->name, mod->family, mod->version, mod->toolid, mod->toolversion);

	return 0;
}

void aim__shutdownmodules(OscarData *od)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)od->modlistv; cur; ) {
		aim_module_t *tmp;

		tmp = cur->next;

		if (cur->shutdown)
			cur->shutdown(od, cur);

		free(cur);

		cur = tmp;
	}

	od->modlistv = NULL;

	return;
}

#if 0
/*
 * Bleck functions get called when there's no non-bleck functions
 * around to cleanup the mess...
 */
static int bleck(OscarData *od, FlapFrame *frame, ...)
{
	guint16 family, subtype;
	guint16 maxf, maxs;

	static const char *channels[6] = {
		"Invalid (0)",
		"FLAP Version",
		"SNAC",
		"Invalid (3)",
		"Negotiation",
		"FLAP NOP"
	};
	static const int maxchannels = 5;

	/* XXX: this is ugly. and big just for debugging. */
	static const char *literals[14][25] = {
		{"Invalid",
		 NULL
		},
		{"General",
		 "Invalid",
		 "Error",
		 "Client Ready",
		 "Server Ready",
		 "Service Request",
		 "Redirect",
		 "Rate Information Request",
		 "Rate Information",
		 "Rate Information Ack",
		 NULL,
		 "Rate Information Change",
		 "Server Pause",
		 NULL,
		 "Server Resume",
		 "Request Personal User Information",
		 "Personal User Information",
		 "Evil Notification",
		 NULL,
		 "Migration notice",
		 "Message of the Day",
		 "Set Privacy Flags",
		 "Well Known URL",
		 "NOP"
		},
		{"Location",
		 "Invalid",
		 "Error",
		 "Request Rights",
		 "Rights Information",
		 "Set user information",
		 "Request User Information",
		 "User Information",
		 "Watcher Sub Request",
		 "Watcher Notification"
		},
		{"Buddy List Management",
		 "Invalid",
		 "Error",
		 "Request Rights",
		 "Rights Information",
		 "Add Buddy",
		 "Remove Buddy",
		 "Watcher List Query",
		 "Watcher List Response",
		 "Watcher SubRequest",
		 "Watcher Notification",
		 "Reject Notification",
		 "Oncoming Buddy",
		 "Offgoing Buddy"
		},
		{"Messeging",
		 "Invalid",
		 "Error",
		 "Add ICBM Parameter",
		 "Remove ICBM Parameter",
		 "Request Parameter Information",
		 "Parameter Information",
		 "Outgoing Message",
		 "Incoming Message",
		 "Evil Request",
		 "Evil Reply",
		 "Missed Calls",
		 "Message Error",
		 "Host Ack"
		},
		{"Advertisements",
		 "Invalid",
		 "Error",
		 "Request Ad",
		 "Ad Data (GIFs)"
		},
		{"Invitation / Client-to-Client",
		 "Invalid",
		 "Error",
		 "Invite a Friend",
		 "Invitation Ack"
		},
		{"Administrative",
		 "Invalid",
		 "Error",
		 "Information Request",
		 "Information Reply",
		 "Information Change Request",
		 "Information Chat Reply",
		 "Account Confirm Request",
		 "Account Confirm Reply",
		 "Account Delete Request",
		 "Account Delete Reply"
		},
		{"Popups",
		 "Invalid",
		 "Error",
		 "Display Popup"
		},
		{"BOS",
		 "Invalid",
		 "Error",
		 "Request Rights",
		 "Rights Response",
		 "Set group permission mask",
		 "Add permission list entries",
		 "Delete permission list entries",
		 "Add deny list entries",
		 "Delete deny list entries",
		 "Server Error"
		},
		{"User Lookup",
		 "Invalid",
		 "Error",
		 "Search Request",
		 "Search Response"
		},
		{"Stats",
		 "Invalid",
		 "Error",
		 "Set minimum report interval",
		 "Report Events"
		},
		{"Translate",
		 "Invalid",
		 "Error",
		 "Translate Request",
		 "Translate Reply",
		},
		{"Chat Navigation",
		 "Invalid",
		 "Error",
		 "Request rights",
		 "Request Exchange Information",
		 "Request Room Information",
		 "Request Occupant List",
		 "Search for Room",
		 "Outgoing Message",
		 "Incoming Message",
		 "Evil Request",
		 "Evil Reply",
		 "Chat Error",
		}
	};

	maxf = sizeof(literals) / sizeof(literals[0]);
	maxs = sizeof(literals[0]) / sizeof(literals[0][0]);

	if (frame->channel == 0x02) {

		family = byte_stream_get16(&frame->data);
		subtype = byte_stream_get16(&frame->data);

		if ((family < maxf) && (subtype+1 < maxs) && (literals[family][subtype] != NULL))
			gaim_debug_misc("oscar", "bleck: channel %s: null handler for %04x/%04x (%s)\n", channels[frame->channel], family, subtype, literals[family][subtype+1]);
		else
			gaim_debug_misc("oscar", "bleck: channel %s: null handler for %04x/%04x (no literal)\n", channels[frame->channel], family, subtype);
	} else {

		if (frame->channel <= maxchannels)
			gaim_debug_misc("oscar", "bleck: channel %s (0x%02x)\n", channels[frame->channel], frame->channel);
		else
			gaim_debug_misc("oscar", "bleck: unknown channel 0x%02x\n", frame->channel);

	}

	return 1;
}
#endif
