/*
 *  libc_interface.c
 *  
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: October 14, 2002
 *  Description: Commonly used libc routines.
 */
#include <winsock.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <time.h>
#include "libc_internal.h"

/*
 *  PROTOS
 */
extern void debug_printf(char * fmt, ...);

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
			debug_printf("wgaim_read: getsockopt returned error: %d\n", error);
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

int wgaim_getsockopt(int socket, int level, int optname, void *optval, unsigned int *optlenptr) {
	int ret;

	ret = getsockopt( socket, level, optname, optval, optlenptr );
	if( ret == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		return -1;
	}

	return 0;
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
		debug_printf("wgaim_fcntl: Unsupported command\n");
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

/* From glibc 2.2.5 */
char* wgaim_strsep(char **stringp, const char *delim) {
	char *begin, *end;

	begin = *stringp;
	if (begin == NULL)
		return NULL;
	
	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if (delim[0] == '\0' || delim[1] == '\0') {
		char ch = delim[0];
		
		if (ch == '\0')
			end = NULL;
		else {
			if (*begin == ch)
				end = begin;
			else if (*begin == '\0')
				end = NULL;
			else
				end = strchr (begin + 1, ch);
		}
	}
	else
		/* Find the end of the token.  */
		end = strpbrk (begin, delim);
	
	if (end) {
		/* Terminate the token and set *STRINGP past NUL character.  */
		*end++ = '\0';
		*stringp = end;
	}
	else
		/* No more delimiters; this is the last token.  */
		*stringp = NULL;

	return begin;
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
		else if( ret == 0 ) {
			/* connection has been gracefully closed */
			errno = WSAENOTCONN;
			return -1;
		}
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
	struct _stat oldstat, newstat;

	if(_stat(oldname, &oldstat) == 0) {
		/* newname exists */
		if(_stat(newname, &newstat) == 0) {
			/* oldname is a dir */
			if(_S_ISDIR(oldstat.st_mode)) {
				if(!_S_ISDIR(newstat.st_mode)) {
					return rename(oldname, newname);
				}
				/* newname is a dir */
				else {
					/* This is not quite right.. If newname is empty and
					   is not a sub dir of oldname, newname should be
					   deleted and oldname should be renamed.
					*/
					debug_printf("Warning: wgaim_rename does not behave here as it should\n");
					return rename(oldname, newname);
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
					remove(newname);
					return rename(oldname, newname);
				}
			}
		}
		/* newname doesn't exist */
		else
			return rename(oldname, newname);
	}
	else {
		/* oldname doesn't exist */
		errno = ENOENT;
		return -1;
	}

}
