/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2007, Andreas Monitzer <andy@monitzer.com>
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
 *
 */

#ifndef _PURPLE_JABBER_PEP_H_
#define _PURPLE_JABBER_PEP_H_

#include "jabber.h"
#include "message.h"

/* called when the own server supports pep */
void jabber_pep_init(JabberStream *js);

void jabber_handle_event(JabberMessage *jm);

/*
 * Publishes PEP item(s)
 *
 * @parameter js      The JabberStream associated with the connection this event should be published
 * @parameter publish The publish node. This could be for example &lt;publish node='http://jabber.org/protocol/tune'/> with an &lt;item/> as subnode
 */
void jabber_pep_publish(JabberStream *js, xmlnode *publish);

#endif /* _PURPLE_JABBER_PEP_H_ */
