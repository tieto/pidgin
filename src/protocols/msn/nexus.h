/**
 * @file nexus.h MSN Nexus functions
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
#ifndef _MSN_NEXUS_H_
#define _MSN_NEXUS_H_

#include "soap.h"

#define MSN_TWN_SERVER	"loginnet.passport.com"

#define TWN_START_TOKEN		"<wsse:BinarySecurityToken Id=\"PPToken1\">"
#define TWN_END_TOKEN		"</wsse:BinarySecurityToken>"

#define TWN_POST_URL			"/RST.srf"
#define TWN_ENVELOP_TEMPLATE 	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"\
						"<Envelope xmlns=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:wsse=\"http://schemas.xmlsoap.org/ws/2003/06/secext\" xmlns:saml=\"urn:oasis:names:tc:SAML:1.0:assertion\" xmlns:wsp=\"http://schemas.xmlsoap.org/ws/2002/12/policy\" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\" xmlns:wssc=\"http://schemas.xmlsoap.org/ws/2004/04/sc\" xmlns:wst=\"http://schemas.xmlsoap.org/ws/2004/04/trust\">"\
						"<Header>"\
						"<ps:AuthInfo xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\" Id=\"PPAuthInfo\">"\
						"<ps:HostingApp>{3:B}</ps:HostingApp>"\
						"<ps:BinaryVersion>4</ps:BinaryVersion>"\
						"<ps:UIVersion>1</ps:UIVersion>"\
						"<ps:Cookies></ps:Cookies>"\
						"<ps:RequestParams>AQAAAAIAAABsYwQAAAAzMDg0</ps:RequestParams>"\
						"</ps:AuthInfo>"\
						"<wsse:Security>"\
						"<wsse:UsernameToken Id=\"user\">"\
						"<wsse:Username>%s</wsse:Username>"\
						"<wsse:Password>%s</wsse:Password>"\
						"</wsse:UsernameToken>"\
						"</wsse:Security>"\
						"</Header><Body>"\
						"<ps:RequestMultipleSecurityTokens xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\" Id=\"RSTS\">"\
						"<wst:RequestSecurityToken Id=\"RST0\">"\
						"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"\
						"<wsp:AppliesTo>"\
						"<wsa:EndpointReference>"\
						"<wsa:Address>http://Passport.NET/tb</wsa:Address>"\
						"</wsa:EndpointReference>"\
						"</wsp:AppliesTo>"\
						"</wst:RequestSecurityToken>"\
						"<wst:RequestSecurityToken Id=\"RST1\">"\
						"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"\
						"<wsp:AppliesTo>"\
						"<wsa:EndpointReference>"\
						"<wsa:Address>messenger.msn.com</wsa:Address>"\
						"</wsa:EndpointReference>"\
						"</wsp:AppliesTo>"\
						"<wsse:PolicyReference URI=\"?%s\">"\
						"</wsse:PolicyReference>"\
						"</wst:RequestSecurityToken>"\
						"</ps:RequestMultipleSecurityTokens>"\
						"</Body>"\
						"</Envelope>"

typedef struct _MsnNexus MsnNexus;

struct _MsnNexus
{
	MsnSession *session;
	MsnSoapConn *soapconn;
	char * challenge_data_str;
	GHashTable *challenge_data;
};

void msn_nexus_connect(MsnNexus *nexus);
MsnNexus *msn_nexus_new(MsnSession *session);
void msn_nexus_destroy(MsnNexus *nexus);

#endif /* _MSN_NEXUS_H_ */
