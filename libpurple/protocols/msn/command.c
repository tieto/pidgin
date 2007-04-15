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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "msn.h"
#include "command.h"

/*local Function prototype*/
int msn_get_payload_position(char *str);
int msn_set_payload_len(MsnCommand *cmd);

static gboolean
is_num(char *str)
{
	char *c;
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
msn_check_payload_cmd(char *str)
{
	if( (!strcmp(str,"ADL")) ||
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
		(!strcmp(str,"UUX"))){
			return TRUE;
		}

	return FALSE;
}

/*get the payload positon*/
int msn_get_payload_position(char *str)
{
	/*because MSG has "MSG hotmail hotmail [payload length]"*/
	if(!(strcmp(str,"MSG"))|| (!strcmp(str,"UBX")) ){
		return 2;
	}
	/*Yahoo User Message UBM 
	 * Format UBM email@yahoo.com 32 1 [payload length]*/
	if(!(strcmp(str,"UBM"))|| (!strcmp(str,"UUM")) ){
		return 3;
	}

	return 1;
}

/*
 * set command Payload length
 */
int
msn_set_payload_len(MsnCommand *cmd)
{
	char * param;

	if(msn_check_payload_cmd(cmd->command)){
		param = cmd->params[msn_get_payload_position(cmd->command)];
#if 0
		if(!(strcmp(cmd->command,"MSG"))){
			param = cmd->params[2];
		}else{
			param = cmd->params[1];
		}
#endif
		cmd->payload_len = is_num(param) ? atoi(param) : 0;
	}else{
		cmd->payload_len = 0;
	}
	return 0;
}

MsnCommand *
msn_command_from_string(const char *string)
{
	MsnCommand *cmd;
	char *tmp;
	char *param_start;

	g_return_val_if_fail(string != NULL, NULL);

	tmp = g_strdup(string);
	param_start = strchr(tmp, ' ');

	cmd = g_new0(MsnCommand, 1);
	cmd->command = tmp;

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
	}else{
		cmd->trId = 0;
	}

	/*add payload Length checking*/
	msn_set_payload_len(cmd);
	gaim_debug_info("MaYuan","get payload len:%d\n",cmd->payload_len);

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
