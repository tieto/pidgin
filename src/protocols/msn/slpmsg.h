/**
 * @file slpmsg.h SLP Message functions
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _MSN_SLPMSG_H_
#define _MSN_SLPMSG_H_

typedef struct _MsnSlpMessage MsnSlpMessage;

#include "slpsession.h"
#include "slpcall.h"
#include "slplink.h"
#include "session.h"
#include "msg.h"

#include "slp.h"

struct _MsnSlpMessage
{
	MsnSlpSession *slpsession;
	MsnSlpCall *slpcall;
	MsnSlpLink *slplink;
	MsnSession *session;

	long session_id;
	long id;
	long ack_id;
	long ack_sub_id;
	long long ack_size;
	long app_id;

	gboolean sip;
#if 0
	gboolean wasted;
#endif
	long flags;

	FILE *fp;
	char *buffer;
	long long offset;
	long long size;

	MsnMessage *msg; /* The temporary real message that will be sent */

#ifdef DEBUG_SLP
	char *info;
	gboolean text_body;
#endif
};

MsnSlpMessage *msn_slpmsg_new(MsnSlpLink *slplink);
void msn_slpmsg_destroy(MsnSlpMessage *slpmsg);
void msn_slpmsg_set_body(MsnSlpMessage *slpmsg, const char *body,
						 long long size);
void msn_slpmsg_open_file(MsnSlpMessage *slpmsg,
						  const char *file_name);
MsnSlpMessage * msn_slpmsg_sip_new(MsnSlpCall *slpcall, int cseq,
								   const char *header,
								   const char *branch,
								   const char *content_type,
								   const char *content);

#ifdef DEBUG_SLP
const void msn_slpmsg_show(MsnMessage *msg);
#endif

#endif /* _MSN_SLPMSG_H_ */
