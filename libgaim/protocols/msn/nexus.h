/**
 * @file nexus.h MSN Nexus functions
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#ifndef _MSN_NEXUS_H_
#define _MSN_NEXUS_H_

typedef struct _MsnNexus MsnNexus;

struct _MsnNexus
{
	MsnSession *session;

	char *login_host;
	char *login_path;
	GHashTable *challenge_data;
	GaimSslConnection *gsc;

	guint input_handler;

	char *write_buf;
	gsize written_len;
	GaimInputFunction written_cb;

	char *read_buf;
	gsize read_len;
};

void msn_nexus_connect(MsnNexus *nexus);
MsnNexus *msn_nexus_new(MsnSession *session);
void msn_nexus_destroy(MsnNexus *nexus);

#endif /* _MSN_NEXUS_H_ */
