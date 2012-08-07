#include "multilogon.h"

#include <debug.h>

#include "gg.h"

struct _ggp_multilogon_session_data
{
	int session_count;
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

void ggp_multilogon_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	
	ggp_multilogon_session_data *mldata = g_new0(ggp_multilogon_session_data, 1);
	accdata->multilogon_data = mldata;
}

void ggp_multilogon_cleanup(PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	g_free(mldata);
}

void ggp_multilogon_msg(PurpleConnection *gc, struct gg_event_msg *msg)
{
	ggp_recv_message_handler(gc, msg, TRUE);
}

void ggp_multilogon_info(PurpleConnection *gc,
	struct gg_event_multilogon_info *info)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	int i;
	
	purple_debug_info("gg", "ggp_multilogon_info: session list changed\n");
	for (i = 0; i < info->count; i++)
	{
		purple_debug_misc("gg", "ggp_multilogon_info: "
			"session [%s] logged in at %lu\n",
			info->sessions[i].name,
			info->sessions[i].logon_time);
	}

	mldata->session_count = info->count;
}

int ggp_multilogon_get_session_count(PurpleConnection *gc)
{
	ggp_multilogon_session_data *mldata = ggp_multilogon_get_mldata(gc);
	return mldata->session_count;
}
