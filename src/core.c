/**
 * @file core.c Gaim Core API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "connection.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "plugin.h"
#include "pounce.h"
#include "prefs.h"
#include "proxy.h"
#include "sound.h"

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

	_core = core = g_new0(GaimCore, 1);
	core->ui = g_strdup(ui);
	core->reserved = NULL;

	ops = gaim_get_core_ui_ops();

	/* Initialize all static protocols. */
	static_proto_init();

	printf("gaim_prefs_init\n");
	gaim_prefs_init();

	if (ops != NULL) {
		if (ops->ui_prefs_init != NULL)
			ops->ui_prefs_init();

		if (ops->debug_ui_init != NULL)
			ops->debug_ui_init();
	}

	printf("conversation_init\n");
	gaim_conversation_init();
	gaim_proxy_init();
	gaim_sound_init();
	gaim_pounces_init();

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

	ops = gaim_get_core_ui_ops();

	if (ops != NULL && ops->quit != NULL)
		ops->quit();

	/* The self destruct sequence has been initiated */
	gaim_event_broadcast(event_quit);

	/* Transmission ends */
	gaim_connections_disconnect_all();

	/* Record what we have before we blow it away... */
	gaim_prefs_sync();
	gaim_accounts_sync();

	gaim_debug(GAIM_DEBUG_INFO, "main", "Unloading all plugins\n");
	gaim_plugins_destroy_all();

	if (core->ui != NULL) {
		g_free(core->ui);
		core->ui = NULL;
	}

	g_free(core);

	_core = NULL;
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
gaim_set_core_ui_ops(GaimCoreUiOps *ops)
{
	_ops = ops;
}

GaimCoreUiOps *
gaim_get_core_ui_ops(void)
{
	return _ops;
}
