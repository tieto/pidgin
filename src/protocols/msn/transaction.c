/**
 * @file transaction.c MSN transaction functions
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
#include "msn.h"
#include "transaction.h"

MsnTransaction *
msn_transaction_new(const char *command, const char *format, ...)
{
	MsnTransaction *trans;
	va_list arg;

	g_return_val_if_fail(command != NULL, NULL);

	trans = g_new0(MsnTransaction, 1);

	trans->command = g_strdup(command);

	if (format != NULL)
	{
		va_start(arg, format);
		trans->params = g_strdup_vprintf(format, arg);
		va_end(arg);
	}

	/* trans->queue = g_queue_new(); */
	
	return trans;
}

void
msn_transaction_destroy(MsnTransaction *trans)
{
	g_return_if_fail(trans != NULL);

	g_free(trans->command);
	g_free(trans->params);
	g_free(trans->payload);

#if 0
	if (trans->pendent_cmd != NULL)
		msn_message_unref(trans->pendent_msg);
#endif

#if 0
	MsnTransaction *elem;
	if (trans->queue != NULL)
	{	
		while ((elem = g_queue_pop_head(trans->queue)) != NULL)
			msn_transaction_destroy(elem);
		
		g_queue_free(trans->queue);
	}
#endif

	g_free(trans);
}

char *
msn_transaction_to_string(MsnTransaction *trans)
{
	char *str;

	g_return_val_if_fail(trans != NULL, FALSE);

	if (trans->params != NULL)
		str = g_strdup_printf("%s %u %s\r\n", trans->command, trans->trId, trans->params);
	else
		str = g_strdup_printf("%s %u\r\n", trans->command, trans->trId);

	return str;
}

void
msn_transaction_set_payload(MsnTransaction *trans,
							const char *payload, int payload_len)
{
	g_return_if_fail(trans   != NULL);
	g_return_if_fail(payload != NULL);

	trans->payload = g_strdup(payload);
	trans->payload_len = payload_len ? payload_len : strlen(trans->payload);
}
