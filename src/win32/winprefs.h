/*
 *  winprefs.h
 *  
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: November, 2002
 *  Description: Windows only preferences page
 */
#ifndef _WINPREFS_H
#define _WINPREFS_H

extern guint wgaim_options;
#define OPT_WGAIM_IMTRANS               0x00000001
#define OPT_WGAIM_SHOW_IMTRANS          0x00000002

#if 0
#define OPT_WGAIM_                      0x00000004
#define OPT_WGAIM_                      0x00000008
#endif

extern GtkWidget *wgaim_winprefs_page();


#endif /*_WINPREFS_H*/
