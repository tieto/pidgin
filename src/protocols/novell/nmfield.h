/*
 * nmfield.h
 *
 * Copyright © 2004 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK OF NOVELL, INC. NO PART OF THIS WORK MAY BE
 * USED, PRACTICED, PERFORMED, COPIED, DISTRIBUTED, REVISED, MODIFIED,
 * TRANSLATED, ABRIDGED, CONDENSED, EXPANDED, COLLECTED, COMPILED, LINKED,
 * RECAST, TRANSFORMED OR ADAPTED WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL,
 * INC. ANY USE OR EXPLOITATION OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 *
 * AS BETWEEN [GAIM] AND NOVELL, NOVELL GRANTS [GAIM] THE RIGHT TO REPUBLISH
 * THIS WORK UNDER THE GPL (GNU GENERAL PUBLIC LICENSE) WITH ALL RIGHTS AND
 * LICENSES THEREUNDER.  IF YOU HAVE RECEIVED THIS WORK DIRECTLY OR INDIRECTLY
 * FROM [GAIM] AS PART OF SUCH A REPUBLICATION, YOU HAVE ALL RIGHTS AND LICENSES
 * GRANTED BY [GAIM] UNDER THE GPL.  IN CONNECTION WITH SUCH A REPUBLICATION, IF
 * ANYTHING IN THIS NOTICE CONFLICTS WITH THE TERMS OF THE GPL, SUCH TERMS
 * PREVAIL.
 *
 */

#ifndef NMFIELD_H
#define NMFIELD_H

#include <glib.h>

typedef struct NMField_t
{
	char *tag;				/* Field tag */

	guint8 method;			/* Method of the field */

	guint8 flags;			/* Flags */

	guint8 type;			/* Type of value */

	guint32 size;			/* Size of value if binary */

	guint32 value;			/* Value of field */

	guint32 len;			/* Length of the array */

} NMField;

/* Field types */
#define	NMFIELD_TYPE_INVALID			0
#define	NMFIELD_TYPE_NUMBER				1
#define	NMFIELD_TYPE_BINARY				2
#define	NMFIELD_TYPE_BYTE				3
#define	NMFIELD_TYPE_UBYTE				4
#define	NMFIELD_TYPE_WORD				5
#define	NMFIELD_TYPE_UWORD				6
#define	NMFIELD_TYPE_DWORD				7
#define	NMFIELD_TYPE_UDWORD				8
#define	NMFIELD_TYPE_ARRAY				9
#define	NMFIELD_TYPE_UTF8				10
#define	NMFIELD_TYPE_BOOL				11
#define	NMFIELD_TYPE_MV					12
#define	NMFIELD_TYPE_DN					13

/* Field methods */
#define NMFIELD_METHOD_VALID			0
#define NMFIELD_METHOD_IGNORE			1
#define NMFIELD_METHOD_DELETE			2
#define NMFIELD_METHOD_DELETE_ALL		3
#define NMFIELD_METHOD_EQUAL			4
#define NMFIELD_METHOD_ADD				5
#define NMFIELD_METHOD_UPDATE			6
#define NMFIELD_METHOD_GTE				10
#define NMFIELD_METHOD_LTE				12
#define NMFIELD_METHOD_NE				14
#define NMFIELD_METHOD_EXIST			15
#define NMFIELD_METHOD_NOTEXIST			16
#define NMFIELD_METHOD_SEARCH			17
#define NMFIELD_METHOD_MATCHBEGIN		19
#define NMFIELD_METHOD_MATCHEND			20
#define NMFIELD_METHOD_NOT_ARRAY		40
#define NMFIELD_METHOD_OR_ARRAY			41
#define NMFIELD_METHOD_AND_ARRAY		42

/* Attribute Names (field tags) */
#define NM_A_IP_ADDRESS					"nnmIPAddress"
#define	NM_A_PORT						"nnmPort"
#define	NM_A_FA_FOLDER					"NM_A_FA_FOLDER"
#define	NM_A_FA_CONTACT					"NM_A_FA_CONTACT"
#define	NM_A_FA_CONVERSATION			"NM_A_FA_CONVERSATION"
#define	NM_A_FA_MESSAGE					"NM_A_FA_MESSAGE"
#define	NM_A_FA_CONTACT_LIST			"NM_A_FA_CONTACT_LIST"
#define	NM_A_FA_RESULTS					"NM_A_FA_RESULTS"
#define	NM_A_FA_INFO_DISPLAY_ARRAY		"NM_A_FA_INFO_DISPLAY_ARRAY"
#define	NM_A_FA_USER_DETAILS			"NM_A_FA_USER_DETAILS"
#define	NM_A_SZ_OBJECT_ID				"NM_A_SZ_OBJECT_ID"
#define	NM_A_SZ_PARENT_ID				"NM_A_SZ_PARENT_ID"
#define	NM_A_SZ_SEQUENCE_NUMBER			"NM_A_SZ_SEQUENCE_NUMBER"
#define	NM_A_SZ_TYPE					"NM_A_SZ_TYPE"
#define	NM_A_SZ_STATUS					"NM_A_SZ_STATUS"
#define	NM_A_SZ_STATUS_TEXT				"NM_A_SZ_STATUS_TEXT"
#define	NM_A_SZ_DN						"NM_A_SZ_DN"
#define	NM_A_SZ_DISPLAY_NAME			"NM_A_SZ_DISPLAY_NAME"
#define	NM_A_SZ_USERID					"NM_A_SZ_USERID"
#define NM_A_SZ_CREDENTIALS				"NM_A_SZ_CREDENTIALS"
#define	NM_A_SZ_MESSAGE_BODY			"NM_A_SZ_MESSAGE_BODY"
#define	NM_A_SZ_MESSAGE_TEXT			"NM_A_SZ_MESSAGE_TEXT"
#define	NM_A_UD_MESSAGE_TYPE			"NM_A_UD_MESSAGE_TYPE"
#define	NM_A_FA_PARTICIPANTS			"NM_A_FA_PARTICIPANTS"
#define	NM_A_FA_INVITES					"NM_A_FA_INVITES"
#define	NM_A_FA_EVENT					"NM_A_FA_EVENT"
#define	NM_A_UD_COUNT					"NM_A_UD_COUNT"
#define	NM_A_UD_DATE					"NM_A_UD_DATE"
#define	NM_A_UD_EVENT					"NM_A_UD_EVENT"
#define	NM_A_B_NO_CONTACTS				"NM_A_B_NO_CONTACTS"
#define	NM_A_B_NO_CUSTOMS				"NM_A_B_NO_CUSTOMS"
#define	NM_A_B_NO_PRIVACY				"NM_A_B_NO_PRIVACY"
#define	NM_A_UW_STATUS					"NM_A_UW_STATUS"
#define	NM_A_UD_OBJECT_ID				"NM_A_UD_OBJECT_ID"
#define	NM_A_SZ_TRANSACTION_ID			"NM_A_SZ_TRANSACTION_ID"
#define	NM_A_SZ_RESULT_CODE				"NM_A_SZ_RESULT_CODE"
#define	NM_A_UD_BUILD					"NM_A_UD_BUILD"
#define	NM_A_SZ_AUTH_ATTRIBUTE			"NM_A_SZ_AUTH_ATTRIBUTE"
#define	NM_A_UD_KEEPALIVE				"NM_A_UD_KEEPALIVE"
#define NM_A_SZ_USER_AGENT				"NM_A_SZ_USER_AGENT"

#define NM_PROTOCOL_VERSION		 		2

#define	NM_FIELD_TRUE					"1"
#define	NM_FIELD_FALSE					"0"

#define NMFIELD_MAX_STR_LENGTH			32768

/**
 * Count the number of fields
 *
 * @param fields	Field array
 *
 * @return			The number of fields in the array.
 *
 */
guint32 nm_count_fields(NMField * fields);

/**
 * Add a field to the field array. NOTE: field array that is passed
 * in may be realloced so you should use the returned field array pointer
 * not the passed in pointer after calling this function.
 *
 * @param fields	Field array
 * @param tag		Tag for the new field
 * @param size		Size of the field value (if type = binary)
 * @param method	Field method (see method defines above)
 * @param flags		Flags for new field
 * @param value		The value of the field
 * @param type		The type of the field value
 *
 * @return			Pointer to the updated field array
 *
 */
NMField *nm_add_field(NMField * fields, char *tag, guint32 size, guint8 method,
					  guint8 flags, guint32 value, guint8 type);

/**
 * Recursively free an array of fields and set pointer to NULL.
 *
 * @param fields	Pointer to a field array
 *
 */
void nm_free_fields(NMField ** fields);

/**
 * Find first field with given tag in field array.
 *
 * Note: this will only work for 7-bit ascii tags (which is all that
 * we use currently).
 *
 * @param tag		Tag to search for
 * @param fields	Field array
 *
 * @return			The first matching field, or NULL if no fields match.
 *
 */
NMField *nm_locate_field(char *tag, NMField * fields);

/**
 * Make a deep copy of a field array
 *
 * @param src		The array to copy
 *
 * @return			The new (copied) array, which must be freed.
 *
 */
NMField *nm_copy_field_array(NMField * src);

/**
 * Remove a field and move other fields up to fill the gap
 *
 * @param field		The field to remove
 *
 */
void nm_remove_field(NMField * field);

/* Print a field array (for debugging purposes) */
void nm_print_fields(NMField * fields);

#endif
