/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZGetSender.c function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987, 1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */

#include "internal.h"

#ifndef WIN32
#include <pwd.h>
#endif

char *ZGetSender()
{
    struct passwd *pw;
#ifdef ZEPHYR_USES_KERBEROS
    char pname[ANAME_SZ], pinst[INST_SZ], prealm[REALM_SZ];
    static char sender[ANAME_SZ+INST_SZ+REALM_SZ+3] = "";
	long int kerror;
#else
    static char sender[128] = "";
#endif

#ifdef WIN32
	unsigned long sender_size = 127;
#endif

    /* Return it if already cached */

    /*    if (*sender)
	return (sender);
    */

#ifdef ZEPHYR_USES_KERBEROS
	if ((kerror = krb_get_tf_fullname((char *)TKT_FILE, pname, pinst, prealm)) == KSUCCESS)
    {
	(void) sprintf(sender, "%s%s%s@%s", pname, (pinst[0]?".":""),
		       pinst, prealm);
	return (sender);
    } else {
	sprintf(sender,"%s@%s",(username?username:"unknown"),__Zephyr_realm);
	return (sender);
    }
#endif

    /* XXX a uid_t is a u_short (now),  but getpwuid
     * wants an int. AARGH! */
#ifdef WIN32
    GetUserName(sender, &sender_size);
#else
    pw = getpwuid((int) getuid());
    if (!pw)
	return ("unknown");
    (void) sprintf(sender, "%s@%s", pw->pw_name, __Zephyr_realm);
#endif
    return (sender);
}
