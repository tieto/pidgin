/**
 * @file parser.h XML parser functions
 *
 * purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PURPLE_JABBER_PARSER_H_
#define _PURPLE_JABBER_PARSER_H_

#include "jabber.h"

void jabber_parser_setup(JabberStream *js);
void jabber_parser_free(JabberStream *js);
void jabber_parser_process(JabberStream *js, const char *buf, int len);

#endif /* _PURPLE_JABBER_PARSER_H_ */
