/**
 * @file gntconv.h GNT Conversation API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _GNT_CONV_H
#define _GNT_CONV_H

#include <gnt.h>
#include <gntwidget.h>
#include <gntmenuitem.h>

#include "conversation.h"

/* Grabs the conv out of a PurpleConverstation */
#define FINCH_CONV(conv) ((FinchConv *)purple_conversation_get_ui_data(conv))

/***************************************************************************
 * @name GNT Conversations API
 ***************************************************************************/
/*@{*/

typedef struct _FinchConv FinchConv;
typedef struct _FinchConvChat FinchConvChat;
typedef struct _FinchConvIm FinchConvIm;

typedef enum
{
	FINCH_CONV_NO_SOUND     = 1 << 0,
} FinchConversationFlag;

struct _FinchConv
{
	GList *list;
	PurpleConversation *active_conv;

	GntWidget *window;        /* the container */
	GntWidget *entry;         /* entry */
	GntWidget *tv;            /* text-view */
	GntWidget *menu;
	GntWidget *info;
	GntMenuItem *plugins;
	FinchConversationFlag flags;

	union
	{
		FinchConvChat *chat;
		FinchConvIm *im;
	} u;
};

struct _FinchConvChat
{
	GntWidget *userlist;       /* the userlist */
	void *pad1;
	void *pad2;
};

struct _FinchConvIm
{
	GntMenuItem *sendto;
	void *something_for_later;
};

/**
 * Get the ui-functions.
 *
 * @return The PurpleConversationUiOps populated with the appropriate functions.
 */
PurpleConversationUiOps *finch_conv_get_ui_ops(void);

/**
 * Perform the necessary initializations.
 */
void finch_conversation_init(void);

/**
 * Perform the necessary uninitializations.
 */
void finch_conversation_uninit(void);

/**
 * Set a conversation as active in a contactized conversation
 *
 * @param conv The conversation to make active.
 */
void finch_conversation_set_active(PurpleConversation *conv);

/**
 * Sets the information widget for the conversation window.
 *
 * @param conv   The conversation.
 * @param widget The widget containing the information. If @c NULL,
 *               the current information widget is removed.
 */
void finch_conversation_set_info_widget(PurpleConversation *conv, GntWidget *widget);

/*@}*/

#endif
