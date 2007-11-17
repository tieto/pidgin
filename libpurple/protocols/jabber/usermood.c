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

#include "internal.h"

#include "usermood.h"
#include "pep.h"
#include <string.h>
#include "internal.h"
#include "request.h"
#include "debug.h"

static const char * const moodstrings[] = {
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
	const char *newmood = NULL;
	char *moodtext = NULL;
	JabberBuddy *buddy = jabber_buddy_find(js, from, FALSE);
	xmlnode *moodinfo, *mood;
	/* ignore the mood of people not on our buddy list */
	if (!buddy || !item)
		return;
	
	mood = xmlnode_get_child_with_namespace(item, "mood", "http://jabber.org/protocol/mood");
	if (!mood)
		return;
	for (moodinfo = mood->child; moodinfo; moodinfo = moodinfo->next) {
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
		const char *status_id;
		JabberBuddyResource *resource = jabber_buddy_find_resource(buddy, NULL);
		if(!resource) { /* huh? */
			g_free(moodtext);
			return;
		}
		status_id = jabber_buddy_state_get_status_id(resource->state);

		purple_prpl_got_user_status(js->gc->account, from, status_id, "mood", _(newmood), "moodtext", moodtext?moodtext:"", NULL);
	}
	g_free(moodtext);
}

void jabber_mood_init(void) {
	jabber_add_feature("mood", "http://jabber.org/protocol/mood", jabber_pep_namespace_only_when_pep_enabled_cb);
	jabber_pep_register_handler("moodn", "http://jabber.org/protocol/mood", jabber_mood_cb);
}

static void do_mood_set_from_fields(PurpleConnection *gc, PurpleRequestFields *fields) {
	JabberStream *js;
	int max_mood_idx;
	int selected_mood = purple_request_fields_get_choice(fields, "mood");

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		purple_debug_error("jabber", "Unable to set mood; account offline.\n");
		return;
	}

	js = gc->proto_data;

	/* This is ugly, but protects us from unexpected values. */
	for (max_mood_idx = 0; moodstrings[max_mood_idx]; max_mood_idx++);

	if (selected_mood < 0 || selected_mood >= max_mood_idx) {
		purple_debug_error("jabber", "Invalid mood index (%d) selected.\n", selected_mood);
		return;
	}

	jabber_mood_set(js, moodstrings[selected_mood], purple_request_fields_get_string(fields, "text"));
}

static void do_mood_set_mood(PurplePluginAction *action) {
	PurpleConnection *gc = (PurpleConnection *) action->context;

	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	int i;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_choice_new("mood",
											_("Mood"), 0);
	
	for(i = 0; moodstrings[i]; ++i)
		purple_request_field_choice_add(field, _(moodstrings[i]));
	
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("text",
											_("Description"), NULL,
											FALSE);
	purple_request_field_group_add_field(group, field);
	
	purple_request_fields(gc, _("Edit User Mood"),
						  _("Edit User Mood"),
						  _("Please select your mood from the list."),
						  fields,
						  _("Set"), G_CALLBACK(do_mood_set_from_fields),
						  _("Cancel"), NULL,
						  purple_connection_get_account(gc), NULL, NULL,
						  gc);
	
}

void jabber_mood_init_action(GList **m) {
	PurplePluginAction *act = purple_plugin_action_new(_("Set Mood..."), do_mood_set_mood);
	*m = g_list_append(*m, act);
}

void jabber_mood_set(JabberStream *js, const char *mood, const char *text) {
	xmlnode *publish, *moodnode;

	g_return_if_fail(mood != NULL);

	publish = xmlnode_new("publish");
	xmlnode_set_attrib(publish,"node","http://jabber.org/protocol/mood");
	moodnode = xmlnode_new_child(xmlnode_new_child(publish, "item"), "mood");
	xmlnode_set_namespace(moodnode, "http://jabber.org/protocol/mood");
	xmlnode_new_child(moodnode, mood);

	if (text && text[0] != '\0') {
		xmlnode *textnode = xmlnode_new_child(moodnode, "text");
		xmlnode_insert_data(textnode, text, -1);
	}
	
	jabber_pep_publish(js, publish);
	/* publish is freed by jabber_pep_publish -> jabber_iq_send -> jabber_iq_free
	   (yay for well-defined memory management rules) */
}
