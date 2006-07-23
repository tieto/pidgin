/**
 * @file nexus.c MSN Nexus functions
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
#include "nexus.h"
#include "notification.h"

/**************************************************************************
 * Main
 **************************************************************************/

MsnNexus *
msn_nexus_new(MsnSession *session)
{
	MsnNexus *nexus;

	nexus = g_new0(MsnNexus, 1);
	nexus->session = session;
	/*we must use SSL connection to do Windows Live ID authentication*/
	nexus->soapconn = msn_soap_new(session,nexus,1);

	nexus->challenge_data = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, g_free);

	return nexus;
}

void
msn_nexus_destroy(MsnNexus *nexus)
{
	if (nexus->challenge_data != NULL)
		g_hash_table_destroy(nexus->challenge_data);

	msn_soap_destroy(nexus->soapconn);
	g_free(nexus);
}

/**************************************************************************
 * Login
 **************************************************************************/

static void
login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond);

static void
login_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{
	MsnSoapConn * soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_AUTH, _("Windows Live ID authentication:Unable to connect"));
	/* the above line will result in nexus being destroyed, so we don't want
	 * to destroy it here, or we'd crash */
}

static void
nexus_login_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnNexus *nexus;
	MsnSession *session;

	nexus = soapconn->parent;
	g_return_if_fail(nexus != NULL);
	session = nexus->session;
	g_return_if_fail(session != NULL);

//	gaim_debug_misc("msn", "TWN Server Reply: {%s}\n", soapconn->read_buf);

	if (strstr(soapconn->read_buf, "HTTP/1.1 302") != NULL){
		/* Redirect. */
		char *location, *c;

		location = strstr(soapconn->read_buf, "Location: ");
		if (location == NULL){
			msn_soap_free_read_buf(soapconn);
			return;
		}
		location = strchr(location, ' ') + 1;

		if ((c = strchr(location, '\r')) != NULL)
			*c = '\0';

		/* Skip the http:// */
		if ((c = strchr(location, '/')) != NULL)
			location = c + 2;

		if ((c = strchr(location, '/')) != NULL){
			g_free(soapconn->login_path);
			soapconn->login_path = g_strdup(c);

			*c = '\0';
		}
		g_free(soapconn->login_host);

		msn_soap_init(soapconn,location,1,login_connect_cb,login_error_cb);
	}else if (strstr(soapconn->read_buf, "HTTP/1.1 401 Unauthorized") != NULL){
		const char *error;

		if ((error = strstr(soapconn->read_buf, "WWW-Authenticate")) != NULL)	{
			if ((error = strstr(error, "cbtxt=")) != NULL){
				const char *c;
				char *temp;

				error += strlen("cbtxt=");

				if ((c = strchr(error, '\n')) == NULL)
					c = error + strlen(error);

				temp = g_strndup(error, c - error);
				error = gaim_url_decode(temp);
				g_free(temp);
			}
		}
		msn_session_set_error(session, MSN_ERROR_AUTH, error);
	}else if (strstr(soapconn->read_buf, "HTTP/1.1 200 OK")){
		/*reply OK, we should process the SOAP body*/
		char *base, *c;
		char *msn_twn_t,*msn_twn_p;
		char *login_params;

		char **elems, **cur, **tokens;
		char * cert_str;

		gaim_debug_info("MaYuan","Windows Live ID Reply OK!\n");

		//TODO: we should parse it using XML
		base  = g_strstr_len(soapconn->read_buf, soapconn->read_len, TWN_START_TOKEN);
		base += strlen(TWN_START_TOKEN);
		c     = g_strstr_len(soapconn->read_buf, soapconn->read_len, TWN_END_TOKEN);
		login_params = g_strndup(base, c - base);

//		gaim_debug_info("msn", "TWN Cert: {%s}\n", login_params);

		/* Parse the challenge data. */
		elems = g_strsplit(login_params, "&amp;", 0);

		for (cur = elems; *cur != NULL; cur++){
				tokens = g_strsplit(*cur, "=", 2);
				g_hash_table_insert(nexus->challenge_data, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
		}

		g_strfreev(elems);

		msn_twn_t = (char *)g_hash_table_lookup(nexus->challenge_data, "t");
		msn_twn_p = (char *)g_hash_table_lookup(nexus->challenge_data, "p");

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

		gaim_debug_info("MaYuan","close nexus connection! \n");
		g_free(cert_str);
		g_free(login_params);
		msn_nexus_destroy(nexus);
		session->nexus = NULL;
		return;
	}
	msn_soap_free_read_buf(soapconn);
}

static void
nexus_login_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = nexus_login_read_cb;
	msn_soap_read_cb(data,source,cond);
}


void
login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn *soapconn;
	MsnNexus * nexus;
	MsnSession *session;
	char *username, *password;
	char *request_str, *head, *tail,*challenge_str;

	gaim_debug_info("MaYuan","starting Windows Live ID authentication\n");

	soapconn = data;
	g_return_if_fail(soapconn != NULL);

	nexus = soapconn->parent;
	g_return_if_fail(nexus != NULL);

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

	/*prepare the Windows Live ID authentication token*/
	username = g_strdup(gaim_account_get_username(session->account));
	password = g_strdup(gaim_connection_get_password(session->account->gc));
//		g_strdup(gaim_url_encode(gaim_connection_get_password(session->account->gc)));

	challenge_str = g_strdup_printf(
		"lc=%s&amp;id=%s&amp;tw=%s&amp;fs=%s&amp;ru=%s&amp;ct=%s&amp;kpp=%s&amp;kv=%s&amp;ver=%s&amp;rn=%s&amp;tpf=%s\r\n",
		(char *)g_hash_table_lookup(nexus->challenge_data, "lc"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "id"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "tw"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "fs"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "ru"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "ct"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "kpp"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "kv"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "ver"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "rn"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "tpf")
		);

	/*build the SOAP windows Live ID XML body */
	tail = g_strdup_printf(TWN_ENVELOP_TEMPLATE,username,password,challenge_str	);

	soapconn->login_path = g_strdup(TWN_POST_URL);
	head = g_strdup_printf(
					"POST %s HTTP/1.1\r\n"
					"Accept: text/*\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
					"Host: %s\r\n"
					"Content-Length: %d\r\n"
					"Connection: Keep-Alive\r\n"
					"Cache-Control: no-cache\r\n\r\n",
					soapconn->login_path,soapconn->login_host,strlen(tail));

	request_str = g_strdup_printf("%s%s", head,tail);
//	gaim_debug_misc("msn", "TWN Sending: {%s}\n", request_str);

	g_free(head);
	g_free(tail);
	g_free(username);
	g_free(password);

	/*prepare to send the SOAP request*/
	msn_soap_write(soapconn,request_str,nexus_login_written_cb);

	return;
}

/**************************************************************************
 * Connect
 **************************************************************************/
void
msn_nexus_connect(MsnNexus *nexus)
{
	/*  Authenticate via Windows Live ID. */
	gaim_debug_info("MaYuan","msn_nexus_connect...\n");

	msn_soap_init(nexus->soapconn,MSN_TWN_SERVER,1,login_connect_cb,login_error_cb);
}

