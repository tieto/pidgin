/**
 * @file trepia.c The Trepia protocol plugin
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "gaim.h"
#include "account.h"
#include "accountopt.h"
#include "md5.h"
#include "profile.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef _WIN32
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <net/if_arp.h>
#endif

static GaimPlugin *my_protocol = NULL;

typedef enum
{
	TREPIA_LOGIN          = 'C',
	TREPIA_PROFILE_REQ    = 'D',
	TREPIA_MSG_OUTGOING   = 'F',
	TREPIA_REGISTER       = 'J',
	TREPIA_USER_LIST      = 'L',
	TREPIA_MEMBER_UPDATE  = 'M',
	TREPIA_MEMBER_OFFLINE = 'N',
	TREPIA_MEMBER_PROFILE = 'O',
	TREPIA_MSG_INCOMING   = 'Q'

} TrepiaMessageType;

typedef struct
{
	GaimConnection *gc;

	int inpa;
	int fd;

	GString *rxqueue;

} TrepiaSession;

typedef struct
{
	TrepiaMessageType *type;
	char *tag;

	GHashTable *keys;

	GString *buffer;

} TrepiaParserData;

#define TREPIA_SERVER    "trepia.com"
#define TREPIA_PORT       8201
#define TREPIA_REG_PORT   8209

static int
trepia_write(int fd, const char *data, size_t len)
{
	gaim_debug(GAIM_DEBUG_MISC, "trepia", "C: %s%c", data,
			   (data[strlen(data) - 1] == '\n' ? '\0' : '\n'));

	return write(fd, data, len);
}

static void
__clear_user_list(GaimAccount *account)
{
	struct gaim_buddy_list *blist;
	GaimBlistNode *group, *buddy, *next_buddy;

	blist = gaim_get_blist();

	for (group = blist->root; group != NULL; group = group->next) {
		for (buddy = group->child; buddy != NULL; buddy = next_buddy) {
			struct buddy *b = (struct buddy *)buddy;

			next_buddy = buddy->next;

			if (b->account == account)
				gaim_blist_remove_buddy(b);
		}
	}
}

static char *
__get_mac_address(const char *ip)
{
	char *mac = NULL;
#ifndef _WIN32
	struct sockaddr_in sin = { 0 };
	struct arpreq myarp = { { 0 } };
	int sockfd;
	unsigned char *ptr;

	sin.sin_family = AF_INET;

	if (inet_aton(ip, &sin.sin_addr) == 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "trepia", "Invalid IP address %s\n", ip);
		return NULL;
	}

	memcpy(&myarp.arp_pa, &sin, sizeof(myarp.arp_pa));
	strcpy(myarp.arp_dev, "eth0");

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		gaim_debug(GAIM_DEBUG_ERROR, "trepia",
				   "Cannot open socket for retrieving MAC address.\n");
		return NULL;
	}

	if (ioctl(sockfd, SIOCGARP, &myarp) == -1) {
		gaim_debug(GAIM_DEBUG_ERROR, "trepia",
				   "No entry in in arp_cache for %s\n", ip);
		return NULL;
	}

	ptr = &myarp.arp_ha.sa_data[0];

	mac = g_strdup_printf("%x:%x:%x:%x:%x:%x",
						  ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
#else
#endif

	return mac;
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
trepia_list_icon(GaimAccount *a, struct buddy *b)
{
	return "trepia";
}

static void
trepia_list_emblems(struct buddy *b, char **se, char **sw,
				 char **nw, char **ne)
{
	TrepiaProfile *profile = (TrepiaProfile *)b->proto_data;

	if (trepia_profile_get_sex(profile) == 'M')
		*sw = "male";
	else if (trepia_profile_get_sex(profile))
		*sw = "female";
}

static char *
trepia_status_text(struct buddy *b)
{
	TrepiaProfile *profile = (TrepiaProfile *)b->proto_data;

	if (trepia_profile_get_profile(profile) != NULL)
		return g_strdup(trepia_profile_get_profile(profile));

	return NULL;
}

static char *
trepia_tooltip_text(struct buddy *b)
{
	TrepiaProfile *profile = (TrepiaProfile *)b->proto_data;
	const char *value;
	const char *first_name, *last_name;
	int int_value;
	char *text = NULL;
	char *tmp, *tmp2;
	char *c;

	text = g_strdup("");

	first_name = trepia_profile_get_first_name(profile);
	last_name  = trepia_profile_get_last_name(profile);

	if (first_name != NULL || last_name != NULL) {
		tmp = g_strdup_printf("<b>Name:</b> %s%s%s\n",
							  (first_name == NULL ? "" : first_name),
							  (first_name == NULL ? "" : " "),
							  (last_name == NULL ? "" : last_name));

		tmp2 = g_strconcat(text, tmp, NULL);
		g_free(tmp);
		g_free(text);
		text = tmp2;
	}

	if ((int_value = trepia_profile_get_age(profile)) != 0) {
		tmp = g_strdup_printf("<b>Age:</b> %d\n", int_value);

		tmp2 = g_strconcat(text, tmp, NULL);
		g_free(tmp);
		g_free(text);
		text = tmp2;
	}

	tmp = g_strdup_printf("<b>Gender:</b> %s\n",
			(trepia_profile_get_sex(profile) == 'F' ? "Female" : "Male"));

	tmp2 = g_strconcat(text, tmp, NULL);
	g_free(tmp);
	g_free(text);
	text = tmp2;

	if ((value = trepia_profile_get_city(profile)) != NULL) {
		tmp = g_strdup_printf("<b>City:</b> %s\n", value);

		tmp2 = g_strconcat(text, tmp, NULL);
		g_free(tmp);
		g_free(text);
		text = tmp2;
	}

	if ((value = trepia_profile_get_state(profile)) != NULL) {
		tmp = g_strdup_printf("<b>State:</b> %s\n", value);

		tmp2 = g_strconcat(text, tmp, NULL);
		g_free(tmp);
		g_free(text);
		text = tmp2;
	}

	if ((value = trepia_profile_get_country(profile)) != NULL) {
		tmp = g_strdup_printf("<b>Country:</b> %s\n", value);

		tmp2 = g_strconcat(text, tmp, NULL);
		g_free(tmp);
		g_free(text);
		text = tmp2;
	}

	c = text + strlen(text);

	if (*c == '\n')
		*c = '\0';

	return text;
}

static GList *
trepia_away_states(GaimConnection *gc)
{
	GList *m = NULL;

	return m;
}

static GList *
trepia_actions(GaimConnection *gc)
{
	return NULL;
}

static GList *
trepia_buddy_menu(GaimConnection *gc, const char *who)
{
	return NULL;
}

static void
__free_parser_data(gpointer user_data)
{
	return;

	TrepiaParserData *data = user_data;

	if (data->buffer != NULL)
		g_string_free(data->buffer, TRUE);

	if (data->tag != NULL)
		g_free(data->tag);

	g_free(data);
}

static void
__start_element_handler(GMarkupParseContext *context,
						const gchar *element_name,
						const gchar **attribute_names,
						const gchar **attribute_values,
						gpointer user_data, GError **error)
{
	TrepiaParserData *data = user_data;

	if (data->buffer != NULL) {
		g_string_free(data->buffer, TRUE);
		data->buffer = NULL;
	}

	if (*data->type == 0) {
		*data->type = *element_name;
	}
	else {
		data->tag = g_strdup(element_name);
	}
}

static void
__end_element_handler(GMarkupParseContext *context, const gchar *element_name,
					  gpointer user_data,  GError **error)
{
	TrepiaParserData *data = user_data;
	gchar *buffer;

	if (*element_name == *data->type)
		return;

	if (data->buffer == NULL || data->tag == NULL) {
		data->tag = NULL;
		return;
	}

	buffer = g_string_free(data->buffer, FALSE);
	data->buffer = NULL;

	g_hash_table_insert(data->keys, data->tag, buffer);

	data->tag = NULL;
}

static void
__text_handler(GMarkupParseContext *context, const gchar *text,
			   gsize text_len, gpointer user_data, GError **error)
{
	TrepiaParserData *data = user_data;

	if (data->buffer == NULL)
		data->buffer = g_string_new_len(text, text_len);
	else
		g_string_append_len(data->buffer, text, text_len);
}

static GMarkupParser accounts_parser =
{
	__start_element_handler,
	__end_element_handler,
	__text_handler,
	NULL,
	NULL
};

static int
__parse_message(const char *buf, TrepiaMessageType *type, GHashTable **info)
{
	TrepiaParserData *parser_data = g_new0(TrepiaParserData, 1);
	GMarkupParseContext *context;
	GHashTable *keys;

	keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	parser_data->keys = keys;
	parser_data->type = type;

	context = g_markup_parse_context_new(&accounts_parser, 0,
										 parser_data, __free_parser_data);

	if (!g_markup_parse_context_parse(context, buf, strlen(buf), NULL)) {
		g_markup_parse_context_free(context);
		g_free(parser_data);
		g_hash_table_destroy(keys);

		return 1;
	}

	if (!g_markup_parse_context_end_parse(context, NULL)) {
		g_markup_parse_context_free(context);
		g_free(parser_data);
		g_hash_table_destroy(keys);

		return 1;
	}

	g_markup_parse_context_free(context);

	*info = keys;

	return 0;
}

static gboolean
__parse_data(TrepiaSession *session, char *buf)
{
	GHashTable *info;
	GaimAccount *account;
	TrepiaMessageType type = 0;
	TrepiaProfile *profile;
	int ret;
	char *buffer;
	struct buddy *b;
	const char *id = NULL;
	const char *value;

	account = gaim_connection_get_account(session->gc);

	ret = __parse_message(buf, &type, &info);

	if (ret == 1)
		return TRUE;

	gaim_debug(GAIM_DEBUG_INFO, "trepia", "Successful parse.\n");
	gaim_debug(GAIM_DEBUG_INFO, "trepia", "Message type: %c\n",
			   type);

	if (info != NULL) {
		switch (type) {
			case TREPIA_USER_LIST:
				gaim_debug(GAIM_DEBUG_INFO, "trepia",
						   "Signon complete. Showing buddy list.\n");
				gaim_connection_set_state(session->gc, GAIM_CONNECTED);
				serv_finish_login(session->gc);
				break;

			case TREPIA_MSG_INCOMING: /* Incoming Message */
				gaim_debug(GAIM_DEBUG_INFO, "trepia", "Receiving message\n");
				serv_got_im(session->gc,
						(char *)g_hash_table_lookup(info, "a"),
						(char *)g_hash_table_lookup(info, "b"),
						0, time(NULL), -1);
				break;

			case TREPIA_MEMBER_UPDATE:
				id  = g_hash_table_lookup(info, "a");
				b = gaim_find_buddy(account, id);

				if (b == NULL) {
					struct group *g;

					g = gaim_find_group(_("Local Users"));

					if (g == NULL) {
						g = gaim_group_new(_("Local Users"));
						gaim_blist_add_group(g, NULL);
					}

					b = gaim_buddy_new(account, id, NULL);

					gaim_blist_add_buddy(b, g, NULL);
				}

				b->proto_data = trepia_profile_new();

				serv_got_update(session->gc, id, 1, 0, 0, 0, 0);

				buffer = g_strdup_printf(
					"<D>\n"
					"<a>%s</a>\n"
					"<b>1</b>\n"
					"</D>",
					id);

				if (trepia_write(session->fd, buffer, strlen(buffer)) < 0) {
					gaim_connection_error(session->gc, _("Write error"));
					g_free(buffer);
					return 1;
				}

				buffer = g_strdup_printf(
					"<D>\n"
					"<a>%s</a>\n"
					"<b>2</b>\n"
					"</D>",
					id);

				if (trepia_write(session->fd, buffer, strlen(buffer)) < 0) {
					gaim_connection_error(session->gc, _("Write error"));
					g_free(buffer);
					return 1;
				}

				g_free(buffer);
				break;

			case TREPIA_MEMBER_PROFILE:
				id = g_hash_table_lookup(info, "a");
				b = gaim_find_buddy(account, id);

				if (b == NULL)
					break;

				profile = b->proto_data;

				/* ID */
				trepia_profile_set_id(profile, atoi(id));

				/* Login Time */
				if ((value = g_hash_table_lookup(info, "b")) != NULL)
					trepia_profile_set_login_time(profile, atoi(value));

				/* Age */
				if ((value = g_hash_table_lookup(info, "m")) != NULL)
					trepia_profile_set_age(profile, atoi(value));

				/* ICQ */
				if ((value = g_hash_table_lookup(info, "i")) != NULL)
					trepia_profile_set_icq(profile, atoi(value));

				/* Sex */
				if ((value = g_hash_table_lookup(info, "n")) != NULL)
					trepia_profile_set_sex(profile, *value);

				/* Location */
				trepia_profile_set_location(profile,
						g_hash_table_lookup(info, "p"));

				/* First Name */
				trepia_profile_set_first_name(profile,
						g_hash_table_lookup(info, "g"));

				/* Last Name */
				trepia_profile_set_last_name(profile,
						g_hash_table_lookup(info, "h"));

				/* Profile */
				trepia_profile_set_profile(profile,
						g_hash_table_lookup(info, "o"));

				/* E-mail */
				trepia_profile_set_email(profile,
						g_hash_table_lookup(info, "e"));

				/* AIM */
				trepia_profile_set_aim(profile,
						g_hash_table_lookup(info, "j"));

				/* MSN */
				trepia_profile_set_msn(profile,
						g_hash_table_lookup(info, "k"));

				/* Yahoo */
				trepia_profile_set_yahoo(profile,
						g_hash_table_lookup(info, "l"));

				/* Homepage */
				trepia_profile_set_homepage(profile,
						g_hash_table_lookup(info, "f"));

				/* Country */
				trepia_profile_set_country(profile,
						g_hash_table_lookup(info, "r"));

				/* State */
				trepia_profile_set_state(profile,
						g_hash_table_lookup(info, "s"));

				/* City */
				trepia_profile_set_city(profile,
						g_hash_table_lookup(info, "t"));

				/* Languages */
				trepia_profile_set_languages(profile,
						g_hash_table_lookup(info, "u"));

				/* School */
				trepia_profile_set_school(profile,
						g_hash_table_lookup(info, "v"));

				/* Company */
				trepia_profile_set_company(profile,
						g_hash_table_lookup(info, "w"));

				/* Login Name */
				if ((value = g_hash_table_lookup(info, "d")) != NULL) {
					serv_got_alias(session->gc, id, value);
					trepia_profile_set_location(profile, value);
				}

				/* Buddy Icon */
				if ((value = g_hash_table_lookup(info, "q")) != NULL) {
					char *icon;
					int icon_len;

					frombase64(value, &icon, &icon_len);

					set_icon_data(session->gc, id, icon, icon_len);

					g_free(icon);
				}
				break;

			case TREPIA_MEMBER_OFFLINE:
				id = g_hash_table_lookup(info, "a");

				b = gaim_find_buddy(account, id);

				if (b != NULL)
					serv_got_update(session->gc, id, 0, 0, 0, 0, 0);

				gaim_blist_remove_buddy(b);

				break;

			default:
				break;
		}

		g_hash_table_destroy(info);
	}
	else {
		gaim_debug(GAIM_DEBUG_WARNING, "trepia",
				   "Unknown data received. Possibly an image?\n");
	}

	return TRUE;
}

static void
__data_cb(gpointer data, gint source, GaimInputCondition cond)
{
	TrepiaSession *session = data;
	int i = 0;
	char buf[1025];
	gboolean cont = TRUE;

	i = read(session->fd, buf, 1024);

	if (i <= 0) {
		gaim_connection_error(session->gc, _("Read error"));
		return;
	}

	buf[i] = '\0';

	gaim_debug(GAIM_DEBUG_MISC, "trepia", "__data_cb\n");

	if (session->rxqueue == NULL)
		session->rxqueue = g_string_new(buf);
	else
		g_string_append(session->rxqueue, buf);

	while (cont) {
		char end_tag[5] = "</ >";
		char *end_s;

		end_tag[2] = session->rxqueue->str[1];

		end_s = strstr(session->rxqueue->str, end_tag);

		if (end_s != NULL) {
			char *buffer;
			size_t len;
			int ret;

			end_s += 4;

			len = end_s - session->rxqueue->str;
			buffer = g_new0(char, len + 1);
			strncpy(buffer, session->rxqueue->str, len);

			g_string_erase(session->rxqueue, 0, len);

			if (*session->rxqueue->str == '\n')
				g_string_erase(session->rxqueue, 0, 1);

			gaim_debug(GAIM_DEBUG_MISC, "trepia", "S: %s\n", buffer);

			ret = __parse_data(session, buffer);

			g_free(buffer);
		}
		else
			break;
	}
}

static void
__login_cb(gpointer data, gint source, GaimInputCondition cond)
{
	TrepiaSession *session = data;
	GaimAccount *account;
	const char *password;
	char *buffer;
	char *mac = "00:04:5A:50:31:DE";
	char *gateway_mac = "00:C0:F0:52:D0:A6";
	char buf[3];
	char md5_password[17];
	md5_state_t st;
	md5_byte_t di[16];
	int i;

#if 0
	mac = __get_mac_address();
	gateway_mac = mac;
#endif

	mac = g_strdup("01:02:03:04:05:06");

	gaim_debug(GAIM_DEBUG_INFO, "trepia", "__login_cb\n");

	if (source < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "trepia", "Write error.\n");
		gaim_connection_error(session->gc, _("Write error"));
		return;
	}

	gaim_debug(GAIM_DEBUG_ERROR, "trepia", "Past the first stage.\n");

	session->fd = source;

	account = gaim_connection_get_account(session->gc);

	password = gaim_account_get_password(account);

	md5_init(&st);
	md5_append(&st, (const md5_byte_t *)password, strlen(password));
	md5_finish(&st, di);

	*md5_password = '\0';

	for (i = 0; i < 16; i++) {
		g_snprintf(buf, sizeof(buf), "%02x", di[i]);
		strcat(md5_password, buf);
	}

	buffer = g_strdup_printf(
		"<C>\n"
		"<a>%s</a>\n"
		"<b1>%s</b1>\n"
		"<c>%s</c>\n"
		"<d>%s</d>\n"
		"<e>2.0</e>\n"
		"</C>",
		mac, gateway_mac, gaim_account_get_username(account),
		md5_password);

	g_free(mac);

	if (trepia_write(session->fd, buffer, strlen(buffer)) < 0) {
		gaim_connection_error(session->gc, _("Write error"));
		return;
	}

	g_free(buffer);

	session->gc->inpa = gaim_input_add(session->fd, GAIM_INPUT_READ,
									   __data_cb, session);
}

static void
trepia_login(GaimAccount *account)
{
	GaimConnection *gc;
	TrepiaSession *session;
	const char *server;
	int port;
	int i;

	gaim_debug(GAIM_DEBUG_INFO, "trepia", "trepia_login\n");

	server = gaim_account_get_string(account, "server", TREPIA_SERVER);
	port   = gaim_account_get_int(account,    "port",   TREPIA_PORT);

	gc = gaim_account_get_connection(account);

	session = g_new0(TrepiaSession, 1);
	gc->proto_data = session;
	session->gc = gc;
	session->fd = -1;

	__clear_user_list(account);

	gaim_debug(GAIM_DEBUG_INFO, "trepia", "connecting to proxy\n");

	i = gaim_proxy_connect(account, server, port, __login_cb, session);

	if (i != 0)
		gaim_connection_error(gc, _("Unable to create socket"));
}

static void
trepia_close(GaimConnection *gc)
{
	__clear_user_list(gaim_connection_get_account(gc));

	gc->proto_data = NULL;
}

static int
trepia_send_im(GaimConnection *gc, const char *who, const char *message,
			int len, int flags)
{
	TrepiaSession *session = gc->proto_data;
	char *buffer;

	buffer = g_strdup_printf(
		"<F>\n"
		"<a>%s</a>\n"
		"<b>%s</b>\n"
		"</F>",
		who, message);

	if (trepia_write(session->fd, buffer, strlen(buffer)) < 0) {
		gaim_connection_error(gc, _("Write error"));
		g_free(buffer);
		return 1;
	}

	return 1;
}

static void
trepia_add_buddy(GaimConnection *gc, const char *name)
{
}

static void
trepia_rem_buddy(GaimConnection *gc, char *who, char *group)
{
}

static void
trepia_buddy_free(struct buddy *b)
{
	if (b->proto_data != NULL) {
		trepia_profile_destroy(b->proto_data);

		b->proto_data = NULL;
	}
}

static void
trepia_register_user(GaimAccount *account)
{
#if 0
	char *buffer;

	buffer = g_strdup_printf(
		"<J><a>%s</a><b1>%s</b1><c>2.0</c><d>%s</d><e>%s</e>"
		"<f>%s</f><g></g><h></h><i></i><j></j><k></k><l></l>"
		"<m></m></J>");

#endif
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_TREPIA,
	OPT_PROTO_BUDDY_ICON,
	NULL,
	NULL,
	trepia_list_icon,
	trepia_list_emblems,
	trepia_status_text,
	trepia_tooltip_text,
	NULL,
	NULL,
	NULL,
	NULL,
	trepia_login,
	trepia_close,
	trepia_send_im,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	trepia_add_buddy,
	NULL,
	trepia_rem_buddy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	trepia_register_user,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	trepia_buddy_free,
	NULL,
	NULL
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-trepia",                                    /**< id             */
	"Trepia",                                         /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Trepia Protocol Plugin"),
	                                                  /**  description    */
	N_("Trepia Protocol Plugin"),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Login server"), "server",
											TREPIA_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("Port"), "port", TREPIA_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(trepia, __init_plugin, info);
