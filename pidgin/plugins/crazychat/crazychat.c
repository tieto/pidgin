#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cc_interface.h"

struct cc_session_node {
	struct cc_session session;
	struct cc_session_node *next;
};

struct cc_session *cc_find_session(struct crazychat *cc, char *handle)
{
	struct cc_session_node *curr;

	assert(cc);
	assert(handle);

	curr = cc->sessions;
	while (curr) {
		struct cc_session *session = &curr->session;
		if (!strncmp(session->name, handle, strlen(session->name))) {
			return session;
		}
		curr = curr->next;
	}
	return NULL;
}

struct cc_session *cc_add_session(struct crazychat *cc, char *handle)
{
	struct cc_session_node *curr;

	assert(cc);
	assert(handle);

	if (!cc->sessions) {
		cc->sessions = (struct cc_session_node*)
			malloc(sizeof(*cc->sessions));
		memset(cc->sessions, 0, sizeof(*cc->sessions));
		cc->sessions->session.cc = cc;
		cc->sessions->session.name = strdup(handle);
		return &cc->sessions->session;
	} else {
		if (!strncmp(cc->sessions->session.name, handle,
				strlen(cc->sessions->session.name))) {
			return &cc->sessions->session;
		}
	}

	curr = cc->sessions;
	while (curr->next) {
		struct cc_session *session = &curr->next->session;
		if (!strncmp(session->name, handle, strlen(session->name))) {
			return session;
		}
		curr = curr->next;
	}
	curr->next = (struct cc_session_node*)malloc(sizeof(*curr->next));
	memset(curr->next, 0, sizeof(*curr->next));
	curr->next->session.cc = cc;
	curr->next->session.name = strdup(handle);
	return &curr->next->session;
}

void cc_remove_session(struct crazychat *cc, struct cc_session *session)
{
	struct cc_session_node *curr, *prev;

	assert(cc);
	assert(session);

	assert(cc->sessions);

	curr = cc->sessions;
	prev = NULL;

	while (curr) {
		if (&curr->session == session) {
			if (prev) {
				prev->next = curr->next;
			} else {
				cc->sessions = curr->next;
			}
			/* destroy curr */
			free(curr);
			g_source_remove(session->timer_id);
			free(session->name);
			free(session);
			return;
		}
		prev = curr;
		curr = curr->next;
	}

	assert(0);
}
