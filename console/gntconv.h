#ifndef _GNT_CONV_H
#define _GNT_CONV_H

#include "conversation.h"

GaimConversationUiOps *gg_conv_get_ui_ops(void);

void gg_conversation_init(void);

void gg_conversation_uninit(void);

/* Set a conversation as active in a contactized conversation */
void gg_conversation_set_active(GaimConversation *conv);

#endif
