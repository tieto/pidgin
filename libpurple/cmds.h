/**
 * @file cmds.h Commands API
 * @ingroup core
 *
 * Copyright (C) 2003 Timothy Ringenbach <omarvo@hotmail.com>
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
#ifndef _PURPLE_CMDS_H_
#define _PURPLE_CMDS_H_

#include "conversation.h"

/**************************************************************************/
/** @name Structures                                                      */
/**************************************************************************/
/*@{*/

typedef enum _PurpleCmdPriority PurpleCmdPriority;
typedef enum _PurpleCmdFlag     PurpleCmdFlag;
typedef enum _PurpleCmdStatus   PurpleCmdStatus;
typedef enum _PurpleCmdRet      PurpleCmdRet;

enum _PurpleCmdStatus {
	PURPLE_CMD_STATUS_OK,
	PURPLE_CMD_STATUS_FAILED,
	PURPLE_CMD_STATUS_NOT_FOUND,
	PURPLE_CMD_STATUS_WRONG_ARGS,
	PURPLE_CMD_STATUS_WRONG_PRPL,
	PURPLE_CMD_STATUS_WRONG_TYPE,
};

enum _PurpleCmdRet {
	PURPLE_CMD_RET_OK,       /**< Everything's okay. Don't look for another command to call. */
	PURPLE_CMD_RET_FAILED,   /**< The command failed, but stop looking.*/
	PURPLE_CMD_RET_CONTINUE, /**< Continue, looking for other commands with the same name to call. */
};

#define PURPLE_CMD_FUNC(func) ((PurpleCmdFunc)func)

typedef PurpleCmdRet (*PurpleCmdFunc)(PurpleConversation *, const gchar *cmd,
                                  gchar **args, gchar **error, void *data);
typedef guint PurpleCmdId;

enum _PurpleCmdPriority {
	PURPLE_CMD_P_VERY_LOW  = -1000,
	PURPLE_CMD_P_LOW       =     0,
	PURPLE_CMD_P_DEFAULT   =  1000,
	PURPLE_CMD_P_PRPL      =  2000,
	PURPLE_CMD_P_PLUGIN    =  3000,
	PURPLE_CMD_P_ALIAS     =  4000,
	PURPLE_CMD_P_HIGH      =  5000,
	PURPLE_CMD_P_VERY_HIGH =  6000,
};

enum _PurpleCmdFlag {
	PURPLE_CMD_FLAG_IM               = 0x01,
	PURPLE_CMD_FLAG_CHAT             = 0x02,
	PURPLE_CMD_FLAG_PRPL_ONLY        = 0x04,
	PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS = 0x08,
};


/*@}*/

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Commands API                                                    */
/**************************************************************************/
/*@{*/

/**
 * Register a new command with the core.
 *
 * The command will only happen if commands are enabled,
 * which is a UI pref. UIs don't have to support commands at all.
 *
 * @param cmd The command. This should be a UTF8 (or ASCII) string, with no spaces
 *            or other white space.
 * @param args This tells Purple how to parse the arguments to the command for you.
 *             If what the user types doesn't match, Purple will keep looking for another
 *             command, unless the flag @c PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS is passed in f.
 *             This string contains no whitespace, and uses a single character for each argument.
 *             The recognized characters are:
 *               'w' Matches a single word.
 *               'W' Matches a single word, with formatting.
 *               's' Matches the rest of the arguments after this point, as a single string.
 *               'S' Same as 's' but with formatting.
 *             If args is the empty string, then the command accepts no arguments.
 *             The args passed to callback func will be a @c NULL terminated array of null
 *             terminated strings, and will always match the number of arguments asked for,
 *             unless PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS is passed.
 * @param p This is the priority. Higher priority commands will be run first, and usually the
 *          first command will stop any others from being called.
 * @param f These are the flags. You need to at least pass one of PURPLE_CMD_FLAG_IM or
 *          PURPLE_CMD_FLAG_CHAT (can may pass both) in order for the command to ever actually
 *          be called.
 * @param prpl_id This is the prpl's id string. This is only meaningful if the proper flag is set.
 * @param func This is the function to call when someone enters this command.
 * @param helpstr This is a whitespace sensitive, UTF-8, HTML string describing how to use the command.
 *                The preferred format of this string shall be the commands name, followed by a space
 *                and any arguments it accepts (if it takes any arguments, otherwise no space), followed
 *                by a colon, two spaces, and a description of the command in sentence form. No slash
 *                before the command name.
 * @param data User defined data to pass to the PurpleCmdFunc
 * @return A PurpleCmdId. This is only used for calling purple_cmd_unregister.
 *         Returns 0 on failure.
 */
PurpleCmdId purple_cmd_register(const gchar *cmd, const gchar *args, PurpleCmdPriority p, PurpleCmdFlag f,
                             const gchar *prpl_id, PurpleCmdFunc func, const gchar *helpstr, void *data);

/**
 * Unregister a command with the core.
 *
 * All registered commands must be unregistered, if they're registered by a plugin
 * or something else that might go away. Normally this is called when the plugin
 * unloads itself.
 *
 * @param id The PurpleCmdId to unregister.
 */
void purple_cmd_unregister(PurpleCmdId id);

/**
 * Do a command.
 *
 * Normally the UI calls this to perform a command. This might also be useful
 * if aliases are ever implemented.
 *
 * @param conv The conversation the command was typed in.
 * @param cmdline The command the user typed (including all arguments) as a single string.
 *            The caller doesn't have to do any parsing, except removing the command
 *            prefix, which the core has no knowledge of. cmd should not contain any
 *            formatting, and should be in plain text (no html entities).
 * @param markup This is the same as cmd, but is the formatted version. It should be in
 *               HTML, with < > and &, at least, escaped to html entities, and should
 *               include both the default formatting and any extra manual formatting.
 * @param errormsg If the command failed errormsg is filled in with the appropriate error
 *                 message. It must be freed by the caller with g_free().
 * @return A PurpleCmdStatus indicated if the command succeeded or failed.
 */
PurpleCmdStatus purple_cmd_do_command(PurpleConversation *conv, const gchar *cmdline,
                                  const gchar *markup, gchar **errormsg);

/**
 * List registered commands.
 *
 * Returns a GList (which must be freed by the caller) of all commands
 * that are valid in the context of conv, or all commands, if conv is
 * @c NULL. Don't keep this list around past the main loop, or anything else
 * that might unregister a command, as the char*'s used get freed then.
 *
 * @param conv The conversation, or @c NULL.
 * @return A GList of const char*, which must be freed with g_list_free().
 */
GList *purple_cmd_list(PurpleConversation *conv);

/**
 * Get the help string for a command.
 *
 * Returns the help strings for a given command in the form of a GList,
 * one node for each matching command.
 *
 * @param conv The conversation, or @c NULL for no context.
 * @param cmd The command. No wildcards accepted, but returns help for all
 *            commands if @c NULL.
 * @return A GList of const char*s, which is the help string
 *         for that command.
 */
GList *purple_cmd_help(PurpleConversation *conv, const gchar *cmd);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_CMDS_H_ */
