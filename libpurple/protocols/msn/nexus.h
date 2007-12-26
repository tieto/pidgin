/**
 * @file nexus.h MSN Nexus functions
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _MSN_NEXUS_H_
#define _MSN_NEXUS_H_

#include "soap.h"

/* Index into ticket_tokens in nexus.c Keep updated! */
typedef enum
{
	MSN_AUTH_MESSENGER     = 0,
	MSN_AUTH_MESSENGER_WEB = 1,
	MSN_AUTH_CONTACTS      = 2,
	MSN_AUTH_LIVE_SECURE   = 3,
	MSN_AUTH_SPACES        = 4,
	MSN_AUTH_LIVE_CONTACTS = 5,
	MSN_AUTH_STORAGE       = 6
} MsnAuthDomains;

#define MSN_SSO_SERVER	"login.live.com"
#define SSO_POST_URL	"/RST.srf"

#define MSN_SSO_RST_TEMPLATE \
"<wst:RequestSecurityToken Id=\"RST%d\">"\
	"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"\
	"<wsp:AppliesTo>"\
		"<wsa:EndpointReference>"\
			"<wsa:Address>%s</wsa:Address>"\
		"</wsa:EndpointReference>"\
	"</wsp:AppliesTo>"\
	"<wsse:PolicyReference URI=\"%s\"></wsse:PolicyReference>"\
"</wst:RequestSecurityToken>"

#define MSN_SSO_TEMPLATE "<?xml version='1.0' encoding='utf-8'?>"\
"<Envelope xmlns=\"http://schemas.xmlsoap.org/soap/envelope/\""\
	" xmlns:wsse=\"http://schemas.xmlsoap.org/ws/2003/06/secext\""\
	" xmlns:saml=\"urn:oasis:names:tc:SAML:1.0:assertion\""\
	" xmlns:wsp=\"http://schemas.xmlsoap.org/ws/2002/12/policy\""\
	" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\""\
	" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\""\
	" xmlns:wssc=\"http://schemas.xmlsoap.org/ws/2004/04/sc\""\
	" xmlns:wst=\"http://schemas.xmlsoap.org/ws/2004/04/trust\">"\
	"<Header>"\
		"<ps:AuthInfo"\
			" xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\""\
			" Id=\"PPAuthInfo\">"\
			"<ps:HostingApp>{7108E71A-9926-4FCB-BCC9-9A9D3F32E423}</ps:HostingApp>"\
			"<ps:BinaryVersion>4</ps:BinaryVersion>"\
			"<ps:UIVersion>1</ps:UIVersion>"\
			"<ps:Cookies></ps:Cookies>"\
			"<ps:RequestParams>AQAAAAIAAABsYwQAAAAxMDMz</ps:RequestParams>"\
		"</ps:AuthInfo>"\
		"<wsse:Security>"\
			"<wsse:UsernameToken Id=\"user\">"\
				"<wsse:Username>%s</wsse:Username>"\
				"<wsse:Password>%s</wsse:Password>"\
			"</wsse:UsernameToken>"\
		"</wsse:Security>"\
	"</Header>"\
	"<Body>"\
		"<ps:RequestMultipleSecurityTokens"\
			" xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\""\
			" Id=\"RSTS\">"\
			"<wst:RequestSecurityToken Id=\"RST0\">"\
				"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"\
				"<wsp:AppliesTo>"\
					"<wsa:EndpointReference>"\
						"<wsa:Address>http://Passport.NET/tb</wsa:Address>"\
					"</wsa:EndpointReference>"\
				"</wsp:AppliesTo>"\
			"</wst:RequestSecurityToken>"\
			"%s"	/* Other RSTn tokens */\
		"</ps:RequestMultipleSecurityTokens>"\
	"</Body>"\
"</Envelope>"

typedef struct _MsnUsrKey MsnUsrKey;
struct _MsnUsrKey
{
	int size; // 28. Does not count data
	int crypt_mode; // CRYPT_MODE_CBC (1)
	int cipher_type; // TripleDES (0x6603)
	int hash_type; // SHA1 (0x8004)
	int iv_len;    // 8
	int hash_len;  // 20
	int cipher_len; // 72
	// Data
	char iv[8];
	char hash[20];
	char cipher[72];
};

typedef struct _MsnTicketToken MsnTicketToken;
struct _MsnTicketToken {
	GHashTable *token;
	char *secret;
	time_t expiry;
};

typedef struct _MsnNexus MsnNexus;

struct _MsnNexus
{
	MsnSession *session;
	char *policy;
	char *nonce;

	MsnTicketToken *tokens;
	int token_len;
};

void msn_nexus_connect(MsnNexus *nexus);
MsnNexus *msn_nexus_new(MsnSession *session);
void msn_nexus_destroy(MsnNexus *nexus);
GHashTable *msn_nexus_get_token(MsnNexus *session, MsnAuthDomains id);
const char *msn_nexus_get_token_str(MsnNexus *session, MsnAuthDomains id);

#endif /* _MSN_NEXUS_H_ */

