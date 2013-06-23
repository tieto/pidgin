#include <assert.h>
#include <errno.h>
#include <string.h>
#include <gtk/gtk.h>
#include "conversation.h"
#include "network.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include "cc_network.h"
#include "cc_interface.h"
#include "util.h"

/* --- begin constant definitions --- */

#define NETWORK_TIMEOUT_DELAY	40		/* in ms */
#define MAX_ACCEPT_CHECKS	1000

/* --- begin type declarations --- */

struct accept_args {
	PurpleAccount *account;
	struct crazychat *cc;
	char *name;
	guint32 peer_ip;
	guint16 peer_port;
};

struct sock_accept_args {
	PurpleAccount *account;
	struct cc_session *session;
};

/* --- begin function prototypes --- */

/**
 * Creates a server socket and sends a response to the peer.
 * @param account		the purple account sending the ready msg
 * @param session		the peer CrazyChat session
 */
static void cc_net_send_ready(PurpleAccount *account, struct cc_session *session);

/**
 * Handles responses from the CrazyChat session invite dialog box.
 * @param dialog		the dialog box
 * @param response		the dialog box button response
 * @param args			account, crazychat global data, peer name
 */
static void invite_handler(GtkDialog *dialog, gint response,
		struct accept_args *args);

/**
 * Periodically checks the server socket for peer's connection.  Gives up
 * after a set number of checks.
 * @param args			peer session and account
 * @return			TRUE to continue checking, FALSE to stop
 */
static gboolean accept_cb(struct sock_accept_args *args);

/**
 * Initialize CrazyChat network session.  Sets up the UDP socket and port.
 * @param account		the account the session is part of
 * @param session		the CrazyChat network session
 */
static void init_cc_net_session(PurpleAccount *account,
		struct cc_session *session);

/**
 * Handles checking the network for new feature data and sending out the
 * latest features.
 * @param session		the session we're checking for network traffic
 */
static gboolean network_cb(struct cc_session *session);

/**
 * Generates random bytes in the user specified byte buffer.
 * @param buf		the byte buffer
 * @param len		length of the byte buffer
 */
static void generate_randomness(uint8_t buf[], unsigned int len);

/**
 * Sends data over a socket.
 * @param s   socket file descriptor
 * @param buf data buffer
 * @param len data buffer length
 * @return    number of bytes sent or -1 if an error occurred
 */
static int __send(int s, char *buf, int len);

/* --- begin function definitions --- */

void cc_net_send_invite(struct crazychat *cc, char *name, PurpleAccount *account)
{
	struct cc_session *session;
	PurpleConversation *conv;
	PurpleIMConversation *im;
	char buf[BUFSIZ];

	session = cc_find_session(cc, name);
	if (session) return; /* already have a session with this guy */
	session = cc_add_session(cc, name);
	session->state = INVITE;
	conv = purple_conversations_find_with_account(name, account);
	if (!conv) {
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, name);
	}
	im = purple_conversation_get_im_data(conv);
	snprintf(buf, BUFSIZ, "%s%s!%d", CRAZYCHAT_INVITE_CODE,
		purple_network_get_my_ip(-1), cc->tcp_port);
	Debug("Sent invite to %s for port: %d\n", name, cc->tcp_port);
	purple_im_conversation_send(im, buf);
}

void cc_net_recv_invite(PurpleAccount *account, struct crazychat *cc, char *name,
		const char *peer_ip, const char *peer_port)
{
	struct cc_session *session;
	PurpleConversation *conv;
	PurpleConvWindow *convwin;
	char buf[BUFSIZ];
	struct accept_args *args;

	assert(cc);
	assert(name);
	Debug("Received a CrazyChat session invite from %s on port %s!\n",
			name, peer_port);
	session = cc_find_session(cc, name);
	if (!session) {
		Debug("Creating a CrazyChat session invite dialog box!\n");
		conv = purple_conversations_find_with_account(name, account);
		if (conv) convwin = purple_conversation_get_window(conv);
		else convwin = NULL;
		/* pop gtk window asking if want to accept */
		GtkWidget *dialog =
			gtk_dialog_new_with_buttons("CrazyChat Session Invite",
			GTK_WINDOW(convwin),
			GTK_DIALOG_MODAL |
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK,
			GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT,
			NULL);
		snprintf(buf, BUFSIZ, "Would you like to CRaZYchAT with %s?", name);
		GtkWidget *label = gtk_label_new(buf);
		gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
				label);
		args = (struct accept_args*)malloc(sizeof(*args));
		args->account = account;
		args->cc = cc;
		args->name = strdup(name);
		assert(inet_aton(peer_ip, (struct in_addr*)&args->peer_ip));
		args->peer_port = atoi(peer_port);

		g_signal_connect(GTK_OBJECT(dialog), "response",
				G_CALLBACK(invite_handler), args);

		gtk_widget_show_all(dialog);
	}
}

void cc_net_recv_accept(PurpleAccount *account, struct crazychat *cc, char *name,
		const char *peer_ip)
{
	struct cc_session *session;
	struct in_addr peer_addr;

	assert(cc);
	assert(name);
	Debug("Received a CrazyChat session accept!\n");
	session = cc_find_session(cc, name);
	if (session && session->state == INVITE) {
		session->state = ACCEPTED;
		assert(inet_aton(peer_ip, &peer_addr));
		session->peer_ip = peer_addr.s_addr;
		cc_net_send_ready(account, session);
	}
}

static void cc_net_send_ready(PurpleAccount *account, struct cc_session *session)
{
	struct sock_accept_args *args;

	assert(session);
	Debug("Initializing the server socket and sending ready message\n");
	/* create the server socket */
	session->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
	assert(session->tcp_sock != -1);
	int reuse = 1;
	assert(setsockopt(session->tcp_sock, SOL_SOCKET, SO_REUSEADDR,
				&reuse, sizeof(int)) != -1);
	struct sockaddr_in my_addr;
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(session->cc->tcp_port);
	assert(inet_aton(purple_network_get_my_ip(-1),
			&my_addr.sin_addr));
	memset(&my_addr.sin_zero, 0, sizeof(my_addr.sin_zero));
	assert(bind(session->tcp_sock, (struct sockaddr*)&my_addr,
				sizeof(my_addr)) != -1);
	Debug("Listening on port: %d\n", my_addr.sin_port);
	assert(listen(session->tcp_sock, 1) != -1);

	/* socket created, send the ready message */
	PurpleConversation *conv;
	PurpleIMConversation *im;

	conv = purple_conversations_find_with_account(session->name, account);
	if (!conv) {
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account,
				session->name);
	}
	im = purple_conversation_get_im_data(conv);
	purple_im_conversation_send(im, CRAZYCHAT_READY_CODE);

	/* register timer callback for checking socket connection */
	args = (struct sock_accept_args*)malloc(sizeof(*args));
	args->session = session;
	args->account = account;
	session->udp_sock = MAX_ACCEPT_CHECKS;
	session->timer_id = g_timeout_add(NETWORK_TIMEOUT_DELAY,
		(GSourceFunc)accept_cb, args);
}

void cc_net_recv_ready(PurpleAccount *account, struct crazychat *cc, char *name)
{
	struct cc_session *session;
	struct sockaddr_in server_addr, my_addr;
	int sock;

	assert(cc);
	assert(name);
	Debug("Received a CrazyChat session ready!\n");
	session = cc_find_session(cc, name);
	if (session && session->state == ACCEPTED) {
		/* connect to peer */
		session->tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
		assert(session->tcp_sock != -1);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = session->peer_port;
		server_addr.sin_addr.s_addr = session->peer_ip;
		memset(&(server_addr.sin_zero), 0,
				sizeof(server_addr.sin_zero));
		assert(connect(session->tcp_sock,
				(struct sockaddr*)&server_addr,
				sizeof(server_addr)) != -1);
		Debug("Connecting to peer on port %d\n", session->peer_port);

		/* now set state */
		session->state = CONNECTED;
		init_cc_net_session(account, session);
	}
}

static void invite_handler(GtkDialog *dialog, gint response, struct accept_args *args)
{
	struct cc_session *session;
	char buf[BUFSIZ];
	PurpleConversation *conv;
	PurpleIMConversation *im;

	if (response == GTK_RESPONSE_ACCEPT) {
		assert(args);
		session = cc_find_session(args->cc, args->name);
		assert(!session);
		session = cc_add_session(args->cc, args->name);
		session->state = ACCEPTED;
		session->peer_ip = args->peer_ip;
		session->peer_port = args->peer_port;
		snprintf(buf, BUFSIZ, "%s%s", CRAZYCHAT_ACCEPT_CODE,
			purple_network_get_my_ip(-1));
		conv = purple_conversations_find_with_account(args->name,
				args->account);
		if (!conv) {
			conv = purple_conversation_new(PURPLE_CONV_TYPE_IM,
				args->account, args->name);
		}
		im = purple_conversation_get_im_data(conv);
		purple_im_conversation_send(im, buf);
	}
	free(args->name);
	free(args);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean accept_cb(struct sock_accept_args *args)
{
	fd_set fds;
	struct timeval zero;
	int ret;
	PurpleAccount *account;
	struct cc_session *session;

	assert(args);
	account = args->account;
	session = args->session;
	assert(account);
	assert(session);

	/* set select to check on our tcp socket */
	FD_ZERO(&fds);
	FD_SET(session->tcp_sock, &fds);
	memset(&zero, 0, sizeof(zero));

	/* check socket */
	ret = select(session->tcp_sock+1,&fds, NULL, NULL, &zero);
	assert(ret != -1);

	if (ret) { /* got something to check */
		Debug("Checking pending connection\n");
		int sock;
		struct sockaddr_in client_addr;
		socklen_t sin_size;

		sin_size = sizeof(client_addr);
		sock = accept(session->tcp_sock,
				(struct sockaddr*)&client_addr, &sin_size);
		assert(sock != -1);

		/* check if it's a match */
		if (client_addr.sin_addr.s_addr == session->peer_ip) {
			/* cool, we're set */
			Debug("Accepted tcp connect from %s\n", session->name);
			close(session->tcp_sock);
			session->tcp_sock = sock;
			session->state = CONNECTED;
			session->timer_id = 0;
			init_cc_net_session(account, session);
			Debug("Will start sending to port %d\n",
					session->peer_port);
			free(args);
			return FALSE;
		}
	}

	session->udp_sock--;

	if (!session->udp_sock) { /* timed out */
		/* remove session from session list */
		cc_remove_session(session->cc, session);
		free(args);
		return FALSE;
	}

	return TRUE;
}

static void init_cc_net_session(PurpleAccount *account,
		struct cc_session *session)
{
	struct sockaddr_in my_addr;
	struct sockaddr_in peer_addr;
	int reuse;

	/* send/obtain the udp port information */

	assert(__send(session->tcp_sock, (char*)&session->cc->udp_port,
			sizeof(session->cc->udp_port)) ==
			sizeof(session->cc->udp_port));
	assert(recv(session->tcp_sock, (char*)&session->peer_port,
			sizeof(session->peer_port), 0) ==
			sizeof(session->peer_port));

	Debug("Established a CrazyChat session with %s!\n", session->name);

	/* connect the udp sockets */

	session->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

	assert(!setsockopt(session->udp_sock, SOL_SOCKET, SO_REUSEADDR,
			&reuse, sizeof(reuse)));

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(session->cc->udp_port);
	assert(inet_aton(purple_network_get_my_ip(-1),
				&my_addr.sin_addr));
	memset(my_addr.sin_zero, 0, sizeof(my_addr.sin_zero));
	assert(!bind(session->udp_sock, (struct sockaddr*)&my_addr,
			sizeof(my_addr)));
	session->peer.sin_family = AF_INET;
	session->peer.sin_port = htons(session->peer_port);
	session->peer.sin_addr.s_addr = session->peer_ip;
	memset(&session->peer.sin_zero, 0, sizeof(session->peer.sin_zero));

	Debug("Bound udp sock to port %d, connecting to port %d\n",
		session->cc->udp_port, session->peer_port);

	memset(&session->features, 0, sizeof(session->features));

	session->output = init_output(&session->features, session);

	session->filter = Filter_Initialize();

	/* initialize timer callback */
	session->timer_id = g_timeout_add(NETWORK_TIMEOUT_DELAY,
		(GSourceFunc)network_cb, session);

	/* initialize input subsystem if not initialized */
	if (!session->cc->features_state) {
		session->cc->input_data = init_input(session->cc);
		session->cc->features_state = 1;
	}
}

static gboolean network_cb(struct cc_session *session)
{
	fd_set fds;
	struct timeval zero;
	int ret;
	int command;
	struct cc_features *features;

	assert(session);

	Debug("Checking for data\n");

	/* set select to check on our tcp socket */
	FD_ZERO(&fds);
	FD_SET(session->tcp_sock, &fds);
	memset(&zero, 0, sizeof(zero));

	/* check tcp socket */
	ret = select(session->tcp_sock+1, &fds, NULL, NULL, &zero);
	assert(ret != -1);

	while (ret) {
		ret = recv(session->tcp_sock, &command, sizeof(command), 0);
		assert(ret != -1);
		if (!ret) {
			/* tcp connection closed, destroy connection */
			gtk_widget_destroy(session->output->widget);
			return FALSE;
		}
		assert(ret == sizeof(command));

		FD_ZERO(&fds);
		FD_SET(session->tcp_sock, &fds);
		ret = select(session->tcp_sock+1, &fds, NULL, NULL, &zero);
		assert(ret != -1);
	}

	/* set select to check on our udp socket */
	FD_ZERO(&fds);
	FD_SET(session->udp_sock, &fds);
	memset(&zero, 0, sizeof(zero));

	/* check udp socket */
	ret = select(session->udp_sock+1, &fds, NULL, NULL, &zero);
	assert(ret != -1);

	features = &session->features;

	while (ret) { /* have data, let's copy it for output */
		struct sockaddr_in from;
		int fromlen;
		ret = recvfrom(session->udp_sock, &session->features,
				sizeof(session->features),
				0, (struct sockaddr*)&from, &fromlen);
		Debug("Received %d bytes from port %d\n", ret,
				ntohs(from.sin_port));
		filter(features, session->filter);
		Debug("\thead size: %d\n", features->head_size);
		Debug("\topen: left(%s), right(%s), mouth(%s)\n",
				features->left_eye_open ? "yes" : "no",
				features->right_eye_open ? "yes" : "no",
				features->mouth_open ? "yes" : "no");
		Debug("\thead rotation: x(%d), y(%d), z(%d)\n",
			features->head_x_rot, features->head_y_rot,
			features->head_z_rot);
		Debug("\tx(%d), y(%d)\n", features->x, features->y);
		if (ret == -1) {
			perror("wtf:");
		}
		assert(ret != -1);

		FD_ZERO(&fds);
		FD_SET(session->udp_sock, &fds);
		ret = select(session->udp_sock+1, &fds, NULL, NULL, &zero);
		assert(ret != -1);
	}

#ifdef _DISABLE_QT_
	struct cc_features bogus;
	features = &bogus;
	generate_randomness((uint8_t*)features, sizeof(*features));
#else
	features = &session->cc->input_data->face;
#endif
	assert(sendto(session->udp_sock, (char*)features,
			sizeof(*features), 0, (struct sockaddr*)&session->peer,
			sizeof(session->peer)) == sizeof(*features));
	Debug("Sent %d bytes\n", sizeof(*features));
	Debug("\thead size: %d\n", features->head_size);
	Debug("\topen: left(%s), right(%s), mouth(%s)\n",
			features->left_eye_open ? "yes" : "no",
			features->right_eye_open ? "yes" : "no",
			features->mouth_open ? "yes" : "no");
	Debug("\thead rotation: x(%d), y(%d), z(%d)\n",
		features->head_x_rot, features->head_y_rot,
		features->head_z_rot);
	Debug("\tx(%d), y(%d)\n", features->x, features->y);

	/* clear easter egg */
	features->mode = 0;

	return TRUE;
}

static void generate_randomness(uint8_t buf[], unsigned int len)
{
	int fd;

	fd = open("/dev/random", O_RDONLY);
	assert(fd != -1);

	assert(read(fd, buf, len) == len);
	close(fd);
}

static int __send(int s, char *buf, int len)
{
	int total = 0;		/* how many bytes we've sent */
	int bytesleft = len;	/* how many we have left to send */
	int n;

	while (total < len) {
		n = send(s, buf + total, bytesleft, 0);
		if (n == -1) {
			Debug("ERROR: %s\n", g_strerror(errno));
			return -1;
		}
		total += n;
		bytesleft -= n;
	}

	return total;
}
