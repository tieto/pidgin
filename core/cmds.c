/**
 * @file cmds.c Commands API
 * @ingroup core
 *
 * Copyright (C) 2003-2004 Timothy Ringenbach <omarvo@hotmail.com
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
 *
 */

#include <string.h>

#include "account.h"
#include "util.h"
#include "cmds.h"

static GList *cmds = NULL;
static guint next_id = 1;

typedef struct _GaimCmd {
	GaimCmdId id;
	gchar *cmd;
	gchar *args;
	GaimCmdPriority priority;
	GaimCmdFlag flags;
	gchar *prpl_id;
	GaimCmdFunc func;
	gchar *help;
	void *data;
} GaimCmd;


static gint cmds_compare_func(const GaimCmd *a, const GaimCmd *b)
{
	if (a->priority > b->priority)
		return -1;
	else if (a->priority < b->priority)
		return 1;
	else return 0;
}

GaimCmdId gaim_cmd_register(const gchar *cmd, const gchar *args,
                            GaimCmdPriority p, GaimCmdFlag f,
                            const gchar *prpl_id, GaimCmdFunc func,
                            const gchar *helpstr, void *data)
{
	GaimCmdId id;
	GaimCmd *c;

	g_return_val_if_fail(cmd != NULL && *cmd != '\0', 0);
	g_return_val_if_fail(args != NULL, 0);
	g_return_val_if_fail(func != NULL, 0);

	id = next_id++;

	c = g_new0(GaimCmd, 1);
	c->id = id;
	c->cmd = g_strdup(cmd);
	c->args = g_strdup(args);
	c->priority = p;
	c->flags = f;
	c->prpl_id = g_strdup(prpl_id);
	c->func = func;
	c->help = g_strdup(helpstr);
	c->data = data;

	cmds = g_list_insert_sorted(cmds, c, (GCompareFunc)cmds_compare_func);

	return id;
}

static void gaim_cmd_free(GaimCmd *c)
{
	g_free(c->cmd);
	g_free(c->args);
	g_free(c->prpl_id);
	g_free(c->help);
	g_free(c);
}

void gaim_cmd_unregister(GaimCmdId id)
{
	GaimCmd *c;
	GList *l;

	for (l = cmds; l; l = l->next) {
		c = l->data;

		if (c->id == id) {
			cmds = g_list_remove(cmds, c);
			gaim_cmd_free(c);
			return;
		}
	}
}

/**
 * This sets args to a NULL-terminated array of strings.  It should
 * be freed using g_strfreev().
 */
static gboolean gaim_cmd_parse_args(GaimCmd *cmd, const gchar *s, const gchar *m, gchar ***args)
{
	int i;
	const char *end, *cur;

	*args = g_new0(char *, strlen(cmd->args) + 1);

	cur = s;

	for (i = 0; cmd->args[i]; i++) {
		if (!*cur)
			return (cmd->flags & GAIM_CMD_FLAG_ALLOW_WRONG_ARGS);

		switch (cmd->args[i]) {
		case 'w':
			if (!(end = strchr(cur, ' '))) {
			  end = cur + strlen(cur);
			  (*args)[i] = g_strndup(cur, end - cur);
			  cur = end;
			} else {
			  (*args)[i] = g_strndup(cur, end - cur);
			  cur = end + 1;
			}
			break;
		case 'W':
		        if (!(end = strchr(cur, ' '))) {
			  end = cur + strlen(cur);
			  (*args)[i] = gaim_markup_slice(m, g_utf8_pointer_to_offset(s, cur), g_utf8_pointer_to_offset(s, end));
			  cur = end;
			} else {
			  (*args)[i] = gaim_markup_slice(m, g_utf8_pointer_to_offset(s, cur), g_utf8_pointer_to_offset(s, end));
			  cur = end +1;
			}
			break;
		case 's':
			(*args)[i] = g_strdup(cur);
			cur = cur + strlen(cur);
			break;
		case 'S':
			(*args)[i] = gaim_markup_slice(m, g_utf8_pointer_to_offset(s, cur), g_utf8_strlen(cur, -1) + 1);
			cur = cur + strlen(cur);
			break;
		}
	}

	if (*cur)
		return (cmd->flags & GAIM_CMD_FLAG_ALLOW_WRONG_ARGS);

	return TRUE;
}

static void gaim_cmd_strip_current_char(gunichar c, char *s, guint len)
{
	int bytes;

	bytes = g_unichar_to_utf8(c, NULL);
	memmove(s, s + bytes, len + 1 - bytes);
}

static void gaim_cmd_strip_cmd_from_markup(char *markup)
{
	guint len = strlen(markup);
	char *s = markup;

	while (*s) {
		gunichar c = g_utf8_get_char(s);

		if (c == '<') {
			s = strchr(s, '>');
			if (!s)
				return;
		} else if (g_unichar_isspace(c)) {
			gaim_cmd_strip_current_char(c, s, len - (s - markup));
			return;
		} else {
			gaim_cmd_strip_current_char(c, s, len - (s - markup));
			continue;
		}
		s = g_utf8_next_char(s);
	}
}

GaimCmdStatus gaim_cmd_do_command(GaimConversation *conv, const gchar *cmdline,
                                  const gchar *markup, gchar **error)
{
	GaimCmd *c;
	GList *l;
	gchar *err = NULL;
	gboolean is_im;
	gboolean found = FALSE, tried_cmd = FALSE, right_type = FALSE, right_prpl = FALSE;
	const gchar *prpl_id;
	gchar **args = NULL;
	gchar *cmd, *rest, *mrest;
	GaimCmdRet ret = GAIM_CMD_RET_CONTINUE;

	*error = NULL;
	prpl_id = gaim_account_get_protocol_id(gaim_conversation_get_account(conv));

	if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM)
		is_im = TRUE;
	else if (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT)
		is_im = FALSE;
	else
		return GAIM_CMD_STATUS_FAILED;

	rest = strchr(cmdline, ' ');
	if (rest) {
		cmd = g_strndup(cmdline, rest - cmdline);
		rest++;
	} else {
		cmd = g_strdup(cmdline);
		rest = "";
	}

	mrest = g_strdup(markup);
	gaim_cmd_strip_cmd_from_markup(mrest);

	for (l = cmds; l; l = l->next) {
		c = l->data;

		if (strcmp(c->cmd, cmd) != 0)
			continue;

		found = TRUE;

		if (is_im)
			if (!(c->flags & GAIM_CMD_FLAG_IM))
				continue;
		if (!is_im)
			if (!(c->flags & GAIM_CMD_FLAG_CHAT))
				continue;

		right_type = TRUE;

		if ((c->flags & GAIM_CMD_FLAG_PRPL_ONLY) && c->prpl_id &&
		    (strcmp(c->prpl_id, prpl_id) != 0))
			continue;

		right_prpl = TRUE;

		/* this checks the allow bad args flag for us */
		if (!gaim_cmd_parse_args(c, rest, mrest, &args)) {
			g_strfreev(args);
			args = NULL;
			continue;
		}

		tried_cmd = TRUE;
		ret = c->func(conv, cmd, args, &err, c->data);
		if (ret == GAIM_CMD_RET_CONTINUE) {
			g_free(err);
			err = NULL;
			g_strfreev(args);
			args = NULL;
			continue;
		} else {
			break;
		}

	}

	g_strfreev(args);
	g_free(cmd);
	g_free(mrest);

	if (!found)
		return GAIM_CMD_STATUS_NOT_FOUND;

	if (!right_type)
		return GAIM_CMD_STATUS_WRONG_TYPE;
	if (!right_prpl)
		return GAIM_CMD_STATUS_WRONG_PRPL;
	if (!tried_cmd)
		return GAIM_CMD_STATUS_WRONG_ARGS;

	if (ret == GAIM_CMD_RET_OK) {
		return GAIM_CMD_STATUS_OK;
	} else {
		*error = err;
		if (ret == GAIM_CMD_RET_CONTINUE)
			return GAIM_CMD_STATUS_NOT_FOUND;
		else
			return GAIM_CMD_STATUS_FAILED;
	}

}


GList *gaim_cmd_list(GaimConversation *conv)
{
	GList *ret = NULL;
	GaimCmd *c;
	GList *l;

	for (l = cmds; l; l = l->next) {
		c = l->data;

		if (conv && (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM))
			if (!(c->flags & GAIM_CMD_FLAG_IM))
				continue;
		if (conv && (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT))
			if (!(c->flags & GAIM_CMD_FLAG_CHAT))
				continue;

		if (conv && (c->flags & GAIM_CMD_FLAG_PRPL_ONLY) && c->prpl_id &&
		    (strcmp(c->prpl_id, gaim_account_get_protocol_id(gaim_conversation_get_account(conv))) != 0))
			continue;

		ret = g_list_append(ret, c->cmd);
	}

	ret = g_list_sort(ret, (GCompareFunc)strcmp);

	return ret;
}


GList *gaim_cmd_help(GaimConversation *conv, const gchar *cmd)
{
	GList *ret = NULL;
	GaimCmd *c;
	GList *l;

	for (l = cmds; l; l = l->next) {
		c = l->data;

		if (cmd && (strcmp(cmd, c->cmd) != 0))
			continue;

		if (conv && (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_IM))
			if (!(c->flags & GAIM_CMD_FLAG_IM))
				continue;
		if (conv && (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT))
			if (!(c->flags & GAIM_CMD_FLAG_CHAT))
				continue;

		if (conv && (c->flags & GAIM_CMD_FLAG_PRPL_ONLY) && c->prpl_id &&
		    (strcmp(c->prpl_id, gaim_account_get_protocol_id(gaim_conversation_get_account(conv))) != 0))
			continue;

		ret = g_list_append(ret, c->help);
	}

	ret = g_list_sort(ret, (GCompareFunc)strcmp);

	return ret;
}

