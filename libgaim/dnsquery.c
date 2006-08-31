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
#include "dnsquery.h"
#include "notify.h"
#include "prefs.h"
#include "util.h"

/**************************************************************************
 * DNS query API
 **************************************************************************/

typedef struct _GaimDnsQueryResolverProcess GaimDnsQueryResolverProcess;

struct _GaimDnsQueryData {
	char *hostname;
	int port;
	GaimDnsQueryConnectFunction callback;
	gpointer data;
	guint timeout;

#if defined(__unix__) || defined(__APPLE__)
	GaimDnsQueryResolverProcess *resolver;
#elif defined _WIN32 /* end __unix__ || __APPLE__ */
	GThread *resolver;
	GSList *hosts;
	gchar *error_message;
#endif
};

#if defined(__unix__) || defined(__APPLE__)

#define MAX_DNS_CHILDREN 4

/*
 * This structure keeps a reference to a child resolver process.
 */
struct _GaimDnsQueryResolverProcess {
	guint inpa;
	int fd_in, fd_out;
	pid_t dns_pid;
};

static GSList *free_dns_children = NULL;
static GQueue *queued_requests = NULL;

static int number_of_dns_children = 0;

/*
 * This is a convenience struct used to pass data to
 * the child resolver process.
 */
typedef struct {
	char hostname[512];
	int port;
} dns_params_t;
#endif

static void
gaim_dnsquery_resolved(GaimDnsQueryData *query_data, GSList *hosts)
{
	gaim_debug_info("dnsquery", "IP resolved for %s\n", query_data->hostname);
	if (query_data->callback != NULL)
		query_data->callback(hosts, query_data->data, NULL);
	else
	{
		/*
		 * Callback is a required parameter, but it can get set to
		 * NULL if we cancel a thread-based DNS lookup.  So we need
		 * to free hosts.
		 */
		while (hosts != NULL)
		{
			hosts = g_slist_remove(hosts, hosts->data);
			g_free(hosts->data);
			hosts = g_slist_remove(hosts, hosts->data);
		}
	}

	gaim_dnsquery_destroy(query_data);
}

static void
gaim_dnsquery_failed(GaimDnsQueryData *query_data, const gchar *error_message)
{
	gaim_debug_info("dnsquery", "%s\n", error_message);
	if (query_data->callback != NULL)
		query_data->callback(NULL, query_data->data, error_message);
	gaim_dnsquery_destroy(query_data);
}

#if defined(__unix__) || defined(__APPLE__)

/*
 * Unix!
 */

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
gaim_dnsquery_resolver_run(int child_out, int child_in, gboolean show_debug)
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
/*
 * End the DNS resolver child process functions.
 */

/*
 * Begin the functions for dealing with the DNS child processes.
 */
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
gaim_dnsquery_resolver_destroy(GaimDnsQueryResolverProcess *resolver)
{
	g_return_if_fail(resolver != NULL);

	/*
	 * We might as well attempt to kill our child process.  It really
	 * doesn't matter if this fails, because children will expire on
	 * their own after a few seconds.
	 */
	if (resolver->dns_pid > 0)
		kill(resolver->dns_pid, SIGKILL);

	if (resolver->inpa != 0)
		gaim_input_remove(resolver->inpa);

	close(resolver->fd_in);
	close(resolver->fd_out);

	g_free(resolver);

	number_of_dns_children--;
}

static GaimDnsQueryResolverProcess *
gaim_dnsquery_resolver_new(gboolean show_debug)
{
	GaimDnsQueryResolverProcess *resolver;
	int child_out[2], child_in[2];

	/* Create pipes for communicating with the child process */
	if (pipe(child_out) || pipe(child_in)) {
		gaim_debug_error("dns",
				   "Could not create pipes: %s\n", strerror(errno));
		return NULL;
	}

	resolver = g_new(GaimDnsQueryResolverProcess, 1);
	resolver->inpa = 0;

	cope_with_gdb_brokenness();

	/* "Go fork and multiply." --Tommy Caldwell (Emily's dad, not the climber) */
	resolver->dns_pid = fork();

	/* If we are the child process... */
	if (resolver->dns_pid == 0) {
		/* We should not access the parent's side of the pipes, so close them */
		close(child_out[0]);
		close(child_in[1]);

		gaim_dnsquery_resolver_run(child_out[1], child_in[0], show_debug);
		/* The thread calls _exit() rather than returning, so we never get here */
	}

	/* We should not access the child's side of the pipes, so close them */
	close(child_out[1]);
	close(child_in[0]);
	if (resolver->dns_pid == -1) {
		gaim_debug_error("dns",
				   "Could not create child process for DNS: %s\n",
				   strerror(errno));
		gaim_dnsquery_resolver_destroy(resolver);
		return NULL;
	}

	resolver->fd_out = child_out[0];
	resolver->fd_in = child_in[1];
	number_of_dns_children++;
	gaim_debug_info("dns",
			   "Created new DNS child %d, there are now %d children.\n",
			   resolver->dns_pid, number_of_dns_children);

	return resolver;
}

/**
 * @return TRUE if the request was sent succesfully.  FALSE
 *         if the request could not be sent.  This isn't
 *         necessarily an error.  If the child has expired,
 *         for example, we won't be able to send the message.
 */
static gboolean
send_dns_request_to_child(GaimDnsQueryData *query_data,
		GaimDnsQueryResolverProcess *resolver)
{
	pid_t pid;
	dns_params_t dns_params;
	int rc;
	char ch;

	/* This waitpid might return the child's PID if it has recently
	 * exited, or it might return an error if it exited "long
	 * enough" ago that it has already been reaped; in either
	 * instance, we can't use it. */
	pid = waitpid(resolver->dns_pid, NULL, WNOHANG);
	if (pid > 0) {
		gaim_debug_warning("dns", "DNS child %d no longer exists\n",
				resolver->dns_pid);
		gaim_dnsquery_resolver_destroy(resolver);
		return FALSE;
	} else if (pid < 0) {
		gaim_debug_warning("dns", "Wait for DNS child %d failed: %s\n",
				resolver->dns_pid, strerror(errno));
		gaim_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	/* Copy the hostname and port into a single data structure */
	strncpy(dns_params.hostname, query_data->hostname, sizeof(dns_params.hostname) - 1);
	dns_params.hostname[sizeof(dns_params.hostname) - 1] = '\0';
	dns_params.port = query_data->port;

	/* Send the data structure to the child */
	rc = write(resolver->fd_in, &dns_params, sizeof(dns_params));
	if (rc < 0) {
		gaim_debug_error("dns", "Unable to write to DNS child %d: %d\n",
				resolver->dns_pid, strerror(errno));
		gaim_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	g_return_val_if_fail(rc == sizeof(dns_params), -1);

	/* Did you hear me? (This avoids some race conditions) */
	rc = read(resolver->fd_out, &ch, sizeof(ch));
	if (rc != 1 || ch != 'Y')
	{
		gaim_debug_warning("dns",
				"DNS child %d not responding. Killing it!\n",
				resolver->dns_pid);
		gaim_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	gaim_debug_info("dns",
			"Successfully sent DNS request to child %d\n",
			resolver->dns_pid);

	query_data->resolver = resolver;

	return TRUE;
}

static void host_resolved(gpointer data, gint source, GaimInputCondition cond);

static void
handle_next_queued_request()
{
	GaimDnsQueryData *query_data;
	GaimDnsQueryResolverProcess *resolver;

	if ((queued_requests == NULL) || (g_queue_is_empty(queued_requests)))
		/* No more DNS queries, yay! */
		return;

	query_data = g_queue_pop_head(queued_requests);

	/*
	 * If we have any children, attempt to have them perform the DNS
	 * query.  If we're able to send the query then resolver will be
	 * set to the GaimDnsQueryResolverProcess.  Otherwise, resolver
	 * will be NULL and we'll need to create a new DNS request child.
	 */
	while (free_dns_children != NULL)
	{
		resolver = free_dns_children->data;
		free_dns_children = g_slist_remove(free_dns_children, resolver);

		if (send_dns_request_to_child(query_data, resolver))
			/* We found an acceptable child, yay */
			break;
	}

	/* We need to create a new DNS request child */
	if (query_data->resolver == NULL)
	{
		if (number_of_dns_children >= MAX_DNS_CHILDREN)
		{
			/* Apparently all our children are busy */
			g_queue_push_head(queued_requests, query_data);
			return;
		}

		resolver = gaim_dnsquery_resolver_new(gaim_debug_is_enabled());
		if (resolver == NULL)
		{
			gaim_dnsquery_failed(query_data, _("Unable to create new resolver process\n"));
			return;
		}
		if (!send_dns_request_to_child(query_data, resolver))
		{
			gaim_dnsquery_failed(query_data, _("Unable to send request to resolver process\n"));
			return;
		}
	}

	query_data->resolver->inpa = gaim_input_add(query_data->resolver->fd_out,
			GAIM_INPUT_READ, host_resolved, query_data);
}

/*
 * End the functions for dealing with the DNS child processes.
 */

static void
host_resolved(gpointer data, gint source, GaimInputCondition cond)
{
	GaimDnsQueryData *query_data;
	int rc, err;
	GSList *hosts = NULL;
	struct sockaddr *addr = NULL;
	size_t addrlen;
	char message[1024];

	query_data = data;

	gaim_debug_info("dns", "Got response for '%s'\n", query_data->hostname);
	gaim_input_remove(query_data->resolver->inpa);
	query_data->resolver->inpa = 0;

	rc = read(query_data->resolver->fd_out, &err, sizeof(err));
	if ((rc == 4) && (err != 0))
	{
#ifdef HAVE_GETADDRINFO
		g_snprintf(message, sizeof(message), _("Error resolving %s:\n%s"),
				query_data->hostname, gai_strerror(err));
#else
		g_snprintf(message, sizeof(message), _("Error resolving %s: %d"),
				query_data->hostname, err);
#endif
		gaim_dnsquery_failed(query_data, message);

	} else if (rc > 0) {
		/* Success! */
		while (rc > 0) {
			rc = read(query_data->resolver->fd_out, &addrlen, sizeof(addrlen));
			if (rc > 0 && addrlen > 0) {
				addr = g_malloc(addrlen);
				rc = read(query_data->resolver->fd_out, addr, addrlen);
				hosts = g_slist_append(hosts, GINT_TO_POINTER(addrlen));
				hosts = g_slist_append(hosts, addr);
			} else {
				break;
			}
		}
		/*	wait4(resolver->dns_pid, NULL, WNOHANG, NULL); */
		gaim_dnsquery_resolved(query_data, hosts);

	} else if (rc == -1) {
		g_snprintf(message, sizeof(message), _("Error reading from resolver process:\n%s"), strerror(errno));
		gaim_dnsquery_failed(query_data, message);

	} else if (rc == 0) {
		g_snprintf(message, sizeof(message), _("EOF while reading from resolver process"));
		gaim_dnsquery_failed(query_data, message);
	}

	handle_next_queued_request();
}

static gboolean
resolve_host(gpointer data)
{
	GaimDnsQueryData *query_data;

	query_data = data;
	query_data->timeout = 0;

	handle_next_queued_request();

	return FALSE;
}

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
				GaimDnsQueryConnectFunction callback, gpointer data)
{
	GaimDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port     != 0, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	query_data = g_new(GaimDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;
	query_data->resolver = NULL;

	if (!queued_requests)
		queued_requests = g_queue_new();
	g_queue_push_tail(queued_requests, query_data);

	gaim_debug_info("dns", "DNS query for '%s' queued\n", query_data->hostname);

	query_data->timeout = gaim_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#elif defined _WIN32 /* end __unix__ || __APPLE__ */

/*
 * Windows!
 */

static gboolean
dns_main_thread_cb(gpointer data)
{
	GaimDnsQueryData *query_data;

	query_data = data;

	if (query_data->error_message != NULL)
		gaim_dnsquery_failed(query_data, query_data->error_message);
	else
	{
		GSList *hosts;

		/* We don't want gaim_dns_query_resolved() to free(hosts) */
		hosts = query_data->hosts;
		query_data->hosts = NULL;
		gaim_dnsquery_resolved(query_data, hosts);
	}

	return FALSE;
}

static gpointer
dns_thread(gpointer data)
{
	GaimDnsQueryData *query_data;
#ifdef HAVE_GETADDRINFO
	int rc;
	struct addrinfo hints, *res, *tmp;
	char servname[20];
#else
	struct sockaddr_in sin;
	struct hostent *hp;
#endif

	query_data = data;

#ifdef HAVE_GETADDRINFO
	g_snprintf(servname, sizeof(servname), "%d", query_data->port);
	memset(&hints,0,sizeof(hints));

	/*
	 * This is only used to convert a service
	 * name to a port number. As we know we are
	 * passing a number already, we know this
	 * value will not be really used by the C
	 * library.
	 */
	hints.ai_socktype = SOCK_STREAM;
	if ((rc = getaddrinfo(query_data->hostname, servname, &hints, &res)) == 0) {
		tmp = res;
		while(res) {
			query_data->hosts = g_slist_append(query_data->hosts,
				GSIZE_TO_POINTER(res->ai_addrlen));
			query_data->hosts = g_slist_append(query_data->hosts,
				g_memdup(res->ai_addr, res->ai_addrlen));
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
	} else {
		query_data->error_message = g_strdup_printf(_("Error resolving %s:\n%s"), query_data->hostname, gai_strerror(rc));
	}
#else
	if ((hp = gethostbyname(query_data->hostname))) {
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = htons(query_data->port);

		query_data->hosts = g_slist_append(query_data->hosts,
				GSIZE_TO_POINTER(sizeof(sin)));
		query_data->hosts = g_slist_append(query_data->hosts,
				g_memdup(&sin, sizeof(sin)));
	} else {
		query_data->error_message = g_strdup_printf(_("Error resolving %s: %d"), query_data->hostname, h_errno);
	}
#endif

	/* back to main thread */
	g_idle_add(dns_main_thread_cb, query_data);

	return 0;
}

static gboolean
resolve_host(gpointer data)
{
	GaimDnsQueryData *query_data;
	struct sockaddr_in sin;
	GError *err = NULL;

	query_data = data;
	query_data->timeout = 0;

	if (inet_aton(query_data->hostname, &sin.sin_addr))
	{
		/*
		 * The given "hostname" is actually an IP address, so we
		 * don't need to do anything.
		 */
		GSList *hosts = NULL;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(query_data->port);
		hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
		hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));
		gaim_dnsquery_resolved(query_data, hosts);
	}
	else
	{
		/*
		 * Spin off a separate thread to perform the DNS lookup so
		 * that we don't block the UI.
		 */
		query_data->resolver = g_thread_create(dns_thread,
				query_data, FALSE, &err);
		if (query_data->resolver == NULL)
		{
			char message[1024];
			g_snprintf(message, sizeof(message), _("Thread creation failure: %s"),
					err ? err->message : _("Unknown reason"));
			g_error_free(err);
			gaim_dnsquery_failed(query_data, message);
		}
	}

	return FALSE;
}

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
				GaimDnsQueryConnectFunction callback, gpointer data)
{
	GaimDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port     != 0, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	gaim_debug_info("dnsquery", "Performing DNS lookup for %s\n", hostname);

	query_data = g_new(GaimDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;
	query_data->error_message = NULL;
	query_data->hosts = NULL;

	/* Don't call the callback before returning */
	query_data->timeout = gaim_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#else /* not __unix__ or __APPLE__ or _WIN32 */

/*
 * We weren't able to do anything fancier above, so use the
 * fail-safe name resolution code, which is blocking.
 */

static gboolean
resolve_host(gpointer data)
{
	GaimDnsQueryData *query_data;
	struct sockaddr_in sin;
	GSList *hosts = NULL;

	query_data = data;
	query_data->timeout = 0;

	if (!inet_aton(query_data->hostname, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(query_data->hostname))) {
			char message[1024];
			g_snprintf(message, sizeof(message), _("Error resolving %s: %d"),
					query_data->hostname, h_errno);
			gaim_dnsquery_failed(query_data, message);
			return FALSE;
		}
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
	} else
		sin.sin_family = AF_INET;
	sin.sin_port = htons(query_data->port);

	hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
	hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));

	gaim_dnsquery_resolved(query_data, hosts);

	return FALSE;
}

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
				GaimDnsQueryConnectFunction callback, gpointer data)
{
	GaimDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port     != 0, NULL);

	query_data = g_new(GaimDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;

	/* Don't call the callback before returning */
	query_data->timeout = gaim_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#endif /* not __unix__ or __APPLE__ or _WIN32 */

void
gaim_dnsquery_destroy(GaimDnsQueryData *query_data)
{
#if defined(__unix__) || defined(__APPLE__)
	if (query_data->resolver != NULL)
		/*
		 * Ideally we would tell our resolver child to stop resolving
		 * shit and then we would add it back to the free_dns_children
		 * linked list.  However, it's hard to tell children stuff,
		 * they just don't listen.
		 */
		gaim_dnsquery_resolver_destroy(query_data->resolver);
#elif defined _WIN32 /* end __unix__ || __APPLE__ */
	if (query_data->resolver != NULL)
	{
		/*
		 * It's not really possible to kill a thread.  So instead we
		 * just set the callback to NULL and let the DNS lookup
		 * finish.
		 */
		query_data->callback = NULL;
		return;
	}

	while (query_data->hosts != NULL)
	{
		/* Discard the length... */
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
		/* Free the address... */
		g_free(query_data->hosts->data);
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
	}
	g_free(query_data->error_message);
#endif

	if (query_data->timeout > 0)
		gaim_timeout_remove(query_data->timeout);

	g_free(query_data->hostname);
	g_free(query_data);
}

void
gaim_dnsquery_init(void)
{
#ifdef _WIN32
	if (!g_thread_supported())
		g_thread_init(NULL);
#endif
}

void
gaim_dnsquery_uninit(void)
{
#if defined(__unix__) || defined(__APPLE__)
	while (free_dns_children != NULL)
	{
		gaim_dnsquery_resolver_destroy(free_dns_children->data);
		free_dns_children = g_slist_remove(free_dns_children, free_dns_children->data);
	}
#endif
}
