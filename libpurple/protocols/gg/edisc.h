#ifndef _GGP_EDISC_H
#define _GGP_EDISC_H

#include <internal.h>

typedef struct _ggp_edisc_session_data ggp_edisc_session_data;

void ggp_edisc_setup(PurpleConnection *gc);
void ggp_edisc_cleanup(PurpleConnection *gc);

gboolean ggp_edisc_xfer_can_receive_file(PurpleConnection *gc, const char *who);

void ggp_edisc_xfer_send_file(PurpleConnection *gc, const char *who,
	const char *filename);

PurpleXfer * ggp_edisc_xfer_new(PurpleConnection *gc, const char *who);

#endif /* _GGP_EDISC_H */
