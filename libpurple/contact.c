/*
 * purple
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "contact.h"
#include "dbus-maybe.h"
#include "internal.h" /* TODO: this needs to die */
#include "util.h"

#define PURPLE_CONTACT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CONTACT, PurpleContactPrivate))

typedef struct _PurpleContactPrivate    PurpleContactPrivate;

struct _PurpleContactPrivate {
	char *alias;                  /* The user-set alias of the contact  */
	PurpleBuddy *priority_buddy;  /* The "top" buddy for this contact   */
	gboolean priority_valid;      /* Is priority valid?                 */
};

enum
{
	PROP_0,
	PROP_ALIAS,
	PROP_PRIORITY_BUDDY,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * API
 *****************************************************************************/
static void
purple_contact_compute_priority_buddy(PurpleContact *contact) {
	PurpleBlistNode *bnode;
	PurpleBuddy *new_priority = NULL;
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(priv != NULL);

	priv->priority_buddy = NULL;
	for (bnode = PURPLE_BLIST_NODE(contact)->child;
			bnode != NULL;
			bnode = bnode->next)
	{
		PurpleBuddy *buddy;

		if (!PURPLE_IS_BUDDY(bnode))
			continue;

		buddy = PURPLE_BUDDY(bnode);
		if (new_priority == NULL)
		{
			new_priority = buddy;
			continue;
		}

		if (purple_account_is_connected(purple_buddy_get_account(buddy)))
		{
			int cmp = 1;
			if (purple_account_is_connected(purple_buddy_get_account(new_priority)))
				cmp = purple_buddy_presence_compare(
						PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(new_priority)),
						PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(buddy)));

			if (cmp > 0 || (cmp == 0 &&
			                purple_prefs_get_bool("/purple/contact/last_match")))
			{
				new_priority = buddy;
			}
		}
	}

	priv->priority_buddy = new_priority;
	priv->priority_valid = TRUE;

	g_object_notify_by_pspec(G_OBJECT(contact),
			properties[PROP_PRIORITY_BUDDY]);
}

PurpleGroup *
purple_contact_get_group(const PurpleContact *contact)
{
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return PURPLE_GROUP(PURPLE_BLIST_NODE(contact)->parent);
}

void
purple_contact_set_alias(PurpleContact *contact, const char *alias)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	PurpleBlistNode *bnode;
	char *old_alias;
	char *new_alias = NULL;
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strequal(priv->alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->alias = new_alias;
	else {
		priv->alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	g_object_notify_by_pspec(G_OBJECT(contact),
			properties[PROP_ALIAS]);

	if (ops) {
		if (ops->save_node)
			ops->save_node(PURPLE_BLIST_NODE(contact));
		if (ops->update)
			ops->update(purple_blist_get_buddy_list(), PURPLE_BLIST_NODE(contact));
	}

	for(bnode = PURPLE_BLIST_NODE(contact)->child; bnode != NULL; bnode = bnode->next)
	{
		PurpleBuddy *buddy = PURPLE_BUDDY(bnode);

		im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy),
				purple_buddy_get_account(buddy));
		if (im)
			purple_conversation_autoset_title(PURPLE_CONVERSATION(im));
	}

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 contact, old_alias);
	g_free(old_alias);
}

const char *purple_contact_get_alias(PurpleContact* contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->alias)
		return priv->alias;

	return purple_buddy_get_alias(purple_contact_get_priority_buddy(contact));
}

gboolean purple_contact_on_account(PurpleContact *c, PurpleAccount *account)
{
	PurpleBlistNode *bnode, *cnode = (PurpleBlistNode *) c;

	g_return_val_if_fail(PURPLE_IS_CONTACT(c), FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);

	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		PurpleBuddy *buddy;

		if (! PURPLE_IS_BUDDY(bnode))
			continue;

		buddy = (PurpleBuddy *)bnode;
		if (purple_buddy_get_account(buddy) == account)
			return TRUE;
	}
	return FALSE;
}

void purple_contact_invalidate_priority_buddy(PurpleContact *contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(priv != NULL);

	priv->priority_valid = FALSE;
}

PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact)
{
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_val_if_fail(priv != NULL, NULL);

	if (!priv->priority_valid)
		purple_contact_compute_priority_buddy(contact);

	return priv->priority_buddy;
}

void purple_contact_merge(PurpleContact *source, PurpleBlistNode *node)
{
	PurpleBlistNode *sourcenode = (PurpleBlistNode*)source;
	PurpleBlistNode *prev, *cur, *next;
	PurpleContact *target;

	g_return_if_fail(PURPLE_IS_CONTACT(source));
	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));

	if (PURPLE_IS_CONTACT(node)) {
		target = (PurpleContact *)node;
		prev = _purple_blist_get_last_child(node);
	} else if (PURPLE_IS_BUDDY(node)) {
		target = (PurpleContact *)node->parent;
		prev = node;
	} else {
		return;
	}

	if (source == target || !target)
		return;

	next = sourcenode->child;

	while (next) {
		cur = next;
		next = cur->next;
		if (PURPLE_IS_BUDDY(cur)) {
			purple_blist_add_buddy((PurpleBuddy *)cur, target, NULL, prev);
			prev = cur;
		}
	}
}

/**************************************************************************
 * GObject Stuff
 **************************************************************************/
/* Set method for GObject properties */
static void
purple_contact_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleContact *contact = PURPLE_CONTACT(obj);

	switch (param_id) {
		case PROP_ALIAS:
			purple_contact_set_alias(contact, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_contact_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleContact *contact = PURPLE_CONTACT(obj);
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	switch (param_id) {
		case PROP_ALIAS:
			g_value_set_string(value, priv->alias);
			break;
		case PROP_PRIORITY_BUDDY:
			g_value_set_object(value, purple_contact_get_priority_buddy(contact));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_contact_init(GTypeInstance *instance, gpointer klass)
{
	PurpleContact *contact = PURPLE_CONTACT(instance);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	if (ops && ops->new_node)
		ops->new_node(PURPLE_BLIST_NODE(contact));

	PURPLE_DBUS_REGISTER_POINTER(contact, PurpleContact);
}

/* GObject finalize function */
static void
purple_contact_finalize(GObject *object)
{
	g_free(PURPLE_CONTACT_GET_PRIVATE(object)->alias);

	PURPLE_DBUS_UNREGISTER_POINTER(object);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_contact_class_init(PurpleContactClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_contact_finalize;

	/* Setup properties */
	obj_class->get_property = purple_contact_get_property;
	obj_class->set_property = purple_contact_set_property;

	g_type_class_add_private(klass, sizeof(PurpleContactPrivate));

	properties[PROP_ALIAS] = g_param_spec_string(
		"alias",
		"Alias",
		"The alias for the contact.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_PRIORITY_BUDDY] = g_param_spec_object(
		"priority-buddy",
		"Priority buddy",
		"The priority buddy of the contact.",
		PURPLE_TYPE_BUDDY,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_contact_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleContactClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_contact_class_init,
			NULL,
			NULL,
			sizeof(PurpleContact),
			0,
			(GInstanceInitFunc)purple_contact_init,
			NULL,
		};

		type = g_type_register_static(
			PURPLE_TYPE_COUNTING_NODE,
			"PurpleContact",
			&info,
			0
		);
	}

	return type;
}

PurpleContact *
purple_contact_new(void)
{
	return g_object_new(PURPLE_TYPE_CONTACT, NULL);
}

