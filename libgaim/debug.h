/**
 * @file debug.h Debug API
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
#ifndef _GAIM_DEBUG_H_
#define _GAIM_DEBUG_H_

#include <glib.h>
#include <stdarg.h>

/**
 * Debug levels.
 */
typedef enum
{
	GAIM_DEBUG_ALL = 0,  /**< All debug levels.              */
	GAIM_DEBUG_MISC,     /**< General chatter.               */
	GAIM_DEBUG_INFO,     /**< General operation Information. */
	GAIM_DEBUG_WARNING,  /**< Warnings.                      */
	GAIM_DEBUG_ERROR,    /**< Errors.                        */
	GAIM_DEBUG_FATAL     /**< Fatal errors.                  */

} GaimDebugLevel;

/**
 * Debug UI operations.
 */
typedef struct
{
	void (*print)(GaimDebugLevel level, const char *category,
				  const char *arg_s);
} GaimDebugUiOps;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Debug API                                                       */
/**************************************************************************/
/**
 * Outputs debug information.
 *
 * @param level    The debug level.
 * @param category The category (or @c NULL).
 * @param format   The format string.
 */
void gaim_debug(GaimDebugLevel level, const char *category,
				const char *format, ...);

/**
 * Outputs misc. level debug information.
 *
 * This is a wrapper for gaim_debug(), and uses GAIM_DEBUG_MISC as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see gaim_debug()
 */
void gaim_debug_misc(const char *category, const char *format, ...);

/**
 * Outputs info level debug information.
 *
 * This is a wrapper for gaim_debug(), and uses GAIM_DEBUG_INFO as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see gaim_debug()
 */
void gaim_debug_info(const char *category, const char *format, ...);

/**
 * Outputs warning level debug information.
 *
 * This is a wrapper for gaim_debug(), and uses GAIM_DEBUG_WARNING as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see gaim_debug()
 */
void gaim_debug_warning(const char *category, const char *format, ...);

/**
 * Outputs error level debug information.
 *
 * This is a wrapper for gaim_debug(), and uses GAIM_DEBUG_ERROR as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see gaim_debug()
 */
void gaim_debug_error(const char *category, const char *format, ...);

/**
 * Outputs fatal error level debug information.
 *
 * This is a wrapper for gaim_debug(), and uses GAIM_DEBUG_ERROR as
 * the level.
 *
 * @param category The category (or @c NULL).
 * @param format   The format string.
 *
 * @see gaim_debug()
 */
void gaim_debug_fatal(const char *category, const char *format, ...);

/**
 * Enable or disable printing debug output to the console.
 *
 * @param enabled TRUE to enable debug output or FALSE to disable it.
 */
void gaim_debug_set_enabled(gboolean enabled);

/**
 * Check if console debug output is enabled.
 *
 * @return TRUE if debuggin is enabled, FALSE if it is not.
 */
gboolean gaim_debug_is_enabled(void);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when outputting debug
 * information.
 *
 * @param ops The UI operations structure.
 */
void gaim_debug_set_ui_ops(GaimDebugUiOps *ops);

/**
 * Returns the UI operations structure used when outputting debug
 * information.
 *
 * @return The UI operations structure in use.
 */
GaimDebugUiOps *gaim_debug_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Debug Subsystem                                                 */
/**************************************************************************/
/*@{*/

/**
 * Initializes the debug subsystem.
 */
void gaim_debug_init(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_DEBUG_H_ */
