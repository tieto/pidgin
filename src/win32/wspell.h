/*
 *  wspell.h
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: March, 2003
 *  Description: Windows Gaim gtkspell interface.
 */
#ifndef _WSPELL_H_
#define _WSPELL_H_
#include <gtkspell/gtkspell.h>

extern void wgaim_gtkspell_init();

extern GtkSpell* (*wgaim_gtkspell_new_attach)(GtkTextView*, const gchar*, GError**);
#define gtkspell_new_attach( view, lang, error ) \
wgaim_gtkspell_new_attach( ## view ##, ## lang ##, ## error ## )

extern GtkSpell* (*wgaim_gtkspell_get_from_text_view)(GtkTextView*);
#define gtkspell_get_from_text_view( view ) \
wgaim_gtkspell_get_from_text_view( ## view ## )

extern void (*wgaim_gtkspell_detach)(GtkSpell*);
#define gtkspell_detach( spell ) \
wgaim_gtkspell_detach( ## spell ## )

extern gboolean (*wgaim_gtkspell_set_language)(GtkSpell*,	const gchar*, GError**);
#define gtkspell_set_language( spell, lang, error ) \
wgaim_gtkspell_set_language( ## spell ##, ## lang ##, ## error ## )

extern void (*wgaim_gtkspell_recheck_all)(GtkSpell*);
#define gtkspell_recheck_all( spell ) \
wgaim_gtkspell_recheck_all( ## spell ## )

#endif /* _WSPELL_H_ */
