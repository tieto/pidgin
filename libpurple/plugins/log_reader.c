#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#ifndef GAIM_PLUGINS
# define GAIM_PLUGINS
#endif

#include "internal.h"

#include "debug.h"
#include "log.h"
#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"
#include "stringref.h"
#include "util.h"
#include "version.h"
#include "xmlnode.h"

/* This must be the last Gaim header included. */
#ifdef _WIN32
#include "win32dep.h"
#endif

/* Where is the Windows partition mounted? */
#ifndef GAIM_LOG_READER_WINDOWS_MOUNT_POINT
#define GAIM_LOG_READER_WINDOWS_MOUNT_POINT "/mnt/windows"
#endif

enum name_guesses {
	NAME_GUESS_UNKNOWN,
	NAME_GUESS_ME,
	NAME_GUESS_THEM
};


/*****************************************************************************
 * Adium Logger                                                              *
 *****************************************************************************/

/* The adium logger doesn't write logs, only reads them.  This is to include
 * Adium logs in the log viewer transparently.
 */

static GaimLogLogger *adium_logger;

enum adium_log_type {
	ADIUM_HTML,
	ADIUM_TEXT,
};

struct adium_logger_data {
	char *path;
	enum adium_log_type type;
};

static GList *adium_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	GList *list = NULL;
	const char *logdir;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prpl_info;
	char *prpl_name;
	char *temp;
	char *path;
	GDir *dir;

	g_return_val_if_fail(sn != NULL, list);
	g_return_val_if_fail(account != NULL, list);

	logdir = gaim_prefs_get_string("/plugins/core/log_reader/adium/log_directory");

	/* By clearing the log directory path, this logger can be (effectively) disabled. */
	if (!*logdir)
		return list;

	plugin = gaim_find_prpl(gaim_account_get_protocol_id(account));
	if (!plugin)
		return NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
	if (!prpl_info->list_icon)
		return NULL;

	prpl_name = g_ascii_strup(prpl_info->list_icon(account, NULL), -1);

	temp = g_strdup_printf("%s.%s", prpl_name, account->username);
	path = g_build_filename(logdir, temp, sn, NULL);
	g_free(temp);

	dir = g_dir_open(path, 0, NULL);
	if (dir) {
		const gchar *file;

		while ((file = g_dir_read_name(dir))) {
			if (!gaim_str_has_prefix(file, sn))
				continue;
			if (gaim_str_has_suffix(file, ".html") || gaim_str_has_suffix(file, ".AdiumHTMLLog")) {
				struct tm tm;
				const char *date = file;

				date += strlen(sn) + 2;
				if (sscanf(date, "%u|%u|%u",
						&tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {

					gaim_debug(GAIM_DEBUG_ERROR, "Adium log parse",
							"Filename timestamp parsing error\n");
				} else {
					char *filename = g_build_filename(path, file, NULL);
					FILE *handle = g_fopen(filename, "rb");
					char *contents;
					char *contents2;
					struct adium_logger_data *data;
					GaimLog *log;

					if (!handle) {
						g_free(filename);
						continue;
					}

					/* XXX: This is really inflexible. */
					contents = g_malloc(57);
					fread(contents, 56, 1, handle);
					fclose(handle);
					contents[56] = '\0';

					/* XXX: This is fairly inflexible. */
					contents2 = contents;
					while (*contents2 && *contents2 != '>')
						contents2++;
					if (*contents2)
						contents2++;
					while (*contents2 && *contents2 != '>')
						contents2++;
					if (*contents2)
						contents2++;

					if (sscanf(contents2, "%u.%u.%u",
							&tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 3) {

						gaim_debug(GAIM_DEBUG_ERROR, "Adium log parse",
								"Contents timestamp parsing error\n");
						g_free(contents);
						g_free(filename);
						continue;
					}
					g_free(contents);

					data = g_new0(struct adium_logger_data, 1);
					data->path = filename;
					data->type = ADIUM_HTML;

					tm.tm_year -= 1900;
					tm.tm_mon  -= 1;

					/* XXX: Look into this later... Should we pass in a struct tm? */
					log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, mktime(&tm), NULL);
					log->logger = adium_logger;
					log->logger_data = data;

					list = g_list_append(list, log);
				}
			} else if (gaim_str_has_suffix(file, ".adiumLog")) {
				struct tm tm;
				const char *date = file;

				date += strlen(sn) + 2;
				if (sscanf(date, "%u|%u|%u",
						&tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {

					gaim_debug(GAIM_DEBUG_ERROR, "Adium log parse",
							"Filename timestamp parsing error\n");
				} else {
					char *filename = g_build_filename(path, file, NULL);
					FILE *handle = g_fopen(filename, "rb");
					char *contents;
					char *contents2;
					struct adium_logger_data *data;
					GaimLog *log;

					if (!handle) {
						g_free(filename);
						continue;
					}

					/* XXX: This is really inflexible. */
					contents = g_malloc(14);
					fread(contents, 13, 1, handle);
					fclose(handle);
					contents[13] = '\0';

					contents2 = contents;
					while (*contents2 && *contents2 != '(')
						contents2++;
					if (*contents2)
						contents2++;

					if (sscanf(contents2, "%u.%u.%u",
							&tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 3) {

						gaim_debug(GAIM_DEBUG_ERROR, "Adium log parse",
								"Contents timestamp parsing error\n");
						g_free(contents);
						g_free(filename);
						continue;
					}

					g_free(contents);

					tm.tm_year -= 1900;
					tm.tm_mon  -= 1;

					data = g_new0(struct adium_logger_data, 1);
					data->path = filename;
					data->type = ADIUM_TEXT;

					/* XXX: Look into this later... Should we pass in a struct tm? */
					log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, mktime(&tm), NULL);
					log->logger = adium_logger;
					log->logger_data = data;

					list = g_list_append(list, log);
				}
			}
		}
		g_dir_close(dir);
	}

	g_free(prpl_name);
	g_free(path);

	return list;
}

static char *adium_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct adium_logger_data *data;
	GError *error = NULL;
	gchar *read = NULL;
	gsize length;

	g_return_val_if_fail(log != NULL, g_strdup(""));

	data = log->logger_data;

	g_return_val_if_fail(data->path != NULL, g_strdup(""));

	gaim_debug(GAIM_DEBUG_INFO, "Adium log read",
				"Reading %s\n", data->path);
	if (!g_file_get_contents(data->path, &read, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "Adium log read",
				"Error reading log\n");
		if (error)
			g_error_free(error);
		return g_strdup("");
	}

	if (data->type != ADIUM_HTML) {
		char *escaped = g_markup_escape_text(read, -1);
		g_free(read);
		read = escaped;
	}

#ifdef WIN32
	/* This problem only seems to show up on Windows.
	 * The BOM is displaying as a space at the beginning of the log.
	 */
	if (gaim_str_has_prefix(read, "\xef\xbb\xbf"))
	{
		/* FIXME: This feels so wrong... */
		char *temp = g_strdup(&(read[3]));
		g_free(read);
		read = temp;
	}
#endif

	/* TODO: Apply formatting.
	 * Replace the above hack with something better, since we'll
	 * be looping over the entire log file contents anyway.
	 */

	return read;
}

static int adium_logger_size (GaimLog *log)
{
	struct adium_logger_data *data;
	char *text;
	size_t size;

	g_return_val_if_fail(log != NULL, 0);

	data = log->logger_data;

	if (gaim_prefs_get_bool("/plugins/core/log_reader/fast_sizes")) {
		struct stat st;

		if (!data->path || stat(data->path, &st))
			st.st_size = 0;

		return st.st_size;
	}

	text = adium_logger_read(log, NULL);
	size = strlen(text);
	g_free(text);

	return size;
}

static void adium_logger_finalize(GaimLog *log)
{
	struct adium_logger_data *data;

	g_return_if_fail(log != NULL);

	data = log->logger_data;

	g_free(data->path);
}


/*****************************************************************************
 * Fire Logger                                                               *
 *****************************************************************************/

#if 0
/* The fire logger doesn't write logs, only reads them.  This is to include
 * Fire logs in the log viewer transparently.
 */

static GaimLogLogger *fire_logger;

struct fire_logger_data {
};

static GList *fire_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	/* TODO: Do something here. */
	return NULL;
}

static char * fire_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct fire_logger_data *data;

	g_return_val_if_fail(log != NULL, g_strdup(""));

	data = log->logger_data;

	/* TODO: Do something here. */
	return g_strdup("");
}

static int fire_logger_size (GaimLog *log)
{
	g_return_val_if_fail(log != NULL, 0);

	if (gaim_prefs_get_bool("/plugins/core/log_reader/fast_sizes"))
		return 0;

	/* TODO: Do something here. */
	return 0;
}

static void fire_logger_finalize(GaimLog *log)
{
	g_return_if_fail(log != NULL);

	/* TODO: Do something here. */
}
#endif


/*****************************************************************************
 * Messenger Plus! Logger                                                    *
 *****************************************************************************/

#if 0
/* The messenger_plus logger doesn't write logs, only reads them.  This is to include
 * Messenger Plus! logs in the log viewer transparently.
 */

static GaimLogLogger *messenger_plus_logger;

struct messenger_plus_logger_data {
};

static GList *messenger_plus_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	/* TODO: Do something here. */
	return NULL;
}

static char * messenger_plus_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct messenger_plus_logger_data *data = log->logger_data;

	g_return_val_if_fail(log != NULL, g_strdup(""));

	data = log->logger_data;

	/* TODO: Do something here. */
	return g_strdup("");
}

static int messenger_plus_logger_size (GaimLog *log)
{
	g_return_val_if_fail(log != NULL, 0);

	if (gaim_prefs_get_bool("/plugins/core/log_reader/fast_sizes"))
		return 0;

	/* TODO: Do something here. */
	return 0;
}

static void messenger_plus_logger_finalize(GaimLog *log)
{
	g_return_if_fail(log != NULL);

	/* TODO: Do something here. */
}
#endif


/*****************************************************************************
 * MSN Messenger Logger                                                      *
 *****************************************************************************/

/* The msn logger doesn't write logs, only reads them.  This is to include
 * MSN Messenger message histories in the log viewer transparently.
 */

static GaimLogLogger *msn_logger;

struct msn_logger_data {
	xmlnode *root;
	xmlnode *message;
	const char *session_id;
	int last_log;
	GString *text;
};

/* This function is really confusing.  It makes baby rlaager cry...
   In other news: "You lost a lot of blood but we found most of it."
 */
static time_t msn_logger_parse_timestamp(xmlnode *message, struct tm **tm_out)
{
	const char *datetime;
	static struct tm tm2;
	time_t stamp;
	const char *date;
	const char *time;
	int month;
	int day;
	int year;
	int hour;
	int min;
	int sec;
	char am_pm;
	char *str;
	static struct tm tm;
	time_t t;
	time_t diff;

#ifndef G_DISABLE_CHECKS
	if (message != NULL)
	{
		*tm_out = NULL;

		/* Trigger the usual warning. */
		g_return_val_if_fail(message != NULL, (time_t)0);
	}
#endif

	datetime = xmlnode_get_attrib(message, "DateTime");
	if (!(datetime && *datetime))
	{
		gaim_debug_error("MSN log timestamp parse",
		                 "Attribute missing: %s\n", "DateTime");
		return (time_t)0;
	}

	stamp = gaim_str_to_time(datetime, TRUE, &tm2, NULL, NULL);
#ifdef HAVE_TM_GMTOFF
	tm2.tm_gmtoff = 0;
#endif
#ifdef HAVE_STRUCT_TM_TM_ZONE
	/* This is used in the place of a timezone abbreviation if the
	 * offset is way off.  The user should never really see it, but
	 * it's here just in case.  The parens are to make it clear it's
	 * not a real timezone. */
	tm2.tm_zone = _("(UTC)");
#endif


	date = xmlnode_get_attrib(message, "Date");
	if (!(date && *date))
	{
		gaim_debug_error("MSN log timestamp parse",
		                 "Attribute missing: %s\n", "Date");
		*tm_out = &tm2;
		return stamp;
	}

	time = xmlnode_get_attrib(message, "Time");
	if (!(time && *time))
	{
		gaim_debug_error("MSN log timestamp parse",
		                 "Attribute missing: %s\n", "Time");
		*tm_out = &tm2;
		return stamp;
	}

	if (sscanf(date, "%u/%u/%u", &month, &day, &year) != 3)
	{
		gaim_debug_error("MSN log timestamp parse",
		                 "%s parsing error\n", "Date");
		*tm_out = &tm2;
		return stamp;
	}
	else
	{
		if (month > 12)
		{
			int tmp = day;
			day = month;
			month = tmp;
		}
	}

	if (sscanf(time, "%u:%u:%u %c", &hour, &min, &sec, &am_pm) != 4)
	{
		gaim_debug_error("MSN log timestamp parse",
		                 "%s parsing error\n", "Time");
		*tm_out = &tm2;
		return stamp;
	}

        if (am_pm == 'P') {
                hour += 12;
        } else if (hour == 12) {
                /* 12 AM = 00 hr */
                hour = 0;
        }

	str = g_strdup_printf("%04i-%02i-%02iT%02i:%02i:%02i", year, month, day, hour, min, sec);
	t = gaim_str_to_time(str, TRUE, &tm, NULL, NULL);


	if (stamp > t)
		diff = stamp - t;
	else
		diff = t - stamp;

	if (diff > (14 * 60 * 60))
	{
		if (day <= 12)
		{
			/* Swap day & month variables, to see if it's a non-US date. */
			g_free(str);
			str = g_strdup_printf("%04i-%02i-%02iT%02i:%02i:%02i", year, month, day, hour, min, sec);
			t = gaim_str_to_time(str, TRUE, &tm, NULL, NULL);

			if (stamp > t)
				diff = stamp - t;
			else
				diff = t - stamp;

			if (diff > (14 * 60 * 60))
			{
				/* We got a time, it's not impossible, but
				 * the diff is too large.  Display the UTC time. */
				g_free(str);
				*tm_out = &tm2;
				return stamp;
			}
			else
			{
				/* Legal time */
				/* Fall out */
			}
		}
		else
		{
			/* We got a time, it's not impossible, but
			 * the diff is too large.  Display the UTC time. */
			g_free(str);
			*tm_out = &tm2;
			return stamp;
		}
	}

	/* If we got here, the time is legal with a reasonable offset.
	 * Let's find out if it's in our TZ. */
	if (gaim_str_to_time(str, FALSE, &tm, NULL, NULL) == stamp)
	{
		g_free(str);
		*tm_out = &tm;
		return stamp;
	}
	g_free(str);

	/* The time isn't in our TZ, but it's reasonable. */
#ifdef HAVE_STRUCT_TM_TM_ZONE
	tm.tm_zone = "   ";
#endif
	*tm_out = &tm;
	return stamp;
}

static GList *msn_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	GList *list = NULL;
	char *username;
	GaimBuddy *buddy;
	const char *logdir;
	const char *savedfilename = NULL;
	char *logfile;
	char *path;
	GError *error = NULL;
	gchar *contents = NULL;
	gsize length;
	xmlnode *root;
	xmlnode *message;
	const char *old_session_id = "";
	struct msn_logger_data *data = NULL;

	g_return_val_if_fail(sn != NULL, list);
	g_return_val_if_fail(account != NULL, list);

	if (strcmp(account->protocol_id, "prpl-msn"))
		return list;

	logdir = gaim_prefs_get_string("/plugins/core/log_reader/msn/log_directory");

	/* By clearing the log directory path, this logger can be (effectively) disabled. */
	if (!*logdir)
		return list;

	buddy = gaim_find_buddy(account, sn);

	if ((username = g_strdup(gaim_account_get_string(
			account, "log_reader_msn_log_folder", NULL)))) {
		/* As a special case, we allow the null string to kill the parsing
		 * straight away. This would allow the user to deal with the case
		 * when two account have the same username at different domains and
		 * only one has logs stored.
		 */
		if (!*username) {
			g_free(username);
			return list;
		}
	} else {
		username = g_strdup(gaim_normalize(account, account->username));
	}

	if (buddy)
		savedfilename = gaim_blist_node_get_string(&buddy->node, "log_reader_msn_log_filename");

	if (savedfilename) {
		/* As a special case, we allow the null string to kill the parsing
		 * straight away. This would allow the user to deal with the case
		 * when two buddies have the same username at different domains and
		 * only one has logs stored.
		 */
		if (!*savedfilename) {
			g_free(username);
			return list;
		}

		logfile = g_strdup(savedfilename);
	} else {
		logfile = g_strdup_printf("%s.xml", gaim_normalize(account, sn));
	}

	path = g_build_filename(logdir, username, "History", logfile, NULL);

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		gboolean found = FALSE;
		char *at_sign;
		GDir *dir;

		g_free(path);

		if (savedfilename) {
			/* We had a saved filename, but it doesn't exist.
			 * Returning now is the right course of action because we don't
			 * want to detect another file incorrectly.
			 */
			g_free(username);
			g_free(logfile);
			return list;
		}

		/* Perhaps we're using a new version of MSN with the weird numbered folders.
		 * I don't know how the numbers are calculated, so I'm going to attempt to
		 * find logs by pattern matching...
		 */

		at_sign = g_strrstr(username, "@");
		if (at_sign)
			*at_sign = '\0';

		dir = g_dir_open(logdir, 0, NULL);
		if (dir) {
			const gchar *name;

			while ((name = g_dir_read_name(dir))) {
				const char *c = name;

				if (!gaim_str_has_prefix(c, username))
					continue;

				c += strlen(username);
				while (*c) {
					if (!g_ascii_isdigit(*c))
						break;

					c++;
				}

				path = g_build_filename(logdir, name, NULL);
				/* The !c makes sure we got to the end of the while loop above. */
				if (!*c && g_file_test(path, G_FILE_TEST_IS_DIR)) {
					char *history_path = g_build_filename(
						path,  "History", NULL);
					if (g_file_test(history_path, G_FILE_TEST_IS_DIR)) {
						gaim_account_set_string(account,
							"log_reader_msn_log_folder", name);
						g_free(path);
						path = history_path;
						found = TRUE;
						break;
					}
					g_free(path);
					g_free(history_path);
				}
				else
					g_free(path);
			}
			g_dir_close(dir);
		}
		g_free(username);

		if (!found) {
			g_free(logfile);
			return list;
		}

		/* If we've reached this point, we've found a History folder. */

		username = g_strdup(gaim_normalize(account, sn));
		at_sign = g_strrstr(username, "@");
		if (at_sign)
			*at_sign = '\0';

		found = FALSE;
		dir = g_dir_open(path, 0, NULL);
		if (dir) {
			const gchar *name;

			while ((name = g_dir_read_name(dir))) {
				const char *c = name;

				if (!gaim_str_has_prefix(c, username))
					continue;

				c += strlen(username);
				while (*c) {
					if (!g_ascii_isdigit(*c))
						break;

					c++;
				}

				path = g_build_filename(path, name, NULL);
				if (!strcmp(c, ".xml") &&
				    g_file_test(path, G_FILE_TEST_EXISTS)) {
					found = TRUE;
					g_free(logfile);
					logfile = g_strdup(name);
					break;
				}
				else
					g_free(path);
			}
			g_dir_close(dir);
		}
		g_free(username);

		if (!found) {
			g_free(logfile);
			return list;
		}
	} else {
		g_free(username);
		g_free(logfile);
		logfile = NULL; /* No sense saving the obvious buddy@domain.com. */
	}

	gaim_debug(GAIM_DEBUG_INFO, "MSN log read",
				"Reading %s\n", path);
	if (!g_file_get_contents(path, &contents, &length, &error)) {
		g_free(path);
		gaim_debug(GAIM_DEBUG_ERROR, "MSN log read",
				"Error reading log\n");
		if (error)
			g_error_free(error);
		return list;
	}
	g_free(path);

	/* Reading the file was successful...
	 * Save its name if it involves the crazy numbers. The idea here is that you could
	 * then tweak the blist.xml file by hand if need be. This would be the case if two
	 * buddies have the same username at different domains. One set of logs would get
	 * detected for both buddies.
	 */
	if (buddy && logfile) {
		gaim_blist_node_set_string(&buddy->node, "log_reader_msn_log_filename", logfile);
		g_free(logfile);
	}

	root = xmlnode_from_str(contents, length);
	g_free(contents);
	if (!root)
		return list;

	for (message = xmlnode_get_child(root, "Message"); message;
			message = xmlnode_get_next_twin(message)) {
		const char *session_id;

		session_id = xmlnode_get_attrib(message, "SessionID");
		if (!session_id) {
			gaim_debug(GAIM_DEBUG_ERROR, "MSN log parse",
					"Error parsing message: %s\n", "SessionID missing");
			continue;
		}

		if (strcmp(session_id, old_session_id)) {
			/*
			 * The session ID differs from the last message.
			 * Thus, this is the start of a new conversation.
			 */
			struct tm *tm;
			time_t stamp;
			GaimLog *log;

			data = g_new0(struct msn_logger_data, 1);
			data->root = root;
			data->message = message;
			data->session_id = session_id;
			data->text = NULL;
			data->last_log = FALSE;

			stamp = msn_logger_parse_timestamp(message, &tm);

			log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, stamp, tm);
			log->logger = msn_logger;
			log->logger_data = data;

			list = g_list_append(list, log);
		}
		old_session_id = session_id;
	}

	if (data)
		data->last_log = TRUE;

	return list;
}

static char * msn_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct msn_logger_data *data;
	GString *text = NULL;
	xmlnode *message;

	g_return_val_if_fail(log != NULL, g_strdup(""));

	data = log->logger_data;

	if (data->text) {
		/* The GTK code which displays the logs g_free()s whatever is
		 * returned from this function. Thus, we can't reuse the str
		 * part of the GString. The only solution is to free it and
		 * start over.
		 */
		g_string_free(data->text, FALSE);
	}

	text = g_string_new("");

	if (!data->root || !data->message || !data->session_id) {
		/* Something isn't allocated correctly. */
		gaim_debug(GAIM_DEBUG_ERROR, "MSN log parse",
				"Error parsing message: %s\n", "Internal variables inconsistent");
		data->text = text;

		return text->str;
	}

	for (message = data->message; message;
			message = xmlnode_get_next_twin(message)) {

		const char *new_session_id;
		xmlnode *text_node;
		const char *from_name = NULL;
		const char *to_name = NULL;
		xmlnode *from;
		xmlnode *to;
		enum name_guesses name_guessed = NAME_GUESS_UNKNOWN;
		const char *their_name;
		time_t time_unix;
		struct tm *tm;
		char *timestamp;
		char *tmp;
		const char *style;

		new_session_id = xmlnode_get_attrib(message, "SessionID");

		/* If this triggers, something is wrong with the XML. */
		if (!new_session_id) {
			gaim_debug(GAIM_DEBUG_ERROR, "MSN log parse",
					"Error parsing message: %s\n", "New SessionID missing");
			break;
		}

		if (strcmp(new_session_id, data->session_id)) {
			/* The session ID differs from the first message.
			 * Thus, this is the start of a new conversation.
			 */
			break;
		}

		text_node = xmlnode_get_child(message, "Text");
		if (!text_node)
			continue;

		from = xmlnode_get_child(message, "From");
		if (from) {
			xmlnode *user = xmlnode_get_child(from, "User");

			if (user) {
				from_name = xmlnode_get_attrib(user, "FriendlyName");

				/* This saves a check later. */
				if (!*from_name)
					from_name = NULL;
			}
		}

		to = xmlnode_get_child(message, "To");
		if (to) {
			xmlnode *user = xmlnode_get_child(to, "User");
			if (user) {
				to_name = xmlnode_get_attrib(user, "FriendlyName");

				/* This saves a check later. */
				if (!*to_name)
					to_name = NULL;
			}
		}

		their_name = from_name;
		if (from_name && gaim_prefs_get_bool("/plugins/core/log_reader/use_name_heuristics")) {
			const char *friendly_name = gaim_connection_get_display_name(log->account->gc);

			if (friendly_name != NULL) {
				int friendly_name_length = strlen(friendly_name);
				const char *alias;
				int alias_length;
				GaimBuddy *buddy = gaim_find_buddy(log->account, log->name);
				gboolean from_name_matches;
				gboolean to_name_matches;

				if (buddy && buddy->alias)
					their_name = buddy->alias;

				if (log->account->alias)
				{
					alias = log->account->alias;
					alias_length = strlen(alias);
				}
				else
				{
					alias = "";
					alias_length = 0;
				}

				/* Try to guess which user is me.
				 * The first step is to determine if either of the names matches either my
				 * friendly name or alias. For this test, "match" is defined as:
				 * ^(friendly_name|alias)([^a-zA-Z0-9].*)?$
				 */
				from_name_matches = (gaim_str_has_prefix(from_name, friendly_name) &&
				                      !isalnum(*(from_name + friendly_name_length))) ||
				                     (gaim_str_has_prefix(from_name, alias) &&
				                      !isalnum(*(from_name + alias_length)));

				to_name_matches = to_name != NULL && (
				                   (gaim_str_has_prefix(to_name, friendly_name) &&
				                    !isalnum(*(to_name + friendly_name_length))) ||
				                   (gaim_str_has_prefix(to_name, alias) &&
				                    !isalnum(*(to_name + alias_length))));

				if (from_name_matches) {
					if (!to_name_matches) {
						name_guessed = NAME_GUESS_ME;
					}
				} else if (to_name_matches) {
					name_guessed = NAME_GUESS_THEM;
				} else {
					if (buddy && buddy->alias) {
						char *alias = g_strdup(buddy->alias);

						/* "Truncate" the string at the first non-alphanumeric
						 * character. The idea is to relax the comparison.
						 */
						char *temp;
						for (temp = alias; *temp ; temp++) {
							if (!isalnum(*temp)) {
								*temp = '\0';
								break;
							}
						}
						alias_length = strlen(alias);

						/* Try to guess which user is them.
						 * The first step is to determine if either of the names
						 * matches their alias. For this test, "match" is
						 * defined as: ^alias([^a-zA-Z0-9].*)?$
						 */
						from_name_matches = (gaim_str_has_prefix(
								from_name, alias) &&
								!isalnum(*(from_name +
								alias_length)));

						to_name_matches = to_name && (gaim_str_has_prefix(
								to_name, alias) &&
								!isalnum(*(to_name +
								alias_length)));

						g_free(alias);

						if (from_name_matches) {
							if (!to_name_matches) {
								name_guessed = NAME_GUESS_THEM;
							}
						} else if (to_name_matches) {
							name_guessed = NAME_GUESS_ME;
						} else if (buddy->server_alias) {
							friendly_name_length =
								strlen(buddy->server_alias);

							/* Try to guess which user is them.
							 * The first step is to determine if either of
							 * the names matches their friendly name. For
							 * this test, "match" is defined as:
							 * ^friendly_name([^a-zA-Z0-9].*)?$
							 */
							from_name_matches = (gaim_str_has_prefix(
									from_name,
									buddy->server_alias) &&
									!isalnum(*(from_name +
									friendly_name_length)));

							to_name_matches = to_name && (
									(gaim_str_has_prefix(
									to_name, buddy->server_alias) &&
									!isalnum(*(to_name +
									friendly_name_length))));

							if (from_name_matches) {
								if (!to_name_matches) {
									name_guessed = NAME_GUESS_THEM;
								}
							} else if (to_name_matches) {
								name_guessed = NAME_GUESS_ME;
							}
						}
					}
				}
			}
		}

		if (name_guessed != NAME_GUESS_UNKNOWN) {
			text = g_string_append(text, "<span style=\"color: #");
			if (name_guessed == NAME_GUESS_ME)
				text = g_string_append(text, "16569E");
			else
				text = g_string_append(text, "A82F2F");
			text = g_string_append(text, ";\">");
		}

		time_unix = msn_logger_parse_timestamp(message, &tm);

		timestamp = g_strdup_printf("<font size=\"2\">(%02u:%02u:%02u)</font> ",
				tm->tm_hour, tm->tm_min, tm->tm_sec);
		text = g_string_append(text, timestamp);
		g_free(timestamp);

		if (from_name) {
			text = g_string_append(text, "<b>");

			if (name_guessed == NAME_GUESS_ME) {
				if (log->account->alias)
					text = g_string_append(text, log->account->alias);
				else
					text = g_string_append(text, log->account->username);
			}
			else if (name_guessed == NAME_GUESS_THEM)
				text = g_string_append(text, their_name);
			else
				text = g_string_append(text, from_name);

			text = g_string_append(text, ":</b> ");
		}

		if (name_guessed != NAME_GUESS_UNKNOWN)
			text = g_string_append(text, "</span>");

		style     = xmlnode_get_attrib(text_node, "Style");

		tmp = xmlnode_get_data(text_node);
		if (style && *style) {
			text = g_string_append(text, "<span style=\"");
			text = g_string_append(text, style);
			text = g_string_append(text, "\">");
			text = g_string_append(text, tmp);
			text = g_string_append(text, "</span>\n");
		} else {
			text = g_string_append(text, tmp);
			text = g_string_append(text, "\n");
		}
		g_free(tmp);
	}

	data->text = text;

	return text->str;
}

static int msn_logger_size (GaimLog *log)
{
	char *text;
	size_t size;

	g_return_val_if_fail(log != NULL, 0);

	if (gaim_prefs_get_bool("/plugins/core/log_reader/fast_sizes"))
		return 0;

	text = msn_logger_read(log, NULL);
	size = strlen(text);
	g_free(text);

	return size;
}

static void msn_logger_finalize(GaimLog *log)
{
	struct msn_logger_data *data;

	g_return_if_fail(log != NULL);

	data = log->logger_data;

	if (data->last_log)
		xmlnode_free(data->root);

	if (data->text)
		g_string_free(data->text, FALSE);
}


/*****************************************************************************
 * Trillian Logger                                                           *
 *****************************************************************************/

/* The trillian logger doesn't write logs, only reads them.  This is to include
 * Trillian logs in the log viewer transparently.
 */

static GaimLogLogger *trillian_logger;
static void trillian_logger_finalize(GaimLog *log);

struct trillian_logger_data {
	char *path; /* FIXME: Change this to use GaimStringref like log.c:old_logger_list */
	int offset;
	int length;
	char *their_nickname;
};

static GList *trillian_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	GList *list = NULL;
	const char *logdir;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prpl_info;
	char *prpl_name;
	const char *buddy_name;
	char *filename;
	char *path;
	GError *error = NULL;
	gchar *contents = NULL;
	gsize length;
	gchar *line;
	gchar *c;

	g_return_val_if_fail(sn != NULL, list);
	g_return_val_if_fail(account != NULL, list);

	logdir = gaim_prefs_get_string("/plugins/core/log_reader/trillian/log_directory");

	/* By clearing the log directory path, this logger can be (effectively) disabled. */
	if (!*logdir)
		return list;

	plugin = gaim_find_prpl(gaim_account_get_protocol_id(account));
	if (!plugin)
		return NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
	if (!prpl_info->list_icon)
		return NULL;

	prpl_name = g_ascii_strup(prpl_info->list_icon(account, NULL), -1);

	buddy_name = gaim_normalize(account, sn);

	filename = g_strdup_printf("%s.log", buddy_name);
	path = g_build_filename(
		logdir, prpl_name, filename, NULL);

	gaim_debug(GAIM_DEBUG_INFO, "Trillian log list",
				"Reading %s\n", path);
	/* FIXME: There's really no need to read the entire file at once.
	 * See src/log.c:old_logger_list for a better approach.
	 */
	if (!g_file_get_contents(path, &contents, &length, &error)) {
		if (error) {
			g_error_free(error);
			error = NULL;
		}
		g_free(path);

		path = g_build_filename(
			logdir, prpl_name, "Query", filename, NULL);
		gaim_debug(GAIM_DEBUG_INFO, "Trillian log list",
					"Reading %s\n", path);
		if (!g_file_get_contents(path, &contents, &length, &error)) {
			if (error)
				g_error_free(error);
		}
	}
	g_free(filename);

	if (contents) {
		struct trillian_logger_data *data = NULL;
		int offset = 0;
		int last_line_offset = 0;

		line = contents;
		c = contents;
		while (*c) {
			offset++;

			if (*c != '\n') {
				c++;
				continue;
			}

			*c = '\0';
			if (gaim_str_has_prefix(line, "Session Close ")) {
				if (data && !data->length) {
					if (!(data->length = last_line_offset - data->offset)) {
						/* This log had no data, so we remove it. */
						GList *last = g_list_last(list);

						gaim_debug(GAIM_DEBUG_INFO, "Trillian log list",
							"Empty log. Offset %i\n", data->offset);

						trillian_logger_finalize((GaimLog *)last->data);
						list = g_list_delete_link(list, last);
					}
				}
			} else if (line[0] && line[1] && line [3] &&
					   gaim_str_has_prefix(&line[3], "sion Start ")) {

				char *their_nickname = line;
				char *timestamp;

				if (data && !data->length)
					data->length = last_line_offset - data->offset;

				while (*their_nickname && (*their_nickname != ':'))
					their_nickname++;
				their_nickname++;

				/* This code actually has nothing to do with
				 * the timestamp YET. I'm simply using this
				 * variable for now to NUL-terminate the
				 * their_nickname string.
				 */
				timestamp = their_nickname;
				while (*timestamp && *timestamp != ')')
					timestamp++;

				if (*timestamp == ')') {
					char *month;
					struct tm tm;

					*timestamp = '\0';
					if (line[0] && line[1] && line[2])
						timestamp += 3;

					/* Now we start dealing with the timestamp. */

					/* Skip over the day name. */
					while (*timestamp && (*timestamp != ' '))
						timestamp++;
					*timestamp = '\0';
					timestamp++;

					/* Parse out the month. */
					month = timestamp;
					while (*timestamp &&  (*timestamp != ' '))
						timestamp++;
					*timestamp = '\0';
					timestamp++;

					/* Parse the day, time, and year. */
					if (sscanf(timestamp, "%u %u:%u:%u %u",
							&tm.tm_mday, &tm.tm_hour,
							&tm.tm_min, &tm.tm_sec,
							&tm.tm_year) != 5) {

						gaim_debug(GAIM_DEBUG_ERROR,
							"Trillian log timestamp parse",
							"Session Start parsing error\n");
					} else {
						GaimLog *log;

						tm.tm_year -= 1900;

						/* Let the C library deal with
						 * daylight savings time.
						 */
						tm.tm_isdst = -1;

						/* Ugly hack, in case current locale
						 * is not English. This code is taken
						 * from log.c.
						 */
						if (strcmp(month, "Jan") == 0) {
							tm.tm_mon= 0;
						} else if (strcmp(month, "Feb") == 0) {
							tm.tm_mon = 1;
						} else if (strcmp(month, "Mar") == 0) {
							tm.tm_mon = 2;
						} else if (strcmp(month, "Apr") == 0) {
							tm.tm_mon = 3;
						} else if (strcmp(month, "May") == 0) {
							tm.tm_mon = 4;
						} else if (strcmp(month, "Jun") == 0) {
							tm.tm_mon = 5;
						} else if (strcmp(month, "Jul") == 0) {
							tm.tm_mon = 6;
						} else if (strcmp(month, "Aug") == 0) {
							tm.tm_mon = 7;
						} else if (strcmp(month, "Sep") == 0) {
							tm.tm_mon = 8;
						} else if (strcmp(month, "Oct") == 0) {
							tm.tm_mon = 9;
						} else if (strcmp(month, "Nov") == 0) {
							tm.tm_mon = 10;
						} else if (strcmp(month, "Dec") == 0) {
							tm.tm_mon = 11;
						}

						data = g_new0(
							struct trillian_logger_data, 1);
						data->path = g_strdup(path);
						data->offset = offset;
						data->length = 0;
						data->their_nickname =
							g_strdup(their_nickname);

						/* XXX: Look into this later... Should we pass in a struct tm? */
						log = gaim_log_new(GAIM_LOG_IM,
							sn, account, NULL, mktime(&tm), NULL);
						log->logger = trillian_logger;
						log->logger_data = data;

						list = g_list_append(list, log);
					}
				}
			}
			c++;
			line = c;
			last_line_offset = offset;
		}

		g_free(contents);
	}
	g_free(path);

	g_free(prpl_name);

	return list;
}

static char * trillian_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct trillian_logger_data *data;
	char *read;
	FILE *file;
	GaimBuddy *buddy;
	char *escaped;
	GString *formatted;
	char *c;
	char *line;

	g_return_val_if_fail(log != NULL, g_strdup(""));

	data = log->logger_data;

	g_return_val_if_fail(data->path != NULL, g_strdup(""));
	g_return_val_if_fail(data->length > 0, g_strdup(""));
	g_return_val_if_fail(data->their_nickname != NULL, g_strdup(""));

	gaim_debug(GAIM_DEBUG_INFO, "Trillian log read",
				"Reading %s\n", data->path);

	read = g_malloc(data->length + 2);

	file = g_fopen(data->path, "rb");
	fseek(file, data->offset, SEEK_SET);
	fread(read, data->length, 1, file);
	fclose(file);

	if (read[data->length-1] == '\n') {
		read[data->length] = '\0';
	} else {
		read[data->length] = '\n';
		read[data->length+1] = '\0';
	}

	/* Load miscellaneous data. */
	buddy = gaim_find_buddy(log->account, log->name);

	escaped = g_markup_escape_text(read, -1);
	g_free(read);
	read = escaped;

	/* Apply formatting... */
	formatted = g_string_new("");
	c = read;
	line = read;
	while (*c)
	{
		if (*c == '\n')
		{
			char *link_temp_line;
			char *link;
			char *timestamp;
			char *footer = NULL;
			*c = '\0';

			/* Convert links.
			 *
			 * The format is (Link: URL)URL
			 * So, I want to find each occurance of "(Link: " and replace that chunk with:
			 * <a href="
			 * Then, replace the next ")" with:
			 * ">
			 * Then, replace the next " " (or add this if the end-of-line is reached) with:
			 * </a>
			 */
			link_temp_line = NULL;
			while ((link = g_strstr_len(line, strlen(line), "(Link: "))) {
				GString *temp;

				if (!*link)
					continue;

				*link = '\0';
				link++;

				temp = g_string_new(line);
				g_string_append(temp, "<a href=\"");

				if (strlen(link) >= 6) {
					link += (sizeof("(Link: ") - 1);

					while (*link && *link != ')') {
						g_string_append_c(temp, *link);
						link++;
					}
					if (link) {
						link++;

						g_string_append(temp, "\">");
						while (*link && *link != ' ') {
							g_string_append_c(temp, *link);
							link++;
						}
						g_string_append(temp, "</a>");
					}

					g_string_append(temp, link);

					/* Free the last round's line. */
					if (link_temp_line)
						g_free(line);

					line = temp->str;
					g_string_free(temp, FALSE);

					/* Save this memory location so we can free it later. */
					link_temp_line = line;
				}
			}

			timestamp = "";
			if (*line == '[') {
				timestamp = line;
				while (*timestamp && *timestamp != ']')
					timestamp++;
				if (*timestamp == ']') {
					*timestamp = '\0';
					line++;
					/* TODO: Parse the timestamp and convert it to Gaim's format. */
					g_string_append_printf(formatted,
						"<font size=\"2\">(%s)</font> ", line);
					line = timestamp;
					if (line[1] && line[2])
						line += 2;
				}

				if (gaim_str_has_prefix(line, "*** ")) {
					line += (sizeof("*** ") - 1);
					g_string_append(formatted, "<b>");
					footer = "</b>";
					if (gaim_str_has_prefix(line, "NOTE: This user is offline.")) {
						line = _("User is offline.");
					} else if (gaim_str_has_prefix(line,
							"NOTE: Your status is currently set to ")) {

						line += (sizeof("NOTE: ") - 1);
					} else if (gaim_str_has_prefix(line, "Auto-response sent to ")) {
						g_string_append(formatted, _("Auto-response sent:"));
						while (*line && *line != ':')
							line++;
						if (*line)
							line++;
						g_string_append(formatted, "</b>");
						footer = NULL;
					} else if (strstr(line, " signed off ")) {
						if (buddy != NULL && buddy->alias)
							g_string_append_printf(formatted,
								_("%s has signed off."), buddy->alias);
						else
							g_string_append_printf(formatted,
								_("%s has signed off."), log->name);
						line = "";
					} else if (strstr(line, " signed on ")) {
						if (buddy != NULL && buddy->alias)
							g_string_append(formatted, buddy->alias);
						else
							g_string_append(formatted, log->name);
						line = " logged in.";
					} else if (gaim_str_has_prefix(line,
						"One or more messages may have been undeliverable.")) {

						g_string_append(formatted,
							"<span style=\"color: #ff0000;\">");
						g_string_append(formatted,
							_("One or more messages may have been "
							  "undeliverable."));
						line = "";
						footer = "</span></b>";
					} else if (gaim_str_has_prefix(line,
							"You have been disconnected.")) {

						g_string_append(formatted,
							"<span style=\"color: #ff0000;\">");
						g_string_append(formatted,
							_("You were disconnected from the server."));
						line = "";
						footer = "</span></b>";
					} else if (gaim_str_has_prefix(line,
							"You are currently disconnected.")) {

						g_string_append(formatted,
							"<span style=\"color: #ff0000;\">");
						line = _("You are currently disconnected. Messages "
						         "will not be received unless you are "
						         "logged in.");
						footer = "</span></b>";
					} else if (gaim_str_has_prefix(line,
							"Your previous message has not been sent.")) {

						g_string_append(formatted,
							"<span style=\"color: #ff0000;\">");

						if (gaim_str_has_prefix(line,
							"Your previous message has not been sent.  "
							"Reason: Maximum length exceeded.")) {

							g_string_append(formatted,
								_("Message could not be sent because "
								  "the maximum length was exceeded."));
							line = "";
						} else {
							g_string_append(formatted,
								_("Message could not be sent."));
							line += (sizeof(
								"Your previous message "
								"has not been sent. ") - 1);
						}

						footer = "</span></b>";
					}
				} else if (gaim_str_has_prefix(line, data->their_nickname)) {
					if (buddy != NULL && buddy->alias) {
						line += strlen(data->their_nickname) + 2;
						g_string_append_printf(formatted,
							"<span style=\"color: #A82F2F;\">"
							"<b>%s</b></span>: ", buddy->alias);
					}
				} else {
					char *line2 = line;
					while (*line2 && *line2 != ':')
						line2++;
					if (*line2 == ':') {
						const char *acct_name;
						line2++;
						line = line2;
						acct_name = gaim_account_get_alias(log->account);
						if (!acct_name)
							acct_name = gaim_account_get_username(log->account);

						g_string_append_printf(formatted,
							"<span style=\"color: #16569E;\">"
							"<b>%s</b></span>:", acct_name);
					}
				}
			}

			g_string_append(formatted, line);

			if (footer)
				g_string_append(formatted, footer);

			g_string_append_c(formatted, '\n');

			if (link_temp_line)
				g_free(link_temp_line);

			c++;
			line = c;
		} else
			c++;
	}

	g_free(read);
	read = formatted->str;
	g_string_free(formatted, FALSE);

	return read;
}

static int trillian_logger_size (GaimLog *log)
{
	struct trillian_logger_data *data;
	char *text;
	size_t size;

	g_return_val_if_fail(log != NULL, 0);

	data = log->logger_data;

	if (gaim_prefs_get_bool("/plugins/core/log_reader/fast_sizes")) {
		return data ? data->length : 0;
	}

	text = trillian_logger_read(log, NULL);
	size = strlen(text);
	g_free(text);

	return size;
}

static void trillian_logger_finalize(GaimLog *log)
{
	struct trillian_logger_data *data;

	g_return_if_fail(log != NULL);

	data = log->logger_data;

	g_free(data->path);
	g_free(data->their_nickname);

}


/*****************************************************************************
 * Plugin Code                                                               *
 *****************************************************************************/

static void
init_plugin(GaimPlugin *plugin)
{
	char *path;
#ifdef _WIN32
	char *folder;
	gboolean found = FALSE;
#endif

	g_return_if_fail(plugin != NULL);

	gaim_prefs_add_none("/plugins/core/log_reader");


	/* Add general preferences. */

	gaim_prefs_add_bool("/plugins/core/log_reader/fast_sizes", FALSE);
	gaim_prefs_add_bool("/plugins/core/log_reader/use_name_heuristics", TRUE);


	/* Add Adium log directory preference. */
	gaim_prefs_add_none("/plugins/core/log_reader/adium");

	/* Calculate default Adium log directory. */
#ifdef _WIN32
		path = "";
#else
		path = g_build_filename(gaim_home_dir(), "Library", "Application Support",
			"Adium 2.0", "Users", "Default", "Logs", NULL);
#endif

	gaim_prefs_add_string("/plugins/core/log_reader/adium/log_directory", path);

#ifndef _WIN32
	g_free(path);
#endif


	/* Add Fire log directory preference. */
	gaim_prefs_add_none("/plugins/core/log_reader/fire");

	/* Calculate default Fire log directory. */
#ifdef _WIN32
		path = "";
#else
		path = g_build_filename(gaim_home_dir(), "Library", "Application Support",
			"Fire", "Sessions", NULL);
#endif

	gaim_prefs_add_string("/plugins/core/log_reader/fire/log_directory", path);

#ifndef _WIN32
	g_free(path);
#endif


	/* Add Messenger Plus! log directory preference. */
	gaim_prefs_add_none("/plugins/core/log_reader/messenger_plus");

	/* Calculate default Messenger Plus! log directory. */
#ifdef _WIN32
	folder = wgaim_get_special_folder(CSIDL_PERSONAL);
	if (folder) {
#endif
	path = g_build_filename(
#ifdef _WIN32
		folder,
#else
		GAIM_LOG_READER_WINDOWS_MOUNT_POINT, "Documents and Settings",
		g_get_user_name(), "My Documents",
#endif
		"My Chat Logs", NULL);
#ifdef _WIN32
	g_free(folder);
	} else /* !folder */
		path = g_strdup("");
#endif

	gaim_prefs_add_string("/plugins/core/log_reader/messenger_plus/log_directory", path);
	g_free(path);


	/* Add MSN Messenger log directory preference. */
	gaim_prefs_add_none("/plugins/core/log_reader/msn");

	/* Calculate default MSN message history directory. */
#ifdef _WIN32
	folder = wgaim_get_special_folder(CSIDL_PERSONAL);
	if (folder) {
#endif
	path = g_build_filename(
#ifdef _WIN32
		folder,
#else
		GAIM_LOG_READER_WINDOWS_MOUNT_POINT, "Documents and Settings",
		g_get_user_name(), "My Documents",
#endif
		"My Received Files", NULL);
#ifdef _WIN32
	g_free(folder);
	} else /* !folder */
		path = g_strdup("");
#endif

	gaim_prefs_add_string("/plugins/core/log_reader/msn/log_directory", path);
	g_free(path);


	/* Add Trillian log directory preference. */
	gaim_prefs_add_none("/plugins/core/log_reader/trillian");

#ifdef _WIN32
	/* XXX: While a major hack, this is the most reliable way I could
	 * think of to determine the Trillian installation directory.
	 */

	path = NULL;
	if ((folder = wgaim_read_reg_string(HKEY_CLASSES_ROOT, "Trillian.SkinZip\\shell\\Add\\command\\", NULL))) {
		char *value = folder;
		char *temp;

		/* Break apart buffer. */
		if (*value == '"') {
			value++;
			temp = value;
			while (*temp && *temp != '"')
				temp++;
		} else {
			temp = value;
			while (*temp && *temp != ' ')
				temp++;
		}
		*temp = '\0';

		/* Set path. */
		if (gaim_str_has_suffix(value, "trillian.exe")) {
			value[strlen(value) - (sizeof("trillian.exe") - 1)] = '\0';
			path = g_build_filename(value, "users", "default", "talk.ini", NULL);
		}
		g_free(folder);
	}

	if (!path) {
		char *folder = wgaim_get_special_folder(CSIDL_PROGRAM_FILES);
		if (folder) {
			path = g_build_filename(folder, "Trillian",
					"users", "default", "talk.ini", NULL);
			g_free(folder);
		}
	}

	if (path) {
		/* Read talk.ini file to find the log directory. */
		GError *error = NULL;

#if 0 && GLIB_CHECK_VERSION(2,6,0) /* FIXME: Not tested yet. */
		GKeyFile *key_file;

		gaim_debug(GAIM_DEBUG_INFO, "Trillian talk.ini read",
				"Reading %s\n", path);
		if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, GError &error)) {
			gaim_debug(GAIM_DEBUG_ERROR, "Trillian talk.ini read",
					"Error reading talk.ini\n");
			if (error)
				g_error_free(error);
		} else {
			char *logdir = g_key_file_get_string(key_file, "Logging", "Directory", &error);
			if (error) {
				gaim_debug(GAIM_DEBUG_ERROR, "Trillian talk.ini read",
						"Error reading Directory value from Logging section\n");
				g_error_free(error);
			}

			if (logdir) {
				g_strchomp(logdir);
				gaim_prefs_add_string(
					"/plugins/core/log_reader/trillian/log_directory", logdir);
				found = TRUE;
			}

			g_key_file_free(key_file);
		}
#else /* !GLIB_CHECK_VERSION(2,6,0) */
		gsize length;
		gchar *contents = NULL;

		gaim_debug(GAIM_DEBUG_INFO, "Trillian talk.ini read",
					"Reading %s\n", path);
		if (!g_file_get_contents(path, &contents, &length, &error)) {
			gaim_debug(GAIM_DEBUG_ERROR, "Trillian talk.ini read",
					"Error reading talk.ini\n");
			if (error)
				g_error_free(error);
		} else {
			char *line = contents;
			while (*contents) {
				if (*contents == '\n') {
					*contents = '\0';

					/* XXX: This assumes the first Directory key is under [Logging]. */
					if (gaim_str_has_prefix(line, "Directory=")) {
						line += (sizeof("Directory=") - 1);
						g_strchomp(line);
						gaim_prefs_add_string(
							"/plugins/core/log_reader/trillian/log_directory",
							line);
						found = TRUE;
					}

					contents++;
					line = contents;
				} else
					contents++;
			}
			g_free(path);
			g_free(contents);
		}
#endif /* !GTK_CHECK_VERSION(2,6,0) */
	} /* path */

	if (!found) {
#endif /* defined(_WIN32) */

	/* Calculate default Trillian log directory. */
#ifdef _WIN32
	folder = wgaim_get_special_folder(CSIDL_PROGRAM_FILES);
	if (folder) {
#endif
	path = g_build_filename(
#ifdef _WIN32
		folder,
#else
		GAIM_LOG_READER_WINDOWS_MOUNT_POINT, "Program Files",
#endif
		"Trillian", "users", "default", "logs", NULL);
#ifdef _WIN32
	g_free(folder);
	} else /* !folder */
		path = g_strdup("");
#endif

	gaim_prefs_add_string("/plugins/core/log_reader/trillian/log_directory", path);
	g_free(path);

#ifdef _WIN32
	} /* !found */
#endif
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	/* The names of IM clients are marked for translation at the request of
	   translators who wanted to transliterate them.  Many translators
	   choose to leave them alone.  Choose what's best for your language. */
	adium_logger = gaim_log_logger_new("adium", _("Adium"), 6,
									   NULL,
									   NULL,
									   adium_logger_finalize,
									   adium_logger_list,
									   adium_logger_read,
									   adium_logger_size);
	gaim_log_logger_add(adium_logger);

#if 0
	/* The names of IM clients are marked for translation at the request of
	   translators who wanted to transliterate them.  Many translators
	   choose to leave them alone.  Choose what's best for your language. */
	fire_logger = gaim_log_logger_new("fire", _("Fire"), 6,
									  NULL,
									  NULL,
									  fire_logger_finalize,
									  fire_logger_list,
									  fire_logger_read,
									  fire_logger_size);
	gaim_log_logger_add(fire_logger);

	/* The names of IM clients are marked for translation at the request of
	   translators who wanted to transliterate them.  Many translators
	   choose to leave them alone.  Choose what's best for your language. */
	messenger_plus_logger = gaim_log_logger_new("messenger_plus", _("Messenger Plus!"), 6,
												NULL,
												NULL,
												messenger_plus_logger_finalize,
												messenger_plus_logger_list,
												messenger_plus_logger_read,
												messenger_plus_logger_size);
	gaim_log_logger_add(messenger_plus_logger);
#endif

	/* The names of IM clients are marked for translation at the request of
	   translators who wanted to transliterate them.  Many translators
	   choose to leave them alone.  Choose what's best for your language. */
	msn_logger = gaim_log_logger_new("msn", _("MSN Messenger"), 6,
									 NULL,
									 NULL,
									 msn_logger_finalize,
									 msn_logger_list,
									 msn_logger_read,
									 msn_logger_size);
	gaim_log_logger_add(msn_logger);

	/* The names of IM clients are marked for translation at the request of
	   translators who wanted to transliterate them.  Many translators
	   choose to leave them alone.  Choose what's best for your language. */
	trillian_logger = gaim_log_logger_new("trillian", _("Trillian"), 6,
										  NULL,
										  NULL,
										  trillian_logger_finalize,
										  trillian_logger_list,
										  trillian_logger_read,
										  trillian_logger_size);
	gaim_log_logger_add(trillian_logger);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	gaim_log_logger_remove(adium_logger);
#if 0
	gaim_log_logger_remove(fire_logger);
	gaim_log_logger_remove(messenger_plus_logger);
#endif
	gaim_log_logger_remove(msn_logger);
	gaim_log_logger_remove(trillian_logger);

	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	g_return_val_if_fail(plugin != NULL, FALSE);

	frame = gaim_plugin_pref_frame_new();


	/* Add general preferences. */

	ppref = gaim_plugin_pref_new_with_label(_("General Log Reading Configuration"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/fast_sizes", _("Fast size calculations"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/use_name_heuristics", _("Use name heuristics"));
	gaim_plugin_pref_frame_add(frame, ppref);


	/* Add Log Directory preferences. */

	ppref = gaim_plugin_pref_new_with_label(_("Log Directory"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/adium/log_directory", _("Adium"));
	gaim_plugin_pref_frame_add(frame, ppref);

#if 0
	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/fire/log_directory", _("Fire"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/messenger_plus/log_directory", _("Messenger Plus!"));
	gaim_plugin_pref_frame_add(frame, ppref);
#endif

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/msn/log_directory", _("MSN Messenger"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
		"/plugins/core/log_reader/trillian/log_directory", _("Trillian"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,   /* page_num (reserved) */
	NULL /* frame (reserved) */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */
	"core-log_reader",                                /**< id             */
	N_("Log Reader"),                                 /**< name           */
	VERSION,                                          /**< version        */

	/** summary */
	N_("Includes other IM clients' logs in the "
	   "log viewer."),

	/** description */
	N_("When viewing logs, this plugin will include "
	   "logs from other IM clients. Currently, this "
	   "includes Adium, MSN Messenger, and Trillian.\n\n"
	   "WARNING: This plugin is still alpha code and "
	   "may crash frequently.  Use it at your own risk!"),

	"Richard Laager <rlaager@pidgin.im>",             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */
	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */
	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	NULL                                              /**< actions        */
};

GAIM_INIT_PLUGIN(log_reader, init_plugin, info)
