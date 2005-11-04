/*
 * idle.c - I'dle Mak'er plugin for Gaim
 *
 * This file is part of Gaim.
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */

#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "notify.h"
#include "plugin.h"
#include "request.h"
#include "server.h"
#include "status.h"
#include "version.h"

/* This plugin no longer depends on gtk */
#define IDLE_PLUGIN_ID "core-idle"

static GList *idled_accts = NULL;

static gboolean
idle_filter(GaimAccount *acct)
{
	if (g_list_find(idled_accts, acct))
		return TRUE;

	return FALSE;
}

static void
set_idle_time(GaimAccount *acct, int mins_idle)
{
	time_t t;
	GaimConnection *gc = gaim_account_get_connection(acct);
	GaimPresence *presence = gaim_account_get_presence(acct);

	if (!gc)
		return;

	gaim_debug_info("idle",
			"setting idle time for %s to %d\n",
			gaim_account_get_username(acct), mins_idle);

	t = time(NULL) - (60 * mins_idle); /* subtract seconds idle from current time */
	gc->last_sent_time = t;

	gaim_presence_set_idle(presence, mins_idle ? TRUE : FALSE, t);
}

static void
idle_action_ok(void *ignored, GaimRequestFields *fields)
{
	int tm = gaim_request_fields_get_integer(fields, "mins");
	GaimAccount *acct = gaim_request_fields_get_account(fields, "acct");

	/* only add the account to the GList if it's not already been idled */
	if (!idle_filter(acct))
	{
		gaim_debug_misc("idle",
				"%s hasn't been idled yet; adding to list.\n",
				gaim_account_get_username(acct));
		idled_accts = g_list_append(idled_accts, acct);
	}

	set_idle_time(acct, tm);
}

static void
unidle_action_ok(void *ignored, GaimRequestFields *fields)
{
	GaimAccount *acct = gaim_request_fields_get_account(fields, "acct");

	set_idle_time(acct, 0); /* unidle the account */

	/* once the account has been unidled it shouldn't be in the list */
	idled_accts = g_list_remove(idled_accts, acct);
}


static void
idle_action(GaimPluginAction *action)
{
	/* Use the super fancy request API */

	GaimRequestFields *request;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	group = gaim_request_field_group_new(NULL);

	field = gaim_request_field_account_new("acct", _("Account"), NULL);
	gaim_request_field_account_set_show_all(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_int_new("mins", _("Minutes"), 10);
	gaim_request_field_group_add_field(group, field);

	request = gaim_request_fields_new();
	gaim_request_fields_add_group(request, group);

	gaim_request_fields(action->plugin,
			N_("I'dle Mak'er"),
			_("Set Account Idle Time"),
			NULL,
			request,
			_("_Set"), G_CALLBACK(idle_action_ok),
			_("_Cancel"), NULL,
			NULL);
}

static void
unidle_action(GaimPluginAction *action)
{
	GaimRequestFields *request;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	if (idled_accts == NULL)
	{
		gaim_notify_info(NULL, NULL, _("None of your accounts are idle."), NULL);
		return;
	}

	group = gaim_request_field_group_new(NULL);

	field = gaim_request_field_account_new("acct", _("Account"), NULL);
	gaim_request_field_account_set_filter(field, idle_filter);
	gaim_request_field_account_set_show_all(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	request = gaim_request_fields_new();
	gaim_request_fields_add_group(request, group);

	gaim_request_fields(action->plugin,
			N_("I'dle Mak'er"),
			_("Unset Account Idle Time"),
			NULL,
			request,
			_("_Unset"), G_CALLBACK(unidle_action_ok),
			_("_Cancel"), NULL,
			NULL);
}

static void
unidle_all_action(GaimPluginAction *action)
{
	GList *l;

	/* freeing the list here will cause segfaults if the user idles an account
	 * after the list is freed */
	for (l = idled_accts; l; l = l->next)
	{
		GaimAccount *account = l->data;
		set_idle_time(account, 0);
	}

	g_list_free(idled_accts);
	idled_accts = NULL;
}

static GList *
actions(GaimPlugin *plugin, gpointer context)
{
	GList *l = NULL;
	GaimPluginAction *act = NULL;

	act = gaim_plugin_action_new(_("Set account idle time"),
			idle_action);
	l = g_list_append(l, act);

	act = gaim_plugin_action_new(_("Unset account idle time"),
			unidle_action);
	l = g_list_append(l, act);

	act = gaim_plugin_action_new(
			_("Unset idle time for all idled accounts"), unidle_all_action);
	l = g_list_append(l, act);

	return l;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	unidle_all_action(NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	IDLE_PLUGIN_ID,
	N_("I'dle Mak'er"),
	VERSION,
	N_("Allows you to hand-configure how long you've been idle"),
	N_("Allows you to hand-configure how long you've been idle"),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	GAIM_WEBSITE,
	NULL,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	actions
};


static void
init_plugin(GaimPlugin *plugin)
{
}


GAIM_INIT_PLUGIN(idle, init_plugin, info)

