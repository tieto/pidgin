#include "module.h"

MODULE = Gaim::Conversation::IM  PACKAGE = Gaim::Conversation::IM  PREFIX = gaim_conv_im_
PROTOTYPES: ENABLE

Gaim::Conversation::IM
new(account, name)
	Gaim::Account account
	const char *name
CODE:
	RETVAL = GAIM_CONV_IM(gaim_conversation_new(GAIM_CONV_IM, account, name));
OUTPUT:
	RETVAL

void
DESTROY(im)
	Gaim::Conversation::IM im
CODE:
	gaim_conversation_destroy(gaim_conv_im_get_conversation(im));


Gaim::Conversation
gaim_conv_im_get_conversation(im)
	Gaim::Conversation::IM im

void
gaim_conv_im_write(im, who, message, flags)
	Gaim::Conversation::IM im
	const char *who
	const char *message
	int flags
CODE:
	gaim_conv_im_write(im, who, message, flags, time(NULL));

void
gaim_conv_im_send(im, message)
	Gaim::Conversation::IM im
	const char *message


MODULE = Gaim::Conversation::IM  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

void
ims()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_get_ims(); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(GAIM_CONV_IM(l->data),
			"Gaim::Conversation")));
	}
