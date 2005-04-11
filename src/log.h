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

/**
 * A log logger.
 *
 * This struct gets filled out and is included in the GaimLog.  It contains everything
 * needed to write and read from logs.
 */
/*@{*/
struct _GaimLogLogger {
	char *name;               /**< The logger's name */
	char *id;                 /**< an identifier to refer to this logger */

	/** This gets called when the log is first created.
	    I don't think this is actually needed. */
	void (*create)(GaimLog *log);

	/** This is used to write to the log file */
	void (*write)(GaimLog *log,
		     GaimMessageFlags type,
		     const char *from,
		     time_t time,
		     const char *message);

	/** Called when the log is destroyed */
	void (*finalize)(GaimLog *log);

	/** This function returns a sorted GList of available GaimLogs */
	GList *(*list)(GaimLogType type, const char *name, GaimAccount *account);

	/** Given one of the logs returned by the logger's list function,
	 * this returns the contents of the log in GtkIMHtml markup */
	char *(*read)(GaimLog *log, GaimLogReadFlags *flags);

	/** Given one of the logs returned by the logger's list function,
	 * this returns the size of the log in bytes */
	int (*size)(GaimLog *log);

	/** Returns the total size of all the logs. If this is undefined a default
	 * implementation is used */
	int (*total_size)(GaimLogType type, const char *name, GaimAccount *account);

	/** This function returns a sorted GList of available system GaimLogs */
	GList *(*list_syslog)(GaimAccount *account);
};

/**
 * A log.  Not the wooden type.
 */
struct _GaimLog {
	GaimLogType type;                     /**< The type of log this is */
	char *name;                           /**< The name of this log */
	GaimAccount *account;                 /**< The account this log is taking
											   place on */
	time_t time;                          /**< The time this conversation
											   started */
	GaimLogLogger *logger;                /**< The logging mechanism this log
											   is to use */
	void *logger_data;                    /**< Data used by the log logger */
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
 * @param name        The name of this conversation (Screenname, chat name,
 *                    etc.)
 * @param account     The account the conversation is occurring on
 * @param time        The time this conversation started
 * @return            The new log
 */
GaimLog *gaim_log_new(GaimLogType type, const char *name,
		GaimAccount *account, time_t time);

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
 * @param from         Whom this message is coming from, or NULL for
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
 * Returns a list of all available system logs
 *
 * @param account The account
 * @return		A sorted list of GaimLogs
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
 * Implements GCompareFunc
 *
 * @param y				   A GaimLog
 * @param z				   Another GaimLog
 * @return					A value as specified by GCompareFunc
 */
gint gaim_log_compare(gconstpointer y, gconstpointer z);
/*@}*/

/******************************************/
/** @name Logger Functions                */
/******************************************/
/*@{*/

/**
 * Creates a new logger
 *
 * @param create   The logger's new function.
 * @param write    The logger's write function.
 * @param finalize The logger's finalize function.
 * @param list     The logger's list function.
 * @param read     The logger's read function.
 * @param size     The logger's size function.
 *
 * @return The new logger
 */
GaimLogLogger *gaim_log_logger_new(
				void(*create)(GaimLog *),
				void(*write)(GaimLog *, GaimMessageFlags, const char *, time_t, const char *),
				void(*finalize)(GaimLog *),
				GList*(*list)(GaimLogType type, const char*, GaimAccount*),
				char*(*read)(GaimLog*, GaimLogReadFlags*),
				int(*size)(GaimLog*));
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
 * Returns a GList containing the IDs and Names of the registered log
 * loggers.
 *
 * @return The list of IDs and names.
 */
GList *gaim_log_logger_get_options(void);

void gaim_log_init(void);
/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_LOG_H_ */
