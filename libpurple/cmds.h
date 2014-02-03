/* Copyright (C) 2003 Timothy Ringenbach <omarvo@hotmail.com>
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
 *
 */
/**
 * SECTION:cmds
 * @section_id: libpurple-cmds
 * @title: cmds.h
 * @short_description: Commands API
 * @see_also: <link linkend="chapter-signals-cmd">Command signals</link>
 */

#ifndef _PURPLE_CMDS_H_
#define _PURPLE_CMDS_H_

#include "conversation.h"

/**************************************************************************/
/** @name Structures                                                      */
/**************************************************************************/
/*@{*/

/**
 * PurpleCmdStatus:
 *
 * The possible results of running a command with purple_cmd_do_command().
 */
typedef enum {
	PURPLE_CMD_STATUS_OK,
	PURPLE_CMD_STATUS_FAILED,
	PURPLE_CMD_STATUS_NOT_FOUND,
	PURPLE_CMD_STATUS_WRONG_ARGS,
	PURPLE_CMD_STATUS_WRONG_PRPL,
	PURPLE_CMD_STATUS_WRONG_TYPE
} PurpleCmdStatus;

/**
 * PurpleCmdRet:
 * @PURPLE_CMD_RET_OK:       Everything's okay; Don't look for another command
 *                           to call.
 * @PURPLE_CMD_RET_FAILED:   The command failed, but stop looking.
 * @PURPLE_CMD_RET_CONTINUE: Continue, looking for other commands with the same
 *                           name to call.
 *
 * Commands registered with the core return one of these values when run.
 * Normally, a command will want to return one of the first two; in some
 * unusual cases, you might want to have several functions called for a
 * particular command; in this case, they should return
 * #PURPLE_CMD_RET_CONTINUE to cause the core to fall through to other
 * commands with the same name.
 */
typedef enum {
	PURPLE_CMD_RET_OK,
	PURPLE_CMD_RET_FAILED,
	PURPLE_CMD_RET_CONTINUE
} PurpleCmdRet;

#define PURPLE_CMD_FUNC(func) ((PurpleCmdFunc)func)

/**
 * PurpleCmdFunc:
 *
 * A function implementing a command, as passed to purple_cmd_register().
 *
 * @todo document the arguments to these functions.
 */
typedef PurpleCmdRet (*PurpleCmdFunc)(PurpleConversation *, const gchar *cmd,
                                  gchar **args, gchar **error, void *data);
/**
 * PurpleCmdId:
 *
 * A unique integer representing a command registered with
 * purple_cmd_register(), which can subsequently be passed to
 * purple_cmd_unregister() to unregister that command.
 */
typedef guint PurpleCmdId;

typedef enum {
	PURPLE_CMD_P_VERY_LOW  = -1000,
	PURPLE_CMD_P_LOW       =     0,
	PURPLE_CMD_P_DEFAULT   =  1000,
	PURPLE_CMD_P_PRPL      =  2000,
	PURPLE_CMD_P_PLUGIN    =  3000,
	PURPLE_CMD_P_ALIAS     =  4000,
	PURPLE_CMD_P_HIGH      =  5000,
	PURPLE_CMD_P_VERY_HIGH =  6000
} PurpleCmdPriority;

/**
 * PurpleCmdFlag:
 * @PURPLE_CMD_FLAG_IM: Command is usable in IMs.
 * @PURPLE_CMD_FLAG_CHAT: Command is usable in multi-user chats.
 * @PURPLE_CMD_FLAG_PRPL_ONLY: Command is usable only for a particular
 *                             protocol.
 * @PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS: Incorrect arguments to this command
 *                                    should be accepted anyway.
 *
 * Flags used to set various properties of commands.  Every command should
 * have at least one of #PURPLE_CMD_FLAG_IM and #PURPLE_CMD_FLAG_CHAT set in
 * order to be even slighly useful.
 *
 * @see purple_cmd_register
 */
typedef enum {
	PURPLE_CMD_FLAG_IM               = 0x01,
	PURPLE_CMD_FLAG_CHAT             = 0x02,
	PURPLE_CMD_FLAG_PRPL_ONLY        = 0x04,
	PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS = 0x08
} PurpleCmdFlag;


/*@}*/

G_BEGIN_DECLS

/**************************************************************************/
/** @name Commands API                                                    */
/**************************************************************************/
/*@{*/

/**
 * purple_cmd_register:
 * @cmd: The command. This should be a UTF-8 (or ASCII) string, with no spaces
 *            or other white space.
 * @args: A string of characters describing to libpurple how to parse this
 *             command's arguments.  If what the user types doesn't match this
 *             pattern, libpurple will keep looking for another command, unless
 *             the flag #PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS is passed in @f.
 *             This string should contain no whitespace, and use a single
 *             character for each argument.  The recognized characters are:
 *             <ul>
 *               <li><literal>'w'</literal>: Matches a single word.</li>
 *               <li><literal>'W'</literal>: Matches a single word, with
 *                                           formatting.</li>
 *               <li><literal>'s'</literal>: Matches the rest of the arguments
 *                                           after this point, as a single
 *                                           string.</li>
 *               <li><literal>'S'</literal>: Same as <literal>'s'</literal> but
 *                                           with formatting.</li>
 *             </ul>
 *             If args is the empty string, then the command accepts no arguments.
 *             The args passed to the callback @func will be a %NULL
 *             terminated array of %NULL terminated strings, and will always
 *             match the number of arguments asked for, unless
 *             #PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS is passed.
 * @p: This is the priority. Higher priority commands will be run first,
 *          and usually the first command will stop any others from being
 *          called.
 * @f: Flags specifying various options about this command, combined with
 *          <literal>|</literal> (bitwise OR). You need to at least pass one of
 *          #PURPLE_CMD_FLAG_IM or #PURPLE_CMD_FLAG_CHAT (you may pass both) in
 *          order for the command to ever actually be called.
 * @protocol_id: If the #PURPLE_CMD_FLAG_PRPL_ONLY flag is set, this is the id
 *                of the protocol to which the command applies (such as
 *                <literal>"prpl-msn"</literal>). If the flag is not set, this
 *                parameter is ignored; pass %NULL (or a humourous string of
 *                your choice!).
 * @func: This is the function to call when someone enters this command.
 * @helpstr: a whitespace sensitive, UTF-8, HTML string describing how to
 *                use the command.  The preferred format of this string is the
 *                command's name, followed by a space and any arguments it
 *                accepts (if it takes any arguments, otherwise no space),
 *                followed by a colon, two spaces, and a description of the
 *                command in sentence form.  Do not include a slash before the
 *                command name.
 * @data: User defined data to pass to the #PurpleCmdFunc @f.
 *
 * Register a new command with the core.
 *
 * The command will only happen if commands are enabled,
 * which is a UI pref. UIs don't have to support commands at all.
 *
 * Returns: A #PurpleCmdId, which is only used for calling
 *         #purple_cmd_unregister, or 0 on failure.
 */
PurpleCmdId purple_cmd_register(const gchar *cmd, const gchar *args, PurpleCmdPriority p, PurpleCmdFlag f,
                             const gchar *protocol_id, PurpleCmdFunc func, const gchar *helpstr, void *data);

/**
 * purple_cmd_unregister:
 * @id: The #PurpleCmdId to unregister, as returned by #purple_cmd_register.
 *
 * Unregister a command with the core.
 *
 * All registered commands must be unregistered, if they're registered by a plugin
 * or something else that might go away. Normally this is called when the plugin
 * unloads itself.
 */
void purple_cmd_unregister(PurpleCmdId id);

/**
 * purple_cmd_do_command:
 * @conv: The conversation the command was typed in.
 * @cmdline: The command the user typed (including all arguments) as a single string.
 *            The caller doesn't have to do any parsing, except removing the command
 *            prefix, which the core has no knowledge of. cmd should not contain any
 *            formatting, and should be in plain text (no html entities).
 * @markup: This is the same as cmd, but is the formatted version. It should be in
 *               HTML, with < > and &, at least, escaped to html entities, and should
 *               include both the default formatting and any extra manual formatting.
 * @errormsg: If the command failed errormsg is filled in with the appropriate error
 *                 message. It must be freed by the caller with g_free().
 *
 * Do a command.
 *
 * Normally the UI calls this to perform a command. This might also be useful
 * if aliases are ever implemented.
 *
 * Returns: A #PurpleCmdStatus indicating if the command succeeded or failed.
 */
PurpleCmdStatus purple_cmd_do_command(PurpleConversation *conv, const gchar *cmdline,
                                  const gchar *markup, gchar **errormsg);

/**
 * purple_cmd_list:
 * @conv: The conversation, or %NULL.
 *
 * List registered commands.
 *
 * Returns a #GList (which must be freed by the caller) of all commands
 * that are valid in the context of @conv, or all commands, if @conv is
 * %NULL.  Don't keep this list around past the main loop, or anything else that
 * might unregister a command, as the <type>const char *</type>'s used get freed
 * then.
 *
 * Returns: A #GList of <type>const char *</type>, which must be freed with
 *          g_list_free().
 */
GList *purple_cmd_list(PurpleConversation *conv);

/**
 * purple_cmd_help:
 * @conv: The conversation, or %NULL for no context.
 * @cmd: The command. No wildcards accepted, but returns help for all
 *            commands if %NULL.
 *
 * Get the help string for a command.
 *
 * Returns the help strings for a given command in the form of a GList,
 * one node for each matching command.
 *
 * Returns: A #GList of <type>const char *</type>s, which is the help string
 *         for that command.
 */
GList *purple_cmd_help(PurpleConversation *conv, const gchar *cmd);

/**
 * purple_cmds_get_handle:
 *
 * Get the handle for the commands API
 * Returns: The handle
 */
gpointer purple_cmds_get_handle(void);

/**
 * purple_cmds_init:
 *
 * Initialize the commands subsystem.
 */
void purple_cmds_init(void);

/**
 * purple_cmds_uninit:
 *
 * Uninitialize the commands subsystem.
 */
void purple_cmds_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CMDS_H_ */
