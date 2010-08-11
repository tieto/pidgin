/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "Gnt"

#ifdef __GNUC__
# ifndef GNT_LOG_DOMAIN
#  define GNT_LOG_DOMAIN ""
# endif
# define gnt_warning(format, args...)  g_warning("(%s) %s: " format, GNT_LOG_DOMAIN, __PRETTY_FUNCTION__, args)
#else /* __GNUC__ */
# define gnt_warning g_warning
#endif

