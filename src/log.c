/**
 * @file log.c Logging API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Buzz Lightyear
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

#include "account.h"
#include "debug.h"
#include "internal.h"
#include "log.h"
#include "prefs.h"
#include "util.h"
#include "stringref.h"

static GaimLogLogger html_logger;
static GaimLogLogger txt_logger;
static GaimLogLogger old_logger;

/**************************************************************************
 * PUBLIC LOGGING FUNCTIONS ***********************************************
 **************************************************************************/

GaimLog *gaim_log_new(GaimLogType type, const char *name, GaimAccount *account, time_t time)
{
	GaimLog *log = g_new0(GaimLog, 1);
	log->name = g_strdup(name);
	log->account = account;
	log->time = time;
	log->logger = gaim_log_logger_get();
	if (log->logger && log->logger->create)
		log->logger->create(log);
	return log;
}

void gaim_log_free(GaimLog *log)
{
	g_return_if_fail(log);
	if (log->logger && log->logger->finalize)
		log->logger->finalize(log);
	g_free(log->name);
	g_free(log);
}


void gaim_log_write(GaimLog *log, GaimMessageFlags type,
		    const char *from, time_t time, const char *message)
{
	g_return_if_fail(log);
	g_return_if_fail(log->logger);
	g_return_if_fail(log->logger->write);

	if ((log->type == GAIM_LOG_IM && gaim_prefs_get_bool("/core/logging/log_ims")) ||
	    (log->type == GAIM_LOG_CHAT && gaim_prefs_get_bool("/core/logging/log_chats")))
		(log->logger->write)(log, type, from, time, message);
}

char *gaim_log_read(GaimLog *log, GaimLogReadFlags *flags)
{
	GaimLogReadFlags mflags;
	g_return_val_if_fail(log && log->logger, NULL);
	if (log->logger->read) {
		char *ret = (log->logger->read)(log, flags ? flags : &mflags);
		gaim_str_strip_cr(ret);
		return ret;
	}
	return (_("<b><font color=\"red\">The logger has no read function</font></b>"));
}

int gaim_log_get_size(GaimLog *log)
{
	g_return_val_if_fail(log && log->logger, 0);
	if (log->logger->size)
		return log->logger->size(log);
	return 0;
}

int gaim_log_get_total_size(const char *name, GaimAccount *account)
{
	GList *logs = gaim_log_get_logs(name, account);
	int size = 0;


	while (logs) {
		GList *logs2 = logs->next;
		GaimLog *log = (GaimLog*)(logs->data);
		size += gaim_log_get_size(log);
		gaim_log_free(log);
		g_list_free_1(logs);
		logs = logs2;
	}

	return size;
}

/****************************************************************************
 * LOGGER FUNCTIONS *********************************************************
 ****************************************************************************/

static GaimLogLogger *current_logger = NULL;
static GSList *loggers = NULL;

static void logger_pref_cb(const char *name, GaimPrefType type,
			   gpointer value, gpointer data)
{
	GaimLogLogger *logger;
	GSList *l = loggers;
	while (l) {
		logger = l->data;
		if (!strcmp(logger->id, value)) {
			gaim_log_logger_set(logger);
			return;
		}
		l = l->next;
	}
	gaim_log_logger_set(&txt_logger);
}


GaimLogLogger *gaim_log_logger_new(void(*create)(GaimLog *),
				   void(*write)(GaimLog *, GaimMessageFlags, const char *,
						time_t, const char *),
				   void(*finalize)(GaimLog *), GList*(*list)(const char*, GaimAccount*),
				   char*(*read)(GaimLog*, GaimLogReadFlags*),
				   int(*size)(GaimLog*))
{
	GaimLogLogger *logger = g_new0(GaimLogLogger, 1);
	logger->create = create;
	logger->write = write;
	logger->finalize = finalize;
	logger->list = list;
	logger->read = read;
	logger->size = size;
	return logger;
}

void gaim_log_logger_free(GaimLogLogger *logger)
{
	g_free(logger);
}

void gaim_log_logger_add (GaimLogLogger *logger)
{
	g_return_if_fail(logger);
	if (g_slist_find(loggers, logger))
		return;
	loggers = g_slist_append(loggers, logger);
}

void gaim_log_logger_remove (GaimLogLogger *logger)
{
	g_return_if_fail(logger);
	g_slist_remove(loggers, logger);
}

void gaim_log_logger_set (GaimLogLogger *logger)
{
	g_return_if_fail(logger);
	current_logger = logger;
}

GaimLogLogger *gaim_log_logger_get()
{
	return current_logger;
}

GList *gaim_log_logger_get_options(void)
{
	GSList *n;
	GList *list = NULL;
	GaimLogLogger *data;

	for (n = loggers; n; n = n->next) {
		data = n->data;
		if (!data->write)
			continue;
		list = g_list_append(list, _(data->name));
		list = g_list_append(list, data->id);
	}

	return list;
}

static gint log_compare(gconstpointer y, gconstpointer z)
{
	const GaimLog *a = y;
	const GaimLog *b = z;

	return b->time - a->time;
}

GList *gaim_log_get_logs(const char *name, GaimAccount *account)
{
	GList *logs = NULL;
	GSList *n;
	for (n = loggers; n; n = n->next) {
		GaimLogLogger *logger = n->data;
		if (!logger->list)
			continue;
		logs = g_list_concat(logs, logger->list(name, account));
	}

	return g_list_sort(logs, log_compare);
}

void gaim_log_init(void)
{
	gaim_prefs_add_none("/core/logging");
	gaim_prefs_add_bool("/core/logging/log_ims", FALSE);
	gaim_prefs_add_bool("/core/logging/log_chats", FALSE);
	gaim_prefs_add_string("/core/logging/format", "txt");
	gaim_log_logger_add(&html_logger);
	gaim_log_logger_add(&txt_logger);
	gaim_log_logger_add(&old_logger);
	gaim_prefs_connect_callback("/core/logging/format",
				    logger_pref_cb, NULL);
	gaim_prefs_trigger_callback("/core/logging/format");
}

/****************************************************************************
 * LOGGERS ******************************************************************
 ****************************************************************************/

struct generic_logger_data {
	char *path;
	FILE *file;
};

static GList *log_lister_common(const char *screenname, GaimAccount *account, const char *ext, GaimLogLogger *logger)
{
	GDir *dir;
	GList *list = NULL;
	const char *filename;
	char *me = g_strdup(gaim_normalize(account, gaim_account_get_username(account)));

	/* does this seem like a bad way to get this component of the path to anyone else? --Nathan */
	const char *prpl = GAIM_PLUGIN_PROTOCOL_INFO
		(gaim_find_prpl(gaim_account_get_protocol_id(account)))->list_icon(account, NULL);
	char *path = g_build_filename(gaim_user_dir(), "logs", prpl, me, gaim_normalize(account, screenname), NULL);

	g_free(me);

	if (!(dir = g_dir_open(path, 0, NULL))) {
		g_free(path);
		return NULL;
	}
	while ((filename = g_dir_read_name(dir))) {
		if (gaim_str_has_suffix(filename, ext)) {
			const char *l = filename;
			struct tm time;
			GaimLog *log;
			struct generic_logger_data *data;
			char d[5];

			strncpy(d, l, 4);
			d[4] = '\0';
			time.tm_year = atoi(d) - 1900;
			l = l + 5;

			strncpy(d, l, 2);
			d[2] = '\0';
			time.tm_mon = atoi(d) - 1;
			l = l + 3;

			strncpy(d, l, 2);
			time.tm_mday = atoi(d);
			l = l + 3;

			strncpy(d, l, 2);
			time.tm_hour = atoi(d);
			l = l + 2;

			strncpy(d, l, 2);
			time.tm_min = atoi(d);
			l = l + 2;

			strncpy(d, l, 2);
			time.tm_sec = atoi(d);
			l = l + 2;
			log = gaim_log_new(GAIM_LOG_IM, screenname, account, mktime(&time));
			log->logger = logger;
			log->logger_data = data = g_new0(struct generic_logger_data, 1);
			data->path = g_build_filename(path, filename, NULL);
			list = g_list_append(list, log);
		}
	}
	g_dir_close(dir);
	g_free(path);
	return list;
}

/* Only to be used with logs listed from log_lister_common */
int log_sizer_common(GaimLog *log)
{
	struct stat st;
	struct generic_logger_data *data = log->logger_data;

	if (!data->path || stat(data->path, &st))
		st.st_size = 0;

	return st.st_size;
}

#if 0 /* Maybe some other time. */
/****************
 ** XML LOGGER **
 ****************/

static const char *str_from_msg_type (GaimMessageFlags type)
{

		return "";

}

static void xml_logger_write(GaimLog *log,
			     GaimMessageFlags type,
			     const char *from, time_t time, const char *message)
{
	char date[64];
	char *xhtml = NULL;
	if (!log->logger_data) {
		/* This log is new.  We could use the loggers 'new' function, but
		 * creating a new file there would result in empty files in the case
		 * that you open a convo with someone, but don't say anything.
		 */
		char *ud = gaim_user_dir();
		char *guy = g_strdup(gaim_normalize(log->account, gaim_account_get_username(log->account)));
		const char *prpl = GAIM_PLUGIN_PROTOCOL_INFO
			(gaim_find_prpl(gaim_account_get_protocol(log->account)))->list_icon(log->account, NULL);
		char *dir;
		FILE *file;

		strftime(date, sizeof(date), "%Y-%m-%d.%H%M%S.xml", localtime(&log->time));

		dir = g_build_filename(ud, "logs",
				       prpl, guy, gaim_normalize(log->account, log->name), NULL);
		gaim_build_dir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		g_free(guy);

		char *filename = g_build_filename(dir, date, NULL);
		g_free(dir);

		log->logger_data = fopen(filename, "a");
		if (!log->logger_data) {
			gaim_debug(GAIM_DEBUG_ERROR, "log", "Could not create log file %s\n", filename);
			g_free(filename);
			return;
		}
		g_free(filename);
		fprintf(log->logger_data, "<?xml version='1.0' encoding='UTF-8' ?>\n"
			"<?xml-stylesheet href='file:///usr/src/web/htdocs/log-stylesheet.xsl' type='text/xml' ?>\n");

		strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&log->time));
		fprintf(log->logger_data, "<conversation time='%s' screenname='%s' protocol='%s'>\n",
			date, log->name, prpl);
	}

	strftime(date, sizeof(date), "%H:%M:%S", localtime(&time));
	gaim_markup_html_to_xhtml(message, &xhtml, NULL);
	if (from)
		fprintf(log->logger_data, "<message %s %s from='%s' time='%s'>%s</message>\n",
			str_from_msg_type(type),
			type & GAIM_MESSAGE_SEND ? "direction='sent'" :
			type & GAIM_MESSAGE_RECV ? "direction='received'" : "",
			from, date, xhtml);
	else
		fprintf(log->logger_data, "<message %s %s time='%s'>%s</message>\n",
			str_from_msg_type(type),
			type & GAIM_MESSAGE_SEND ? "direction='sent'" :
			type & GAIM_MESSAGE_RECV ? "direction='received'" : "",
			date, xhtml):
	fflush(log->logger_data);
	g_free(xhtml);
}

 static void xml_logger_finalize(GaimLog *log)
{
	if (log->logger_data) {
		fprintf(log->logger_data, "</conversation>\n");
		fclose(log->logger_data);
		log->logger_data = NULL;
	}
}

static GList *xml_logger_list(const char *sn, GaimAccount *account)
{
	return log_lister_common(sn, account, ".xml", &xml_logger);
}

static GaimLogLogger xml_logger =  {
	N_("XML"), "xml",
	NULL,
	xml_logger_write,
	xml_logger_finalize,
	xml_logger_list,
	NULL
};
#endif

/****************************
 ** HTML LOGGER *************
 ****************************/

static void html_logger_write(GaimLog *log, GaimMessageFlags type,
		const char *from, time_t time, const char *message)
{
	GaimConnection *gc = gaim_account_get_connection(log->account);
	char date[64];
	char *msg_fixed;
	struct generic_logger_data *data = log->logger_data;
	if(!data) {
		/* This log is new */
		char *ud = gaim_user_dir();
		char *guy = g_strdup(gaim_normalize(log->account, gaim_account_get_username(log->account)));
		char *chat;
		const char *prpl = GAIM_PLUGIN_PROTOCOL_INFO
			(gaim_find_prpl(gaim_account_get_protocol_id(log->account)))->list_icon(log->account, NULL);
		char *dir;
		char *filename;

		if (log->type == GAIM_LOG_CHAT) {
			chat = g_strdup_printf("%s.chat", guy);
			g_free(guy);
			guy = chat;
		}

		strftime(date, sizeof(date), "%Y-%m-%d.%H%M%S.html", localtime(&log->time));

		dir = g_build_filename(ud, "logs",
				       prpl, guy, gaim_normalize(log->account, log->name), NULL);
		gaim_build_dir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		g_free(guy);

		filename = g_build_filename(dir, date, NULL);
		g_free(dir);

		log->logger_data = data = g_new0(struct generic_logger_data, 1);

		data->file = fopen(filename, "a");
		if (!data->file) {
			gaim_debug(GAIM_DEBUG_ERROR, "log",
					"Could not create log file %s\n", filename);
			g_free(filename);
			return;
		}
		g_free(filename);
		strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&log->time));
		fprintf(data->file, "<html><head><title>");
		fprintf(data->file, "Conversation with %s at %s on %s (%s)",
			log->name, date, gaim_account_get_username(log->account), prpl);
		fprintf(data->file, "</title></head><body>");
		fprintf(data->file,
			"<h3>Conversation with %s at %s on %s (%s)</h3>\n",
			log->name, date, gaim_account_get_username(log->account), prpl);
	}

	/* if we can't write to the file, give up before we hurt ourselves */
	if(!data->file)
		return;

	gaim_markup_html_to_xhtml(message, &msg_fixed, NULL);

	strftime(date, sizeof(date), "%H:%M:%S", localtime(&time));
	if (type & GAIM_MESSAGE_SYSTEM)
		fprintf(data->file, "(%s)<b> %s</b><br/>\n", date, msg_fixed);
	else if (type & GAIM_MESSAGE_WHISPER)
		fprintf(data->file, "<font color=\"#6C2585\">(%s)<b> %s:</b></font> %s<br/>\n",
			date, from, msg_fixed);
	else if (type & GAIM_MESSAGE_AUTO_RESP) {
		if (type & GAIM_MESSAGE_SEND)
			fprintf(data->file, _("<font color=\"#16569E\">(%s) <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>\n"), date, from, msg_fixed);
		else if (type & GAIM_MESSAGE_RECV)
			fprintf(data->file, _("<font color=\"#A82F2F\">(%s) <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>\n"), date, from, msg_fixed);
	} else if (type & GAIM_MESSAGE_RECV) {
		if(gaim_message_meify(msg_fixed, -1))
			fprintf(data->file, "<font color=\"#6C2585\">(%s) <b>***%s</b></font> <font sml=\"%s\">%s</font><br/>\n",
					date, from, gc->prpl->info->name, msg_fixed);
		else
			fprintf(data->file, "<font color=\"#A82F2F\">(%s) <b>%s:</b></font> <font sml=\"%s\">%s</font><br/>\n",
					date, from, gc->prpl->info->name, msg_fixed);
	} else if (type & GAIM_MESSAGE_SEND) {
		if(gaim_message_meify(msg_fixed, -1))
			fprintf(data->file, "<font color=\"#6C2585\">(%s) <b>***%s</b></font> <font sml=\"%s\">%s</font><br/>\n",
					date, from, gc->prpl->info->name, msg_fixed);
		else
			fprintf(data->file, "<font color=\"#16569E\">(%s) <b>%s:</b></font> <font sml=\"%s\">%s</font><br/>\n",
					date, from, gc->prpl->info->name, msg_fixed);
	}
	g_free(msg_fixed);
	fflush(data->file);
}

static void html_logger_finalize(GaimLog *log)
{
	struct generic_logger_data *data = log->logger_data;
	if (data) {
		if(data->file) {
			fprintf(data->file, "</body></html>");
			fclose(data->file);
		}
		g_free(data->path);
		g_free(data);
	}
}

static GList *html_logger_list(const char *sn, GaimAccount *account)
{
	return log_lister_common(sn, account, ".html", &html_logger);
}

static char *html_logger_read(GaimLog *log, GaimLogReadFlags *flags)
{
	char *read, *minus_header;
	struct generic_logger_data *data = log->logger_data;
	*flags = GAIM_LOG_READ_NO_NEWLINE;
	if (!data || !data->path)
		return g_strdup(_("<font color=\"red\"><b>Unable to find log path!</b></font>"));
	if (g_file_get_contents(data->path, &read, NULL, NULL)) {
		minus_header = strchr(read, '\n');
		if (!minus_header)
			minus_header = g_strdup(read);
		else
			minus_header = g_strdup(minus_header + 1);
		g_free(read);
		return minus_header;
	}
	return g_strdup(_("<font color=\"red\"><b>Could not read file: %s</b></font>"));
}

static GaimLogLogger html_logger = {
	N_("HTML"), "html",
	NULL,
	html_logger_write,
	html_logger_finalize,
	html_logger_list,
	html_logger_read,
	log_sizer_common
};




/****************************
 ** PLAIN TEXT LOGGER *******
 ****************************/

static void txt_logger_write(GaimLog *log,
			     GaimMessageFlags type,
			     const char *from, time_t time, const char *message)
{
	char date[64];
	char *stripped = NULL;
	struct generic_logger_data *data = log->logger_data;
	if (!data) {
		/* This log is new.  We could use the loggers 'new' function, but
		 * creating a new file there would result in empty files in the case
		 * that you open a convo with someone, but don't say anything.
		 */
		char *ud = gaim_user_dir();
		char *filename;
		char *guy = g_strdup(gaim_normalize(log->account, gaim_account_get_username(log->account)));
		char *chat;
		const char *prpl = GAIM_PLUGIN_PROTOCOL_INFO
			(gaim_find_prpl(gaim_account_get_protocol_id(log->account)))->list_icon(log->account, NULL);
		char *dir;

		if (log->type == GAIM_LOG_CHAT) {
			chat = g_strdup_printf("%s.chat", guy);
			g_free(guy);
			guy = chat;
		}
		strftime(date, sizeof(date), "%Y-%m-%d.%H%M%S.txt", localtime(&log->time));

		dir = g_build_filename(ud, "logs",
				       prpl, guy, gaim_normalize(log->account, log->name), NULL);
		gaim_build_dir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		g_free(guy);

		filename = g_build_filename(dir, date, NULL);
		g_free(dir);

		log->logger_data = data = g_new0(struct generic_logger_data, 1);

		data->file = fopen(filename, "a");
		if (!data->file) {
			gaim_debug(GAIM_DEBUG_ERROR, "log", "Could not create log file %s\n", filename);
			g_free(filename);
			return;
		}
		g_free(filename);
		strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&log->time));
		fprintf(data->file, "Conversation with %s at %s on %s (%s)\n",
			log->name, date, gaim_account_get_username(log->account), prpl);
	}

	/* if we can't write to the file, give up before we hurt ourselves */
	if(!data->file)
		return;

	strftime(date, sizeof(date), "%H:%M:%S", localtime(&time));
	stripped = gaim_markup_strip_html(message);
	if (type & GAIM_MESSAGE_SEND ||
	    type & GAIM_MESSAGE_RECV) {
		if (type & GAIM_MESSAGE_AUTO_RESP) {
			fprintf(data->file, _("(%s) %s <AUTO-REPLY>: %s\n"), date, from, stripped);
		} else {
			if(gaim_message_meify(stripped, -1))
				fprintf(data->file, "(%s) ***%s %s\n", date, from,
						stripped);
			else
				fprintf(data->file, "(%s) %s: %s\n", date, from,
						stripped);
		}
	} else if (type & GAIM_MESSAGE_SYSTEM)
		fprintf(data->file, "(%s) %s\n", date, stripped);
	else if (type & GAIM_MESSAGE_NO_LOG) {
		/* This shouldn't happen */
		g_free(stripped);
		return;
	} else if (type & GAIM_MESSAGE_WHISPER)
		fprintf(data->file, "(%s) *%s* %s", date, from, stripped);
	else
		fprintf(data->file, "(%s) %s%s %s\n", date, from ? from : "", from ? ":" : "", stripped);

	fflush(data->file);
	g_free(stripped);
}

static void txt_logger_finalize(GaimLog *log)
{
	struct generic_logger_data *data = log->logger_data;
	if (data) {
		if(data->file)
			fclose(data->file);
		if(data->path)
			g_free(data->path);
		g_free(data);
	}
}

static GList *txt_logger_list(const char *sn, GaimAccount *account)
{
	return log_lister_common(sn, account, ".txt", &txt_logger);
}

static char *txt_logger_read(GaimLog *log, GaimLogReadFlags *flags)
{
	char *read, *minus_header;
	struct generic_logger_data *data = log->logger_data;
	*flags = 0;
	if (!data || !data->path)
		return g_strdup(_("<font color=\"red\"><b>Unable to find log path!</b></font>"));
	if (g_file_get_contents(data->path, &read, NULL, NULL)) {
		minus_header = strchr(read, '\n');
		if (!minus_header)
			minus_header = g_strdup(read);
		else
			minus_header = g_strdup(minus_header + 1);
		g_free(read);
		return minus_header;
	}
        return g_strdup(_("<font color=\"red\"><b>Could not read file: %s</b></font>"));
}

static GaimLogLogger txt_logger = {
	N_("Plain text"), "txt",
	NULL,
	txt_logger_write,
	txt_logger_finalize,
	txt_logger_list,
	txt_logger_read,
	log_sizer_common
};

/****************
 * OLD LOGGER ***
 ****************/

/* The old logger doesn't write logs, only reads them.  This is to include
 * old logs in the log viewer transparently.
 */

struct old_logger_data {
	GaimStringref *pathref;
	int offset;
	int length;
};

static GList *old_logger_list(const char *sn, GaimAccount *account)
{
	FILE *file;
	char buf[BUF_LONG];
	struct tm tm;
	char month[4];
	struct old_logger_data *data = NULL;
	char *logfile = g_strdup_printf("%s.log", gaim_normalize(account, sn));
	char *pathstr = g_build_filename(gaim_user_dir(), "logs", logfile, NULL);
	GaimStringref *pathref = gaim_stringref_new(pathstr);
	char *newlog;
	int logfound = 0;
	int lastoff = 0;
	int newlen;
	time_t lasttime = 0;

	GaimLog *log = NULL;
	GList *list = NULL;

	g_free(logfile);
	g_free(pathstr);

	if (!(file = fopen(gaim_stringref_value(pathref), "rb"))) {
		gaim_stringref_unref(pathref);
		return NULL;
	}

	while (fgets(buf, BUF_LONG, file)) {
		if ((newlog = strstr(buf, "---- New C"))) {
			int length;
			int offset;
			char convostart[32];
			char *temp = strchr(buf, '@');

			if (temp == NULL || strlen(temp) < 2)
				continue;

			temp++;
			length = strcspn(temp, "-");
			if (length > 31) length = 31;

			offset = ftell(file);

			if (logfound) {
				newlen = offset - lastoff - length;
				if(strstr(buf, "----</H3><BR>")) {
					newlen -=
						sizeof("<HR><BR><H3 Align=Center> ---- New Conversation @ ") +
						sizeof("----</H3><BR>") - 2;
				} else {
					newlen -=
						sizeof("---- New Conversation @ ") + sizeof("----") - 2;
				}

				if(strchr(buf, '\r'))
					newlen--;

				if (newlen != 0) {
					log = gaim_log_new(GAIM_LOG_IM, sn, account, -1);
					log->logger = &old_logger;
					log->time = lasttime;
					data = g_new0(struct old_logger_data, 1);
					data->offset = lastoff;
					data->length = newlen;
					data->pathref = gaim_stringref_ref(pathref);
					log->logger_data = data;
					list = g_list_append(list, log);
				}
			}

			logfound = 1;
			lastoff = offset;

			g_snprintf(convostart, length, "%s", temp);
			sscanf(convostart, "%*s %s %d %d:%d:%d %d",
			       month, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tm.tm_year);
			/* Ugly hack, in case current locale is not English */
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
			tm.tm_year -= 1900;
			lasttime = mktime(&tm);
		}
	}

	if (logfound) {
		if ((newlen = ftell(file) - lastoff) != 0) {
			log = gaim_log_new(GAIM_LOG_IM, sn, account, -1);
			log->logger = &old_logger;
			log->time = lasttime;
			data = g_new0(struct old_logger_data, 1);
			data->offset = lastoff;
			data->length = newlen;
			data->pathref = gaim_stringref_ref(pathref);
			log->logger_data = data;
			list = g_list_append(list, log);
		}
	}

	gaim_stringref_unref(pathref);
	fclose(file);
	return list;
}

static char * old_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct old_logger_data *data = log->logger_data;
	FILE *file = fopen(gaim_stringref_value(data->pathref), "rb");
	char *read = g_malloc(data->length + 1);
	fseek(file, data->offset, SEEK_SET);
	fread(read, data->length, 1, file);
	read[data->length] = '\0';
	*flags = 0;
	if(strstr(read, "<BR>"))
		*flags |= GAIM_LOG_READ_NO_NEWLINE;
	return read;
}

static int old_logger_size (GaimLog *log)
{
	struct old_logger_data *data = log->logger_data;
	return data ? data->length : 0;
}

static void old_logger_finalize(GaimLog *log)
{
	struct old_logger_data *data = log->logger_data;
	gaim_stringref_unref(data->pathref);
	g_free(data);
}

static GaimLogLogger old_logger = {
	"old logger", "old",
	NULL, NULL,
	old_logger_finalize,
	old_logger_list,
	old_logger_read,
	old_logger_size
};
