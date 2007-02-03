typedef struct group *Gaim__GtkUI__Group;

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
typedef PidginAccountDialogType			Gaim__GtkUI__Account__Dialog__Type;

/* gtkblist.h */
typedef PidginBuddyList *				Gaim__GtkUI__BuddyList;
typedef pidgin_blist_sort_function			Gaim__GtkUI__BuddyList__SortFunction;

/* gtkconv.h */
typedef PidginConversation *				Gaim__GtkUI__Conversation;
typedef GaimUnseenState					Gaim__UnseenState;

/* gtkconvwin.h */
typedef PidginWindow *					Gaim__GtkUI__Conversation__Window;
typedef GaimConvPlacementFunc				Gaim__Conversation__PlacementFunc;

/* gtkft.h */
typedef PidginXferDialog *				Gaim__GtkUI__Xfer__Dialog;

/* gtkimhtml.h */
typedef GtkIMHtml *					Gaim__GtkUI__IMHtml;
typedef GtkIMHtmlButtons				Gaim__GtkUI__IMHtml__Buttons;
typedef GtkIMHtmlFuncs *				Gaim__GtkUI__IMHtml__Funcs;
typedef GtkIMHtmlScalable *				Gaim__GtkUI__IMHtml__Scalable;
typedef GtkIMHtmlSmiley *				Gaim__GtkUI__IMHtml__Smiley;
typedef GtkIMHtmlOptions				Gaim__GtkUI__IMHtml__Options;

/* gtkimhtmltoolbar.h */
typedef GtkIMHtmlToolbar *				Gaim__GtkUI__IMHtmlToolbar;

/* gtkmenutray.h */
typedef PidginMenuTray *				Gaim__GtkUI__MenuTray;

/* gtkstatusbox.h */
typedef GtkGaimStatusBox *				Gaim__GtkUI__StatusBox;
