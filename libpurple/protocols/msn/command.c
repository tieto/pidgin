/**
 * @file command.c MSN command functions
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#include "msn.h"
#include "command.h"

static gboolean
is_num(const char *str)
{
	const char *c;
	for (c = str; *c; c++) {
		if (!(g_ascii_isdigit(*c)))
			return FALSE;
	}

	return TRUE;
}

/*
 * check the command is the command with payload content
 *  if it is	return TRUE
 *  else 		return FALSE
 */
static gboolean
msn_check_payload_cmd(const char *str)
{
	g_return_val_if_fail(str != NULL, FALSE);

	if((!strcmp(str,"ADL")) ||
		(!strcmp(str,"GCF")) ||
		(!strcmp(str,"SG")) ||
		(!strcmp(str,"MSG")) ||
		(!strcmp(str,"RML")) ||
		(!strcmp(str,"UBX")) ||
		(!strcmp(str,"UBN")) ||
		(!strcmp(str,"UUM")) ||
		(!strcmp(str,"UBM")) ||
		(!strcmp(str,"FQY")) ||
		(!strcmp(str,"UUN")) ||
		(!strcmp(str,"UUX")) ||
		(!strcmp(str,"IPG")) ||
		(is_num(str))){
			return TRUE;
		}

	return FALSE;
}

/*
 * set command Payload length
 */
static void
msn_set_payload_len(MsnCommand *cmd)
{
	char *param;
	int len = 0;

	if (msn_check_payload_cmd(cmd->command) && (cmd->param_count > 0)){
		param = cmd->params[cmd->param_count - 1];
		len = is_num(param) ? atoi(param) : 0;
	}

	cmd->payload_len = len;
}

MsnCommand *
msn_command_from_string(const char *string)
{
	MsnCommand *cmd;
	char *param_start;

	g_return_val_if_fail(string != NULL, NULL);

	cmd = g_new0(MsnCommand, 1);
	cmd->command = g_strdup(string);
	param_start = strchr(cmd->command, ' ');

	if (param_start)
	{
		*param_start++ = '\0';
		cmd->params = g_strsplit(param_start, " ", 0);
	}

	if (cmd->params != NULL)
	{
		char *param;
		int c;

		for (c = 0; cmd->params[c]; c++);
		cmd->param_count = c;

		param = cmd->params[0];

		cmd->trId = is_num(param) ? atoi(param) : 0;
	}
	else
	{
		cmd->trId = 0;
	}

	/* khc: Huh! */
	/*add payload Length checking*/
	msn_set_payload_len(cmd);
	purple_debug_info("MSNP14","get payload len:%d\n",cmd->payload_len);

	msn_command_ref(cmd);

	return cmd;
}

void
msn_command_destroy(MsnCommand *cmd)
{
	g_return_if_fail(cmd != NULL);

	if (cmd->ref_count > 0)
	{
		msn_command_unref(cmd);
		return;
	}

	if (cmd->payload != NULL)
		g_free(cmd->payload);

	g_free(cmd->command);
	g_strfreev(cmd->params);
	g_free(cmd);
}

MsnCommand *
msn_command_ref(MsnCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, NULL);

	cmd->ref_count++;
	return cmd;
}

MsnCommand *
msn_command_unref(MsnCommand *cmd)
{
	g_return_val_if_fail(cmd != NULL, NULL);
	g_return_val_if_fail(cmd->ref_count > 0, NULL);

	cmd->ref_count--;

	if (cmd->ref_count == 0)
	{
		msn_command_destroy(cmd);
		return NULL;
	}

	return cmd;
}
