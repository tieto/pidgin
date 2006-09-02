typedef struct group *Gaim__Gtk__Group;

#define group perl_group

#include <glib.h>
#include <gtk/gtk.h>
#ifdef _WIN32
#undef pipe
#endif
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#undef group

#include <plugins/perl/common/module.h>

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkconvwin.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkft.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtklog.h"
#include "gtkmenutray.h"
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtksavedstatuses.h"
#include "gtksession.h"
#include "gtksound.h"
#include "gtkstatusbox.h"
#include "gtkthemes.h"
#include "gtkutils.h"

/* gtkaccount.h */
typedef GaimGtkAccountDialogType			Gaim__Gtk__Account__Dialog__Type;

/* gtkblist.h */
typedef GaimGtkBuddyList *				Gaim__Gtk__BuddyList;
typedef gaim_gtk_blist_sort_function			Gaim__Gtk__BuddyList__SortFunction;

/* gtkconv.h */
typedef GaimGtkConversation *				Gaim__Gtk__Conversation;
typedef GaimUnseenState					Gaim__UnseenState;

/* gtkconvwin.h */
typedef GaimGtkWindow *					Gaim__Gtk__Conversation__Window;
typedef GaimConvPlacementFunc				Gaim__Conversation__PlacementFunc;

/* gtkft.h */
typedef GaimGtkXferDialog *				Gaim__Gtk__Xfer__Dialog;

/* gtkimhtml.h */
typedef GtkIMHtml *					Gaim__Gtk__IMHtml;
typedef GtkIMHtmlButtons					Gaim__Gtk__IMHtml__Buttons;
typedef GtkIMHtmlFuncs *					Gaim__Gtk__IMHtml__Funcs;
typedef GtkIMHtmlScalable *					Gaim__Gtk__IMHtml__Scalable;
typedef GtkIMHtmlSmiley *					Gaim__Gtk__IMHtml__Smiley;
typedef GtkIMHtmlOptions					Gaim__Gtk__IMHtml__Options;

/* gtkimhtmltoolbar.h */
typedef GtkIMHtmlToolbar *					Gaim__Gtk__IMHtmlToolbar;

/* gtkmenutray.h */
typedef GaimGtkMenuTray *					Gaim__Gtk__MenuTray;

/* gtkroomlist.h */
typedef GaimGtkRoomlistDialog *					Gaim__Gtk__Roomlist__Dialog;

/* gtkstatusbox.h */
typedef GtkGaimStatusBox *						Gaim__Gtk__StatusBox;
