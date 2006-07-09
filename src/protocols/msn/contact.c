/**
 * @file contact.c 
 * 	get MSN contacts via SOAP request
 *	created by MaYuan<mayuan2006@gmail.com>
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#include "soap.h"
#include "contact.h"
#include "xmlnode.h"

/*new a contact*/
MsnContact *
msn_contact_new(MsnSession *session)
{
	MsnContact *contact;

	contact = g_new0(MsnContact, 1);
	contact->session = session;
	contact->soapconn = msn_soap_new(session);
	contact->soapconn->parent = contact;
	contact->soapconn->ssl_conn = 1;

	return contact;
}

/*destroy the contact*/
void
msn_contact_destroy(MsnContact *contact)
{
	msn_soap_destroy(contact->soapconn);
	g_free(contact);
}

/*contact SOAP server login error*/
static void
msn_contact_login_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to contact server"));
}

/*msn contact SOAP server connect process*/
static void
msn_contact_login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnContact *contact;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);

	session = contact->session;
	g_return_if_fail(session != NULL);

	/*login ok!We can retrieve the contact list*/
	msn_get_contact_list(contact);
}

static void
msn_parse_contact_list(MsnContact * contact)
{
	xmlnode * node,*envelop,*body,*response,*result,*services,*service,*memberships;
	xmlnode *membershipnode,*members,*member,*passport,*role;
	int len;

	gaim_debug_misc("xml","parse contact list:{%s}\nsize:%d\n",contact->soapconn->body,contact->soapconn->body_len);
	node = 	xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);
//	node = 	xmlnode_from_str(contact->soapconn->body, -1);

	if(node == NULL){
		gaim_debug_misc("xml","parse from str err!\n");
		return;
	}
	gaim_debug_misc("xml","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");
	gaim_debug_misc("xml","body{%p},name:%s\n",body,body->name);
	response = xmlnode_get_child(body,"FindMembershipResponse");
	gaim_debug_misc("xml","response{%p},name:%s\n",response,response->name);
	result =xmlnode_get_child(response,"FindMembershipResult");
	gaim_debug_misc("xml","result{%p},name:%s\n",result,result->name);
	services =xmlnode_get_child(result,"Services");
	gaim_debug_misc("xml","services{%p},name:%s\n",services,services->name);
	service =xmlnode_get_child(services,"Service");
	gaim_debug_misc("xml","service{%p},name:%s\n",service,service->name);
	memberships =xmlnode_get_child(service,"Memberships");
	gaim_debug_misc("xml","memberships{%p},name:%s\n",memberships,memberships->name);
	for(membershipnode = xmlnode_get_child(memberships, "Membership"); membershipnode;
					membershipnode = xmlnode_get_next_twin(membershipnode)){
		role = xmlnode_get_child(membershipnode,"MemberRole");
		gaim_debug_misc("memberrole","role:%s\n",xmlnode_get_data(role));
		members = xmlnode_get_child(membershipnode,"Members");
			for(member = xmlnode_get_child(members, "Member"); member;
					member = xmlnode_get_next_twin(member)){
				passport = xmlnode_get_child(member,"PassportName");
				gaim_debug_misc("Passport","name:%s\n",xmlnode_get_data(passport));
			}
	}

	xmlnode_free(node);
}

static void
msn_get_contact_list_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnContact *contact;
	MsnSession *session;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

//	gaim_debug_misc("msn", "soap contact server Reply: {%s}\n", soapconn->read_buf);
	msn_parse_contact_list(contact);
}

static void
msn_contact_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish contact written\n");
	soapconn->read_cb = msn_get_contact_list_cb;
	msn_soap_read_cb(data,source,cond);
}

void
msn_get_contact_list(MsnContact * contact)
{
	char * soap_head = NULL;
	char * soap_body = NULL;
	char * request_str = NULL;

	gaim_debug_info("MaYuan","msn_get_contact_list()...\n");
	contact->soapconn->login_path = g_strdup(MSN_GET_CONTACT_POST_URL);
	soap_body = g_strdup_printf(MSN_GET_CONTACT_TEMPLATE);
	soap_head = g_strdup_printf(
					"POST %s HTTP/1.1\r\n"
					"SOAPAction: http://www.msn.com/webservices/AddressBook/FindMembership\r\n"
					"Content-Type:text/xml; charset=utf-8\r\n"
					"Cookie: MSPAuth=%s\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
					"Accept: text/*\r\n"
					"Host: %s\r\n"
					"Content-Length: %d\r\n"
					"Connection: Keep-Alive\r\n"
					"Cache-Control: no-cache\r\n\r\n",
					contact->soapconn->login_path,
					contact->session->passport_info.mspauth,
					contact->soapconn->login_host,
					strlen(soap_body)
					);
	request_str = g_strdup_printf("%s%s", soap_head,soap_body);
	g_free(soap_head);
	g_free(soap_body);

//	gaim_debug_info("MaYuan","send to contact server{%s}\n",request_str);
	msn_soap_write(contact->soapconn,request_str,msn_contact_written_cb);
}

msn_add_contact()
{
}

msn_delete_contact()
{
}

msn_block_contact()
{
}

msn_unblock_contact()
{
}

msn_get_gleams()
{
}

void
msn_contact_connect(MsnContact *contact)
{
	/*  Authenticate via Windows Live ID. */
	gaim_debug_info("MaYuan","msn_contact_connect...\n");

	msn_soap_init(contact->soapconn,MSN_CONTACT_SERVER,1,
					msn_contact_login_connect_cb,
					msn_contact_login_error_cb);
}

