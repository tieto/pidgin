/**
 * @file debug.c Debug API
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
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

static GaimDebugUiOps *debug_ui_ops = NULL;

void
gaim_debug_vargs(GaimDebugLevel level, const char *category,
				 const char *format, va_list args)
{
	GaimDebugUiOps *ops;

	g_return_if_fail(level != GAIM_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	ops = gaim_debug_get_ui_ops();

	if (ops != NULL && ops->print != NULL)
		ops->print(level, category, format, args);
}

void
gaim_debug(GaimDebugLevel level, const char *category,
		   const char *format, ...)
{
	va_list args;

	g_return_if_fail(level != GAIM_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(level, category, format, args);
	va_end(args);
}

void
gaim_debug_misc(const char *category, const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(GAIM_DEBUG_MISC, category, format, args);
	va_end(args);
}

void
gaim_debug_info(const char *category, const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(GAIM_DEBUG_INFO, category, format, args);
	va_end(args);
}

void
gaim_debug_warning(const char *category, const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(GAIM_DEBUG_WARNING, category, format, args);
	va_end(args);
}

void
gaim_debug_error(const char *category, const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(GAIM_DEBUG_ERROR, category, format, args);
	va_end(args);
}

void
gaim_debug_fatal(const char *category, const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(GAIM_DEBUG_FATAL, category, format, args);
	va_end(args);
}

void
gaim_debug_set_ui_ops(GaimDebugUiOps *ops)
{
	debug_ui_ops = ops;
}

GaimDebugUiOps *
gaim_debug_get_ui_ops(void)
{
	return debug_ui_ops;
}
