/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#else
#include <winsock.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define GAIM_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

char proxyhost[128] = { 0 };
int proxyport = 0;
proxytype_t proxytype = PROXY_NONE;
char proxyuser[128] = { 0 };
char proxypass[128] = { 0 };

struct PHB {
	GaimInputFunction func;
	gpointer data;
	char *host;
	int port;
	gint inpa;
	proxytype_t proxytype;
};

typedef struct _GaimIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;
} GaimIOClosure;

static void gaim_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

	/*
	debug_printf("CLOSURE: callback for %d, fd is %d\n",
		     closure->result, g_io_channel_unix_get_fd(source));
	*/

	closure->function(closure->data, g_io_channel_unix_get_fd(source), gaim_cond);

	return TRUE;
}

gint gaim_input_add(gint source, GaimInputCondition condition, GaimInputFunction function, gpointer data)
{
	GaimIOClosure *closure = g_new0(GaimIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_WRITE_COND;

	channel = g_io_channel_unix_new(source);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_io_invoke, closure, gaim_io_destroy);

	/* debug_printf("CLOSURE: adding input watcher %d for fd %d\n", closure->result, source); */

	g_io_channel_unref(channel);
	return closure->result;
}

void gaim_input_remove(gint tag)
{
	/* debug_printf("CLOSURE: removing input watcher %d\n", tag); */
	if (tag > 0)
		g_source_remove(tag);
}


typedef void (*dns_callback_t)(struct sockaddr *addr, size_t addrlen,
	gpointer data, const char *error_message);

#ifdef __unix__

/*  This structure represents both a pending DNS request and
 *  a free child process.
 */
typedef struct {
	char *host;
	int port;
	int socktype;
	dns_callback_t callback;
	gpointer data;
	gint inpa;
	int fd_in, fd_out;
	pid_t dns_pid;
} pending_dns_request_t;

static GSList *free_dns_children = NULL;
static GQueue *queued_requests = NULL;

static int number_of_dns_children = 0;

const int MAX_DNS_CHILDREN = 2;

typedef struct {
	char hostname[512];
	int port;
	int socktype;
} dns_params_t;

typedef struct {
	dns_params_t params;
	dns_callback_t callback;
	gpointer data;
} queued_dns_request_t;

static int send_dns_request_to_child(pending_dns_request_t *req, dns_params_t *dns_params)
{
	char ch;
	int rc;

	/* Are you alive? */
	if(kill(req->dns_pid, 0) != 0) {
		debug_printf("DNS child %d no longer exists\n", req->dns_pid);
		return -1;
	}
	
	/* Let's contact this lost child! */
	rc = write(req->fd_in, dns_params, sizeof(*dns_params));
	if(rc<0) {
		debug_printf("Error writing to DNS child %d: %s\n", req->dns_pid, strerror(errno));
		return -1;
	}
	
	g_return_val_if_fail(rc == sizeof(*dns_params), -1);
	
	/* Did you hear me? (This avoids some race conditions) */
	rc = read(req->fd_out, &ch, 1);
	if(rc != 1 || ch!='Y') {
		debug_printf("DNS child %d not responding: killing it!\n", req->dns_pid);
		kill(req->dns_pid, SIGKILL);
		return -1;
	}
	
	debug_printf("Successfuly sent DNS request to child %d\n", req->dns_pid);
	return 0;
}

static void host_resolved(gpointer data, gint source, GaimInputCondition cond);

static void release_dns_child(pending_dns_request_t *req)
{
	g_free(req->host);

	if(queued_requests && !g_queue_is_empty(queued_requests)) {
		queued_dns_request_t *r = g_queue_pop_head(queued_requests);
		req->host = g_strdup(r->params.hostname);
		req->port = r->params.port;
		req->socktype = r->params.socktype;
		req->callback = r->callback;
		req->data = r->data;

		debug_printf("Processing queued DNS query for '%s' with child %d\n", req->host, req->dns_pid);

		if(send_dns_request_to_child(req, &(r->params)) != 0) {
			g_free(req->host);
			g_free(req);
			req = NULL;
			number_of_dns_children--;
			
			debug_printf("Intent of process queued query of '%s' failed, requeing...\n",
				r->params.hostname);
			g_queue_push_head(queued_requests, r);
		} else {
			req->inpa = gaim_input_add(req->fd_out, GAIM_INPUT_READ, host_resolved, req);
			g_free(r);
		}
		
	} else {
		req->host = NULL;
		req->callback = NULL;
		req->data = NULL;
		free_dns_children = g_slist_append(free_dns_children, req);
	}
}

static void host_resolved(gpointer data, gint source, GaimInputCondition cond)
{
	pending_dns_request_t *req = (pending_dns_request_t*)data;
	int rc, err;
	struct sockaddr *addr = NULL;
	socklen_t addrlen;

	debug_printf("Host '%s' resolved\n", req->host);
	gaim_input_remove(req->inpa);

	rc=read(req->fd_out, &err, sizeof(err));
	if((rc==4) && (err!=0)) {
		char message[1024];
		g_snprintf(message, sizeof(message), "DNS error: %s (pid=%d)",
#ifdef HAVE_GETADDRINFO
			gai_strerror(err),
#else
			hstrerror(err),
#endif
			req->dns_pid);
		debug_printf("%s\n",message);
		req->callback(NULL, 0, req->data, message);
		release_dns_child(req);	
		return;
	}
	if(rc>0) {
		rc=read(req->fd_out, &addrlen, sizeof(addrlen));
		if(rc>0) {
			addr=g_malloc(addrlen);
			rc=read(req->fd_out, addr, addrlen);
		}
	}
	if(rc==-1) {
		char message[1024];
		g_snprintf(message, sizeof(message), "Error reading from DNS child: %s",strerror(errno));
		debug_printf("%s\n",message);
		close(req->fd_out);
		req->callback(NULL, 0, req->data, message);
		g_free(req->host);
		g_free(req);
		number_of_dns_children--;
		return;
	}
	if(rc==0) {
		char message[1024];
		g_snprintf(message, sizeof(message), "EOF reading from DNS child");
		close(req->fd_out);
		debug_printf("%s\n",message);
		req->callback(NULL, 0, req->data, message);
		g_free(req->host);
		g_free(req);
		number_of_dns_children--;
		return;
	}

/*	wait4(req->dns_pid, NULL, WNOHANG, NULL); */

	req->callback(addr, addrlen, req->data, NULL);
	
	g_free(addr);

	release_dns_child(req);	
}

static void trap_gdb_bug()
{
	const char *message =
		"Gaim's DNS child got a SIGTRAP signal. \n"
		"This can be caused by trying to run gaim into gdb.\n"
		"There's a known gdb bug which prevent this, supposedly gaim\n"
		"should have detected you were using gdb and use an ugly hack,\n"
		"check cope_with_gdb_brokenness() in proxy.c.\n\n"
		"For more info about the bug check http://sources.redhat.com/ml/gdb/2001-07/msg00349.html\n";
	fputs("\n* * *\n",stderr);
	fputs(message,stderr);
	fputs("* * *\n\n",stderr);
	execlp("xmessage","xmessage","-center", message, NULL);
	_exit(1);
}

static void cope_with_gdb_brokenness()
{
	static gboolean already_done = FALSE;
	char s[300], e[300];
	int n;
	pid_t ppid;

#ifdef __linux__
	if(already_done)
		return;
	already_done = TRUE;
	ppid = getppid();
	sprintf(s,"/proc/%d/exe", ppid);
	n = readlink(s, e, sizeof(e));
	e[MAX(n,sizeof(e)-1)] = '\0';
	
	if(strstr(e,"gdb")) {
		debug_printf("Debugger detected, performing useless query...\n");
		gethostbyname("x.x.x.x.x");
	}
#endif
}

int gaim_gethostbyname_async(const char *hostname, int port, int socktype, dns_callback_t callback, gpointer data)
{
	pending_dns_request_t *req = NULL;
	dns_params_t dns_params;
	
	strncpy(dns_params.hostname, hostname, sizeof(dns_params.hostname)-1);
	dns_params.hostname[sizeof(dns_params.hostname)-1] = '\0';
	dns_params.port = port;
	dns_params.socktype = socktype;
	
	/* Is there a free available child? */
	while(free_dns_children && !req) {
		GSList *l = free_dns_children;
		free_dns_children = g_slist_remove_link(free_dns_children, l);
		req = l->data;
		g_slist_free(l);
		
		if(send_dns_request_to_child(req, &dns_params) != 0) {
			g_free(req);
			req = NULL;
			number_of_dns_children--;
			continue;
		}
		
	}

	if(!req) {
		int child_out[2], child_in[2];

		if(number_of_dns_children >= MAX_DNS_CHILDREN) {
			queued_dns_request_t *r = g_new(queued_dns_request_t, 1);
			memcpy(&(r->params), &dns_params, sizeof(dns_params));
			r->callback = callback;
			r->data = data;
			if(!queued_requests)
				queued_requests = g_queue_new();
			g_queue_push_tail(queued_requests, r);
			debug_printf("DNS query for '%s' queued\n", hostname);
			return 0;
		}

		if(pipe(child_out) || pipe(child_in)) {
			debug_printf("Could not create pipes: %s",strerror(errno));
			return -1;
		}

		/* We need to create a new child. */
		req = g_new(pending_dns_request_t,1);
		
		cope_with_gdb_brokenness();
	
		req->dns_pid=fork();
		if(req->dns_pid==0) {
			const int zero = 0;
			int rc;

#ifdef HAVE_GETADDRINFO
			struct addrinfo hints, *res;
			char servname[20];
#else
			struct sockaddr_in sin;
			const socklen_t addrlen = sizeof(sin);
#endif
#ifdef HAVE_SIGNAL_H
			signal(SIGHUP, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGTRAP, trap_gdb_bug);
#endif


			close(child_out[0]);
			close(child_in[1]);

			while(1) {
				if(dns_params.hostname[0] == '\0') {
					const char Y = 'Y';
					fd_set fds;
					struct timeval tv = { .tv_sec = 40 , .tv_usec = 0 };
					FD_ZERO(&fds);
					FD_SET(child_in[0], &fds);
					rc = select(child_in[0]+1, &fds, NULL, NULL, &tv);
					if(!rc) {
						if(opt_debug)
							fprintf(stderr,"dns[%d]: nobody needs me... =(\n", getpid());
						break;
					}
					rc = read(child_in[0], &dns_params, sizeof(dns_params));
					if(rc < 0) {
						perror("read()");
						break;
					}
					if(rc==0) {
						if(opt_debug)
							fprintf(stderr,"dns[%d]: Ops, father has gone, wait for me, wait...!\n", getpid());
						_exit(0);
					}
					if(dns_params.hostname[0] == '\0') {
						fprintf(stderr, "dns[%d]: hostname = \"\" (port = %d)!!!\n", getpid(), dns_params.port);
						_exit(1);
					}
					write(child_out[1], &Y, 1);
				}

#ifdef HAVE_GETADDRINFO
				g_snprintf(servname, sizeof(servname), "%d", dns_params.port);
				memset(&hints,0,sizeof(hints));
				hints.ai_socktype = dns_params.socktype;
				rc = getaddrinfo(dns_params.hostname, servname, &hints, &res);
				if(rc) {
					write(child_out[1], &rc, sizeof(int));
					close(child_out[1]);
					if(opt_debug)
						fprintf(stderr,"dns[%d] Error: getaddrinfo returned %d\n",
							getpid(), rc);
					dns_params.hostname[0] = '\0';
					continue;
				}
				write(child_out[1], &zero, sizeof(zero));
				write(child_out[1], &(res->ai_addrlen), sizeof(res->ai_addrlen));
				write(child_out[1], res->ai_addr, res->ai_addrlen);
				freeaddrinfo(res);
#else
				if (!inet_aton(hostname, &sin.sin_addr)) {
					struct hostent *hp;
					if(!(hp = gethostbyname(dns_params.hostname))) {
						write(child_out[1], &h_errno, sizeof(int));
						close(child_out[1]);
						if(opt_debug)
							fprintf(stderr,"DNS Error: %s\n",hstrerror(h_errno));
						_exit(0);
					}
					memset(&sin, 0, sizeof(struct sockaddr_in));
					memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
					sin.sin_family = hp->h_addrtype;
				} else
					sin.sin_family = AF_INET;
				sin.sin_port = htons(dns_params.port);
				write(child_out[1], &zero, sizeof(zero));
				write(child_out[1], &addrlen, sizeof(addrlen));
				write(child_out[1], &sin, addrlen);
#endif
				dns_params.hostname[0] = '\0';
			}
			close(child_out[1]);
			close(child_in[0]);
			_exit(0);
		}
		close(child_out[1]);
		close(child_in[0]);
		if(req->dns_pid==-1) {
			debug_printf("Could not create child process for DNS: %s\n",strerror(errno));
			g_free(req);
			return -1;
		}
		req->fd_in = child_in[1];
		req->fd_out = child_out[0];
		number_of_dns_children++;
		debug_printf("Created new DNS child %d, there are now %d children.\n",
			req->dns_pid, number_of_dns_children);
	}
	req->host=g_strdup(hostname);
	req->port=port;
	req->callback=callback;
	req->data=data;
	req->inpa = gaim_input_add(req->fd_out, GAIM_INPUT_READ, host_resolved, req);
	return 0;
}
#else

typedef struct {
	gpointer data;
	size_t addrlen;
	struct sockaddr *addr;
	dns_callback_t callback;
} pending_dns_request_t;

static gboolean host_resolved(gpointer data)
{
	pending_dns_request_t *req = (pending_dns_request_t*)data;
	req->callback(req->addr, req->addrlen, req->data, NULL);
	g_free(req->addr);
	g_free(req);
	return FALSE;
}

int gaim_gethostbyname_async(const char *hostname, int port, int socktype, dns_callback_t callback, gpointer data)
{
	struct sockaddr_in sin;
	pending_dns_request_t *req;

	if (!inet_aton(hostname, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(hostname))) {
			debug_printf("gaim_gethostbyname(\"%s\", %d) failed: %s",
				     hostname, port, hstrerror(h_errno));
			return -1;
		}
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
	} else
		sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	req = g_new(pending_dns_request_t, 1);
	req->addr = (struct sockaddr*) g_memdup(&sin, sizeof(sin));
	req->addrlen = sizeof(sin);
	req->data = data;
	req->callback = callback;
	g_timeout_add(10, host_resolved, req);
	return 0;
}

#endif

static void no_one_calls(gpointer data, gint source, GaimInputCondition cond)
{
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");

	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		gaim_input_remove(phb->inpa);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);
	gaim_input_remove(phb->inpa);
	phb->func(phb->data, source, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
}

static gboolean clean_connect(gpointer data)
{
	struct PHB *phb = data;

	phb->func(phb->data, phb->port, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);

	return FALSE;
}


static int proxy_connect_none(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	debug_printf("connecting to %s:%d with no proxy\n", phb->host, phb->port);

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		debug_printf("unable to create socket: %s\n", strerror(errno));
		g_free(phb->host);
		g_free(phb);
		return -1;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, (struct sockaddr *)addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, no_one_calls, phb);
		} else {
			debug_printf("connect failed (errno %d)\n", errno);
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;
		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			debug_printf("getsockopt failed\n");
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		phb->port = fd;	/* bleh */
		g_timeout_add(50, clean_connect, phb);	/* we do this because we never
							   want to call our callback
							   before we return. */
	}

	return fd;
}

#define HTTP_GOODSTRING "HTTP/1.0 200"
#define HTTP_GOODSTRING2 "HTTP/1.1 200"

static void http_canread(gpointer data, gint source, GaimInputCondition cond)
{
	int nlc = 0;
	int pos = 0;
	int minor, major, status, error=0;
	struct PHB *phb = data;
	char inputline[8192], *p;

	gaim_input_remove(phb->inpa);

	while ((nlc != 2) && (read(source, &inputline[pos++], 1) == 1)) {
		if (inputline[pos - 1] == '\n')
			nlc++;
		else if (inputline[pos - 1] != '\r')
			nlc = 0;
	}
	inputline[pos] = '\0';
	
	error = strncmp(inputline, "HTTP/", 5) != 0;
	if(!error) {
		p = inputline + 5;
		major = strtol(p, &p, 10);
		error = (major==0) || (*p != '.');
		if(!error) {
			p++;
			minor = strtol(p, &p, 10);
			error = (*p!=' ');
			if(!error) {
				p++;
				status = strtol(p, &p, 10);
				error = (*p!=' ');
			}
		}
	}
	
	if(error) {
		debug_printf("Unable to parse proxy's response: %s\n", inputline);
		close(source);
		source=-1;
	} else if(status!=200) {
		debug_printf("Proxy reserver replied: (%s)\n", p);
		close(source);
		source=-1;
	}

	phb->func(phb->data, source, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
	return;
}

static void http_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	char request[8192];
	int request_len = 0;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	request_len = g_snprintf(request, sizeof(request), "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n", phb->host, phb->port,
		   phb->host, phb->port);

	if (proxyuser) {
		char *t1, *t2;
		t1 = g_strdup_printf("%s:%s", proxyuser, proxypass);
		t2 = tobase64(t1);
		g_free(t1);
		g_return_if_fail(request_len < sizeof(request));
		request_len += g_snprintf(request + request_len, sizeof(request) - request_len, "Proxy-Authorization: Basic %s\r\n", t2);
		g_free(t2);
	}

	g_return_if_fail(request_len < sizeof(request));
	strcpy(request + request_len, "\r\n");
	request_len += 2;

	if (write(source, request, request_len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, http_canread, phb);
}

static int proxy_connect_http(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using HTTP\n", phb->host, phb->port, proxyhost, proxyport);

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb->host);
		g_free(phb);
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, http_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		http_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void s4_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);

	memset(packet, 0, sizeof(packet));

	if (read(source, packet, 9) >= 4 && packet[1] == 90) {
		phb->func(phb->data, source, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	close(source);
	phb->func(phb->data, -1, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
}

static void s4_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct hostent *hp;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	/* XXX does socks4 not support host name lookups by the proxy? */
	if (!(hp = gethostbyname(phb->host))) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	packet[0] = 4;
	packet[1] = 1;
	packet[2] = phb->port >> 8;
	packet[3] = phb->port & 0xff;
	packet[4] = (unsigned char)(hp->h_addr_list[0])[0];
	packet[5] = (unsigned char)(hp->h_addr_list[0])[1];
	packet[6] = (unsigned char)(hp->h_addr_list[0])[2];
	packet[7] = (unsigned char)(hp->h_addr_list[0])[3];
	packet[8] = 0;

	if (write(source, packet, 9) != 9) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s4_canread, phb);
}

static int proxy_connect_socks4(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS4\n", phb->host, phb->port, proxyhost, proxyport);

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb->host);
		g_free(phb);
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s4_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		s4_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("able to read again\n");

	if (read(source, buf, 10) < 10) {
		debug_printf("or not...\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		debug_printf("bad data\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->func(phb->data, source, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
	return;
}

static void s5_sendconnect(gpointer data, gint source)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	int hlen = strlen(phb->host);

	buf[0] = 0x05;
	buf[1] = 0x01;		/* CONNECT */
	buf[2] = 0x00;		/* reserved */
	buf[3] = 0x03;		/* address type -- host name */
	buf[4] = hlen;
	memcpy(buf + 5, phb->host, hlen);
	buf[5 + strlen(phb->host)] = phb->port >> 8;
	buf[5 + strlen(phb->host) + 1] = phb->port & 0xff;

	if (write(source, buf, (5 + strlen(phb->host) + 2)) < (5 + strlen(phb->host) + 2)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread_again, phb);
}

static void s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("got auth response\n");

	if (read(source, buf, 2) < 2) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	s5_sendconnect(phb, source);
}

static void s5_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("able to read\n");

	if (read(source, buf, 2) < 2) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if (buf[1] == 0x02) {
		unsigned int i = strlen(proxyuser), j = strlen(proxypass);
		buf[0] = 0x01;	/* version 1 */
		buf[1] = i;
		memcpy(buf + 2, proxyuser, i);
		buf[2 + i] = j;
		memcpy(buf + 2 + i + 1, proxypass, j);

		if (write(source, buf, 3 + i + j) < 3 + i + j) {
			close(source);
			phb->func(phb->data, -1, GAIM_INPUT_READ);
			g_free(phb->host);
			g_free(phb);
			return;
		}

		phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_readauth, phb);
	} else {
		s5_sendconnect(phb, source);
	}
}

static void s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	int i;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */
	if (proxyuser[0]) {
		buf[1] = 0x02;	/* two methods */
		buf[2] = 0x00;	/* no authentication */
		buf[3] = 0x02;	/* username/password authentication */
		i = 4;
	} else {
		buf[1] = 0x01;
		buf[2] = 0x00;
		i = 3;
	}

	if (write(source, buf, i) < i) {
		debug_printf("unable to write\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread, phb);
}

static int proxy_connect_socks5(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS5\n", phb->host, phb->port, proxyhost, proxyport);

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb->host);
		g_free(phb);
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s5_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		s5_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void connection_host_resolved(struct sockaddr *addr, size_t addrlen, gpointer data, const char *error_message)
{
	struct PHB *phb = (struct PHB*)data;
	
	if(!addr)
	{
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		return;
	}

	switch(phb->proxytype)
	{
		case PROXY_NONE:
			proxy_connect_none(phb, addr, addrlen);
			break;
		case PROXY_HTTP:
			proxy_connect_http(phb, addr, addrlen);
			break;
		case PROXY_SOCKS4:
			proxy_connect_socks4(phb, addr, addrlen);
			break;
		case PROXY_SOCKS5:
			proxy_connect_socks5(phb, addr, addrlen);
			break;
	}
}

int proxy_connect(char *host, int port, GaimInputFunction func, gpointer data)
{
	char *connecthost = host;
	int connectport = port;
	struct PHB *phb = g_new0(struct PHB, 1);
	phb->func = func;
	phb->data = data;
	phb->host = g_strdup(host);
	phb->port = port;
	phb->proxytype = proxytype;

	if (!host || !port || (port == -1) || !func) {
		if(host)
			g_free(phb->host);
		g_free(phb);
		return -1;
	}

	if ((phb->proxytype!=PROXY_NONE) && (!proxyhost || !proxyhost[0] || !proxyport || (proxyport == -1)))
		phb->proxytype=PROXY_NONE;

	switch(phb->proxytype)
	{
		case PROXY_NONE:
			break;
		case PROXY_HTTP:
		case PROXY_SOCKS4:
		case PROXY_SOCKS5:
			connecthost=proxyhost;
			connectport=proxyport;
			break;
		default:
			g_free(phb->host);
			g_free(phb);
			return -1;
	}
	
	gaim_gethostbyname_async(connecthost, connectport, SOCK_STREAM, connection_host_resolved, phb);
	return 1;
}
