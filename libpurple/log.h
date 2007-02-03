/**
 * @file log.h Logging API
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
#ifndef _GAIM_LOG_H_
#define _GAIM_LOG_H_

#include <stdio.h>


/********************************************************
 * DATA STRUCTURES **************************************
 ********************************************************/

typedef struct _GaimLog GaimLog;
typedef struct _GaimLogLogger GaimLogLogger;
typedef struct _GaimLogCommonLoggerData GaimLogCommonLoggerData;
typedef struct _GaimLogSet GaimLogSet;

typedef enum {
	GAIM_LOG_IM,
	GAIM_LOG_CHAT,
	GAIM_LOG_SYSTEM
} GaimLogType;

typedef enum {
	GAIM_LOG_READ_NO_NEWLINE = 1
} GaimLogReadFlags;

#include "account.h"
#include "conversation.h"

typedef void (*GaimLogSetCallback) (GHashTable *sets, GaimLogSet *set);

/**
 * A log logger.
 *
 * This struct gets filled out and is included in the GaimLog.  It contains everything
 * needed to write and read from logs.
 */
struct _GaimLogLogger {
	char *name;               /**< The logger's name */
	char *id;                 /**< an identifier to refer to this logger */

	/** This gets called when the log is first created.
	    I don't think this is actually needed. */
	void (*create)(GaimLog *log);

	/** This is used to write to the log file */
	gsize (*write)(GaimLog *log,
		     GaimMessageFlags type,
		     const char *from,
		     time_t time,
		     const char *message);

	/** Called when the log is destroyed */
	void (*finalize)(GaimLog *log);

	/** This function returns a sorted GList of available GaimLogs */
	GList *(*list)(GaimLogType type, const char *name, GaimAccount *account);

	/** Given one of the logs returned by the logger's list function,
	 *  this returns the contents of the log in GtkIMHtml markup */
	char *(*read)(GaimLog *log, GaimLogReadFlags *flags);

	/** Given one of the logs returned by the logger's list function,
	 *  this returns the size of the log in bytes */
	int (*size)(GaimLog *log);

	/** Returns the total size of all the logs. If this is undefined a default
	 *  implementation is used */
	int (*total_size)(GaimLogType type, const char *name, GaimAccount *account);

	/** This function returns a sorted GList of available system GaimLogs */
	GList *(*list_syslog)(GaimAccount *account);

	/** Adds GaimLogSets to a GHashTable. By passing the data in the GaimLogSets
	 *  to list, the caller can get every available GaimLog from the logger.
	 *  Loggers using gaim_log_common_writer() (or otherwise storing their
	 *  logs in the same directory structure as the stock loggers) do not
	 *  need to implement this function.
	 *
	 *  Loggers which implement this function must create a GaimLogSet,
	 *  then call @a cb with @a sets and the newly created GaimLogSet. */
	void (*get_log_sets)(GaimLogSetCallback cb, GHashTable *sets);

	/* Attempts to delete the specified log, indicating success or failure */
	gboolean (*delete)(GaimLog *log);

	/* Tests whether a log is deletable */
	gboolean (*is_deletable)(GaimLog *log);
};

/**
 * A log.  Not the wooden type.
 */
struct _GaimLog {
	GaimLogType type;                     /**< The type of log this is */
	char *name;                           /**< The name of this log */
	GaimAccount *account;                 /**< The account this log is taking
	                                           place on */
	GaimConversation *conv;               /**< The conversation being logged */
	time_t time;                          /**< The time this conversation
	                                           started, converted to the local timezone */

	GaimLogLogger *logger;                /**< The logging mechanism this log
	                                           is to use */
	void *logger_data;                    /**< Data used by the log logger */
	struct tm *tm;                        /**< The time this conversation
	                                           started, saved with original
	                                           timezone data, if available and
	                                           if struct tm has the BSD
	                                           timezone fields, else @c NULL.
	                                           Do NOT modify anything in this struct.*/

	/* IMPORTANT: Some code in log.c allocates these without zeroing them.
	 * IMPORTANT: Update that code if you add members here. */
};

/**
 * A common logger_data struct containing a file handle and path, as well
 * as a pointer to something else for additional data.
 */
struct _GaimLogCommonLoggerData {
	char *path;
	FILE *file;
	void *extra_data;
};

/**
 * Describes available logs.
 *
 * By passing the elements of this struct to gaim_log_get_logs(), the caller
 * can get all available GaimLogs.
 */
struct _GaimLogSet {
	GaimLogType type;                     /**< The type of logs available */
	char *name;                           /**< The name of the logs available */
	GaimAccount *account;                 /**< The account the available logs
	                                           took place on. This will be
	                                           @c NULL if the account no longer
	                                           exists. (Depending on a
	                                           logger's implementation of
	                                           list, it may not be possible
	                                           to load such logs.) */
	gboolean buddy;                       /**< Is this (account, name) a buddy
	                                           on the buddy list? */
	char *normalized_name;                /**< The normalized version of
	                                           @a name. It must be set, and
	                                           may be set to the same pointer
	                                           value as @a name. */

	/* IMPORTANT: Some code in log.c allocates these without zeroing them.
	 * IMPORTANT: Update that code if you add members here. */
};

#ifdef __cplusplus
extern "C" {
#endif

/***************************************/
/** @name Log Functions                */
/***************************************/
/*@{*/

/**
 * Creates a new log
 *
 * @param type        The type of log this is.
 * @param name        The name of this conversation (screenname, chat name,
 *                    etc.)
 * @param account     The account the conversation is occurring on
 * @param conv        The conversation being logged
 * @param time        The time this conversation started
 * @param tm          The time this conversation started, with timezone data,
 *                    if available and if struct tm has the BSD timezone fields.
 * @return            The new log
 */
GaimLog *gaim_log_new(GaimLogType type, const char *name, GaimAccount *account,
                      GaimConversation *conv, time_t time, const struct tm *tm);

/**
 * Frees a log
 *
 * @param log         The log to destroy
 */
void gaim_log_free(GaimLog *log);

/**
 * Writes to a log file. Assumes you have checked preferences already.
 *
 * @param log          The log to write to
 * @param type         The type of message being logged
 * @param from         Whom this message is coming from, or @c NULL for
 *                     system messages
 * @param time         A timestamp in UNIX time
 * @param message      The message to log
 */
void gaim_log_write(GaimLog *log,
		    GaimMessageFlags type,
		    const char *from,
		    time_t time,
		    const char *message);

/**
 * Reads from a log
 *
 * @param log   The log to read from
 * @param flags The returned logging flags.
 *
 * @return The contents of this log in Gaim Markup.
 */
char *gaim_log_read(GaimLog *log, GaimLogReadFlags *flags);

/**
 * Returns a list of all available logs
 *
 * @param type                The type of the log
 * @param name                The name of the log
 * @param account             The account
 * @return                    A sorted list of GaimLogs
 */
GList *gaim_log_get_logs(GaimLogType type, const char *name, GaimAccount *account);

/**
 * Returns a GHashTable of GaimLogSets.
 *
 * A "log set" here means the information necessary to gather the
 * GaimLogs for a given buddy/chat. This information would be passed
 * to gaim_log_list to get a list of GaimLogs.
 *
 * The primary use of this function is to get a list of everyone the
 * user has ever talked to (assuming he or she uses logging).
 *
 * The GHashTable that's returned will free all log sets in it when
 * destroyed. If a GaimLogSet is removed from the GHashTable, it
 * must be freed with gaim_log_set_free().
 *
 * @return A GHashTable of all available unique GaimLogSets
 */
GHashTable *gaim_log_get_log_sets(void);

/**
 * Returns a list of all available system logs
 *
 * @param account The account
 * @return        A sorted list of GaimLogs
 */
GList *gaim_log_get_system_logs(GaimAccount *account);

/**
 * Returns the size of a log
 *
 * @param log                 The log
 * @return                    The size of the log, in bytes
 */
int gaim_log_get_size(GaimLog *log);

/**
 * Returns the size, in bytes, of all available logs in this conversation
 *
 * @param type                The type of the log
 * @param name                The name of the log
 * @param account             The account
 * @return                    The size in bytes
 */
int gaim_log_get_total_size(GaimLogType type, const char *name, GaimAccount *account);

/**
 * Tests whether a log is deletable
 *
 * A return value of @c FALSE indicates that gaim_log_delete() will fail on this
 * log, unless something changes between the two calls.  A return value of @c TRUE,
 * however, does not guarantee the log can be deleted.
 *
 * @param log                 The log
 * @return                    A boolean indicating if the log is deletable
 */
gboolean gaim_log_is_deletable(GaimLog *log);

/**
 * Deletes a log
 *
 * @param log                 The log
 * @return                    A boolean indicating success or failure
 */
gboolean gaim_log_delete(GaimLog *log);

/**
 * Returns the default logger directory Gaim uses for a given account
 * and username.  This would be where Gaim stores logs created by
 * the built-in text or HTML loggers.
 *
 * @param type                The type of the log.
 * @param name                The name of the log.
 * @param account             The account.
 * @return                    The default logger directory for Gaim.
 */
char *gaim_log_get_log_dir(GaimLogType type, const char *name, GaimAccount *account);

/**
 * Implements GCompareFunc for GaimLogs
 *
 * @param y                   A GaimLog
 * @param z                   Another GaimLog
 * @return                    A value as specified by GCompareFunc
 */
gint gaim_log_compare(gconstpointer y, gconstpointer z);

/**
 * Implements GCompareFunc for GaimLogSets
 *
 * @param y                   A GaimLogSet
 * @param z                   Another GaimLogSet
 * @return                    A value as specified by GCompareFunc
 */
gint gaim_log_set_compare(gconstpointer y, gconstpointer z);

/**
 * Frees a log set
 *
 * @param set         The log set to destroy
 */
void gaim_log_set_free(GaimLogSet *set);

/*@}*/

/******************************************/
/** @name Common Logger Functions         */
/******************************************/
/*@{*/

/**
 * Opens a new log file in the standard Gaim log location
 * with the given file extension, named for the current time,
 * for writing.  If a log file is already open, the existing
 * file handle is retained.  The log's logger_data value is
 * set to a GaimLogCommonLoggerData struct containing the log
 * file handle and log path.
 *
 * This function is intended to be used as a "common"
 * implementation of a logger's @c write function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param log   The log to write to.
 * @param ext   The file extension to give to this log file.
 */
void gaim_log_common_writer(GaimLog *log, const char *ext);

/**
 * Returns a sorted GList of GaimLogs of the requested type.
 *
 * This function should only be used with logs that are written
 * with gaim_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's @c list function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param type     The type of the logs being listed.
 * @param name     The name of the log.
 * @param account  The account of the log.
 * @param ext      The file extension this log format uses.
 * @param logger   A reference to the logger struct for this log.
 *
 * @return A sorted GList of GaimLogs matching the parameters.
 */
GList *gaim_log_common_lister(GaimLogType type, const char *name,
							  GaimAccount *account, const char *ext,
							  GaimLogLogger *logger);

/**
 * Returns the total size of all the logs for a given user, with
 * a given extension.
 *
 * This function should only be used with logs that are written
 * with gaim_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's @c total_size function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param type     The type of the logs being sized.
 * @param name     The name of the logs to size
 *                 (e.g. the username or chat name).
 * @param account  The account of the log.
 * @param ext      The file extension this log format uses.
 *
 * @return The size of all the logs with the specified extension
 *         for the specified user.
 */
int gaim_log_common_total_sizer(GaimLogType type, const char *name,
								GaimAccount *account, const char *ext);

/**
 * Returns the size of a given GaimLog.
 *
 * This function should only be used with logs that are written
 * with gaim_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's @c size function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param log      The GaimLog to size.
 *
 * @return An integer indicating the size of the log in bytes.
 */
int gaim_log_common_sizer(GaimLog *log);

/**
 * Deletes a log
 *
 * This function should only be used with logs that are written
 * with gaim_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's @c delete function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param log      The GaimLog to delete.
 *
 * @return A boolean indicating success or failure.
 */
gboolean gaim_log_common_deleter(GaimLog *log);

/**
 * Checks to see if a log is deletable
 *
 * This function should only be used with logs that are written
 * with gaim_log_common_writer().  It's intended to be used as
 * a "common" implementation of a logger's @c is_deletable function.
 * It should only be passed to gaim_log_logger_new() and never
 * called directly.
 *
 * @param log      The GaimLog to check.
 *
 * @return A boolean indicating if the log is deletable.
 */
gboolean gaim_log_common_is_deletable(GaimLog *log);

/*@}*/

/******************************************/
/** @name Logger Functions                */
/******************************************/
/*@{*/

/**
 * Creates a new logger
 *
 * @param id           The logger's id.
 * @param name         The logger's name.
 * @param functions    The number of functions being passed. The following
 *                     functions are currently available (in order): @c create,
 *                     @c write, @c finalize, @c list, @c read, @c size,
 *                     @c total_size, @c list_syslog, @c get_log_sets,
 *                     @c delete, @c is_deletable.
 *                     For details on these functions, see GaimLogLogger.
 *                     Functions may not be skipped. For example, passing
 *                     @c create and @c write is acceptable (for a total of
 *                     two functions). Passing @c create and @c finalize,
 *                     however, is not. To accomplish that, the caller must
 *                     pass @c create, @c NULL (a placeholder for @c write),
 *                     and @c finalize (for a total of 3 functions).
 *
 * @return The new logger
 */
GaimLogLogger *gaim_log_logger_new(const char *id, const char *name, int functions, ...);

/**
 * Frees a logger
 *
 * @param logger       The logger to free
 */
void gaim_log_logger_free(GaimLogLogger *logger);

/**
 * Adds a new logger
 *
 * @param logger       The new logger to add
 */
void gaim_log_logger_add (GaimLogLogger *logger);

/**
 *
 * Removes a logger
 *
 * @param logger       The logger to remove
 */
void gaim_log_logger_remove (GaimLogLogger *logger);

/**
 *
 * Sets the current logger
 *
 * @param logger       The logger to set
 */
void gaim_log_logger_set (GaimLogLogger *logger);

/**
 *
 * Returns the current logger
 *
 * @return logger      The current logger
 */
GaimLogLogger *gaim_log_logger_get (void);

/**
 * Returns a GList containing the IDs and names of the registered
 * loggers.
 *
 * @return The list of IDs and names.
 */
GList *gaim_log_logger_get_options(void);

/**************************************************************************/
/** @name Log Subsystem                                                   */
/**************************************************************************/
/*@{*/

/**
 * Initializes the log subsystem.
 */
void gaim_log_init(void);

/**
 * Returns the log subsystem handle.
 *
 * @return The log subsystem handle.
 */
void *gaim_log_get_handle(void);

/**
 * Uninitializes the log subsystem.
 */
void gaim_log_uninit(void);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_LOG_H_ */
