/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_SMILEY_PARSER_H_
#define _PURPLE_SMILEY_PARSER_H_
/**
 * SECTION:smiley-parser
 * @include:smiley-parser.h
 * @section_id: libpurple-smiley-parser
 * @short_description: a efficient smiley processor
 * @title: Smiley parser
 *
 * This module is a fast and easy method for searching (and optionally replacing)
 * #PurpleSmiley's in a text. It may use all suitable smiley sets to smileyify
 * the message in one step. The priority if always the following: remote
 * smileys > local custom smileys > theme smileys.
 */

#include "purple.h"

/**
 * PurpleSmileyParseCb:
 * @out: the message buffer.
 * @smiley: found smiley.
 * @conv: the conversation of a message.
 * @ui_data: the data being passed to #purple_smiley_parse.
 *
 * A replace callback for the found @smiley. It should append a HTML tag
 * representing the @smiley to the @out string. It must not modify the
 * @out string in other way than appending to its end.
 */
typedef void (*PurpleSmileyParseCb)(GString *out, PurpleSmiley *smiley,
	PurpleConversation *conv, gpointer ui_data);

/**
 * purple_smiley_parse:
 * @conv: the conversation of a message.
 * @html_message: the html message, or escaped plain message.
 * @use_remote_smileys: %TRUE if remote smileys of @conv should be parsed.
 * @cb: the callback to replace smiley text with an image.
 * @ui_data: the user data to be passed to @cb and
 *           #purple_smiley_theme_get_smileys.
 *
 * Replaces all textual smiley representations with proper smiley images.
 *
 * The @use_remote_smileys parameter should be %TRUE for incoming messages,
 * %FALSE for outgoing.
 *
 * Returns: (transfer full): the smileyifed message. Should be #g_free'd when
 *          done using it. Returns %NULL if and only if @html_message was %NULL.
 */
gchar *
purple_smiley_parse(PurpleConversation *conv, const gchar *html_message,
	gboolean use_remote_smileys, PurpleSmileyParseCb cb, gpointer ui_data);

/**
 * purple_smiley_find:
 * @smileys: the list of smileys to find.
 * @message: the message.
 * @is_html: %TRUE if the message is HTML, %FALSE if it's plain, unescaped.
 *
 * Searches for all smileys from the @smileys list present in @message.
 * Each smiley is returned only once, regardless how many times it appeared in
 * text. However, distinct smileys may share common image file (thus, their
 * paths will be the same).
 *
 * Returns: (transfer container): the #GList of found smileys. Use #g_list_free
 *          when no longer need it.
 */
GList *
purple_smiley_find(PurpleSmileyList *smileys, const gchar *message,
	gboolean is_html);

/**
 * _purple_smiley_parser_init: (skip)
 *
 * Initializes the smileys parser subsystem.
 */
void
_purple_smiley_parser_init(void);

/**
 * _purple_smiley_parser_uninit: (skip)
 *
 * Uninitializes the smileys parser subsystem.
 */
void
_purple_smiley_parser_uninit(void);

#endif /* _PURPLE_SMILEY_PARSER_H_ */
