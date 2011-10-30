/**
 * @file win32-resolver.h
 *
 * purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#ifndef _PURPLE_GG_WIN32_RESOLVER
#define _PURPLE_GG_WIN32_RESOLVER

/**
 * Starts hostname resolving in new win32 thread.
 *
 * @param fd           Pointer to variable, where pipe descriptor will be saved.
 * @param private_data Pointer to variable, where pointer to private data will
 *                     be saved.
 * @param hostname     Hostname to resolve.
 */
int ggp_resolver_win32thread_start(int *fd, void **private_data,
	const char *hostname);

/**
 * Cleans up resources after hostname resolving.
 *
 * @param private_data Pointer to variable storing pointer to private data.
 * @param force        TRUE, if resources should be cleaned up even, if
 *                     resolving process didn't finished.
 */
void ggp_resolver_win32thread_cleanup(void **private_data, int force);

#endif /* _PURPLE_GG_WIN32_RESOLVER */

/* vim: set ts=8 sts=0 sw=8 noet: */
