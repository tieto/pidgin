#include "status.h"

#include <libgadu.h>
#include <debug.h>
#include <request.h>

#include "gg.h"

struct _ggp_status_session_data
{
	gboolean status_broadcasting;
	gchar *current_description;
};

static inline ggp_status_session_data *
ggp_status_get_ssdata(PurpleConnection *gc);

static int ggp_status_from_purplestatus(PurpleStatus *status, gchar **message);

static void ggp_status_broadcasting_dialog_ok(PurpleConnection *gc,
	PurpleRequestFields *fields);

////

static inline ggp_status_session_data *
ggp_status_get_ssdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->status_data;
}

void ggp_status_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);

	ggp_status_session_data *ssdata = g_new0(ggp_status_session_data, 1);
	accdata->status_data = ssdata;
	
	ssdata->status_broadcasting =
		purple_account_get_bool(account, "status_broadcasting", TRUE);
}

void ggp_status_cleanup(PurpleConnection *gc)
{
	ggp_status_session_data *ssdata = ggp_status_get_ssdata(gc);
	g_free(ssdata->current_description);
	g_free(ssdata);
}

static int ggp_status_from_purplestatus(PurpleStatus *status, gchar **message)
{
	const char *status_id = purple_status_get_id(status);
	const char *status_message =
		purple_status_get_attr_string(status, "message");
	
	g_return_val_if_fail(message != NULL, 0);
	
	*message = NULL;
	if (status_message)
		*message = purple_markup_strip_html(status_message);
	
	if (0 == strcmp(status_id, "available"))
		return status_message ? GG_STATUS_AVAIL_DESCR : GG_STATUS_AVAIL;
	if (0 == strcmp(status_id, "freeforchat"))
		return status_message ? GG_STATUS_FFC_DESCR : GG_STATUS_FFC;
	if (0 == strcmp(status_id, "away"))
		return status_message ? GG_STATUS_BUSY_DESCR : GG_STATUS_BUSY;
	if (0 == strcmp(status_id, "unavailable"))
		return status_message ? GG_STATUS_DND_DESCR : GG_STATUS_DND;
	if (0 == strcmp(status_id, "invisible"))
		return status_message ?
			GG_STATUS_INVISIBLE_DESCR : GG_STATUS_INVISIBLE;
	if (0 == strcmp(status_id, "offline"))
		return status_message ?
			GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL;
	
	purple_debug_error("gg", "ggp_status_from_purplestatus: "
		"unknown status requested (%s)\n", status_id);
	return status_message ? GG_STATUS_AVAIL_DESCR : GG_STATUS_AVAIL;
}

/*******************************************************************************
 * Own status.
 ******************************************************************************/

void ggp_status_set_initial(PurpleConnection *gc, struct gg_login_params *glp)
{
	ggp_status_session_data *ssdata = ggp_status_get_ssdata(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	
	glp->status = ggp_status_from_purplestatus(
		purple_account_get_active_status(account), &glp->status_descr);
	if (!ggp_status_get_status_broadcasting(gc))
		glp->status |= GG_STATUS_FRIENDS_MASK;
	ssdata->current_description = g_strdup(glp->status_descr);
}

gboolean ggp_status_set(PurpleAccount *account, int status, const gchar* msg)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	ggp_status_session_data *ssdata = ggp_status_get_ssdata(gc);
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	
	if (!ssdata->status_broadcasting)
		status |= GG_STATUS_FRIENDS_MASK;
	
	if ((status == GG_STATUS_NOT_AVAIL ||
		status == GG_STATUS_NOT_AVAIL_DESCR) &&
		0 == g_strcmp0(ssdata->current_description, msg))
	{
		purple_debug_info("gg", "ggp_status_set: new status doesn't "
			"differ when closing connection - ignore\n");
		return FALSE;
	}
	g_free(ssdata->current_description);
	ssdata->current_description = g_strdup(msg);
	
	if (msg == NULL)
		gg_change_status(accdata->session, status);
	else
		gg_change_status_descr(accdata->session, status, msg);
	
	return TRUE;
}

void ggp_status_set_purplestatus(PurpleAccount *account, PurpleStatus *status)
{
	int status_gg;
	gchar *msg = NULL;
	
	if (!purple_status_is_active(status))
		return;
	
	status_gg = ggp_status_from_purplestatus(status, &msg);
	ggp_status_set(account, status_gg, msg);
	g_free(msg);
}

gboolean ggp_status_get_status_broadcasting(PurpleConnection *gc)
{
	return ggp_status_get_ssdata(gc)->status_broadcasting;
}

void ggp_status_set_status_broadcasting(PurpleConnection *gc,
	gboolean broadcasting)
{
	PurpleAccount *account = purple_connection_get_account(gc);

	ggp_status_get_ssdata(gc)->status_broadcasting = broadcasting;
	purple_account_set_bool(account, "status_broadcasting", broadcasting);
	ggp_status_set_purplestatus(account,
		purple_account_get_active_status(account));
}

void ggp_status_broadcasting_dialog(PurpleConnection *gc)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	
	field = purple_request_field_bool_new("buddies_only",
		_("Show status only for buddies"),
		!ggp_status_get_status_broadcasting(gc));
	purple_request_field_group_add_field(group, field);
	
	purple_request_fields(gc,
		_("Change status broadcasting"),
		_("Please, select who can see your status"),
		NULL,
		fields,
		_("OK"), G_CALLBACK(ggp_status_broadcasting_dialog_ok),
		_("Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL, gc);
}

static void ggp_status_broadcasting_dialog_ok(PurpleConnection *gc,
	PurpleRequestFields *fields)
{
	ggp_status_set_status_broadcasting(gc,
		!purple_request_fields_get_bool(fields, "buddies_only"));
}
