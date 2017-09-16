/* purple
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

#ifndef _PURPLE_LOG_H_
#define _PURPLE_LOG_H_
/**
 * SECTION:log
 * @section_id: libpurple-log
 * @short_description: <filename>log.h</filename>
 * @title: Logging API
 * @see_also: <link linkend="chapter-signals-log">Log signals</link>
 */

#include <stdio.h>

#define PURPLE_TYPE_LOG  (purple_log_get_type())

/********************************************************
 * DATA STRUCTURES **************************************
 ********************************************************/

typedef struct _PurpleLog PurpleLog;
typedef struct _PurpleLogLogger PurpleLogLogger;
typedef struct _PurpleLogCommonLoggerData PurpleLogCommonLoggerData;
typedef struct _PurpleLogSet PurpleLogSet;

typedef enum {
	PURPLE_LOG_IM,
	PURPLE_LOG_CHAT,
	PURPLE_LOG_SYSTEM
} PurpleLogType;

typedef enum {
	PURPLE_LOG_READ_NO_NEWLINE = 1
} PurpleLogReadFlags;

#include "account.h"
#include "conversations.h"

typedef void (*PurpleLogSetCallback) (GHashTable *sets, PurpleLogSet *set);

/**
 * PurpleLogLogger:
 * @name:         The logger's name
 * @id:           An identifier to refer to this logger
 * @create:       This gets called when the log is first created. I don't think
 *                this is actually needed.
 * @write:        This is used to write to the log file
 * @finalize:     Called when the log is destroyed
 * @list:         This function returns a sorted #GList of available PurpleLogs
 * @read:         Given one of the logs returned by the logger's list function,
 *                this returns the contents of the log in #GtkWebView markup
 * @size:         Given one of the logs returned by the logger's list function,
 *                this returns the size of the log in bytes
 * @total_size:   Returns the total size of all the logs. If this is undefined a
 *                default implementation is used
 * @list_syslog:  This function returns a sorted #GList of available system
 *                #PurpleLog's
 * @get_log_sets: Adds #PurpleLogSet's to a #GHashTable. By passing the data in
 *                the #PurpleLogSet's to list, the caller can get every
 *                available #PurpleLog from the logger. Loggers using
 *                purple_log_common_writer() (or otherwise storing their logs in
 *                the same directory structure as the stock loggers) do not
 *                need to implement this function.
 *                <sbr/>Loggers which implement this function must create a
 *                #PurpleLogSet, then call @cb with @sets and the newly created
 *                #PurpleLogSet.
 * @remove:       Attempts to delete the specified log, indicating success or
 *                failure
 * @is_deletable: Tests whether a log is deletable
 *
 * A log logger.
 *
 * This struct gets filled out and is included in the PurpleLog.  It contains
 * everything needed to write and read from logs.
 */
struct _PurpleLogLogger {
	char *name;
	char *id;

	void (*create)(PurpleLog *log);

	gsize (*write)(PurpleLog *log,
		     PurpleMessageFlags type,
		     const char *from,
		     GDateTime *time,
		     const char *message);

	void (*finalize)(PurpleLog *log);

	GList *(*list)(PurpleLogType type, const char *name, PurpleAccount *account);

	char *(*read)(PurpleLog *log, PurpleLogReadFlags *flags);

	int (*size)(PurpleLog *log);

	int (*total_size)(PurpleLogType type, const char *name, PurpleAccount *account);

	GList *(*list_syslog)(PurpleAccount *account);

	void (*get_log_sets)(PurpleLogSetCallback cb, GHashTable *sets);

	gboolean (*remove)(PurpleLog *log);

	gboolean (*is_deletable)(PurpleLog *log);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleLog:
 * @type:        The type of log this is
 * @name:        The name of this log
 * @account:     The account this log is taking place on
 * @conv:        The conversation being logged
 * @time:        The time this conversation started, converted to the local
 *               timezone
 * @logger:      The logging mechanism this log is to use
 * @logger_data: Data used by the log logger
 *
 * A log.  Not the wooden type.
 */
struct _PurpleLog {
	PurpleLogType type;
	char *name;
	PurpleAccount *account;
	PurpleConversation *conv;
	GDateTime *time;

	PurpleLogLogger *logger;
	void *logger_data;

	/* IMPORTANT: Some code in log.c allocates these without zeroing them.
	 * IMPORTANT: Update that code if you add members here. */
};

/**
 * PurpleLogCommonLoggerData:
 *
 * A common logger_data struct containing a file handle and path, as well
 * as a pointer to something else for additional data.
 */
struct _PurpleLogCommonLoggerData {
	char *path;
	FILE *file;
	void *extra_data;
};

/**
 * PurpleLogSet:
 * @type:            The type of logs available
 * @name:            The name of the logs available
 * @account:         The account the available logs took place on. This will be
 *                   %NULL if the account no longer exists. (Depending on a
 *                   logger's implementation of list, it may not be possible to
 *                   load such logs.)
 * @buddy:           Is this (account, name) a buddy on the buddy list?
 * @normalized_name: The normalized version of @name. It must be set, and may
 *                   be set to the same pointer value as @name.
 *
 * Describes available logs.
 *
 * By passing the elements of this struct to purple_log_get_logs(), the caller
 * can get all available PurpleLogs.
 */
struct _PurpleLogSet {
	PurpleLogType type;
	char *name;
	PurpleAccount *account;
	gboolean buddy;
	char *normalized_name;

	/* IMPORTANT: Some code in log.c allocates these without zeroing them.
	 * IMPORTANT: Update that code if you add members here. */
};

G_BEGIN_DECLS

/***************************************/
/* Log Functions                       */
/***************************************/

/**
 * purple_log_get_type:
 *
 * Returns: The #GType for the #PurpleLog boxed structure.
 */
/* TODO Boxing of PurpleLog is a temporary solution to having a GType for
 *      logs. This should rather be a GObject instead of a GBoxed.
 */
GType purple_log_get_type(void);

/**
 * purple_log_new:
 * @type:        The type of log this is.
 * @name:        The name of this conversation (buddy name, chat name,
 *                    etc.)
 * @account:     The account the conversation is occurring on
 * @conv:        The conversation being logged
 * @time:        The time this conversation started
 *
 * Creates a new log
 *
 * Returns:            The new log
 */
PurpleLog *purple_log_new(PurpleLogType type, const char *name, PurpleAccount *account,
                      PurpleConversation *conv, GDateTime *time);

/**
 * purple_log_free:
 * @log:         The log to destroy
 *
 * Frees a log
 */
void purple_log_free(PurpleLog *log);

/**
 * purple_log_write:
 * @log:          The log to write to
 * @type:         The type of message being logged
 * @from:         Whom this message is coming from, or %NULL for
 *                     system messages
 * @time:         A timestamp in UNIX time
 * @message:      The message to log
 *
 * Writes to a log file. Assumes you have checked preferences already.
 */
void purple_log_write(PurpleLog *log,
		    PurpleMessageFlags type,
		    const char *from,
		    GDateTime *time,
		    const char *message);

/**
 * purple_log_read:
 * @log:   The log to read from
 * @flags: The returned logging flags.
 *
 * Reads from a log
 *
 * Returns: The contents of this log in Purple Markup.
 */
char *purple_log_read(PurpleLog *log, PurpleLogReadFlags *flags);

/**
 * purple_log_get_logs:
 * @type:                The type of the log
 * @name:                The name of the log
 * @account:             The account
 *
 * Returns a list of all available logs
 *
 * Returns: (element-type PurpleLog): A sorted list of logs
 */
GList *purple_log_get_logs(PurpleLogType type, const char *name, PurpleAccount *account);

/**
 * purple_log_get_log_sets:
 *
 * Returns a GHashTable of PurpleLogSets.
 *
 * A "log set" here means the information necessary to gather the
 * PurpleLogs for a given buddy/chat. This information would be passed
 * to purple_log_list to get a list of PurpleLogs.
 *
 * The primary use of this function is to get a list of everyone the
 * user has ever talked to (assuming he or she uses logging).
 *
 * The GHashTable that's returned will free all log sets in it when
 * destroyed. If a PurpleLogSet is removed from the GHashTable, it
 * must be freed with purple_log_set_free().
 *
 * Returns: (element-type PurpleLogSet PurpleLogSet): All available unique log
 *          sets.
 */
GHashTable *purple_log_get_log_sets(void);

/**
 * purple_log_get_system_logs:
 * @account: The account
 *
 * Returns a list of all available system logs
 *
 * Returns: (element-type PurpleLog): A sorted list of logs
 */
GList *purple_log_get_system_logs(PurpleAccount *account);

/**
 * purple_log_get_size:
 * @log:                 The log
 *
 * Returns the size of a log
 *
 * Returns:                    The size of the log, in bytes
 */
int purple_log_get_size(PurpleLog *log);

/**
 * purple_log_get_total_size:
 * @type:                The type of the log
 * @name:                The name of the log
 * @account:             The account
 *
 * Returns the size, in bytes, of all available logs in this conversation
 *
 * Returns:                    The size in bytes
 */
int purple_log_get_total_size(PurpleLogType type, const char *name, PurpleAccount *account);

/**
 * purple_log_get_activity_score:
 * @type:                The type of the log
 * @name:                The name of the log
 * @account:             The account
 *
 * Returns the activity score of a log, based on total size in bytes,
 * which is then decayed based on age
 *
 * Returns:                    The activity score
 */
int purple_log_get_activity_score(PurpleLogType type, const char *name, PurpleAccount *account);

/**
 * purple_log_is_deletable:
 * @log:                 The log
 *
 * Tests whether a log is deletable
 *
 * A return value of %FALSE indicates that purple_log_delete() will fail on this
 * log, unless something changes between the two calls.  A return value of %TRUE,
 * however, does not guarantee the log can be deleted.
 *
 * Returns:                    A boolean indicating if the log is deletable
 */
gboolean purple_log_is_deletable(PurpleLog *log);

/**
 * purple_log_delete:
 * @log:                 The log
 *
 * Deletes a log
 *
 * Returns:                    A boolean indicating success or failure
 */
gboolean purple_log_delete(PurpleLog *log);

/**
 * purple_log_get_log_dir:
 * @type:                The type of the log.
 * @name:                The name of the log.
 * @account:             The account.
 *
 * Returns the default logger directory Purple uses for a given account
 * and username.  This would be where Purple stores logs created by
 * the built-in text or HTML loggers.
 *
 * Returns:                    The default logger directory for Purple.
 */
char *purple_log_get_log_dir(PurpleLogType type, const char *name, PurpleAccount *account);

/**
 * purple_log_compare:
 * @y:                   A PurpleLog
 * @z:                   Another PurpleLog
 *
 * Implements GCompareFunc for PurpleLogs
 *
 * Returns:                    A value as specified by GCompareFunc
 */
gint purple_log_compare(gconstpointer y, gconstpointer z);

/**
 * purple_log_set_compare:
 * @y:                   A PurpleLogSet
 * @z:                   Another PurpleLogSet
 *
 * Implements GCompareFunc for PurpleLogSets
 *
 * Returns:                    A value as specified by GCompareFunc
 */
gint purple_log_set_compare(gconstpointer y, gconstpointer z);

/**
 * purple_log_set_free:
 * @set:         The log set to destroy
 *
 * Frees a log set
 */
void purple_log_set_free(PurpleLogSet *set);

/******************************************/
/* Common Logger Functions                */
/******************************************/

/**
 * purple_log_common_writer:
 * @log:   The log to write to.
 * @ext:   The file extension to give to this log file.
 *
 * Opens a new log file in the standard Purple log location
 * with the given file extension, named for the current time,
 * for writing.  If a log file is already open, the existing
 * file handle is retained.  The log's logger_data value is
 * set to a PurpleLogCommonLoggerData struct containing the log
 * file handle and log path.
 *
 * This function is intended to be used as a "common"
 * implementation of a logger's <literal>write</literal> function.
 * It should only be passed to purple_log_logger_new() and never
 * called directly.
 */
void purple_log_common_writer(PurpleLog *log, const char *ext);

/**
 * purple_log_common_lister:
 * @type:     The type of the logs being listed.
 * @name:     The name of the log.
 * @account:  The account of the log.
 * @ext:      The file extension this log format uses.
 * @logger:   A reference to the logger struct for this log.
 *
 * Returns a sorted list of logs of the requested type.
 *
 * This function should only be used with logs that are written
 * with purple_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's <literal>list</literal> function.
 * It should only be passed to purple_log_logger_new() and never
 * called directly.
 *
 * Returns: (element-type PurpleLog): A sorted list of logs matching the parameters.
 */
GList *purple_log_common_lister(PurpleLogType type, const char *name,
							  PurpleAccount *account, const char *ext,
							  PurpleLogLogger *logger);

/**
 * purple_log_common_total_sizer:
 * @type:     The type of the logs being sized.
 * @name:     The name of the logs to size
 *                 (e.g. the username or chat name).
 * @account:  The account of the log.
 * @ext:      The file extension this log format uses.
 *
 * Returns the total size of all the logs for a given user, with
 * a given extension.
 *
 * This function should only be used with logs that are written
 * with purple_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's <literal>total_size</literal>
 * function. It should only be passed to purple_log_logger_new() and never
 * called directly.
 *
 * Returns: The size of all the logs with the specified extension
 *         for the specified user.
 */
int purple_log_common_total_sizer(PurpleLogType type, const char *name,
								PurpleAccount *account, const char *ext);

/**
 * purple_log_common_sizer:
 * @log:      The PurpleLog to size.
 *
 * Returns the size of a given PurpleLog.
 *
 * This function should only be used with logs that are written
 * with purple_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's <literal>size</literal> function.
 * It should only be passed to purple_log_logger_new() and never
 * called directly.
 *
 * Returns: An integer indicating the size of the log in bytes.
 */
int purple_log_common_sizer(PurpleLog *log);

/**
 * purple_log_common_deleter:
 * @log:      The PurpleLog to delete.
 *
 * Deletes a log
 *
 * This function should only be used with logs that are written
 * with purple_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's <literal>delete</literal> function.
 * It should only be passed to purple_log_logger_new() and never
 * called directly.
 *
 * Returns: A boolean indicating success or failure.
 */
gboolean purple_log_common_deleter(PurpleLog *log);

/**
 * purple_log_common_is_deletable:
 * @log:      The PurpleLog to check.
 *
 * Checks to see if a log is deletable
 *
 * This function should only be used with logs that are written
 * with purple_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's <literal>is_deletable</literal>
 * function. It should only be passed to purple_log_logger_new() and never
 * called directly.
 *
 * Returns: A boolean indicating if the log is deletable.
 */
gboolean purple_log_common_is_deletable(PurpleLog *log);

/******************************************/
/* Logger Functions                       */
/******************************************/

/**
 * purple_log_logger_new:
 * @id:           The logger's id.
 * @name:         The logger's name.
 * @functions:    The number of functions being passed. The following
 *                functions are currently available (in order):
 *                <literal>create</literal>, <literal>write</literal>,
 *                <literal>finalize</literal>, <literal>list</literal>,
 *                <literal>read</literal>, <literal>size</literal>,
 *                <literal>total_size</literal>, <literal>list_syslog</literal>,
 *                <literal>get_log_sets</literal>, <literal>remove</literal>,
 *                <literal>is_deletable</literal>.
 *                For details on these functions, see PurpleLogLogger.
 *                Functions may not be skipped. For example, passing
 *                <literal>create</literal> and <literal>write</literal> is
 *                acceptable (for a total of two functions). Passing
 *                <literal>create</literal> and <literal>finalize</literal>,
 *                however, is not. To accomplish that, the caller must pass
 *                <literal>create</literal>, %NULL (a placeholder for
 *                <literal>write</literal>), and <literal>finalize</literal>
 *                (for a total of 3 functions).
 *
 * Creates a new logger
 *
 * Returns: The new logger
 */
PurpleLogLogger *purple_log_logger_new(const char *id, const char *name, int functions, ...);

/**
 * purple_log_logger_free:
 * @logger:       The logger to free
 *
 * Frees a logger
 */
void purple_log_logger_free(PurpleLogLogger *logger);

/**
 * purple_log_logger_add:
 * @logger:       The new logger to add
 *
 * Adds a new logger
 */
void purple_log_logger_add (PurpleLogLogger *logger);

/**
 * purple_log_logger_remove:
 * @logger:       The logger to remove
 *
 * Removes a logger
 */
void purple_log_logger_remove (PurpleLogLogger *logger);

/**
 * purple_log_logger_set:
 * @logger:       The logger to set
 *
 * Sets the current logger
 */
void purple_log_logger_set (PurpleLogLogger *logger);

/**
 * purple_log_logger_get:
 *
 * Returns the current logger
 *
 * Returns: logger      The current logger
 */
PurpleLogLogger *purple_log_logger_get (void);

/**
 * purple_log_logger_get_options:
 *
 * Returns a GList containing the IDs and names of the registered
 * loggers.
 *
 * Returns: (element-type utf8) (transfer container): The list of IDs and names.
 */
GList *purple_log_logger_get_options(void);

/**************************************************************************/
/* Log Subsystem                                                          */
/**************************************************************************/

/**
 * purple_log_init:
 *
 * Initializes the log subsystem.
 */
void purple_log_init(void);

/**
 * purple_log_get_handle:
 *
 * Returns the log subsystem handle.
 *
 * Returns: The log subsystem handle.
 */
void *purple_log_get_handle(void);

/**
 * purple_log_uninit:
 *
 * Uninitializes the log subsystem.
 */
void purple_log_uninit(void);


G_END_DECLS

#endif /* _PURPLE_LOG_H_ */
