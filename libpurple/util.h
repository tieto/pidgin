/**
 * @file util.h Utility Functions
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 *
 * @todo Rename the functions so that they live somewhere in the gaim
 *       namespace.
 */
#ifndef _GAIM_UTIL_H_
#define _GAIM_UTIL_H_

#include <stdio.h>

#include "account.h"
#include "xmlnode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GaimUtilFetchUrlData GaimUtilFetchUrlData;

typedef struct _GaimMenuAction
{
	char *label;
	GaimCallback callback;
	gpointer data;
	GList *children;
} GaimMenuAction;

typedef char *(*GaimInfoFieldFormatCallback)(const char *field, size_t len);

/**
 * A key-value pair.
 *
 * This is used by, among other things, gaim_gtk_combo* functions to pass in a
 * list of key-value pairs so it can display a user-friendly value.
 */
typedef struct _GaimKeyValuePair
{
	gchar *key;
	void *value;

} GaimKeyValuePair;

/**
 * Creates a new GaimMenuAction.
 *
 * @param label    The text label to display for this action.
 * @param callback The function to be called when the action is used on
 *                 the selected item.
 * @param data     Additional data to be passed to the callback.
 * @param children A GList of GaimMenuActions to be added as a submenu
 *                 of the action.
 * @return The GaimMenuAction.
 */
GaimMenuAction *gaim_menu_action_new(const char *label, GaimCallback callback,
                                     gpointer data, GList *children);

/**
 * Frees a GaimMenuAction
 *
 * @param act The GaimMenuAction to free.
 */
void gaim_menu_action_free(GaimMenuAction *act);

/**************************************************************************/
/** @name Base16 Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * Converts a chunk of binary data to its base-16 equivalent.
 *
 * @param data The data to convert.
 * @param len  The length of the data.
 *
 * @return The base-16 string in the ASCII encoding.  Must be
 *         g_free'd when no longer needed.
 *
 * @see gaim_base16_decode()
 */
gchar *gaim_base16_encode(const guchar *data, gsize len);

/**
 * Converts an ASCII string of base-16 encoded data to
 * the binary equivalent.
 *
 * @param str     The base-16 string to convert to raw data.
 * @param ret_len The length of the returned data.  You can
 *                pass in NULL if you're sure that you know
 *                the length of the decoded data, or if you
 *                know you'll be able to use strlen to
 *                determine the length, etc.
 *
 * @return The raw data.  Must be g_free'd when no longer needed.
 *
 * @see gaim_base16_encode()
 */
guchar *gaim_base16_decode(const char *str, gsize *ret_len);

/*@}*/

/**************************************************************************/
/** @name Base64 Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * Converts a chunk of binary data to its base-64 equivalent.
 *
 * @param data The data to convert.
 * @param len  The length of the data.
 *
 * @return The base-64 string in the ASCII encoding.  Must be
 *         g_free'd when no longer needed.
 *
 * @see gaim_base64_decode()
 */
gchar *gaim_base64_encode(const guchar *data, gsize len);

/**
 * Converts an ASCII string of base-64 encoded data to
 * the binary equivalent.
 *
 * @param str     The base-64 string to convert to raw data.
 * @param ret_len The length of the returned data.  You can
 *                pass in NULL if you're sure that you know
 *                the length of the decoded data, or if you
 *                know you'll be able to use strlen to
 *                determine the length, etc.
 *
 * @return The raw data.  Must be g_free'd when no longer needed.
 *
 * @see gaim_base64_encode()
 */
guchar *gaim_base64_decode(const char *str, gsize *ret_len);

/*@}*/

/**************************************************************************/
/** @name Quoted Printable Functions                                      */
/**************************************************************************/
/*@{*/

/**
 * Converts a quoted printable string back to its readable equivalent.
 * What is a quoted printable string, you ask?  It's an encoding used
 * to transmit binary data as ASCII.  It's intended purpose is to send
 * e-mails containing non-ASCII characters.  Wikipedia has a pretty good
 * explanation.  Also see RFC 2045.
 *
 * @param str     The quoted printable ASCII string to convert to raw data.
 * @param ret_len The length of the returned data.
 *
 * @return The readable string.  Must be g_free'd when no longer needed.
 */
guchar *gaim_quotedp_decode(const char *str, gsize *ret_len);

/*@}*/

/**************************************************************************/
/** @name MIME Functions                                                  */
/**************************************************************************/
/*@{*/

/**
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
 * @param str The ASCII string, possibly containing any number of
 *            encoded-word sections.
 *
 * @return The string, with any encoded-word sections decoded and
 *         converted to UTF-8.  Must be g_free'd when no longer
 *         needed.
 */
char *gaim_mime_decode_field(const char *str);

/*@}*/


/**************************************************************************/
/** @name Date/Time Functions                                             */
/**************************************************************************/
/*@{*/

/**
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
 * @param format The format string, in UTF-8
 * @param tm     The time to format, or @c NULL to use the current local time
 *
 * @return The formatted time, in UTF-8.
 *
 * @note @a format is required to be in UTF-8.  This differs from strftime(),
 *       where the format is provided in the locale charset.
 */
const char *gaim_utf8_strftime(const char *format, const struct tm *tm);

/**
 * Formats a time into the user's preferred short date format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * @param tm The time to format, or @c NULL to use the current local time
 *
 * @return The date, formatted as per the user's settings.
 */
const char *gaim_date_format_short(const struct tm *tm);

/**
 * Formats a time into the user's preferred short date plus time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * @param tm The time to format, or @c NULL to use the current local time
 *
 * @return The timestamp, formatted as per the user's settings.
 */
const char *gaim_date_format_long(const struct tm *tm);

/**
 * Formats a time into the user's preferred full date and time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * @param tm The time to format, or @c NULL to use the current local time
 *
 * @return The date and time, formatted as per the user's settings.
 */
const char *gaim_date_format_full(const struct tm *tm);

/**
 * Formats a time into the user's preferred time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * @param tm The time to format, or @c NULL to use the current local time
 *
 * @return The time, formatted as per the user's settings.
 */
const char *gaim_time_format(const struct tm *tm);

/**
 * Builds a time_t from the supplied information.
 *
 * @param year  The year.
 * @param month The month.
 * @param day   The day.
 * @param hour  The hour.
 * @param min   The minute.
 * @param sec   The second.
 *
 * @return A time_t.
 */
time_t gaim_time_build(int year, int month, int day, int hour,
					   int min, int sec);

/** Used by gaim_str_to_time to indicate no timezone offset was
  * specified in the timestamp string. */
#define GAIM_NO_TZ_OFF -500000

/**
 * Parses a timestamp in jabber, ISO8601, or MM/DD/YYYY format and returns
 * a time_t.
 *
 * @param timestamp The timestamp
 * @param utc       Assume UTC if no timezone specified
 * @param tm        If not @c NULL, the caller can get a copy of the
 *                  struct tm used to calculate the time_t return value.
 * @param tz_off    If not @c NULL, the caller can get a copy of the
 *                  timezone offset (from UTC) used to calculate the time_t
 *                  return value. Note: Zero is a valid offset. As such,
 *                  the value of the macro @c GAIM_NO_TZ_OFF indicates no
 *                  offset was specified (which means that the local
 *                  timezone was used in the calculation).
 * @param rest      If not @c NULL, the caller can get a pointer to the
 *                  part of @a timestamp left over after parsing is
 *                  completed, if it's not the end of @a timestamp.
 *
 * @return A time_t.
 */
time_t gaim_str_to_time(const char *timestamp, gboolean utc,
                        struct tm *tm, long *tz_off, const char **rest);

/*@}*/


/**************************************************************************/
/** @name Markup Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * Finds an HTML tag matching the given name.
 *
 * This locates an HTML tag's start and end, and stores its attributes
 * in a GData hash table. The names of the attributes are lower-cased
 * in the hash table, and the name of the tag is case insensitive.
 *
 * @param needle	  The name of the tag
 * @param haystack	  The null-delimited string to search in
 * @param start		  A pointer to the start of the tag if found
 * @param end		  A pointer to the end of the tag if found
 * @param attributes  The attributes, if the tag was found.  This should
 *                    be freed with g_datalist_clear().
 * @return TRUE if the tag was found
 */
gboolean gaim_markup_find_tag(const char *needle, const char *haystack,
							  const char **start, const char **end,
							  GData **attributes);

/**
 * Extracts a field of data from HTML.
 *
 * This is a scary function. See protocols/msn/msn.c and
 * protocols/yahoo/yahoo_profile.c for example usage.
 *
 * @param str            The string to parse.
 * @param len            The size of str.
 * @param user_info      The destination GaimNotifyUserInfo to which the new
 *                       field info should be added.
 * @param start_token    The beginning token.
 * @param skip           The number of characters to skip after the
 *                       start token.
 * @param end_token      The ending token.
 * @param check_value    The value that the last character must meet.
 * @param no_value_token The token indicating no value is given.
 * @param display_name   The short descriptive name to display for this token.
 * @param is_link        TRUE if this should be a link, or FALSE otherwise.
 * @param link_prefix    The prefix for the link.
 * @param format_cb      A callback to format the value before adding it.
 *
 * @return TRUE if successful, or FALSE otherwise.
 */
gboolean gaim_markup_extract_info_field(const char *str, int len, GaimNotifyUserInfo *user_info,
                                        const char *start_token, int skip,
                                        const char *end_token, char check_value,
                                        const char *no_value_token,
                                        const char *display_name, gboolean is_link,
                                        const char *link_prefix,
					GaimInfoFieldFormatCallback format_cb);

/**
 * Converts HTML markup to XHTML.
 *
 * @param html       The HTML markup.
 * @param dest_xhtml The destination XHTML output.
 * @param dest_plain The destination plain-text output.
 */
void gaim_markup_html_to_xhtml(const char *html, char **dest_xhtml,
							   char **dest_plain);

/**
 * Strips HTML tags from a string.
 *
 * @param str The string to strip HTML from.
 *
 * @return The new string without HTML. This must be freed.
 */
char *gaim_markup_strip_html(const char *str);

/**
 * Adds the necessary HTML code to turn URIs into HTML links in a string.
 *
 * @param str The string to linkify.
 *
 * @return The linkified text.
 */
char *gaim_markup_linkify(const char *str);

/**
 * Unescapes HTML entities to their literal characters.
 * For example "&amp;" is replaced by '&' and so on.
 * Actually only "&amp;", "&quot;", "&lt;" and "&gt;" are currently
 * supported.
 *
 * @param html The string in which to unescape any HTML entities
 *
 * @return the text with HTML entities literalized
 */
char *gaim_unescape_html(const char *html);

/**
 * Returns a newly allocated substring of the HTML UTF-8 string "str".
 * The markup is preserved such that the substring will have the same
 * formatting as original string, even though some tags may have been
 * opened before "x", or may close after "y". All open tags are closed
 * at the end of the returned string, in the proper order.
 *
 * Note that x and y are in character offsets, not byte offsets, and
 * are offsets into an unformatted version of str. Because of this,
 * this function may be sensitive to changes in GtkIMHtml and may break
 * when used with other UI's. libgaim users are encouraged to report and
 * work out any problems encountered.
 *
 * @param str The input NUL terminated, HTML, UTF-8 (or ASCII) string.
 * @param x The character offset into an unformatted version of str to
 *          begin at.
 * @param y The character offset (into an unformatted vesion of str) of
 *          one past the last character to include in the slice.
 *
 * @return The HTML slice of string, with all formatting retained.
 */
char *gaim_markup_slice(const char *str, guint x, guint y);

/**
 * Returns a newly allocated string containing the name of the tag
 * located at "tag". Tag is expected to point to a '<', and contain
 * a '>' sometime after that. If there is no '>' and the string is
 * not NUL terminated, this function can be expected to segfault.
 *
 * @param tag The string starting a HTML tag.
 * @return A string containing the name of the tag.
 */
char *gaim_markup_get_tag_name(const char *tag);

/*@}*/


/**************************************************************************/
/** @name Path/Filename Functions                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns the user's home directory.
 *
 * @return The user's home directory.
 *
 * @see gaim_user_dir()
 */
const gchar *gaim_home_dir(void);

/**
 * Returns the gaim settings directory in the user's home directory.
 * This is usually ~/.gaim
 *
 * @return The gaim settings directory.
 *
 * @see gaim_home_dir()
 */
const char *gaim_user_dir(void);

/**
 * Define a custom gaim settings directory, overriding the default (user's home directory/.gaim)
 * @param dir The custom settings directory
 */
void gaim_util_set_user_dir(const char *dir);

/**
 * Builds a complete path from the root, making any directories along
 * the path which do not already exist.
 *
 * @param path The path you wish to create.  Note that it must start
 *        from the root or this function will fail.
 * @param mode Unix-style permissions for this directory.
 *
 * @return 0 for success, nonzero on any error.
 */
int gaim_build_dir(const char *path, int mode);

/**
 * Write a string of data to a file of the given name in the Gaim
 * user directory ($HOME/.gaim by default).  The data is typically
 * a serialized version of one of Gaim's config files, such as
 * prefs.xml, accounts.xml, etc.  And the string is typically
 * obtained using xmlnode_to_formatted_str.  However, this function
 * should work fine for saving binary files as well.
 *
 * @param filename The basename of the file to write in the gaim_user_dir.
 * @param data     A null-terminated string of data to write.
 * @param size     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * @return TRUE if the file was written successfully.  FALSE otherwise.
 */
gboolean gaim_util_write_data_to_file(const char *filename, const char *data,
									  size_t size);

/**
 * Read the contents of a given file and parse the results into an
 * xmlnode tree structure.  This is intended to be used to read
 * Gaim's configuration xml files (prefs.xml, pounces.xml, etc.)
 *
 * @param filename    The basename of the file to open in the gaim_user_dir.
 * @param description A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * @return An xmlnode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 */
xmlnode *gaim_util_read_xml_from_file(const char *filename,
									  const char *description);

/**
 * Creates a temporary file and returns a file pointer to it.
 *
 * This is like mkstemp(), but returns a file pointer and uses a
 * pre-set template. It uses the semantics of tempnam() for the
 * directory to use and allocates the space for the file path.
 *
 * The caller is responsible for closing the file and removing it when
 * done, as well as freeing the space pointed to by @a path with
 * g_free().
 *
 * @param path   The returned path to the temp file.
 * @param binary Text or binary, for platforms where it matters.
 *
 * @return A file pointer to the temporary file, or @c NULL on failure.
 */
FILE *gaim_mkstemp(char **path, gboolean binary);

/**
 * Checks if the given program name is valid and executable.
 *
 * @param program The file name of the application.
 *
 * @return TRUE if the program is runable.
 */
gboolean gaim_program_is_valid(const char *program);

/**
 * Check if running GNOME.
 *
 * @return TRUE if running GNOME, FALSE otherwise.
 */
gboolean gaim_running_gnome(void);

/**
 * Check if running KDE.
 *
 * @return TRUE if running KDE, FALSE otherwise.
 */
gboolean gaim_running_kde(void);

/**
 * Check if running OS X.
 *
 * @return TRUE if running OS X, FALSE otherwise.
 */
gboolean gaim_running_osx(void);

/**
 * Returns the IP address from a socket file descriptor.
 *
 * @param fd The socket file descriptor.
 *
 * @return The IP address, or @c NULL on error.
 */
char *gaim_fd_get_ip(int fd);

/*@}*/


/**************************************************************************/
/** @name String Functions                                                */
/**************************************************************************/
/*@{*/

/**
 * Normalizes a string, so that it is suitable for comparison.
 *
 * The returned string will point to a static buffer, so if the
 * string is intended to be kept long-term, you <i>must</i>
 * g_strdup() it. Also, calling normalize() twice in the same line
 * will lead to problems.
 *
 * @param account  The account the string belongs to, or NULL if you do
 *                 not know the account.  If you use NULL, the string
 *                 will still be normalized, but if the PRPL uses a
 *                 custom normalization function then the string may
 *                 not be normalized correctly.
 * @param str      The string to normalize.
 *
 * @return A pointer to the normalized version stored in a static buffer.
 */
const char *gaim_normalize(const GaimAccount *account, const char *str);

/**
 * Normalizes a string, so that it is suitable for comparison.
 *
 * This is one possible implementation for the PRPL callback
 * function "normalize."  It returns a lowercase and UTF-8
 * normalized version of the string.
 *
 * @param account  The account the string belongs to.
 * @param str      The string to normalize.
 *
 * @return A pointer to the normalized version stored in a static buffer.
 */
const char *gaim_normalize_nocase(const GaimAccount *account, const char *str);

/**
 * Compares two strings to see if the first contains the second as
 * a proper prefix.
 *
 * @param s  The string to check.
 * @param p  The prefix in question.
 *
 * @return   TRUE if p is a prefix of s, otherwise FALSE.
 */
gboolean gaim_str_has_prefix(const char *s, const char *p);

/**
 * Compares two strings to see if the second is a proper suffix
 * of the first.
 *
 * @param s  The string to check.
 * @param x  The suffix in question.
 *
 * @return   TRUE if x is a a suffix of s, otherwise FALSE.
 */
gboolean gaim_str_has_suffix(const char *s, const char *x);

/**
 * Duplicates a string and replaces all newline characters from the
 * source string with HTML linebreaks.
 *
 * @param src The source string.
 *
 * @return The new string.  Must be g_free'd by the caller.
 */
gchar *gaim_strdup_withhtml(const gchar *src);

/**
 * Ensures that all linefeeds have a matching carriage return.
 *
 * @param str The source string.
 *
 * @return The string with carriage returns.
 */
char *gaim_str_add_cr(const char *str);

/**
 * Strips all instances of the given character from the
 * given string.  The string is modified in place.  This
 * is useful for stripping new line characters, for example.
 *
 * Example usage:
 * gaim_str_strip_char(my_dumb_string, '\n');
 *
 * @param str     The string to strip characters from.
 * @param thechar The character to strip from the given string.
 */
void gaim_str_strip_char(char *str, char thechar);

/**
 * Given a string, this replaces all instances of one character
 * with another.  This happens inline (the original string IS
 * modified).
 *
 * @param string The string from which to replace stuff.
 * @param delimiter The character you want replaced.
 * @param replacement The character you want inserted in place
 *        of the delimiting character.
 */
void gaim_util_chrreplace(char *string, char delimiter,
						  char replacement);

/**
 * Given a string, this replaces one substring with another
 * and returns a newly allocated string.
 *
 * @param string The string from which to replace stuff.
 * @param delimiter The substring you want replaced.
 * @param replacement The substring you want inserted in place
 *        of the delimiting substring.
 *
 * @return A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *gaim_strreplace(const char *string, const char *delimiter,
					   const char *replacement);


/**
 * Given a string, this replaces any utf-8 substrings in that string with
 * the corresponding numerical character reference, and returns a newly
 * allocated string.
 *
 * @param in The string which might contain utf-8 substrings
 *
 * @return A new string, with utf-8 replaced with numerical character
 *         references, free this with g_free()
*/
char *gaim_utf8_ncr_encode(const char *in);


/**
 * Given a string, this replaces any numerical character references
 * in that string with the corresponding actual utf-8 substrings,
 * and returns a newly allocated string.
 *
 * @param in The string which might contain numerical character references.
 *
 * @return A new string, with numerical character references
 *         replaced with actual utf-8, free this with g_free().
 */
char *gaim_utf8_ncr_decode(const char *in);


/**
 * Given a string, this replaces one substring with another
 * ignoring case and returns a newly allocated string.
 *
 * @param string The string from which to replace stuff.
 * @param delimiter The substring you want replaced.
 * @param replacement The substring you want inserted in place
 *        of the delimiting substring.
 *
 * @return A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *gaim_strcasereplace(const char *string, const char *delimiter,
						   const char *replacement);

/**
 * This is like strstr, except that it ignores ASCII case in
 * searching for the substring.
 *
 * @param haystack The string to search in.
 * @param needle   The substring to find.
 *
 * @return the location of the substring if found, or NULL if not
 */
const char *gaim_strcasestr(const char *haystack, const char *needle);

/**
 * Returns a string representing a filesize in the appropriate
 * units (MB, KB, GB, etc.)
 *
 * @param size The size
 *
 * @return The string in units form. This must be freed.
 */
char *gaim_str_size_to_units(size_t size);

/**
 * Converts seconds into a human-readable form.
 *
 * @param sec The seconds.
 *
 * @return A human-readable form, containing days, hours, minutes, and
 *         seconds.
 */
char *gaim_str_seconds_to_string(guint sec);

/**
 * Converts a binary string into a NUL terminated ascii string,
 * replacing nonascii characters and characters below SPACE (including
 * NUL) into \\xyy, where yy are two hex digits. Also backslashes are
 * changed into two backslashes (\\\\). The returned, newly allocated
 * string can be outputted to the console, and must be g_free()d.
 *
 * @param binary A string of random data, possibly with embedded NULs
 *               and such.
 * @param len The length in bytes of the input string. Must not be 0.
 *
 * @return A newly allocated ASCIIZ string.
 */
char *gaim_str_binary_to_ascii(const unsigned char *binary, guint len);
/*@}*/


/**************************************************************************/
/** @name URI/URL Functions                                               */
/**************************************************************************/
/*@{*/

void gaim_got_protocol_handler_uri(const char *uri);

/**
 * Parses a URL, returning its host, port, file path, username and password.
 *
 * The returned data must be freed.
 *
 * @param url      The URL to parse.
 * @param ret_host The returned host.
 * @param ret_port The returned port.
 * @param ret_path The returned path.
 * @param ret_user The returned username.
 * @param ret_passwd The returned password.
 */
gboolean gaim_url_parse(const char *url, char **ret_host, int *ret_port,
						char **ret_path, char **ret_user, char **ret_passwd);

/**
 * This is the signature used for functions that act as the callback
 * to gaim_util_fetch_url() or gaim_util_fetch_url_request().
 *
 * @param url_data      The same value that was returned when you called
 *                      gaim_fetch_url() or gaim_fetch_url_request().
 * @param user_data     The user data that your code passed into either
 *                      gaim_util_fetch_url() or gaim_util_fetch_url_request().
 * @param url_text      This will be NULL on error.  Otherwise this
 *                      will contain the contents of the URL.
 * @param len           0 on error, otherwise this is the length of buf.
 * @param error_message If something went wrong then this will contain
 *                      a descriptive error message, and buf will be
 *                      NULL and len will be 0.
 */
typedef void (*GaimUtilFetchUrlCallback)(GaimUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);

/**
 * Fetches the data from a URL, and passes it to a callback function.
 *
 * @param url        The URL.
 * @param full       TRUE if this is the full URL, or FALSE if it's a
 *                   partial URL.
 * @param user_agent The user agent field to use, or NULL.
 * @param http11     TRUE if HTTP/1.1 should be used to download the file.
 * @param cb         The callback function.
 * @param data       The user data to pass to the callback function.
 */
#define gaim_util_fetch_url(url, full, user_agent, http11, cb, data) \
	gaim_util_fetch_url_request(url, full, user_agent, http11, NULL, \
		FALSE, cb, data);

/**
 * Fetches the data from a URL, and passes it to a callback function.
 *
 * @param url        The URL.
 * @param full       TRUE if this is the full URL, or FALSE if it's a
 *                   partial URL.
 * @param user_agent The user agent field to use, or NULL.
 * @param http11     TRUE if HTTP/1.1 should be used to download the file.
 * @param request    A HTTP request to send to the server instead of the
 *                   standard GET
 * @param include_headers
 *                   If TRUE, include the HTTP headers in the response.
 * @param callback   The callback function.
 * @param data       The user data to pass to the callback function.
 */
GaimUtilFetchUrlData *gaim_util_fetch_url_request(const gchar *url,
		gboolean full, const gchar *user_agent, gboolean http11,
		const gchar *request, gboolean include_headers,
		GaimUtilFetchUrlCallback callback, gpointer data);

/**
 * Cancel a pending URL request started with either
 * gaim_util_fetch_url_request() or gaim_util_fetch_url().
 *
 * @param url_data The data returned when you initiated the URL fetch.
 */
void gaim_util_fetch_url_cancel(GaimUtilFetchUrlData *url_data);

/**
 * Decodes a URL into a plain string.
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
const char *gaim_url_decode(const char *str);

/**
 * Encodes a URL into an escaped string.
 *
 * This will change non-alphanumeric characters to hex codes.
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
const char *gaim_url_encode(const char *str);

/**
 * Checks if the given email address is syntactically valid.
 *
 * @param address The email address to validate.
 *
 * @return True if the email address is syntactically correct.
 */
gboolean gaim_email_is_valid(const char *address);

/**
 * This function extracts a list of URIs from the a "text/uri-list"
 * string.  It was "borrowed" from gnome_uri_list_extract_uris
 *
 * @param uri_list An uri-list in the standard format.
 *
 * @return A GList containing strings allocated with g_malloc
 *         that have been splitted from uri-list.
 */
GList *gaim_uri_list_extract_uris(const gchar *uri_list);

/**
 * This function extracts a list of filenames from a
 * "text/uri-list" string.  It was "borrowed" from
 * gnome_uri_list_extract_filenames
 *
 * @param uri_list A uri-list in the standard format.
 *
 * @return A GList containing strings allocated with g_malloc that
 *         contain the filenames in the uri-list. Note that unlike
 *         gaim_uri_list_extract_uris() function, this will discard
 *         any non-file uri from the result value.
 */
GList *gaim_uri_list_extract_filenames(const gchar *uri_list);

/*@}*/

/**************************************************************************
 * UTF8 String Functions
 **************************************************************************/
/*@{*/

/**
 * Attempts to convert a string to UTF-8 from an unknown encoding.
 *
 * This function checks the locale and tries sane defaults.
 *
 * @param str The source string.
 *
 * @return The UTF-8 string, or @c NULL if it could not be converted.
 */
gchar *gaim_utf8_try_convert(const char *str);

/**
 * Salvages the valid UTF-8 characters from a string, replacing any
 * invalid characters with a filler character (currently hardcoded to
 * '?').
 *
 * @param str The source string.
 *
 * @return A valid UTF-8 string.
 */
gchar *gaim_utf8_salvage(const char *str);

/**
 * Compares two UTF-8 strings case-insensitively.
 *
 * @param a The first string.
 * @param b The second string.
 *
 * @return -1 if @a is less than @a b.
 *          0 if @a is equal to @a b.
 *          1 if @a is greater than @a b.
 */
int gaim_utf8_strcasecmp(const char *a, const char *b);

/**
 * Case insensitive search for a word in a string. The needle string
 * must be contained in the haystack string and not be immediately
 * preceded or immediately followed by another alpha-numeric character.
 *
 * @param haystack The string to search in.
 * @param needle   The substring to find.
 *
 * @return TRUE if haystack has the word, otherwise FALSE
 */
gboolean gaim_utf8_has_word(const char *haystack, const char *needle);

/**
 * Prints a UTF-8 message to the given file stream. The function
 * tries to convert the UTF-8 message to user's locale. If this
 * is not possible, the original UTF-8 text will be printed.
 *
 * @param filestream The file stream (e.g. STDOUT or STDERR)
 * @param message    The message to print.
 */
void gaim_print_utf8_to_console(FILE *filestream, char *message);

/**
 * Checks for messages starting with "/me "
 *
 * @param message The message to check
 * @param len     The message length, or -1
 *
 * @return TRUE if it starts with /me, and it has been removed, otherwise FALSE
 */
gboolean gaim_message_meify(char *message, size_t len);

/**
 * Removes the underscore characters from a string used identify the mnemonic
 * character.
 *
 * @param in  The string to strip
 *
 * @return The stripped string
 */
char *gaim_text_strip_mnemonic(const char *in);

/*@}*/

/**
 * Adds 8 to something.
 *
 * Blame SimGuy.
 *
 * @param x The number to add 8 to.
 *
 * @return x + 8
 */
#define gaim_add_eight(x) ((x)+8)

/**
 * Does the reverse of gaim_escape_filename
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
const char *gaim_unescape_filename(const char *str);

/**
 * Escapes filesystem-unfriendly characters from a filename
 *
 * @param str The string to translate.
 *
 * @return The resulting string.
 */
const char *gaim_escape_filename(const char *str);

/**
 * This is added temporarily to assist the split of oscar into aim and icq.
 * This should not be used by plugins.
 */
const char *_gaim_oscar_convert(const char *act, const char *protocol);

/**
 * Restore default signal handlers for signals which might reasonably have
 * handlers. This should be called by a fork()'d child process, since child processes
 * inherit the handlers of the parent.
 */
void gaim_restore_default_signal_handlers(void);
#ifdef __cplusplus
}
#endif

#endif /* _GAIM_UTIL_H_ */
