/**
 * @file error.h Error functions
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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
 */
#ifndef _MSN_ERROR_H_
#define _MSN_ERROR_H_

typedef enum
{
	MSN_ERROR_NONE,
	MSN_ERROR_MISC,
	MSN_ERROR_CONNECT,
	MSN_ERROR_WRITE,
	MSN_ERROR_READ,
	MSN_ERROR_SIGNOTHER,
	MSN_ERROR_SERVDOWN

} MsnErrorType;

#include "session.h"

/**
 * Returns the string representation of an error type.
 *
 * @param type The error type.
 *
 * @return The string representation of the error type.
 */
const char *msn_error_get_text(unsigned int type);

/**
 * Handles an error.
 *
 * @param session The current session.
 * @param type    The error type.
 */
void msn_error_handle(MsnSession *session, unsigned int type);

#endif /* _MSN_ERROR_H_ */
