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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#include "multilogon.h"

#include <debug.h>

#include "gg.h"
#include "keymapper.h"
#include "utils.h"
#include "message-prpl.h"

typedef struct
{
	uint64_t id;
	uint32_t remote_addr;
	gchar *name;
	time_t logon_time;
} ggp_multilogon_session_info;

struct _ggp_multilogon_session_data
{
	int session_count;
	ggp_multilogon_session_info *sessions;
	PurpleRequestDatasheet *sheet_handle;
	gpointer dialog_handle;
	ggp_keymapper *sid_mapper;
};

static inline ggp_multilogon_session_data *
ggp_multilogon_get_mldata(PurpleConnection *gc);

////////////

static inline ggp_multilogon_session_data *
ggp_multilogon_get_mldata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->multilogon_data;
}

void
ggp_multilogon_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);

	ggp_multilogon_session_data *mldata =
		g_new0(ggp_multilogon_session_data, 1);
	accdata->multilogon_data = mldata;

	mldata->sid_mapper = ggp_keymapper_new();
}

static void
ggp_multilogon_free_sessions(PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	int i;

	for (i = 0; i < mldata->session_count; i++)
		g_free(mldata->sessions[i].name);
	g_free(mldata->sessions);

	mldata->sessions = NULL;
	mldata->session_count = 0;
}

void
ggp_multilogon_cleanup(PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);

	if (mldata->dialog_handle) {
		purple_request_close(PURPLE_REQUEST_FIELDS,
			mldata->dialog_handle);
		mldata->dialog_handle = NULL;
	}

	ggp_multilogon_free_sessions(gc);
	ggp_keymapper_free(mldata->sid_mapper);
	g_free(mldata);
}

static uint64_t
ggp_multilogon_sid_from_libgadu(gg_multilogon_id_t lsid)
{
	uint64_t sid;

	memcpy(&sid, lsid.id, sizeof(uint64_t));

	return sid;
}

static gg_multilogon_id_t
ggp_multilogon_sid_to_libgadu(uint64_t sid)
{
	gg_multilogon_id_t lsid;

	memcpy(lsid.id, &sid, sizeof(uint64_t));

	return lsid;
}

void
ggp_multilogon_fill_sessions(PurpleRequestDatasheet *sheet,
	PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	ggp_keymapper *km = mldata->sid_mapper;
	int i;

	purple_request_datasheet_record_mark_all_for_rem(sheet);

	for (i = 0; i < mldata->session_count; i++) {
		ggp_multilogon_session_info *sess = &mldata->sessions[i];
		PurpleRequestDatasheetRecord *rec;

		rec = purple_request_datasheet_record_add(sheet,
			ggp_keymapper_to_key(km, sess->id));

		purple_request_datasheet_record_set_string_data(rec, 0,
			ggp_ipv4_to_str(sess->remote_addr));
		purple_request_datasheet_record_set_string_data(rec, 1,
			purple_date_format_full(localtime(&sess->logon_time)));
		purple_request_datasheet_record_set_string_data(rec, 2,
			sess->name);
	}

	purple_request_datasheet_record_remove_marked(sheet);
}

void
ggp_multilogon_info(PurpleConnection *gc, struct gg_event_multilogon_info *info)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	int i;

	ggp_multilogon_free_sessions(gc);

	purple_debug_info("gg", "ggp_multilogon_info: session list changed "
		"(count now: %d)\n", info->count);

	mldata->sessions = g_new(ggp_multilogon_session_info, info->count);
	for (i = 0; i < info->count; i++) {
		struct gg_multilogon_session *lsess = &info->sessions[i];
		ggp_multilogon_session_info *psess = &mldata->sessions[i];

		psess->id = ggp_multilogon_sid_from_libgadu(lsess->id);
		psess->remote_addr = lsess->remote_addr;
		psess->name = g_strdup(lsess->name);
		psess->logon_time = lsess->logon_time;
	}

	mldata->session_count = info->count;

	if (mldata->sheet_handle != NULL)
		ggp_multilogon_fill_sessions(mldata->sheet_handle, gc);
}

void
ggp_multilogon_disconnect(PurpleRequestDatasheetRecord *rec, gpointer _gc)
{
	PurpleConnection *gc = _gc;
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	uint64_t sid;
	gpointer key;

	key = purple_request_datasheet_record_get_key(rec);
	sid = ggp_keymapper_from_key(mldata->sid_mapper, key);

	gg_multilogon_disconnect(accdata->session,
		ggp_multilogon_sid_to_libgadu(sid));

	purple_request_datasheet_record_remove(
		purple_request_datasheet_record_get_datasheet(rec), key);
}

void
ggp_multilogon_dialog(PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	PurpleRequestField *field;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestCommonParameters *cpar;
	PurpleRequestDatasheet *sheet;
	PurpleRequestDatasheetAction *action;
	gpointer dialog_handle;

	if (mldata->dialog_handle != NULL)
		return;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	sheet = purple_request_datasheet_new();
	purple_request_datasheet_add_column(sheet,
		PURPLE_REQUEST_DATASHEET_COLUMN_STRING, _("IP"));
	purple_request_datasheet_add_column(sheet,
		PURPLE_REQUEST_DATASHEET_COLUMN_STRING, _("Logon time"));
	purple_request_datasheet_add_column(sheet,
		PURPLE_REQUEST_DATASHEET_COLUMN_STRING, _("Session"));

	action = purple_request_datasheet_action_new();
	purple_request_datasheet_action_set_label(action, _("Disconnect"));
	purple_request_datasheet_action_set_cb(action,
		ggp_multilogon_disconnect, gc);
	purple_request_datasheet_add_action(sheet, action);
	ggp_multilogon_fill_sessions(sheet, gc);

	field = purple_request_field_datasheet_new("sessions", NULL, sheet);
	purple_request_field_group_add_field(group, field);

	cpar = purple_request_cpar_new();
	purple_request_cpar_set_icon(cpar, PURPLE_REQUEST_ICON_DIALOG);

	dialog_handle = purple_request_fields(gc,
		_("Other Gadu-Gadu sessions"), NULL, NULL, fields,
		NULL, NULL, _("Close"), NULL,
		cpar, NULL);
	mldata->sheet_handle = sheet;
	mldata->dialog_handle = dialog_handle;

	purple_request_add_close_notify(dialog_handle,
		purple_callback_set_zero, &mldata->sheet_handle);
	purple_request_add_close_notify(dialog_handle,
		purple_callback_set_zero, &mldata->dialog_handle);
}
