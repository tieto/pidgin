/* a nifty little plugin to set your idle time to whatever you want it to be.
 * useful for almost nothing. mostly just a demo plugin. but it's fun to have
 * 40-day idle times.
 */

#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "multi.h"
#include "plugin.h"
#include "request.h"
#include "server.h"

#define IDLE_PLUGIN_ID "gtk-idle"


static void
idle_action_ok(void *ignored, GaimRequestFields *fields)
{
	time_t t;
	int tm;
	GaimAccount *acct;
	GaimConnection *gc;

	tm = gaim_request_fields_get_integer(fields, "mins");
	acct = gaim_request_fields_get_account(fields, "acct");
	gc = gaim_account_get_connection(acct);

	gaim_debug(GAIM_DEBUG_INFO, "idle",
			"setting idle time for %s to %d\n",
			gaim_account_get_username(acct), tm);
	time(&t);
	t -= 60 * tm;
	gc->last_sent_time = t;
	serv_set_idle(gc, 60 * tm);
	gc->is_idle = 0;
}


static void
idle_action(GaimPlugin *plugin)
{
	/* Use the super fancy request API */

	GaimRequestFields *request;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	group = gaim_request_field_group_new(NULL);

	field = gaim_request_field_account_new("acct", "Account", NULL);
	gaim_request_field_account_set_show_all(field, FALSE);
	gaim_request_field_group_add_field(group, field);
	
	field = gaim_request_field_int_new("mins", "Minutes", 10);
	gaim_request_field_group_add_field(group, field);

	request = gaim_request_fields_new();
	gaim_request_fields_add_group(request, group);

	gaim_request_fields(plugin,
			N_("I'dle Mak'er"),
			_("Set Account Idle Time"),
			NULL,
			request,
			_("_Set"), G_CALLBACK(idle_action_ok),
			_("_Cancel"), NULL,
			NULL);
}


static GList *
actions(GaimPlugin *plugin)
{
	GList *l = NULL;
	struct plugin_actions_menu *pam;

	pam = g_new0(struct plugin_actions_menu, 1);
	pam->label = _("Set Account Idle Time");
	pam->callback = idle_action;
	pam->plugin = plugin;
	l = g_list_append(l, pam);

	return l;
}


static GaimPluginInfo info =
{
	GAIM_PLUGIN_API_VERSION,
	GAIM_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	IDLE_PLUGIN_ID,
	N_("I'dle Mak'er"),
	VERSION,
	N_("Allows you to hand-configure how long you've been idle for"),
	N_("Allows you to hand-configure how long you've been idle for"),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	GAIM_WEBSITE,
	NULL,
	NULL,
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

