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

/* new a contact */
MsnContact *
msn_contact_new(MsnSession *session)
{
	MsnContact *contact;

	contact = g_new0(MsnContact, 1);
	contact->session = session;
	contact->soapconn = msn_soap_new(session,contact,1);

	return contact;
}

/* destroy the contact */
void
msn_contact_destroy(MsnContact *contact)
{
	msn_soap_destroy(contact->soapconn);
	g_free(contact);
}

MsnCallbackState *
msn_callback_state_new(void)
{
	return g_new0(MsnCallbackState, 1);
}	

void
msn_callback_state_free(MsnCallbackState *state)
{
	g_return_if_fail(state != NULL);
	
	if (state->who != NULL)
		g_free(state->who);
	
	if (state->old_group_name != NULL)
		g_free(state->old_group_name);
	
	if (state->new_group_name != NULL)
		g_free(state->new_group_name);
	
	if (state->guid != NULL)
		g_free(state->guid);
	
	g_free(state);
}
	
void
msn_callback_state_set_who(MsnCallbackState *state, const gchar *who)
{
	g_return_if_fail(state != NULL);
	
	if (state->who != NULL)
		g_free(state->who);
	
	state->who = who != NULL ? g_strdup(who) : NULL;
}

void
msn_callback_state_set_old_group_name(MsnCallbackState *state, const gchar *old_group_name)
{
	g_return_if_fail(state != NULL);
	
	if (state->old_group_name != NULL)
		g_free(state->old_group_name);
	
	state->old_group_name = old_group_name != NULL ? g_strdup(old_group_name) : NULL;
}

void
msn_callback_state_set_new_group_name(MsnCallbackState *state, const gchar *new_group_name)
{
	g_return_if_fail(state != NULL);
	
	if (state->new_group_name != NULL)
		g_free(state->new_group_name);
	
	state->new_group_name = new_group_name != NULL ? g_strdup(new_group_name) : NULL;
}

void
msn_callback_state_set_guid(MsnCallbackState *state, const gchar *guid)
{
	g_return_if_fail(state != NULL);
	
	if (state->guid != NULL)
		g_free(state->guid);
	
	state->guid = guid != NULL ? g_strdup(guid) : NULL;
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
		       
/*contact SOAP server login error*/
static void
msn_contact_login_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to contact server"));
}

/*msn contact SOAP server connect process*/
static void
msn_contact_login_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnContact *contact;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);

	session = contact->session;
	g_return_if_fail(session != NULL);

	/*login ok!We can retrieve the contact list*/
//	msn_get_contact_list(contact, MSN_PS_INITIAL, NULL);
}

/*get MSN member role utility*/
static MsnListId
msn_get_memberrole(char * role)
{
	
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
msn_get_user_type(char * type)
{
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
msn_create_address_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnContact *contact;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);

	purple_debug_info("MSN AddressBook", "Address Book successfully created!\n");
	msn_get_address_book(contact, MSN_PS_INITIAL, NULL, NULL);

//	msn_soap_free_read_buf(soapconn);
	return;
}

static void
msn_create_address_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	purple_debug_info("MSN AddressBook","AddressBookAdd written\n");
	soapconn->read_cb = msn_create_address_cb;

	return;
}

static void
msn_create_address_book(MsnContact * contact)
{
	MsnSoapReq *soap_request;
	gchar *body;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(contact->session != NULL);
	g_return_if_fail(contact->session->user != NULL);
	g_return_if_fail(contact->session->user->passport != NULL);
	
	purple_debug_info("MSN AddressBook","Creating an Address Book.\n");

	body = g_strdup_printf(MSN_ADD_ADDRESSBOOK_TEMPLATE, contact->session->user->passport);

	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_ADD_ADDRESSBOOK_SOAP_ACTION,
					body,
					NULL,
					msn_create_address_cb,
					msn_create_address_written_cb);
	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);

	g_free(body);
	
	return;
}

/*parse contact list*/
static void
msn_parse_contact_list(MsnContact * contact)
{
	MsnSession * session;
	MsnListOp list_op = 0;
	MsnListId list;
	char * passport, *typedata;
	xmlnode *fault, *faultstringnode, *faultdetail, *errorcode;
	xmlnode *node, *body, *response, *result, *services;
	xmlnode *service, *memberships, *info, *handle, *handletype;
	xmlnode *LastChangeNode;
	xmlnode *membershipnode, *members, *member, *passportNode;
	char *LastChangeStr;

	session = contact->session;
	node = xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);

	if (node == NULL) {
		purple_debug_error("MSNCL","Unable to parse SOAP data!\n");
		return;
	}

	purple_debug_misc("MSNCL","Parsing contact list with size %d\n", contact->soapconn->body_len);

	purple_debug_misc("MSNCL","Root node @ %p: Name: '%s', child: '%s', lastchild: '%s'\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");

	if (body == NULL) {
		purple_debug_warning("MSNCL", "Failed to parse contact list Body node\n");
		xmlnode_free(node);
		return;
	}
	purple_debug_info("MSNCL","Body @ %p:  Name: '%s'\n",body,body->name);

	/* Did we receive a <Fault> ? */
	if ( (fault = xmlnode_get_child(body, "Fault")) != NULL) {
	        purple_debug_info("MSNCL","Fault received from SOAP server!\n");

		if ( (faultstringnode = xmlnode_get_child(fault, "faultstring")) != NULL ) {
			gchar * faultstring = xmlnode_get_data(faultstringnode);
			purple_debug_info("MSNCL","Faultstring: %s\n", faultstring);
			g_free(faultstring);
		}
		if ( (faultdetail = xmlnode_get_child(fault, "detail")) != NULL ) {
			purple_debug_info("MSNCL","detail @ %p, name: %s\n",faultdetail, faultdetail->name);

			if ( (errorcode = xmlnode_get_child(faultdetail, "errorcode")) != NULL ) {
				purple_debug_info("MSNCL","errorcode @ %p, name: %s\n",errorcode, errorcode->name);

				if (errorcode->child != NULL) {
					gchar *errorcodestring = xmlnode_get_data(errorcode);
					purple_debug_info("MSNCL", "Error Code: %s\n", errorcodestring);

					if ( !strncmp(errorcodestring, "ABDoesNotExist", 14) ) {
						xmlnode_free(node);
						g_free(errorcodestring);
						msn_create_address_book(contact);
						return;
					}
					g_free(errorcodestring);
				}
			}
		}
		xmlnode_free(node);
		msn_get_contact_list(contact, MSN_PS_INITIAL, NULL);
		return;
	}

	response = xmlnode_get_child(body,"FindMembershipResponse");

	if (response == NULL) {
		/* we may get a response if our cache data is too old:
		 *
		 * <faultstring>Need to do full sync. Can't sync deltas Client
		 * has too old a copy for us to do a delta sync</faultstring>
		 */
		xmlnode_free(node);
		msn_get_contact_list(contact, MSN_PS_INITIAL, NULL);
		return;
	}
	purple_debug_info("MSNCL","FindMembershipResponse @ %p: Name: '%s'\n",response,response->name);

	result = xmlnode_get_child(response,"FindMembershipResult");
	if (result == NULL) {
		purple_debug_warning("MSNCL","Received No Update!\n");
		xmlnode_free(node);
		return;
	}
	purple_debug_info("MSNCL","Result @ %p: Name: '%s'\n", result, result->name);

	if ( (services = xmlnode_get_child(result,"Services")) == NULL) {
		purple_debug_misc("MSNCL","No <Services> received.\n");
		xmlnode_free(node);
		return;
	}

	purple_debug_info("MSNCL","Services @ %p\n",services);
	
	for (service = xmlnode_get_child(services, "Service"); service;
	                                service = xmlnode_get_next_twin(service)) {
		purple_debug_info("MSNCL","Service @ %p\n",service);
	
		if ( (info = xmlnode_get_child(service,"Info")) == NULL ) {
			purple_debug_error("MSNCL","Error getting 'Info' child node\n");
			continue;
		}
		if ( (handle = xmlnode_get_child(info,"Handle")) == NULL ) {
			purple_debug_error("MSNCL","Error getting 'Handle' child node\n");
			continue;
		}
		if ( (handletype = xmlnode_get_child(handle,"Type")) == NULL ) {
			purple_debug_error("MSNCL","Error getting 'Type' child node\n");
			continue;
		}

		if ( (typedata = xmlnode_get_data(handletype)) == NULL) {
			purple_debug_error("MSNCL","Error retrieving data from 'Type' child node\n");
			continue;
		}

		purple_debug_info("MSNCL","processing '%s' Service\n", typedata);

		if ( !g_strcasecmp(typedata, "Profile") ) {
			/* Process Windows Live 'Messenger Roaming Identity' */
			g_free(typedata);
			continue;
		}

		if ( !g_strcasecmp(typedata, "Messenger") ) {

			/*Last Change Node*/
			LastChangeNode = xmlnode_get_child(service, "LastChange");
			LastChangeStr = xmlnode_get_data(LastChangeNode);
			purple_debug_info("MSNCL","LastChangeNode: '%s'\n",LastChangeStr);	
			purple_account_set_string(session->account, "CLLastChange", LastChangeStr);
			g_free(LastChangeStr);

			memberships = xmlnode_get_child(service,"Memberships");
			if (memberships == NULL) {
				purple_debug_warning("MSNCL","Memberships = NULL, cleaning up and returning.\n");
				g_free(typedata);
				xmlnode_free(node);
				return;
			}
			purple_debug_info("MSNCL","Memberships @ %p: Name: '%s'\n",memberships,memberships->name);
			for (membershipnode = xmlnode_get_child(memberships, "Membership"); membershipnode;
							membershipnode = xmlnode_get_next_twin(membershipnode)){
				xmlnode *roleNode;
				char *role;

				roleNode = xmlnode_get_child(membershipnode,"MemberRole");
				role = xmlnode_get_data(roleNode);
				list = msn_get_memberrole(role);
				list_op = 1 << list;

				purple_debug_info("MSNCL","MemberRole role: %s, list_op: %d\n",role,list_op);
				
				g_free(role);
				
				members = xmlnode_get_child(membershipnode,"Members");
				for (member = xmlnode_get_child(members, "Member"); member;
						member = xmlnode_get_next_twin(member)){
					MsnUser *user = NULL;
					xmlnode *typeNode, *membershipIdNode=NULL;
					gchar *type, *membershipId = NULL;

					purple_debug_info("MSNCL","Member type: %s\n", xmlnode_get_attrib(member,"type"));
					
					if( !g_strcasecmp(xmlnode_get_attrib(member,"type"), "PassportMember") ) {
						passportNode = xmlnode_get_child(member,"PassportName");
						passport = xmlnode_get_data(passportNode);
						typeNode = xmlnode_get_child(member,"Type");
						type = xmlnode_get_data(typeNode);
						purple_debug_info("MSNCL","Passport name: '%s', Type: %s\n",passport,type);
						g_free(type);

						user = msn_userlist_find_add_user(session->userlist,passport,NULL);

						membershipIdNode = xmlnode_get_child(member,"MembershipId");
						if (membershipIdNode != NULL) {
							membershipId = xmlnode_get_data(membershipIdNode);
							if (membershipId != NULL) {
								user->membership_id[list] = atoi(membershipId);
								g_free(membershipId);
							}
						}
							
						msn_got_lst_user(session, user, list_op, NULL);

						g_free(passport);
					}
					
					if (!g_strcasecmp(xmlnode_get_attrib(member,"type"),"PhoneMember")) {
					}
					
					if (!g_strcasecmp(xmlnode_get_attrib(member,"type"),"EmailMember")) {
						xmlnode *emailNode;

						emailNode = xmlnode_get_child(member,"Email");
						passport = xmlnode_get_data(emailNode);
						purple_debug_info("MSNCL","Email Member: Name: '%s', list_op: %d\n", passport, list_op);
						user = msn_userlist_find_add_user(session->userlist, passport, NULL);
						
						membershipIdNode = xmlnode_get_child(member,"MembershipId");
						if (membershipIdNode != NULL) {
							membershipId = xmlnode_get_data(membershipIdNode);
							if (membershipId != NULL) {
								user->membership_id[list] = atoi(membershipId);
								g_free(membershipId);
							}
						}
						
						msn_got_lst_user(session, user, list_op, NULL);
						g_free(passport);
					}
				}
			}
			g_free(typedata);	/* Free 'Type' node data after processing 'Messenger' Service */
		}
	}

	xmlnode_free(node);	/* Free the whole XML tree */
}

static void
msn_get_contact_list_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnContact *contact;
	MsnSession *session;
	const char *abLastChange;
	const char *dynamicItemLastChange;
	gchar *partner_scenario;

	purple_debug_misc("MSNCL","Got the contact list!\n");

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);
	g_return_if_fail(soapconn->data_cb != NULL);

	partner_scenario = soapconn->data_cb;

	msn_parse_contact_list(contact);
	/*free the read buffer*/
	msn_soap_free_read_buf(soapconn);

	abLastChange = purple_account_get_string(session->account, "ablastChange", NULL);
	dynamicItemLastChange = purple_account_get_string(session->account, "dynamicItemLastChange", NULL);

	if (!strcmp(partner_scenario, MsnSoapPartnerScenarioText[MSN_PS_INITIAL])) {

#ifdef MSN_PARTIAL_LISTS
		/* XXX: this should be enabled when we can correctly do partial
	 	  syncs with the server. Currently we need to retrieve the whole
	 	  list to detect sync issues */
		msn_get_address_book(contact, MSN_PS_INITIAL, abLastChange, dynamicItemLastChange);
#else
		msn_get_address_book(contact, MSN_PS_INITIAL, NULL, NULL);
#endif
	} else {
		msn_soap_free_read_buf(soapconn);
	}
}

static void
msn_get_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_misc("MSNCL","Sent SOAP request for the contact list.\n");
	soapconn->read_cb = msn_get_contact_list_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*SOAP  get contact list*/
void
msn_get_contact_list(MsnContact * contact, const MsnSoapPartnerScenario partner_scenario, const char *update_time)
{
	MsnSoapReq *soap_request;
	gchar *body = NULL;
	gchar * update_str;
	const gchar *partner_scenario_str = MsnSoapPartnerScenarioText[partner_scenario];

	purple_debug_misc("MSNCL","Getting Contact List.\n");

	if ( update_time != NULL ) {
		purple_debug_info("MSNCL","Last update time: %s\n",update_time);
		update_str = g_strdup_printf(MSN_GET_CONTACT_UPDATE_XML,update_time);
	} else {
		update_str = g_strdup("");
	}

	body = g_strdup_printf(MSN_GET_CONTACT_TEMPLATE, partner_scenario_str, update_str);
	g_free(update_str);

	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_GET_CONTACT_POST_URL,
					MSN_GET_CONTACT_SOAP_ACTION,
					body,
					(gpointer) partner_scenario_str,
					msn_get_contact_list_cb,
					msn_get_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	g_free(body);
}

static void
msn_parse_addressbook_groups(MsnContact *contact, xmlnode *node)
{
	MsnSession *session = contact->session;
	xmlnode *group;

	purple_debug_info("MsnAb","msn_parse_addressbook_groups()\n");

	for(group = xmlnode_get_child(node, "Group"); group;
					group = xmlnode_get_next_twin(group)){
		xmlnode *groupId, *groupInfo, *groupname;
		char *group_id, *group_name;

		groupId = xmlnode_get_child(group,"groupId");
		group_id = xmlnode_get_data(groupId);
		groupInfo = xmlnode_get_child(group,"groupInfo");
		groupname = xmlnode_get_child(groupInfo,"name");
		group_name = xmlnode_get_data(groupname);

		msn_group_new(session->userlist, group_id, group_name);

		if (group_id == NULL){
			/* Group of ungroupped buddies */
			g_free(group_name);
			continue;
		}

		purple_debug_info("MsnAB","group_id: %s, name: %s\n",group_id,group_name);
		if ((purple_find_group(group_name)) == NULL){
			PurpleGroup *g = purple_group_new(group_name);
			purple_blist_add_group(g, NULL);
		}
		g_free(group_id);
		g_free(group_name);
	}
}

static void
msn_parse_addressbook_contacts(MsnContact *contact, xmlnode *node)
{
	MsnSession *session = contact->session;
	xmlnode *contactNode;

	for(contactNode = xmlnode_get_child(node, "Contact"); contactNode;
				contactNode = xmlnode_get_next_twin(contactNode)){
		xmlnode *contactId,*contactInfo,*contactType,*passportName,*displayName,*guid;
		xmlnode *groupIds;
		MsnUser *user;
		MsnUserType usertype;
		char *passport,*Name,*uid,*type;

		passport = NULL;

		contactId= xmlnode_get_child(contactNode,"contactId");
		uid = xmlnode_get_data(contactId);

		contactInfo = xmlnode_get_child(contactNode,"contactInfo");
		contactType = xmlnode_get_child(contactInfo,"contactType");
		type = xmlnode_get_data(contactType);

		/*setup the Display Name*/
		if (!strcmp(type, "Me")){
			char *friendly;
			friendly = xmlnode_get_data(xmlnode_get_child(contactInfo, "displayName"));
			purple_connection_set_display_name(session->account->gc, purple_url_decode(friendly));
			g_free(friendly);
			g_free(uid);
			g_free(type);
			continue; /* Not adding own account as buddy to buddylist */
		}
		usertype = msn_get_user_type(type);
		passportName = xmlnode_get_child(contactInfo,"passportName");
		if (passportName == NULL) {
			xmlnode *emailsNode, *contactEmailNode, *emailNode;
			xmlnode *messengerEnabledNode;
			char *msnEnabled;

			/*TODO: add it to the none-instant Messenger group and recognize as email Membership*/
			/*Yahoo User?*/
			emailsNode = xmlnode_get_child(contactInfo,"emails");
			if (emailsNode == NULL) {
				/*TODO:  need to support the Mobile type*/
				g_free(uid);
				g_free(type);
				continue;
			}
			for(contactEmailNode = xmlnode_get_child(emailsNode,"ContactEmail");contactEmailNode;
					contactEmailNode = xmlnode_get_next_twin(contactEmailNode) ){
				messengerEnabledNode = xmlnode_get_child(contactEmailNode,"isMessengerEnabled");
				if(messengerEnabledNode == NULL){
					break;
				}
				msnEnabled = xmlnode_get_data(messengerEnabledNode);
				if(!strcmp(msnEnabled,"true")){
					/*Messenger enabled, Get the Passport*/
					emailNode = xmlnode_get_child(contactEmailNode,"email");
					passport = xmlnode_get_data(emailNode);
					purple_debug_info("MsnAB","Yahoo User %s\n",passport);
					usertype = MSN_USER_TYPE_YAHOO;
					break;
				}else{
					/*TODO maybe we can just ignore it in Purple?*/
					emailNode = xmlnode_get_child(contactEmailNode,"email");
					passport = xmlnode_get_data(emailNode);
					purple_debug_info("MSNAB","Other type user\n");
				}
				g_free(msnEnabled);
			}
		} else {
			passport = xmlnode_get_data(passportName);
		}

		if (passport == NULL) {
			g_free(uid);
			g_free(type);
			continue;
		}

		displayName = xmlnode_get_child(contactInfo,"displayName");
		if (displayName == NULL) {
			Name = g_strdup(passport);
		} else {
			Name =xmlnode_get_data(displayName);
		}

		purple_debug_misc("MsnAB","passport:{%s} uid:{%s} display:{%s}\n",
						passport,uid,Name);

		user = msn_userlist_find_add_user(session->userlist, passport,Name);
		msn_user_set_uid(user,uid);
		msn_user_set_type(user, usertype);
		g_free(Name);
		g_free(passport);
		g_free(uid);
		g_free(type);

		purple_debug_misc("MsnAB","parse guid...\n");
		groupIds = xmlnode_get_child(contactInfo,"groupIds");
		if (groupIds) {
			for (guid = xmlnode_get_child(groupIds, "guid");guid;
							guid = xmlnode_get_next_twin(guid)){
				char *group_id;
				group_id = xmlnode_get_data(guid);
				msn_user_add_group_id(user,group_id);
				purple_debug_misc("MsnAB","guid:%s\n",group_id);
				g_free(group_id);
			}
		} else {
			/*not in any group,Then set default group*/
			msn_user_add_group_id(user, MSN_INDIVIDUALS_GROUP_ID);
		}

		msn_got_lst_user(session, user, MSN_LIST_FL_OP, NULL);
	}
}

static gboolean
msn_parse_addressbook(MsnContact * contact)
{
	MsnSession * session;
	xmlnode * node,*body,*response,*result;
	xmlnode *groups;
	xmlnode	*contacts;
	xmlnode *abNode;
	xmlnode *fault, *faultstringnode, *faultdetail, *errorcode;

	session = contact->session;

	

	node = xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);
	if ( node == NULL ) {
		purple_debug_error("MSN AddressBook","Error parsing Address Book with size %d\n", contact->soapconn->body_len);
		return FALSE;
	}

	purple_debug_misc("MSN AddressBook", "Parsing Address Book with size %d\n", contact->soapconn->body_len);

	purple_debug_misc("MSN AddressBook","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	
	body = xmlnode_get_child(node,"Body");
	purple_debug_misc("MSN AddressBook","body{%p},name:%s\n",body,body->name);
	
	if ( (fault = xmlnode_get_child(body, "Fault")) != NULL) {
		purple_debug_info("MSN AddressBook","Fault received from SOAP server!\n");
		
		if ( (faultstringnode = xmlnode_get_child(fault, "faultstring")) != NULL ) {
			gchar *faultstring = xmlnode_get_data(faultstringnode);
			purple_debug_info("MSN AddressBook","Faultstring: %s\n", faultstring);
			g_free(faultstring);
		}
		if ( (faultdetail = xmlnode_get_child(fault, "detail")) != NULL ) {
			purple_debug_info("MSN AddressBook","detail @ %p, name: %s\n",faultdetail, faultdetail->name);

			if ( (errorcode = xmlnode_get_child(faultdetail, "errorcode")) != NULL ) {
				gchar *errorcodestring;
				purple_debug_info("MSN AddressBook","errorcode @ %p, name: %s\n",errorcode, errorcode->name);

				errorcodestring = xmlnode_get_data(errorcode);
				purple_debug_info("MSN AddressBook", "Error Code: %s\n", errorcodestring);
						
				if ( !strncmp(errorcodestring, "ABDoesNotExist", 14) ) {
					g_free(errorcodestring);
					return TRUE;
				}
				g_free(errorcodestring);
			}
		}
		return FALSE;
	}


	response = xmlnode_get_child(body,"ABFindAllResponse");

	if (response == NULL) {
		return FALSE;
	}

	purple_debug_misc("MSN SOAP","response{%p},name:%s\n",response,response->name);
	result = xmlnode_get_child(response,"ABFindAllResult");
	if(result == NULL){
		purple_debug_misc("MSNAB","receive no address book update\n");
		return TRUE;
	}
	purple_debug_info("MSN SOAP","result{%p},name:%s\n",result,result->name);

	/*Process Group List*/
	groups = xmlnode_get_child(result,"groups");
	if (groups != NULL) {
		msn_parse_addressbook_groups(contact, groups);
	}

	/*add a default No group to set up the no group Membership*/
	msn_group_new(session->userlist, MSN_INDIVIDUALS_GROUP_ID,
				  MSN_INDIVIDUALS_GROUP_NAME);
	purple_debug_misc("MsnAB","group_id:%s name:%s\n",
					  MSN_INDIVIDUALS_GROUP_ID, MSN_INDIVIDUALS_GROUP_NAME);
	if ((purple_find_group(MSN_INDIVIDUALS_GROUP_NAME)) == NULL){
		PurpleGroup *g = purple_group_new(MSN_INDIVIDUALS_GROUP_NAME);
		purple_blist_add_group(g, NULL);
	}

	/*add a default No group to set up the no group Membership*/
	msn_group_new(session->userlist, MSN_NON_IM_GROUP_ID, MSN_NON_IM_GROUP_NAME);
	purple_debug_misc("MsnAB","group_id:%s name:%s\n", MSN_NON_IM_GROUP_ID, MSN_NON_IM_GROUP_NAME);
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
		xmlnode *LastChangeNode, *DynamicItemLastChangedNode;
		char *lastchange, *dynamicChange;

		LastChangeNode = xmlnode_get_child(abNode,"lastChange");
		lastchange = xmlnode_get_data(LastChangeNode);
		purple_debug_info("MsnAB"," lastchanged Time:{%s}\n",lastchange);
		purple_account_set_string(session->account, "ablastChange", lastchange);

		DynamicItemLastChangedNode = xmlnode_get_child(abNode,"DynamicItemLastChanged");
		dynamicChange = xmlnode_get_data(DynamicItemLastChangedNode);
		purple_debug_info("MsnAB"," DynamicItemLastChanged :{%s}\n",dynamicChange);
		purple_account_set_string(session->account, "DynamicItemLastChanged", lastchange);
		g_free(dynamicChange);
		g_free(lastchange);
	}

	xmlnode_free(node);
	msn_soap_free_read_buf(contact->soapconn);
	return TRUE;
}

static void
msn_get_address_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnContact *contact;
	MsnSession *session;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

	purple_debug_misc("MSN AddressBook", "Got the Address Book!\n");

	if ( msn_parse_addressbook(contact) ) {
		//msn_soap_free_read_buf(soapconn);

		if (!session->logged_in) {
			msn_send_privacy(session->account->gc);
			msn_notification_dump_contact(session);
		}
	} else {
		/* This is making us loop infinitely when we fail to parse the address book,
		  disable for now (we should re-enable when we send timestamps)
		*/
		/*
		msn_get_address_book(contact, NULL, NULL);
		*/
		msn_session_disconnect(session);
		purple_connection_error(session->account->gc, _("Unable to retrieve MSN Address Book"));
	}

	/*free the read buffer*/
	msn_soap_free_read_buf(soapconn);
}

/**/
static void
msn_address_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_misc("MSN AddressBook","Sent SOAP request for the Address Book.\n");
	soapconn->read_cb = msn_get_address_cb;
}

/*get the address book*/
void
msn_get_address_book(MsnContact *contact, const MsnSoapPartnerScenario partner_scenario, const char *LastChanged, const char *dynamicItemLastChange)
{
	MsnSoapReq *soap_request;
	char *body = NULL;
	char *ab_update_str,*update_str;

	purple_debug_misc("MSN AddressBook","Getting Address Book\n");

	/*build SOAP and POST it*/
	if ( LastChanged != NULL ) {
		ab_update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML,LastChanged);
	} else {
		ab_update_str = g_strdup("");
	}
	if ( dynamicItemLastChange != NULL ) {
		update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML,
									 dynamicItemLastChange);
	} else {
		update_str = g_strdup(ab_update_str);
	}
	g_free(ab_update_str);
	

	body = g_strdup_printf(MSN_GET_ADDRESS_TEMPLATE, MsnSoapPartnerScenarioText[partner_scenario], update_str);
	g_free(update_str);

	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_GET_ADDRESS_SOAP_ACTION,
					body,
					NULL,
					msn_get_address_cb,
					msn_address_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	g_free(body);
}

static void
msn_add_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnCallbackState *state = NULL;
	MsnUserList *userlist;
	MsnUser *user;
	
	g_return_if_fail(soapconn->data_cb != NULL);
	g_return_if_fail(soapconn->session != NULL);
	g_return_if_fail(soapconn->session->userlist != NULL);
	
	userlist = soapconn->session->userlist;
	
	state = (MsnCallbackState *) soapconn->data_cb;
	
	purple_debug_info("MSNCL","Contact added successfully\n");

	// the code this block is replacing didn't send ADL for yahoo contacts,
	// but i haven't confirmed this is WLM's behaviour wrt yahoo contacts

	if ( !msn_user_is_yahoo(soapconn->session->account, state->who) ) {
		
		msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_AL);
		msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_FL);
	}
	msn_notification_send_fqy(soapconn->session, state->who);

	user = msn_userlist_find_add_user(userlist, state->who, state->who);
	msn_user_add_group_id(user, state->guid);

	if (msn_userlist_user_is_in_list(user, MSN_LIST_PL)) {
		msn_del_contact_from_list(soapconn->session->contact, NULL, state->who, MSN_LIST_PL);
	} else {
		msn_soap_free_read_buf(soapconn);
	}
	
	msn_callback_state_free(state);
}

static void
msn_add_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSNCL","Add contact request written\n");
	soapconn->read_cb = msn_add_contact_read_cb;
}

/* add a Contact in MSN_INDIVIDUALS_GROUP */
void
msn_add_contact(MsnContact *contact, MsnCallbackState *state, const char *passport)
{
	MsnSoapReq *soap_request;
	gchar *body = NULL;
	gchar *contact_xml = NULL;
	gchar *soap_action;
/*	gchar *escaped_displayname;


	 if (displayname != NULL) {
		escaped_displayname = g_markup_decode_text(displayname, -1);
	 } else {
		escaped_displayname = passport;
	 }
	contact_xml = g_strdup_printf(MSN_XML_ADD_CONTACT, escaped_displayname, passport);
*/
	purple_debug_info("MSNCL","Adding contact %s to contact list\n", passport);

//	if ( !strcmp(state->guid, MSN_INDIVIDUALS_GROUP_ID) ) {
		contact_xml = g_strdup_printf(MSN_CONTACT_XML, passport);
//	}
	body = g_strdup_printf(MSN_ADD_CONTACT_TEMPLATE, contact_xml);

	g_free(contact_xml);

	/*build SOAP and POST it*/
	soap_action = g_strdup(MSN_CONTACT_ADD_SOAP_ACTION);

	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					soap_action,
					body,
					state,
					msn_add_contact_read_cb,
					msn_add_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(soap_action);
	g_free(body);
}

static void
msn_add_contact_to_group_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnCallbackState *state; 
	MsnUserList *userlist;

        g_return_if_fail(soapconn->data_cb != NULL);
        g_return_if_fail(soapconn->session != NULL);
        g_return_if_fail(soapconn->session->userlist != NULL);

        userlist = soapconn->session->userlist;

	state = (MsnCallbackState *) soapconn->data_cb;

	if (msn_userlist_add_buddy_to_group(userlist, state->who, state->new_group_name) == TRUE) {
		purple_debug_info("MSNCL", "Contact %s added to group %s successfully!\n", state->who, state->new_group_name);
	} else {
		purple_debug_info("MSNCL","Contact %s added to group %s successfully on server, but failed in the local list\n", state->who, state->new_group_name);
	}

	if (state->action & MSN_ADD_BUDDY) {
		MsnUser *user = msn_userlist_find_user(userlist, state->who);

        	if ( !msn_user_is_yahoo(soapconn->session->account, state->who) ) {

		                msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_AL);
		                msn_userlist_add_buddy_to_list(userlist, state->who, MSN_LIST_FL);
	        }
	        msn_notification_send_fqy(soapconn->session, state->who);

		if (msn_userlist_user_is_in_list(user, MSN_LIST_PL)) {
			msn_del_contact_from_list(soapconn->session->contact, NULL, state->who, MSN_LIST_PL);
			msn_callback_state_free(state);
			return;
		}
	}

	if (state->action & MSN_MOVE_BUDDY) {
		msn_del_contact_from_group(soapconn->session->contact, state->who, state->old_group_name);
	} else {
		msn_callback_state_free(state);
		msn_soap_free_read_buf(soapconn);
	}
}

static void
msn_add_contact_to_group_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSNCL","Add contact to group request sent!\n");
	soapconn->read_cb = msn_add_contact_to_group_read_cb;
}

void
msn_add_contact_to_group(MsnContact *contact, MsnCallbackState *state, 
			 const char *passport, const char *groupId)
{
	MsnSoapReq *soap_request;
	MsnUserList *userlist;
	MsnUser *user;
	gchar *body = NULL, *soap_action, *contact_xml;

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
		purple_debug_warning("MSN CL", "Unable to retrieve user %s from the userlist!\n", passport);
	}

	if (user->uid != NULL) {
		contact_xml = g_strdup_printf(MSN_CONTACT_ID_XML, user->uid);
	} else {
		contact_xml = g_strdup_printf(MSN_CONTACT_XML, passport);
	}

	body = g_strdup_printf(MSN_ADD_CONTACT_GROUP_TEMPLATE, groupId, contact_xml);
	g_free(contact_xml);

	/*build SOAP and POST it*/
	soap_action = g_strdup(MSN_ADD_CONTACT_GROUP_SOAP_ACTION);

	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					soap_action,
					body,
					state,
					msn_add_contact_to_group_read_cb,
					msn_add_contact_to_group_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(soap_action);
	g_free(body);
}



static void
msn_delete_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	// we should probably delete it from the userlist aswell
	purple_debug_info("MSNCL","Delete contact successful\n");
	msn_soap_free_read_buf(soapconn);
}

static void
msn_delete_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSNCL","Delete contact request written\n");
	soapconn->read_cb = msn_delete_contact_read_cb;
}

/*delete a Contact*/
void
msn_delete_contact(MsnContact *contact, const char *contactId)
{	
	gchar *body = NULL;
	gchar *contact_id_xml = NULL ;
	MsnSoapReq *soap_request;

	g_return_if_fail(contactId != NULL);
	contact_id_xml = g_strdup_printf(MSN_CONTACT_ID_XML, contactId);

	/* build SOAP request */
	purple_debug_info("MSNCL","Deleting contact with contactId: %s\n", contactId);
	body = g_strdup_printf(MSN_DEL_CONTACT_TEMPLATE, contact_id_xml);
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					MSN_CONTACT_DEL_SOAP_ACTION,
					body,
					NULL,
					msn_delete_contact_read_cb,
					msn_delete_contact_written_cb);

	g_free(contact_id_xml);

	/* POST the SOAP request */
	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);

	g_free(body);
}

static void
msn_del_contact_from_group_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnCallbackState *state = (MsnCallbackState *) soapconn->data_cb;
	
	if (msn_userlist_rem_buddy_from_group(soapconn->session->userlist, state->who, state->old_group_name)) {
		purple_debug_info("MSN CL", "Contact %s deleted successfully from group %s\n", state->who, state->old_group_name);
	} else {
		purple_debug_info("MSN CL", "Contact %s deleted successfully from group %s in the server, but failed in the local list\n", state->who, state->old_group_name);
	}
	
	msn_callback_state_free(state);
	msn_soap_free_read_buf(soapconn);
	return;
}

static void
msn_del_contact_from_group_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	
	purple_debug_info("MSN CL","Del contact from group request sent!\n");
	soapconn->read_cb = msn_del_contact_from_group_read_cb;
}

void
msn_del_contact_from_group(MsnContact *contact, const char *passport, const char *group_name)
{
	MsnSoapReq *soap_request;
	MsnUserList * userlist;
	MsnUser *user;
	MsnCallbackState *state;
	gchar *body = NULL, *soap_action, *contact_id_xml;
	const gchar *groupId;
	
	g_return_if_fail(passport != NULL);
	g_return_if_fail(group_name != NULL);
	g_return_if_fail(contact != NULL);
	g_return_if_fail(contact->session != NULL);
	g_return_if_fail(contact->session->userlist != NULL);
	
	userlist = contact->session->userlist;
	
	groupId = msn_userlist_find_group_id(userlist, group_name);
	if (groupId != NULL) {
		purple_debug_info("MSN CL", "Deleting user %s from group %s\n", passport, group_name);
	} else {
		purple_debug_warning("MSN CL", "Unable to retrieve group id from group %s !\n", group_name);
		return;
	}
	
	user = msn_userlist_find_user(userlist, passport);
	
	if (user == NULL) {
		purple_debug_warning("MSN CL", "Unable to retrieve user from passport %s!\n", passport);
		return;
	}

	if ( !strcmp(groupId, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(groupId, MSN_NON_IM_GROUP_ID)) {
		msn_user_remove_group_id(user, groupId);
		return;
	}

	state = msn_callback_state_new();
	msn_callback_state_set_who(state, passport);
	msn_callback_state_set_guid(state, groupId);
	msn_callback_state_set_old_group_name(state, group_name);
		
	contact_id_xml = g_strdup_printf(MSN_CONTACT_ID_XML, user->uid);
	body = g_strdup_printf(MSN_CONTACT_DEL_GROUP_TEMPLATE, contact_id_xml, groupId);
	g_free(contact_id_xml);
	
	/*build SOAP and POST it*/
	soap_action = g_strdup(MSN_CONTACT_DEL_GROUP_SOAP_ACTION);
	
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					    MSN_ADDRESS_BOOK_POST_URL,
					    soap_action,
					    body,
					    state,
					    msn_del_contact_from_group_read_cb,
					    msn_del_contact_from_group_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	
	g_free(soap_action);
	g_free(body);
}


static void
msn_update_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MSN CL","Contact updated successfully\n");
}

static void
msn_update_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSN CL","Update contact information request sent\n");
	soapconn->read_cb = msn_update_contact_read_cb;
}

/* Update a contact's nickname */

void
msn_update_contact(MsnContact *contact, const char* nickname)
{
	MsnSoapReq *soap_request;
	gchar *body = NULL, *escaped_nickname;

	purple_debug_info("MSN CL","Update contact information with new friendly name: %s\n", nickname);
	
	escaped_nickname = g_markup_escape_text(nickname, -1);

	body = g_strdup_printf(MSN_CONTACT_UPDATE_TEMPLATE, escaped_nickname);
	
	g_free(escaped_nickname);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					MSN_CONTACT_UPDATE_SOAP_ACTION,
					body,
					NULL,
					msn_update_contact_read_cb,
					msn_update_contact_written_cb);
	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);

	g_free(body);
}


static void
msn_del_contact_from_list_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnCallbackState *state = NULL;

	g_return_if_fail(soapconn->data_cb != NULL);
	g_return_if_fail(soapconn->session != NULL);
	g_return_if_fail(soapconn->session->contact != NULL);

	state = (MsnCallbackState *) soapconn->data_cb;

	purple_debug_info("MSN CL", "Contact %s deleted successfully from %s list on server!\n", state->who, MsnMemberRole[state->list_id]);

	if (state->list_id == MSN_LIST_PL) {
		msn_add_contact_to_list(soapconn->session->contact, state, state->who, MSN_LIST_RL);
		return;
	}

	if (state->list_id == MSN_LIST_AL) {
		purple_privacy_permit_remove(soapconn->session->account, state->who, TRUE);
		msn_add_contact_to_list(soapconn->session->contact, NULL, state->who, MSN_LIST_BL);
		msn_callback_state_free(state);
		return;
	}

	if (state->list_id == MSN_LIST_BL) {
		purple_privacy_deny_remove(soapconn->session->account, state->who, TRUE);
		msn_add_contact_to_list(soapconn->session->contact, NULL, state->who, MSN_LIST_AL);
		msn_callback_state_free(state);
		return;
	}

	msn_callback_state_free(state);
	msn_soap_free_read_buf(soapconn);
}

static void
msn_del_contact_from_list_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	purple_debug_info("MSN CL","Delete contact from list SOAP request sent!\n");
	soapconn->read_cb = msn_del_contact_from_list_read_cb;
}

void
msn_del_contact_from_list(MsnContact *contact, MsnCallbackState *state,
			  const gchar *passport, const MsnListId list)
{
	MsnSoapReq *soap_request;
	gchar *body = NULL, *member = NULL;
	MsnSoapPartnerScenario partner_scenario;
	MsnUser *user;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(passport != NULL);
	g_return_if_fail(list < 5);

	purple_debug_info("MSN CL", "Deleting contact %s from %s list\n", passport, MsnMemberRole[list]);

	if (state == NULL) {
		state = msn_callback_state_new();
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
	g_free(member);

	soap_request = msn_soap_request_new( MSN_CONTACT_SERVER,
					     MSN_SHARE_POST_URL,
					     MSN_DELETE_MEMBER_FROM_LIST_SOAP_ACTION,
					     body,
					     state,
					     msn_del_contact_from_list_read_cb,
					     msn_del_contact_from_list_written_cb);

	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	
	g_free(body);
}

static void
msn_add_contact_to_list_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnCallbackState *state = NULL;

	g_return_if_fail(soapconn->data_cb != NULL);

	state = (MsnCallbackState *) soapconn->data_cb;
	
	purple_debug_info("MSN CL", "Contact %s added successfully to %s list on server!\n", state->who, MsnMemberRole[state->list_id]);

	if (state->list_id == MSN_LIST_RL && (state->action & MSN_DENIED_BUDDY) ) {
		g_return_if_fail(soapconn->session != NULL);
		g_return_if_fail(soapconn->session->contact != NULL);

		msn_add_contact_to_list(soapconn->session->contact, NULL, state->who, MSN_LIST_BL);
		return;
	}

	if (state->list_id == MSN_LIST_AL) {
		purple_privacy_permit_add(soapconn->session->account, state->who, TRUE);
	} else if (state->list_id == MSN_LIST_BL) {
		purple_privacy_deny_add(soapconn->session->account, state->who, TRUE);
	}

	msn_callback_state_free(state);
	msn_soap_free_read_buf(soapconn);
}


static void
msn_add_contact_to_list_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	purple_debug_info("MSN CL","Add contact to list SOAP request sent!\n");
	soapconn->read_cb = msn_add_contact_to_list_read_cb;
}

void
msn_add_contact_to_list(MsnContact *contact, MsnCallbackState *state,
			const gchar *passport, const MsnListId list)
{
	MsnSoapReq *soap_request;
	gchar *body = NULL, *member = NULL;
	MsnSoapPartnerScenario partner_scenario;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(passport != NULL);
	g_return_if_fail(list < 5);

	purple_debug_info("MSN CL", "Adding contact %s to %s list\n", passport, MsnMemberRole[list]);

	if (state == NULL) {
		state = msn_callback_state_new();
	}
	msn_callback_state_set_list_id(state, list);
	msn_callback_state_set_who(state, passport);

	partner_scenario = (list == MSN_LIST_RL) ? MSN_PS_CONTACT_API : MSN_PS_BLOCK_UNBLOCK;

	member = g_strdup_printf(MSN_MEMBER_PASSPORT_XML, passport);

	body = g_strdup_printf(MSN_CONTACT_ADD_TO_LIST_TEMPLATE, 
			       MsnSoapPartnerScenarioText[partner_scenario],
			       MsnMemberRole[list], 
			       member);

	g_free(member);

	soap_request = msn_soap_request_new( MSN_CONTACT_SERVER,
					     MSN_SHARE_POST_URL,
					     MSN_ADD_MEMBER_TO_LIST_SOAP_ACTION,
					     body,
					     state,
					     msn_add_contact_to_list_read_cb,
					     msn_add_contact_to_list_written_cb);

	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);

	g_free(body);
}


#if 0
static void
msn_gleams_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MSNP14","Gleams read done\n");
}

static void
msn_gleams_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSNP14","finish Group written\n");
	soapconn->read_cb = msn_gleams_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*get the gleams info*/
void
msn_get_gleams(MsnContact *contact)
{
	MsnSoapReq *soap_request;

	purple_debug_info("MSNP14","msn get gleams info...\n");
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					MSN_GET_GLEAMS_SOAP_ACTION,
					MSN_GLEAMS_TEMPLATE,
					NULL,
					msn_gleams_read_cb,
					msn_gleams_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
}
#endif


/***************************************************************
 * Group Operations
 ***************************************************************/

static void
msn_group_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnUserList *userlist;
	MsnCallbackState *state = NULL;
	
	purple_debug_info("MSN CL", "Group request successful.\n");
	
	g_return_if_fail(soapconn->session != NULL);
	g_return_if_fail(soapconn->session->userlist != NULL);
	g_return_if_fail(soapconn->session->contact != NULL);
	
	if (soapconn->data_cb != NULL) {
		state = (MsnCallbackState *) soapconn->data_cb;
		userlist = soapconn->session->userlist;
		
		if (state->action & MSN_RENAME_GROUP) {
			msn_userlist_rename_group_id(soapconn->session->userlist,
						     state->guid,
						     state->new_group_name);
		}
		
		if (state->action & MSN_ADD_GROUP) {
			gchar *guid, *endguid;
			
			guid = g_strstr_len(soapconn->read_buf, soapconn->read_len, "<guid>");
			guid += 6;
			endguid = g_strstr_len(soapconn->read_buf, soapconn->read_len, "</guid>");
			*endguid = '\0';
			/* create and add the new group to the userlist */
			purple_debug_info("MSN CL", "Adding group %s with guid = %s to the userlist\n", state->new_group_name, guid);
			msn_group_new(soapconn->session->userlist, guid, state->new_group_name);

			if (state->action & MSN_ADD_BUDDY) {
				msn_userlist_add_buddy(soapconn->session->userlist,
						       state->who,
						       state->new_group_name);
				msn_callback_state_free(state);
				return;
			}
			
			if (state->action & MSN_MOVE_BUDDY) {
				msn_add_contact_to_group(soapconn->session->contact, state, state->who, guid); 
				return;
			}
		}
		
		if (state->action & MSN_DEL_GROUP) {
			GList *l;
			
			msn_userlist_remove_group_id(soapconn->session->userlist, state->guid);
			for (l = userlist->users; l != NULL; l = l->next) {
				msn_user_remove_group_id( (MsnUser *)l->data, state->guid);
			}
			
		}
			
		msn_callback_state_free(state);
	}
	
	msn_soap_free_read_buf(soapconn);
}

static void
msn_group_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MSN CL","Sent group request.\n");
	soapconn->read_cb = msn_group_read_cb;
}

/* add group */
void
msn_add_group(MsnSession *session, MsnCallbackState *state, const char* group_name)
{
	MsnSoapReq *soap_request;
	MsnContact *contact;
	char *body = NULL;
	gchar *escaped_group_name;

	g_return_if_fail(session != NULL);
	g_return_if_fail(group_name != NULL);
	
	contact = session->contact;
	purple_debug_info("MSN CL","Adding group %s to contact list.\n", group_name);

	if (state == NULL) {
		state = msn_callback_state_new();
	}

	msn_callback_state_set_action(state, MSN_ADD_GROUP);
	msn_callback_state_set_new_group_name(state, group_name);

	/* escape group name's html special chars so it can safely be sent
	* in a XML SOAP request
	*/
	escaped_group_name = g_markup_escape_text(group_name, -1);
	body = g_strdup_printf(MSN_GROUP_ADD_TEMPLATE, escaped_group_name);
	g_free(escaped_group_name);

	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,
					MSN_GROUP_ADD_SOAP_ACTION,
					body,
					state,
					msn_group_read_cb,
					msn_group_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	
	g_free(body);
}

/* delete group */
void
msn_del_group(MsnSession *session, const gchar *group_name)
{
	MsnSoapReq *soap_request;
	MsnContact *contact;
	MsnCallbackState *state;
	char *body = NULL;
	const gchar *guid;

	g_return_if_fail(session != NULL);
	
	g_return_if_fail(group_name != NULL);
	contact = session->contact;
	purple_debug_info("MSN CL","Deleting group %s from contact list\n", group_name);
	
	guid = msn_userlist_find_group_id(session->userlist, group_name);
	
	/* if group uid we need to del is NULL, 
	*  we need to delete nothing
	*/
	if (guid == NULL) {
		purple_debug_info("MSN CL", "Group %s guid not found, returning.\n", group_name);
		return;
	}

	if ( !strcmp(guid, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(guid, MSN_NON_IM_GROUP_ID) ) {
		// XXX add back PurpleGroup since it isn't really removed in the server?
		return;
	}

	state = msn_callback_state_new();
	msn_callback_state_set_action(state, MSN_DEL_GROUP);
	msn_callback_state_set_guid(state, guid);
	
	body = g_strdup_printf(MSN_GROUP_DEL_TEMPLATE, guid);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					    MSN_ADDRESS_BOOK_POST_URL,
					    MSN_GROUP_DEL_SOAP_ACTION,
					    body,
					    state,
					    msn_group_read_cb,
					    msn_group_written_cb);
	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);

	g_free(body);
}

/* rename group */
void
msn_contact_rename_group(MsnSession *session, const char *old_group_name, const char *new_group_name)
{
	MsnSoapReq *soap_request;
	MsnContact *contact;
	gchar * escaped_group_name, *body = NULL;
	const gchar * guid;
	MsnCallbackState *state = msn_callback_state_new();
	
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->userlist != NULL);
	g_return_if_fail(old_group_name != NULL);
	g_return_if_fail(new_group_name != NULL);
	
	contact = session->contact;
	purple_debug_info("MSN CL", "Renaming group %s to %s.\n", old_group_name, new_group_name);
	
	guid = msn_userlist_find_group_id(session->userlist, old_group_name);
	if (guid == NULL)
		return;

	msn_callback_state_set_guid(state, guid);
	msn_callback_state_set_new_group_name(state, new_group_name);

	if ( !strcmp(guid, MSN_INDIVIDUALS_GROUP_ID) || !strcmp(guid, MSN_NON_IM_GROUP_ID) ) {
		msn_add_group(session, state, new_group_name);
		// XXX move every buddy there (we probably need to fix concurrent SOAP reqs first)
	}

	msn_callback_state_set_action(state, MSN_RENAME_GROUP);
	
	/* escape group name's html special chars so it can safely be sent
	 * in a XML SOAP request
	*/
	escaped_group_name = g_markup_escape_text(new_group_name, -1);
	
	body = g_strdup_printf(MSN_GROUP_RENAME_TEMPLATE, guid, escaped_group_name);
	
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					    MSN_ADDRESS_BOOK_POST_URL,
					    MSN_GROUP_RENAME_SOAP_ACTION,
					    body,
					    state,
					    msn_group_read_cb,
					    msn_group_written_cb);
	msn_soap_post(contact->soapconn, soap_request, msn_contact_connect_init);
	
	g_free(escaped_group_name);
	g_free(body);
}

void
msn_contact_connect_init(MsnSoapConn *soapconn)
{
	msn_soap_init(soapconn, MSN_CONTACT_SERVER, 1,
					msn_contact_login_connect_cb,
					msn_contact_login_error_cb);
}
