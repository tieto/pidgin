#ifndef _MSN_NEXUS_H_
#define _MSN_NEXUS_H_

typedef struct _MsnNexus MsnNexus;

#include "session.h"

struct _MsnNexus
{
	MsnSession *session;

	char *login_host;
	char *login_path;
	GHashTable *challenge_data;
};

void msn_nexus_connect(MsnNexus *nexus);
MsnNexus *msn_nexus_new(MsnSession *session);
void msn_nexus_destroy(MsnNexus *nexus);

#endif /* _MSN_NEXUS_H_ */
