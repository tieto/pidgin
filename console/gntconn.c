#include "notify.h"

#include "gntconn.h"
#include "gntgaim.h"

static void
gg_connection_report_disconnect(GaimConnection *gc, const char *text)
{
	char *act, *primary, *secondary;
	GaimAccount *account = gaim_connection_get_account(gc);

	act = g_strdup_printf(_("%s (%s)"), gaim_account_get_username(account),
			gaim_account_get_protocol_name(account));

	primary = g_strdup_printf(_("%s disconnected."), act);
	secondary = g_strdup_printf(_("%s was disconnected due to the following error:\n%s"),
			act, text);

	gaim_notify_error(account, _("Connection Error"), primary, secondary);

	g_free(act);
	g_free(primary);
	g_free(secondary);
}

static GaimConnectionUiOps ops = 
{
	.connect_progress = NULL,
	.connected = NULL,
	.disconnected = NULL,
	.notice = NULL,
	.report_disconnect = gg_connection_report_disconnect
};

GaimConnectionUiOps *gg_connections_get_ui_ops()
{
	return &ops;
}

void gg_connections_init()
{}

void gg_connections_uninit()
{}

