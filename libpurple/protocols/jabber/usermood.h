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

#ifndef _PURPLE_JABBER_USERMOOD_H_
#define _PURPLE_JABBER_USERMOOD_H_

#include "jabber.h"

/* Implementation of XEP-0107 */

typedef enum _jabber_mood { /* wtf */
    unknown = 0,
    afraid,
    amazed,
    angry,
    annoyed,
    anxious,
    aroused,
    ashamed,
    bored,
    brave,
    calm,
    cold,
    confused,
    contented,
    cranky,
    curious,
    depressed,
    disappointed,
    disgusted,
    distracted,
    embarrassed,
    excited,
    flirtatious,
    frustrated,
    grumpy,
    guilty,
    happy,
    hot,
    humbled,
    humiliated,
    hungry,
    hurt,
    impressed,
    in_awe,
    in_love,
    indignant,
    interested,
    intoxicated,
    invincible,
    jealous,
    lonely,
    mean,
    moody,
    nervous,
    neutral,
    offended,
    playful,
    proud,
    relieved,
    remorseful,
    restless,
    sad,
    sarcastic,
    serious,
    shocked,
    shy,
    sick,
    sleepy,
    stressed,
    surprised,
    thirsty,
    worried
} jabber_mood;

void jabber_mood_init(void);

void jabber_set_mood(JabberStream *js, jabber_mood mood, const char *text /* might be NULL */);

#endif /* _PURPLE_JABBER_USERMOOD_H_ */
