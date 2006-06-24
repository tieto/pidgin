#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"

#include "gntgaim.h"

/* Anything IO-related is directly copied from gtkgaim's source tree */

static GaimCoreUiOps core_ops =
{
	NULL, /*gaim_gtk_prefs_init,*/
	NULL, /*debug_init,*/
	NULL, /*gaim_gtk_ui_init,*/
	NULL, /*gaim_gtk_quit*/
};

static GaimCoreUiOps *
gnt_core_get_ui_ops()
{
	return &core_ops;
}

#define GAIM_GTK_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_GTK_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _GaimGtkIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;

} GaimGtkIOClosure;

static void gaim_gtk_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_gtk_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimGtkIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_GTK_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_GTK_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

#if 0
	gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: callback for %d, fd is %d\n",
			   closure->result, g_io_channel_unix_get_fd(source));
#endif

#ifdef _WIN32
	if(! gaim_cond) {
#if DEBUG
		gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE received GIOCondition of 0x%x, which does not"
			   " match 0x%x (READ) or 0x%x (WRITE)\n",
			   condition, GAIM_GTK_READ_COND, GAIM_GTK_WRITE_COND);
#endif /* DEBUG */

		return TRUE;
	}
#endif /* _WIN32 */

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  gaim_cond);

	return TRUE;
}

static guint gnt_input_add(gint fd, GaimInputCondition condition, GaimInputFunction function,
							   gpointer data)
{
	GaimGtkIOClosure *closure = g_new0(GaimGtkIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_GTK_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_GTK_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_gtk_io_invoke, closure, gaim_gtk_io_destroy);

#if 0
	gaim_debug(GAIM_DEBUG_MISC, "gtk_eventloop",
			   "CLOSURE: adding input watcher %d for fd %d\n",
			   closure->result, fd);
#endif

	g_io_channel_unref(channel);
	return closure->result;
}

static GaimEventLoopUiOps eventloop_ops =
{
	g_timeout_add,
	(guint (*)(guint))g_source_remove,
	gnt_input_add,
	(guint (*)(guint))g_source_remove
};

GaimEventLoopUiOps *
gnt_eventloop_get_ui_ops(void)
{
	return &eventloop_ops;
}

/* This is mostly copied from gtkgaim's source tree */
static void
init_libgaim()
{
	char *path;

	gaim_debug_set_enabled(FALSE);

	gaim_core_set_ui_ops(gnt_core_get_ui_ops());
	gaim_eventloop_set_ui_ops(gnt_eventloop_get_ui_ops());

	gaim_util_set_user_dir("/tmp/tmp/");		/* XXX: */

	path = g_build_filename(gaim_user_dir(), "plugins", NULL);
	gaim_plugins_add_search_path(path);
	g_free(path);
	gaim_plugins_add_search_path("/usr/local/lib/gaim");	/* XXX: */

	if (!gaim_core_init(GAIM_GNT_UI))
	{
		fprintf(stderr, "OOPSSS!!\n");
		abort();
	}

	/* TODO: Move blist loading into gaim_blist_init() */
	gaim_set_blist(gaim_blist_new());
	gaim_blist_load();

	/* TODO: Move prefs loading into gaim_prefs_init() */
	gaim_prefs_load();
	gaim_prefs_update_old();

	/* load plugins we had when we quit */
	gaim_plugins_load_saved("/gaim/gtk/plugins/loaded");

	/* TODO: Move pounces loading into gaim_pounces_init() */
	gaim_pounces_load();

}

int main(int argc, char **argv)
{
	/* XXX: Don't puke */
	freopen("/dev/null", "w", stderr);

	/* Initialize the libgaim stuff */
	init_libgaim();

	/* Enable the accounts and restore the status */
	gaim_accounts_restore_current_statuses();

	/* Initialize the UI */
	init_gnt_ui();

	gaim_core_quit();

	return 0;
}

