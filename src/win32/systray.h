/*
 *  systray.h
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: November, 2002
 *  Description: Gaim systray functionality
 */

#ifndef _SYSTRAY_H_
#define _SYSTRAY_H_

extern void wgaim_systray_init(void);
extern void wgaim_created_blistwin( GtkWidget *blist );
extern void wgaim_created_loginwin( GtkWidget *loginwin );
extern void wgaim_systray_cleanup(void);

#endif /* _SYSTRAY_H_ */
