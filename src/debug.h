/**
 * @file debug.h Debug API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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

#include <stdarg.h>

/**
 * Debug levels.
 */
typedef enum
{
	GAIM_DEBUG_ALL = 0,  /**< All debug levels.              */
	GAIM_DEBUG_MISC,  /**< General chatter.               */
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
				  const char *format, va_list args);

} GaimDebugUiOps;

/**
 * Outputs debug information.
 *
 * This differs from gaim_debug() in that it takes a va_list.
 *
 * @param level    The debug level.
 * @param category The category (or @c NULL).
 * @param format   The format string.
 * @param args     The format parameters.
 *
 * @see gaim_debug()
 */
void gaim_debug_vargs(GaimDebugLevel level, const char *category,
					  const char *format, va_list args);

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
 * Outputs debug information.
 *
 * @deprecated This has been replaced with gaim_debug(), and will be
 *             removed in a future release.
 *
 * @param fmt The format string.
 *
 * @see gaim_debug()
 */
void debug_printf(const char *fmt, ...);

/**
 * Sets the UI operations structure to be used when outputting debug
 * information.
 *
 * @param ops The UI operations structure.
 */
void gaim_set_debug_ui_ops(GaimDebugUiOps *ops);

/**
 * Returns the UI operations structure used when outputting debug
 * information.
 *
 * @return The UI operations structure in use.
 */
GaimDebugUiOps *gaim_get_debug_ui_ops(void);

#endif /* _GAIM_DEBUG_H_ */
