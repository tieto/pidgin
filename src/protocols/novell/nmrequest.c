/*
 * nmrequest.c
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

#include "nmrequest.h"

struct _NMRequest
{
	int trans_id;
	char *cmd;
	int gmt;
	gpointer data;
	gpointer user_define;
	nm_response_cb callback;
	int ref_count;
	NMERR_T ret_code;
};


NMRequest *
nm_create_request(const char *cmd, int trans_id, int gmt)
{
	NMRequest *req;

	if (cmd == NULL)
		return NULL;

	req  = g_new0(NMRequest, 1);
	req->cmd = g_strdup(cmd);
	req->trans_id = trans_id;
	req->gmt = gmt;
	req->ref_count = 1;

	return req;
}

void
nm_release_request(NMRequest * req)
{
	if (req && (--req->ref_count == 0)) {
		if (req->cmd)
			g_free(req->cmd);
		g_free(req);
	}
}

void
nm_request_add_ref(NMRequest * req)
{
	if (req)
		req->ref_count++;
}

void
nm_request_set_callback(NMRequest * req, nm_response_cb callback)
{
	if (req)
		req->callback = callback;
}

void
nm_request_set_data(NMRequest * req, gpointer data)
{
	if (req)
		req->data = data;
}

void
nm_request_set_user_define(NMRequest * req, gpointer user_define)
{
	if (req)
		req->user_define = user_define;
}

int
nm_request_get_trans_id(NMRequest * req)
{
	if (req)
		return req->trans_id;
	else
		return -1;
}

const char *
nm_request_get_cmd(NMRequest * req)
{
	if (req == NULL)
		return NULL;

	return req->cmd;
}

gpointer
nm_request_get_data(NMRequest * req)
{
	if (req == NULL)
		return NULL;

	return req->data;
}

gpointer
nm_request_get_user_define(NMRequest * req)
{
	if (req == NULL)
		return NULL;

	return req->user_define;
}

nm_response_cb
nm_request_get_callback(NMRequest * req)
{
	if (req == NULL)
		return NULL;

	return req->callback;
}


void
nm_request_set_ret_code(NMRequest * req, NMERR_T rc)
{
	if (req)
		req->ret_code = rc;
}

NMERR_T
nm_request_get_ret_code(NMRequest * req)
{
	if (req)
		return req->ret_code;
	else
		return (NMERR_T) - 1;
}
