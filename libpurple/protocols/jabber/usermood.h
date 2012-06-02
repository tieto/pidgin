/*
 * purple - Jabber Protocol Plugin
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#ifndef PURPLE_JABBER_USERMOOD_H_
#define PURPLE_JABBER_USERMOOD_H_

#include "jabber.h"

/* Implementation of XEP-0107 */

void jabber_mood_init(void);

/**
 * Sets / unsets the mood for the specified account.  The mood passed in
 * must either be NULL, "", or one of the moods returned by
 * jabber_get_moods().
 *
 * @param js The JabberStream object.
 * @param mood The mood to set, NULL, or ""
 * @param text Optional text that goes along with a mood.  Only used when
 *             setting a mood (not when unsetting a mood).
 *
 * @return FALSE if an invalid mood was specified, or TRUE otherwise.
 */
gboolean
jabber_mood_set(JabberStream *js, const char *mood, const char *text);

PurpleMood *jabber_get_moods(PurpleAccount *account);

#endif /* PURPLE_JABBER_USERMOOD_H_ */
