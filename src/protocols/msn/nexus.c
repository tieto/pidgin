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
	nexus->challenge_data = g_hash_table_new_full(g_str_hash, g_str_equal,
												  g_free, g_free);

	return nexus;
}

void
msn_nexus_destroy(MsnNexus *nexus)
{
	if (nexus->login_host != NULL)
		g_free(nexus->login_host);

	if (nexus->login_path != NULL)
		g_free(nexus->login_path);

	if (nexus->challenge_data != NULL)
		g_hash_table_destroy(nexus->challenge_data);

	g_free(nexus);
}

/**************************************************************************
 * Util
 **************************************************************************/

static size_t
msn_ssl_read(GaimSslConnection *gsc, char **dest_buffer)
{
	gssize size = 0, s;
	char *buffer = NULL;
	char temp_buf[4096];

	while ((s = gaim_ssl_read(gsc, temp_buf, sizeof(temp_buf))) > 0)
	{
		buffer = g_realloc(buffer, size + s + 1);

		strncpy(buffer + size, temp_buf, s);

		buffer[size + s] = '\0';

		size += s;
	}

	*dest_buffer = buffer;

	return size;
}

/**************************************************************************
 * Login
 **************************************************************************/

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
login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnNexus *nexus;
	MsnSession *session;
	char *username, *password;
	char *request_str, *head, *tail;
	char *buffer = NULL;
	guint32 ctint;
	size_t s;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_GET_COOKIE);

	username =
		g_strdup(gaim_url_encode(gaim_account_get_username(session->account)));

	password =
		g_strdup(gaim_url_encode(gaim_connection_get_password(session->account->gc)));

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
		(char *)g_hash_table_lookup(nexus->challenge_data, "lc"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "id"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "tw"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "fs"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "ru"),
		ctint,
		(char *)g_hash_table_lookup(nexus->challenge_data, "kpp"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "kv"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "ver"),
		(char *)g_hash_table_lookup(nexus->challenge_data, "tpf"),
		nexus->login_host);

	buffer = g_strdup_printf("%s,pwd=XXXXXXXX,%s\r\n", head, tail);
	request_str = g_strdup_printf("%s,pwd=%s,%s\r\n", head, password, tail);

	gaim_debug_misc("msn", "Sending: {%s}\n", buffer);

	g_free(buffer);
	g_free(head);
	g_free(tail);
	g_free(username);
	g_free(password);

	if ((s = gaim_ssl_write(gsc, request_str, strlen(request_str))) <= 0)
	{
		g_free(request_str);

		return;
	}

	g_free(request_str);

	if ((s = msn_ssl_read(gsc, &buffer)) <= 0)
		return;

	gaim_ssl_close(gsc);

	gaim_debug_misc("msn", "ssl buffer: {%s}", buffer);

	if (strstr(buffer, "HTTP/1.1 302") != NULL)
	{
		/* Redirect. */
		char *location, *c;

		location = strstr(buffer, "Location: ");
		if (location == NULL)
		{
			g_free(buffer);

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
	}
	else if (strstr(buffer, "HTTP/1.1 401 Unauthorized") != NULL)
	{
		const char *error;

		if ((error = strstr(buffer, "WWW-Authenticate")) != NULL)
		{
			if ((error = strstr(error, "cbtxt=")) != NULL)
			{
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
	}
	else if (strstr(buffer, "HTTP/1.1 200 OK"))
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

		base  = strstr(buffer, "Authentication-Info: ");

		g_return_if_fail(base != NULL);

		base  = strstr(base, "from-PP='");
		base += strlen("from-PP='");
		c     = strchr(base, '\'');

		login_params = g_strndup(base, c - base);

		msn_got_login_params(session, login_params);

		g_free(login_params);

		msn_nexus_destroy(nexus);
		session->nexus = NULL;
	}

	g_free(buffer);
}

/**************************************************************************
 * Connect
 **************************************************************************/

static void
nexus_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnNexus *nexus;
	MsnSession *session;
	char *request_str;
	char *da_login;
	char *base, *c;
	char *buffer = NULL;
	size_t s;

	nexus = data;
	g_return_if_fail(nexus != NULL);

	session = nexus->session;
	g_return_if_fail(session != NULL);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_AUTH);

	request_str = g_strdup_printf("GET /rdr/pprdr.asp\r\n\r\n");

	if ((s = gaim_ssl_write(gsc, request_str, strlen(request_str))) <= 0)
	{
		g_free(request_str);
		return;
	}

	g_free(request_str);

	/* Get the PassportURLs line. */
	if ((s = msn_ssl_read(gsc, &buffer)) <= 0)
		return;

	base = strstr(buffer, "PassportURLs");

	if (base == NULL)
	{
		g_free(buffer);
		return;
	}

	if ((da_login = strstr(base, "DALogin=")) != NULL)
	{
		if ((da_login = strchr(da_login, '=')) != NULL)
			da_login++;

		if ((c = strchr(da_login, ',')) != NULL)
			*c = '\0';

		if ((c = strchr(da_login, '/')) != NULL)
		{
			nexus->login_path = g_strdup(c);

			*c = '\0';
		}

		nexus->login_host = g_strdup(da_login);
	}

	g_free(buffer);

	gaim_ssl_close(gsc);

	/* Now begin the connection to the login server. */
	gaim_ssl_connect(session->account, nexus->login_host,
					 GAIM_SSL_DEFAULT_PORT, login_connect_cb,
					 login_error_cb, nexus);
}

void
msn_nexus_connect(MsnNexus *nexus)
{
	gaim_ssl_connect(nexus->session->account, "nexus.passport.com",
					 GAIM_SSL_DEFAULT_PORT, nexus_connect_cb,
					 login_error_cb, nexus);
}
