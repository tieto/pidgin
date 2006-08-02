/**
 * @file contact.h			Header file for contact.c
 *	Author
 * 		MaYuan<mayuan2006@gmail.com>
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
#ifndef _MSN_CONTACT_H_
#define _MSN_CONTACT_H_

#define MSN_CONTACT_SERVER	"contacts.msn.com"

/*get contact list soap request template*/
#define MSN_GET_CONTACT_POST_URL	"/abservice/SharingService.asmx"
#define MSN_GET_CONTACT_SOAP_ACTION "http://www.msn.com/webservices/AddressBook/FindMembership"
#define MSN_GET_CONTACT_TEMPLATE	"<?xml version='1.0' encoding='utf-8'?>"\
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"\
	"<soap:Header xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"\
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<ApplicationId xmlns=\"http://www.msn.com/webservices/AddressBook\">09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId>"\
			"<IsMigration xmlns=\"http://www.msn.com/webservices/AddressBook\">false</IsMigration>"\
			"<PartnerScenario xmlns=\"http://www.msn.com/webservices/AddressBook\">Initial</PartnerScenario>"\
		 "</ABApplicationHeader>"\
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
		"<ManagedGroupRequest xmlns=\"http://www.msn.com/webservices/AddressBook\">false</ManagedGroupRequest>"\
		"</ABAuthHeader>"\
	"</soap:Header>"\
	"<soap:Body xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"\
		"<FindMembership xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<serviceFilter xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<Types xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
				"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Messenger</ServiceType>"\
				"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Invitation</ServiceType>"\
				"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">SocialNetwork</ServiceType>"\
				"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Space</ServiceType>"\
				"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Profile</ServiceType>"\
			"</Types>"\
			"</serviceFilter>"\
		"</FindMembership>"\
	"</soap:Body>"\
"</soap:Envelope>"

/*get addressbook soap request template*/
#define MSN_GET_ADDRESS_SOAP_ACTION	"http://www.msn.com/webservices/AddressBook/ABFindAll"
#define MSN_GET_ADDRESS_POST_URL	"/abservice/abservice.asmx"
#define MSN_GET_ADDRESS_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?>"\
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"\
	"<soap:Header>"\
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId>"\
			"<IsMigration>false</IsMigration>"\
			"<PartnerScenario>Initial</PartnerScenario>"\
		"</ABApplicationHeader>"\
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<ManagedGroupRequest>false</ManagedGroupRequest>"\
		"</ABAuthHeader>"\
	"</soap:Header>"\
	"<soap:Body>"\
		"<ABFindAll xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<abId>00000000-0000-0000-0000-000000000000</abId>"\
			"<abView>Full</abView>"\
		"</ABFindAll>"\
	"</soap:Body>"\
"</soap:Envelope>"

/*Gleams SOAP request template*/
#define MSN_GLEAMS_TEMPLATE "<?xml version=\"1.0\" encoding=\"utf-8\"?>"\
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"\
	"<soap:Header>"\
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId>"\
			"<IsMigration>false</IsMigration>"\
			"<PartnerScenario>Initial</PartnerScenario>"\
		"</ABApplicationHeader>"\
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<ManagedGroupRequest>false</ManagedGroupRequest>"\
		"</ABAuthHeader>"\
	"</soap:Header>"\
	"<soap:Body>"\
		"<ABFindAll xmlns=\"http://www.msn.com/webservices/AddressBook\">"\
			"<abId>00000000-0000-0000-0000-000000000000</abId>"\
			"<abView>Full</abView>"\
			"<dynamicItemView>Gleam</dynamicItemView>"\
			"<dynamicItemLastChange>0001-01-01T00:00:00.0000000-08:00</dynamicItemLastChange>"\
		"</ABFindAll>"\
	"</soap:Body>"\
"</soap:Envelope>"

/*add conatct soap request*/
#define MSN_CONTACT_ADD_SOAP_ACTION	"http://www.msn.com/webservices/AddressBook/ABContactAdd"
#define MSN_CONTACT_XML	"<Contact xmlns=\"http://www.msn.com/webservices/AddressBook\"><contactInfo><contactType>LivePending</contactType><passportName>%s</passportName><isMessengerUser>true</isMessengerUser></contactInfo></Contact>"

#define MSN_ADD_CONTACT_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>ContactSave</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><ABContactAdd xmlns=\"http://www.msn.com/webservices/AddressBook\"><abId>00000000-0000-0000-0000-000000000000</abId><contacts>%s</contacts><options><EnableAllowListManagement>true</EnableAllowListManagement></options></ABContactAdd></soap:Body></soap:Envelope>"

/*delete contact from contact list soap request template*/
#define MSN_CONTACT_DEL_SOAP_ACTION	"http://www.msn.com/webservices/AddressBook/ABContactDelete"
#define MSN_CONTACTS_DEL		"<Contact><contactId>5e8a2e64-c271-443f-ac86-2429f3ffd18a</contactId></Contact>"
#define MSN_DEL_CONTACT_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>Timer</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><ABContactDelete xmlns=\"http://www.msn.com/webservices/AddressBook\"><abId>00000000-0000-0000-0000-000000000000</abId><contacts>%s</contacts></ABContactDelete></soap:Body></soap:Envelope>"

#define MSN_MEMBER_TEMPLATE		"<Member xsi:type=\"PassportMember\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><Type>Passport</Type><State>Accepted</State><PassportName>%s</PassportName></Member>"

/*block means add contact to block list*/
#define MSN_CONTACT_BLOCK_SOAP_ACTION	"http://www.msn.com/webservices/AddressBook/AddMember"
#define MSN_BLOCK_CONTACT_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>BlockUnblock</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><AddMember xmlns=\"http://www.msn.com/webservices/AddressBook\"><serviceHandle><Id>0</Id><Type>Messenger</Type><ForeignId></ForeignId></serviceHandle><memberships><Membership><MemberRole>Block</MemberRole><Members>%s</Members></Membership></memberships></AddMember></soap:Body></soap:Envelope>"

/*unblock means delete contact to block list*/
#define MSN_CONTACT_UNBLOCK_SOAP_ACTION	"http://www.msn.com/webservices/AddressBook/DeleteMember"
#define MSN_UNBLOCK_CONTACT_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>BlockUnblock</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><DeleteMember xmlns=\"http://www.msn.com/webservices/AddressBook\"><serviceHandle><Id>0</Id><Type>Messenger</Type><ForeignId></ForeignId></serviceHandle><memberships><Membership><MemberRole>Block</MemberRole><Members>%s</Members></Membership></memberships></DeleteMember></soap:Body></soap:Envelope>"

typedef struct _MsnContact MsnContact;

struct _MsnContact
{
	MsnSession *session;

	MsnSoapConn *soapconn;
};

/*function prototype*/
MsnContact * msn_contact_new(MsnSession *session);
void msn_contact_destroy(MsnContact *contact);

void msn_contact_connect(MsnContact *contact);
void msn_get_contact_list(MsnContact * contact);
void msn_get_address_book(MsnContact *contact);

#endif/* _MSN_CMDPROC_H_*/

