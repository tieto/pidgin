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
#include "internal.h"
#include "prefs.h"
#include "util.h"

static GaimDebugUiOps *debug_ui_ops = NULL;

/*
 * This determines whether debug info should be written to the
 * console or not.
 *
 * It doesn't make sense to make this a normal Gaim preference
 * because it's a command line option.  This will always be FALSE,
 * unless the user explicitly started Gaim with the -d flag.
 * It doesn't matter what this value was the last time Gaim was
 * started, so it doesn't make sense to save it in prefs.
 */
static gboolean debug_enabled = FALSE;

static void
gaim_debug_vargs(GaimDebugLevel level, const char *category,
				 const char *format, va_list args)
{
	GaimDebugUiOps *ops;
	char *arg_s = NULL;

	g_return_if_fail(level != GAIM_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	ops = gaim_debug_get_ui_ops();

	if (!debug_enabled && ((ops == NULL) || (ops->print == NULL)))
		return;

	arg_s = g_strdup_vprintf(format, args);

	if (debug_enabled) {
		gchar *ts_s;

		if ((category != NULL) &&
			(gaim_prefs_exists("/core/debug/timestamps")) &&
			(gaim_prefs_get_bool("/core/debug/timestamps"))) {
			const char *mdate;

			time_t mtime = time(NULL);
			mdate = gaim_utf8_strftime("%H:%M:%S", localtime(&mtime));
			ts_s = g_strdup_printf("(%s) ", mdate);
		} else {
			ts_s = g_strdup("");
		}

		if (category == NULL)
			g_print("%s%s", ts_s, arg_s);
		else
			g_print("%s%s: %s", ts_s, category, arg_s);

		g_free(ts_s);
	}

	if (ops != NULL && ops->print != NULL)
		ops->print(level, category, arg_s);

	g_free(arg_s);
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
gaim_debug_set_enabled(gboolean enabled)
{
	debug_enabled = enabled;
}

gboolean
gaim_debug_is_enabled()
{
	return debug_enabled;
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

void
gaim_debug_init(void)
{
	gaim_prefs_add_none("/core/debug");

	/*
	 * This pref is currently used by both the console
	 * output and the debug window output.
	 */
	gaim_prefs_add_bool("/core/debug/timestamps", FALSE);
}
