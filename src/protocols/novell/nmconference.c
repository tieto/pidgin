/*
 * nmconference.c
 *
 * Copyright © 2004 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK OF NOVELL, INC. NO PART OF THIS WORK MAY BE
 * USED, PRACTICED, PERFORMED, COPIED, DISTRIBUTED, REVISED, MODIFIED,
 * TRANSLATED, ABRIDGED, CONDENSED, EXPANDED, COLLECTED, COMPILED, LINKED,
 * RECAST, TRANSFORMED OR ADAPTED WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL,
 * INC. ANY USE OR EXPLOITATION OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 *
 * AS BETWEEN [GAIM] AND NOVELL, NOVELL GRANTS [GAIM] THE RIGHT TO REPUBLISH
 * THIS WORK UNDER THE GPL (GNU GENERAL PUBLIC LICENSE) WITH ALL RIGHTS AND
 * LICENSES THEREUNDER.  IF YOU HAVE RECEIVED THIS WORK DIRECTLY OR INDIRECTLY
 * FROM [GAIM] AS PART OF SUCH A REPUBLICATION, YOU HAVE ALL RIGHTS AND LICENSES
 * GRANTED BY [GAIM] UNDER THE GPL.  IN CONNECTION WITH SUCH A REPUBLICATION, IF
 * ANYTHING IN THIS NOTICE CONFLICTS WITH THE TERMS OF THE GPL, SUCH TERMS
 * PREVAIL.
 *
 */

#include <string.h>
#include "nmconference.h"

static int conf_count = 0;

struct _NMConference
{

	/* The conference identifier */
	char *guid;

	/* The list of participants for the conference */
	GSList *participants;

	/* Flags for the conference */
	guint32 flags;

	/* User defined data */
	gpointer data;

	/* Reference count for this object */
	int ref_count;

};


/*******************************************************************************
 * Conference API -- see header file for comments
 ******************************************************************************/

NMConference *
nm_create_conference(const char *guid)
{
	NMConference *conf = g_new0(NMConference, 1);

	if (guid) {
		conf->guid = g_strdup(guid);
	} else {
		conf->guid = g_strdup(BLANK_GUID);
	}
	conf->ref_count = 1;

	gaim_debug(GAIM_DEBUG_INFO, "novell",
			   "Creating a conference 0x%x, total=%d\n",
			   (guint32) conf, conf_count++);

	return conf;
}

void
nm_release_conference(NMConference * conference)
{
	GSList *node;

	gaim_debug(GAIM_DEBUG_INFO, "novell",
			   "In release conference 0x%x, refs=%d\n",
			   (guint32) conference, conference->ref_count);
	if (conference != NULL && (--conference->ref_count == 0)) {

		gaim_debug(GAIM_DEBUG_INFO, "novell",
				   "Releasing conference 0x%x, total=%d\n",
				   (guint32) conference, --conf_count);

		if (conference->guid)
			g_free(conference->guid);

		if (conference->participants) {
			for (node = conference->participants; node; node = node->next) {
				if (node->data) {
					NMUserRecord *user_record = node->data;

					nm_release_user_record(user_record);
					node->data = NULL;
				}
			}

			g_slist_free(conference->participants);
		}

		g_free(conference);
	}
}

gboolean
nm_conference_is_instantiated(NMConference * conference)
{
	if (conference == NULL)
		return FALSE;

	return (strncmp(conference->guid, BLANK_GUID, CONF_GUID_END) != 0);
}

int
nm_conference_get_participant_count(NMConference * conference)
{
	if (conference == NULL)
		return 0;

	return g_slist_length(conference->participants);
}

NMUserRecord *
nm_conference_get_participant(NMConference * conference, int index)
{
	if (conference == NULL)
		return NULL;

	return (NMUserRecord *) g_slist_nth_data(conference->participants, index);
}

void
nm_conference_add_participant(NMConference * conference,
							  NMUserRecord * user_record)
{
	if (conference == NULL || user_record == NULL) {
		return;
	}

	nm_user_record_add_ref(user_record);
	conference->participants = g_slist_append(conference->participants, user_record);
}

void
nm_conference_remove_participant(NMConference * conference, const char *dn)
{
	GSList *node, *element = NULL;

	if (conference == NULL || dn == NULL) {
		return;
	}

	for (node = conference->participants; node; node = node->next) {
		NMUserRecord *user_record = node->data;

		if (user_record) {
			if (nm_utf8_str_equal(dn, nm_user_record_get_dn(user_record))) {
				element = node;
				break;
			}
		}
	}

	if (element) {
		nm_release_user_record((NMUserRecord *) element->data);
		element->data = NULL;
		conference->participants =
			g_slist_remove_link(conference->participants, element);
		g_slist_free_1(element);
	}
}

void
nm_conference_add_ref(NMConference * conference)
{
	if (conference)
		conference->ref_count++;
}

void
nm_conference_set_flags(NMConference * conference, guint32 flags)
{
	if (conference) {
		conference->flags = flags;
	}
}

void
nm_conference_set_guid(NMConference * conference, const char *guid)
{
	if (conference) {

		/* Release memory for old guid */
		if (conference->guid) {
			g_free(conference->guid);
		}

		/* Set the new guid */
		if (guid)
			conference->guid = g_strdup(guid);
		else
			conference->guid = g_strdup(BLANK_GUID);
	}
}

void
nm_conference_set_data(NMConference * conference, gpointer data)
{
	if (conference == NULL)
		return;

	conference->data = data;
}

gpointer
nm_conference_get_data(NMConference * conference)
{
	if (conference == NULL)
		return NULL;

	return conference->data;
}

const char *
nm_conference_get_guid(NMConference * conference)
{
	if (conference == NULL)
		return NULL;

	return conference->guid;
}
