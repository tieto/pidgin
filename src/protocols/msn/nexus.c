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
	nexus->challenge_data = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, g_free);

	return nexus;
}

void
msn_nexus_destroy(MsnNexus *nexus)
{
	g_free(nexus->login_host);

	g_free(nexus->login_path);

	if (nexus->challenge_data != NULL)
		g_hash_table_destroy(nexus->challenge_data);

	if (nexus->input_handler > 0)
		gaim_input_remove(nexus->input_handler);
	g_free(nexus->write_buf);
	g_free(nexus->read_buf);

	g_free(nexus);
}

/**************************************************************************
 * Util
 **************************************************************************/

static gssize
msn_ssl_read(MsnNexus *nexus)
{
	gssize len;
	gssize total_len = 0;
	char temp_buf[4096];

	if((len = gaim_ssl_read(nexus->gsc, temp_buf,
			sizeof(temp_buf))) > 0)
	{
#if 0
		g_string_append(nexus->read_buf,temp_buf);
#else
		total_len += len;
		nexus->read_buf = g_realloc(nexus->read_buf,
			nexus->read_len + len + 1);
		strncpy(nexus->read_buf + nexus->read_len, temp_buf, len);
		nexus->read_len += len;
		nexus->read_buf[nexus->read_len] = '\0';
#endif
	}
//	gaim_debug_info("MaYuan","nexus ssl read:{%s}\n",nexus->read_buf);
	return total_len;
}

static void
nexus_write_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnNexus *nexus = data;
	int len, total_len;

	total_len = strlen(nexus->write_buf);

	/* 
	 * write the content to SSL server,
	 * We use SOAP to request Windows Live ID authentication
	 */
	len = gaim_ssl_write(nexus->gsc,
		nexus->write_buf + nexus->written_len,
		total_len - nexus->written_len);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		gaim_input_remove(nexus->input_handler);
		nexus->input_handler = -1;
		/* TODO: notify of the error */
		return;
	}
	nexus->written_len += len;

	if (nexus->written_len < total_len)
		return;

	gaim_input_remove(nexus->input_handler);
	nexus->input_handler = -1;

	g_free(nexus->write_buf);
	nexus->write_buf = NULL;
	nexus->written_len = 0;

	nexus->written_cb(nexus, source, 0);
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
	MsnNexus *nexus;
	MsnSession *session;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_AUTH, _("Unable to connect"));
	/* the above line will result in nexus being destroyed, so we don't want
	 * to destroy it here, or we'd crash */
}

static void
nexus_login_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnNexus *nexus = data;
	MsnSession *session;
	int len;

	session = nexus->session;
	g_return_if_fail(session != NULL);

	if (nexus->input_handler == -1)
		nexus->input_handler = gaim_input_add(nexus->gsc->fd,
			GAIM_INPUT_READ, nexus_login_written_cb, nexus);

	/*read the request header*/
	len = msn_ssl_read(nexus);
	if (len < 0 && errno == EAGAIN){
		return;
	}else if (len < 0) {
		gaim_input_remove(nexus->input_handler);
		nexus->input_handler = -1;
		g_free(nexus->read_buf);
		nexus->read_buf = NULL;
		nexus->read_len = 0;
		/* TODO: error handling */
		return;
	}

	if(nexus->read_buf == NULL){
		return;
	}
	if (g_strstr_len(nexus->read_buf, nexus->read_len,
			"</S:Envelope>") == NULL){
		return;
	}

	gaim_input_remove(nexus->input_handler);
	nexus->input_handler = -1;
	gaim_ssl_close(nexus->gsc);
	nexus->gsc = NULL;

//	gaim_debug_misc("msn", "TWN Server Reply: {%s}", nexus->read_buf);

	if (strstr(nexus->read_buf, "HTTP/1.1 302") != NULL){
		/* Redirect. */
		char *location, *c;

		location = strstr(nexus->read_buf, "Location: ");
		if (location == NULL)
		{
			g_free(nexus->read_buf);
			nexus->read_buf = NULL;
			nexus->read_len = 0;

			return;
		}
		location = strchr(location, ' ') + 1;

		if ((c = strchr(location, '\r')) != NULL)
			*c = '\0';

		/* Skip the http:// */
		if ((c = strchr(location, '/')) != NULL)
			location = c + 2;

		if ((c = strchr(location, '/')) != NULL)
		{
			g_free(nexus->login_path);
			nexus->login_path = g_strdup(c);

			*c = '\0';
		}

		g_free(nexus->login_host);
		nexus->login_host = g_strdup(location);

		gaim_ssl_connect(session->account, nexus->login_host,
			GAIM_SSL_DEFAULT_PORT, login_connect_cb,
			login_error_cb, nexus);
	}else if (strstr(nexus->read_buf, "HTTP/1.1 401 Unauthorized") != NULL){
		const char *error;

		if ((error = strstr(nexus->read_buf, "WWW-Authenticate")) != NULL)	{
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
	}else if (strstr(nexus->read_buf, "HTTP/1.1 200 OK")){
		/*reply OK, we should process the SOAP body*/
		char *base, *c;
		char *login_params;
		char *length_start,*length_end,*body_len;

		char **elems, **cur, **tokens;
		const char * cert_str;

		gaim_debug_info("MaYuan","Receive 200\n");
#if 0
		length_start = strstr(nexus->read_buf, "Content-Length: ");
		length_start += strlen("Content-Length: ");
		length_end = strstr(length_start, "\r\n");
		body_len = g_strndup(length_start,length_end - length_start);
//		gaim_debug_info("MaYuan","body length is :%s\n",body_len);

		g_free(body_len);
//		g_return_if_fail(body_len != NULL);
#endif
		//TODO: we should parse it using XML
		base  = strstr(base, TWN_START_TOKEN);
		base += strlen(TWN_START_TOKEN);
//		gaim_debug_info("MaYuan","base is :%s\n",base);
		c     = strstr(base, TWN_END_TOKEN);
//		gaim_debug_info("MaYuan","c is :%s\n",c);
//		gaim_debug_info("MaYuan","len is :%d\n",c-base);
		login_params = g_strndup(base, c - base);

		gaim_debug_info("msn", "TWN Cert: {%s}\n", login_params);

		/* Parse the challenge data. */
		elems = g_strsplit(login_params, "&amp;", 0);

		for (cur = elems; *cur != NULL; cur++){
				tokens = g_strsplit(*cur, "=", 2);
				g_hash_table_insert(session->nexus->challenge_data, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
		}

		g_strfreev(elems);

		cert_str = g_strdup_printf("t=%s&p=%s",
			(char *)g_hash_table_lookup(nexus->challenge_data, "t"),
			(char *)g_hash_table_lookup(nexus->challenge_data, "p")
		);
		msn_got_login_params(session, cert_str);

		g_free(cert_str);
//		g_free(body_len);
		g_free(login_params);
//		return;
		msn_nexus_destroy(nexus);
		session->nexus = NULL;
		return;
	}

	g_free(nexus->read_buf);
	nexus->read_buf = NULL;
	nexus->read_len = 0;
}


void
login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnNexus *nexus;
	MsnSession *session;
	char *username, *password;
	char *request_str, *head, *tail,*challenge_str;
	char *buffer = NULL;
	guint32 ctint;

	gaim_debug_info("MaYuan","starting Windows Live ID authentication\n");
	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	nexus->gsc = gsc;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

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

	nexus->login_path = g_strdup(TWN_POST_URL);
	head = g_strdup_printf(
					"POST %s HTTP/1.1\r\n"
					"Accept: text/*\r\n"
					"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
					"Host: %s\r\n"
					"Content-Length: %d\r\n"
					"Connection: Keep-Alive\r\n"
					"Cache-Control: no-cache\r\n\r\n",
					nexus->login_path,nexus->login_host,strlen(tail));

	request_str = g_strdup_printf("%s%s", head,tail);
//	gaim_debug_misc("msn", "TWN Sending: {%s}\n", request_str);

//	g_free(nexus->login_path);
	g_free(buffer);
	g_free(head);
	g_free(tail);
	g_free(username);
	g_free(password);

	nexus->write_buf = request_str;
	nexus->written_len = 0;

	nexus->read_len = 0;

	nexus->written_cb = nexus_login_written_cb;

	nexus->input_handler = gaim_input_add(gsc->fd, GAIM_INPUT_WRITE,
		nexus_write_cb, nexus);

	nexus_write_cb(nexus, gsc->fd, GAIM_INPUT_WRITE);

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
	nexus->login_host = g_strdup(TWN_SERVER);
	gaim_ssl_connect(nexus->session->account, nexus->login_host,
		GAIM_SSL_DEFAULT_PORT, login_connect_cb, login_error_cb,
		nexus);
}
