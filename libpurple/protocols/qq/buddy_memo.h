#ifndef _QQ_BUDDY_MEMO_H_
#define _QQ_BUDDY_MEMO_H_

#include <glib.h>
#include "connection.h"
#include "blist.h"

#define QQ_BUDDY_MEMO_REQUEST_SUCCESS 0x00

/* clan command for memo */
enum
{ 
	QQ_BUDDY_MEMO_MODIFY = 0x01,	/* upload memo */
	QQ_BUDDY_MEMO_REMOVE,		/* remove memo */
	QQ_BUDDY_MEMO_GET		/* get memo */
};


void qq_process_get_buddy_memo(PurpleConnection *gc, guint8* data, gint data_len, guint32 update_class, guint32 action);

void qq_request_buddy_memo(PurpleConnection *gc, guint32 bd_uid, guint32 update_class, guint32 action);

#endif

