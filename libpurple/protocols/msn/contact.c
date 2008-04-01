/**
 * @file contact.c 
 * 	get MSN contacts via SOAP request
 *	created by MaYuan<mayuan2006@gmail.com>
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "msn.h"
#include "contact.h"
#include "xmlnode.h"
#include "group.h"
#include "soap2.h"

const char *MsnSoapPartnerScenarioText[] =
{
	"Initial",
	"ContactSave",
	"MessengerPendingList",
	"ContactMsgrAPI",
	"BlockUnblock"
};

const char *MsnMemberRole[] =
{
	"Forward",
	"Allow",
	"Block",
	"Reverse",
	"Pending"
};

typedef struct {
	MsnContact *contact;
	MsnSoapPartnerScenario which;
} GetContactListCbData;

/* new a contact */
MsnContact *
msn_contact_new(MsnSession *session)
{
	MsnContact *contact;

	contact = g_new0(MsnContact, 1);
	contact->session = session;

	return contact;
}

/* destroy the contact */
void
msn_contact_destroy(MsnContact *contact)
{
	g_free(contact);
}

MsnCallbackState *
msn_callback_state_new(MsnSession *session)
{
	MsnCallbackState *state = g_new0(MsnCallbackState, 1);

	state->session = session;

	return state;
}	

void
msn_callback_state_free(MsnCallbackState *state)
{
	if (state == NULL)
		return;

	g_free(state->who);
	g_free(state->uid);
	g_free(state->old_group_name);
	g_free(state->new_group_name);
	g_free(state->guid);

	g_free(state);
}

void
msn_callback_state_set_who(MsnCallbackState *state, const gchar *who)
{
	gchar *nval;
	g_return_if_fail(state != NULL);

	nval = g_strdup(who);
	g_free(state->who);
	state->who = nval;
}

void
msn_callback_state_set_uid(MsnCallbackState *state, const gchar *uid)
{
	gchar *nval;
	g_return_if_fail(state != NULL);

	nval = g_strdup(uid);
	g_free(state->uid);
	state->uid = nval;
}

void
msn_callback_state_set_old_group_name(MsnCallbackState *state, const gchar *old_group_name)
{
	gchar *nval;
	g_return_if_fail(state != NULL);

	nval = g_strdup(old_group_name);
	g_free(state->old_group_name);
	state->old_group_name = nval;
}

void
msn_callback_state_set_new_group_name(MsnCallbackState *state, const gchar *new_group_name)
{
	gchar *nval;
	g_return_if_fail(state != NULL);

	nval = g_strdup(new_group_name);
	g_free(state->new_group_name);
	state->new_group_name = nval;
}

void
msn_callback_state_set_guid(MsnCallbackState *state, const gchar *guid)
{
	gchar *nval;
	g_return_if_fail(state != NULL);

	nval = g_strdup(guid);
	g_free(state->guid);
	state->guid = nval;
}


void
msn_callback_state_set_list_id(MsnCallbackState *state, MsnListId list_id)
{
	g_return_if_fail(state != NULL);

	state->list_id = list_id;
}

void
msn_callback_state_set_action(MsnCallbackState *state, MsnCallbackAction action)
{
	g_return_if_fail(state != NULL);

	state->action |= action;
}

/*get MSN member role utility*/
static MsnListId
msn_get_memberrole(const char *role)
{
	g_return_val_if_fail(role != NULL, 0);

	if (!strcmp(role,"Allow")) {
		return MSN_LIST_AL;
	} else if (!strcmp(role,"Block")) {
		return MSN_LIST_BL;
	} else if (!strcmp(role,"Reverse")) {
		return MSN_LIST_RL;
	} else if (!strcmp(role,"Pending")) {
		return MSN_LIST_PL;
	}
	return 0;
}

/*get User Type*/
static int
msn_get_user_type(char *type)
{
	g_return_val_if_fail(type != NULL, 0);

	if (!strcmp(type,"Regular")) {
		return MSN_USER_TYPE_PASSPORT;
	}
	if (!strcmp(type,"Live")) {
		return MSN_USER_TYPE_PASSPORT;
	}
	if (!strcmp(type,"LivePending")) {
		return MSN_USER_TYPE_PASSPORT;
	}

	return MSN_USER_TYPE_UNKNOWN;
}

/* Create the AddressBook in the server, if we don't have one */
static void
msn_create_address_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	if (resp && msn_soap_xml_get(resp->xml, "Body/Fault") == NULL) {
		purple_debug_info("msnab", "Address Book successfully created!\n");
		msn_get_address_book((MsnContact *)data, MSN_PS_INITIAL, NULL, NULL);
	} else {
		purple_debug_info("msnab", "Address Book creation failed!\n");
	}
}

static void
msn_create_address_book(MsnContact * contact)
{
	gchar *body;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(contact->session != NULL);
	g_return_if_fail(contact->session->user != NULL);
	g_return_if_fail(contact->session->user->passport != NULL);
	
	purple_debug_info("msnab","Creating an Address Book.\n");

	body = g_strdup_printf(MSN_ADD_ADDRESSBOOK_TEMPLATE, contact->session->user->passport);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_ADD_ADDRESSBOOK_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL, msn_create_address_cb,
		contact);

	g_free(body);
}

static void
msn_parse_each_member(MsnSession *session, xmlnode *member, const char *node,
	MsnListId list)
{
	char *passport = xmlnode_get_data(xmlnode_get_child(member, node));
	char *type = xmlnode_get_data(xmlnode_get_child(member, "Type"));
	char *member_id = xmlnode_get_data(xmlnode_get_child(member, "MembershipId"));
	MsnUser *user = msn_userlist_find_add_user(session->userlist, passport, NULL);

	purple_debug_info("msncl","%s name: %s, Type: %s, MembershipID: %s\n",
		node, passport, type, member_id == NULL ? "(null)" : member_id);

	if (member_id) {
		user->membership_id[list] = atoi(member_id);
	}

	msn_got_lst_user(session, user, 1 << list, NULL);         

	g_free(passport);
	g_free(type);
	g_free(member_id);
}

static void
msn_parse_each_service(MsnSession *session, xmlnode *service)
{
	xmlnode *type;

	if ((type = msn_soap_xml_get(service, "Info/Handle/Type"))) {
		char *type_str = xmlnode_get_data(type);

		if (g_str_equal(type_str, "Profile")) {
			/* Process Windows Live 'Messenger Roaming Identity' */
		} else if (g_str_equal(type_str, "Messenger")) {
			xmlnode *lastchange = xmlnode_get_child(service, "LastChange");
			char *lastchange_str = xmlnode_get_data(lastchange);
			xmlnode *membership;

			purple_debug_info("msncl","last change: %s\n", lastchange_str);	
			purple_account_set_string(session->account,	"CLLastChange",
				lastchange_str);

			for (membership = msn_soap_xml_get(service,
					"Memberships/Membership");
				 membership; membership = xmlnode_get_next_twin(membership)) {

				xmlnode *role = xmlnode_get_child(membership, "MemberRole");
				char *role_str = xmlnode_get_data(role);
				MsnListId list = msn_get_memberrole(role_str);
				xmlnode *member;

				purple_debug_info("msncl", "MemberRole role: %s, list: %d\n",
					role_str, list);

				for (member = msn_soap_xml_get(membership, "Members/Member");
					 member; member = xmlnode_get_next_twin(member)) {
					const char *member_type = xmlnode_get_attrib(member, "type");
					if (g_str_equal(member_type, "PassportMember")) {
						msn_parse_each_member(session, member, "PassportName",
							list);
					} else if (g_str_equal(member_type, "PhoneMember")) {

					} else if (g_str_equal(member_type, "EmailMember")) {
						msn_parse_each_member(session, member, "Email",	list);
					}
				}

				g_free(role_str);
			}

			g_free(lastchange_str);
		}

		g_free(type_str);
	}
}

/*parse contact list*/
static void
msn_parse_contact_list(MsnContact *contact, xmlnode *node)
{
	xmlnode *fault, *faultnode;

	/* we may get a response if our cache data is too old:
	 *
	 * <faultstring>Need to do full sync. Can't sync deltas Client
	 * has too old a copy for us to do a delta sync</faultstring>
	 *
	 * this is not handled yet
	 */
	if ((fault = msn_soap_xml_get(node, "Body/Fault"))) {
		if ((faultnode = xmlnode_get_child(fault, "faultstring"))) {
			char *faultstring = xmlnode_get_data(faultnode);
			purple_debug_info("msncl", "Retrieving contact list failed: %s\n",
				faultstring);
			g_free(faultstring);
		}
		if ((faultnode = msn_soap_xml_get(fault, "detail/errorcode"))) {
			char *errorcode = xmlnode_get_data(faultnode);

			if (g_str_equal(errorcode, "ABDoesNotExist")) {
				msn_create_address_book(contact);
				g_free(errorcode);
				return;
			}

			g_free(errorcode);
		}

		msn_get_contact_list(contact, MSN_PS_INITIAL, NULL);
	} else {
		xmlnode *service;

		for (service = msn_soap_xml_get(node, "Body/FindMembershipResponse/"
				"FindMembershipResult/Services/Service");
			 service; service = xmlnode_get_next_twin(service)) {
			msn_parse_each_service(contact->session, service);
		}
	}
}

static void
msn_get_contact_list_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	GetContactListCbData *cb_data = data;
	MsnContact *contact = cb_data->contact;
	MsnSession *session = contact->session;

	g_return_if_fail(session != NULL);

	if (resp != NULL) {
		const char *abLastChange;
		const char *dynamicItemLastChange;

		purple_debug_misc("msncl","Got the contact list!\n");

		msn_parse_contact_list(cb_data->contact, resp->xml);
		abLastChange = purple_account_get_string(session->account,
			"ablastChange", NULL);
		dynamicItemLastChange = purple_account_get_string(session->account,
			"dynamicItemLastChange", NULL);

		if (cb_data->which == MSN_PS_INITIAL) {
#ifdef MSN_PARTIAL_LISTS
			/* XXX: this should be enabled when we can correctly do partial
			   syncs with the server. Currently we need to retrieve the whole
			   list to detect sync issues */
			msn_get_address_book(contact, MSN_PS_INITIAL, abLastChange, dynamicItemLastChange);
#else
			msn_get_address_book(contact, MSN_PS_INITIAL, NULL, NULL);
#endif
		}
	}

	g_free(cb_data);
}

/*SOAP  get contact list*/
void
msn_get_contact_list(MsnContact * contact,
	const MsnSoapPartnerScenario partner_scenario, const char *update_time)
{
	gchar *body = NULL;
	gchar *update_str = NULL;
	GetContactListCbData cb_data = { contact, partner_scenario };
	const gchar *partner_scenario_str = MsnSoapPartnerScenarioText[partner_scenario];

	purple_debug_misc("MSNCL","Getting Contact List.\n");

	if ( update_time != NULL ) {
		purple_debug_info("MSNCL","Last update time: %s\n",update_time);
		update_str = g_strdup_printf(MSN_GET_CONTACT_UPDATE_XML,update_time);
	}

	body = g_strdup_printf(MSN_GET_CONTACT_TEMPLATE, partner_scenario_str, update_str ? update_str : "");

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_GET_CONTACT_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_GET_CONTACT_POST_URL,
		msn_get_contact_list_cb, g_memdup(&cb_data, sizeof(cb_data)));

	g_free(update_str);
	g_free(body);
}

static void
msn_parse_addressbook_groups(MsnContact *contact, xmlnode *node)
{
	MsnSession *session = contact->session;
	xmlnode *group;

	purple_debug_info("MSNAB","msn_parse_addressbook_groups()\n");

	for(group = xmlnode_get_child(node, "Group"); group;
					group = xmlnode_get_next_twin(group)){
		xmlnode *groupId, *groupInfo, *groupname;
		char *group_id = NULL, *group_name = NULL;

		if ((groupId = xmlnode_get_child(group, "groupId")))
			group_id = xmlnode_get_data(groupId);
		if ((groupInfo = xmlnode_get_child(group, "groupInfo")) && (groupname = xmlnode_get_child(groupInfo, "name")))
			group_name = xmlnode_get_data(groupname);

		msn_group_new(session->userlist, group_id, group_name);

		if (group_id == NULL){
			/* Group of ungroupped buddies */
			g_free(group_name);
			continue;
		}

		purple_debug_info("MsnAB","group_id: %s, name: %s\n", group_id, group_name ? group_name : "(null)");
		if ((purple_find_group(group_name)) == NULL){
			PurpleGroup *g = purple_group_new(group_name);
			purple_blist_add_group(g, NULL);
		}
		g_free(group_id);
		g_free(group_name);
	}
}

static gboolean
msn_parse_addressbook_mobile(xmlnode *contactInfo, char **inout_mobile_number)
{
	xmlnode *phones;
	char *mobile_number = NULL;
	gboolean mobile = FALSE;

	*inout_mobile_number = NULL;

	if ((phones = xmlnode_get_child(contactInfo, "phones"))) {
		xmlnode *contact_phone;
		char *phone_type = NULL;

		for (contact_phone = xmlnode_get_child(phones, "ContactPhone");
			 contact_phone;
			 contact_phone = xmlnode_get_next_twin(contact_phone)) {
			xmlnode *contact_phone_type;

			if (!(contact_phone_type =
					xmlnode_get_child(contact_phone, "contactPhoneType")))
				continue;

			phone_type = xmlnode_get_data(contact_phone_type);

			if (phone_type && !strcmp(phone_type, "ContactPhoneMobile")) {
				xmlnode *number;
					
				if ((number = xmlnode_get_child(contact_phone, "number"))) {
					xmlnode *messenger_enabled;
					char *is_messenger_enabled = NULL;

					mobile_number = xmlnode_get_data(number);

					if (mobile_number &&
						(messenger_enabled = xmlnode_get_child(contact_phone, "isMessengerEnabled")) 
						&& (is_messenger_enabled = xmlnode_get_data(messenger_enabled)) 
						&& !strcmp(is_messenger_enabled, "true"))
						mobile = TRUE;

					g_free(is_messenger_enabled);
				}
			}

			g_free(phone_type);
		}
	}

	*inout_mobile_number = mobile_number;
	return mobile;
}

static void
msn_parse_addressbook_contacts(MsnContact *contact, xmlnode *node)
{
	MsnSession *session = contact->session;
	xmlnode *contactNode;
	char *passport = NULL, *Name = NULL, *uid = NULL, *type = NULL, *mobile_number = NULL;
	gboolean mobile = FALSE;

	for(contactNode = xmlnode_get_child(node, "Contact"); contactNode;
				contactNode = xmlnode_get_next_twin(contactNode)) {
		xmlnode *contactId, *contactInfo, *contactType, *passportName, *displayName, *guid, *groupIds, *messenger_user;
		MsnUser *user;
		MsnUserType usertype;

		if (!(contactId = xmlnode_get_child(contactNode,"contactId"))
				|| !(contactInfo = xmlnode_get_child(contactNode, "contactInfo"))
				|| !(contactType = xmlnode_get_child(contactInfo, "contactType")))
			continue;

		g_free(passport);
		g_free(Name);
		g_free(uid);
		g_free(type);
		passport = Name = uid = type = mobile_number = NULL;
		mobile = FALSE;

		uid = xmlnode_get_data(contactId);
		type = xmlnode_get_data(contactType);

		/*setup the Display Name*/
		if (type && !strcmp(type, "Me")){
			char *friendly = NULL;
			if ((displayName = xmlnode_get_child(contactInfo, "displayName")))
				friendly = xmlnode_get_data(displayName);
			purple_connection_set_display_name(session->account->gc, friendly ? purple_url_decode(friendly) : NULL);
			g_free(friendly);
			continue; /* Not adding own account as buddy to buddylist */
		}

		/* ignore non-messenger contacts */
		if((messenger_user = xmlnode_get_child(contactInfo, "isMessengerUser"))) {
			char *is_messenger_user = xmlnode_get_data(messenger_user);

			if(is_messenger_user && !strcmp(is_messenger_user, "false")) {
				g_free(is_messenger_user);
				continue;
			}

			g_free(is_messenger_user);
		}

		usertype = msn_get_user_type(type);
		passportName = xmlnode_get_child(contactInfo, "passportName");
		if (passportName == NULL) {
			xmlnode *emailsNode, *contactEmailNode, *emailNode;
			xmlnode *messengerEnabledNode;
			char *msnEnabled;

			/*TODO: add it to the none-instant Messenger group and recognize as email Membership*/
			/*Yahoo User?*/
			emailsNode = xmlnode_get_child(contactInfo, "emails");
			if (emailsNode == NULL) {
				/*TODO:  need to support the Mobile type*/
				continue;
			}
			for(contactEmailNode = xmlnode_get_child(emailsNode, "ContactEmail"); contactEmailNode;
					contactEmailNode = xmlnode_get_next_twin(contactEmailNode) ){
				if (!(messengerEnabledNode = xmlnode_get_child(contactEmailNode, "isMessengerEnabled"))) {
					/* XXX: Should this be a continue instead of a break? It seems like it'd cause unpredictable results otherwise. */
					break;
				}

				msnEnabled = xmlnode_get_data(messengerEnabledNode);

				if ((emailNode = xmlnode_get_child(contactEmailNode, "email"))) {
					g_free(passport);
					passport = xmlnode_get_data(emailNode);
				}

				if(msnEnabled && !strcmp(msnEnabled, "true")) {
					/*Messenger enabled, Get the Passport*/
					purple_debug_info("MsnAB", "Yahoo User %s\n", passport ? passport : "(null)");
					usertype = MSN_USER_TYPE_YAHOO;
					g_free(msnEnabled);
					break;
				} else {
					/*TODO maybe we can just ignore it in Purple?*/
					purple_debug_info("MSNAB", "Other type user\n");
				}

				g_free(msnEnabled);
			}
		} else {
			passport = xmlnode_get_data(passportName);
		}

		if (passport == NULL)
			continue;

		if ((displayName = xmlnode_get_child(contactInfo, "displayName")))
			Name = xmlnode_get_data(displayName);
		else
			Name = g_strdup(passport);

		mobile = msn_parse_addressbook_mobile(contactInfo, &mobile_number);

		purple_debug_misc("MsnAB","passport:{%s} uid:{%s} display:{%s} mobile:{%s} mobile number:{%s}\n",
			passport, uid ? uid : "(null)", Name ? Name : "(null)",
			mobile ? "true" : "false", mobile_number ? mobile_number : "(null)");

		user = msn_userlist_find_add_user(session->userlist, passport, Name);
		msn_user_set_uid(user, uid);
		msn_user_set_type(user, usertype);
		msn_user_set_mobile_phone(user, mobile_number);

		groupIds = xmlnode_get_child(contactInfo, "groupIds");
		if (groupIds) {
			for (guid = xmlnode_get_child(groupIds, "guid"); guid;
							guid = xmlnode_get_next_twin(guid)){
				char *group_id = xmlnode_get_data(guid);
				msn_user_add_group_id(user, group_id);
				purple_debug_misc("MsnAB", "guid:%s\n", group_id ? group_id : "(null)");
				g_free(group_id);
			}
		} else {
			purple_debug_info("msn", "User not in any groups, adding to default group.\n");
			/*not in any group,Then set default group*/
			msn_user_add_group_id(user, MSN_INDIVIDUALS_GROUP_ID);
		}

		msn_got_lst_user(session, user, MSN_LIST_FL_OP, NULL);

		if(mobile && user)
		{
			user->mobile = TRUE;
			purple_prpl_got_user_status(session->account, user->passport, "mobile", NULL);
			purple_prpl_got_user_status(session->account, user->passport, "available", NULL);
		}
	}

	g_free(passport);
	g_free(Name);
	g_free(uid);
	g_free(type);
}

static gboolean
msn_parse_addressbook(MsnContact * contact, xmlnode *node)
{
	MsnSession * session;
	xmlnode *result;
	xmlnode *groups;
	xmlnode *contacts;
	xmlnode *abNode;
	xmlnode *fault;

	session = contact->session;

	if ((fault = msn_soap_xml_get(node, "Body/Fault"))) {
		xmlnode *faultnode;

		if ((faultnode = xmlnode_get_child(fault, "faultstring"))) {
			gchar *faultstring = xmlnode_get_data(faultnode);
			purple_debug_info("MSNAB","Faultstring: %s\n", faultstring);
			g_free(faultstring);
		}

		if ((faultnode = msn_soap_xml_get(fault, "detail/errorcode"))) {
			gchar *errorcode = xmlnode_get_data(faultnode);

			purple_debug_info("MSNAB", "Error Code: %s\n", errorcode);
						
			if (g_str_equal(errorcode, "ABDoesNotExist")) {
				g_free(errorcode);
				return TRUE;
			}
		}

		return FALSE;
	}

	result = msn_soap_xml_get(node, "Body/ABFindAllResponse/ABFindAllResult");
	if(result == NULL){
		purple_debug_misc("MSNAB","receive no address book update\n");
		return TRUE;
	}

	/* I don't see this "groups" tag documented on msnpiki, need to find out
	   if they are really there, and update msnpiki */
	/*Process Group List*/
	groups = xmlnode_get_child(result,"groups");
	if (groups != NULL) {
		msn_parse_addressbook_groups(contact, groups);
	}

	/*add a default No group to set up the no group Membership*/
	msn_group_new(session->userlist, MSN_INDIVIDUALS_GROUP_ID,
				  MSN_INDIVIDUALS_GROUP_NAME);
	purple_debug_misc("MSNAB","group_id:%s name:%s\n",
					  MSN_INDIVIDUALS_GROUP_ID, MSN_INDIVIDUALS_GROUP_NAME);
	if ((purple_find_group(MSN_INDIVIDUALS_GROUP_NAME)) == NULL){
		PurpleGroup *g = purple_group_new(MSN_INDIVIDUALS_GROUP_NAME);
		purple_blist_add_group(g, NULL);
	}

	/*add a default No group to set up the no group Membership*/
	msn_group_new(session->userlist, MSN_NON_IM_GROUP_ID, MSN_NON_IM_GROUP_NAME);
	purple_debug_misc("MSNAB","group_id:%s name:%s\n", MSN_NON_IM_GROUP_ID, MSN_NON_IM_GROUP_NAME);
	if ((purple_find_group(MSN_NON_IM_GROUP_NAME)) == NULL){
		PurpleGroup *g = purple_group_new(MSN_NON_IM_GROUP_NAME);
		purple_blist_add_group(g, NULL);
	}

	/*Process contact List*/
	purple_debug_info("MSNAB","process contact list...\n");
	contacts =xmlnode_get_child(result,"contacts");
	if (contacts != NULL) {
		msn_parse_addressbook_contacts(contact, contacts);
	}

	abNode =xmlnode_get_child(result,"ab");
	if(abNode != NULL){
		xmlnode *node2;
		char *tmp = NULL;

		if ((node2 = xmlnode_get_child(abNode, "lastChange")))
			tmp = xmlnode_get_data(node2);
		purple_debug_info("MSNAB"," lastchanged Time:{%s}\n", tmp ? tmp : "(null)");
		purple_account_set_string(session->account, "ablastChange", tmp);

		g_free(tmp); tmp = NULL;
		if ((node2 = xmlnode_get_child(abNode, "DynamicItemLastChanged")))
			tmp = xmlnode_get_data(node2);
		purple_debug_info("MsnAB"," DynamicItemLastChanged :{%s}\n", tmp ? tmp : "(null)");
		purple_account_set_string(session->account, "DynamicItemLastChanged", tmp);
		g_free(tmp);
	}

	return TRUE;
}

static void
msn_get_address_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	MsnContact *contact = data;
	MsnSession *session;

	if (resp == NULL)
		return;

	g_return_if_fail(contact != NULL);
	session = contact->session;
	g_return_if_fail(session != NULL);

	purple_debug_misc("MSNAB", "Got the Address Book!\n");

	if (msn_parse_addressbook(contact, resp->xml)) {
		if (!session->logged_in) {
			msn_send_privacy(session->account->gc);
			msn_notification_dump_contact(session);
		}
	} else {
		/* This is making us loop infinitely when we fail to parse the
		  address book, disable for now (we should re-enable when we
		  send timestamps)
		*/
		/*
		msn_get_address_book(contact, NULL, NULL);
		*/
		msn_session_disconnect(session);
		purple_connection_error_reason(session->account->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to retrieve MSN Address Book"));
	}
}

/*get the address book*/
void
msn_get_address_book(MsnContact *contact,
	MsnSoapPartnerScenario partner_scenario, const char *LastChanged,
	const char *dynamicItemLastChange)
{
	char *body, *update_str = NULL;

	purple_debug_misc("MSNAB","Getting Address Book\n");

	/*build SOAP and POST it*/
	if (dynamicItemLastChange != NULL)
		update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML, dynamicItemLastChange);
	else if (LastChanged != NULL)
		update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML, LastChanged);

	body = g_strdup_printf(MSN_GET_ADDRESS_TEMPLATE, MsnSoapPartnerScenarioText[partner_scenario], update_str ? update_str : "");

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_GET_ADDRESS_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL, msn_get_address_cb,
		contact);

	g_free(update_str);
	g_free(body);
}

static void
msn_add_contact_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;
	MsnSession *session = state->session;

	g_return_if_fail(session != NULL);

	if (resp != NULL) {
		MsnUserList *userlist = session->userlist;
		MsnUser *user;
	
		purple_debug_info("MSNCL","Contact added successfully\n");

		// the code this block is replacing didn't send ADL for yahoo contacts,
		// but i haven't confirmed this is WLM's behaviour wrt yahoo contacts
		if ( !msn_user_is_yahoo(session->account, state->who) ) {
			msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_AL);
			msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_FL);
		}

		msn_notification_send_fqy(session, state->who);

		user = msn_userlist_find_add_user(userlist, state->who, state->who);
		msn_user_add_group_id(user, state->guid);
	}

	msn_callback_state_free(state);
}

/* add a Contact in MSN_INDIVIDUALS_GROUP */
void
msn_add_contact(MsnContact *contact, MsnCallbackState *state, const char *passport)
{
	gchar *body = NULL;
	gchar *contact_xml = NULL;

#if 0
	gchar *escaped_displayname;


	 if (displayname != NULL) {
		escaped_displayname = g_markup_decode_text(displayname, -1);
	 } else {
		escaped_displayname = passport;
	 }
	contact_xml = g_strdup_printf(MSN_XML_ADD_CONTACT, escaped_displayname, passport);
#endif

	purple_debug_info("MSNCL","Adding contact %s to contact list\n", passport);

	contact_xml = g_strdup_printf(MSN_CONTACT_XML, passport);
	body = g_strdup_printf(MSN_ADD_CONTACT_TEMPLATE, contact_xml);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_CONTACT_ADD_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_add_contact_read_cb, state);

	g_free(contact_xml);
	g_free(body);
}

static void
msn_add_contact_to_group_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;
	MsnUserList *userlist;

	g_return_if_fail(data != NULL);

	userlist = state->session->userlist;

	if (resp != NULL) {
		if (msn_userlist_add_buddy_to_group(userlist, state->who,
				state->new_group_name)) {
			purple_debug_info("MSNCL", "Contact %s added to group %s successfully!\n", state->who, state->new_group_name);
		} else {
			purple_debug_info("MSNCL","Contact %s added to group %s successfully on server, but failed in the local list\n", state->who, state->new_group_name);
		}

		if (state->action & MSN_ADD_BUDDY) {
			MsnUser *user = msn_userlist_find_user(userlist, state->who);

        	if ( !msn_user_is_yahoo(state->session->account, state->who) ) {

				msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_AL);
				msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_FL);
	        }
	        msn_notification_send_fqy(state->session, state->who);

			if (msn_userlist_user_is_in_list(user, MSN_LIST_PL)) {
				msn_del_contact_from_list(state->session->contact, NULL, state->who, MSN_LIST_PL);
				msn_callback_state_free(state);
				return;
			}
		}

		if (state->action & MSN_MOVE_BUDDY) {
			msn_del_contact_from_group(state->session->contact, state->who, state->old_group_name);
		}
	}

	msn_callback_state_free(state);
}

void
msn_add_contact_to_group(MsnContact *contact, MsnCallbackState *state, 
			 const char *passport, const char *groupId)
{
	MsnUserList *userlist;
	MsnUser *user;
	gchar *body = NULL, *contact_xml;

	g_return_if_fail(passport != NULL);
	g_return_if_fail(groupId != NULL);

	g_return_if_fail(contact != NULL);
	g_return_if_fail(contact->session != NULL);
	g_return_if_fail(contact->session->userlist != NULL);
	
	userlist = contact->session->userlist;

	if (!strcmp(groupId, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(groupId, MSN_NON_IM_GROUP_ID)) {
		
		user = msn_userlist_find_add_user(userlist, passport, passport);

		if (state->action & MSN_ADD_BUDDY) {
			msn_add_contact(contact, state, passport);
			return;
		}

		if (state->action & MSN_MOVE_BUDDY) {
			msn_user_add_group_id(user, groupId);
			msn_del_contact_from_group(contact, passport, state->old_group_name);
		} else {
			msn_callback_state_free(state);
		}

		return;
	}

	purple_debug_info("MSNCL", "Adding user %s to group %s\n", passport, 
			  msn_userlist_find_group_name(userlist, groupId));

	user = msn_userlist_find_user(userlist, passport);
	if (user == NULL) {
		purple_debug_warning("MSNCL", "Unable to retrieve user %s from the userlist!\n", passport);
		msn_callback_state_free(state);                                     
		return; /* guess this never happened! */
	}

	if (user != NULL && user->uid != NULL) {
		contact_xml = g_strdup_printf(MSN_CONTACT_ID_XML, user->uid);
	} else {
		contact_xml = g_strdup_printf(MSN_CONTACT_XML, passport);
	}

	body = g_strdup_printf(MSN_ADD_CONTACT_GROUP_TEMPLATE, groupId, contact_xml);

	msn_soap_message_send(state->session,
		msn_soap_message_new(MSN_ADD_CONTACT_GROUP_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_add_contact_to_group_read_cb, state);

	g_free(contact_xml);
	g_free(body);
}

static void
msn_delete_contact_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;

	if (resp != NULL) {
		MsnUserList *userlist = state->session->userlist;
		MsnUser *user = msn_userlist_find_user_with_id(userlist, state->uid);

		purple_debug_info("MSNCL","Delete contact successful\n");

		if (user != NULL) {
			msn_userlist_remove_user(userlist, user);
		}
	}

	msn_callback_state_free(state);
}

/*delete a Contact*/
void
msn_delete_contact(MsnContact *contact, const char *contactId)
{	
	gchar *body = NULL;
	gchar *contact_id_xml = NULL ;
	MsnCallbackState *state;

	g_return_if_fail(contactId != NULL);
	contact_id_xml = g_strdup_printf(MSN_CONTACT_ID_XML, contactId);

	state = msn_callback_state_new(contact->session);
	msn_callback_state_set_uid(state, contactId);

	/* build SOAP request */
	purple_debug_info("MSNCL","Deleting contact with contactId: %s\n", contactId);
	body = g_strdup_printf(MSN_DEL_CONTACT_TEMPLATE, contact_id_xml);
	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_CONTACT_DEL_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_delete_contact_read_cb, state);

	g_free(contact_id_xml);
	g_free(body);
}

static void
msn_del_contact_from_group_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;

	if (resp != NULL) {
		if (msn_userlist_rem_buddy_from_group(state->session->userlist,
				state->who, state->old_group_name)) {
			purple_debug_info("MSNCL", "Contact %s deleted successfully from group %s\n", state->who, state->old_group_name);
		} else {
			purple_debug_info("MSNCL", "Contact %s deleted successfully from group %s in the server, but failed in the local list\n", state->who, state->old_group_name);
		}
	}
	
	msn_callback_state_free(state);
}

void
msn_del_contact_from_group(MsnContact *contact, const char *passport, const char *group_name)
{
	MsnUserList * userlist;
	MsnUser *user;
	MsnCallbackState *state;
	gchar *body, *contact_id_xml;
	const gchar *groupId;
	
	g_return_if_fail(passport != NULL);
	g_return_if_fail(group_name != NULL);
	g_return_if_fail(contact != NULL);
	g_return_if_fail(contact->session != NULL);
	g_return_if_fail(contact->session->userlist != NULL);
	
	userlist = contact->session->userlist;
	
	groupId = msn_userlist_find_group_id(userlist, group_name);
	if (groupId != NULL) {
		purple_debug_info("MSNCL", "Deleting user %s from group %s\n", passport, group_name);
	} else {
		purple_debug_warning("MSNCL", "Unable to retrieve group id from group %s !\n", group_name);
		return;
	}
	
	user = msn_userlist_find_user(userlist, passport);
	
	if (user == NULL) {
		purple_debug_warning("MSNCL", "Unable to retrieve user from passport %s!\n", passport);
		return;
	}

	if ( !strcmp(groupId, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(groupId, MSN_NON_IM_GROUP_ID)) {
		msn_user_remove_group_id(user, groupId);
		return;
	}

	state = msn_callback_state_new(contact->session);
	msn_callback_state_set_who(state, passport);
	msn_callback_state_set_guid(state, groupId);
	msn_callback_state_set_old_group_name(state, group_name);

	contact_id_xml = g_strdup_printf(MSN_CONTACT_ID_XML, user->uid);
	body = g_strdup_printf(MSN_CONTACT_DEL_GROUP_TEMPLATE, contact_id_xml, groupId);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_CONTACT_DEL_GROUP_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_del_contact_from_group_read_cb, state);
	
	g_free(contact_id_xml);
	g_free(body);
}


static void
msn_update_contact_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	if (resp)
		purple_debug_info("MSN CL","Contact updated successfully\n");
	else
		purple_debug_info("MSN CL","Contact updated successfully\n");
}

/* Update a contact's nickname */
void
msn_update_contact(MsnContact *contact, const char* nickname)
{
	gchar *body = NULL, *escaped_nickname;

	purple_debug_info("MSN CL","Update contact information with new friendly name: %s\n", nickname);

	escaped_nickname = g_markup_escape_text(nickname, -1);

	body = g_strdup_printf(MSN_CONTACT_UPDATE_TEMPLATE, escaped_nickname);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_CONTACT_UPDATE_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_update_contact_read_cb, NULL);

	g_free(escaped_nickname);
	g_free(body);
}

static void
msn_del_contact_from_list_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;
	MsnSession *session = state->session;

	if (resp != NULL) {
		purple_debug_info("MSN CL", "Contact %s deleted successfully from %s list on server!\n", state->who, MsnMemberRole[state->list_id]);

		if (state->list_id == MSN_LIST_PL) {
			MsnUser *user = msn_userlist_find_user(session->userlist, state->who);

			if (user != NULL)
				msn_user_unset_op(user, MSN_LIST_PL_OP);

			msn_add_contact_to_list(session->contact, state, state->who, MSN_LIST_RL);
			return;
		} else if (state->list_id == MSN_LIST_AL) {
			purple_privacy_permit_remove(session->account, state->who, TRUE);
			msn_add_contact_to_list(session->contact, NULL, state->who, MSN_LIST_BL);
		} else if (state->list_id == MSN_LIST_BL) {
			purple_privacy_deny_remove(session->account, state->who, TRUE);
			msn_add_contact_to_list(session->contact, NULL, state->who, MSN_LIST_AL);
		}
	}

	msn_callback_state_free(state);
}

void
msn_del_contact_from_list(MsnContact *contact, MsnCallbackState *state,
			  const gchar *passport, const MsnListId list)
{
	gchar *body = NULL, *member = NULL;
	MsnSoapPartnerScenario partner_scenario;
	MsnUser *user;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(passport != NULL);
	g_return_if_fail(list < 5);

	purple_debug_info("MSN CL", "Deleting contact %s from %s list\n", passport, MsnMemberRole[list]);

	if (state == NULL) {
		state = msn_callback_state_new(contact->session);
	}
	msn_callback_state_set_list_id(state, list);
	msn_callback_state_set_who(state, passport);

	if (list == MSN_LIST_PL) {
		g_return_if_fail(contact->session != NULL);
		g_return_if_fail(contact->session->userlist != NULL);

		user = msn_userlist_find_user(contact->session->userlist, passport);

		partner_scenario = MSN_PS_CONTACT_API;
		member = g_strdup_printf(MSN_MEMBER_MEMBERSHIPID_XML, user->membership_id[MSN_LIST_PL]);
	} else {
		/* list == MSN_LIST_AL || list == MSN_LIST_BL */
		partner_scenario = MSN_PS_BLOCK_UNBLOCK;
		
		member = g_strdup_printf(MSN_MEMBER_PASSPORT_XML, passport);
	}

	body = g_strdup_printf( MSN_CONTACT_DELECT_FROM_LIST_TEMPLATE,
			        MsnSoapPartnerScenarioText[partner_scenario],
			        MsnMemberRole[list],
			        member);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_DELETE_MEMBER_FROM_LIST_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_SHARE_POST_URL,
		msn_del_contact_from_list_read_cb, state);

	g_free(member);
	g_free(body);
}

static void
msn_add_contact_to_list_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp,
	gpointer data)
{
	MsnCallbackState *state = data;

	g_return_if_fail(state != NULL);
	g_return_if_fail(state->session != NULL);
	g_return_if_fail(state->session->contact != NULL);
	
	if (resp != NULL) {
		purple_debug_info("MSN CL", "Contact %s added successfully to %s list on server!\n", state->who, MsnMemberRole[state->list_id]);

		if (state->list_id == MSN_LIST_RL) {
			MsnUser *user = msn_userlist_find_user(state->session->userlist, state->who);
		
			if (user != NULL) {
				msn_user_set_op(user, MSN_LIST_RL_OP);
			}

			if (state->action & MSN_DENIED_BUDDY) {

				msn_add_contact_to_list(state->session->contact, NULL, state->who, MSN_LIST_BL);
			} else if (state->list_id == MSN_LIST_AL) {
				purple_privacy_permit_add(state->session->account, state->who, TRUE);
			} else if (state->list_id == MSN_LIST_BL) {
				purple_privacy_deny_add(state->session->account, state->who, TRUE);
			}
		}
	}

	msn_callback_state_free(state);
}

void
msn_add_contact_to_list(MsnContact *contact, MsnCallbackState *state,
			const gchar *passport, const MsnListId list)
{
	gchar *body = NULL, *member = NULL;
	MsnSoapPartnerScenario partner_scenario;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(passport != NULL);
	g_return_if_fail(list < 5);

	purple_debug_info("MSN CL", "Adding contact %s to %s list\n", passport, MsnMemberRole[list]);

	if (state == NULL) {
		state = msn_callback_state_new(contact->session);
	}
	msn_callback_state_set_list_id(state, list);
	msn_callback_state_set_who(state, passport);

	partner_scenario = (list == MSN_LIST_RL) ? MSN_PS_CONTACT_API : MSN_PS_BLOCK_UNBLOCK;

	member = g_strdup_printf(MSN_MEMBER_PASSPORT_XML, state->who);

	body = g_strdup_printf(MSN_CONTACT_ADD_TO_LIST_TEMPLATE, 
			       MsnSoapPartnerScenarioText[partner_scenario],
			       MsnMemberRole[list], 
			       member);

	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_ADD_MEMBER_TO_LIST_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_SHARE_POST_URL,
		msn_add_contact_to_list_read_cb, state);

	g_free(member);
	g_free(body);
}

#if 0
static void
msn_gleams_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	if (resp != NULL)
		purple_debug_info("MSNP14","Gleams read done\n");
	else
		purple_debug_info("MSNP14","Gleams read failed\n");
}

/*get the gleams info*/
void
msn_get_gleams(MsnContact *contact)
{
	MsnSoapReq *soap_request;

	purple_debug_info("MSNP14","msn get gleams info...\n");
	msn_soap_message_send(contact->session,
		msn_soap_message_new(MSN_GET_GLEAMS_SOAP_ACTION,
			xmlnode_from_str(MSN_GLEAMS_TEMPLATE, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_gleams_read_cb, NULL);
}
#endif


/***************************************************************
 * Group Operations
 ***************************************************************/

static void
msn_group_read_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	MsnCallbackState *state = data;
	
	purple_debug_info("MSNCL", "Group request successful.\n");
	
	g_return_if_fail(state->session != NULL);
	g_return_if_fail(state->session->userlist != NULL);
	g_return_if_fail(state->session->contact != NULL);

	if (resp == NULL) {
		msn_callback_state_free(state);
		return;
	}

	if (state) {
		MsnSession *session = state->session;
		MsnUserList *userlist = session->userlist;
		
		if (state->action & MSN_RENAME_GROUP) {
			msn_userlist_rename_group_id(session->userlist,
						     state->guid,
						     state->new_group_name);
		}
		
		if (state->action & MSN_ADD_GROUP) {
			/* the response is taken from
			   http://telepathy.freedesktop.org/wiki/Pymsn/MSNP/ContactListActions
			   should copy it to msnpiki some day */
			xmlnode *guid_node = msn_soap_xml_get(resp->xml,
				"Body/ABGroupAddResponse/ABGroupAddResult/guid");

			if (guid_node) {
				char *guid = xmlnode_get_data(guid_node);

				/* create and add the new group to the userlist */
				purple_debug_info("MSNCL", "Adding group %s with guid = %s to the userlist\n", state->new_group_name, guid);
				msn_group_new(session->userlist, guid, state->new_group_name);

				if (state->action & MSN_ADD_BUDDY) {
					msn_userlist_add_buddy(session->userlist,
						state->who,
						state->new_group_name);
				} else if (state->action & MSN_MOVE_BUDDY) {
					msn_add_contact_to_group(session->contact, state, state->who, guid); 
					g_free(guid);
					return;
				}
				g_free(guid);
			} else {
				purple_debug_info("MSNCL", "Adding group %s failed\n",
					state->new_group_name);
			}
		}
		
		if (state->action & MSN_DEL_GROUP) {
			GList *l;
			
			msn_userlist_remove_group_id(session->userlist, state->guid);
			for (l = userlist->users; l != NULL; l = l->next) {
				msn_user_remove_group_id( (MsnUser *)l->data, state->guid);
			}
		}
			
		msn_callback_state_free(state);
	}
}

/* add group */
void
msn_add_group(MsnSession *session, MsnCallbackState *state, const char* group_name)
{
	char *body = NULL;

	g_return_if_fail(session != NULL);
	g_return_if_fail(group_name != NULL);
	
	purple_debug_info("MSNCL","Adding group %s to contact list.\n", group_name);

	if (state == NULL) {
		state = msn_callback_state_new(session);
	}

	msn_callback_state_set_action(state, MSN_ADD_GROUP);
	msn_callback_state_set_new_group_name(state, group_name);

	/* escape group name's html special chars so it can safely be sent
	* in a XML SOAP request
	*/
	body = g_markup_printf_escaped(MSN_GROUP_ADD_TEMPLATE, group_name);

	msn_soap_message_send(session,
		msn_soap_message_new(MSN_GROUP_ADD_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_group_read_cb, state);

	g_free(body);
}

/* delete group */
void
msn_del_group(MsnSession *session, const gchar *group_name)
{
	MsnCallbackState *state;
	char *body = NULL;
	const gchar *guid;

	g_return_if_fail(session != NULL);
	
	g_return_if_fail(group_name != NULL);
	purple_debug_info("MSNCL","Deleting group %s from contact list\n", group_name);
	
	guid = msn_userlist_find_group_id(session->userlist, group_name);
	
	/* if group uid we need to del is NULL, 
	*  we need to delete nothing
	*/
	if (guid == NULL) {
		purple_debug_info("MSNCL", "Group %s guid not found, returning.\n", group_name);
		return;
	}

	if ( !strcmp(guid, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(guid, MSN_NON_IM_GROUP_ID) ) {
		// XXX add back PurpleGroup since it isn't really removed in the server?
		return;
	}

	state = msn_callback_state_new(session);
	msn_callback_state_set_action(state, MSN_DEL_GROUP);
	msn_callback_state_set_guid(state, guid);
	
	body = g_strdup_printf(MSN_GROUP_DEL_TEMPLATE, guid);

	msn_soap_message_send(session,
		msn_soap_message_new(MSN_GROUP_DEL_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_group_read_cb, state);

	g_free(body);
}

/* rename group */
void
msn_contact_rename_group(MsnSession *session, const char *old_group_name, const char *new_group_name)
{
	gchar *body = NULL;
	const gchar * guid;
	MsnCallbackState *state;
	
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->userlist != NULL);
	g_return_if_fail(old_group_name != NULL);
	g_return_if_fail(new_group_name != NULL);
	
	purple_debug_info("MSN CL", "Renaming group %s to %s.\n", old_group_name, new_group_name);
	
	guid = msn_userlist_find_group_id(session->userlist, old_group_name);
	if (guid == NULL)
		return;

	state = msn_callback_state_new(session);
	msn_callback_state_set_guid(state, guid);
	msn_callback_state_set_new_group_name(state, new_group_name);

	if ( !strcmp(guid, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(guid, MSN_NON_IM_GROUP_ID) ) {
		msn_add_group(session, state, new_group_name);
		// XXX move every buddy there (we probably need to fix concurrent SOAP reqs first)
	}

	msn_callback_state_set_action(state, MSN_RENAME_GROUP);
	
	body = g_markup_printf_escaped(MSN_GROUP_RENAME_TEMPLATE,
		guid, new_group_name);
	
	msn_soap_message_send(session,
		msn_soap_message_new(MSN_GROUP_RENAME_SOAP_ACTION,
			xmlnode_from_str(body, -1)),
		MSN_CONTACT_SERVER, MSN_ADDRESS_BOOK_POST_URL,
		msn_group_read_cb, state);

	g_free(body);
}
