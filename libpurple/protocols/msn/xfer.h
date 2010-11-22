
#include "slpcall.h"

void msn_xfer_init(PurpleXfer *xfer);
void msn_xfer_cancel(PurpleXfer *xfer);

gssize msn_xfer_write(const guchar *data, gsize len, PurpleXfer *xfer);
gssize msn_xfer_read(guchar **data, PurpleXfer *xfer);

void msn_xfer_completed_cb(MsnSlpCall *slpcall,
						   const guchar *body, gsize size);
void msn_xfer_end_cb(MsnSlpCall *slpcall, MsnSession *session);
