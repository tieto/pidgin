/**
 * @file keyring.h Keyring plugin API
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
 */

#ifndef _PURPLE_KEYRING_H_
#define _PURPLE_KEYRING_H_

#include <whatever>		// FIXME
#include "account.h"

/********************************************************
 * DATA STRUCTURES **************************************
 ********************************************************/


/* maybe strip a couple GError* if they're not used */

typedef void (*PurpleKeyringReadCallback)(const PurpleAccount * account, int result, gchar * password, GError * error, gpointer data);
typedef void (*PurpleKeyringSaveCallback)(const PurpleAccount * account, int result, GError * error, gpointer data);
typedef void (*PurpleKeyringCloseCallback)(int result, Gerror * error, gpointer data);	/* async just so we can check for errors */
typedef void (*PurpleKeyringChangeMasterCallback)(int result, Gerror * error, gpointer data);

typedef void (*PurpleKeyringRead)(const PurpleAccount * account, gchar * masterpassword, GError ** error, PurpleKeyringReadCallback cb, gpointer data);
typedef void (*PurpleKeyringSave)(const PurpleAccount * account, gchar * masterpassword, GError ** error, PurpleKeyringSaveCallback cb, gpointer data);
typedef void (*PurpleKeyringClose)(GError ** error, gpointer data);
typedef void (*PurpleKeyringChangeMaster)(gchar * oldpassword, gchar * newpassword, GError ** error, PurpleKeyringChangeMasterCallback cb, gpointer data);
typedef void (*PurpleKeyringFree)(gchar * password, GError ** error);

typedef struct _PurpleKeyringInfo {
	PurpleKeyringRead read_password;
	PurpleKeyringSave save_password;
	PurpleKeyringClose close_keyring;
	PurpleKeyringFree free_password;
	PurpleKeyringChangeMaster change_master;
	int capabilities;
	gpointer r1;	/* RESERVED */
	gpointer r2;	/* RESERVED */
	gpointer r3;	/* RESERVED */
} PurpleKeyringInfo;


/***************************************/
/** @name Keyring API                  */
/***************************************/

void purple_plugin_keyring_register(PurpleKeyringInfo * info) /* do we need a callback ? */
