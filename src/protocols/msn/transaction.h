/**
 * @file transaction.h MSN transaction functions
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _MSN_TRANSACTION_H
#define _MSN_TRANSACTION_H

typedef struct _MsnTransaction MsnTransaction;

#include "command.h"
#include "cmdproc.h"

/**
 * A transaction. A command that will initiate the transaction.
 */
struct _MsnTransaction
{
	unsigned int trId;

	char *command;
	char *params;

	GHashTable *callbacks;
	void *data;

	char *payload;
	size_t payload_len;

	GQueue *queue;
	MsnCommand *pendent_cmd;
};

MsnTransaction *msn_transaction_new(const char *command,
									const char *format, ...);

void msn_transaction_destroy(MsnTransaction *trans);

char *msn_transaction_to_string(MsnTransaction *trans);
void msn_transaction_set_payload(MsnTransaction *trans,
								 const char *payload, int payload_len);

#endif /* _MSN_TRANSACTION_H */
