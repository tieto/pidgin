/* iChat-like timestamps by Sean Egan.
 * <INSERT GPL HERE> */

#define GAIM_PLUGINS
#include <time.h>
#include "gaim.h"
#include "gtkimhtml.h"

#define TIMESTAMP_DELAY (5 * 60 * 1000)

GModule *handle;
GSList *timestamp_timeouts;

gboolean do_timestamp (struct conversation *c)
{
	char *buf;
	char mdate[6];
	time_t tim = time(NULL);
	
	if (!g_list_find(conversations, c))
		return FALSE;

	strftime(mdate, sizeof(mdate), "%H:%M", localtime(&tim));
	buf = g_strdup_printf("            %s", mdate);
	write_to_conv(c, buf, WFLAG_NOLOG, NULL, tim, -1);
	g_free(buf);
	return TRUE;
}

void timestamp_new_convo(char *name)
{
	struct conversation *c = find_conversation(name);
	do_timestamp(c);

	timestamp_timeouts = g_slist_append(timestamp_timeouts,
			GINT_TO_POINTER(g_timeout_add(TIMESTAMP_DELAY, do_timestamp, c)));

}
char *gaim_plugin_init(GModule *h) {
	GList *cnvs = conversations;
	struct conversation *c;
	handle = h;

	while (cnvs) {
		c = cnvs->data;
		timestamp_new_convo(c->name);
		cnvs = cnvs->next;
	}
	gaim_signal_connect(handle, event_new_conversation, timestamp_new_convo, NULL);

	return NULL;
}

void gaim_plugin_remove() {
	GSList *to;
	to = timestamp_timeouts;
	while (to) {
		g_source_remove(GPOINTER_TO_INT(to->data));
		to = to->next;
	}
	g_slist_free(timestamp_timeouts);
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Timestamp"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Adds iChat-style timestamps to conversations every 5 minutes."));
	desc.authors = g_strdup("Sean Egan &lt;bj91704@binghamton.edu>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
