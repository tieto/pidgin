/*
 * gaim
 *
 * File: libc_interface.c
 * Date: October 14, 2002
 * Description: libc interface for Windows api
 * 
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
 *
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <time.h>
#include <glib.h>
#include "debug.h"
#include "libc_internal.h"
#if GLIB_CHECK_VERSION(2,6,0)
# include <glib/gstdio.h>
#else
#define g_remove remove
#define g_rename rename
#define g_stat stat
#endif
/*
 *  PROTOS
 */

/*
 *  LOCALS
 */

static char errbuf[1024];

/*
 *  CODE
 */

/* helpers */
static int wgaim_is_socket( int fd ) {
	int optval;
	unsigned int optlen = sizeof(int);

	if( (getsockopt(fd, SOL_SOCKET, SO_TYPE, (void*)&optval, &optlen)) == SOCKET_ERROR ) {
		int error = WSAGetLastError();
		if( error == WSAENOTSOCK )
			return FALSE;
		else {
                        gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "wgaim_is_socket: getsockopt returned error: %d\n", error);
			return FALSE;
		}
	}
	return TRUE;
}

/* socket.h */
int wgaim_socket (int namespace, int style, int protocol) {
	int ret;

	ret = socket( namespace, style, protocol );

	if( ret == INVALID_SOCKET ) {
		errno = WSAGetLastError();
		return -1;
	}
	return ret;
}

int wgaim_connect(int socket, struct sockaddr *addr, u_long length) {
	int ret;

	ret = connect( socket, addr, length );
	
	if( ret == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		if( errno == WSAEWOULDBLOCK )
			errno = WSAEINPROGRESS;
		return -1;
	}
	return 0;
}

int wgaim_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlenptr) {
	if(getsockopt(socket, level, optname, optval, optlenptr) == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		return -1;
	}
	return 0;
}

int wgaim_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen) {
	if(setsockopt(socket, level, optname, optval, optlen) == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		return -1;
	}
	return 0;
}

int wgaim_getsockname(int socket, struct sockaddr *addr, socklen_t *lenptr) {
        if(getsockname(socket, addr, lenptr) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wgaim_bind(int socket, struct sockaddr *addr, socklen_t length) {
        if(bind(socket, addr, length) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wgaim_listen(int socket, unsigned int n) {
        if(listen(socket, n) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wgaim_sendto(int socket, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
	int ret;
	if ((ret = sendto(socket, buf, len, flags, to, tolen)
			) == SOCKET_ERROR) {
		errno = WSAGetLastError();
		return -1;
	}
	return ret;
}

/* fcntl.h */
/* This is not a full implementation of fcntl. Update as needed.. */
int wgaim_fcntl(int socket, int command, int val) {
	switch( command ) {
	case F_SETFL:
	{
		int ret=0;

		switch( val ) {
		case O_NONBLOCK:
		{
			u_long imode=1;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		case 0:
	        {
			u_long imode=0;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		default:
			errno = EINVAL;
			return -1;
		}/*end switch*/
		if( ret == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	default:
                gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "wgaim_fcntl: Unsupported command\n");
		return -1;
	}/*end switch*/
}

/* sys/ioctl.h */
int wgaim_ioctl(int fd, int command, void* val) {
	switch( command ) {
	case FIONBIO:
	{
		if (ioctlsocket(fd, FIONBIO, (unsigned long *)val) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	case SIOCGIFCONF:
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(fd, SIO_GET_INTERFACE_LIST,
				0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned,
				0, 0) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		} else {
			int i;
			struct ifconf *ifc = val;
			char *tmp = ifc->ifc_buf;
			int nNumInterfaces =
				nBytesReturned / sizeof(INTERFACE_INFO);
			for (i = 0; i < nNumInterfaces; i++) {
				INTERFACE_INFO ii = InterfaceList[i];
				struct ifreq *ifr = (struct ifreq *) tmp;
				struct sockaddr_in *sa = (struct sockaddr_in *) &ifr->ifr_addr;

				sa->sin_family = ii.iiAddress.AddressIn.sin_family;
				sa->sin_port = ii.iiAddress.AddressIn.sin_port;
				sa->sin_addr.s_addr = ii.iiAddress.AddressIn.sin_addr.s_addr;
				tmp += sizeof(struct ifreq);

				/* Make sure that we can fit in the original buffer */
				if (tmp >= (ifc->ifc_buf + ifc->ifc_len + sizeof(struct ifreq))) {
					break;
				}
			}
			/* Replace the length with the actually used length */
			ifc->ifc_len = ifc->ifc_len - (ifc->ifc_buf - tmp);
			return 0;
		}
	}
	default:
		errno = EINVAL;
		return -1;
	}/*end switch*/
}

/* arpa/inet.h */
int wgaim_inet_aton(const char *name, struct in_addr *addr) {
	if((addr->s_addr = inet_addr(name)) == INADDR_NONE)
		return 0;
	else
		return 1;
}

/* netdb.h */
struct hostent* wgaim_gethostbyname(const char *name) {
	struct hostent *hp;

	if((hp = gethostbyname(name)) == NULL) {
		errno = WSAGetLastError();
		return NULL;
	}
	return hp;
}

/* string.h */
char* wgaim_strerror( int errornum ) {
	if( errornum > WSABASEERR ) {
		sprintf( errbuf, "Windows socket error #%d", errornum );
		return errbuf;
	}
	else
		return strerror( errornum );
}

/* unistd.h */

/*
 *  We need to figure out whether fd is a file or socket handle.
 */
int wgaim_read(int fd, void *buf, unsigned int size) {
	int ret;

	if( wgaim_is_socket(fd) ) {
		if( (ret = recv(fd, buf, size, 0)) == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
#if 0
		else if( ret == 0 ) {
			/* connection has been gracefully closed */
			errno = WSAENOTCONN;
			return -1;
		}
#endif
		else {
			/* success reading socket */
			return ret;
		}
	}
	else {
		/* fd is not a socket handle.. pass it off to read */
		return read(fd, buf, size);
	}
}

int wgaim_write(int fd, const void *buf, unsigned int size) {
	int ret;

	if( wgaim_is_socket(fd) ) {
		if( (ret = send(fd, buf, size, 0)) == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		else {
			/* success */
			return ret;
		}
	}
	else
		return write(fd, buf, size);
}

int wgaim_recv(int fd, void *buf, size_t len, int flags) {
	int ret;

	if ((ret = recv(fd, buf, len, flags)) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
	} else {
		return ret;
	}
}

int wgaim_close(int fd) {
	int ret;

	if( wgaim_is_socket(fd) ) {
		if( (ret = closesocket(fd)) == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		else
			return 0;
	}
	else
		return close(fd);
}

int wgaim_gethostname(char *name, size_t size) {
        if(gethostname(name, size) == SOCKET_ERROR) {
                errno = WSAGetLastError();
			return -1;
        }
        return 0;
}

/* sys/time.h */

int wgaim_gettimeofday(struct timeval *p, struct timezone *z) {
	int res = 0;
	struct _timeb timebuffer;

	if (z != 0) {
		_tzset();
		z->tz_minuteswest = _timezone/60;
		z->tz_dsttime = _daylight;
	}
	
	if (p != 0) {
		_ftime(&timebuffer);
	   	p->tv_sec = timebuffer.time;			/* seconds since 1-1-1970 */
		p->tv_usec = timebuffer.millitm*1000; 	/* microseconds */
	}

	return res;
}

/* stdio.h */

int wgaim_rename (const char *oldname, const char *newname) {
	struct stat oldstat, newstat;

	if(g_stat(oldname, &oldstat) == 0) {
		/* newname exists */
		if(g_stat(newname, &newstat) == 0) {
			/* oldname is a dir */
			if(_S_ISDIR(oldstat.st_mode)) {
				if(!_S_ISDIR(newstat.st_mode)) {
					return g_rename(oldname, newname);
				}
				/* newname is a dir */
				else {
					/* This is not quite right.. If newname is empty and
					   is not a sub dir of oldname, newname should be
					   deleted and oldname should be renamed.
					*/
					gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "wgaim_rename does not behave here as it should\n");
					return g_rename(oldname, newname);
				}
			}
			/* oldname is not a dir */
			else {
				/* newname is a dir */
				if(_S_ISDIR(newstat.st_mode)) {
					errno = EISDIR;
					return -1;
				}
				/* newname is not a dir */
				else {
					g_remove(newname);
					return g_rename(oldname, newname);
				}
			}
		}
		/* newname doesn't exist */
		else
			return g_rename(oldname, newname);
	}
	else {
		/* oldname doesn't exist */
		errno = ENOENT;
		return -1;
	}

}

/* time.h */

struct tm * wgaim_localtime_r (const time_t *time, struct tm *resultp) {
	struct tm* tmptm;

	if(!time)
		return NULL;
	tmptm = localtime(time);
	if(resultp && tmptm)
		return memcpy(resultp, tmptm, sizeof(struct tm));
	else
		return NULL;
}
