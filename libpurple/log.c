/**
 * @file log.c Logging API
 * @ingroup core
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

#include "internal.h"
#include "account.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "internal.h"
#include "log.h"
#include "prefs.h"
#include "util.h"
#include "stringref.h"

static GSList *loggers = NULL;

static GaimLogLogger *html_logger;
static GaimLogLogger *txt_logger;
static GaimLogLogger *old_logger;

struct _gaim_logsize_user {
	char *name;
	GaimAccount *account;
};
static GHashTable *logsize_users = NULL;

static void log_get_log_sets_common(GHashTable *sets);

static gsize html_logger_write(GaimLog *log, GaimMessageFlags type,
							  const char *from, time_t time, const char *message);
static void html_logger_finalize(GaimLog *log);
static GList *html_logger_list(GaimLogType type, const char *sn, GaimAccount *account);
static GList *html_logger_list_syslog(GaimAccount *account);
static char *html_logger_read(GaimLog *log, GaimLogReadFlags *flags);
static int html_logger_total_size(GaimLogType type, const char *name, GaimAccount *account);

static GList *old_logger_list(GaimLogType type, const char *sn, GaimAccount *account);
static int old_logger_total_size(GaimLogType type, const char *name, GaimAccount *account);
static char * old_logger_read (GaimLog *log, GaimLogReadFlags *flags);
static int old_logger_size (GaimLog *log);
static void old_logger_get_log_sets(GaimLogSetCallback cb, GHashTable *sets);
static void old_logger_finalize(GaimLog *log);

static gsize txt_logger_write(GaimLog *log,
							 GaimMessageFlags type,
							 const char *from, time_t time, const char *message);
static void txt_logger_finalize(GaimLog *log);
static GList *txt_logger_list(GaimLogType type, const char *sn, GaimAccount *account);
static GList *txt_logger_list_syslog(GaimAccount *account);
static char *txt_logger_read(GaimLog *log, GaimLogReadFlags *flags);
static int txt_logger_total_size(GaimLogType type, const char *name, GaimAccount *account);

/**************************************************************************
 * PUBLIC LOGGING FUNCTIONS ***********************************************
 **************************************************************************/

GaimLog *gaim_log_new(GaimLogType type, const char *name, GaimAccount *account,
                      GaimConversation *conv, time_t time, const struct tm *tm)
{
	GaimLog *log;

	/* IMPORTANT: Make sure to initialize all the members of GaimLog */
	log = g_slice_new(GaimLog);
	GAIM_DBUS_REGISTER_POINTER(log, GaimLog);

	log->type = type;
	log->name = g_strdup(gaim_normalize(account, name));
	log->account = account;
	log->conv = conv;
	log->time = time;
	log->logger = gaim_log_logger_get();
	log->logger_data = NULL;

	if (tm == NULL)
		log->tm = NULL;
	else
	{
		/* There's no need to zero this as we immediately do a direct copy. */
		log->tm = g_slice_new(struct tm);

		*(log->tm) = *tm;

#ifdef HAVE_STRUCT_TM_TM_ZONE
		/* XXX: This is so wrong... */
		if (log->tm->tm_zone != NULL)
		{
			char *tmp = g_locale_from_utf8(log->tm->tm_zone, -1, NULL, NULL, NULL);
			if (tmp != NULL)
				log->tm->tm_zone = tmp;
			else
				/* Just shove the UTF-8 bytes in and hope... */
				log->tm->tm_zone = g_strdup(log->tm->tm_zone);
		}
#endif
	}

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

	if (log->tm != NULL)
	{
#ifdef HAVE_STRUCT_TM_TM_ZONE
		/* XXX: This is so wrong... */
		g_free((char *)log->tm->tm_zone);
#endif
		g_slice_free(struct tm, log->tm);
	}

	GAIM_DBUS_UNREGISTER_POINTER(log);
	g_slice_free(GaimLog, log);
}

void gaim_log_write(GaimLog *log, GaimMessageFlags type,
		    const char *from, time_t time, const char *message)
{
	struct _gaim_logsize_user *lu;
	gsize written, total = 0;
	gpointer ptrsize;

	g_return_if_fail(log);
	g_return_if_fail(log->logger);
	g_return_if_fail(log->logger->write);

	written = (log->logger->write)(log, type, from, time, message);

	lu = g_new(struct _gaim_logsize_user, 1);

	lu->name = g_strdup(gaim_normalize(log->account, log->name));
	lu->account = log->account;

	if(g_hash_table_lookup_extended(logsize_users, lu, NULL, &ptrsize)) {
		total = GPOINTER_TO_INT(ptrsize);
		total += written;
		g_hash_table_replace(logsize_users, lu, GINT_TO_POINTER(total));
	} else {
		g_free(lu->name);
		g_free(lu);
	}

}

char *gaim_log_read(GaimLog *log, GaimLogReadFlags *flags)
{
	GaimLogReadFlags mflags;
	g_return_val_if_fail(log && log->logger, NULL);
	if (log->logger->read) {
		char *ret = (log->logger->read)(log, flags ? flags : &mflags);
		gaim_str_strip_char(ret, '\r');
		return ret;
	}
	return g_strdup(_("<b><font color=\"red\">The logger has no read function</font></b>"));
}

int gaim_log_get_size(GaimLog *log)
{
	g_return_val_if_fail(log && log->logger, 0);

	if (log->logger->size)
		return log->logger->size(log);
	return 0;
}

static guint _gaim_logsize_user_hash(struct _gaim_logsize_user *lu)
{
	return g_str_hash(lu->name);
}

static guint _gaim_logsize_user_equal(struct _gaim_logsize_user *lu1,
		struct _gaim_logsize_user *lu2)
{
	return (lu1->account == lu2->account && (!strcmp(lu1->name, lu2->name)));
}

static void _gaim_logsize_user_free_key(struct _gaim_logsize_user *lu)
{
	g_free(lu->name);
	g_free(lu);
}

int gaim_log_get_total_size(GaimLogType type, const char *name, GaimAccount *account)
{
	gpointer ptrsize;
	int size = 0;
	GSList *n;
	struct _gaim_logsize_user *lu;

	lu = g_new(struct _gaim_logsize_user, 1);
	lu->name = g_strdup(gaim_normalize(account, name));
	lu->account = account;

	if(g_hash_table_lookup_extended(logsize_users, lu, NULL, &ptrsize)) {
		size = GPOINTER_TO_INT(ptrsize);
		g_free(lu->name);
		g_free(lu);
	} else {
		for (n = loggers; n; n = n->next) {
			GaimLogLogger *logger = n->data;

			if(logger->total_size){
				size += (logger->total_size)(type, name, account);
			} else if(logger->list) {
				GList *logs = (logger->list)(type, name, account);
				int this_size = 0;

				while (logs) {
					GaimLog *log = (GaimLog*)(logs->data);
					this_size += gaim_log_get_size(log);
					gaim_log_free(log);
					logs = g_list_delete_link(logs, logs);
				}

				size += this_size;
			}
		}

		g_hash_table_replace(logsize_users, lu, GINT_TO_POINTER(size));
	}
	return size;
}

gboolean gaim_log_is_deletable(GaimLog *log)
{
	g_return_val_if_fail(log != NULL, FALSE);
	g_return_val_if_fail(log->logger != NULL, FALSE);

	if (log->logger->delete == NULL)
		return FALSE;

	if (log->logger->is_deletable != NULL)
		return log->logger->is_deletable(log);

	return TRUE;
}

gboolean gaim_log_delete(GaimLog *log)
{
	g_return_val_if_fail(log != NULL, FALSE);
	g_return_val_if_fail(log->logger != NULL, FALSE);

	if (log->logger->delete != NULL)
		return log->logger->delete(log);

	return FALSE;
}

char *
gaim_log_get_log_dir(GaimLogType type, const char *name, GaimAccount *account)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	const char *prpl_name;
	char *acct_name;
	const char *target;
	char *dir;

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	if (!prpl)
		return NULL;
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	prpl_name = prpl_info->list_icon(account, NULL);

	acct_name = g_strdup(gaim_escape_filename(gaim_normalize(account,
				gaim_account_get_username(account))));

	if (type == GAIM_LOG_CHAT) {
		char *temp = g_strdup_printf("%s.chat", gaim_normalize(account, name));
		target = gaim_escape_filename(temp);
		g_free(temp);
	} else if(type == GAIM_LOG_SYSTEM) {
		target = ".system";
	} else {
		target = gaim_escape_filename(gaim_normalize(account, name));
	}

	dir = g_build_filename(gaim_user_dir(), "logs", prpl_name, acct_name, target, NULL);

	g_free(acct_name);

	return dir;
}

/****************************************************************************
 * LOGGER FUNCTIONS *********************************************************
 ****************************************************************************/

static GaimLogLogger *current_logger = NULL;

static void logger_pref_cb(const char *name, GaimPrefType type,
						   gconstpointer value, gpointer data)
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
	gaim_log_logger_set(txt_logger);
}


GaimLogLogger *gaim_log_logger_new(const char *id, const char *name, int functions, ...)
{
#if 0
				void(*create)(GaimLog *),
				gsize(*write)(GaimLog *, GaimMessageFlags, const char *, time_t, const char *),
				void(*finalize)(GaimLog *),
				GList*(*list)(GaimLogType type, const char*, GaimAccount*),
				char*(*read)(GaimLog*, GaimLogReadFlags*),
				int(*size)(GaimLog*),
				int(*total_size)(GaimLogType type, const char *name, GaimAccount *account),
				GList*(*list_syslog)(GaimAccount *account),
				void(*get_log_sets)(GaimLogSetCallback cb, GHashTable *sets),
				gboolean(*delete)(GaimLog *log),
				gboolean(*is_deletable)(GaimLog *log))
{
#endif
	GaimLogLogger *logger;
	va_list args;

	g_return_val_if_fail(id != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(functions >= 1, NULL);

	logger = g_new0(GaimLogLogger, 1);
	logger->id = g_strdup(id);
	logger->name = g_strdup(name);

	va_start(args, functions);

	if (functions >= 1)
		logger->create = va_arg(args, void *);
	if (functions >= 2)
		logger->write = va_arg(args, void *);
	if (functions >= 3)
		logger->finalize = va_arg(args, void *);
	if (functions >= 4)
		logger->list = va_arg(args, void *);
	if (functions >= 5)
		logger->read = va_arg(args, void *);
	if (functions >= 6)
		logger->size = va_arg(args, void *);
	if (functions >= 7)
		logger->total_size = va_arg(args, void *);
	if (functions >= 8)
		logger->list_syslog = va_arg(args, void *);
	if (functions >= 9)
		logger->get_log_sets = va_arg(args, void *);
	if (functions >= 10)
		logger->delete = va_arg(args, void *);
	if (functions >= 11)
		logger->is_deletable = va_arg(args, void *);

	if (functions >= 12)
		gaim_debug_info("log", "Dropping new functions for logger: %s (%s)\n", name, id);

	va_end(args);

	return logger;
}

void gaim_log_logger_free(GaimLogLogger *logger)
{
	g_free(logger->name);
	g_free(logger->id);
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
	loggers = g_slist_remove(loggers, logger);
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
		list = g_list_append(list, data->name);
		list = g_list_append(list, data->id);
	}

	return list;
}

gint gaim_log_compare(gconstpointer y, gconstpointer z)
{
	const GaimLog *a = y;
	const GaimLog *b = z;

	return b->time - a->time;
}

GList *gaim_log_get_logs(GaimLogType type, const char *name, GaimAccount *account)
{
	GList *logs = NULL;
	GSList *n;
	for (n = loggers; n; n = n->next) {
		GaimLogLogger *logger = n->data;
		if (!logger->list)
			continue;
		logs = g_list_concat(logger->list(type, name, account), logs);
	}

	return g_list_sort(logs, gaim_log_compare);
}

gint gaim_log_set_compare(gconstpointer y, gconstpointer z)
{
	const GaimLogSet *a = y;
	const GaimLogSet *b = z;
	gint ret = 0;

	/* This logic seems weird at first...
	 * If either account is NULL, we pretend the accounts are
	 * equal. This allows us to detect duplicates that will
	 * exist if one logger knows the account and another
	 * doesn't. */
	if (a->account != NULL && b->account != NULL) {
		ret = strcmp(gaim_account_get_username(a->account), gaim_account_get_username(b->account));
		if (ret != 0)
			return ret;
	}

	ret = strcmp(a->normalized_name, b->normalized_name);
	if (ret != 0)
		return ret;

	return (gint)b->type - (gint)a->type;
}

static guint
log_set_hash(gconstpointer key)
{
	const GaimLogSet *set = key;

	/* The account isn't hashed because we need GaimLogSets with NULL accounts
	 * to be found when we search by a GaimLogSet that has a non-NULL account
	 * but the same type and name. */
	return g_int_hash(&set->type) + g_str_hash(set->name);
}

static gboolean
log_set_equal(gconstpointer a, gconstpointer b)
{
	/* I realize that the choices made for GList and GHashTable
	 * make sense for those data types, but I wish the comparison
	 * functions were compatible. */
	return !gaim_log_set_compare(a, b);
}

static void
log_add_log_set_to_hash(GHashTable *sets, GaimLogSet *set)
{
	GaimLogSet *existing_set = g_hash_table_lookup(sets, set);

	if (existing_set == NULL)
		g_hash_table_insert(sets, set, set);
	else if (existing_set->account == NULL && set->account != NULL)
		g_hash_table_replace(sets, set, set);
	else
		gaim_log_set_free(set);
}

GHashTable *gaim_log_get_log_sets(void)
{
	GSList *n;
	GHashTable *sets = g_hash_table_new_full(log_set_hash, log_set_equal,
											 (GDestroyNotify)gaim_log_set_free, NULL);

	/* Get the log sets from all the loggers. */
	for (n = loggers; n; n = n->next) {
		GaimLogLogger *logger = n->data;

		if (!logger->get_log_sets)
			continue;

		logger->get_log_sets(log_add_log_set_to_hash, sets);
	}

	log_get_log_sets_common(sets);

	/* Return the GHashTable of unique GaimLogSets. */
	return sets;
}

void gaim_log_set_free(GaimLogSet *set)
{
	g_return_if_fail(set != NULL);

	g_free(set->name);
	if (set->normalized_name != set->name)
		g_free(set->normalized_name);

	g_slice_free(GaimLogSet, set);
}

GList *gaim_log_get_system_logs(GaimAccount *account)
{
	GList *logs = NULL;
	GSList *n;
	for (n = loggers; n; n = n->next) {
		GaimLogLogger *logger = n->data;
		if (!logger->list_syslog)
			continue;
		logs = g_list_concat(logger->list_syslog(account), logs);
	}

	return g_list_sort(logs, gaim_log_compare);
}

/****************************************************************************
 * LOG SUBSYSTEM ************************************************************
 ****************************************************************************/

void *
gaim_log_get_handle(void)
{
	static int handle;

	return &handle;
}

void gaim_log_init(void)
{
	void *handle = gaim_log_get_handle();

	gaim_prefs_add_none("/core/logging");
	gaim_prefs_add_bool("/core/logging/log_ims", FALSE);
	gaim_prefs_add_bool("/core/logging/log_chats", FALSE);
	gaim_prefs_add_bool("/core/logging/log_system", FALSE);

	gaim_prefs_add_string("/core/logging/format", "txt");

	html_logger = gaim_log_logger_new("html", _("HTML"), 11,
									  NULL,
									  html_logger_write,
									  html_logger_finalize,
									  html_logger_list,
									  html_logger_read,
									  gaim_log_common_sizer,
									  html_logger_total_size,
									  html_logger_list_syslog,
									  NULL,
									  gaim_log_common_deleter,
									  gaim_log_common_is_deletable);
	gaim_log_logger_add(html_logger);

	txt_logger = gaim_log_logger_new("txt", _("Plain text"), 11,
									 NULL,
									 txt_logger_write,
									 txt_logger_finalize,
									 txt_logger_list,
									 txt_logger_read,
									 gaim_log_common_sizer,
									 txt_logger_total_size,
									 txt_logger_list_syslog,
									 NULL,
									 gaim_log_common_deleter,
									 gaim_log_common_is_deletable);
	gaim_log_logger_add(txt_logger);

	old_logger = gaim_log_logger_new("old", _("Old flat format"), 9,
									 NULL,
									 NULL,
									 old_logger_finalize,
									 old_logger_list,
									 old_logger_read,
									 old_logger_size,
									 old_logger_total_size,
									 NULL,
									 old_logger_get_log_sets);
	gaim_log_logger_add(old_logger);

	gaim_signal_register(handle, "log-timestamp",
#if SIZEOF_TIME_T == 4
	                     gaim_marshal_POINTER__POINTER_INT_BOOLEAN,
#elif SIZEOF_TIME_T == 8
			     gaim_marshal_POINTER__POINTER_INT64_BOOLEAN,
#else
#error Unknown size of time_t
#endif
	                     gaim_value_new(GAIM_TYPE_POINTER), 2,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_LOG),
#if SIZEOF_TIME_T == 4
	                     gaim_value_new(GAIM_TYPE_INT),
#elif SIZEOF_TIME_T == 8
	                     gaim_value_new(GAIM_TYPE_INT64),
#else
# error Unknown size of time_t
#endif
	                     gaim_value_new(GAIM_TYPE_BOOLEAN));

	gaim_prefs_connect_callback(NULL, "/core/logging/format",
							    logger_pref_cb, NULL);
	gaim_prefs_trigger_callback("/core/logging/format");

	logsize_users = g_hash_table_new_full((GHashFunc)_gaim_logsize_user_hash,
			(GEqualFunc)_gaim_logsize_user_equal,
			(GDestroyNotify)_gaim_logsize_user_free_key, NULL);
}

void
gaim_log_uninit(void)
{
	gaim_signals_unregister_by_instance(gaim_log_get_handle());
}

/****************************************************************************
 * LOGGERS ******************************************************************
 ****************************************************************************/

static char *log_get_timestamp(GaimLog *log, time_t when)
{
	gboolean show_date;
	char *date;
	struct tm tm;

	show_date = (log->type == GAIM_LOG_SYSTEM) || (time(NULL) > when + 20*60);

	date = gaim_signal_emit_return_1(gaim_log_get_handle(),
	                          "log-timestamp",
	                          log, when, show_date);
	if (date != NULL)
		return date;

	tm = *(localtime(&when));
	if (show_date)
		return g_strdup(gaim_date_format_long(&tm));
	else
		return g_strdup(gaim_time_format(&tm));
}

void gaim_log_common_writer(GaimLog *log, const char *ext)
{
	GaimLogCommonLoggerData *data = log->logger_data;

	if (data == NULL)
	{
		/* This log is new */
		char *dir;
		struct tm *tm;
		const char *tz;
		const char *date;
		char *filename;
		char *path;

		dir = gaim_log_get_log_dir(log->type, log->name, log->account);
		if (dir == NULL)
			return;

		gaim_build_dir (dir, S_IRUSR | S_IWUSR | S_IXUSR);

		tm = localtime(&log->time);
		tz = gaim_escape_filename(gaim_utf8_strftime("%Z", tm));
		date = gaim_utf8_strftime("%Y-%m-%d.%H%M%S%z", tm);

		filename = g_strdup_printf("%s%s%s", date, tz, ext ? ext : "");

		path = g_build_filename(dir, filename, NULL);
		g_free(dir);
		g_free(filename);

		log->logger_data = data = g_slice_new0(GaimLogCommonLoggerData);

		data->file = g_fopen(path, "a");
		if (data->file == NULL)
		{
			gaim_debug(GAIM_DEBUG_ERROR, "log",
					"Could not create log file %s\n", path);

			if (log->conv != NULL)
				gaim_conversation_write(log->conv, NULL, _("Logging of this conversation failed."),
										GAIM_MESSAGE_ERROR, time(NULL));

			g_free(path);
			return;
		}
		g_free(path);
	}
}

GList *gaim_log_common_lister(GaimLogType type, const char *name, GaimAccount *account, const char *ext, GaimLogLogger *logger)
{
	GDir *dir;
	GList *list = NULL;
	const char *filename;
	char *path;

	if(!account)
		return NULL;

	path = gaim_log_get_log_dir(type, name, account);
	if (path == NULL)
		return NULL;

	if (!(dir = g_dir_open(path, 0, NULL)))
	{
		g_free(path);
		return NULL;
	}

	while ((filename = g_dir_read_name(dir)))
	{
		if (gaim_str_has_suffix(filename, ext) &&
		    strlen(filename) >= (17 + strlen(ext)))
		{
			GaimLog *log;
			GaimLogCommonLoggerData *data;
			struct tm tm;
#if defined (HAVE_TM_GMTOFF) && defined (HAVE_STRUCT_TM_TM_ZONE)
			long tz_off;
			const char *rest;
			time_t stamp = gaim_str_to_time(gaim_unescape_filename(filename), FALSE, &tm, &tz_off, &rest);
			char *end;

			/* As zero is a valid offset, GAIM_NO_TZ_OFF means no offset was
			 * provided. See util.h. Yes, it's kinda ugly. */
			if (tz_off != GAIM_NO_TZ_OFF)
				tm.tm_gmtoff = tz_off - tm.tm_gmtoff;

			if (rest == NULL || (end = strchr(rest, '.')) == NULL || strchr(rest, ' ') != NULL)
			{
				log = gaim_log_new(type, name, account, NULL, stamp, NULL);
			}
			else
			{
				char *tmp = g_strndup(rest, end - rest);
				tm.tm_zone = tmp;
				log = gaim_log_new(type, name, account, NULL, stamp, &tm);
				g_free(tmp);
			}
#else
			time_t stamp = gaim_str_to_time(filename, FALSE, &tm, NULL, NULL);

			log = gaim_log_new(type, name, account, NULL, stamp, &tm);
#endif

			log->logger = logger;
			log->logger_data = data = g_slice_new0(GaimLogCommonLoggerData);

			data->path = g_build_filename(path, filename, NULL);
			list = g_list_prepend(list, log);
		}
	}
	g_dir_close(dir);
	g_free(path);
	return list;
}

int gaim_log_common_total_sizer(GaimLogType type, const char *name, GaimAccount *account, const char *ext)
{
	GDir *dir;
	int size = 0;
	const char *filename;
	char *path;

	if(!account)
		return 0;

	path = gaim_log_get_log_dir(type, name, account);
	if (path == NULL)
		return 0;

	if (!(dir = g_dir_open(path, 0, NULL)))
	{
		g_free(path);
		return 0;
	}

	while ((filename = g_dir_read_name(dir)))
	{
		if (gaim_str_has_suffix(filename, ext) &&
		    strlen(filename) >= (17 + strlen(ext)))
		{
			char *tmp = g_build_filename(path, filename, NULL);
			struct stat st;
			if (g_stat(tmp, &st))
			{
				gaim_debug_error("log", "Error stating log file: %s\n", tmp);
				g_free(tmp);
				continue;
			}
			g_free(tmp);
			size += st.st_size;
		}
	}
	g_dir_close(dir);
	g_free(path);
	return size;
}

int gaim_log_common_sizer(GaimLog *log)
{
	struct stat st;
	GaimLogCommonLoggerData *data = log->logger_data;

	g_return_val_if_fail(data != NULL, 0);

	if (!data->path || g_stat(data->path, &st))
		st.st_size = 0;

	return st.st_size;
}

/* This will build log sets for all loggers that use the common logger
 * functions because they use the same directory structure. */
static void log_get_log_sets_common(GHashTable *sets)
{
	gchar *log_path = g_build_filename(gaim_user_dir(), "logs", NULL);
	GDir *log_dir = g_dir_open(log_path, 0, NULL);
	const gchar *protocol;

	if (log_dir == NULL) {
		g_free(log_path);
		return;
	}

	while ((protocol = g_dir_read_name(log_dir)) != NULL) {
		gchar *protocol_path = g_build_filename(log_path, protocol, NULL);
		GDir *protocol_dir;
		const gchar *username;
		gchar *protocol_unescaped;
		GList *account_iter;
		GList *accounts = NULL;

		if ((protocol_dir = g_dir_open(protocol_path, 0, NULL)) == NULL) {
			g_free(protocol_path);
			continue;
		}

		/* Using g_strdup() to cover the one-in-a-million chance that a
		 * prpl's list_icon function uses gaim_unescape_filename(). */
		protocol_unescaped = g_strdup(gaim_unescape_filename(protocol));

		/* Find all the accounts for protocol. */
		for (account_iter = gaim_accounts_get_all() ; account_iter != NULL ; account_iter = account_iter->next) {
			GaimPlugin *prpl;
			GaimPluginProtocolInfo *prpl_info;

			prpl = gaim_find_prpl(gaim_account_get_protocol_id((GaimAccount *)account_iter->data));
			if (!prpl)
				continue;
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

			if (!strcmp(protocol_unescaped, prpl_info->list_icon((GaimAccount *)account_iter->data, NULL)))
				accounts = g_list_prepend(accounts, account_iter->data);
		}
		g_free(protocol_unescaped);

		while ((username = g_dir_read_name(protocol_dir)) != NULL) {
			gchar *username_path = g_build_filename(protocol_path, username, NULL);
			GDir *username_dir;
			const gchar *username_unescaped;
			GaimAccount *account = NULL;
			gchar *name;

			if ((username_dir = g_dir_open(username_path, 0, NULL)) == NULL) {
				g_free(username_path);
				continue;
			}

			/* Find the account for username in the list of accounts for protocol. */
			username_unescaped = gaim_unescape_filename(username);
			for (account_iter = g_list_first(accounts) ; account_iter != NULL ; account_iter = account_iter->next) {
				if (!strcmp(((GaimAccount *)account_iter->data)->username, username_unescaped)) {
					account = account_iter->data;
					break;
				}
			}

			/* Don't worry about the cast, name will point to dynamically allocated memory shortly. */
			while ((name = (gchar *)g_dir_read_name(username_dir)) != NULL) {
				size_t len;
				GaimLogSet *set;

				/* IMPORTANT: Always initialize all members of GaimLogSet */
				set = g_slice_new(GaimLogSet);

				/* Unescape the filename. */
				name = g_strdup(gaim_unescape_filename(name));

				/* Get the (possibly new) length of name. */
				len = strlen(name);

				set->type = GAIM_LOG_IM;
				set->name = name;
				set->account = account;
				/* set->buddy is always set below */
				set->normalized_name = g_strdup(gaim_normalize(account, name));

				/* Chat for .chat or .system at the end of the name to determine the type. */
				if (len > 7) {
					gchar *tmp = &name[len - 7];
					if (!strcmp(tmp, ".system")) {
						set->type = GAIM_LOG_SYSTEM;
						*tmp = '\0';
					}
				}
				if (len > 5) {
					gchar *tmp = &name[len - 5];
					if (!strcmp(tmp, ".chat")) {
						set->type = GAIM_LOG_CHAT;
						*tmp = '\0';
					}
				}

				/* Determine if this (account, name) combination exists as a buddy. */
				if (account != NULL)
					set->buddy = (gaim_find_buddy(account, name) != NULL);
				else
					set->buddy = FALSE;

				log_add_log_set_to_hash(sets, set);
			}
			g_free(username_path);
			g_dir_close(username_dir);
		}
		g_free(protocol_path);
		g_dir_close(protocol_dir);
	}
	g_free(log_path);
	g_dir_close(log_dir);
}

gboolean gaim_log_common_deleter(GaimLog *log)
{
	GaimLogCommonLoggerData *data;
	int ret;

	g_return_val_if_fail(log != NULL, FALSE);

	data = log->logger_data;
	if (data == NULL)
		return FALSE;

	if (data->path == NULL)
		return FALSE;

	ret = g_unlink(data->path);
	if (ret == 0)
		return TRUE;
	else if (ret == -1)
	{
		gaim_debug_error("log", "Failed to delete: %s - %s\n", data->path, strerror(errno));
	}
	else
	{
		/* I'm not sure that g_unlink() will ever return
		 * something other than 0 or -1. -- rlaager */
		gaim_debug_error("log", "Failed to delete: %s\n", data->path);
	}

	return FALSE;
}

gboolean gaim_log_common_is_deletable(GaimLog *log)
{
	GaimLogCommonLoggerData *data;
	gchar *dirname;

	g_return_val_if_fail(log != NULL, FALSE);

	data = log->logger_data;
	if (data == NULL)
		return FALSE;

	if (data->path == NULL)
		return FALSE;

#ifndef _WIN32
	dirname = g_path_get_dirname(data->path);
	if (g_access(dirname, W_OK) == 0)
	{
		g_free(dirname);
		return TRUE;
	}
	gaim_debug_info("log", "access(%s) failed: %s\n", dirname, strerror(errno));
	g_free(dirname);
#else
	/* Unless and until someone writes equivalent win32 code,
	 * we'll assume the file is deletable. */
	return TRUE;
#endif

	return FALSE;
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
	char *xhtml = NULL;

	if (!log->logger_data) {
		/* This log is new.  We could use the loggers 'new' function, but
		 * creating a new file there would result in empty files in the case
		 * that you open a convo with someone, but don't say anything.
		 */
		struct tm *tm;
		const char *tz;
		const char *date;
		char *dir = gaim_log_get_log_dir(log->type, log->name, log->account);
		char *name;
		char *filename;

		if (dir == NULL)
			return;

		tm = localtime(&log->time);
		tz = gaim_escape_filename(gaim_utf8_strftime("%Z", tm);
		date = gaim_utf8_strftime("%Y-%m-%d.%H%M%S%z", tm);

		name = g_strdup_printf("%s%s%s", date, tz, ext ? ext : "");

		gaim_build_dir (dir, S_IRUSR | S_IWUSR | S_IXUSR);

		filename = g_build_filename(dir, name, NULL);
		g_free(dir);
		g_free(name);

		log->logger_data = g_fopen(filename, "a");
		if (!log->logger_data) {
			gaim_debug(GAIM_DEBUG_ERROR, "log", "Could not create log file %s\n", filename);
			g_free(filename);
			return;
		}
		g_free(filename);
		fprintf(log->logger_data, "<?xml version='1.0' encoding='UTF-8' ?>\n"
			"<?xml-stylesheet href='file:///usr/src/web/htdocs/log-stylesheet.xsl' type='text/xml' ?>\n");

		date = gaim_utf8_strftime("%Y-%m-%d %H:%M:%S", localtime(&log->time));
		fprintf(log->logger_data, "<conversation time='%s' screenname='%s' protocol='%s'>\n",
			date, log->name, prpl);
	}

	/* if we can't write to the file, give up before we hurt ourselves */
	if(!data->file)
		return;

	date = log_get_timestamp(log, time);

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
	g_free(date);
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

static GList *xml_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	return gaim_log_common_lister(type, sn, account, ".xml", &xml_logger);
}

static GaimLogLogger xml_logger =  {
	N_("XML"), "xml",
	NULL,
	xml_logger_write,
	xml_logger_finalize,
	xml_logger_list,
	NULL,
	NULL,
	NULL
};
#endif

/****************************
 ** HTML LOGGER *************
 ****************************/

static gsize html_logger_write(GaimLog *log, GaimMessageFlags type,
							  const char *from, time_t time, const char *message)
{
	char *msg_fixed;
	char *date;
	char *header;
	GaimPlugin *plugin = gaim_find_prpl(gaim_account_get_protocol_id(log->account));
	GaimLogCommonLoggerData *data = log->logger_data;
	gsize written = 0;

	if(!data) {
		const char *prpl =
			GAIM_PLUGIN_PROTOCOL_INFO(plugin)->list_icon(log->account, NULL);
		const char *date;
		gaim_log_common_writer(log, ".html");

		data = log->logger_data;

		/* if we can't write to the file, give up before we hurt ourselves */
		if(!data->file)
			return 0;

		date = gaim_date_format_full(localtime(&log->time));

		written += fprintf(data->file, "<html><head>");
		written += fprintf(data->file, "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">");
		written += fprintf(data->file, "<title>");
		if (log->type == GAIM_LOG_SYSTEM)
			header = g_strdup_printf("System log for account %s (%s) connected at %s",
					gaim_account_get_username(log->account), prpl, date);
		else
			header = g_strdup_printf("Conversation with %s at %s on %s (%s)",
					log->name, date, gaim_account_get_username(log->account), prpl);

		written += fprintf(data->file, "%s", header);
		written += fprintf(data->file, "</title></head><body>");
		written += fprintf(data->file, "<h3>%s</h3>\n", header);
		g_free(header);
	}

	/* if we can't write to the file, give up before we hurt ourselves */
	if(!data->file)
		return 0;

	gaim_markup_html_to_xhtml(message, &msg_fixed, NULL);
	date = log_get_timestamp(log, time);

	if(log->type == GAIM_LOG_SYSTEM){
		written += fprintf(data->file, "---- %s @ %s ----<br/>\n", msg_fixed, date);
	} else {
		if (type & GAIM_MESSAGE_SYSTEM)
			written += fprintf(data->file, "<font size=\"2\">(%s)</font><b> %s</b><br/>\n", date, msg_fixed);
		else if (type & GAIM_MESSAGE_RAW)
			written += fprintf(data->file, "<font size=\"2\">(%s)</font> %s<br/>\n", date, msg_fixed);
		else if (type & GAIM_MESSAGE_ERROR)
			written += fprintf(data->file, "<font color=\"#FF0000\"><font size=\"2\">(%s)</font><b> %s</b></font><br/>\n", date, msg_fixed);
		else if (type & GAIM_MESSAGE_WHISPER)
			written += fprintf(data->file, "<font color=\"#6C2585\"><font size=\"2\">(%s)</font><b> %s:</b></font> %s<br/>\n",
					date, from, msg_fixed);
		else if (type & GAIM_MESSAGE_AUTO_RESP) {
			if (type & GAIM_MESSAGE_SEND)
				written += fprintf(data->file, _("<font color=\"#16569E\"><font size=\"2\">(%s)</font> <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>\n"), date, from, msg_fixed);
			else if (type & GAIM_MESSAGE_RECV)
				written += fprintf(data->file, _("<font color=\"#A82F2F\"><font size=\"2\">(%s)</font> <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>\n"), date, from, msg_fixed);
		} else if (type & GAIM_MESSAGE_RECV) {
			if(gaim_message_meify(msg_fixed, -1))
				written += fprintf(data->file, "<font color=\"#062585\"><font size=\"2\">(%s)</font> <b>***%s</b></font> %s<br/>\n",
						date, from, msg_fixed);
			else
				written += fprintf(data->file, "<font color=\"#A82F2F\"><font size=\"2\">(%s)</font> <b>%s:</b></font> %s<br/>\n",
						date, from, msg_fixed);
		} else if (type & GAIM_MESSAGE_SEND) {
			if(gaim_message_meify(msg_fixed, -1))
				written += fprintf(data->file, "<font color=\"#062585\"><font size=\"2\">(%s)</font> <b>***%s</b></font> %s<br/>\n",
						date, from, msg_fixed);
			else
				written += fprintf(data->file, "<font color=\"#16569E\"><font size=\"2\">(%s)</font> <b>%s:</b></font> %s<br/>\n",
						date, from, msg_fixed);
		} else {
			gaim_debug_error("log", "Unhandled message type.");
			written += fprintf(data->file, "<font size=\"2\">(%s)</font><b> %s:</b></font> %s<br/>\n",
						date, from, msg_fixed);
		}
	}
	g_free(date);
	g_free(msg_fixed);
	fflush(data->file);

	return written;
}

static void html_logger_finalize(GaimLog *log)
{
	GaimLogCommonLoggerData *data = log->logger_data;
	if (data) {
		if(data->file) {
			fprintf(data->file, "</body></html>\n");
			fclose(data->file);
		}
		g_free(data->path);

		g_slice_free(GaimLogCommonLoggerData, data);
	}
}

static GList *html_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	return gaim_log_common_lister(type, sn, account, ".html", html_logger);
}

static GList *html_logger_list_syslog(GaimAccount *account)
{
	return gaim_log_common_lister(GAIM_LOG_SYSTEM, ".system", account, ".html", html_logger);
}

static char *html_logger_read(GaimLog *log, GaimLogReadFlags *flags)
{
	char *read;
	GaimLogCommonLoggerData *data = log->logger_data;
	*flags = GAIM_LOG_READ_NO_NEWLINE;
	if (!data || !data->path)
		return g_strdup(_("<font color=\"red\"><b>Unable to find log path!</b></font>"));
	if (g_file_get_contents(data->path, &read, NULL, NULL)) {
		char *minus_header = strchr(read, '\n');

		if (!minus_header)
			return read;

		minus_header = g_strdup(minus_header + 1);
		g_free(read);

		return minus_header;
	}
	return g_strdup_printf(_("<font color=\"red\"><b>Could not read file: %s</b></font>"), data->path);
}

static int html_logger_total_size(GaimLogType type, const char *name, GaimAccount *account)
{
	return gaim_log_common_total_sizer(type, name, account, ".html");
}


/****************************
 ** PLAIN TEXT LOGGER *******
 ****************************/

static gsize txt_logger_write(GaimLog *log,
							 GaimMessageFlags type,
							 const char *from, time_t time, const char *message)
{
	char *date;
	GaimPlugin *plugin = gaim_find_prpl(gaim_account_get_protocol_id(log->account));
	GaimLogCommonLoggerData *data = log->logger_data;
	char *stripped = NULL;

	gsize written = 0;

	if (data == NULL) {
		/* This log is new.  We could use the loggers 'new' function, but
		 * creating a new file there would result in empty files in the case
		 * that you open a convo with someone, but don't say anything.
		 */
		const char *prpl =
			GAIM_PLUGIN_PROTOCOL_INFO(plugin)->list_icon(log->account, NULL);
		gaim_log_common_writer(log, ".txt");

		data = log->logger_data;

		/* if we can't write to the file, give up before we hurt ourselves */
		if(!data->file)
			return 0;

		if (log->type == GAIM_LOG_SYSTEM)
			written += fprintf(data->file, "System log for account %s (%s) connected at %s\n",
				gaim_account_get_username(log->account), prpl,
				gaim_date_format_full(localtime(&log->time)));
		else
			written += fprintf(data->file, "Conversation with %s at %s on %s (%s)\n",
				log->name, gaim_date_format_full(localtime(&log->time)),
				gaim_account_get_username(log->account), prpl);
	}

	/* if we can't write to the file, give up before we hurt ourselves */
	if(!data->file)
		return 0;

	stripped = gaim_markup_strip_html(message);
	date = log_get_timestamp(log, time);

	if(log->type == GAIM_LOG_SYSTEM){
		written += fprintf(data->file, "---- %s @ %s ----\n", stripped, date);
	} else {
		if (type & GAIM_MESSAGE_SEND ||
			type & GAIM_MESSAGE_RECV) {
			if (type & GAIM_MESSAGE_AUTO_RESP) {
				written += fprintf(data->file, _("(%s) %s <AUTO-REPLY>: %s\n"), date,
						from, stripped);
			} else {
				if(gaim_message_meify(stripped, -1))
					written += fprintf(data->file, "(%s) ***%s %s\n", date, from,
							stripped);
				else
					written += fprintf(data->file, "(%s) %s: %s\n", date, from,
							stripped);
			}
		} else if (type & GAIM_MESSAGE_SYSTEM ||
			type & GAIM_MESSAGE_ERROR ||
			type & GAIM_MESSAGE_RAW)
			written += fprintf(data->file, "(%s) %s\n", date, stripped);
		else if (type & GAIM_MESSAGE_NO_LOG) {
			/* This shouldn't happen */
			g_free(stripped);
			return written;
		} else if (type & GAIM_MESSAGE_WHISPER)
			written += fprintf(data->file, "(%s) *%s* %s", date, from, stripped);
		else
			written += fprintf(data->file, "(%s) %s%s %s\n", date, from ? from : "",
					from ? ":" : "", stripped);
	}
	g_free(date);
	g_free(stripped);
	fflush(data->file);

	return written;
}

static void txt_logger_finalize(GaimLog *log)
{
	GaimLogCommonLoggerData *data = log->logger_data;
	if (data) {
		if(data->file)
			fclose(data->file);
		g_free(data->path);

		g_slice_free(GaimLogCommonLoggerData, data);
	}
}

static GList *txt_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	return gaim_log_common_lister(type, sn, account, ".txt", txt_logger);
}

static GList *txt_logger_list_syslog(GaimAccount *account)
{
	return gaim_log_common_lister(GAIM_LOG_SYSTEM, ".system", account, ".txt", txt_logger);
}

static char *txt_logger_read(GaimLog *log, GaimLogReadFlags *flags)
{
	char *read, *minus_header, *minus_header2;
	GaimLogCommonLoggerData *data = log->logger_data;
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
		minus_header2 = g_markup_escape_text(minus_header, -1);
		g_free(minus_header);
		read = gaim_markup_linkify(minus_header2);
		g_free(minus_header2);
		return read;
	}
	return g_strdup_printf(_("<font color=\"red\"><b>Could not read file: %s</b></font>"), data->path);
}

static int txt_logger_total_size(GaimLogType type, const char *name, GaimAccount *account)
{
	return gaim_log_common_total_sizer(type, name, account, ".txt");
}


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

static GList *old_logger_list(GaimLogType type, const char *sn, GaimAccount *account)
{
	char *logfile = g_strdup_printf("%s.log", gaim_normalize(account, sn));
	char *pathstr = g_build_filename(gaim_user_dir(), "logs", logfile, NULL);
	GaimStringref *pathref = gaim_stringref_new(pathstr);
	struct stat st;
	time_t log_last_modified;
	FILE *index;
	FILE *file;
	int index_fd;
	char *index_tmp;
	char buf[BUF_LONG];
	struct tm tm;
	char month[4];
	struct old_logger_data *data = NULL;
	char *newlog;
	int logfound = 0;
	int lastoff = 0;
	int newlen;
	time_t lasttime = 0;

	GaimLog *log = NULL;
	GList *list = NULL;

	g_free(logfile);

	if (g_stat(gaim_stringref_value(pathref), &st))
	{
		gaim_stringref_unref(pathref);
		g_free(pathstr);
		return NULL;
	}
	else
		log_last_modified = st.st_mtime;

	/* Change the .log extension to .idx */
	strcpy(pathstr + strlen(pathstr) - 3, "idx");

	if (g_stat(pathstr, &st) == 0)
	{
		if (st.st_mtime < log_last_modified)
		{
			gaim_debug_warning("log", "Index \"%s\" exists, but is older than the log.\n", pathstr);
		}
		else
		{
			/* The index file exists and is at least as new as the log, so open it. */
			if (!(index = g_fopen(pathstr, "rb")))
			{
				gaim_debug_error("log", "Failed to open index file \"%s\" for reading: %s\n",
				                 pathstr, strerror(errno));

				/* Fall through so that we'll parse the log file. */
			}
			else
			{
				gaim_debug_info("log", "Using index: %s\n", pathstr);
				g_free(pathstr);
				while (fgets(buf, BUF_LONG, index))
				{
					unsigned long idx_time;
					if (sscanf(buf, "%d\t%d\t%lu", &lastoff, &newlen, &idx_time) == 3)
					{
						log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, -1, NULL);
						log->logger = old_logger;
						log->time = (time_t)idx_time;

						/* IMPORTANT: Always set all members of struct old_logger_data */
						data = g_slice_new(struct old_logger_data);

						data->pathref = gaim_stringref_ref(pathref);
						data->offset = lastoff;
						data->length = newlen;

						log->logger_data = data;
						list = g_list_prepend(list, log);
					}
				}
				fclose(index);

				return list;
			}
		}
	}

	if (!(file = g_fopen(gaim_stringref_value(pathref), "rb"))) {
		gaim_debug_error("log", "Failed to open log file \"%s\" for reading: %s\n",
		                   gaim_stringref_value(pathref), strerror(errno));
		gaim_stringref_unref(pathref);
		g_free(pathstr);
		return NULL;
	}

	index_tmp = g_strdup_printf("%s.XXXXXX", pathstr);
	if ((index_fd = g_mkstemp(index_tmp)) == -1) {
		gaim_debug_error("log", "Failed to open index temp file: %s\n",
		                 strerror(errno));
		g_free(pathstr);
		g_free(index_tmp);
		index = NULL;
	} else {
		if ((index = fdopen(index_fd, "wb")) == NULL)
		{
			gaim_debug_error("log", "Failed to fdopen() index temp file: %s\n",
			                 strerror(errno));
			close(index_fd);
			if (index_tmp != NULL)
			{
				g_unlink(index_tmp);
				g_free(index_tmp);
			}
			g_free(pathstr);
		}
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
					log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, -1, NULL);
					log->logger = old_logger;
					log->time = lasttime;

					/* IMPORTANT: Always set all members of struct old_logger_data */
					data = g_slice_new(struct old_logger_data);

					data->pathref = gaim_stringref_ref(pathref);
					data->offset = lastoff;
					data->length = newlen;

					log->logger_data = data;
					list = g_list_prepend(list, log);

					/* XXX: There is apparently Is there a proper way to print a time_t? */
					if (index != NULL)
						fprintf(index, "%d\t%d\t%lu\n", data->offset, data->length, (unsigned long)log->time);
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
			log = gaim_log_new(GAIM_LOG_IM, sn, account, NULL, -1, NULL);
			log->logger = old_logger;
			log->time = lasttime;

			/* IMPORTANT: Always set all members of struct old_logger_data */
			data = g_slice_new(struct old_logger_data);

			data->pathref = gaim_stringref_ref(pathref);
			data->offset = lastoff;
			data->length = newlen;

			log->logger_data = data;
			list = g_list_prepend(list, log);

			/* XXX: Is there a proper way to print a time_t? */
			if (index != NULL)
				fprintf(index, "%d\t%d\t%d\n", data->offset, data->length, (int)log->time);
		}
	}

	gaim_stringref_unref(pathref);
	fclose(file);
	if (index != NULL)
	{
		fclose(index);

		if (index_tmp == NULL)
		{
			g_free(pathstr);
			g_return_val_if_reached(list);
		}

		if (g_rename(index_tmp, pathstr))
		{
			gaim_debug_warning("log", "Failed to rename index temp file \"%s\" to \"%s\": %s\n",
			                   index_tmp, pathstr, strerror(errno));
			g_unlink(index_tmp);
			g_free(index_tmp);
		}
		else
			gaim_debug_info("log", "Built index: %s\n", pathstr);

		g_free(pathstr);
	}
	return list;
}

static int old_logger_total_size(GaimLogType type, const char *name, GaimAccount *account)
{
	char *logfile = g_strdup_printf("%s.log", gaim_normalize(account, name));
	char *pathstr = g_build_filename(gaim_user_dir(), "logs", logfile, NULL);
	int size;
	struct stat st;

	if (g_stat(pathstr, &st))
		size = 0;
	else
		size = st.st_size;

	g_free(logfile);
	g_free(pathstr);

	return size;
}

static char * old_logger_read (GaimLog *log, GaimLogReadFlags *flags)
{
	struct old_logger_data *data = log->logger_data;
	FILE *file = g_fopen(gaim_stringref_value(data->pathref), "rb");
	char *tmp, *read = g_malloc(data->length + 1);
	fseek(file, data->offset, SEEK_SET);
	fread(read, data->length, 1, file);
	fclose(file);
	read[data->length] = '\0';
	*flags = 0;
	if(strstr(read, "<BR>"))
		*flags |= GAIM_LOG_READ_NO_NEWLINE;
	else {
		tmp = g_markup_escape_text(read, -1);
		g_free(read);
		read = gaim_markup_linkify(tmp);
		g_free(tmp);
	}
	return read;
}

static int old_logger_size (GaimLog *log)
{
	struct old_logger_data *data = log->logger_data;
	return data ? data->length : 0;
}

static void old_logger_get_log_sets(GaimLogSetCallback cb, GHashTable *sets)
{
	char *log_path = g_build_filename(gaim_user_dir(), "logs", NULL);
	GDir *log_dir = g_dir_open(log_path, 0, NULL);
	gchar *name;
	GaimBlistNode *gnode, *cnode, *bnode;

	g_free(log_path);
	if (log_dir == NULL)
		return;

	/* Don't worry about the cast, name will be filled with a dynamically allocated data shortly. */
	while ((name = (gchar *)g_dir_read_name(log_dir)) != NULL) {
		size_t len;
		gchar *ext;
		GaimLogSet *set;
		gboolean found = FALSE;

		/* Unescape the filename. */
		name = g_strdup(gaim_unescape_filename(name));

		/* Get the (possibly new) length of name. */
		len = strlen(name);

		if (len < 5) {
			g_free(name);
			continue;
		}

		/* Make sure we're dealing with a log file. */
		ext = &name[len - 4];
		if (strcmp(ext, ".log")) {
			g_free(name);
			continue;
		}

		/* IMPORTANT: Always set all members of GaimLogSet */
		set = g_slice_new(GaimLogSet);

		/* Chat for .chat at the end of the name to determine the type. */
		*ext = '\0';
		set->type = GAIM_LOG_IM;
		if (len > 9) {
			char *tmp = &name[len - 9];
			if (!strcmp(tmp, ".chat")) {
				set->type = GAIM_LOG_CHAT;
				*tmp = '\0';
			}
		}

		set->name = set->normalized_name = name;

		/* Search the buddy list to find the account and to determine if this is a buddy. */
		for (gnode = gaim_get_blist()->root; !found && gnode != NULL; gnode = gnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;

			for (cnode = gnode->child; !found && cnode != NULL; cnode = cnode->next)
			{
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;

				for (bnode = cnode->child; !found && bnode != NULL; bnode = bnode->next)
				{
					GaimBuddy *buddy = (GaimBuddy *)bnode;

					if (!strcmp(buddy->name, name)) {
						set->account = buddy->account;
						set->buddy = TRUE;
						found = TRUE;
					}
				}
			}
		}

		if (!found)
		{
			set->account = NULL;
			set->buddy = FALSE;
		}

		cb(sets, set);
	}
	g_dir_close(log_dir);
}

static void old_logger_finalize(GaimLog *log)
{
	struct old_logger_data *data = log->logger_data;
	gaim_stringref_unref(data->pathref);
	g_slice_free(struct old_logger_data, data);
}
