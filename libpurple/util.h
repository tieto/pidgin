/**
 * @file util.h Utility Functions
 * @ingroup core
 */

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 * @todo Rename the functions so that they live somewhere in the purple
 *       namespace.
 */
#ifndef _PURPLE_UTIL_H_
#define _PURPLE_UTIL_H_

#include <stdio.h>

/**
 * PurpleMenuAction:
 *
 * A generic structure that contains information about an "action."  One
 * place this is is used is by protocols to tell the core the list of available
 * right-click actions for a buddy list row.
 */
typedef struct _PurpleMenuAction PurpleMenuAction;

/**
 * PurpleKeyValuePair:
 *
 * A key-value pair.
 *
 * This is used by, among other things, purple_gtk_combo* functions to pass in a
 * list of key-value pairs so it can display a user-friendly value.
 */
typedef struct _PurpleKeyValuePair PurpleKeyValuePair;

#include "account.h"
#include "signals.h"
#include "xmlnode.h"
#include "notify.h"
#include "plugin.h"


typedef char *(*PurpleInfoFieldFormatCallback)(const char *field, size_t len);

struct _PurpleKeyValuePair
{
	gchar *key;
	void *value;

};

G_BEGIN_DECLS

/**
 * purple_menu_action_new:
 * @label:    The text label to display for this action.
 * @callback: The function to be called when the action is used on
 *                 the selected item.
 * @data:     Additional data to be passed to the callback.
 * @children: A GList of PurpleMenuActions to be added as a submenu
 *                 of the action.
 *
 * Creates a new PurpleMenuAction.
 *
 * Returns: The PurpleMenuAction.
 */
PurpleMenuAction *purple_menu_action_new(const char *label, PurpleCallback callback,
                                     gpointer data, GList *children);

/**
 * purple_menu_action_free:
 * @act: The PurpleMenuAction to free.
 *
 * Frees a PurpleMenuAction
 */
void purple_menu_action_free(PurpleMenuAction *act);

/**
 * purple_menu_action_get_label:
 * @act:	The PurpleMenuAction.
 *
 * Returns the label of the PurpleMenuAction.
 *
 * Returns: The label string.
 */
char * purple_menu_action_get_label(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_callback:
 * @act:	The PurpleMenuAction.
 *
 * Returns the callback of the PurpleMenuAction.
 *
 * Returns: The callback function.
 */
PurpleCallback purple_menu_action_get_callback(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_data:
 * @act:	The PurpleMenuAction.
 *
 * Returns the data stored in the PurpleMenuAction.
 *
 * Returns: The data.
 */
gpointer purple_menu_action_get_data(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_children:
 * @act:	The PurpleMenuAction.
 *
 * Returns the children of the PurpleMenuAction.
 *
 * Returns: The  GList of children.
 */
GList* purple_menu_action_get_children(const PurpleMenuAction *act);

/**
 * purple_menu_action_set_label:
 * @act:   The menu action.
 * @label: The label for the menu action.
 *
 * Set the label to the PurpleMenuAction.
 */
void purple_menu_action_set_label(PurpleMenuAction *act, char *label);

/**
 * purple_menu_action_set_callback:
 * @act:        The menu action.
 * @callback:   The callback.
 *
 * Set the callback that will be used by the PurpleMenuAction.
 */
void purple_menu_action_set_callback(PurpleMenuAction *act, PurpleCallback callback);

/**
 * purple_menu_action_set_data:
 * @act:   The menu action.
 * @data:  The data used by this PurpleMenuAction
 *
 * Set the label to the PurpleMenuAction.
 */
void purple_menu_action_set_data(PurpleMenuAction *act, gpointer data);

/**
 * purple_menu_action_set_children:
 * @act:       The menu action.
 * @children:  The PurpleMenuAtion children
 *
 * Set the children of the PurpleMenuAction.
 */
void purple_menu_action_set_children(PurpleMenuAction *act, GList *children);

/**
 * purple_menu_action_set_stock_icon:
 * @act:    The menu action.
 * @strock: The stock icon identifier.
 *
 * Sets the icon for the PurpleMenuAction.
 */
void purple_menu_action_set_stock_icon(PurpleMenuAction *act,
	const gchar *stock);

/**
 * purple_menu_action_get_stock_icon:
 * @act: The menu action.
 *
 * Gets the stock icon of the PurpleMenuAction.
 *
 * Returns: The stock icon identifier.
 */
const gchar *
purple_menu_action_get_stock_icon(PurpleMenuAction *act);

/**
 * purple_util_set_current_song:
 * @title:     The title of the song, %NULL to unset the value.
 * @artist:    The artist of the song, can be %NULL.
 * @album:     The album of the song, can be %NULL.
 *
 * Set the appropriate presence values for the currently playing song.
 */
void purple_util_set_current_song(const char *title, const char *artist,
		const char *album);

/**
 * purple_util_format_song_info:
 * @title:     The title of the song, %NULL to unset the value.
 * @artist:    The artist of the song, can be %NULL.
 * @album:     The album of the song, can be %NULL.
 * @unused:    Currently unused, must be %NULL.
 *
 * Format song information.
 *
 * Returns:   The formatted string. The caller must g_free the returned string.
 */
char * purple_util_format_song_info(const char *title, const char *artist,
		const char *album, gpointer unused);

/**************************************************************************/
/** @name Utility Subsystem                                               */
/**************************************************************************/
/*@{*/

/**
 * purple_util_init:
 *
 * Initializes the utility subsystem.
 */
void purple_util_init(void);

/**
 * purple_util_uninit:
 *
 * Uninitializes the util subsystem.
 */
void purple_util_uninit(void);

/*@}*/

/**************************************************************************/
/** @name Base16 Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * purple_base16_encode:
 * @data: The data to convert.
 * @len:  The length of the data.
 *
 * Converts a chunk of binary data to its base-16 equivalent.
 *
 * Returns: The base-16 string in the ASCII encoding.  Must be
 *         g_free'd when no longer needed.
 *
 * @see purple_base16_decode()
 */
gchar *purple_base16_encode(const guchar *data, gsize len);

/**
 * purple_base16_decode:
 * @str:     The base-16 string to convert to raw data.
 * @ret_len: The length of the returned data.  You can
 *                pass in NULL if you're sure that you know
 *                the length of the decoded data, or if you
 *                know you'll be able to use strlen to
 *                determine the length, etc.
 *
 * Converts an ASCII string of base-16 encoded data to
 * the binary equivalent.
 *
 * Returns: The raw data.  Must be g_free'd when no longer needed.
 *
 * @see purple_base16_encode()
 */
guchar *purple_base16_decode(const char *str, gsize *ret_len);

/**
 * purple_base16_encode_chunked:
 * @data: The data to convert.
 * @len:  The length of the data.
 *
 * Converts a chunk of binary data to a chunked base-16 representation
 * (handy for key fingerprints)
 *
 * Example output: 01:23:45:67:89:AB:CD:EF
 *
 * Returns: The base-16 string in the ASCII chunked encoding.  Must be
 *         g_free'd when no longer needed.
 */
gchar *purple_base16_encode_chunked(const guchar *data, gsize len);


/*@}*/

/**************************************************************************/
/** @name Base64 Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * purple_base64_encode:
 * @data: The data to convert.
 * @len:  The length of the data.
 *
 * Converts a chunk of binary data to its base-64 equivalent.
 *
 * Returns: The base-64 string in the ASCII encoding.  Must be
 *         g_free'd when no longer needed.
 *
 * @see purple_base64_decode()
 */
gchar *purple_base64_encode(const guchar *data, gsize len);

/**
 * purple_base64_decode:
 * @str:     The base-64 string to convert to raw data.
 * @ret_len: The length of the returned data.  You can
 *                pass in NULL if you're sure that you know
 *                the length of the decoded data, or if you
 *                know you'll be able to use strlen to
 *                determine the length, etc.
 *
 * Converts an ASCII string of base-64 encoded data to
 * the binary equivalent.
 *
 * Returns: The raw data.  Must be g_free'd when no longer needed.
 *
 * @see purple_base64_encode()
 */
guchar *purple_base64_decode(const char *str, gsize *ret_len);

/*@}*/

/**************************************************************************/
/** @name Quoted Printable Functions                                      */
/**************************************************************************/
/*@{*/

/**
 * purple_quotedp_decode:
 * @str:     The quoted printable ASCII string to convert to raw data.
 * @ret_len: The length of the returned data.
 *
 * Converts a quoted printable string back to its readable equivalent.
 * What is a quoted printable string, you ask?  It's an encoding used
 * to transmit binary data as ASCII.  It's intended purpose is to send
 * emails containing non-ASCII characters.  Wikipedia has a pretty good
 * explanation.  Also see RFC 2045.
 *
 * Returns: The readable string.  Must be g_free'd when no longer needed.
 */
guchar *purple_quotedp_decode(const char *str, gsize *ret_len);

/*@}*/

/**************************************************************************/
/** @name MIME Functions                                                  */
/**************************************************************************/
/*@{*/

/**
 * purple_mime_decode_field:
 * @str: The ASCII string, possibly containing any number of
 *            encoded-word sections.
 *
 * Converts a MIME header field string back to its readable equivalent
 * according to RFC 2047.  Basically, a header is plain ASCII and can
 * contain any number of sections called "encoded-words."  The format
 * of an encoded word is =?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?=
 * =? designates the beginning of the encoded-word
 * ?= designates the end of the encoded-word
 *
 * An encoded word is segmented into three pieces by the use of a
 * question mark.  The first piece is the character set, the second
 * piece is the encoding, and the third piece is the encoded text.
 *
 * Returns: The string, with any encoded-word sections decoded and
 *         converted to UTF-8.  Must be g_free'd when no longer
 *         needed.
 */
char *purple_mime_decode_field(const char *str);

/*@}*/


/**************************************************************************/
/** @name Date/Time Functions                                             */
/**************************************************************************/
/*@{*/

/**
 * purple_utf8_strftime:
 * @format: The format string, in UTF-8
 * @tm:     The time to format, or %NULL to use the current local time
 *
 * Formats a time into the specified format.
 *
 * This is essentially strftime(), but it has a static buffer
 * and handles the UTF-8 conversion for the caller.
 *
 * This function also provides the GNU %z formatter if the underlying C
 * library doesn't.  However, the format string parser is very naive, which
 * means that conversions specifiers to %z cannot be guaranteed.  The GNU
 * strftime(3) man page describes %z as: 'The time-zone as hour offset from
 * GMT.  Required to emit RFC822-conformant dates
 * (using "%a, %d %b %Y %H:%M:%S %z"). (GNU)'
 *
 * On Windows, this function also converts the results for %Z from a timezone
 * name (as returned by the system strftime() %Z format string) to a timezone
 * abbreviation (as is the case on Unix).  As with %z, conversion specifiers
 * should not be used.
 *
 * Note: @format is required to be in UTF-8.  This differs from strftime(),
 *       where the format is provided in the locale charset.
 *
 * Returns: The formatted time, in UTF-8.
 */
const char *purple_utf8_strftime(const char *format, const struct tm *tm);

/**
 * purple_get_tzoff_str:
 * @tm:   The time to get the timezone for
 * @iso:  TRUE to format the offset according to ISO-8601, FALSE to
 *             not substitute 'Z' for 0 offset, and to not separate
 *             hours and minutes with a colon.
 *
 * Gets a string representation of the local timezone offset
 */
const char *purple_get_tzoff_str(const struct tm *tm, gboolean iso);

/**
 * purple_date_format_short:
 * @tm: The time to format, or %NULL to use the current local time
 *
 * Formats a time into the user's preferred short date format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * Returns: The date, formatted as per the user's settings.  In the USA this
 *         is something like "02/18/13"
 */
const char *purple_date_format_short(const struct tm *tm);

/**
 * purple_date_format_long:
 * @tm: The time to format, or %NULL to use the current local time
 *
 * Formats a time into the user's preferred short date plus time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * Returns: The timestamp, formatted as per the user's settings.  In the USA
 *         this is something like "02/18/13 15:26:44"
 */
const char *purple_date_format_long(const struct tm *tm);

/**
 * purple_date_format_full:
 * @tm: The time to format, or %NULL to use the current local time
 *
 * Formats a time into the user's preferred full date and time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * Returns: The date and time, formatted as per the user's settings.  In the
 *         USA this is something like "Mon Feb 18 15:26:44 2013"
 */
const char *purple_date_format_full(const struct tm *tm);

/**
 * purple_time_format:
 * @tm: The time to format, or %NULL to use the current local time
 *
 * Formats a time into the user's preferred time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * Returns: The time, formatted as per the user's settings.  In the USA this
 *         is something like "15:26:44"
 */
const char *purple_time_format(const struct tm *tm);

/**
 * purple_time_build:
 * @year:  The year.
 * @month: The month.
 * @day:   The day.
 * @hour:  The hour.
 * @min:   The minute.
 * @sec:   The second.
 *
 * Builds a time_t from the supplied information.
 *
 * Returns: A time_t.
 */
time_t purple_time_build(int year, int month, int day, int hour,
					   int min, int sec);

/**
 * PURPLE_NO_TZ_OFF:
 *
 * Used by purple_str_to_time to indicate no timezone offset was
 * specified in the timestamp string.
 */
#define PURPLE_NO_TZ_OFF -500000

/**
 * purple_str_to_time:
 * @timestamp: The timestamp
 * @utc:       Assume UTC if no timezone specified
 * @tm:        If not %NULL, the caller can get a copy of the
 *                  struct tm used to calculate the time_t return value.
 * @tz_off:    If not %NULL, the caller can get a copy of the
 *                  timezone offset (from UTC) used to calculate the time_t
 *                  return value. Note: Zero is a valid offset. As such,
 *                  the value of the macro PURPLE_NO_TZ_OFF indicates no
 *                  offset was specified (which means that the local
 *                  timezone was used in the calculation).
 * @rest:      If not %NULL, the caller can get a pointer to the
 *                  part of @timestamp left over after parsing is
 *                  completed, if it's not the end of @timestamp.
 *
 * Parses a timestamp in jabber, ISO8601, or MM/DD/YYYY format and returns
 * a time_t.
 *
 * Returns: A time_t.
 */
time_t purple_str_to_time(const char *timestamp, gboolean utc,
                        struct tm *tm, long *tz_off, const char **rest);

/**
 * purple_uts35_to_str:
 * @format: The formatting string, according to UTS #35
 *               See http://unicode.org/reports/tr35/
 *               (NOTE: not all formats are supported)
 * @len:    The length of the formatting string
 * @tm:     The time to format, or %NULL to use the current local time
 *
 * Formats a datetime according to a UTS-35 Date Format Pattern.
 *
 * Returns: The time, formatted as per the user's settings.
 */
char *purple_uts35_to_str(const char *format, size_t len, struct tm *tm);

/*@}*/


/**************************************************************************/
/** @name Markup Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * purple_markup_escape_text:
 *
 * Escapes special characters in a plain-text string so they display
 * correctly as HTML.  For example, & is replaced with &amp; and < is
 * replaced with &lt;
 *
 * This is exactly the same as g_markup_escape_text(), except that it
 * does not change ' to &apos; because &apos; is not a valid HTML 4 entity,
 * and is displayed literally in IE7.
 */
gchar *purple_markup_escape_text(const gchar *text, gssize length);

/**
 * purple_markup_find_tag:
 * @needle:	  The name of the tag
 * @haystack:	  The null-delimited string to search in
 * @start:		  A pointer to the start of the tag if found
 * @end:		  A pointer to the end of the tag if found
 * @attributes:  The attributes, if the tag was found.  This should
 *                    be freed with g_datalist_clear().
 *
 * Finds an HTML tag matching the given name.
 *
 * This locates an HTML tag's start and end, and stores its attributes
 * in a GData hash table. The names of the attributes are lower-cased
 * in the hash table, and the name of the tag is case insensitive.
 *
 * Returns: TRUE if the tag was found
 */
gboolean purple_markup_find_tag(const char *needle, const char *haystack,
							  const char **start, const char **end,
							  GData **attributes);

/**
 * purple_markup_extract_info_field:
 * @str:            The string to parse.
 * @len:            The size of str.
 * @user_info:      The destination PurpleNotifyUserInfo to which the new
 *                       field info should be added.
 * @start_token:    The beginning token.
 * @skip:           The number of characters to skip after the
 *                       start token.
 * @end_token:      The ending token.
 * @check_value:    The value that the last character must meet.
 * @no_value_token: The token indicating no value is given.
 * @display_name:   The short descriptive name to display for this token.
 * @is_link:        TRUE if this should be a link, or FALSE otherwise.
 * @link_prefix:    The prefix for the link.
 * @format_cb:      A callback to format the value before adding it.
 *
 * Extracts a field of data from HTML.
 *
 * This is a scary function. See protocols/msn/msn.c and
 * protocols/yahoo/yahoo_profile.c for example usage.
 *
 * Returns: TRUE if successful, or FALSE otherwise.
 */
gboolean purple_markup_extract_info_field(const char *str, int len, PurpleNotifyUserInfo *user_info,
                                        const char *start_token, int skip,
                                        const char *end_token, char check_value,
                                        const char *no_value_token,
                                        const char *display_name, gboolean is_link,
                                        const char *link_prefix,
					PurpleInfoFieldFormatCallback format_cb);

/**
 * purple_markup_html_to_xhtml:
 * @html:       The HTML markup.
 * @dest_xhtml: The destination XHTML output.
 * @dest_plain: The destination plain-text output.
 *
 * Converts HTML markup to XHTML.
 */
void purple_markup_html_to_xhtml(const char *html, char **dest_xhtml,
							   char **dest_plain);

/**
 * purple_markup_strip_html:
 * @str: The string to strip HTML from.
 *
 * Strips HTML tags from a string.
 *
 * Returns: The new string without HTML.  You must g_free this string
 *         when finished with it.
 */
char *purple_markup_strip_html(const char *str);

/**
 * purple_markup_linkify:
 * @str: The string to linkify.
 *
 * Adds the necessary HTML code to turn URIs into HTML links in a string.
 *
 * Returns: The new string with all URIs surrounded in standard
 *         HTML <a href="whatever"></a> tags.  You must g_free this
 *         string when finished with it.
 */
char *purple_markup_linkify(const char *str);

/**
 * purple_unescape_text:
 * @text: The string in which to unescape any HTML entities
 *
 * Unescapes HTML entities to their literal characters in the text.
 * For example "&amp;" is replaced by '&' and so on.  Also converts
 * numerical entities (e.g. "&#38;" is also '&').
 *
 * This function currently supports the following named entities:
 *     "&amp;", "&lt;", "&gt;", "&copy;", "&quot;", "&reg;", "&apos;"
 *
 * purple_unescape_html() is similar, but also converts "<br>" into "\n".
 *
 * Returns: The text with HTML entities literalized.  You must g_free
 *         this string when finished with it.
 *
 * @see purple_unescape_html()
 */
char *purple_unescape_text(const char *text);

/**
 * purple_unescape_html:
 * @html: The string in which to unescape any HTML entities
 *
 * Unescapes HTML entities to their literal characters and converts
 * "<br>" to "\n".  See purple_unescape_text() for more details.
 *
 * Returns: The text with HTML entities literalized.  You must g_free
 *         this string when finished with it.
 *
 * @see purple_unescape_text()
 */
char *purple_unescape_html(const char *html);

/**
 * purple_markup_slice:
 * @str: The input NUL terminated, HTML, UTF-8 (or ASCII) string.
 * @x: The character offset into an unformatted version of str to
 *          begin at.
 * @y: The character offset (into an unformatted vesion of str) of
 *          one past the last character to include in the slice.
 *
 * Returns a newly allocated substring of the HTML UTF-8 string "str".
 * The markup is preserved such that the substring will have the same
 * formatting as original string, even though some tags may have been
 * opened before "x", or may close after "y". All open tags are closed
 * at the end of the returned string, in the proper order.
 *
 * Note that x and y are in character offsets, not byte offsets, and
 * are offsets into an unformatted version of str. Because of this,
 * this function may be sensitive to changes in GtkIMHtml and may break
 * when used with other UI's. libpurple users are encouraged to report and
 * work out any problems encountered.
 *
 * Returns: The HTML slice of string, with all formatting retained.
 */
char *purple_markup_slice(const char *str, guint x, guint y);

/**
 * purple_markup_get_tag_name:
 * @tag: The string starting a HTML tag.
 *
 * Returns a newly allocated string containing the name of the tag
 * located at "tag". Tag is expected to point to a '<', and contain
 * a '>' sometime after that. If there is no '>' and the string is
 * not NUL terminated, this function can be expected to segfault.
 *
 * Returns: A string containing the name of the tag.
 */
char *purple_markup_get_tag_name(const char *tag);

/**
 * purple_markup_unescape_entity:
 * @text:   A string containing an HTML entity.
 * @length: If not %NULL, the string length of the entity is stored in this location.
 *
 * Returns a constant string of the character representation of the HTML
 * entity pointed to by @text. For example, purple_markup_unescape_entity("&amp;")
 * will return "&". The @text variable is expected to point to an '&',
 * the first character of the entity. If given an unrecognized entity, the function
 * returns %NULL.
 *
 * Note that this function, unlike purple_unescape_html(), does not search
 * the string for the entity, does not replace the entity, and does not
 * return a newly allocated string.
 *
 * Returns: A constant string containing the character representation of the given entity.
 */
const char * purple_markup_unescape_entity(const char *text, int *length);

/**
 * purple_markup_get_css_property:
 * @style: A string containing the inline CSS text.
 * @opt:   The requested CSS property.
 *
 * Returns a newly allocated string containing the value of the CSS property specified
 * in opt. The @style argument is expected to point to a HTML inline CSS.
 * The function will seek for the CSS property and return its value.
 *
 * For example, purple_markup_get_css_property("direction:rtl;color:#dc4d1b;",
 * "color") would return "#dc4d1b".
 *
 * On error or if the requested property was not found, the function returns
 * %NULL.
 *
 * Returns: The value of the requested CSS property.
 */
char * purple_markup_get_css_property(const gchar *style, const gchar *opt);

/**
 * purple_markup_is_rtl:
 * @html:  The HTML text.
 *
 * Check if the given HTML contains RTL text.
 *
 * Returns:  TRUE if the text contains RTL text, FALSE otherwise.
 */
gboolean purple_markup_is_rtl(const char *html);

/*@}*/


/**************************************************************************/
/** @name Path/Filename Functions                                         */
/**************************************************************************/
/*@{*/

/**
 * purple_home_dir:
 *
 * Returns the user's home directory.
 *
 * Returns: The user's home directory.
 *
 * @see purple_user_dir()
 */
const gchar *purple_home_dir(void);

/**
 * purple_user_dir:
 *
 * Returns the purple settings directory in the user's home directory.
 * This is usually ~/.purple
 *
 * Returns: The purple settings directory.
 *
 * @see purple_home_dir()
 */
const char *purple_user_dir(void);

/**
 * purple_util_set_user_dir:
 * @dir: The custom settings directory
 *
 * Define a custom purple settings directory, overriding the default (user's home directory/.purple)
 */
void purple_util_set_user_dir(const char *dir);

/**
 * purple_build_dir:
 * @path: The path you wish to create.  Note that it must start
 *        from the root or this function will fail.
 * @mode: Unix-style permissions for this directory.
 *
 * Builds a complete path from the root, making any directories along
 * the path which do not already exist.
 *
 * Returns: 0 for success, nonzero on any error.
 */
int purple_build_dir(const char *path, int mode);

/**
 * purple_util_write_data_to_file:
 * @filename: The basename of the file to write in the purple_user_dir.
 * @data:     A null-terminated string of data to write.
 * @size:     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * Write a string of data to a file of the given name in the Purple
 * user directory ($HOME/.purple by default).  The data is typically
 * a serialized version of one of Purple's config files, such as
 * prefs.xml, accounts.xml, etc.  And the string is typically
 * obtained using purple_xmlnode_to_formatted_str.  However, this function
 * should work fine for saving binary files as well.
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 */
gboolean purple_util_write_data_to_file(const char *filename, const char *data,
									  gssize size);

/**
 * purple_util_write_data_to_file_absolute:
 * @filename_full: Filename to write to
 * @data:          A null-terminated string of data to write.
 * @size:          The size of the data to save.  If data is
 *                      null-terminated you can pass in -1.
 *
 * Write data to a file using the absolute path.
 *
 * This exists for Glib backwards compatibility reasons.
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 *
 * @todo Remove this function (use g_file_set_contents instead) when 3.0.0
 *       rolls around.
 * @see purple_util_write_data_to_file()
 *
 */
gboolean
purple_util_write_data_to_file_absolute(const char *filename_full, const char *data, gssize size);

/**
 * purple_util_read_xml_from_file:
 * @filename:    The basename of the file to open in the purple_user_dir.
 * @description: A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * Read the contents of a given file and parse the results into an
 * PurpleXmlNode tree structure.  This is intended to be used to read
 * Purple's configuration xml files (prefs.xml, pounces.xml, etc.)
 *
 * Returns: An PurpleXmlNode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 */
PurpleXmlNode *purple_util_read_xml_from_file(const char *filename,
									  const char *description);

/**
 * purple_mkstemp:
 * @path:   The returned path to the temp file.
 * @binary: Text or binary, for platforms where it matters.
 *
 * Creates a temporary file and returns a file pointer to it.
 *
 * This is like mkstemp(), but returns a file pointer and uses a
 * pre-set template. It uses the semantics of tempnam() for the
 * directory to use and allocates the space for the file path.
 *
 * The caller is responsible for closing the file and removing it when
 * done, as well as freeing the space pointed to by @path with
 * g_free().
 *
 * Returns: A file pointer to the temporary file, or %NULL on failure.
 */
FILE *purple_mkstemp(char **path, gboolean binary);

/**
 * purple_util_get_image_extension:
 * @data: A pointer to the image data
 * @len:  The length of the image data
 *
 * Returns an extension corresponding to the image data's file type.
 *
 * Returns: The appropriate extension, or "icon" if unknown.
 */
const char *
purple_util_get_image_extension(gconstpointer data, size_t len);

/**
 * purple_util_get_image_filename:
 *
 * Returns: A hex encoded version of the SHA-1 hash of the data passed
 *         in with the correct file extention appended.  The file
 *         extension is determined by calling
 *         purple_util_get_image_extension().  This return value must
 *         be g_freed by the caller.
 */
char *purple_util_get_image_filename(gconstpointer image_data, size_t image_len);

/*@}*/


/**************************************************************************/
/** @name Environment Detection Functions                                 */
/**************************************************************************/
/*@{*/

/**
 * purple_program_is_valid:
 * @program: The file name of the application.
 *
 * Checks if the given program name is valid and executable.
 *
 * Returns: TRUE if the program is runable.
 */
gboolean purple_program_is_valid(const char *program);

/**
 * purple_running_gnome:
 *
 * Check if running GNOME.
 *
 * Returns: TRUE if running GNOME, FALSE otherwise.
 */
gboolean purple_running_gnome(void);

/**
 * purple_running_kde:
 *
 * Check if running KDE.
 *
 * Returns: TRUE if running KDE, FALSE otherwise.
 */
gboolean purple_running_kde(void);

/**
 * purple_running_osx:
 *
 * Check if running OS X.
 *
 * Returns: TRUE if running OS X, FALSE otherwise.
 */
gboolean purple_running_osx(void);

/**
 * purple_fd_get_ip:
 * @fd: The socket file descriptor.
 *
 * Returns the IP address from a socket file descriptor.
 *
 * Returns: The IP address, or %NULL on error.
 */
char *purple_fd_get_ip(int fd);

/**
 * purple_socket_get_family:
 * @fd: The socket file descriptor.
 *
 * Returns the address family of a socket.
 *
 * Returns: The address family of the socket (AF_INET, AF_INET6, etc) or -1
 *         on error.
 */
int purple_socket_get_family(int fd);

/**
 * purple_socket_speaks_ipv4:
 * @fd: The socket file descriptor
 *
 * Returns TRUE if a socket is capable of speaking IPv4.
 *
 * This is the case for IPv4 sockets and, on some systems, IPv6 sockets
 * (due to the IPv4-mapped address functionality).
 *
 * Returns: TRUE if a socket can speak IPv4.
 */
gboolean purple_socket_speaks_ipv4(int fd);

/*@}*/


/**************************************************************************/
/** @name String Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * purple_strequal:
 * @left:	A string
 * @right: A string to compare with left
 *
 * Tests two strings for equality.
 *
 * Unlike strcmp(), this function will not crash if one or both of the
 * strings are %NULL.
 *
 * Returns: %TRUE if the strings are the same, else %FALSE.
 */
gboolean purple_strequal(const gchar *left, const gchar *right);

/**
 * purple_normalize:
 * @account:  The account the string belongs to, or NULL if you do
 *                 not know the account.  If you use NULL, the string
 *                 will still be normalized, but if the protocol uses a
 *                 custom normalization function then the string may
 *                 not be normalized correctly.
 * @str:      The string to normalize.
 *
 * Normalizes a string, so that it is suitable for comparison.
 *
 * The returned string will point to a static buffer, so if the
 * string is intended to be kept long-term, you <i>must</i>
 * g_strdup() it. Also, calling normalize() twice in the same line
 * will lead to problems.
 *
 * Returns: A pointer to the normalized version stored in a static buffer.
 */
const char *purple_normalize(const PurpleAccount *account, const char *str);

/**
 * purple_normalize_nocase:
 * @account:  The account the string belongs to.
 * @str:      The string to normalize.
 *
 * Normalizes a string, so that it is suitable for comparison.
 *
 * This is one possible implementation for the protocol callback
 * function "normalize."  It returns a lowercase and UTF-8
 * normalized version of the string.
 *
 * Returns: A pointer to the normalized version stored in a static buffer.
 */
const char *purple_normalize_nocase(const PurpleAccount *account, const char *str);

/**
 * purple_validate:
 * @prpl:     The protocol plugin the string belongs to.
 * @str:      The string to validate.
 *
 * Checks, if a string is valid.
 *
 * Returns: TRUE, if string is valid, otherwise FALSE.
 */
gboolean purple_validate(const PurplePlugin *prpl, const char *str);

/**
 * purple_str_has_prefix:
 * @s:  The string to check.
 * @p:  The prefix in question.
 *
 * Compares two strings to see if the first contains the second as
 * a proper prefix.
 *
 * Returns:   TRUE if p is a prefix of s, otherwise FALSE.
 */
gboolean purple_str_has_prefix(const char *s, const char *p);

/**
 * purple_str_has_suffix:
 * @s:  The string to check.
 * @x:  The suffix in question.
 *
 * Compares two strings to see if the second is a proper suffix
 * of the first.
 *
 * Returns:   TRUE if x is a a suffix of s, otherwise FALSE.
 */
gboolean purple_str_has_suffix(const char *s, const char *x);

/**
 * purple_strdup_withhtml:
 * @src: The source string.
 *
 * Duplicates a string and replaces all newline characters from the
 * source string with HTML linebreaks.
 *
 * Returns: The new string.  Must be g_free'd by the caller.
 */
gchar *purple_strdup_withhtml(const gchar *src);

/**
 * purple_str_add_cr:
 * @str: The source string.
 *
 * Ensures that all linefeeds have a matching carriage return.
 *
 * Returns: The string with carriage returns.
 */
char *purple_str_add_cr(const char *str);

/**
 * purple_str_strip_char:
 * @str:     The string to strip characters from.
 * @thechar: The character to strip from the given string.
 *
 * Strips all instances of the given character from the
 * given string.  The string is modified in place.  This
 * is useful for stripping new line characters, for example.
 *
 * Example usage:
 * purple_str_strip_char(my_dumb_string, '\n');
 */
void purple_str_strip_char(char *str, char thechar);

/**
 * purple_util_chrreplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The character you want replaced.
 * @replacement: The character you want inserted in place
 *        of the delimiting character.
 *
 * Given a string, this replaces all instances of one character
 * with another.  This happens inline (the original string IS
 * modified).
 */
void purple_util_chrreplace(char *string, char delimiter,
						  char replacement);

/**
 * purple_strreplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The substring you want replaced.
 * @replacement: The substring you want inserted in place
 *        of the delimiting substring.
 *
 * Given a string, this replaces one substring with another
 * and returns a newly allocated string.
 *
 * Returns: A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *purple_strreplace(const char *string, const char *delimiter,
					   const char *replacement);


/**
 * purple_utf8_ncr_encode:
 * @in: The string which might contain utf-8 substrings
 *
 * Given a string, this replaces any utf-8 substrings in that string with
 * the corresponding numerical character reference, and returns a newly
 * allocated string.
 *
 * Returns: A new string, with utf-8 replaced with numerical character
 *         references, free this with g_free()
*/
char *purple_utf8_ncr_encode(const char *in);


/**
 * purple_utf8_ncr_decode:
 * @in: The string which might contain numerical character references.
 *
 * Given a string, this replaces any numerical character references
 * in that string with the corresponding actual utf-8 substrings,
 * and returns a newly allocated string.
 *
 * Returns: A new string, with numerical character references
 *         replaced with actual utf-8, free this with g_free().
 */
char *purple_utf8_ncr_decode(const char *in);


/**
 * purple_strcasereplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The substring you want replaced.
 * @replacement: The substring you want inserted in place
 *        of the delimiting substring.
 *
 * Given a string, this replaces one substring with another
 * ignoring case and returns a newly allocated string.
 *
 * Returns: A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *purple_strcasereplace(const char *string, const char *delimiter,
						   const char *replacement);

/**
 * purple_strcasestr:
 * @haystack: The string to search in.
 * @needle:   The substring to find.
 *
 * This is like strstr, except that it ignores ASCII case in
 * searching for the substring.
 *
 * Returns: the location of the substring if found, or NULL if not
 */
const char *purple_strcasestr(const char *haystack, const char *needle);

/**
 * purple_str_size_to_units:
 * @size: The size
 *
 * Returns a string representing a filesize in the appropriate
 * units (MB, KB, GB, etc.)
 *
 * Returns: The string in units form. This must be freed.
 */
char *purple_str_size_to_units(goffset size);

/**
 * purple_str_seconds_to_string:
 * @sec: The seconds.
 *
 * Converts seconds into a human-readable form.
 *
 * Returns: A human-readable form, containing days, hours, minutes, and
 *         seconds.
 */
char *purple_str_seconds_to_string(guint sec);

/**
 * purple_str_binary_to_ascii:
 * @binary: A string of random data, possibly with embedded NULs
 *               and such.
 * @len: The length in bytes of the input string. Must not be 0.
 *
 * Converts a binary string into a NUL terminated ascii string,
 * replacing nonascii characters and characters below SPACE (including
 * NUL) into \\xyy, where yy are two hex digits. Also backslashes are
 * changed into two backslashes (\\\\). The returned, newly allocated
 * string can be outputted to the console, and must be g_free()d.
 *
 * Returns: A newly allocated ASCIIZ string.
 */
char *purple_str_binary_to_ascii(const unsigned char *binary, guint len);

/**
 * purple_utf16_size:
 * @str: String to check.
 *
 * Calculates UTF-16 string size (in bytes).
 *
 * Returns:    Number of bytes (including NUL character) that string occupies.
 */
size_t purple_utf16_size(const gunichar2 *str);

/**
 * purple_str_wipe:
 * @str: A NUL-terminated string to free, or a NULL-pointer.
 *
 * Fills a NUL-terminated string with zeros and frees it.
 *
 * It should be used to free sensitive data, like passwords.
 */
void purple_str_wipe(gchar *str);

/**
 * purple_utf16_wipe:
 * @str: A NUL-terminated string to free, or a NULL-pointer.
 *
 * Fills a NUL-terminated UTF-16 string with zeros and frees it.
 *
 * It should be used to free sensitive data, like passwords.
 */
void purple_utf16_wipe(gunichar2 *str);

/*@}*/


/**************************************************************************/
/** @name URI/URL Functions                                               */
/**************************************************************************/
/*@{*/

void purple_got_protocol_handler_uri(const char *uri);

/**
 * purple_url_decode:
 * @str: The string to translate.
 *
 * Decodes a URL into a plain string.
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * Returns: The resulting string.
 */
const char *purple_url_decode(const char *str);

/**
 * purple_url_encode:
 * @str: The string to translate.
 *
 * Encodes a URL into an escaped string.
 *
 * This will change non-alphanumeric characters to hex codes.
 *
 * Returns: The resulting string.
 */
const char *purple_url_encode(const char *str);

/**
 * purple_email_is_valid:
 * @address: The email address to validate.
 *
 * Checks if the given email address is syntactically valid.
 *
 * Returns: True if the email address is syntactically correct.
 */
gboolean purple_email_is_valid(const char *address);

/**
 * purple_ip_address_is_valid:
 * @ip: The IP address to validate.
 *
 * Checks if the given IP address is a syntactically valid IPv4 or
 * IPv6 address.
 * If you specifically want to check for an IPv4 address use
 * purple_ipv4_address_is_valid(), or for an IPv6 address use
 * purple_ipv6_address_is_valid().
 *
 * Returns: True if the IP address is syntactically correct.
 */
gboolean purple_ip_address_is_valid(const char *ip);

/**
 * purple_ipv4_address_is_valid:
 * @ip: The IP address to validate.
 *
 * Checks if the given IP address is a syntactically valid IPv4 address.
 *
 * Returns: True if the IP address is syntactically correct.
 */
gboolean purple_ipv4_address_is_valid(const char *ip);

/**
 * purple_ipv6_address_is_valid:
 * @ip: The IP address to validate.
 *
 * Checks if the given IP address is a syntactically valid IPv6 address.
 *
 * Returns: True if the IP address is syntactically correct.
 */
gboolean purple_ipv6_address_is_valid(const char *ip);

/**
 * purple_uri_list_extract_uris:
 * @uri_list: An uri-list in the standard format.
 *
 * This function extracts a list of URIs from the a "text/uri-list"
 * string.  It was "borrowed" from gnome_uri_list_extract_uris
 *
 * Returns: A GList containing strings allocated with g_malloc
 *         that have been splitted from uri-list.
 */
GList *purple_uri_list_extract_uris(const gchar *uri_list);

/**
 * purple_uri_list_extract_filenames:
 * @uri_list: A uri-list in the standard format.
 *
 * This function extracts a list of filenames from a
 * "text/uri-list" string.  It was "borrowed" from
 * gnome_uri_list_extract_filenames
 *
 * Returns: A GList containing strings allocated with g_malloc that
 *         contain the filenames in the uri-list. Note that unlike
 *         purple_uri_list_extract_uris() function, this will discard
 *         any non-file uri from the result value.
 */
GList *purple_uri_list_extract_filenames(const gchar *uri_list);

/*@}*/

/**************************************************************************
 * UTF8 String Functions
 **************************************************************************/
/*@{*/

/**
 * purple_utf8_try_convert:
 * @str: The source string.
 *
 * Attempts to convert a string to UTF-8 from an unknown encoding.
 *
 * This function checks the locale and tries sane defaults.
 *
 * Returns: The UTF-8 string, or %NULL if it could not be converted.
 */
gchar *purple_utf8_try_convert(const char *str);

/**
 * purple_utf8_salvage:
 * @str: The source string.
 *
 * Salvages the valid UTF-8 characters from a string, replacing any
 * invalid characters with a filler character (currently hardcoded to
 * '?').
 *
 * Returns: A valid UTF-8 string.
 */
gchar *purple_utf8_salvage(const char *str);

/**
 * purple_utf8_strip_unprintables:
 * @str: A valid UTF-8 string.
 *
 * Removes unprintable characters from a UTF-8 string. These characters
 * (in particular low-ASCII characters) are invalid in XML 1.0 and thus
 * are not allowed in XMPP and are rejected by libxml2 by default.
 *
 * The returned string must be freed by the caller.
 *
 * Returns: A newly allocated UTF-8 string without the unprintable characters.
 */
gchar *purple_utf8_strip_unprintables(const gchar *str);

/**
 * purple_gai_strerror:
 * @errnum: The error code.
 *
 * Return the UTF-8 version of gai_strerror().  It calls gai_strerror()
 * then converts the result to UTF-8.  This function is analogous to
 * g_strerror().
 *
 * Returns: The UTF-8 error message.
 */
const gchar *purple_gai_strerror(gint errnum);

/**
 * purple_utf8_strcasecmp:
 * @a: The first string.
 * @b: The second string.
 *
 * Compares two UTF-8 strings case-insensitively.  This comparison is
 * more expensive than a simple g_utf8_collate() comparison because
 * it calls g_utf8_casefold() on each string, which allocates new
 * strings.
 *
 * Returns: -1 if @a is less than @b.
 *           0 if @a is equal to @b.
 *           1 if @a is greater than @b.
 */
int purple_utf8_strcasecmp(const char *a, const char *b);

/**
 * purple_utf8_has_word:
 * @haystack: The string to search in.
 * @needle:   The substring to find.
 *
 * Case insensitive search for a word in a string. The needle string
 * must be contained in the haystack string and not be immediately
 * preceded or immediately followed by another alpha-numeric character.
 *
 * Returns: TRUE if haystack has the word, otherwise FALSE
 */
gboolean purple_utf8_has_word(const char *haystack, const char *needle);

/**
 * purple_print_utf8_to_console:
 * @filestream: The file stream (e.g. STDOUT or STDERR)
 * @message:    The message to print.
 *
 * Prints a UTF-8 message to the given file stream. The function
 * tries to convert the UTF-8 message to user's locale. If this
 * is not possible, the original UTF-8 text will be printed.
 */
void purple_print_utf8_to_console(FILE *filestream, char *message);

/**
 * purple_message_meify:
 * @message: The message to check
 * @len:     The message length, or -1
 *
 * Checks for messages starting (post-HTML) with "/me ", including the space.
 *
 * Returns: TRUE if it starts with "/me ", and it has been removed, otherwise
 *         FALSE
 */
gboolean purple_message_meify(char *message, gssize len);

/**
 * purple_text_strip_mnemonic:
 * @in:  The string to strip
 *
 * Removes the underscore characters from a string used identify the mnemonic
 * character.
 *
 * Returns: The stripped string
 */
char *purple_text_strip_mnemonic(const char *in);

/*@}*/

/**
 * purple_add_eight:
 * @x: The number to add 8 to.
 *
 * Adds 8 to something.
 *
 * Blame SimGuy.
 *
 * Returns: x + 8
 */
#define purple_add_eight(x) ((x)+8)

/**
 * purple_unescape_filename:
 * @str: The string to translate.
 *
 * Does the reverse of purple_escape_filename
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * Returns: The resulting string.
 */
const char *purple_unescape_filename(const char *str);

/**
 * purple_escape_filename:
 * @str: The string to translate.
 *
 * Escapes filesystem-unfriendly characters from a filename
 *
 * Returns: The resulting string.
 */
const char *purple_escape_filename(const char *str);

/**
 * purple_escape_js:
 * @str: The string to escape.
 *
 * Escapes javascript-unfriendly substrings from a string.
 *
 * Returns: The javascript-safe string (must be g_free'd after use).
 */
gchar * purple_escape_js(const gchar *str);

/**
 * purple_restore_default_signal_handlers:
 *
 * Restore default signal handlers for signals which might reasonably have
 * handlers. This should be called by a fork()'d child process, since child processes
 * inherit the handlers of the parent.
 */
void purple_restore_default_signal_handlers(void);

/**
 * purple_get_host_name:
 *
 * Gets the host name of the machine. If it not possible to determine the
 * host name, "localhost" is returned
 *
 * Returns: The hostname
 */
const gchar *purple_get_host_name(void);

/**
 * purple_uuid_random:
 *
 * Returns a type 4 (random) UUID
 *
 * Returns: A UUID, caller is responsible for freeing it
 */
gchar *purple_uuid_random(void);

/**
 * purple_callback_set_zero:
 * @data: A pointer to variable, which should be set to NULL.
 *
 * Sets given pointer to NULL.
 *
 * Function designed to be used as a GDestroyNotify callback.
 */
void purple_callback_set_zero(gpointer data);

/**
 * purple_value_new:
 * @type:  The type of data to be held by the GValue
 *
 * Creates a new GValue of the specified type.
 *
 * Returns:  The created GValue
 */
GValue *purple_value_new(GType type);

/**
 * purple_value_dup:
 * @value:  The GValue to duplicate
 *
 * Duplicates a GValue.
 *
 * Returns:  The duplicated GValue
 */
GValue *purple_value_dup(GValue *value);

/**
 * purple_value_free:
 * @value:  The GValue to free.
 *
 * Frees a GValue.
 */
void purple_value_free(GValue *value);

/**
 * purple_http_digest_calculate_session_key:
 * @algorithm:    The hash algorithm to use
 * @username:     The username provided by the user
 * @realm:        The authentication realm provided by the server
 * @password:     The password provided by the user
 * @nonce:        The nonce provided by the server
 * @client_nonce: The nonce provided by the client
 *
 * Calculates a session key for HTTP Digest authentation
 *
 * See RFC 2617 for more information.
 *
 * Returns: The session key, or %NULL if an error occurred.
 */
gchar *purple_http_digest_calculate_session_key(
		const gchar *algorithm, const gchar *username,
		const gchar *realm, const gchar *password,
		const gchar *nonce, const gchar *client_nonce);

/**
 * purple_http_digest_calculate_response:
 * @algorithm:         The hash algorithm to use
 * @method:            The HTTP method in use
 * @digest_uri:        The URI from the initial request
 * @qop:               The "quality of protection"
 * @entity:            The entity body
 * @nonce:             The nonce provided by the server
 * @nonce_count:       The nonce count
 * @client_nonce:      The nonce provided by the client
 * @session_key:       The session key from purple_cipher_http_digest_calculate_session_key()
 *
 * Calculate a response for HTTP Digest authentication
 *
 * See RFC 2617 for more information.
 *
 * Returns: The hashed response, or %NULL if an error occurred.
 */
gchar *purple_http_digest_calculate_response(
		const gchar *algorithm, const gchar *method,
		const gchar *digest_uri, const gchar *qop,
		const gchar *entity, const gchar *nonce,
		const gchar *nonce_count, const gchar *client_nonce,
		const gchar *session_key);

G_END_DECLS

#endif /* _PURPLE_UTIL_H_ */
