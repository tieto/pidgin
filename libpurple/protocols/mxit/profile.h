/*
 *					MXit Protocol libPurple Plugin
 *
 *					-- user profile's --
 *
 *				Andrew Victor	<libpurple@mxit.com>
 *
 *			(C) Copyright 2009	MXit Lifestyle (Pty) Ltd.
 *				<http://www.mxitlifestyle.com>
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
 */

#ifndef		_MXIT_PROFILE_H_
#define		_MXIT_PROFILE_H_

#include	<glib.h>


struct MXitProfile {
	/* required */
	char		loginname[64];						/* name user uses to log into MXit with (aka 'mxitid') */
	char		nickname[64];						/* user's own display name (aka 'nickname', aka 'fullname', aka 'alias') in MXit */
	char		birthday[16];						/* user's birthday "YYYY-MM-DD" */
	gboolean	male;								/* true if the user's gender is male (otherwise female) */
	char		pin[16];							/* user's password */

	/* optional */
	char		title[32];							/* user's title */
	char		firstname[64];						/* user's first name */
	char		lastname[64];						/* user's last name (aka 'surname') */
	char		email[64];							/* user's email address */
	char		mobilenr[21];						/* user's mobile number */
	char		regcountry[3];						/* user's registered country code */
	int64_t		flags;								/* user's profile flags */
	int64_t		lastonline;							/* user's last-online timestamp */

	gboolean	hidden;								/* set if the user's msisdn should remain hidden */
};

struct MXitSession;
void mxit_show_profile( struct MXitSession* session, const char* username, struct MXitProfile* profile );

gboolean validateDate( const char* bday );


#endif		/* _MXIT_PROFILE_H_ */
