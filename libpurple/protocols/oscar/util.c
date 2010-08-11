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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * A little bit of this
 * A little bit of that
 * It started with a kiss
 * Now we're up to bat
 */

#include "oscar.h"
#include <ctype.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

/*
 * Tokenizing functions.  Used to portably replace strtok/sep.
 *   -- DMP.
 *
 */
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
		toReturn = malloc(sizeof(char));
		*toReturn = '\0';
	} else {
		if (next == NULL) {
			toReturn = malloc((strlen(last) + 1) * sizeof(char));
			strcpy(toReturn, last);
		} else {
			toReturn = malloc((next - last + 1) * sizeof(char));
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
 * Check if the given screen name is a valid AIM screen name.
 * Example: BobDole
 * Example: Henry_Ford@mac.com
 *
 * @return TRUE if the screen name is valid, FALSE if not.
 */
static gboolean
aim_snvalid_aim(const char *sn)
{
	int i;

	for (i = 0; sn[i] != '\0'; i++) {
		if (!isalnum(sn[i]) && (sn[i] != ' ') &&
			(sn[i] != '@') && (sn[i] != '.') &&
			(sn[i] != '_') && (sn[i] != '-'))
			return FALSE;
	}

	return TRUE;
}

/**
 * Check if the given screen name is a valid ICQ screen name.
 * Example: 1234567
 *
 * @return TRUE if the screen name is valid, FALSE if not.
 */
static gboolean
aim_snvalid_icq(const char *sn)
{
	int i;

	for (i = 0; sn[i] != '\0'; i++) {
		if (!isdigit(sn[i]))
			return 0;
	}

	return 1;
}

/**
 * Check if the given screen name is a valid SMS screen name.
 * Example: +19195551234
 *
 * @return TRUE if the screen name is valid, FALSE if not.
 */
static gboolean
aim_snvalid_sms(const char *sn)
{
	int i;

	if (sn[0] != '+')
		return 0;

	for (i = 1; sn[i] != '\0'; i++) {
		if (!isdigit(sn[i]))
			return 0;
	}

	return 1;
}

/**
 * Check if the given screen name is a valid oscar screen name.
 *
 * @return TRUE if the screen name is valid, FALSE if not.
 */
gboolean
aim_snvalid(const char *sn)
{
	if ((sn == NULL) || (*sn == '\0'))
		return 0;

	if (isalpha(sn[0]))
		return aim_snvalid_aim(sn);
	else if (isdigit(sn[0]))
		return aim_snvalid_icq(sn);
	else if (sn[0] == '+')
		return aim_snvalid_sms(sn);

	return 0;
}

/**
 * Determine if a given screen name is an ICQ screen name
 * (i.e. it begins with a number).
 *
 * @sn A valid AIM or ICQ screen name.
 * @return TRUE if the screen name is an ICQ screen name.  Otherwise
 *         FALSE is returned.
 */
gboolean
aim_sn_is_icq(const char *sn)
{
	if (isalpha(sn[0]))
		return FALSE;
	return TRUE;
}

/**
 * Determine if a given screen name is an SMS number
 * (i.e. it begins with a +).
 *
 * @sn A valid AIM or ICQ screen name.
 * @return TRUE if the screen name is an SMS number.  Otherwise
 *         FALSE is returned.
 */
gboolean
aim_sn_is_sms(const char *sn)
{
	if (sn[0] != '+')
		return FALSE;
	return TRUE;
}

/**
 * This takes a screen name and returns its length without
 * spaces.  If there are no spaces in the SN, then the
 * return is equal to that of strlen().
 */
int
aim_snlen(const char *sn)
{
	int i = 0;

	if (!sn)
		return 0;

	while (*sn != '\0') {
		if (*sn != ' ')
			i++;
		sn++;
	}

	return i;
}

/**
 * This takes two screen names and compares them using the rules
 * on screen names for AIM/AOL.  Mainly, this means case and space
 * insensitivity (all case differences and spacing differences are
 * ignored, with the exception that screen names can not start with
 * a space).
 *
 * Return: 0 if equal
 *     non-0 if different
 */
int
aim_sncmp(const char *sn1, const char *sn2)
{

	if ((sn1 == NULL) || (sn2 == NULL))
		return -1;

	do {
		while (*sn2 == ' ')
			sn2++;
		while (*sn1 == ' ')
			sn1++;
		if (toupper(*sn1) != toupper(*sn2))
			return 1;
	} while ((*sn1 != '\0') && sn1++ && sn2++);

	return 0;
}
