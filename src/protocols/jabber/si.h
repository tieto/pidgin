/**
 * @file jutil.h utility functions
 *
 * gaim
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
#ifndef _GAIM_JABBER_SI_H_
#define _GAIM_JABBER_SI_H_

#include "ft.h"

#include "jabber.h"

typedef struct _JabberSIXfer {
	JabberStream *js;

	char *id;
	char *resource;

	enum {
		STREAM_METHOD_UNKNOWN,
		STREAM_METHOD_BYTESTREAMS,
		STREAM_METHOD_IBB,
		STREAM_METHOD_UNSUPPORTED
	} stream_method;
} JabberSIXfer;

void jabber_si_parse(JabberStream *js, xmlnode *packet);

void jabber_si_xfer_init(GaimXfer *xfer);
void jabber_si_xfer_start(GaimXfer *xfer);
void jabber_si_xfer_end(GaimXfer *xfer);
void jabber_si_xfer_cancel_send(GaimXfer *xfer);
void jabber_si_xfer_cancel_recv(GaimXfer *xfer);
void jabber_si_xfer_ack(GaimXfer *xfer, const char *buffer, size_t size);


#endif /* _GAIM_JABBER_SI_H_ */
