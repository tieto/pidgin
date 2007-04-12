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
	if (nexus->gsc)
		purple_ssl_close(nexus->gsc);

	g_free(nexus->login_host);

	g_free(nexus->login_path);

	if (nexus->challenge_data != NULL)
		g_hash_table_destroy(nexus->challenge_data);

	if (nexus->input_handler > 0)
		purple_input_remove(nexus->input_handler);
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
	char temp_buf[4096];

	if ((len = purple_ssl_read(nexus->gsc, temp_buf,
			sizeof(temp_buf))) > 0)
	{
		nexus->read_buf = g_realloc(nexus->read_buf,
			nexus->read_len + len + 1);
		strncpy(nexus->read_buf + nexus->read_len, temp_buf, len);
		nexus->read_len += len;
		nexus->read_buf[nexus->read_len] = '\0';
	}

	return len;
}

static void
nexus_write_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnNexus *nexus = data;
	int len, total_len;

	total_len = strlen(nexus->write_buf);

	len = purple_ssl_write(nexus->gsc,
		nexus->write_buf + nexus->written_len,
		total_len - nexus->written_len);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		purple_input_remove(nexus->input_handler);
		nexus->input_handler = 0;
		/* TODO: notify of the error */
		return;
	}
	nexus->written_len += len;

	if (nexus->written_len < total_len)
		return;

	purple_input_remove(nexus->input_handler);
	nexus->input_handler = 0;

	g_free(nexus->write_buf);
	nexus->write_buf = NULL;
	nexus->written_len = 0;

	nexus->written_cb(nexus, source, 0);
}

/**************************************************************************
 * Login
 **************************************************************************/

static void
login_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond);

static void
login_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{
	MsnNexus *nexus;
	MsnSession *session;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	nexus->gsc = NULL;

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_AUTH, _("Unable to connect"));
	/* the above line will result in nexus being destroyed, so we don't want
	 * to destroy it here, or we'd crash */
}

static void
nexus_login_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnNexus *nexus = data;
	MsnSession *session;
	int len;

	session = nexus->session;
	g_return_if_fail(session != NULL);

	if (nexus->input_handler == 0)
		//TODO: Use purple_ssl_input_add()?
		nexus->input_handler = purple_input_add(nexus->gsc->fd,
			PURPLE_INPUT_READ, nexus_login_written_cb, nexus);


	len = msn_ssl_read(nexus);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_input_remove(nexus->input_handler);
		nexus->input_handler = 0;
		g_free(nexus->read_buf);
		nexus->read_buf = NULL;
		nexus->read_len = 0;
		/* TODO: error handling */
		return;
	}

	if (g_strstr_len(nexus->read_buf, nexus->read_len,
			"\r\n\r\n") == NULL)
		return;

	purple_input_remove(nexus->input_handler);
	nexus->input_handler = 0;

	purple_ssl_close(nexus->gsc);
	nexus->gsc = NULL;

	purple_debug_misc("msn", "ssl buffer: {%s}", nexus->read_buf);

	if (strstr(nexus->read_buf, "HTTP/1.1 302") != NULL)
	{
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

		nexus->gsc = purple_ssl_connect(session->account,
				nexus->login_host, PURPLE_SSL_DEFAULT_PORT,
				login_connect_cb, login_error_cb, nexus);
	}
	else if (strstr(nexus->read_buf, "HTTP/1.1 401 Unauthorized") != NULL)
	{
		const char *error;

		if ((error = strstr(nexus->read_buf, "WWW-Authenticate")) != NULL)
		{
			if ((error = strstr(error, "cbtxt=")) != NULL)
			{
				const char *c;
				char *temp;

				error += strlen("cbtxt=");

				if ((c = strchr(error, '\n')) == NULL)
					c = error + strlen(error);

				temp = g_strndup(error, c - error);
				error = purple_url_decode(temp);
				g_free(temp);
			}
		}

		msn_session_set_error(session, MSN_ERROR_AUTH, error);
	}
	else if (strstr(nexus->read_buf, "HTTP/1.1 503 Service Unavailable"))
	{
		msn_session_set_error(session, MSN_ERROR_SERV_UNAVAILABLE, NULL);
	}
	else if (strstr(nexus->read_buf, "HTTP/1.1 200 OK"))
	{
		char *base, *c;
		char *login_params;

#if 0
		/* All your base are belong to us. */
		base = buffer;

		/* For great cookie! */
		while ((base = strstr(base, "Set-Cookie: ")) != NULL)
		{
			base += strlen("Set-Cookie: ");

			c = strchr(base, ';');

			session->login_cookies =
				g_list_append(session->login_cookies,
							  g_strndup(base, c - base));
		}
#endif

		base  = strstr(nexus->read_buf, "Authentication-Info: ");

		g_return_if_fail(base != NULL);

		base  = strstr(base, "from-PP='");
		base += strlen("from-PP='");
		c     = strchr(base, '\'');

		login_params = g_strndup(base, c - base);

		msn_got_login_params(session, login_params);

		g_free(login_params);

		msn_nexus_destroy(nexus);
		session->nexus = NULL;
		return;
	}

	g_free(nexus->read_buf);
	nexus->read_buf = NULL;
	nexus->read_len = 0;

}

/* this guards against missing hash entries */
static char *
nexus_challenge_data_lookup(GHashTable *challenge_data, const char *key)
{
	char *entry;

	return (entry = (char *)g_hash_table_lookup(challenge_data, key)) ?
		entry : "(null)";
}

void
login_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnNexus *nexus;
	MsnSession *session;
	char *username, *password;
	char *request_str, *head, *tail;
	char *buffer = NULL;
	guint32 ctint;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

	username =
		g_strdup(purple_url_encode(purple_account_get_username(session->account)));

	password =
		g_strdup(purple_url_encode(purple_connection_get_password(session->account->gc)));

	ctint = strtoul((char *)g_hash_table_lookup(nexus->challenge_data, "ct"), NULL, 10) + 200;

	head = g_strdup_printf(
		"GET %s HTTP/1.1\r\n"
		"Authorization: Passport1.4 OrgVerb=GET,OrgURL=%s,sign-in=%s",
		nexus->login_path,
		(char *)g_hash_table_lookup(nexus->challenge_data, "ru"),
		username);

	tail = g_strdup_printf(
		"lc=%s,id=%s,tw=%s,fs=%s,ru=%s,ct=%" G_GUINT32_FORMAT ",kpp=%s,kv=%s,ver=%s,tpf=%s\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Connection: Keep-Alive\r\n"
		"Cache-Control: no-cache\r\n",
		nexus_challenge_data_lookup(nexus->challenge_data, "lc"),
		nexus_challenge_data_lookup(nexus->challenge_data, "id"),
		nexus_challenge_data_lookup(nexus->challenge_data, "tw"),
		nexus_challenge_data_lookup(nexus->challenge_data, "fs"),
		nexus_challenge_data_lookup(nexus->challenge_data, "ru"),
		ctint,
		nexus_challenge_data_lookup(nexus->challenge_data, "kpp"),
		nexus_challenge_data_lookup(nexus->challenge_data, "kv"),
		nexus_challenge_data_lookup(nexus->challenge_data, "ver"),
		nexus_challenge_data_lookup(nexus->challenge_data, "tpf"),
		nexus->login_host);

	buffer = g_strdup_printf("%s,pwd=XXXXXXXX,%s\r\n", head, tail);
	request_str = g_strdup_printf("%s,pwd=%s,%s\r\n", head, password, tail);

	purple_debug_misc("msn", "Sending: {%s}\n", buffer);

	g_free(buffer);
	g_free(head);
	g_free(tail);
	g_free(username);
	g_free(password);

	nexus->write_buf = request_str;
	nexus->written_len = 0;

	nexus->read_len = 0;

	nexus->written_cb = nexus_login_written_cb;

	nexus->input_handler = purple_input_add(gsc->fd, PURPLE_INPUT_WRITE,
		nexus_write_cb, nexus);

	nexus_write_cb(nexus, gsc->fd, PURPLE_INPUT_WRITE);

	return;


}

static void
nexus_connect_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnNexus *nexus = data;
	int len;
	char *da_login;
	char *base, *c;

	if (nexus->input_handler == 0)
		//TODO: Use purple_ssl_input_add()?
		nexus->input_handler = purple_input_add(nexus->gsc->fd,
			PURPLE_INPUT_READ, nexus_connect_written_cb, nexus);

	/* Get the PassportURLs line. */
	len = msn_ssl_read(nexus);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_input_remove(nexus->input_handler);
		nexus->input_handler = 0;
		g_free(nexus->read_buf);
		nexus->read_buf = NULL;
		nexus->read_len = 0;
		/* TODO: error handling */
		return;
	}

	if (g_strstr_len(nexus->read_buf, nexus->read_len,
			"\r\n\r\n") == NULL)
		return;

	purple_input_remove(nexus->input_handler);
	nexus->input_handler = 0;

	base = strstr(nexus->read_buf, "PassportURLs");

	if (base == NULL)
	{
		g_free(nexus->read_buf);
		nexus->read_buf = NULL;
		nexus->read_len = 0;
		return;
	}

	if ((da_login = strstr(base, "DALogin=")) != NULL)
	{
		/* skip over "DALogin=" */
		da_login += 8;

		if ((c = strchr(da_login, ',')) != NULL)
			*c = '\0';

		if ((c = strchr(da_login, '/')) != NULL)
		{
			nexus->login_path = g_strdup(c);
			*c = '\0';
		}

		nexus->login_host = g_strdup(da_login);
	}

	g_free(nexus->read_buf);
	nexus->read_buf = NULL;
	nexus->read_len = 0;

	purple_ssl_close(nexus->gsc);

	/* Now begin the connection to the login server. */
	nexus->gsc = purple_ssl_connect(nexus->session->account,
			nexus->login_host, PURPLE_SSL_DEFAULT_PORT,
			login_connect_cb, login_error_cb, nexus);
}


/**************************************************************************
 * Connect
 **************************************************************************/

static void
nexus_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnNexus *nexus;
	MsnSession *session;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_AUTH);

	nexus->write_buf = g_strdup("GET /rdr/pprdr.asp\r\n\r\n");
	nexus->written_len = 0;

	nexus->read_len = 0;

	nexus->written_cb = nexus_connect_written_cb;

	nexus->input_handler = purple_input_add(gsc->fd, PURPLE_INPUT_WRITE,
		nexus_write_cb, nexus);

	nexus_write_cb(nexus, gsc->fd, PURPLE_INPUT_WRITE);
}

void
msn_nexus_connect(MsnNexus *nexus)
{
	nexus->gsc = purple_ssl_connect(nexus->session->account,
			"nexus.passport.com", PURPLE_SSL_DEFAULT_PORT,
			nexus_connect_cb, login_error_cb, nexus);
}
