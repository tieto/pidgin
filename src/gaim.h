/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */

#ifndef _GAIM_H_
#define _GAIM_H_

#define XPATCH BAD /* Because Kalla Said So */

#include "connection.h"

/* Globals in main.c */
extern int opt_away;
extern char *opt_away_arg;
extern char *opt_rcfile_arg;
extern int opt_debug;

extern GSList *message_queue;
extern GSList *unread_message_queue;
extern GSList *away_time_queue;

/* Functions in main.c */
extern void do_quit();

/* Functions in dialogs.c */
extern void g_show_info_text(GaimConnection *, const char *, int, const char *, ...);
extern void show_change_passwd(GaimConnection *);
extern void show_set_dir(GaimConnection *);
extern void show_find_email(GaimConnection *);
extern void show_find_info(GaimConnection *);
extern void show_set_info(GaimConnection *);
extern void show_confirm_del(struct buddy *);
extern void show_confirm_del_group(struct group *);
extern void show_confirm_del_chat(struct chat *);

/* Functions in gaimrc.c */
extern gint sort_awaymsg_list(gconstpointer, gconstpointer);

/* Functions in idle.c */
extern gint check_idle(gpointer);


/*------------------------------------------------------------------------*/
/*  Multi-Entry dialog and vCard dialog support                           */
/*------------------------------------------------------------------------*/

/*
 * Struct for "instructions" dialog data
 */
typedef struct multi_instr_dlg {
	GtkWidget *label;		/* dialog instructions widget */
	gchar *text;			/* dialog instructions */
} MultiInstrData;

/*
 * Struct for multiple-entry dialog data
 */
typedef struct multi_entry_data {
	GtkWidget *widget;		/* entry widget object */
	char *label;			/* label text pointer */
	char *text;			/* entry text pointer */
	int  visible;			/* should entry field be "visible?" */
	int  editable;			/* should entry field be editable? */
} MultiEntryData;

/*
 * Struct for multiple-textbox dialog data
 */
typedef struct multi_text_data {
	char *label;			/* frame label */
	GtkWidget *textbox;		/* text entry widget object */
	char *text;			/* textbox text pointer */
} MultiTextData;

/*
 * Struct to create a multi-entry dialog
 */
typedef struct multi_entry_dlg {
	GtkWidget *window;			/* dialog main window */
	gchar *role;				/* window role */
	char *title;				/* window title */

	GaimAccount *account;			/* user info - needed for most everything */

	MultiInstrData *instructions;		/* instructions (what else?) */

	GtkWidget *entries_table;		/* table widget containing m-e lables & entries */
	GtkWidget *entries_frame;		/* frame widget containing the table widget */
	gchar *entries_title;			/* title of multi-entries list */
	GSList *multi_entry_items;		/* entry dialogs parameters */

	GtkWidget *texts_ibox;			/* inner vbox containing multi-text frames */
	GtkWidget *texts_obox;			/* outer vbox containing multi-text frames */
	GSList *multi_text_items;		/* text dialogs parameters */

	void * (*custom)(struct multi_entry_dlg *);	/* Custom function that may be used by */
							/* multi-entry dialog "wrapper" functions */
							/* (Not used by multi-entry dialog routines) */

	void (*ok)(GtkWidget *, gpointer);	/* "Save/OK" action */
	void (*cancel)(GtkWidget *, gpointer);	/* "Cancel" action */
} MultiEntryDlg;

extern MultiTextData *multi_text_list_update(GSList **, const char *, const char *, int);
extern void multi_text_items_free_all(GSList **);
extern MultiEntryData *multi_entry_list_update(GSList **, const char *, const char *, int);
extern void multi_entry_items_free_all(GSList **);

extern void re_show_multi_entry_instr(MultiInstrData *);
extern void re_show_multi_entry_entries(GtkWidget **, GtkWidget *, GSList *);
extern void re_show_multi_entry_textboxes(GtkWidget **, GtkWidget *, GSList *);

extern MultiEntryDlg *multi_entry_dialog_new(void);
extern void show_multi_entry_dialog(gpointer);

extern void show_set_vcard(MultiEntryDlg *);

/*------------------------------------------------------------------------*/
/*  End Multi-Entry dialog and vCard dialog support                       */
/*------------------------------------------------------------------------*/

#endif /* _GAIM_H_ */
