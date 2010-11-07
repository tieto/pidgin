/* $Id$ */

/*
 *  (C) Copyright 2009 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#ifndef LIBGADU_INTERNAL_H
#define LIBGADU_INTERNAL_H

#include "libgadu.h"

char *gg_cp_to_utf8(const char *b);
char *gg_utf8_to_cp(const char *b);
int gg_pubdir50_handle_reply_sess(struct gg_session *sess, struct gg_event *e, const char *packet, int length);
void gg_debug_common(struct gg_session *sess, int level, const char *format, va_list ap);

#endif /* LIBGADU_INTERNAL_H */
