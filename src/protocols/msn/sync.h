#ifndef _MSN_SYNC_H_
#define _MSN_SYNC_H_

typedef struct _MsnSync MsnSync;

#include "session.h"
#include "table.h"
#include "user.h"

struct _MsnSync
{
	MsnSession *session;
	MsnTable *cbs_table;
	MsnTable *old_cbs_table;

	int num_users;
	int total_users;
	int num_groups;
	int total_groups;
	MsnUser *last_user;
};

void msn_sync_init(void);
void msn_sync_end(void);

MsnSync * msn_sync_new(MsnSession *session);
void msn_sync_destroy(MsnSync *sync);

#endif /* _MSN_SYNC_H_ */
