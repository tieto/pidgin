/**
 * @file group.h Group functions
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _MSN_GROUP_H_
#define _MSN_GROUP_H_

typedef struct _MsnGroup  MsnGroup;

#include <stdio.h>

#include "session.h"
#include "user.h"
#include "soap.h"
#include "userlist.h"

#define MSN_ADD_GROUPS	"<GroupInfo><name>test111</name><groupType>C8529CE2-6EAD-434d-881F-341E17DB3FF8</groupType><fMessenger>false</fMessenger><annotations><Annotation><Name>MSN.IM.Display</Name><Value>1</Value></Annotation></annotations></GroupInfo>"

#define MSN_ADD_GROUP_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>GroupSave</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><ABGroupAdd xmlns=\"http://www.msn.com/webservices/AddressBook\"><abId>00000000-0000-0000-0000-000000000000</abId><groupAddOptions><fRenameOnMsgrConflict>false</fRenameOnMsgrConflict></groupAddOptions><groupInfo>%s</groupInfo></ABGroupAdd></soap:Body></soap:Envelope>"

#define MSN_GROUP_IDS	"<guid>9e57e654-59f0-44d1-aedc-0a7500b7e51f</guid>"
#define MSN_DELETE_GROUP_TEMPLATE	"<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\"><soap:Header><ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ApplicationId>09607671-1C32-421F-A6A6-CBFAA51AB5F4</ApplicationId><IsMigration>false</IsMigration><PartnerScenario>Timer</PartnerScenario></ABApplicationHeader><ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\"><ManagedGroupRequest>false</ManagedGroupRequest></ABAuthHeader></soap:Header><soap:Body><ABGroupDelete xmlns=\"http://www.msn.com/webservices/AddressBook\"><abId>00000000-0000-0000-0000-000000000000</abId><groupFilter><groupIds>%s</groupIds></groupFilter></ABGroupDelete></soap:Body></soap:Envelope>"

#define MSN_INDIVIDUALS_GROUP_ID	"1983"
#define MSN_INDIVIDUALS_GROUP_NAME	"Other Contacts"

#define MSN_NON_IM_GROUP_ID		"email"
#define MSN_NON_IM_GROUP_NAME	"Non-IM Contacts"

/**
 * A group.
 */
struct _MsnGroup
{
	MsnSession *session;    /**< The MSN session.           */
	MsnSoapConn *soapconn;

	char *id;                 /**< The group ID.              */
	char *name;             /**< The name of the group.     */
};

/**************************************************************************/
/** @name Group API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a new group structure.
 *
 * @param session The MSN session.
 * @param id      The group ID.
 * @param name    The name of the group.
 *
 * @return A new group structure.
 */
MsnGroup *msn_group_new(MsnUserList *userlist, const char *id, const char *name);

/**
 * Destroys a group structure.
 *
 * @param group The group to destroy.
 */
void msn_group_destroy(MsnGroup *group);

/**
 * Sets the ID for a group.
 *
 * @param group The group.
 * @param id    The ID.
 */
void msn_group_set_id(MsnGroup *group, const char *id);

/**
 * Sets the name for a group.
 *
 * @param group The group.
 * @param name  The name.
 */
void msn_group_set_name(MsnGroup *group, const char *name);

/**
 * Returns the ID for a group.
 *
 * @param group The group.
 *
 * @return The ID.
 */
char* msn_group_get_id(const MsnGroup *group);

/**
 * Returns the name for a group.
 *
 * @param group The group.
 *
 * @return The name.
 */
const char *msn_group_get_name(const MsnGroup *group);
#endif /* _MSN_GROUP_H_ */
