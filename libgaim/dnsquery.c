/**
 * @file dnsquery.c DNS query API
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

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "dnsquery.h"
#include "util.h"

/**************************************************************************
 * DNS query API
 **************************************************************************/

struct _GaimDnsQueryData {
};

#if defined(__unix__) || defined(__APPLE__)

#define MAX_DNS_CHILDREN 4

/*
 * This structure represents both a pending DNS request and
 * a free child process.
 */
typedef struct {
	char *host;
	int port;
	GaimDnsQueryConnectFunction callback;
	gpointer data;
	guint inpa;
	int fd_in, fd_out;
	pid_t dns_pid;
} pending_dns_request_t;

static GSList *free_dns_children = NULL;
static GQueue *queued_requests = NULL;

static int number_of_dns_children = 0;

typedef struct {
	char hostname[512];
	int port;
} dns_params_t;

typedef struct {
	dns_params_t params;
	GaimDnsQueryConnectFunction callback;
	gpointer data;
} queued_dns_request_t;

/*
 * Begin the DNS resolver child process functions.
 */
#ifdef HAVE_SIGNAL_H
static void
trap_gdb_bug()
{
	const char *message =
		"Gaim's DNS child got a SIGTRAP signal.\n"
		"This can be caused by trying to run gaim inside gdb.\n"
		"There is a known gdb bug which prevents this.  Supposedly gaim\n"
		"should have detected you were using gdb and used an ugly hack,\n"
		"check cope_with_gdb_brokenness() in dnsquery.c.\n\n"
		"For more info about this bug, see http://sources.redhat.com/ml/gdb/2001-07/msg00349.html\n";
	fputs("\n* * *\n",stderr);
	fputs(message,stderr);
	fputs("* * *\n\n",stderr);
	execlp("xmessage","xmessage","-center", message, NULL);
	_exit(1);
}
#endif

static void
cope_with_gdb_brokenness()
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
		gaim_debug_info("dns",
				   "Debugger detected, performing useless query...\n");
		gethostbyname("x.x.x.x.x");
	}
#endif
}

static void
gaim_dns_resolverthread(int child_out, int child_in, gboolean show_debug)
{
	dns_params_t dns_params;
	const size_t zero = 0;
	int rc;
#ifdef HAVE_GETADDRINFO
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

	/*
	 * We resolve 1 host name for each iteration of this
	 * while loop.
	 *
	 * The top half of this reads in the hostname and port
	 * number from the socket with our parent.  The bottom
	 * half of this resolves the IP (blocking) and sends
	 * the result back to our parent, when finished.
	 */
	while (1) {
		const char ch = 'Y';
		fd_set fds;
		struct timeval tv = { .tv_sec = 40 , .tv_usec = 0 };
		FD_ZERO(&fds);
		FD_SET(child_in, &fds);
		rc = select(child_in + 1, &fds, NULL, NULL, &tv);
		if (!rc) {
			if (show_debug)
				printf("dns[%d]: nobody needs me... =(\n", getpid());
			break;
		}
		rc = read(child_in, &dns_params, sizeof(dns_params_t));
		if (rc < 0) {
			perror("read()");
			break;
		}
		if (rc == 0) {
			if (show_debug)
				printf("dns[%d]: Oops, father has gone, wait for me, wait...!\n", getpid());
			_exit(0);
		}
		if (dns_params.hostname[0] == '\0') {
			printf("dns[%d]: hostname = \"\" (port = %d)!!!\n", getpid(), dns_params.port);
			_exit(1);
		}
		/* Tell our parent that we read the data successfully */
		write(child_out, &ch, sizeof(ch));

		/* We have the hostname and port, now resolve the IP */

#ifdef HAVE_GETADDRINFO
		g_snprintf(servname, sizeof(servname), "%d", dns_params.port);
		memset(&hints, 0, sizeof(hints));

		/* This is only used to convert a service
		 * name to a port number. As we know we are
		 * passing a number already, we know this
		 * value will not be really used by the C
		 * library.
		 */
		hints.ai_socktype = SOCK_STREAM;
		rc = getaddrinfo(dns_params.hostname, servname, &hints, &res);
		write(child_out, &rc, sizeof(rc));
		if (rc != 0) {
			close(child_out);
			if (show_debug)
				printf("dns[%d] Error: getaddrinfo returned %d\n",
					getpid(), rc);
			dns_params.hostname[0] = '\0';
			continue;
		}
		tmp = res;
		while (res) {
			size_t ai_addrlen = res->ai_addrlen;
			write(child_out, &ai_addrlen, sizeof(ai_addrlen));
			write(child_out, res->ai_addr, res->ai_addrlen);
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
		write(child_out, &zero, sizeof(zero));
#else
		if (!inet_aton(dns_params.hostname, &sin.sin_addr)) {
			struct hostent *hp;
			if (!(hp = gethostbyname(dns_params.hostname))) {
				write(child_out, &h_errno, sizeof(int));
				close(child_out);
				if (show_debug)
					printf("DNS Error: %d\n", h_errno);
				_exit(0);
			}
			memset(&sin, 0, sizeof(struct sockaddr_in));
			memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
			sin.sin_family = hp->h_addrtype;
		} else
			sin.sin_family = AF_INET;

		sin.sin_port = htons(dns_params.port);
		write(child_out, &addrlen, sizeof(addrlen));
		write(child_out, &sin, addrlen);
		write(child_out, &zero, sizeof(zero));
#endif
		dns_params.hostname[0] = '\0';
	}

	close(child_out);
	close(child_in);

	_exit(0);
}

static pending_dns_request_t *
gaim_dns_new_resolverthread(gboolean show_debug)
{
	pending_dns_request_t *req;
	int child_out[2], child_in[2];

	/* Create pipes for communicating with the child process */
	if (pipe(child_out) || pipe(child_in)) {
		gaim_debug_error("dns",
				   "Could not create pipes: %s\n", strerror(errno));
		return NULL;
	}

	req = g_new(pending_dns_request_t, 1);

	cope_with_gdb_brokenness();

	/* Fork! */
	req->dns_pid = fork();

	/* If we are the child process... */
	if (req->dns_pid == 0) {
		/* We should not access the parent's side of the pipes, so close them */
		close(child_out[0]);
		close(child_in[1]);

		gaim_dns_resolverthread(child_out[1], child_in[0], show_debug);
		/* The thread calls _exit() rather than returning, so we never get here */
	}

	/* We should not access the child's side of the pipes, so close them */
	close(child_out[1]);
	close(child_in[0]);
	if (req->dns_pid == -1) {
		gaim_debug_error("dns",
				   "Could not create child process for DNS: %s\n",
				   strerror(errno));
		g_free(req);
		return NULL;
	}

	req->fd_out = child_out[0];
	req->fd_in = child_in[1];
	number_of_dns_children++;
	gaim_debug_info("dns",
			   "Created new DNS child %d, there are now %d children.\n",
			   req->dns_pid, number_of_dns_children);

	return req;
}
/*
 * End the DNS resolver child process functions.
 */

/*
 * Begin the functions for dealing with the DNS child processes.
 */
static void
req_free(pending_dns_request_t *req)
{
	g_return_if_fail(req != NULL);

	close(req->fd_in);
	close(req->fd_out);

	g_free(req->host);
	g_free(req);

	number_of_dns_children--;
}

static int
send_dns_request_to_child(pending_dns_request_t *req, dns_params_t *dns_params)
{
	char ch;
	int rc;
	pid_t pid;

	/* This waitpid might return the child's PID if it has recently
	 * exited, or it might return an error if it exited "long
	 * enough" ago that it has already been reaped; in either
	 * instance, we can't use it. */
	if ((pid = waitpid (req->dns_pid, NULL, WNOHANG)) > 0) {
		gaim_debug_warning("dns",
				   "DNS child %d no longer exists\n", req->dns_pid);
		return -1;
	} else if (pid < 0) {
		gaim_debug_warning("dns",
		                   "Wait for DNS child %d failed: %s\n",
		                   req->dns_pid, strerror(errno));
		return -1;
	}

	/* Let's contact this lost child! */
	rc = write(req->fd_in, dns_params, sizeof(*dns_params));
	if (rc < 0) {
		gaim_debug_error("dns",
				   "Unable to write to DNS child %d: %d\n",
				   req->dns_pid, strerror(errno));
		close(req->fd_in);
		return -1;
	}

	g_return_val_if_fail(rc == sizeof(*dns_params), -1);

	/* Did you hear me? (This avoids some race conditions) */
	rc = read(req->fd_out, &ch, sizeof(ch));
	if (rc != 1 || ch != 'Y')
	{
		gaim_debug_warning("dns",
				   "DNS child %d not responding. Killing it!\n",
				   req->dns_pid);
		kill(req->dns_pid, SIGKILL);
		return -1;
	}

	gaim_debug_info("dns",
			   "Successfully sent DNS request to child %d\n", req->dns_pid);

	return 0;
}

static void
host_resolved(gpointer data, gint source, GaimInputCondition cond);

static void
release_dns_child(pending_dns_request_t *req)
{
	g_free(req->host);
	req->host = NULL;

	if (queued_requests && !g_queue_is_empty(queued_requests)) {
		queued_dns_request_t *r = g_queue_pop_head(queued_requests);
		req->host = g_strdup(r->params.hostname);
		req->port = r->params.port;
		req->callback = r->callback;
		req->data = r->data;

		gaim_debug_info("dns",
				   "Processing queued DNS query for '%s' with child %d\n",
				   req->host, req->dns_pid);

		if (send_dns_request_to_child(req, &(r->params)) != 0) {
			req_free(req);
			req = NULL;

			gaim_debug_warning("dns",
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

static void
host_resolved(gpointer data, gint source, GaimInputCondition cond)
{
	pending_dns_request_t *req = (pending_dns_request_t*)data;
	int rc, err;
	GSList *hosts = NULL;
	struct sockaddr *addr = NULL;
	size_t addrlen;

	gaim_debug_info("dns", "Got response for '%s'\n", req->host);
	gaim_input_remove(req->inpa);

	rc = read(req->fd_out, &err, sizeof(err));
	if ((rc == 4) && (err != 0))
	{
		char message[1024];
#ifdef HAVE_GETADDRINFO
		g_snprintf(message, sizeof(message), "DNS error: %s (pid=%d)",
				   gai_strerror(err), req->dns_pid);
#else
		g_snprintf(message, sizeof(message), "DNS error: %d (pid=%d)",
				   err, req->dns_pid);
#endif
		gaim_debug_error("dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		release_dns_child(req);
		return;
	}
	if (rc > 0)
	{
		while (rc > 0) {
			rc = read(req->fd_out, &addrlen, sizeof(addrlen));
			if (rc > 0 && addrlen > 0) {
				addr = g_malloc(addrlen);
				rc = read(req->fd_out, addr, addrlen);
				hosts = g_slist_append(hosts, GINT_TO_POINTER(addrlen));
				hosts = g_slist_append(hosts, addr);
			} else {
				break;
			}
		}
	} else if (rc == -1) {
		char message[1024];
		g_snprintf(message, sizeof(message), "Error reading from DNS child: %s",strerror(errno));
		gaim_debug_error("dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		req_free(req);
		return;
	} else if (rc == 0) {
		char message[1024];
		g_snprintf(message, sizeof(message), "EOF reading from DNS child");
		close(req->fd_out);
		gaim_debug_error("dns", "%s\n", message);
		req->callback(NULL, req->data, message);
		req_free(req);
		return;
	}

/*	wait4(req->dns_pid, NULL, WNOHANG, NULL); */

	req->callback(hosts, req->data, NULL);

	release_dns_child(req);
}
/*
 * End the functions for dealing with the DNS child processes.
 */

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port, GaimDnsQueryConnectFunction callback, gpointer data)
{
	pending_dns_request_t *req = NULL;
	dns_params_t dns_params;
	gchar *host_temp;
	gboolean show_debug;

	show_debug = gaim_debug_is_enabled();

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
		if (number_of_dns_children >= MAX_DNS_CHILDREN) {
			queued_dns_request_t *r = g_new(queued_dns_request_t, 1);
			memcpy(&(r->params), &dns_params, sizeof(dns_params));
			r->callback = callback;
			r->data = data;
			if (!queued_requests)
				queued_requests = g_queue_new();
			g_queue_push_tail(queued_requests, r);

			gaim_debug_info("dns",
					   "DNS query for '%s' queued\n", dns_params.hostname);

			return (GaimDnsQueryData *)1;
		}

		req = gaim_dns_new_resolverthread(show_debug);
		if (req == NULL)
		{
			gaim_debug_error("proxy", "oh dear, this is going to explode, I give up\n");
			return NULL;
		}
		send_dns_request_to_child(req, &dns_params);
	}

	req->host = g_strdup(hostname);
	req->port = port;
	req->callback = callback;
	req->data = data;
	req->inpa = gaim_input_add(req->fd_out, GAIM_INPUT_READ, host_resolved, req);

	return (GaimDnsQueryData *)1;
}

#elif defined _WIN32 /* end __unix__ || __APPLE__ */

typedef struct _dns_tdata {
	char *hostname;
	int port;
	GaimDnsQueryConnectFunction callback;
	gpointer data;
	GSList *hosts;
	char *errmsg;
} dns_tdata;

static gboolean dns_main_thread_cb(gpointer data) {
	dns_tdata *td = (dns_tdata*)data;
	if (td->errmsg != NULL) {
		gaim_debug_info("dns", "%s\n", td->errmsg);
	}
	td->callback(td->hosts, td->data, td->errmsg);
	g_free(td->hostname);
	g_free(td->errmsg);
	g_free(td);
	return FALSE;
}

static gpointer dns_thread(gpointer data) {

#ifdef HAVE_GETADDRINFO
	int rc;
	struct addrinfo hints, *res, *tmp;
	char servname[20];
#else
	struct sockaddr_in sin;
	struct hostent *hp;
#endif
	dns_tdata *td = (dns_tdata*)data;

#ifdef HAVE_GETADDRINFO
	g_snprintf(servname, sizeof(servname), "%d", td->port);
	memset(&hints,0,sizeof(hints));

	/* This is only used to convert a service
	 * name to a port number. As we know we are
	 * passing a number already, we know this
	 * value will not be really used by the C
	 * library.
	 */
	hints.ai_socktype = SOCK_STREAM;
	if ((rc = getaddrinfo(td->hostname, servname, &hints, &res)) == 0) {
		tmp = res;
		while(res) {
			td->hosts = g_slist_append(td->hosts,
				GSIZE_TO_POINTER(res->ai_addrlen));
			td->hosts = g_slist_append(td->hosts,
				g_memdup(res->ai_addr, res->ai_addrlen));
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
	} else {
		td->errmsg = g_strdup_printf("DNS getaddrinfo(\"%s\", \"%s\") error: %d", td->hostname, servname, rc);
	}
#else
	if ((hp = gethostbyname(td->hostname))) {
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = htons(td->port);

		td->hosts = g_slist_append(td->hosts,
				GSIZE_TO_POINTER(sizeof(sin)));
		td->hosts = g_slist_append(td->hosts,
				g_memdup(&sin, sizeof(sin)));
	} else {
		td->errmsg = g_strdup_printf("DNS gethostbyname(\"%s\") error: %d", td->hostname, h_errno);
	}
#endif
	/* back to main thread */
	g_idle_add(dns_main_thread_cb, td);
	return 0;
}

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
							  GaimDnsQueryConnectFunction callback, gpointer data)
{
	dns_tdata *td;
	struct sockaddr_in sin;
	GError* err = NULL;

	if(inet_aton(hostname, &sin.sin_addr)) {
		GSList *hosts = NULL;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
		hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));
		callback(hosts, data, NULL);
		return (GaimDnsQueryData *)1;
	}

	gaim_debug_info("dns", "DNS Lookup for: %s\n", hostname);
	td = g_new0(dns_tdata, 1);
	td->hostname = g_strdup(hostname);
	td->port = port;
	td->callback = callback;
	td->data = data;

	if(!g_thread_create(dns_thread, td, FALSE, &err)) {
		gaim_debug_error("dns", "DNS thread create failure: %s\n", err?err->message:"");
		g_error_free(err);
		g_free(td->hostname);
		g_free(td);
		return NULL;
	}
	return (GaimDnsQueryData *)1;
}

#else /* not __unix__ or __APPLE__ or _WIN32 */

typedef struct {
	gpointer data;
	size_t addrlen;
	struct sockaddr *addr;
	GaimDnsQueryConnectFunction callback;
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

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
						 GaimDnsQueryConnectFunction callback, gpointer data)
{
	struct sockaddr_in sin;
	pending_dns_request_t *req;

	if (!inet_aton(hostname, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(hostname))) {
			gaim_debug_error("dns",
					   "gaim_gethostbyname(\"%s\", %d) failed: %d\n",
					   hostname, port, h_errno);
			return NULL;
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
	return (GaimDnsQueryData *)1;
}

#endif /* not __unix__ or __APPLE__ or _WIN32 */
