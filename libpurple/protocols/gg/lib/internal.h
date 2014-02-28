/* $Id$ */

/*
 *  (C) Copyright 2009 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#ifndef LIBGADU_INTERNAL_H
#define LIBGADU_INTERNAL_H

#include "libgadu.h"

#define GG_DEFAULT_CLIENT_VERSION_100 "10.1.0.11070"
#define GG_DEFAULT_CLIENT_VERSION_110 "11.3.45.10771"

#define GG_LOGIN_PARAMS_HAS_FIELD(glp, member) \
	(offsetof(struct gg_login_params, member) < (glp)->struct_size || \
	offsetof(struct gg_login_params, member) <= offsetof(struct gg_login_params, struct_size))

struct gg_dcc7_relay {
	uint32_t addr;
	uint16_t port;
	uint8_t family;
};

typedef struct _gg_chat_list gg_chat_list_t;
struct _gg_chat_list {
	uint64_t id;
	uint32_t version;
	uint32_t participants_count;
	uin_t *participants;

	gg_chat_list_t *next;
};

typedef struct _gg_msg_list gg_msg_list_t;
struct _gg_msg_list {
	int seq;
	uin_t *recipients;
	size_t recipients_count;

	gg_msg_list_t *next;
};

typedef struct _gg_eventqueue gg_eventqueue_t;
struct _gg_eventqueue {
	struct gg_event *event;

	gg_eventqueue_t *next;
};

struct gg_session_private {
	gg_compat_t compatibility;

	gg_chat_list_t *chat_list;
	gg_msg_list_t *sent_messages;

	gg_eventqueue_t *event_queue;
	int check_after_queue;
	int fd_after_queue;

	gg_socket_manager_type_t socket_manager_type;
	gg_socket_manager_t socket_manager;
	void *socket_handle;
	int socket_next_state;
	int socket_is_external;
	enum gg_failure_t socket_failure;

	int time_diff;

	int dummyfds_created;
	int dummyfds[2];

	char **host_white_list;
};

typedef enum
{
	GG_COMPAT_FEATURE_ACK_EVENT,
	GG_COMPAT_FEATURE_LEGACY_CONFER
} gg_compat_feature_t;

typedef struct gg_dcc7_relay gg_dcc7_relay_t;

void * gg_new0(size_t size);
int gg_required_proto(struct gg_session *gs, int protocol_version);
int gg_get_dummy_fd(struct gg_session *sess);

int gg_compat_feature_is_enabled(struct gg_session *sess, gg_compat_feature_t feature);

int gg_pubdir50_handle_reply_sess(struct gg_session *sess, struct gg_event *e, const char *packet, int length);

int gg_resolve(int *fd, int *pid, const char *hostname);
int gg_resolve_pthread(int *fd, void **resolver, const char *hostname);
void gg_resolve_pthread_cleanup(void *resolver, int kill);

int gg_login_hash_sha1_2(const char *password, uint32_t seed, uint8_t *result);

int gg_chat_update(struct gg_session *sess, uint64_t id, uint32_t version, const uin_t *participants, unsigned int participants_count);
gg_chat_list_t *gg_chat_find(struct gg_session *sess, uint64_t id);

uin_t gg_str_to_uin(const char *str, int len);

uint64_t gg_fix64(uint64_t x);
void gg_connection_failure(struct gg_session *gs, struct gg_event *ge,
	enum gg_failure_t failure);

time_t gg_server_time(struct gg_session *gs);

int gg_session_init_ssl(struct gg_session *gs);
void gg_close(struct gg_session *gs);

struct gg_event *gg_eventqueue_add(struct gg_session *sess);

void gg_compat_message_ack(struct gg_session *sess, int seq);

void gg_strarr_free(char **strarr);
char ** gg_strarr_dup(char **strarr);

#endif /* LIBGADU_INTERNAL_H */
