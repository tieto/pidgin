/**
 * @file proxy.c Proxy API
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
 *
 */

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support
 , 3rd draw women to it like flies to honey */

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "util.h"

#include "gaim.h"

static GaimProxyInfo *global_proxy_info = NULL;

struct PHB {
	GaimInputFunction func;
	gpointer data;
	char *host;
	int port;
	gint inpa;
	GaimProxyInfo *gpi;
	GaimAccount *account;
	GSList *hosts;
};

static void try_connect(struct PHB *);

const char* socks5errors[] = {
	"succeeded\n",
	"general SOCKS server failure\n",
	"connection not allowed by ruleset\n",
	"Network unreachable\n",
	"Host unreachable\n",
	"Connection refused\n",
	"TTL expired\n",
	"Command not supported\n",
	"Address type not supported\n"
};

/**************************************************************************
 * Proxy structure API
 **************************************************************************/
GaimProxyInfo *
gaim_proxy_info_new(void)
{
	return g_new0(GaimProxyInfo, 1);
}

void
gaim_proxy_info_destroy(GaimProxyInfo *info)
{
	g_return_if_fail(info != NULL);

	if (info->host     != NULL) g_free(info->host);
	if (info->username != NULL) g_free(info->username);
	if (info->password != NULL) g_free(info->password);

	g_free(info);
}

void
gaim_proxy_info_set_type(GaimProxyInfo *info, GaimProxyType type)
{
	g_return_if_fail(info != NULL);

	info->type = type;
}

void
gaim_proxy_info_set_host(GaimProxyInfo *info, const char *host)
{
	g_return_if_fail(info != NULL);

	if (info->host != NULL)
		g_free(info->host);

	info->host = (host == NULL ? NULL : g_strdup(host));
}

void
gaim_proxy_info_set_port(GaimProxyInfo *info, int port)
{
	g_return_if_fail(info != NULL);

	info->port = port;
}

void
gaim_proxy_info_set_username(GaimProxyInfo *info, const char *username)
{
	g_return_if_fail(info != NULL);

	if (info->username != NULL)
		g_free(info->username);

	info->username = (username == NULL ? NULL : g_strdup(username));
}

void
gaim_proxy_info_set_password(GaimProxyInfo *info, const char *password)
{
	g_return_if_fail(info != NULL);

	if (info->password != NULL)
		g_free(info->password);

	info->password = (password == NULL ? NULL : g_strdup(password));
}

GaimProxyType
gaim_proxy_info_get_type(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, GAIM_PROXY_NONE);

	return info->type;
}

const char *
gaim_proxy_info_get_host(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->host;
}

int
gaim_proxy_info_get_port(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, 0);

	return info->port;
}

const char *
gaim_proxy_info_get_username(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->username;
}

const char *
gaim_proxy_info_get_password(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->password;
}

/**************************************************************************
 * Global Proxy API
 **************************************************************************/
GaimProxyInfo *
gaim_global_proxy_get_info(void)
{
	return global_proxy_info;
}

/**************************************************************************
 * Proxy API
 **************************************************************************/
typedef void (*dns_callback_t)(GSList *hosts, gpointer data,
		const char *error_message);

#ifdef __unix__

/*  This structure represents both a pending DNS request and
 *  a free child process.
 */
typedef struct {
	char *host;
	int port;
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
} dns_params_t;

typedef struct {
	dns_params_t params;
	dns_callback_t callback;
	gpointer data;
} queued_dns_request_t;

static void req_free(pending_dns_request_t *req)
{
	g_return_if_fail(req != NULL);
	if(req->host)
		g_free(req->host);
	close(req->fd_in);
	close(req->fd_out);
	g_free(req);
	number_of_dns_children--;
}

static int send_dns_request_to_child(pending_dns_request_t *req, dns_params_t *dns_params)
{
	char ch;
	int rc;

	/* Are you alive? */
	if(kill(req->dns_pid, 0) != 0) {
		gaim_debug(GAIM_DEBUG_WARNING, "dns",
				   "DNS child %d no longer exists\n", req->dns_pid);
		return -1;
	}

	/* Let's contact this lost child! */
	rc = write(req->fd_in, dns_params, sizeof(*dns_params));
	if(rc<0) {
		gaim_debug(GAIM_DEBUG_ERROR, "dns",
				   "Unable to write to DNS child %d: %d\n",
				   req->dns_pid, strerror(errno));
		close(req->fd_in);
		return -1;
	}

	g_return_val_if_fail(rc == sizeof(*dns_params), -1);

	/* Did you hear me? (This avoids some race conditions) */
	rc = read(req->fd_out, &ch, 1);
	if(rc != 1 || ch!='Y') {
		gaim_debug(GAIM_DEBUG_WARNING, "dns",
				   "DNS child %d not responding. Killing it!\n",
				   req->dns_pid);
		kill(req->dns_pid, SIGKILL);
		return -1;
	}

	gaim_debug(GAIM_DEBUG_INFO, "dns",
			   "Successfully sent DNS request to child %d\n", req->dns_pid);
	return 0;
}

static void host_resolved(gpointer data, gint source, GaimInputCondition cond);

static void release_dns_child(pending_dns_request_t *req)
{
	g_free(req->host);
	req->host=NULL;

	if(queued_requests && !g_queue_is_empty(queued_requests)) {
		queued_dns_request_t *r = g_queue_pop_head(queued_requests);
		req->host = g_strdup(r->params.hostname);
		req->port = r->params.port;
		req->callback = r->callback;
		req->data = r->data;

		gaim_debug(GAIM_DEBUG_INFO, "dns",
				   "Processing queued DNS query for '%s' with child %d\n",
				   req->host, req->dns_pid);

		if(send_dns_request_to_child(req, &(r->params)) != 0) {
			req_free(req);
			req = NULL;

			gaim_debug(GAIM_DEBUG_WARNING, "dns",
					   "Intent of process queued query of '%s' failed, "
					   "requeueing...\n", r->params.hostname);
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
	GSList *hosts = NULL;
	struct sockaddr *addr = NULL;
	size_t addrlen;

	gaim_debug(GAIM_DEBUG_INFO, "dns", "Host '%s' resolved\n", req->host);
	gaim_input_remove(req->inpa);

	rc=read(req->fd_out, &err, sizeof(err));
	if((rc==4) && (err!=0)) {
		char message[1024];
#if HAVE_GETADDRINFO
		g_snprintf(message, sizeof(message), "DNS error: %s (pid=%d)",
				   gai_strerror(err), req->dns_pid);
#else
		g_snprintf(message, sizeof(message), "DNS error: %d (pid=%d)",
				   err, req->dns_pid);
#endif
		gaim_debug(GAIM_DEBUG_ERROR, "dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		release_dns_child(req);
		return;
	}
	if(rc>0) {
		while(rc > 0) {
			rc=read(req->fd_out, &addrlen, sizeof(addrlen));
			if(rc>0 && addrlen > 0) {
				addr=g_malloc(addrlen);
				rc=read(req->fd_out, addr, addrlen);
				hosts = g_slist_append(hosts, GINT_TO_POINTER(addrlen));
				hosts = g_slist_append(hosts, addr);
			} else {
				break;
			}
		}
	} else if(rc==-1) {
		char message[1024];
		g_snprintf(message, sizeof(message), "Error reading from DNS child: %s",strerror(errno));
		gaim_debug(GAIM_DEBUG_ERROR, "dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		req_free(req);
		return;
	} else if(rc==0) {
		char message[1024];
		g_snprintf(message, sizeof(message), "EOF reading from DNS child");
		close(req->fd_out);
		gaim_debug(GAIM_DEBUG_ERROR, "dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		req_free(req);
		return;
	}

/*	wait4(req->dns_pid, NULL, WNOHANG, NULL); */

	req->callback(hosts, req->data, NULL);

	release_dns_child(req);
}

static void trap_gdb_bug()
{
	const char *message =
		"Gaim's DNS child got a SIGTRAP signal. \n"
		"This can be caused by trying to run gaim inside gdb.\n"
		"There is a known gdb bug which prevents this.  Supposedly gaim\n"
		"should have detected you were using gdb and used an ugly hack,\n"
		"check cope_with_gdb_brokenness() in proxy.c.\n\n"
		"For more info about this bug, see http://sources.redhat.com/ml/gdb/2001-07/msg00349.html\n";
	fputs("\n* * *\n",stderr);
	fputs(message,stderr);
	fputs("* * *\n\n",stderr);
	execlp("xmessage","xmessage","-center", message, NULL);
	_exit(1);
}

static void cope_with_gdb_brokenness()
{
#ifdef __linux__
	static gboolean already_done = FALSE;
	char s[256], e[512];
	int n;
	pid_t ppid;

	if(already_done)
		return;
	already_done = TRUE;
	ppid = getppid();
	snprintf(s, sizeof(s), "/proc/%d/exe", ppid);
	n = readlink(s, e, sizeof(e));
	if(n < 0)
		return;

	e[MIN(n,sizeof(e)-1)] = '\0';

	if(strstr(e,"gdb")) {
		gaim_debug(GAIM_DEBUG_INFO, "dns",
				   "Debugger detected, performing useless query...\n");
		gethostbyname("x.x.x.x.x");
	}
#endif
}

static void
gaim_dns_childthread(int child_out, int child_in, dns_params_t *dns_params, gboolean show_debug)
{
	const int zero = 0;
	int rc;
#if HAVE_GETADDRINFO
	struct addrinfo hints, *res, *tmp;
	char servname[20];
#else
	struct sockaddr_in sin;
	const size_t addrlen = sizeof(sin);
#endif

#ifdef HAVE_SIGNAL_H
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGTRAP, trap_gdb_bug);
#endif

	while (1) {
		if (dns_params->hostname[0] == '\0') {
			const char Y = 'Y';
			fd_set fds;
			struct timeval tv = { .tv_sec = 40 , .tv_usec = 0 };
			FD_ZERO(&fds);
			FD_SET(child_in, &fds);
			rc = select(child_in + 1, &fds, NULL, NULL, &tv);
			if (!rc) {
				if (show_debug)
					fprintf(stderr,"dns[%d]: nobody needs me... =(\n", getpid());
				break;
			}
			rc = read(child_in, dns_params, sizeof(dns_params_t));
			if (rc < 0) {
				perror("read()");
				break;
			}
			if (rc==0) {
				if (show_debug)
					fprintf(stderr,"dns[%d]: Oops, father has gone, wait for me, wait...!\n", getpid());
				_exit(0);
			}
			if (dns_params->hostname[0] == '\0') {
				fprintf(stderr, "dns[%d]: hostname = \"\" (port = %d)!!!\n", getpid(), dns_params->port);
				_exit(1);
			}
			write(child_out, &Y, 1);
		}

#if HAVE_GETADDRINFO
		g_snprintf(servname, sizeof(servname), "%d", dns_params->port);
		memset(&hints,0,sizeof(hints));

		/* This is only used to convert a service
		 * name to a port number. As we know we are
		 * passing a number already, we know this
		 * value will not be really used by the C
		 * library.
		 */
		hints.ai_socktype = SOCK_STREAM;
		rc = getaddrinfo(dns_params->hostname, servname, &hints, &res);
		if (rc) {
			write(child_out, &rc, sizeof(int));
			close(child_out);
			if (show_debug)
				fprintf(stderr,"dns[%d] Error: getaddrinfo returned %d\n",
					getpid(), rc);
			dns_params->hostname[0] = '\0';
			continue;
		}
		write(child_out, &zero, sizeof(zero));
		tmp = res;
		while(res) {
			size_t ai_addrlen = res->ai_addrlen;
			write(child_out, &ai_addrlen, sizeof(ai_addrlen));
			write(child_out, res->ai_addr, res->ai_addrlen);
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
		write(child_out, &zero, sizeof(zero));
#else
		if (!inet_aton(hostname, &sin.sin_addr)) {
			struct hostent *hp;
			if (!(hp = gethostbyname(dns_params->hostname))) {
				write(child_out, &h_errno, sizeof(int));
				close(child_out);
				if (show_debug)
					fprintf(stderr,"DNS Error: %d\n", h_errno);
				_exit(0);
			}
			memset(&sin, 0, sizeof(struct sockaddr_in));
			memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
			sin.sin_family = hp->h_addrtype;
		} else
			sin.sin_family = AF_INET;

		sin.sin_port = htons(dns_params->port);
		write(child_out, &zero, sizeof(zero));
		write(child_out, &addrlen, sizeof(addrlen));
		write(child_out, &sin, addrlen);
		write(child_out, &zero, sizeof(zero));
#endif
		dns_params->hostname[0] = '\0';
	}

	close(child_out);
	close(child_in);

	_exit(0);
}

int gaim_gethostbyname_async(const char *hostname, int port, dns_callback_t callback, gpointer data)
{
	pending_dns_request_t *req = NULL;
	dns_params_t dns_params;
	gchar *host_temp;

	host_temp = g_strstrip(g_strdup(hostname));
	strncpy(dns_params.hostname, host_temp, sizeof(dns_params.hostname) - 1);
	g_free(host_temp);
	dns_params.hostname[sizeof(dns_params.hostname) - 1] = '\0';
	dns_params.port = port;

	/*
	 * If we have any children, attempt to have them perform the DNS
	 * query.  If we're able to send the query to a child, then req
	 * will be set to the pending_dns_request_t.  Otherwise, req will
	 * be NULL and we'll need to create a new DNS request child.
	 */
	while (free_dns_children != NULL) {
		req = free_dns_children->data;
		free_dns_children = g_slist_remove(free_dns_children, req);

		if (send_dns_request_to_child(req, &dns_params) == 0)
			/* We found an acceptable child, yay */
			break;

		req_free(req);
		req = NULL;
	}

	/* We need to create a new DNS request child */
	if (req == NULL) {
		int child_out[2], child_in[2];

		if (number_of_dns_children >= MAX_DNS_CHILDREN) {
			queued_dns_request_t *r = g_new(queued_dns_request_t, 1);
			memcpy(&(r->params), &dns_params, sizeof(dns_params));
			r->callback = callback;
			r->data = data;
			if (!queued_requests)
				queued_requests = g_queue_new();
			g_queue_push_tail(queued_requests, r);

			gaim_debug(GAIM_DEBUG_INFO, "dns",
					   "DNS query for '%s' queued\n", dns_params.hostname);

			return 0;
		}

		/* Create pipes for communicating with the child process */
		if (pipe(child_out) || pipe(child_in)) {
			gaim_debug(GAIM_DEBUG_ERROR, "dns",
					   "Could not create pipes: %s\n", strerror(errno));
			return -1;
		}

		req = g_new(pending_dns_request_t, 1);

		cope_with_gdb_brokenness();

		/* Fork! */
		req->dns_pid = fork();

		/* If we are the child process... */
		if (req->dns_pid == 0) {
			/* We should not access the parent's side of the pipe, so close them... */
			close(child_out[0]);
			close(child_in[1]);

			gaim_dns_childthread(child_out[1], child_in[0], &dns_params, opt_debug);
			/* The thread calls _exit() rather than returning, so we never get here */
		}

		/* We should not access the child's side of the pipe, so close them... */
		close(child_out[1]);
		close(child_in[0]);
		if (req->dns_pid == -1) {
			gaim_debug(GAIM_DEBUG_ERROR, "dns",
					   "Could not create child process for DNS: %s\n",
					   strerror(errno));
			g_free(req);
			return -1;
		}

		req->fd_out = child_out[0];
		req->fd_in = child_in[1];
		number_of_dns_children++;
		gaim_debug(GAIM_DEBUG_INFO, "dns",
				   "Created new DNS child %d, there are now %d children.\n",
				   req->dns_pid, number_of_dns_children);
	}

	req->host = g_strdup(hostname);
	req->port = port;
	req->callback = callback;
	req->data = data;
	req->inpa = gaim_input_add(req->fd_out, GAIM_INPUT_READ, host_resolved, req);

	return 0;
}
#else /* __unix__ */

typedef struct {
	gpointer data;
	size_t addrlen;
	struct sockaddr *addr;
	dns_callback_t callback;
} pending_dns_request_t;

static gboolean host_resolved(gpointer data)
{
	pending_dns_request_t *req = (pending_dns_request_t*)data;
	GSList *hosts = NULL;
	hosts = g_slist_append(hosts, GINT_TO_POINTER(req->addrlen));
	hosts = g_slist_append(hosts, req->addr);
	req->callback(hosts, req->data, NULL);
	g_free(req);
	return FALSE;
}

int
gaim_gethostbyname_async(const char *hostname, int port,
						 dns_callback_t callback, gpointer data)
{
	struct sockaddr_in sin;
	pending_dns_request_t *req;

	if (!inet_aton(hostname, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(hostname))) {
			gaim_debug(GAIM_DEBUG_ERROR, "dns",
					   "gaim_gethostbyname(\"%s\", %d) failed: %d\n",
					   hostname, port, h_errno);
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
	gaim_timeout_add(10, host_resolved, req);
	return 0;
}

#endif /* __unix__ */

static void
no_one_calls(gpointer data, gint source, GaimInputCondition cond)
{
	struct PHB *phb = data;
	unsigned int len;
	int error=0, ret;

	gaim_debug(GAIM_DEBUG_INFO, "proxy", "Connected.\n");

	len = sizeof(error);

	/*
	 * getsockopt after a non-blocking connect returns -1 if something is
	 * really messed up (bad descriptor, usually). Otherwise, it returns 0 and
	 * error holds what connect would have returned if it blocked until now.
	 * Thus, error == 0 is success, error == EINPROGRESS means "try again",
	 * and anything else is a real error.
	 *
	 * (error == EINPROGRESS can happen after a select because the kernel can
	 * be overly optimistic sometimes. select is just a hint that you might be
	 * able to do something.)
	 */
	ret = getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0 && error == EINPROGRESS)
		return; /* we'll be called again later */
	if (ret < 0 || error != 0) {
		if(ret!=0) error = errno;
		close(source);
		gaim_input_remove(phb->inpa);

		gaim_debug(GAIM_DEBUG_ERROR, "proxy",
			   "getsockopt SO_ERROR check: %s\n", strerror(error));

		try_connect(phb);
		return;
	}

	fcntl(source, F_SETFL, 0);
	gaim_input_remove(phb->inpa);

	if (phb->account == NULL ||
		gaim_account_get_connection(phb->account) != NULL) {

		phb->func(phb->data, source, GAIM_INPUT_READ);
	}

	g_free(phb->host);
	g_free(phb);
}

static gboolean clean_connect(gpointer data)
{
	struct PHB *phb = data;

	if (phb->account == NULL ||
		gaim_account_get_connection(phb->account) != NULL) {

		phb->func(phb->data, phb->port, GAIM_INPUT_READ);
	}

	g_free(phb->host);
	g_free(phb);

	return FALSE;
}


static int
proxy_connect_none(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	gaim_debug(GAIM_DEBUG_INFO, "proxy",
			   "Connecting to %s:%d with no proxy\n", phb->host, phb->port);

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "proxy",
				   "Unable to create socket: %s\n", strerror(errno));
		return -1;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, (struct sockaddr *)addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "proxy",
					   "Connect would have blocked.\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, no_one_calls, phb);
		}
		else {
			gaim_debug(GAIM_DEBUG_ERROR, "proxy",
					   "Connect failed: %s\n", strerror(errno));
			close(fd);
			return -1;
		}
	}
	else {
		unsigned int len;
		int error = ETIMEDOUT;
		gaim_debug(GAIM_DEBUG_MISC, "proxy", "Connect didn't block.\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			gaim_debug(GAIM_DEBUG_ERROR, "proxy", "getsockopt failed.\n");
			close(fd);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		phb->port = fd;	/* bleh */
		gaim_timeout_add(50, clean_connect, phb);	/* we do this because we never
							   want to call our callback
							   before we return. */
	}

	return fd;
}

#define HTTP_GOODSTRING "HTTP/1.0 200"
#define HTTP_GOODSTRING2 "HTTP/1.1 200"

static void
http_complete(struct PHB *phb, gint source)
{
	gaim_debug(GAIM_DEBUG_INFO, "http proxy", "proxy connection established\n");
	if(source < 0) {
		try_connect(phb);
	} else if(!phb->account || phb->account->gc) {
		phb->func(phb->data, source, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
	}
}


/* read the response to the CONNECT request, if we are requesting a non-port-80 tunnel */
static void
http_canread(gpointer data, gint source, GaimInputCondition cond)
{
	int nlc = 0;
	int pos = 0;
	int minor, major, status, error=0;
	struct PHB *phb = data;
	char inputline[8192], *p;

	gaim_input_remove(phb->inpa);

	while ((pos < sizeof(inputline)-1) && (nlc != 2) && (read(source, &inputline[pos++], 1) == 1)) {
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
		gaim_debug(GAIM_DEBUG_ERROR, "proxy",
				   "Unable to parse proxy's response: %s\n", inputline);
		close(source);
		source=-1;
	}
	else if(status!=200) {
		gaim_debug(GAIM_DEBUG_ERROR, "proxy",
				   "Proxy server replied with:\n%s\n", p);
		close(source);
		source = -1;

		/* XXX: why in the hell are we calling gaim_connection_error() here? */
		if ( status == 403 /* Forbidden */ ) {
			gchar *msg = g_strdup_printf(_("Access denied: proxy server forbids port %d tunnelling."), phb->port);
			gaim_connection_error(phb->account->gc, msg);
			g_free(msg);
		} else {
			char *msg = g_strdup_printf(_("Proxy connection error %d"), status);
			gaim_connection_error(phb->account->gc, msg);
			g_free(msg);
		}

	} else {
		http_complete(phb, source);
	}

	return;
}

static void
http_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	char request[8192];
	int request_len = 0;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	gaim_debug(GAIM_DEBUG_INFO, "http proxy", "Connected.\n");

	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);

	len = sizeof(error);

	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);

		try_connect(phb);
		return;
	}

	gaim_debug(GAIM_DEBUG_INFO, "proxy", "using CONNECT tunnelling for %s:%d\n", phb->host, phb->port);
	request_len = g_snprintf(request, sizeof(request),
				 "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n",
				 phb->host, phb->port, phb->host, phb->port);

	if (gaim_proxy_info_get_username(phb->gpi) != NULL) {
		char *t1, *t2;
		t1 = g_strdup_printf("%s:%s",
				     gaim_proxy_info_get_username(phb->gpi),
				     gaim_proxy_info_get_password(phb->gpi) ? 
				     gaim_proxy_info_get_password(phb->gpi) : "");
		t2 = gaim_base64_encode(t1, strlen(t1));
		g_free(t1);
		g_return_if_fail(request_len < sizeof(request));
		request_len += g_snprintf(request + request_len,
					  sizeof(request) - request_len,
					  "Proxy-Authorization: Basic %s\r\n", t2);
		g_free(t2);
	}

	g_return_if_fail(request_len < sizeof(request));
	strcpy(request + request_len, "\r\n");
	request_len += 2;

	if (write(source, request, request_len) < 0) {
		close(source);

		try_connect(phb);
		return;
	}

	/* register the response handler for the CONNECT request */
	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, http_canread, phb);
}

static int
proxy_connect_http(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	gaim_debug(GAIM_DEBUG_INFO, "http proxy",
			   "Connecting to %s:%d via %s:%d using HTTP\n",
			   phb->host, phb->port,
			   gaim_proxy_info_get_host(phb->gpi),
			   gaim_proxy_info_get_port(phb->gpi));

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "http proxy",
					   "Connect would have blocked.\n");

			if (phb->port != 80) {
				/* we need to do CONNECT first */
				phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE,
							   http_canwrite, phb);
			} else {
				http_complete(phb, fd);
			}
		} else {
			close(fd);
			return -1;
		}
	}
	else {
		unsigned int len;
		int error = ETIMEDOUT;

		gaim_debug(GAIM_DEBUG_MISC, "http proxy",
				   "Connect didn't block.\n");

		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		http_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void
s4_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);

	memset(packet, 0, sizeof(packet));

	if (read(source, packet, 9) >= 4 && packet[1] == 90) {
		if (phb->account == NULL ||
			gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, source, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	close(source);

	try_connect(phb);
}

static void
s4_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct hostent *hp;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	gaim_debug(GAIM_DEBUG_INFO, "s4 proxy", "Connected.\n");

	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);

	len = sizeof(error);

	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);

		try_connect(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	/* XXX does socks4 not support host name lookups by the proxy? */
	if (!(hp = gethostbyname(phb->host))) {
		close(source);

		try_connect(phb);
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

		try_connect(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s4_canread, phb);
}

static int
proxy_connect_socks4(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	gaim_debug(GAIM_DEBUG_INFO, "socks4 proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS4\n",
			   phb->host, phb->port,
			   gaim_proxy_info_get_host(phb->gpi),
			   gaim_proxy_info_get_port(phb->gpi));

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
		return -1;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "socks4 proxy",
					   "Connect would have blocked.\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s4_canwrite, phb);
		}
		else {
			close(fd);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		gaim_debug(GAIM_DEBUG_MISC, "socks4 proxy",
				   "Connect didn't block.\n");

		len = sizeof(error);

		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			return -1;
		}

		fcntl(fd, F_SETFL, 0);
		s4_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void
s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Able to read again.\n");

	if (read(source, buf, 4) < 4) {
		gaim_debug(GAIM_DEBUG_WARNING, "socks5 proxy", "or not...\n");
		close(source);

		try_connect(phb);
		return;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		if ((buf[0] == 0x05) && (buf[1] < 0x09))
			gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", socks5errors[buf[1]]);
		else
			gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", "Bad data.\n");
		close(source);

		try_connect(phb);
		return;
	}

	/* Skip past BND.ADDR */
	switch(buf[3]) {
		case 0x01: /* the address is a version-4 IP address, with a length of 4 octets */
			read(source, buf, 4);
			break;
		case 0x03: /* the address field contains a fully-qualified domain name.  The first
					  octet of the address field contains the number of octets of name that
					  follow, there is no terminating NUL octet. */
			read(source, buf, 1);
			read(source, buf, buf[0]);
			break;
		case 0x04: /* the address is a version-6 IP address, with a length of 16 octets */
			read(source, buf, 16);
			break;
	}

	/* Skip past BND.PORT */
	read(source, buf, 2);

	if (phb->account == NULL ||
		gaim_account_get_connection(phb->account) != NULL) {

		phb->func(phb->data, source, GAIM_INPUT_READ);
	}

	g_free(phb->host);
	g_free(phb);
}

static void
s5_sendconnect(gpointer data, gint source)
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

		try_connect(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread_again, phb);
}

static void
s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Got auth response.\n");

	if (read(source, buf, 2) < 2) {
		close(source);

		try_connect(phb);
		return;
	}

	if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
		close(source);

		try_connect(phb);
		return;
	}

	s5_sendconnect(phb, source);
}

static void
s5_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Able to read.\n");

	if (read(source, buf, 2) < 2) {
		close(source);

		try_connect(phb);
		return;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		close(source);

		try_connect(phb);
		return;
	}

	if (buf[1] == 0x02) {
		unsigned int i, j;

		i = strlen(gaim_proxy_info_get_username(phb->gpi));
		j = strlen(gaim_proxy_info_get_password(phb->gpi));

		buf[0] = 0x01;	/* version 1 */
		buf[1] = i;
		memcpy(buf + 2, gaim_proxy_info_get_username(phb->gpi), i);
		buf[2 + i] = j;
		memcpy(buf + 2 + i + 1, gaim_proxy_info_get_password(phb->gpi), j);

		if (write(source, buf, 3 + i + j) < 3 + i + j) {
			close(source);

			try_connect(phb);
			return;
		}

		phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_readauth, phb);
	}
	else {
		s5_sendconnect(phb, source);
	}
}

static void
s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	int i;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Connected.\n");

	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);

	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);

		try_connect(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */

	if (gaim_proxy_info_get_username(phb->gpi) != NULL) {
		buf[1] = 0x02;	/* two methods */
		buf[2] = 0x00;	/* no authentication */
		buf[3] = 0x02;	/* username/password authentication */
		i = 4;
	}
	else {
		buf[1] = 0x01;
		buf[2] = 0x00;
		i = 3;
	}

	if (write(source, buf, i) < i) {
		gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", "Unable to write\n");
		close(source);

		try_connect(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread, phb);
}

static int
proxy_connect_socks5(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	int fd = -1;

	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS5\n",
			   phb->host, phb->port,
			   gaim_proxy_info_get_host(phb->gpi),
			   gaim_proxy_info_get_port(phb->gpi));

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
		return -1;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "socks5 proxy",
					   "Connect would have blocked.\n");

			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s5_canwrite, phb);
		}
		else {
			close(fd);
			return -1;
		}
	}
	else {
		unsigned int len;
		int error = ETIMEDOUT;

		gaim_debug(GAIM_DEBUG_MISC, "socks5 proxy",
				   "Connect didn't block.\n");

		len = sizeof(error);

		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			return -1;
		}

		fcntl(fd, F_SETFL, 0);
		s5_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void try_connect(struct PHB *phb)
{
	size_t addrlen;
	struct sockaddr *addr;
	int ret = -1;

	while (phb->hosts) {
		addrlen = GPOINTER_TO_INT(phb->hosts->data);
		phb->hosts = g_slist_remove(phb->hosts, phb->hosts->data);
		addr = phb->hosts->data;
		phb->hosts = g_slist_remove(phb->hosts, phb->hosts->data);

		switch (gaim_proxy_info_get_type(phb->gpi)) {
			case GAIM_PROXY_NONE:
				ret = proxy_connect_none(phb, addr, addrlen);
				break;

			case GAIM_PROXY_HTTP:
				ret = proxy_connect_http(phb, addr, addrlen);
				break;

			case GAIM_PROXY_SOCKS4:
				ret = proxy_connect_socks4(phb, addr, addrlen);
				break;

			case GAIM_PROXY_SOCKS5:
				ret = proxy_connect_socks5(phb, addr, addrlen);
				break;

			case GAIM_PROXY_USE_ENVVAR:
				ret = proxy_connect_http(phb, addr, addrlen);
				break;

			default:
				break;
		}

		g_free(addr);

		if (ret > 0)
			break;
	}

	if (ret < 0) {
		if (phb->account == NULL ||
			gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
	}
}

static void
connection_host_resolved(GSList *hosts, gpointer data,
						 const char *error_message)
{
	struct PHB *phb = (struct PHB*)data;

	phb->hosts = hosts;

	try_connect(phb);
}

int
gaim_proxy_connect(GaimAccount *account, const char *host, int port,
				   GaimInputFunction func, gpointer data)
{
	const char *connecthost = host;
	int connectport = port;
	struct PHB *phb;
	const gchar *tmp;

	g_return_val_if_fail(host != NULL, -1);
	g_return_val_if_fail(port != 0 && port != -1, -1);
	g_return_val_if_fail(func != NULL, -1);

	phb = g_new0(struct PHB, 1);

	if (account == NULL || gaim_account_get_proxy_info(account) == NULL)
		phb->gpi = gaim_global_proxy_get_info();
	else
		phb->gpi = gaim_account_get_proxy_info(account);

	phb->func = func;
	phb->data = data;
	phb->host = g_strdup(host);
	phb->port = port;
	phb->account = account;

	if (gaim_proxy_info_get_type(phb->gpi) == GAIM_PROXY_USE_ENVVAR) {
		if ((tmp = g_getenv("HTTP_PROXY")) != NULL ||
			(tmp = g_getenv("http_proxy")) != NULL ||
			(tmp= g_getenv("HTTPPROXY")) != NULL) {
			char *proxyhost,*proxypath,*proxyuser,*proxypasswd;
			int proxyport;

			/* http_proxy-format:
			 * export http_proxy="http://user:passwd@your.proxy.server:port/"
			 */
			if(gaim_url_parse(tmp, &proxyhost, &proxyport, &proxypath, &proxyuser, &proxypasswd)) {
				gaim_proxy_info_set_host(phb->gpi, proxyhost);
				g_free(proxyhost);
				g_free(proxypath);
				if (proxyuser != NULL) {
					gaim_proxy_info_set_username(phb->gpi, proxyuser);
					g_free(proxyuser);
				}
				if (proxypasswd != NULL) {
					gaim_proxy_info_set_password(phb->gpi, proxypasswd);
					g_free(proxypasswd);
				}

				/* only for backward compatibility */
				if (proxyport == 80 &&
				    ((tmp = g_getenv("HTTP_PROXY_PORT")) != NULL ||
				     (tmp = g_getenv("http_proxy_port")) != NULL ||
				     (tmp = g_getenv("HTTPPROXYPORT")) != NULL))
				    proxyport = atoi(tmp);

				gaim_proxy_info_set_port(phb->gpi, proxyport);
			}
		}

		/* XXX: Do we want to skip this step if user/password were part of url? */
		if ((tmp = g_getenv("HTTP_PROXY_USER")) != NULL ||
			(tmp = g_getenv("http_proxy_user")) != NULL ||
			(tmp = g_getenv("HTTPPROXYUSER")) != NULL)
			gaim_proxy_info_set_username(phb->gpi, tmp);

		if ((tmp = g_getenv("HTTP_PROXY_PASS")) != NULL ||
			(tmp = g_getenv("http_proxy_pass")) != NULL ||
			(tmp = g_getenv("HTTPPROXYPASS")) != NULL)
			gaim_proxy_info_set_password(phb->gpi, tmp);
	}

	if ((gaim_proxy_info_get_type(phb->gpi) != GAIM_PROXY_NONE) &&
		(gaim_proxy_info_get_host(phb->gpi) == NULL ||
		 gaim_proxy_info_get_port(phb->gpi) <= 0)) {

		gaim_notify_error(NULL, NULL, _("Invalid proxy settings"), _("Either the host name or port number specified for your given proxy type is invalid."));
		g_free(phb->host);
		g_free(phb);
		return -1;
	}

	switch (gaim_proxy_info_get_type(phb->gpi))
	{
		case GAIM_PROXY_NONE:
			break;

		case GAIM_PROXY_HTTP:
		case GAIM_PROXY_SOCKS4:
		case GAIM_PROXY_SOCKS5:
		case GAIM_PROXY_USE_ENVVAR:
			connecthost = gaim_proxy_info_get_host(phb->gpi);
			connectport = gaim_proxy_info_get_port(phb->gpi);
			break;

		default:
			g_free(phb->host);
			g_free(phb);
			return -1;
	}

	return gaim_gethostbyname_async(connecthost, connectport,
									connection_host_resolved, phb);
}

int
gaim_proxy_connect_socks5(GaimProxyInfo *gpi, const char *host, int port,
		GaimInputFunction func, gpointer data)
{
	struct PHB *phb;

	phb = g_new0(struct PHB, 1);
	phb->gpi = gpi;
	phb->func = func;
	phb->data = data;
	phb->host = g_strdup(host);
	phb->port = port;

	return gaim_gethostbyname_async(gaim_proxy_info_get_host(gpi), gaim_proxy_info_get_port(gpi),
									connection_host_resolved, phb);
}


static void
proxy_pref_cb(const char *name, GaimPrefType type, gpointer value,
			  gpointer data)
{
	GaimProxyInfo *info = gaim_global_proxy_get_info();

	if (!strcmp(name, "/core/proxy/type")) {
		int proxytype;
		char *type = value;

		if (!strcmp(type, "none"))
			proxytype = GAIM_PROXY_NONE;
		else if (!strcmp(type, "http"))
			proxytype = GAIM_PROXY_HTTP;
		else if (!strcmp(type, "socks4"))
			proxytype = GAIM_PROXY_SOCKS4;
		else if (!strcmp(type, "socks5"))
			proxytype = GAIM_PROXY_SOCKS5;
		else if (!strcmp(type, "envvar"))
			proxytype = GAIM_PROXY_USE_ENVVAR;
		else
			proxytype = -1;

		gaim_proxy_info_set_type(info, proxytype);
	} else if (!strcmp(name, "/core/proxy/host"))
		gaim_proxy_info_set_host(info, value);
	else if (!strcmp(name, "/core/proxy/port"))
		gaim_proxy_info_set_port(info, GPOINTER_TO_INT(value));
	else if (!strcmp(name, "/core/proxy/username"))
		gaim_proxy_info_set_username(info, value);
	else if (!strcmp(name, "/core/proxy/password"))
		gaim_proxy_info_set_password(info, value);
}

void
gaim_proxy_init(void)
{
	void *handle;

	/* Initialize a default proxy info struct. */
	global_proxy_info = gaim_proxy_info_new();

	/* Proxy */
	gaim_prefs_add_none("/core/proxy");
	gaim_prefs_add_string("/core/proxy/type", "none");
	gaim_prefs_add_string("/core/proxy/host", "");
	gaim_prefs_add_int("/core/proxy/port", 0);
	gaim_prefs_add_string("/core/proxy/username", "");
	gaim_prefs_add_string("/core/proxy/password", "");

	/* Setup callbacks for the preferences. */
	handle = gaim_proxy_get_handle();
	gaim_prefs_connect_callback(handle, "/core/proxy/type",
								proxy_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/host",
								proxy_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/port",
								proxy_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/username",
								proxy_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/password",
								proxy_pref_cb, NULL);
}

void *
gaim_proxy_get_handle()
{
	static int handle;

	return &handle;
}
