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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	 02111-1307	 USA
 *
 */

#include "usermood.h"
#include "pep.h"
#include <assert.h>
#include <string.h>
#include "internal.h"

static char *moodstrings[] = {
	"afraid",
	"amazed",
	"angry",
	"annoyed",
	"anxious",
	"aroused",
	"ashamed",
	"bored",
	"brave",
	"calm",
	"cold",
	"confused",
	"contented",
	"cranky",
	"curious",
	"depressed",
	"disappointed",
	"disgusted",
	"distracted",
	"embarrassed",
	"excited",
	"flirtatious",
	"frustrated",
	"grumpy",
	"guilty",
	"happy",
	"hot",
	"humbled",
	"humiliated",
	"hungry",
	"hurt",
	"impressed",
	"in_awe",
	"in_love",
	"indignant",
	"interested",
	"intoxicated",
	"invincible",
	"jealous",
	"lonely",
	"mean",
	"moody",
	"nervous",
	"neutral",
	"offended",
	"playful",
	"proud",
	"relieved",
	"remorseful",
	"restless",
	"sad",
	"sarcastic",
	"serious",
	"shocked",
	"shy",
	"sick",
	"sleepy",
	"stressed",
	"surprised",
	"thirsty",
	"worried",
	NULL
};

static void jabber_mood_cb(JabberStream *js, const char *from, xmlnode *items) {
	/* it doesn't make sense to have more than one item here, so let's just pick the first one */
	xmlnode *item = xmlnode_get_child(items, "item");
	char *newmood;
	char *moodtext = NULL;
	JabberBuddy *buddy = jabber_buddy_find(js, from, FALSE);
	xmlnode *moodinfo, *mood;
	/* ignore the mood of people not on our buddy list */
	if (!buddy || !item)
		return;
	
	mood = xmlnode_get_child_with_namespace(item, "mood", "http://jabber.org/protocol/mood");
	if (!mood)
		return;
	for (moodinfo = mood->child; moodinfo != mood->lastchild; moodinfo = moodinfo->next) {
		if (moodinfo->type == XMLNODE_TYPE_TAG) {
			if (!strcmp(moodinfo->name, "text")) {
				if (!moodtext) /* only pick the first one */
					moodtext = xmlnode_get_data(moodinfo);
			} else {
				int i;
				for (i = 0; moodstrings[i]; ++i) {
					/* verify that the mood is known (valid) */
					if (!strcmp(moodinfo->name, moodstrings[i])) {
						newmood = moodstrings[i];
						break;
					}
				}
			}
			if (newmood != NULL && moodtext != NULL)
			   break;
		}
	}
	if (newmood != NULL) {
		JabberBuddyResource *resource = jabber_buddy_find_resource(buddy, NULL);
		const char *status_id = jabber_buddy_state_get_status_id(resource->state);
		
		purple_prpl_got_user_status(js->gc->account, from, status_id, "mood", _(newmood), "moodtext", moodtext?moodtext:"", NULL);
	}
	if (moodtext)
		g_free(moodtext);
}

void jabber_mood_init(void) {
	jabber_add_feature("mood", "http://jabber.org/protocol/mood");
	jabber_pep_register_handler("moodn", "http://jabber.org/protocol/mood", jabber_mood_cb);
}

static void do_mood_set_mood(PurplePluginAction *action) {
	
}

void jabber_mood_init_action(GList **m) {
	PurplePluginAction *act = purple_plugin_action_new(_("Set Mood..."), do_mood_set_mood);
	*m = g_list_append(*m, act);
}

void jabber_set_mood(JabberStream *js, const char *mood, const char *text) {
	xmlnode *publish, *moodnode;
	
	assert(mood != NULL);
	
	publish = xmlnode_new("publish");
	xmlnode_set_attrib(publish,"node","http://jabber.org/protocol/mood");
	moodnode = xmlnode_new_child(xmlnode_new_child(publish, "item"), "mood");
	xmlnode_set_namespace(moodnode, "http://jabber.org/protocol/mood");
	xmlnode_new_child(moodnode, mood);

	if (text) {
		xmlnode *textnode = xmlnode_new_child(moodnode, "text");
		xmlnode_insert_data(textnode, text, -1);
	}
	
	jabber_pep_publish(js, publish);
	
	xmlnode_free(publish);
}
