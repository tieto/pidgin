/**
 * @file util.h Utility Functions
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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

/**
 * Normalizes a string, so that it is suitable for comparison.
 *
 * The returned string will point to a static buffer, so if the
 * string is intended to be kept long-term, you <i>must</i>
 * g_strdup() it. Also, calling normalize() twice in the same line
 * will lead to problems.
 *
 * @param str The string to normalize.
 *
 * @return A pointer to the normalized version stored in a static buffer.
 */
char *normalize(const char *str);

/**
 * Converts a string to its base-64 equivalent.
 *
 * @param buf The data to convert.
 * @param len The length of the data, or -1 if it's a NULL-terminated string.
 *
 * @return The base-64 version of @a str.
 *
 * @see frombase64()
 */
char *tobase64(const unsigned char *buf, size_t len);

/**
 * Converts a string back from its base-64 equivalent.
 *
 * @param str     The string to convert back.
 * @param ret_str The returned, non-base-64 string.
 * @param ret_len The returned string length.
 *
 * @see tobase64()
 */
void frombase64(const char *str, char **ret_str, int *ret_len);

/**
 * Converts a string to its base-16 equivalent.
 *
 * @param str The string to convert.
 * @param len The length of the string.
 *
 * @return The base-16 string.
 *
 * @see frombase16()
 */
char *tobase16(const char *str, int len);

/**
 * Converts a string back from its base-16 equivalent.
 *
 * @param str     The string to convert back.
 * @param ret_str The returned, non-base-16 string.
 * 
 * @return The length of the returned string.
 *
 * @see tobase16()
 */
int frombase16(const char *str, char **ret_str);

/**
 * Waits for all child processes to terminate.
 */
void clean_pid(void);

/**
 * Returns the current local time in hour:minute:second form.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's intended to be used for long.
 *
 * @return The current local time.
 *
 * @see full_date()
 */
char *date(void);

/**
 * Adds the necessary HTML code to turn URIs into HTML links in a string.
 *
 * @param str The string to linkify.
 *
 * @return The linkified text.
 */
char *linkify_text(const char *str);

/**
 * Converts seconds into a human-readable form.
 *
 * @param sec The seconds.
 *
 * @return A human-readable form, containing days, hours, minutes, and
 *         seconds.
 */
char *sec_to_text(guint sec);

/**
 * Finds a gaim_account with the specified name and protocol ID.
 *
 * @param name     The name of the account.
 * @param protocol The protocol type.
 *
 * @return The gaim_account, if found, or @c NULL otherwise.
 */
struct gaim_account *gaim_account_find(const char *name,
									   int protocol) G_GNUC_PURE;

/**
 * Returns the date and time in human-readable form.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's intended to be used for long.
 *
 * @return The date and time in human-readable form.
 *
 * @see date()
 */
char *full_date(void) G_GNUC_PURE;

/**
 * Looks for %n, %d, or %t in a string, and replaces them with the
 * specified name, date, and time, respectively.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's intended to be used for long.
 *
 * @param str  The string that may contain the special variables.
 * @param name The sender name.
 *
 * @return A new string where the special variables are expanded.
 */
char *away_subs(const char *str, const char *name);

/**
 * Stylizes the specified text using HTML, according to the current
 * font options.
 *
 * @param text The text to stylize.
 * @param len  The intended length of the new buffer.
 *
 * @return A newly allocated string of length @a len, containing the
 *         stylized version of @a text.
 *
 * @todo Move this to a UI-specific file.
 */
char *stylize(const gchar *text, int len);

/**
 * Shows the usage options for the gaim binary.
 *
 * @param mode @c 0 for full options, or @c 1 for a short summary.
 * @param name The name of the binary.
 * 
 * @todo Move this to the binary, when a library is formed.
 */
void show_usage(int mode, const char *name);

/**`
 * Returns the user's home directory.
 *
 * @return The user's home directory.
 *
 * @see gaim_user_dir()
 */
const gchar *gaim_home_dir(void);

/**
 * Returns the gaim settings directory in the user's home directory.
 * 
 * @return The gaim settings directory.
 *
 * @see gaim_home_dir()
 */
char *gaim_user_dir(void);

/**
 * Copies a string and replaces all HTML linebreaks with newline characters.
 *
 * @param dest     The destination string.
 * @param src      The source string.
 * @param dest_len The destination string length.
 *
 * @see strncpy_withhtml()
 * @see strdup_withhtml()
 */
void strncpy_nohtml(gchar *dest, const gchar *src, size_t dest_len);

/**
 * Copies a string and replaces all newline characters with HTML linebreaks.
 *
 * @param dest     The destination string.
 * @param src      The source string.
 * @param dest_len The destination string length.
 *
 * @see strncpy_nohtml()
 * @see strdup_withhtml()
 */
void strncpy_withhtml(gchar *dest, const gchar *src, size_t dest_len);

/**
 * Duplicates a string and replaces all newline characters from the
 * source string with HTML linebreaks.
 *
 * @param src The source string.
 *
 * @return The new string.
 *
 * @see strncpy_nohtml()
 * @see strncpy_withhtml()
 */
gchar *strdup_withhtml(const gchar *src);

/**
 * Ensures that all linefeeds have a matching carriage return.
 *
 * @param str The source string.
 *
 * @return The string with carriage returns.
 */
char *add_cr(const char *);

/**
 * Strips all linefeeds from a string.
 *
 * @param str The string to strip linefeeds from.
 */
void strip_linefeed(char *str);

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
time_t get_time(int year, int month, int day,
				int hour, int min, int sec) G_GNUC_CONST;

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
 * @param path The returned path to the temp file.
 * 
 * @return A file pointer to the temporary file, or @c NULL on failure.
 */
FILE *gaim_mkstemp(gchar **path);

/**
 * Acts upon an aim: URI.
 *
 * @param uri The URI.
 *
 * @return The response based off the action in the URI.
 */
const char *handle_uri(char *uri);

/* This guy does its best to convert a string to UTF-8 from an unknown
 * encoding by checking the locale and trying some sane defaults ...
 * if everything fails it returns NULL. */

/**
 * Attempts to convert a string to UTF-8 from an unknown encoding.
 *
 * This function checks the locale and tries sane defaults.
 *
 * @param str The source string.
 *
 * @return The UTF-8 string, or @c NULL if it could not be converted.
 */
char *gaim_try_conv_to_utf8(const char *str);

/**
 * Returns the IP address from a socket file descriptor.
 *
 * @param fd The socket file descriptor.
 *
 * @return The IP address, or @c NULL on error.
 */
char *gaim_getip_from_fd(int fd);

/**
 * Compares two UTF-8 strings.
 *
 * @param a The first string.
 * @param b The second string.
 *
 * @return -1 if @a is less than @a b.
 *          0 if @a is equal to @a b.
 *          1 if @a is greater than @a b.
 */
gint gaim_utf8_strcasecmp(const gchar *a, const gchar *b);

#endif /* _GAIM_UTIL_H_ */
