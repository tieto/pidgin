/**
 * @file nexus.c MSN Nexus functions
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
#include "msn.h"
#include "soap2.h"
#include "nexus.h"
#include "notification.h"

#undef NEXUS_LOGIN_TWN

/**************************************************************************
 * Main
 **************************************************************************/

MsnNexus *
msn_nexus_new(MsnSession *session)
{
	MsnNexus *nexus;

	nexus = g_new0(MsnNexus, 1);
	nexus->session = session;

	nexus->challenge_data = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, g_free);

	return nexus;
}

void
msn_nexus_destroy(MsnNexus *nexus)
{
	if (nexus->challenge_data != NULL)
		g_hash_table_destroy(nexus->challenge_data);

	g_free(nexus);
}

/**************************************************************************
 * Login
 **************************************************************************/

static void
nexus_got_response_cb(MsnSoapMessage *req, MsnSoapMessage *resp, gpointer data)
{
	MsnNexus *nexus = data;
	MsnSession *session = nexus->session;
	xmlnode *node;

	if (resp == NULL) {
		msn_session_set_error(session, MSN_ERROR_SERVCONN, _("Windows Live ID authentication:Unable to connect"));
		return;
	}

	node = msn_soap_xml_get(resp->xml,	"Body/"
		"RequestSecurityTokenResponseCollection/RequestSecurityTokenResponse");

	for (; node; node = node->next) {
		xmlnode *token = msn_soap_xml_get(node,
			"RequestedSecurityToken/BinarySecurityToken");

		if (token) {
			char *token_str = xmlnode_get_data(token);
			char **elems, **cur, **tokens;
			char *msn_twn_t, *msn_twn_p, *cert_str;

			if (token_str == NULL) continue;

			elems = g_strsplit(token_str, "&", 0);

			for (cur = elems; *cur != NULL; cur++){
				tokens = g_strsplit(*cur, "=", 2);
				g_hash_table_insert(nexus->challenge_data, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
			}

			g_free(token_str);
			g_strfreev(elems);

			msn_twn_t = g_hash_table_lookup(nexus->challenge_data, "t");
			msn_twn_p = g_hash_table_lookup(nexus->challenge_data, "p");

			/*setup the t and p parameter for session*/
			if (session->passport_info.t != NULL){
				g_free(session->passport_info.t);
			}
			session->passport_info.t = g_strdup(msn_twn_t);

			if (session->passport_info.p != NULL)
				g_free(session->passport_info.p);
			session->passport_info.p = g_strdup(msn_twn_p);

			cert_str = g_strdup_printf("t=%s&p=%s",msn_twn_t,msn_twn_p);
			msn_got_login_params(session, cert_str);

			purple_debug_info("MSN Nexus","Close nexus connection!\n");
			g_free(cert_str);
			msn_nexus_destroy(nexus);
			session->nexus = NULL;

			return;
		}
	}

	/* we must have failed! */
	msn_session_set_error(session, MSN_ERROR_AUTH, _("Windows Live ID authentication: cannot find authenticate token in server response"));
}

/*when connect, do the SOAP Style windows Live ID authentication */
void
msn_nexus_connect(MsnNexus *nexus)
{
	MsnSession *session = nexus->session;
	char *ru,*lc,*id,*tw,*ct,*kpp,*kv,*ver,*rn,*tpf;
	char *fs0,*fs;
	char *username, *password;
	char *tail;
#ifdef NEXUS_LOGIN_TWN
	char *challenge_str;
#else
	char *rst1_str,*rst2_str,*rst3_str;
#endif

	MsnSoapMessage *soap;

	purple_debug_info("MSN Nexus","Starting Windows Live ID authentication\n");
	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

	/*prepare the Windows Live ID authentication token*/
	username = g_strdup(purple_account_get_username(session->account));
	password = g_strndup(purple_connection_get_password(session->account->gc), 16);

	lc =	(char *)g_hash_table_lookup(nexus->challenge_data, "lc");
	id =	(char *)g_hash_table_lookup(nexus->challenge_data, "id");
	tw =	(char *)g_hash_table_lookup(nexus->challenge_data, "tw");
	fs0=	(char *)g_hash_table_lookup(nexus->challenge_data, "fs");
	ru =	(char *)g_hash_table_lookup(nexus->challenge_data, "ru");
	ct =	(char *)g_hash_table_lookup(nexus->challenge_data, "ct");
	kpp=	(char *)g_hash_table_lookup(nexus->challenge_data, "kpp");
	kv =	(char *)g_hash_table_lookup(nexus->challenge_data, "kv");
	ver=	(char *)g_hash_table_lookup(nexus->challenge_data, "ver");
	rn =	(char *)g_hash_table_lookup(nexus->challenge_data, "rn");
	tpf=	(char *)g_hash_table_lookup(nexus->challenge_data, "tpf");

	/*
	 * add some fail-safe code to avoid windows Purple Crash bug #1540454
	 * If any of these string is NULL, will return Authentication Fail!
	 * for when windows g_strdup_printf() implementation get NULL point,It crashed!
	 */
	if(!(lc && id && tw && ru && ct && kpp && kv && ver && tpf)){
		purple_debug_error("MSN Nexus","WLM Authenticate Key Error!\n");
		msn_session_set_error(session, MSN_ERROR_AUTH, _("Windows Live ID authentication Failed"));
		g_free(username);
		g_free(password);
		msn_nexus_destroy(nexus);
		session->nexus = NULL;
		return;
	}

	/*
	 * in old MSN NS server's "USR TWN S" return,didn't include fs string
	 * so we use a default "1" for fs.
	 */
	if(fs0){
		fs = g_strdup(fs0);
	}else{
		fs = g_strdup("1");
	}

#ifdef NEXUS_LOGIN_TWN
	challenge_str = g_strdup_printf(
		"lc=%s&amp;id=%s&amp;tw=%s&amp;fs=%s&amp;ru=%s&amp;ct=%s&amp;kpp=%s&amp;kv=%s&amp;ver=%s&amp;rn=%s&amp;tpf=%s\r\n",
		lc,id,tw,fs,ru,ct,kpp,kv,ver,rn,tpf
		);

	/*build the SOAP windows Live ID XML body */
	tail = g_strdup_printf(TWN_ENVELOP_TEMPLATE, username, password, challenge_str);
	g_free(challenge_str);
#else
	rst1_str = g_strdup_printf(
		"id=%s&amp;tw=%s&amp;fs=%s&amp;kpp=%s&amp;kv=%s&amp;ver=%s&amp;rn=%s",
		id,tw,fs,kpp,kv,ver,rn
		);
	rst2_str = g_strdup_printf(
		"fs=%s&amp;id=%s&amp;kv=%s&amp;rn=%s&amp;tw=%s&amp;ver=%s",
		fs,id,kv,rn,tw,ver
		);
	rst3_str = g_strdup_printf("id=%s",id);
	tail = g_strdup_printf(TWN_LIVE_ENVELOP_TEMPLATE,username,password,rst1_str,rst2_str,rst3_str);
	g_free(rst1_str);
	g_free(rst2_str);
	g_free(rst3_str);
#endif
	g_free(fs);
	g_free(password);

	soap = msn_soap_message_new(NULL, xmlnode_from_str(tail, -1));
	g_free(tail);
	msn_soap_message_send(nexus->session, soap, MSN_TWN_SERVER, TWN_POST_URL,
		nexus_got_response_cb, nexus);
}

