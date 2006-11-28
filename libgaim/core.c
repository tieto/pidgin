/**
 * @file core.c Gaim Core API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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

struct GaimCore
{
	char *ui;

	void *reserved;
};

static GaimCoreUiOps *_ops  = NULL;
static GaimCore      *_core = NULL;

STATIC_PROTO_INIT

gboolean
gaim_core_init(const char *ui)
{
	GaimCoreUiOps *ops;
	GaimCore *core;

	g_return_val_if_fail(ui != NULL, FALSE);
	g_return_val_if_fail(gaim_get_core() == NULL, FALSE);

#ifdef _WIN32
	wgaim_init();
#endif

	_core = core = g_new0(GaimCore, 1);
	core->ui = g_strdup(ui);
	core->reserved = NULL;

	ops = gaim_core_get_ui_ops();

	/* The signals subsystem is important and should be first. */
	gaim_signals_init();

	gaim_signal_register(core, "quitting", gaim_marshal_VOID, NULL, 0);

	/* The prefs subsystem needs to be initialized before static protocols
	 * for protocol prefs to work. */
	gaim_prefs_init();

	gaim_debug_init();

	if (ops != NULL)
	{
		if (ops->ui_prefs_init != NULL)
			ops->ui_prefs_init();

		if (ops->debug_ui_init != NULL)
			ops->debug_ui_init();
	}

#ifdef HAVE_DBUS
	gaim_dbus_init();
#endif

	/* Initialize all static protocols. */
	static_proto_init();

	/* Since plugins get probed so early we should probably initialize their
	 * subsystem right away too.
	 */
	gaim_plugins_init();
	gaim_plugins_probe(G_MODULE_SUFFIX);

	/* Accounts use status and buddy icons, so initialize these before accounts */
	gaim_status_init();
	gaim_buddy_icons_init();

	gaim_accounts_init();
	gaim_savedstatuses_init();
	gaim_ciphers_init();
	gaim_notify_init();
	gaim_connections_init();
	gaim_conversations_init();
	gaim_blist_init();
	gaim_log_init();
	gaim_network_init();
	gaim_privacy_init();
	gaim_pounces_init();
	gaim_proxy_init();
	gaim_dnsquery_init();
	gaim_sound_init();
	gaim_ssl_init();
	gaim_stun_init();
	gaim_xfers_init();
	gaim_idle_init();

	/*
	 * Call this early on to try to auto-detect our IP address and
	 * hopefully save some time later.
	 */
	gaim_network_get_my_ip(-1);

	if (ops != NULL && ops->ui_init != NULL)
		ops->ui_init();

	return TRUE;
}

void
gaim_core_quit(void)
{
	GaimCoreUiOps *ops;
	GaimCore *core = gaim_get_core();

	g_return_if_fail(core != NULL);

	/* The self destruct sequence has been initiated */
	gaim_signal_emit(gaim_get_core(), "quitting");

	/* Transmission ends */
	gaim_connections_disconnect_all();

	/* Save .xml files, remove signals, etc. */
	gaim_idle_uninit();
	gaim_ssl_uninit();
	gaim_pounces_uninit();
	gaim_blist_uninit();
	gaim_ciphers_uninit();
	gaim_notify_uninit();
	gaim_conversations_uninit();
	gaim_connections_uninit();
	gaim_buddy_icons_uninit();
	gaim_accounts_uninit();
	gaim_savedstatuses_uninit();
	gaim_status_uninit();
	gaim_prefs_uninit();
	gaim_xfers_uninit();
	gaim_proxy_uninit();
	gaim_dnsquery_uninit();

	gaim_debug_info("main", "Unloading all plugins\n");
	gaim_plugins_destroy_all();

	ops = gaim_core_get_ui_ops();
	if (ops != NULL && ops->quit != NULL)
		ops->quit();

	/*
	 * gaim_sound_uninit() should be called as close to
	 * shutdown as possible.  This is because the call
	 * to ao_shutdown() can sometimes leave our
	 * environment variables in an unusable state, which
	 * can cause a crash when getenv is called (by gettext
	 * for example).  See the complete bug report at
	 * http://trac.xiph.org/cgi-bin/trac.cgi/ticket/701
	 *
	 * TODO: Eventually move this call higher up with the others.
	 */
	gaim_sound_uninit();

	gaim_plugins_uninit();
	gaim_signals_uninit();

#ifdef HAVE_DBUS
	gaim_dbus_uninit();
#endif

	g_free(core->ui);
	g_free(core);

#ifdef _WIN32
	wgaim_cleanup();
#endif

	_core = NULL;
}

gboolean
gaim_core_quit_cb(gpointer unused)
{
	gaim_core_quit();

	return FALSE;
}

const char *
gaim_core_get_version(void)
{
	return VERSION;
}

const char *
gaim_core_get_ui(void)
{
	GaimCore *core = gaim_get_core();

	g_return_val_if_fail(core != NULL, NULL);

	return core->ui;
}

GaimCore *
gaim_get_core(void)
{
	return _core;
}

void
gaim_core_set_ui_ops(GaimCoreUiOps *ops)
{
	_ops = ops;
}

GaimCoreUiOps *
gaim_core_get_ui_ops(void)
{
	return _ops;
}
