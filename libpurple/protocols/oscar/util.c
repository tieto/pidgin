/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

/*
 * A little bit of this
 * A little bit of that
 * It started with a kiss
 * Now we're up to bat
 */

#include "oscar.h"

#include "core.h"

#include <ctype.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

int oscar_get_ui_info_int(const char *str, int default_value)
{
	GHashTable *ui_info;

	ui_info = purple_core_get_ui_info();
	if (ui_info != NULL) {
		gpointer value;
		if (g_hash_table_lookup_extended(ui_info, str, NULL, &value))
			return GPOINTER_TO_INT(value);
	}

	return default_value;
}

const char *oscar_get_ui_info_string(const char *str, const char *default_value)
{
	GHashTable *ui_info;
	const char *value = NULL;

	ui_info = purple_core_get_ui_info();
	if (ui_info != NULL)
		value = g_hash_table_lookup(ui_info, str);
	if (value == NULL)
		value = default_value;

	return value;
}

gchar *oscar_get_clientstring(void)
{
	const char *name, *version;

	name = oscar_get_ui_info_string("name", "Purple");
	version = oscar_get_ui_info_string("version", VERSION);

	return g_strdup_printf("%s/%s", name, version);;
}

/*
 * Tokenizing functions.  Used to portably replace strtok/sep.
 *   -- DMP.
 *
 */
/* TODO: Get rid of this and use glib functions */
int
aimutil_tokslen(char *toSearch, int theindex, char dl)
{
	int curCount = 1;
	char *next;
	char *last;
	int toReturn;

	last = toSearch;
	next = strchr(toSearch, dl);

	while(curCount < theindex && next != NULL) {
		curCount++;
		last = next + 1;
		next = strchr(last, dl);
	}

	if ((curCount < theindex) || (next == NULL))
		toReturn = strlen(toSearch) - (curCount - 1);
	else
		toReturn = next - toSearch - (curCount - 1);

	return toReturn;
}

int
aimutil_itemcnt(char *toSearch, char dl)
{
	int curCount;
	char *next;

	curCount = 1;

	next = strchr(toSearch, dl);

	while(next != NULL) {
		curCount++;
		next = strchr(next + 1, dl);
	}

	return curCount;
}

char *
aimutil_itemindex(char *toSearch, int theindex, char dl)
{
	int curCount;
	char *next;
	char *last;
	char *toReturn;

	curCount = 0;

	last = toSearch;
	next = strchr(toSearch, dl);

	while (curCount < theindex && next != NULL) {
		curCount++;
		last = next + 1;
		next = strchr(last, dl);
	}
	next = strchr(last, dl);

	if (curCount < theindex) {
		toReturn = g_malloc(sizeof(char));
		*toReturn = '\0';
	} else {
		if (next == NULL) {
			toReturn = g_malloc((strlen(last) + 1) * sizeof(char));
			strcpy(toReturn, last);
		} else {
			toReturn = g_malloc((next - last + 1) * sizeof(char));
			memcpy(toReturn, last, (next - last));
			toReturn[next - last] = '\0';
		}
	}
	return toReturn;
}

/**
 * Calculate the checksum of a given icon.
 */
guint16
aimutil_iconsum(const guint8 *buf, int buflen)
{
	guint32 sum;
	int i;

	for (i=0, sum=0; i+1<buflen; i+=2)
		sum += (buf[i+1] << 8) + buf[i];
	if (i < buflen)
		sum += buf[i];
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0x0000ffff);

	return sum;
}

/**
 * Check if the given name is a valid AIM username.
 * Example: BobDole
 * Example: Henry_Ford@mac.com
 * Example: 1KrazyKat@example.com
 *
 * @return TRUE if the name is valid, FALSE if not.
 */
static gboolean
oscar_util_valid_name_aim(const char *name)
{
	int i;

	if (purple_email_is_valid(name))
		return TRUE;

	/* Normal AIM usernames can't start with a number */
	if (isdigit(name[0]))
		return FALSE;

	for (i = 0; name[i] != '\0'; i++) {
		if (!isalnum(name[i]) && (name[i] != ' '))
			return FALSE;
	}

	return TRUE;
}

/**
 * Check if the given name is a valid ICQ username.
 * Example: 1234567
 *
 * @return TRUE if the name is valid, FALSE if not.
 */
gboolean
oscar_util_valid_name_icq(const char *name)
{
	int i;

	for (i = 0; name[i] != '\0'; i++) {
		if (!isdigit(name[i]))
			return FALSE;
	}

	return TRUE;
}

/**
 * Check if the given name is a valid SMS username.
 * Example: +19195551234
 *
 * @return TRUE if the name is valid, FALSE if not.
 */
gboolean
oscar_util_valid_name_sms(const char *name)
{
	int i;

	if (name[0] != '+')
		return FALSE;

	for (i = 1; name[i] != '\0'; i++) {
		if (!isdigit(name[i]))
			return FALSE;
	}

	return TRUE;
}

/**
 * Check if the given name is a valid oscar username.
 *
 * @return TRUE if the name is valid, FALSE if not.
 */
gboolean
oscar_util_valid_name(const char *name)
{
	if ((name == NULL) || (*name == '\0'))
		return FALSE;

	return oscar_util_valid_name_icq(name)
			|| oscar_util_valid_name_sms(name)
			|| oscar_util_valid_name_aim(name);
}

/**
 * This takes two names and compares them using the rules
 * on usernames for AIM/AOL.  Mainly, this means case and space
 * insensitivity (all case differences and spacing differences are
 * ignored, with the exception that usernames can not start with
 * a space).
 *
 * @return 0 if equal, non-0 if different
 */
/* TODO: Do something different for email addresses. */
int
oscar_util_name_compare(const char *name1, const char *name2)
{

	if ((name1 == NULL) || (name2 == NULL))
		return -1;

	do {
		while (*name2 == ' ')
			name2++;
		while (*name1 == ' ')
			name1++;
		if (toupper(*name1) != toupper(*name2))
			return 1;
	} while ((*name1 != '\0') && name1++ && name2++);

	return 0;
}
