/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZInitialize function.
 *
 *	Created by:	Robert French
 *
 *	$Source$
 *	$Author: warmenhoven $
 *
 *	Copyright (c) 1987, 1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h". 
 */
/* $Header$ */

#ifndef lint
static char rcsid_ZInitialize_c[] =
    "$Zephyr: /afs/athena.mit.edu/astaff/project/zephyr/src/lib/RCS/ZInitialize.c,v 1.17 89/05/30 18:11:25 jtkohl Exp $";
#endif

#include <internal.h>

#include <sys/socket.h>
#ifdef ZEPHYR_USES_KERBEROS
#include <krb_err.h>
#endif

Code_t ZInitialize()
{
    struct servent *hmserv;
    char addr[4];
#ifdef ZEPHYR_USES_KERBEROS
    int krbval;

    initialize_krb_error_table();
#endif

    initialize_zeph_error_table();
    
    (void) memset((char *)&__HM_addr, 0, sizeof(__HM_addr));

    __HM_addr.sin_family = AF_INET;

    /* Set up local loopback address for HostManager */
    addr[0] = 127;
    addr[1] = 0;
    addr[2] = 0;
    addr[3] = 1;

    hmserv = (struct servent *)getservbyname(HM_SVCNAME, "udp");
    __HM_addr.sin_port = (hmserv) ? hmserv->s_port : HM_SVC_FALLBACK;

    (void) memcpy((char *)&__HM_addr.sin_addr, addr, 4);

    __HM_set = 0;

#ifdef ZEPHYR_USES_KERBEROS
    if ((krbval = krb_get_lrealm(__Zephyr_realm, 1)) != KSUCCESS)
	return (krbval);
#endif

    /* Get the sender so we can cache it */
    (void) ZGetSender();

    /* Initialize the input queue */
    __Q_Tail = NULL;
    __Q_Head = NULL;
    
    return (ZERR_NONE);
}
