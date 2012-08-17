#include "status.h"

#include <libgadu.h>
#include <debug.h>
#include <request.h>

#include "gg.h"
#include "utils.h"

struct _ggp_status_session_data
{
	gboolean status_broadcasting;
	gchar *current_description;
};

static inline ggp_status_session_data *
ggp_status_get_ssdata(PurpleConnection *gc);

static gchar * ggp_status_validate_description(const gchar* msg);

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

static gchar * ggp_status_validate_description(const gchar* msg)
{
	if (msg == NULL || msg[0] == '\0')
		return NULL;
	
	return ggp_utf8_strndup(msg, GG_STATUS_DESCR_MAXSIZE);
}

GList * ggp_status_types(PurpleAccount *account)
{
	GList *types = NULL;

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_AVAILABLE, NULL, NULL,
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_AVAILABLE, "freeforchat", _("Chatty"),
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_AWAY, NULL, NULL,
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_UNAVAILABLE, NULL, NULL,
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_INVISIBLE, NULL, NULL,
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	types = g_list_append(types, purple_status_type_new_with_attrs(
		PURPLE_STATUS_OFFLINE, NULL, NULL,
		TRUE, TRUE, FALSE, "message", _("Message"),
		purple_value_new(PURPLE_TYPE_STRING), NULL));

	return types;
}

int ggp_status_from_purplestatus(PurpleStatus *status, gchar **message)
{
	const char *status_id = purple_status_get_id(status);
	const char *status_message =
		purple_status_get_attr_string(status, "message");
	
	g_return_val_if_fail(message != NULL, 0);
	
	*message = NULL;
	if (status_message)
	{
		gchar *stripped = purple_markup_strip_html(status_message);
		*message = ggp_status_validate_description(stripped);
		g_free(stripped);
	}
	
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

const gchar * ggp_status_to_purplestatus(int status)
{
	switch (status)
	{
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
		case GG_STATUS_BLOCKED:
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_OFFLINE);
		case GG_STATUS_FFC:
		case GG_STATUS_FFC_DESCR:
			return "freeforchat";
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_AVAILABLE);
		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_AWAY);
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_INVISIBLE);
		case GG_STATUS_DND:
		case GG_STATUS_DND_DESCR:
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_UNAVAILABLE);
		default:
			purple_debug_warning("gg", "ggp_status_to_purplestatus: unknown status %d\n", status);
			return purple_primitive_get_id_from_type(
				PURPLE_STATUS_AVAILABLE);
	}
}

const gchar * ggp_status_get_name(const gchar *purple_status)
{
	if (g_strcmp0(purple_status, "freeforchat") == 0)
		return _("Chatty");
	return purple_primitive_get_name_from_type(
		purple_primitive_get_type_from_id(purple_status));
}

/*******************************************************************************
 * Own status.
 ******************************************************************************/

static void ggp_status_broadcasting_dialog_ok(PurpleConnection *gc,
	PurpleRequestFields *fields);

/******************************************************************************/

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
	gchar *new_description = ggp_status_validate_description(msg);
	
	if (!ssdata->status_broadcasting)
		status |= GG_STATUS_FRIENDS_MASK;
	
	if ((status == GG_STATUS_NOT_AVAIL ||
		status == GG_STATUS_NOT_AVAIL_DESCR) &&
		0 == g_strcmp0(ssdata->current_description, new_description))
	{
		purple_debug_info("gg", "ggp_status_set: new status doesn't "
			"differ when closing connection - ignore\n");
		g_free(new_description);
		return FALSE;
	}
	g_free(ssdata->current_description);
	ssdata->current_description = new_description;
	
	if (msg == NULL)
		gg_change_status(accdata->session, status);
	else
		gg_change_status_descr(accdata->session, status, new_description);
	
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

void ggp_status_set_disconnected(PurpleAccount *account)
{
	gchar *msg = NULL;
	
	ggp_status_from_purplestatus(purple_account_get_active_status(account),
		&msg);
	if (!ggp_status_set(account,
		msg ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL, msg))
	{
		g_free(msg);
		return;
	}
	
	/*
	struct gg_event *ev;
	guint64 wait_start = ggp_microtime(), now;
	int sleep_time = 5000;
	while ((ev = gg_watch_fd(info->session)) != NULL)
	{
		if (ev->type == GG_EVENT_DISCONNECT_ACK)
			break;
		now = ggp_microtime();
		if (now - wait_start + sleep_time >= 100000)
			break;
		usleep(sleep_time);
		sleep_time *= 2;
	}
	*/
	usleep(100000);
	
	g_free(msg);
}

void ggp_status_fake_to_self(PurpleConnection *gc)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleStatus *status = purple_presence_get_active_status(
		purple_account_get_presence(account));
	const char *status_msg = purple_status_get_attr_string(status,
		"message");
	gchar *status_msg_gg = NULL;
	
	if (status_msg != NULL && status_msg[0] != '\0')
	{
		status_msg_gg = g_new0(gchar, GG_STATUS_DESCR_MAXSIZE + 1);
		g_utf8_strncpy(status_msg_gg, status_msg,
			GG_STATUS_DESCR_MAXSIZE);
	}
	
	purple_prpl_got_user_status(account,
		purple_account_get_username(account),
		purple_status_get_id(status),
		status_msg_gg ? "message" : NULL, status_msg_gg, NULL);
	
	g_free(status_msg_gg);
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

/*******************************************************************************
 * Buddy status.
 ******************************************************************************/

void ggp_status_got_others_buddy(PurpleConnection *gc, uin_t uin, int status,
	const char *descr);

/******************************************************************************/

void ggp_status_got_others(PurpleConnection *gc, struct gg_event *ev)
{
	if (ev->type == GG_EVENT_NOTIFY60)
	{
		struct gg_event_notify60 *notify = ev->event.notify60;
		int i;
		for (i = 0; notify[i].uin; i++)
			ggp_status_got_others_buddy(gc, notify[i].uin,
				GG_S(notify[i].status), notify[i].descr);
	}
	else if (ev->type == GG_EVENT_STATUS60)
	{
		struct gg_event_status60 *notify = &ev->event.status60;
		ggp_status_got_others_buddy(gc, notify->uin,
			GG_S(notify->status), notify->descr);
	}
	else
		purple_debug_fatal("gg", "ggp_status_got_others: "
			"unexpected event %d\n", ev->type);
}

void ggp_status_got_others_buddy(PurpleConnection *gc, uin_t uin, int status,
	const char *descr)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBuddy *buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
	const gchar *purple_status = ggp_status_to_purplestatus(status);
	gchar *status_message = NULL;
	
	if (!buddy)
	{
		purple_debug_warning("gg", "ggp_status_got_others_buddy: "
			"buddy %u not found\n", uin);
		return;
	}
	ggp_buddy_get_data(buddy)->blocked = (status == GG_STATUS_BLOCKED);
	
	if (descr != NULL)
	{
		status_message = g_strdup(descr);
		g_strstrip(status_message);
		if (status_message[0] == '\0')
		{
			g_free(status_message);
			status_message = NULL;
		}
	}
	
	if (uin == ggp_str_to_uin(purple_account_get_username(account)))
	{
		purple_debug_info("gg", "ggp_status_got_others_buddy: "
			"own status changed to %s [%s]\n",
			purple_status, status_message ? status_message : "");
	}
	else
	{
		purple_debug_misc("gg", "ggp_status_got_others_buddy: "
			"status of %u changed to %s [%s]\n", uin,
			purple_status, status_message ? status_message : "");
	}
	
	if (status_message)
	{
		purple_prpl_got_user_status(account, ggp_uin_to_str(uin),
			purple_status, "message", status_message, NULL);
	}
	else
	{
		purple_prpl_got_user_status(account, ggp_uin_to_str(uin),
			purple_status, NULL);
	}
	
	g_free(status_message);
}

char * ggp_status_buddy_text(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = ggp_buddy_get_data(buddy);
	const gchar *purple_message;
	
	if (buddy_data->blocked)
		return g_strdup(_("Blocked"));
	
	purple_message = purple_status_get_attr_string(
		purple_presence_get_active_status(
			purple_buddy_get_presence(buddy)), "message");
	if (!purple_message)
		return NULL;
	
	return g_markup_escape_text(purple_message, -1);
}
