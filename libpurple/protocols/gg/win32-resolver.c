/**
 * @file win32-resolver.c
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

#include "win32-resolver.h"

#include <errno.h>
#include <resolver.h>
#include "debug.h"

#ifndef _WIN32
#error "win32thread resolver is not supported on current platform"
#endif

/**
 * Deal with the fact that you can't select() on a win32 file fd.
 * This makes it practically impossible to tie into purple's event loop.
 *
 * -This is thanks to Tor Lillqvist.
 */
static int ggp_resolver_win32thread_socket_pipe(int *fds)
{
	SOCKET temp, socket1 = -1, socket2 = -1;
	struct sockaddr_in saddr;
	int len;
	u_long arg;
	fd_set read_set, write_set;
	struct timeval tv;

	purple_debug_misc("gg", "ggp_resolver_win32thread_socket_pipe(&%d)\n",
		*fds);

	temp = socket(AF_INET, SOCK_STREAM, 0);

	if (temp == INVALID_SOCKET) {
		goto out0;
	}

	arg = 1;
	if (ioctlsocket(temp, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out0;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = 0;
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(temp, (struct sockaddr *)&saddr, sizeof (saddr))) {
		goto out0;
	}

	if (listen(temp, 1) == SOCKET_ERROR) {
		goto out0;
	}

	len = sizeof(saddr);
	if (getsockname(temp, (struct sockaddr *)&saddr, &len)) {
		goto out0;
	}

	socket1 = socket(AF_INET, SOCK_STREAM, 0);

	if (socket1 == INVALID_SOCKET) {
		goto out0;
	}

	arg = 1;
	if (ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out1;
	}

	if (connect(socket1, (struct sockaddr *)&saddr, len) != SOCKET_ERROR ||
			WSAGetLastError() != WSAEWOULDBLOCK) {
		goto out1;
	}

	FD_ZERO(&read_set);
	FD_SET(temp, &read_set);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(0, &read_set, NULL, NULL, NULL) == SOCKET_ERROR) {
		goto out1;
	}

	if (!FD_ISSET(temp, &read_set)) {
		goto out1;
	}

	socket2 = accept(temp, (struct sockaddr *) &saddr, &len);
	if (socket2 == INVALID_SOCKET) {
		goto out1;
	}

	FD_ZERO(&write_set);
	FD_SET(socket1, &write_set);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(0, NULL, &write_set, NULL, NULL) == SOCKET_ERROR) {
		goto out2;
	}

	if (!FD_ISSET(socket1, &write_set)) {
		goto out2;
	}

	arg = 0;
	if (ioctlsocket(socket1, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out2;
	}

	arg = 0;
	if (ioctlsocket(socket2, FIONBIO, &arg) == SOCKET_ERROR) {
		goto out2;
	}

	fds[0] = socket1;
	fds[1] = socket2;

	closesocket (temp);

	return 0;

out2:
	closesocket (socket2);
out1:
	closesocket (socket1);
out0:
	closesocket (temp);
	errno = EIO; /* XXX */

	return -1;
}

struct ggp_resolver_win32thread_data {
	char *hostname;
	int fd;
};

static DWORD WINAPI ggp_resolver_win32thread_thread(LPVOID arg)
{
	struct ggp_resolver_win32thread_data *data = arg;
	struct in_addr addr_ip[2], *addr_list;
	int addr_count;

	purple_debug_info("gg", "ggp_resolver_win32thread_thread() host: %s, "
		"fd: %i called\n", data->hostname, data->fd);

	if ((addr_ip[0].s_addr = inet_addr(data->hostname)) == INADDR_NONE) {
		/* W przypadku błędu gg_gethostbyname_real() zwróci -1
		 * i nie zmieni &addr. Tam jest już INADDR_NONE,
		 * więc nie musimy robić nic więcej. */
		if (gg_gethostbyname_real(data->hostname, &addr_list,
			&addr_count, 0) == -1) {
			addr_list = addr_ip;
		}
	} else {
		addr_list = addr_ip;
		addr_ip[1].s_addr = INADDR_NONE;
		addr_count = 1;
	}

	purple_debug_misc("gg", "ggp_resolver_win32thread_thread() "
		"count = %d\n", addr_count);

	write(data->fd, addr_list, (addr_count+1) * sizeof(struct in_addr));
	close(data->fd);

	free(data->hostname);
	data->hostname = NULL;

	free(data);

	if (addr_list != addr_ip)
		free(addr_list);

	purple_debug_misc("gg", "ggp_resolver_win32thread_thread() done\n");

	return 0;
}


int ggp_resolver_win32thread_start(int *fd, void **private_data,
	const char *hostname)
{
	struct ggp_resolver_win32thread_data *data = NULL;
	HANDLE h;
	DWORD dwTId;
	int pipes[2], new_errno;

	purple_debug_info("gg", "ggp_resolver_win32thread_start(%p, %p, "
		"\"%s\");\n", fd, private_data, hostname);

	if (!private_data || !fd || !hostname) {
		purple_debug_error("gg", "ggp_resolver_win32thread_start() "
			"invalid arguments\n");
		errno = EFAULT;
		return -1;
	}

	purple_debug_misc("gg", "ggp_resolver_win32thread_start() creating "
		"pipes...\n");

	if (ggp_resolver_win32thread_socket_pipe(pipes) == -1) {
		purple_debug_error("gg", "ggp_resolver_win32thread_start() "
			"unable to create pipes (errno=%d, %s)\n",
			errno, strerror(errno));
		return -1;
	}

	if (!(data = malloc(sizeof(*data)))) {
		purple_debug_error("gg", "ggp_resolver_win32thread_start() out "
			"of memory\n");
		new_errno = errno;
		goto cleanup;
	}

	data->hostname = NULL;

	if (!(data->hostname = strdup(hostname))) {
		purple_debug_error("gg", "ggp_resolver_win32thread_start() out "
			"of memory\n");
		new_errno = errno;
		goto cleanup;
	}

	data->fd = pipes[1];

	purple_debug_misc("gg", "ggp_resolver_win32thread_start() creating "
		"thread...\n");

	h = CreateThread(NULL, 0, ggp_resolver_win32thread_thread, data, 0,
		&dwTId);

	if (h == NULL) {
		purple_debug_error("gg", "ggp_resolver_win32thread_start() "
			"unable to create thread\n");
		new_errno = errno;
		goto cleanup;
	}

	*private_data = h;
	*fd = pipes[0];

	purple_debug_misc("gg", "ggp_resolver_win32thread_start() done\n");

	return 0;

cleanup:
	if (data) {
		free(data->hostname);
		free(data);
	}

	close(pipes[0]);
	close(pipes[1]);

	errno = new_errno;

	return -1;

}

void ggp_resolver_win32thread_cleanup(void **private_data, int force)
{
	struct ggp_resolver_win32thread_data *data;

	purple_debug_info("gg", "ggp_resolver_win32thread_cleanup() force: %i "
		"called\n", force);

	if (private_data == NULL || *private_data == NULL) {
		purple_debug_error("gg", "ggp_resolver_win32thread_cleanup() "
			"private_data: NULL\n");
		return;
	}
	return; /* XXX */

	data = (struct ggp_resolver_win32thread_data*) *private_data;
	purple_debug_misc("gg", "ggp_resolver_win32thread_cleanup() data: "
		"%s called\n", data->hostname);
	*private_data = NULL;

	if (force) {
		purple_debug_misc("gg", "ggp_resolver_win32thread_cleanup() "
			"force called\n");
		//pthread_cancel(data->thread);
		//pthread_join(data->thread, NULL);
	}

	free(data->hostname);
	data->hostname = NULL;

	if (data->fd != -1) {
		close(data->fd);
		data->fd = -1;
	}
	purple_debug_info("gg", "ggp_resolver_win32thread_cleanup() done\n");
	free(data);
}

/* vim: set ts=8 sts=0 sw=8 noet: */
