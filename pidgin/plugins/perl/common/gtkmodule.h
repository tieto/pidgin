/* Allow the Perl code to see deprecated functions, so we can continue to
 * export them to Perl plugins. */
#undef PIDGIN_DISABLE_DEPRECATED

typedef struct group *Pidgin__Group;

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
#include "gtkbuddylist.h"
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
typedef PidginAccountDialogType		Pidgin__Account__Dialog__Type;

/* gtkbuddylist.h */
typedef PidginBuddyList *		Pidgin__BuddyList;
typedef pidgin_blist_sort_function	Pidgin__BuddyList__SortFunction;

/* gtkconv.h */
typedef PidginConversation *		Pidgin__Conversation;
typedef PidginUnseenState		Pidgin__UnseenState;

/* gtkconvwin.h */
typedef PidginWindow *			Pidgin__Conversation__Window;
typedef PidginConvPlacementFunc		Pidgin__Conversation__PlacementFunc;

/* gtkft.h */
typedef PidginXferDialog *		Pidgin__Xfer__Dialog;

/* gtkimhtml.h */
typedef GtkIMHtml *			Pidgin__IMHtml;
typedef GtkIMHtmlButtons		Pidgin__IMHtml__Buttons;
typedef GtkIMHtmlFuncs *		Pidgin__IMHtml__Funcs;
typedef GtkIMHtmlScalable *		Pidgin__IMHtml__Scalable;
typedef GtkIMHtmlSmiley *		Pidgin__IMHtml__Smiley;
typedef GtkIMHtmlOptions		Pidgin__IMHtml__Options;

/* gtkimhtmltoolbar.h */
typedef GtkIMHtmlToolbar *		Pidgin__IMHtmlToolbar;

/* gtkmenutray.h */
typedef PidginMenuTray *		Pidgin__MenuTray;

/* gtkstatusbox.h */
typedef PidginStatusBox *		Pidgin__StatusBox;
