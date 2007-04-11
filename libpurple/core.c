/**
 * @file core.c Purple Core API
 * @ingroup core
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */
#include "internal.h"
#include "cipher.h"
#include "connection.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "dnsquery.h"
#include "ft.h"
#include "idle.h"
#include "network.h"
#include "notify.h"
#include "plugin.h"
#include "pounce.h"
#include "prefs.h"
#include "privacy.h"
#include "proxy.h"
#include "savedstatuses.h"
#include "signals.h"
#include "sound.h"
#include "sslconn.h"
#include "status.h"
#include "stun.h"

#ifdef HAVE_DBUS
#  include "dbus-server.h"
#endif

struct PurpleCore
{
	char *ui;

	void *reserved;
};

static PurpleCoreUiOps *_ops  = NULL;
static PurpleCore      *_core = NULL;

STATIC_PROTO_INIT

gboolean
purple_core_init(const char *ui)
{
	PurpleCoreUiOps *ops;
	PurpleCore *core;

	g_return_val_if_fail(ui != NULL, FALSE);
	g_return_val_if_fail(purple_get_core() == NULL, FALSE);

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
#endif
#ifdef _WIN32
	wpurple_init();
#endif

	_core = core = g_new0(PurpleCore, 1);
	core->ui = g_strdup(ui);
	core->reserved = NULL;

	ops = purple_core_get_ui_ops();

	/* The signals subsystem is important and should be first. */
	purple_signals_init();

	purple_signal_register(core, "uri-handler",
		purple_marshal_BOOLEAN__POINTER_POINTER_POINTER,
		purple_value_new(PURPLE_TYPE_BOOLEAN), 3,
		purple_value_new(PURPLE_TYPE_STRING), /* Protocol */
		purple_value_new(PURPLE_TYPE_STRING), /* Command */
		purple_value_new(PURPLE_TYPE_BOXED, "GHashTable *")); /* Parameters */

	purple_signal_register(core, "quitting", purple_marshal_VOID, NULL, 0);

	/* The prefs subsystem needs to be initialized before static protocols
	 * for protocol prefs to work. */
	purple_prefs_init();

	purple_debug_init();

	if (ops != NULL)
	{
		if (ops->ui_prefs_init != NULL)
			ops->ui_prefs_init();

		if (ops->debug_ui_init != NULL)
			ops->debug_ui_init();
	}

#ifdef HAVE_DBUS
	purple_dbus_init();
#endif

	/* Initialize all static protocols. */
	static_proto_init();

	/* Since plugins get probed so early we should probably initialize their
	 * subsystem right away too.
	 */
	purple_plugins_init();
	purple_plugins_probe(G_MODULE_SUFFIX);

	/* Accounts use status and buddy icons, so initialize these before accounts */
	purple_status_init();
	purple_buddy_icons_init();

	purple_accounts_init();
	purple_savedstatuses_init();
	purple_ciphers_init();
	purple_notify_init();
	purple_connections_init();
	purple_conversations_init();
	purple_blist_init();
	purple_log_init();
	purple_network_init();
	purple_privacy_init();
	purple_pounces_init();
	purple_proxy_init();
	purple_dnsquery_init();
	purple_sound_init();
	purple_ssl_init();
	purple_stun_init();
	purple_xfers_init();
	purple_idle_init();

	/*
	 * Call this early on to try to auto-detect our IP address and
	 * hopefully save some time later.
	 */
	purple_network_get_my_ip(-1);

	if (ops != NULL && ops->ui_init != NULL)
		ops->ui_init();

	return TRUE;
}

void
purple_core_quit(void)
{
	PurpleCoreUiOps *ops;
	PurpleCore *core = purple_get_core();

	g_return_if_fail(core != NULL);

	/* The self destruct sequence has been initiated */
	purple_signal_emit(purple_get_core(), "quitting");

	/* Transmission ends */
	purple_connections_disconnect_all();

	/* Save .xml files, remove signals, etc. */
	purple_idle_uninit();
	purple_ssl_uninit();
	purple_pounces_uninit();
	purple_blist_uninit();
	purple_ciphers_uninit();
	purple_notify_uninit();
	purple_conversations_uninit();
	purple_connections_uninit();
	purple_buddy_icons_uninit();
	purple_accounts_uninit();
	purple_savedstatuses_uninit();
	purple_status_uninit();
	purple_prefs_uninit();
	purple_xfers_uninit();
	purple_proxy_uninit();
	purple_dnsquery_uninit();

	purple_debug_info("main", "Unloading all plugins\n");
	purple_plugins_destroy_all();

	ops = purple_core_get_ui_ops();
	if (ops != NULL && ops->quit != NULL)
		ops->quit();

	/*
	 * purple_sound_uninit() should be called as close to
	 * shutdown as possible.  This is because the call
	 * to ao_shutdown() can sometimes leave our
	 * environment variables in an unusable state, which
	 * can cause a crash when getenv is called (by gettext
	 * for example).  See the complete bug report at
	 * http://trac.xiph.org/cgi-bin/trac.cgi/ticket/701
	 *
	 * TODO: Eventually move this call higher up with the others.
	 */
	purple_sound_uninit();

	purple_plugins_uninit();
	purple_signals_uninit();

#ifdef HAVE_DBUS
	purple_dbus_uninit();
#endif

	g_free(core->ui);
	g_free(core);

#ifdef _WIN32
	wpurple_cleanup();
#endif

	_core = NULL;
}

gboolean
purple_core_quit_cb(gpointer unused)
{
	purple_core_quit();

	return FALSE;
}

const char *
purple_core_get_version(void)
{
	return VERSION;
}

const char *
purple_core_get_ui(void)
{
	PurpleCore *core = purple_get_core();

	g_return_val_if_fail(core != NULL, NULL);

	return core->ui;
}

PurpleCore *
purple_get_core(void)
{
	return _core;
}

void
purple_core_set_ui_ops(PurpleCoreUiOps *ops)
{
	_ops = ops;
}

PurpleCoreUiOps *
purple_core_get_ui_ops(void)
{
	return _ops;
}
