/*
 * nmmessage.c
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

#include "nmmessage.h"

struct _NMMessage
{
	NMConference *conference;
	char *text;
	gpointer data;
	guint32 ref_count;
};


/** Message API **/

NMMessage *
nm_create_message(const char *text)
{
	NMMessage *msg = g_new0(NMMessage, 1);

	if (text)
		msg->text = g_strdup(text);

	msg->ref_count = 1;
	return msg;
}

void
nm_release_message(NMMessage * msg)
{
	if (msg && (--(msg->ref_count) == 0)) {
		if (msg->text)
			g_free(msg->text);

		if (msg->conference)
			nm_release_conference(msg->conference);

		g_free(msg);
	}
}

const char *
nm_message_get_text(NMMessage * msg)
{
	if (msg == NULL)
		return NULL;

	return msg->text;
}

void
nm_message_set_conference(NMMessage * msg, NMConference * conf)
{
	if (msg == NULL || conf == NULL)
		return;

	/* Need to ref the conference first so that it doesn't
	 * get released out from under us
	 */
	nm_conference_add_ref(conf);

	msg->conference = conf;
}

NMConference *
nm_message_get_conference(NMMessage * msg)
{
	if (msg == NULL)
		return NULL;

	return msg->conference;
}
