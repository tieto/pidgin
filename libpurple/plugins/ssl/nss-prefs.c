/*
 * Plugin to configure NSS
 *
 * Copyright (C) 2014, Daniel Atallah <datallah@pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"
#include "debug.h"
#include "plugin.h"
#include "version.h"

#ifdef _WIN32
# ifndef HAVE_LONG_LONG
#define HAVE_LONG_LONG
/* WINDDK_BUILD is defined because the checks around usage of
 * intrisic functions are wrong in nspr */
#define WINDDK_BUILD
# endif
#endif

#include <nspr.h>
#include <nss.h>
#include <nssb64.h>
#include <ocsp.h>
#include <pk11func.h>
#include <prio.h>
#include <secerr.h>
#include <secmod.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

/* There's a bug in some versions of this header that requires that some of
   the headers above be included first. This is true for at least libnss
   3.15.4. */
#include <certdb.h>

#define PLUGIN_ID "core-nss_prefs"

#define PREF_BASE		"/plugins/core/nss_prefs"
#define CIPHERS_PREF	PREF_BASE "/cipher_list"
#define CIPHER_TMP_ROOT PREF_BASE "/ciphers_dummy_ui"
#define CIPHER_TMP		CIPHER_TMP_ROOT "/0x%04x"
#define MIN_TLS			PREF_BASE "/min_tls"
#define MAX_TLS			PREF_BASE "/max_tls"

static PurplePlugin *handle = NULL;
static GList *tmp_prefs = NULL;
static GList *default_ciphers = NULL;
#if NSS_VMAJOR > 3 || ( NSS_VMAJOR == 3 && NSS_VMINOR >= 14 )
static SSLVersionRange *default_versions = NULL;
#endif

static gchar *get_error_text(void)
{
	PRInt32 len = PR_GetErrorTextLength();
	gchar *ret = NULL;

	if (len > 0) {
		ret = g_malloc(len + 1);
		len = PR_GetErrorText(ret);
		ret[len] = '\0';
	}

	return ret;
}

static GList *get_current_cipher_list(gboolean force_default) {
	GList *conf_ciphers = NULL;
	if (!force_default) {
		conf_ciphers = purple_prefs_get_string_list(CIPHERS_PREF);
	}

	/* If we don't have any specifically configured ciphers, use the
	 * a copy of the defaults */
	if (!conf_ciphers) {
		GList *tmp;
		for(tmp = default_ciphers; tmp; tmp = tmp->next) {
			conf_ciphers = g_list_prepend(conf_ciphers, g_strdup(tmp->data));
		}
	}

	return conf_ciphers;
}

static void
enable_ciphers(gboolean force_default) {
	const PRUint16 *cipher;
	GList *conf_ciphers, *tmp;
	SECStatus rv;

	conf_ciphers = get_current_cipher_list(force_default);

	/** First disable everything */
	for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher) {
		rv = SSL_CipherPrefSetDefault(*cipher, PR_FALSE);
		if (rv != SECSuccess) {
			gchar *error_msg = get_error_text();
			purple_debug_warning("nss-prefs",
					"Unable to disable 0x%04x: %s\n",
					*cipher, error_msg);
			g_free(error_msg);
		}
	}

	for (tmp = conf_ciphers; tmp; tmp = g_list_delete_link(tmp, tmp)) {
		guint64 parsed = g_ascii_strtoull(tmp->data, NULL, 16);

		if (parsed == 0 || parsed > PR_UINT16_MAX) {
			purple_debug_error("nss-prefs",
					"Cipher '%s' is not valid.\n",
					(const char *) tmp->data);
			g_free(tmp->data);
			continue;
		}

		rv = SSL_CipherPrefSetDefault((PRUint16) parsed, PR_TRUE);
		if (rv != SECSuccess) {
			gchar *error_msg = get_error_text();
			purple_debug_warning("nss-prefs",
					"Unable to enable 0x%04x: %s\n",
					*cipher, error_msg);
			g_free(error_msg);
		}
		purple_debug_info("nss-prefs",
				"Enabled Cipher 0x%04x.\n", (PRUint16) parsed);

		g_free(tmp->data);
	}
}

static void set_cipher_pref(const char *pref, PurplePrefType type,
		gconstpointer value, gpointer user_data) {
	const PRUint16 *cipher = user_data;
	GList *conf_ciphers, *tmp;
	gboolean enabled = GPOINTER_TO_INT(value);
	gboolean found = FALSE;

	purple_debug_info("nss-prefs",
			"%s pref for Cipher 0x%04x.\n",
			enabled ? "Adding" : "Removing", *cipher);

	conf_ciphers = get_current_cipher_list(FALSE);

	for (tmp = conf_ciphers; tmp; tmp = tmp->next) {
		guint64 parsed = g_ascii_strtoull(tmp->data, NULL, 16);
		if (parsed == 0 || parsed > PR_UINT16_MAX) {
			purple_debug_error("nss-prefs",
					"Cipher '%s' is not valid to set_cipher_pref.\n",
					(const char *) tmp->data);
		}
		if (parsed == *cipher) {
			if (!enabled) {
				g_free(tmp->data);
				conf_ciphers = g_list_delete_link(conf_ciphers, tmp);
			}
			found = TRUE;
			break;
		}
	}
	if (!found) {
		if (enabled) {
			conf_ciphers = g_list_prepend(conf_ciphers,
					g_strdup_printf("0x%04x", *cipher));
		} else {
			purple_debug_info("nss-prefs",
					"Unable to find 0x%04x to disable.\n",
					*cipher);
		}
	}

	purple_prefs_set_string_list(CIPHERS_PREF, conf_ciphers);

	for (tmp = conf_ciphers; tmp; tmp = g_list_delete_link(tmp, tmp)) {
		g_free(tmp->data);
	}

	enable_ciphers(FALSE);
}

static void set_versions(gboolean force_default) {
#if NSS_VMAJOR > 3 || ( NSS_VMAJOR == 3 && NSS_VMINOR >= 14 )
	SSLVersionRange supported, enabled;

	/* Get the ranges of supported and enabled SSL versions */
	if ((SSL_VersionRangeGetSupported(ssl_variant_stream, &supported) == SECSuccess) &&
			(SSL_VersionRangeGetDefault(ssl_variant_stream, &enabled) == SECSuccess)) {
		PRUint16 tmp;

		/* Store the defaults if this is the first time we've encountered them */
		if (!default_versions) {
			default_versions = g_new0(SSLVersionRange, 1);
			default_versions->min = enabled.min;
			default_versions->max = enabled.max;
		}

		if (force_default) {
			tmp = default_versions->min;
		} else {
			tmp = purple_prefs_get_int(MIN_TLS);
		}
		if (tmp > 0) {
			enabled.min = tmp;
		}

		if (force_default) {
			tmp = default_versions->max;
		} else {
			tmp = purple_prefs_get_int(MAX_TLS);
		}
		if (tmp > 0) {
			enabled.max = tmp;
		}

		if (SSL_VersionRangeSetDefault(ssl_variant_stream, &enabled) == SECSuccess) {
			purple_debug_info("nss-prefs", "Changed allowed TLS versions to "
					"0x%04hx through 0x%04hx\n", enabled.min, enabled.max);
		} else {
			purple_debug_error("nss-prefs", "Error setting allowed TLS versions to "
					"0x%04hx through 0x%04hx\n", enabled.min, enabled.max);
		}
	}
#else
	purple_debug_error("nss-prefs", "Unable set SSL/TLS Versions\n");
#endif /* NSS >= 3.14 */
}

static void set_version_pref(const char *pref, PurplePrefType type,
		gconstpointer value, gpointer user_data) {
	set_versions(FALSE);
}

/* This is horrible, but is the only way I can think of to tie into the
 * prefs UI. Add a bunch of temporary prefs that will be used to set
 * the prefs list. They'll get cleaned up when the plugin is unloaded*/
static void init_tmp_prefs(void) {
	GList *conf_ciphers, *tmp;
	const PRUint16 *cipher;

	if (tmp_prefs) {
		return;
	}

	conf_ciphers = get_current_cipher_list(FALSE);

	purple_prefs_add_none(CIPHER_TMP_ROOT);
	for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher) {
		gboolean found = FALSE;
		gchar *pref_name = g_strdup_printf(CIPHER_TMP, *cipher);

		tmp_prefs = g_list_prepend(tmp_prefs, pref_name);

		tmp = conf_ciphers;
		while (tmp) {
			guint64 parsed = g_ascii_strtoull(tmp->data, NULL, 16);
			if (parsed == 0 || parsed > PR_UINT16_MAX) {
				purple_debug_error("nss-prefs",
						"Cipher '%s' is not valid to init_tmp_pref.\n",
						(const char *) tmp->data);
			}
			if (parsed == *cipher) {
				found = TRUE;
				/** Remove the entry since we're done with it */
				g_free(tmp->data);
				conf_ciphers = g_list_delete_link(conf_ciphers, tmp);
				break;
			}
			tmp = tmp->next;
		}
		purple_prefs_add_bool(pref_name, found);
		purple_prefs_set_bool(pref_name, found);
		purple_prefs_connect_callback(handle, pref_name,
				set_cipher_pref, (void *) cipher);
	}
	tmp_prefs = g_list_reverse(tmp_prefs);

	for (tmp = conf_ciphers; tmp; tmp = g_list_delete_link(tmp, tmp)) {
		g_free(tmp->data);
	}

}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;
	int offset;
	GList *tmp;
#if NSS_VMAJOR > 3 || ( NSS_VMAJOR == 3 && NSS_VMINOR >= 14 )
	SSLVersionRange supported, enabled;
#endif /* NSS >= 3.14 */

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("TLS/SSL Versions"));
	purple_plugin_pref_frame_add(frame, ppref);
#if NSS_VMAJOR > 3 || ( NSS_VMAJOR == 3 && NSS_VMINOR >= 14 )
	/* Get the ranges of supported and enabled SSL versions */
	if ((SSL_VersionRangeGetSupported(ssl_variant_stream, &supported) == SECSuccess) &&
			(SSL_VersionRangeGetDefault(ssl_variant_stream, &enabled) == SECSuccess)) {
		PRUint16 tmp_version;
		PurplePluginPref *ppref_max;

		ppref = purple_plugin_pref_new_with_name_and_label(MIN_TLS,
				_("Minimum Version"));
		purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
		ppref_max = purple_plugin_pref_new_with_name_and_label(MAX_TLS,
				_("Maximum Version"));
		purple_plugin_pref_set_type(ppref_max, PURPLE_PLUGIN_PREF_CHOICE);

		for (tmp_version = supported.min; tmp_version <= supported.max; tmp_version++) {
			gchar *ver;
			switch (tmp_version) {
				case SSL_LIBRARY_VERSION_2:
					ver = g_strdup(_("SSL 2"));
					break;
				case SSL_LIBRARY_VERSION_3_0:
					ver = g_strdup(_("SSL 3"));
					break;
				case SSL_LIBRARY_VERSION_TLS_1_0:
					ver = g_strdup(_("TLS 1.0"));
					break;
				case SSL_LIBRARY_VERSION_TLS_1_1:
					ver = g_strdup(_("TLS 1.1"));
					break;
#ifdef SSL_LIBRARY_VERSION_TLS_1_2
				case SSL_LIBRARY_VERSION_TLS_1_2:
					ver = g_strdup(_("TLS 1.2"));
					break;
#endif
#ifdef SSL_LIBRARY_VERSION_TLS_1_3
				case SSL_LIBRARY_VERSION_TLS_1_3:
					ver = g_strdup(_("TLS 1.3"));
					break;
#endif
				default:
					ver = g_strdup_printf("0x%04hx", tmp_version);
			}
			purple_plugin_pref_add_choice(ppref, ver, GINT_TO_POINTER((gint) tmp_version));
			purple_plugin_pref_add_choice(ppref_max, ver, GINT_TO_POINTER((gint) tmp_version));
			g_free(ver);
		}
		purple_plugin_pref_frame_add(frame, ppref);
		purple_plugin_pref_frame_add(frame, ppref_max);
	}
#else
	/* TODO: look into how to do this for older versions? */

	ppref = purple_plugin_pref_new_with_label(_("Not Supported for NSS < 3.14"));
	purple_plugin_pref_frame_add(frame, ppref);

#endif /* NSS >= 3.14 */

	ppref = purple_plugin_pref_new_with_label(_("Ciphers"));
	purple_plugin_pref_frame_add(frame, ppref);

	init_tmp_prefs();

	offset = strlen(CIPHER_TMP_ROOT) + 1;
	for (tmp = tmp_prefs; tmp; tmp = tmp->next) {
		guint64 parsed = g_ascii_strtoull( (char *) tmp->data + offset,
				NULL, 16);
		PRUint16 cipher;
		SECStatus rv;
		gchar **split;
		gchar *escaped_name;
		SSLCipherSuiteInfo info;

		if (parsed == 0 || parsed > PR_UINT16_MAX) {
			purple_debug_error("nss-prefs",
					"Cipher '%s' is not valid to build pref frame.\n",
					(const char *) tmp->data + offset);
			continue;
		}

		cipher = (PRUint16) parsed;

		rv = SSL_GetCipherSuiteInfo(cipher, &info, (int)(sizeof info));
		if (rv != SECSuccess) {
			gchar *error_msg = get_error_text();
			purple_debug_warning("nss-prefs",
					"SSL_GetCipherSuiteInfo didn't like value 0x%04x: %s\n",
					cipher, error_msg);
			g_free(error_msg);
			continue;
		}
		escaped_name = g_strdup_printf("%s (0x%04x)",
				info.cipherSuiteName, cipher);
		/** Escape the _ for the label */
		split = g_strsplit(escaped_name, "_", -1);
		g_free(escaped_name);
		escaped_name = g_strjoinv("__", split);
		g_strfreev(split);
		ppref = purple_plugin_pref_new_with_name_and_label(
				(const char *) tmp->data, escaped_name);
		g_free(escaped_name);
		purple_plugin_pref_frame_add(frame, ppref);
	}

	return frame;
}

static gboolean
plugin_load(PurplePlugin *plugin) {
	const PRUint16 *cipher;

	handle = plugin;

	tmp_prefs = NULL;
	default_ciphers = NULL;

	for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher) {
		PRBool enabled;
		SECStatus rv = SSL_CipherPrefGetDefault(*cipher, &enabled);
		if (rv == SECSuccess && enabled) {
			default_ciphers = g_list_prepend(default_ciphers,
					g_strdup_printf("0x%04x", *cipher));
		}
	}

	enable_ciphers(FALSE);
	set_versions(FALSE);
	purple_prefs_connect_callback(handle, MIN_TLS,
				set_version_pref, GINT_TO_POINTER(FALSE));
	purple_prefs_connect_callback(handle, MAX_TLS,
				set_version_pref, GINT_TO_POINTER(TRUE));

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin) {

	/* Remove the temporary prefs */
	if (tmp_prefs) {
		purple_prefs_remove(CIPHER_TMP_ROOT);
		while (tmp_prefs) {
			g_free(tmp_prefs->data);
			tmp_prefs = g_list_delete_link(tmp_prefs, tmp_prefs);
		}
	}

	/* Restore the original ciphers */
	enable_ciphers(TRUE);
	while (default_ciphers) {
		g_free(default_ciphers->data);
		default_ciphers = g_list_delete_link(default_ciphers,
				default_ciphers);
	}

	set_versions(TRUE);
#if NSS_VMAJOR > 3 || ( NSS_VMAJOR == 3 && NSS_VMINOR >= 14 )
	g_free(default_versions);
	default_versions = NULL;
#endif

	return TRUE;
}


static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL, /* frame (Reserved) */
	/* Padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,				/**< type           */
	NULL,						/**< ui_requirement */
	0,						/**< flags          */
	NULL,						/**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,			/**< priority       */

	PLUGIN_ID,					/**< id             */
	N_("NSS Preferences"),				/**< name           */
	DISPLAY_VERSION,				/**< version        */
							/**  summary        */
	N_("Configure Ciphers and other Settings for "
	   "the NSS SSL/TLS Plugin"),
							/**  description    */
	N_("Configure Ciphers and other Settings for "
	   "the NSS SSL/TLS Plugin"),
	"Daniel Atallah <datallah@pidgin.im>",		/**< author         */
	PURPLE_WEBSITE,					/**< homepage       */

	plugin_load,					/**< load           */
	plugin_unload,					/**< unload         */
	NULL,						/**< destroy        */

	NULL,						/**< ui_info        */
	NULL,						/**< extra_info     */
	&prefs_info,					/**< prefs_info     */
	NULL,
	/* Padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin) {
	info.dependencies = g_list_prepend(info.dependencies, "ssl-nss");

	purple_prefs_add_none(PREF_BASE);
	purple_prefs_add_string_list(CIPHERS_PREF, NULL);
	purple_prefs_add_int(MIN_TLS, 0);
	purple_prefs_add_int(MAX_TLS, 0);
}

PURPLE_INIT_PLUGIN(nss_prefs, init_plugin, info)
