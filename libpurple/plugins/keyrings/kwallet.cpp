/**
 * @file kwallet.cpp KWallet password storage
 * @ingroup plugins
 */

/* purple
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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#include <QQueue>
#include <QCoreApplication>
#include <kwallet.h>

#define KWALLET_NAME        N_("KWallet")
#define KWALLET_DESCRIPTION N_("This plugin will store passwords in KWallet.")
#define KWALLET_AUTHOR      "QuLogic (qulogic[at]pidgin.im)"
#define KWALLET_ID          "keyring-kwallet"

#define KWALLET_WALLET_NAME KWallet::Wallet::NetworkWallet()
#define KWALLET_APP_NAME "Libpurple"
#define KWALLET_FOLDER_NAME "libpurple"

PurpleKeyring *keyring_handler = NULL;
QCoreApplication *qCoreApp = NULL;

namespace KWalletPlugin {

class request
{
	public:
		virtual ~request();
		virtual void detailedAbort(enum PurpleKeyringError error) = 0;
		void abort();
		virtual void execute(KWallet::Wallet *wallet) = 0;

	protected:
		gpointer data;
		PurpleAccount *account;
		QString password;
		bool noPassword;
};

class engine : private QObject, private QQueue<request*>
{
	Q_OBJECT

	public:
		engine();
		~engine();
		void queue(request *req);
		static engine *instance();
		static void closeInstance();

	private slots:
		void walletOpened(bool opened);
		void walletClosed();

	private:
		static engine *pinstance;

		bool connected;
		bool failed;
		bool closing;
		bool externallyClosed;
		bool busy;
		bool closeAfterBusy;

		KWallet::Wallet *wallet;

		void reopenWallet();
		void executeRequests();
};

class save_request : public request
{
	public:
		save_request(PurpleAccount *account, const char *password,
			PurpleKeyringSaveCallback cb, void *data);
		void detailedAbort(enum PurpleKeyringError error);
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringSaveCallback callback;
};

class read_request : public request
{
	public:
		read_request(PurpleAccount *account,
			PurpleKeyringReadCallback cb, void *data);
		void detailedAbort(enum PurpleKeyringError error);
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringReadCallback callback;
};

}

static gboolean
kwallet_is_enabled(void)
{
	return KWallet::Wallet::isEnabled() ? TRUE : FALSE;
}

KWalletPlugin::engine *KWalletPlugin::engine::pinstance = NULL;

KWalletPlugin::request::~request()
{
}

void
KWalletPlugin::request::abort()
{
	detailedAbort(PURPLE_KEYRING_ERROR_NOCHANNEL);
}

KWalletPlugin::engine::engine()
{
	connected = false;
	failed = false;
	closing = false;
	externallyClosed = false;
	wallet = NULL;
	busy = false;
	closeAfterBusy = false;

	reopenWallet();
}

void
KWalletPlugin::engine::reopenWallet()
{
	if (closing) {
		purple_debug_error("keyring-kwallet",
			"wallet is closing right now\n");
		failed = true;
		return;
	}

	connected = false;
	failed = false;
	externallyClosed = false;

	wallet = KWallet::Wallet::openWallet(KWALLET_WALLET_NAME, 0,
		KWallet::Wallet::Asynchronous);
	if (wallet == NULL) {
		failed = true;
		purple_debug_error("keyring-kwallet",
			"failed opening a wallet\n");
		return;
	}

	failed |= !connect(wallet, SIGNAL(walletClosed()),
		SLOT(walletClosed()));
	failed |= !connect(wallet, SIGNAL(walletOpened(bool)),
		SLOT(walletOpened(bool)));
	if (failed) {
		purple_debug_error("keyring-kwallet",
			"failed connecting to wallet signal\n");
	}
}

KWalletPlugin::engine::~engine()
{
	closing = true;
	int abortedCount = 0;

	while (!isEmpty()) {
		request *req = dequeue();
		req->abort();
		delete req;
		abortedCount++;
	}

	if (abortedCount > 0) {
		purple_debug_info("keyring-kwallet", "aborted requests: %d\n",
			abortedCount);
	}

	delete wallet;

	if (pinstance == this)
		pinstance = NULL;
}

KWalletPlugin::engine *
KWalletPlugin::engine::instance()
{
	if (pinstance == NULL)
		pinstance = new engine;
	return pinstance;
}

void
KWalletPlugin::engine::closeInstance()
{
	if (pinstance == NULL)
		return;
	if (pinstance->closing)
		return;
	if (pinstance->busy) {
		purple_debug_misc("keyring-kwallet",
			"current instance is busy, will be freed later\n");
		pinstance->closeAfterBusy = true;
	} else
		delete pinstance;
	pinstance = NULL;
}

void
KWalletPlugin::engine::walletOpened(bool opened)
{
	connected = opened;

	if (!opened) {
		purple_debug_warning("keyring-kwallet",
			"failed to open a wallet\n");
		delete this;
		return;
	}

	if (!wallet->hasFolder(KWALLET_FOLDER_NAME)) {
		if (!wallet->createFolder(KWALLET_FOLDER_NAME)) {
			purple_debug_error("keyring-kwallet",
				"couldn't create \"" KWALLET_FOLDER_NAME
				"\" folder in wallet\n");
			failed = true;
		}
	}
	if (!failed)
		wallet->setFolder(KWALLET_FOLDER_NAME);

	executeRequests();
}

void
KWalletPlugin::engine::walletClosed()
{
	if (!closing) {
		purple_debug_info("keyring-kwallet",
			"wallet was externally closed\n");
		externallyClosed = true;
		delete wallet;
		wallet = NULL;
	}
}

void
KWalletPlugin::engine::queue(request *req)
{
	enqueue(req);
	executeRequests();
}

void
KWalletPlugin::engine::executeRequests()
{
	if (closing || busy)
		return;
	busy = true;
	if (externallyClosed) {
		reopenWallet();
	} else if (connected || failed) {
		while (!isEmpty()) {
			request *req = dequeue();
			if (connected)
				req->execute(wallet);
			else
				req->abort();
			delete req;
		}
	} else {
		purple_debug_misc("keyring-kwallet", "not yet connected\n");
	}
	busy = false;
	if (closeAfterBusy) {
		purple_debug_misc("keyring-kwallet",
			"instance freed after being busy\n");
		delete this;
	}
}

KWalletPlugin::save_request::save_request(PurpleAccount *acc, const char *pw,
	PurpleKeyringSaveCallback cb, void *userdata)
{
	account = acc;
	data = userdata;
	callback = cb;
	password = QString(pw);
	noPassword = (pw == NULL);
}

KWalletPlugin::read_request::read_request(PurpleAccount *acc,
	PurpleKeyringReadCallback cb, void *userdata)
{
	account = acc;
	data = userdata;
	callback = cb;
	password = QString();
}

void
KWalletPlugin::save_request::detailedAbort(enum PurpleKeyringError error)
{
	GError *gerror;
	if (callback == NULL)
		return;

	gerror = g_error_new(PURPLE_KEYRING_ERROR, error,
		"Failed to save password");
	callback(account, gerror, data);
	g_error_free(gerror);
}

void
KWalletPlugin::read_request::detailedAbort(enum PurpleKeyringError error)
{
	GError *gerror;
	if (callback == NULL)
		return;

	gerror = g_error_new(PURPLE_KEYRING_ERROR, error,
		"Failed to read password");
	callback(account, NULL, gerror, data);
	g_error_free(gerror);
}

static QString
kwallet_account_key(PurpleAccount *account)
{
	return QString(purple_account_get_protocol_id(account)) + ":" +
		purple_account_get_username(account);
}

void
KWalletPlugin::read_request::execute(KWallet::Wallet *wallet)
{
	int result;

	g_return_if_fail(wallet != NULL);

	result = wallet->readPassword(kwallet_account_key(account), password);

	if (result != 0) {
		purple_debug_warning("keyring-kwallet",
			"failed to read password, result was %d\n", result);
		abort();
	}
	else if (callback != NULL) {
		purple_debug_misc("keyring-kwallet",
			"Got password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		callback(account, password.toUtf8().constData(), NULL, data);
	}
}

void
KWalletPlugin::save_request::execute(KWallet::Wallet *wallet)
{
	int result;

	g_return_if_fail(wallet != NULL);

	if (noPassword)
		result = wallet->removeEntry(kwallet_account_key(account));
	else {
		result = wallet->writePassword(kwallet_account_key(account),
			password);
	}

	if (result != 0) {
		purple_debug_warning("keyring-kwallet",
			"failed to write password, result was %d\n", result);
		abort();
	} else if (callback != NULL) {
		purple_debug_misc("keyring-kwallet",
			"Password %s for account %s (%s).\n",
			(noPassword ? "removed" : "saved"),
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
		callback(account, NULL, data);
	}
}

extern "C"
{

static void
kwallet_read(PurpleAccount *account, PurpleKeyringReadCallback cb,
	gpointer data)
{
	KWalletPlugin::read_request *req =
		new KWalletPlugin::read_request(account, cb, data);

	if (KWallet::Wallet::keyDoesNotExist(KWALLET_WALLET_NAME,
		KWALLET_FOLDER_NAME, kwallet_account_key(account))) {
		req->detailedAbort(PURPLE_KEYRING_ERROR_NOPASSWD);
		delete req;
	}
	else
		KWalletPlugin::engine::instance()->queue(req);
}

static void
kwallet_save(PurpleAccount *account, const char *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	if (password == NULL && KWallet::Wallet::keyDoesNotExist(
		KWALLET_WALLET_NAME, KWALLET_FOLDER_NAME,
		kwallet_account_key(account))) {
		if (cb != NULL)
			cb(account, NULL, data);
	}
	else
		KWalletPlugin::engine::instance()->queue(
			new KWalletPlugin::save_request(account, password, cb,
			data));
}

static void
kwallet_close(GError **error)
{
	KWalletPlugin::engine::closeInstance();
}

static gboolean
kwallet_import(PurpleAccount *account, const char *mode, const char *data,
	GError **error)
{
	return TRUE;
}

static gboolean
kwallet_export(PurpleAccount *account, const char **mode, char **data,
	GError **error, GDestroyNotify *destroy)
{
	*mode = NULL;
	*data = NULL;
	*destroy = NULL;

	return TRUE;
}

static gboolean
kwallet_load(PurplePlugin *plugin)
{
	if (!qCoreApp) {
		int argc = 0;
		qCoreApp = new QCoreApplication(argc, NULL);
		qCoreApp->setApplicationName(KWALLET_APP_NAME);
	}

	if (!kwallet_is_enabled()) {
		purple_debug_info("keyring-kwallet",
			"KWallet service is disabled\n");
		return FALSE;
	}

	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, KWALLET_NAME);
	purple_keyring_set_id(keyring_handler, KWALLET_ID);
	purple_keyring_set_read_password(keyring_handler, kwallet_read);
	purple_keyring_set_save_password(keyring_handler, kwallet_save);
	purple_keyring_set_close_keyring(keyring_handler, kwallet_close);
	purple_keyring_set_import_password(keyring_handler, kwallet_import);
	purple_keyring_set_export_password(keyring_handler, kwallet_export);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
kwallet_unload(PurplePlugin *plugin)
{
	if (purple_keyring_get_inuse() == keyring_handler) {
		purple_debug_warning("keyring-kwallet",
			"keyring in use, cannot unload\n");
		return FALSE;
	}

	kwallet_close(NULL);

	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	if (qCoreApp) {
		delete qCoreApp;
		qCoreApp = NULL;
	}

	return TRUE;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,			/* magic */
	PURPLE_MAJOR_VERSION,			/* major_version */
	PURPLE_MINOR_VERSION,			/* minor_version */
	PURPLE_PLUGIN_STANDARD,			/* type */
	NULL,					/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,		/* flags */
	NULL,					/* dependencies */
	PURPLE_PRIORITY_DEFAULT,		/* priority */
	(char*)KWALLET_ID,			/* id */
	(char*)KWALLET_NAME,			/* name */
	(char*)DISPLAY_VERSION,			/* version */
	(char*)"KWallet Keyring Plugin",	/* summary */
	(char*)KWALLET_DESCRIPTION,		/* description */
	(char*)KWALLET_AUTHOR,			/* author */
	(char*)PURPLE_WEBSITE,			/* homepage */
	kwallet_load,				/* load */
	kwallet_unload,				/* unload */
	NULL,					/* destroy */
	NULL,					/* ui_info */
	NULL,					/* extra_info */
	NULL,					/* prefs_info */
	NULL,					/* actions */
	NULL, NULL, NULL, NULL			/* padding */
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(kwallet_keyring, init_plugin, plugininfo)

} /* extern "C" */

#include "kwallet.moc"
