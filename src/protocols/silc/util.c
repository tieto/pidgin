/*

  silcgaim_util.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2004 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcincludes.h"
#include "silcclient.h"
#include "silcgaim.h"

/**************************** Utility Routines *******************************/

static char str[256], str2[256];

const char *silcgaim_silcdir(void)
{
	const char *hd = gaim_home_dir();
	memset(str, 0, sizeof(str));
	g_snprintf(str, sizeof(str) - 1, "%s" G_DIR_SEPARATOR_S ".silc", hd ? hd : "/tmp");
	return (const char *)str;
}

const char *silcgaim_session_file(const char *account)
{
	memset(str2, 0, sizeof(str2));
	g_snprintf(str2, sizeof(str2) - 1, "%s" G_DIR_SEPARATOR_S "%s_session",
		   silcgaim_silcdir(), account);
	return (const char *)str2;
}

gboolean silcgaim_ip_is_private(const char *ip)
{
	if (silc_net_is_ip4(ip)) {
		if (!strncmp(ip, "10.", 3)) {
			return TRUE;
		} else if (!strncmp(ip, "172.", 4) && strlen(ip) > 6) {
			char tmp[3];
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, ip + 4, 2);
			int s = atoi(tmp);
			if (s >= 16 && s <= 31)
				return TRUE;
		} else if (!strncmp(ip, "192.168.", 8)) {
			return TRUE;
		}
	}

	return FALSE;
}

/* This checks stats for various SILC files and directories. First it
   checks if ~/.silc directory exist and is owned by the correct user. If
   it doesn't exist, it will create the directory. After that it checks if
   user's Public and Private key files exists and creates them if needed. */

gboolean silcgaim_check_silc_dir(GaimConnection *gc)
{
	char filename[256], file_public_key[256], file_private_key[256];
	char servfilename[256], clientfilename[256], friendsfilename[256];
	struct stat st;
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (!pw) {
		fprintf(stderr, "silc: %s\n", strerror(errno));
		return FALSE;
	}

	g_snprintf(filename, sizeof(filename) - 1, "%s" G_DIR_SEPARATOR_S, silcgaim_silcdir());
	g_snprintf(servfilename, sizeof(servfilename) - 1, "%s" G_DIR_SEPARATOR_S "serverkeys",
		   silcgaim_silcdir());
	g_snprintf(clientfilename, sizeof(clientfilename) - 1, "%s" G_DIR_SEPARATOR_S "clientkeys",
		   silcgaim_silcdir());
	g_snprintf(friendsfilename, sizeof(friendsfilename) - 1, "%s" G_DIR_SEPARATOR_S "friends",
		   silcgaim_silcdir());

	/*
	 * Check ~/.silc directory
	 */
	if ((stat(filename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((mkdir(filename, 0755)) == -1) {
					fprintf(stderr, "Couldn't create `%s' directory\n", filename);
					return FALSE;
				}
			} else {
				fprintf(stderr, "Couldn't create `%s' directory due to a wrong uid!\n",
					filename);
				return FALSE;
			}
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	} else {
		/* Check the owner of the dir */
		if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
			fprintf(stderr, "You don't seem to own `%s' directory\n",
				filename);
			return FALSE;
		}
	}

	/*
	 * Check ~./silc/serverkeys directory
	 */
	if ((stat(servfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((mkdir(servfilename, 0755)) == -1) {
					fprintf(stderr, "Couldn't create `%s' directory\n", servfilename);
					return FALSE;
				}
			} else {
				fprintf(stderr, "Couldn't create `%s' directory due to a wrong uid!\n",
					servfilename);
				return FALSE;
			}
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check ~./silc/clientkeys directory
	 */
	if ((stat(clientfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((mkdir(clientfilename, 0755)) == -1) {
					fprintf(stderr, "Couldn't create `%s' directory\n", clientfilename);
					return FALSE;
				}
			} else {
				fprintf(stderr, "Couldn't create `%s' directory due to a wrong uid!\n",
					clientfilename);
				return FALSE;
			}
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check ~./silc/friends directory
	 */
	if ((stat(friendsfilename, &st)) == -1) {
		/* If dir doesn't exist */
		if (errno == ENOENT) {
			if (pw->pw_uid == geteuid()) {
				if ((mkdir(friendsfilename, 0755)) == -1) {
					fprintf(stderr, "Couldn't create `%s' directory\n", friendsfilename);
					return FALSE;
				}
			} else {
				fprintf(stderr, "Couldn't create `%s' directory due to a wrong uid!\n",
					friendsfilename);
				return FALSE;
			}
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	}

	/*
	 * Check Public and Private keys
	 */
	g_snprintf(file_public_key, sizeof(file_public_key) - 1, "%s",
		   gaim_prefs_get_string("/plugins/prpl/silc/pubkey"));
	g_snprintf(file_private_key, sizeof(file_public_key) - 1, "%s",
		   gaim_prefs_get_string("/plugins/prpl/silc/privkey"));

	if ((stat(file_public_key, &st)) == -1) {
		/* If file doesn't exist */
		if (errno == ENOENT) {
			gaim_connection_update_progress(gc, _("Creating SILC key pair..."), 1, 5);
			silc_create_key_pair(SILCGAIM_DEF_PKCS,
					     SILCGAIM_DEF_PKCS_LEN,
					     file_public_key, file_private_key, NULL,
					     "", NULL, NULL, NULL, FALSE);
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	}

	/* Check the owner of the public key */
	if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
		fprintf(stderr, "You don't seem to own your public key!?\n");
		return FALSE;
	}

	if ((stat(file_private_key, &st)) == -1) {
		/* If file doesn't exist */
		if (errno == ENOENT) {
			gaim_connection_update_progress(gc, _("Creating SILC key pair..."), 1, 5);
			silc_create_key_pair(SILCGAIM_DEF_PKCS,
					     SILCGAIM_DEF_PKCS_LEN,
					     file_public_key, file_private_key, NULL,
					     "", NULL, NULL, NULL, FALSE);
		} else {
			fprintf(stderr, "%s\n", strerror(errno));
			return FALSE;
		}
	}

	/* Check the owner of the private key */
	if (st.st_uid != 0 && st.st_uid != pw->pw_uid) {
		fprintf(stderr, "You don't seem to own your private key!?\n");
		return FALSE;
	}

	/* Check the permissions for the private key */
	if ((st.st_mode & 0777) != 0600) {
		fprintf(stderr, "Wrong permissions in your private key file `%s'!\n"
			"Trying to change them ... ", file_private_key);
		if ((chmod(file_private_key, 0600)) == -1) {
			fprintf(stderr,
				"Failed to change permissions for private key file!\n"
				"Permissions for your private key file must be 0600.\n");
			return FALSE;
		}
		fprintf(stderr, "Done.\n\n");
	}

	return TRUE;
}

void silcgaim_show_public_key(SilcGaim sg,
			      const char *name, SilcPublicKey public_key,
			      GCallback callback, void *context)
{
	SilcPublicKeyIdentifier ident;
	SilcPKCS pkcs;
	char *fingerprint, *babbleprint;
	unsigned char *pk;
	SilcUInt32 pk_len, key_len = 0;
	GString *s;
	char *buf;

	ident = silc_pkcs_decode_identifier(public_key->identifier);
	if (!ident)
		return;

	pk = silc_pkcs_public_key_encode(public_key, &pk_len);
	fingerprint = silc_hash_fingerprint(NULL, pk, pk_len);
	babbleprint = silc_hash_babbleprint(NULL, pk, pk_len);

	if (silc_pkcs_alloc(public_key->name, &pkcs)) {
		key_len = silc_pkcs_public_key_set(pkcs, public_key);
		silc_pkcs_free(pkcs);
	}

	s = g_string_new("");
	if (ident->realname)
		g_string_append_printf(s, "Real Name: \t%s\n", ident->realname);
	if (ident->username)
		g_string_append_printf(s, "User Name: \t%s\n", ident->username);
	if (ident->email)
		g_string_append_printf(s, "EMail: \t\t%s\n", ident->email);
	if (ident->host)
		g_string_append_printf(s, "Host Name: \t%s\n", ident->host);
	if (ident->org)
		g_string_append_printf(s, "Organization: \t%s\n", ident->org);
	if (ident->country)
		g_string_append_printf(s, "Country: \t%s\n", ident->country);
	g_string_append_printf(s, "Algorithm: \t\t%s\n", public_key->name);
	g_string_append_printf(s, "Key Length: \t%d bits\n", (int)key_len);
	g_string_append_printf(s, "\n");
	g_string_append_printf(s, "Public Key Fingerprint:\n%s\n\n", fingerprint);
	g_string_append_printf(s, "Public Key Babbleprint:\n%s", babbleprint);

	buf = g_string_free(s, FALSE);

	gaim_request_action(NULL, _("Public Key Information"),
			    _("Public Key Information"),
			    buf, 0, context, 1,
			    _("Close"), callback);

	g_free(buf);
	silc_free(fingerprint);
	silc_free(babbleprint);
	silc_free(pk);
	silc_pkcs_free_identifier(ident);
}

SilcAttributePayload
silcgaim_get_attr(SilcDList attrs, SilcAttribute attribute)
{
	SilcAttributePayload attr = NULL;

	if (!attrs)
		return NULL;

	silc_dlist_start(attrs);
	while ((attr = silc_dlist_get(attrs)) != SILC_LIST_END)
		if (attribute == silc_attribute_get_attribute(attr))
			break;

	return attr;
}

void silcgaim_get_umode_string(SilcUInt32 mode, char *buf,
			       SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if ((mode & SILC_UMODE_SERVER_OPERATOR) ||
	    (mode & SILC_UMODE_ROUTER_OPERATOR)) {
		strcat(buf, (mode & SILC_UMODE_SERVER_OPERATOR) ?
		       "[server operator] " :
		       (mode & SILC_UMODE_ROUTER_OPERATOR) ?
		       "[SILC operator] " : "[unknown mode] ");
	}
	if (mode & SILC_UMODE_GONE)
		strcat(buf, "[away] ");
	if (mode & SILC_UMODE_INDISPOSED)
		strcat(buf, "[indisposed] ");
	if (mode & SILC_UMODE_BUSY)
		strcat(buf, "[busy] ");
	if (mode & SILC_UMODE_PAGE)
		strcat(buf, "[wake me up] ");
	if (mode & SILC_UMODE_HYPER)
		strcat(buf, "[hyperactive] ");
	if (mode & SILC_UMODE_ROBOT)
		strcat(buf, "[robot] ");
	if (mode & SILC_UMODE_ANONYMOUS)
		strcat(buf, "[anonymous] ");
	if (mode & SILC_UMODE_BLOCK_PRIVMSG)
		strcat(buf, "[blocks private messages] ");
	if (mode & SILC_UMODE_DETACHED)
		strcat(buf, "[detached] ");
	if (mode & SILC_UMODE_REJECT_WATCHING)
		strcat(buf, "[rejects watching] ");
	if (mode & SILC_UMODE_BLOCK_INVITE)
		strcat(buf, "[blocks invites] ");
}

void silcgaim_get_chmode_string(SilcUInt32 mode, char *buf,
				SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if (mode & SILC_CHANNEL_MODE_FOUNDER_AUTH)
		strcat(buf, "[permanent] ");
	if (mode & SILC_CHANNEL_MODE_PRIVATE)
		strcat(buf, "[private] ");
	if (mode & SILC_CHANNEL_MODE_SECRET)
		strcat(buf, "[secret] ");
	if (mode & SILC_CHANNEL_MODE_SECRET)
		strcat(buf, "[secret] ");
	if (mode & SILC_CHANNEL_MODE_PRIVKEY)
		strcat(buf, "[private key] ");
	if (mode & SILC_CHANNEL_MODE_INVITE)
		strcat(buf, "[invite only] ");
	if (mode & SILC_CHANNEL_MODE_TOPIC)
		strcat(buf, "[topic restricted] ");
	if (mode & SILC_CHANNEL_MODE_ULIMIT)
		strcat(buf, "[user count limit] ");
	if (mode & SILC_CHANNEL_MODE_PASSPHRASE)
		strcat(buf, "[passphrase auth] ");
	if (mode & SILC_CHANNEL_MODE_CHANNEL_AUTH)
		strcat(buf, "[public key auth] ");
	if (mode & SILC_CHANNEL_MODE_SILENCE_USERS)
		strcat(buf, "[users silenced] ");
	if (mode & SILC_CHANNEL_MODE_SILENCE_OPERS)
		strcat(buf, "[operators silenced] ");
}

void silcgaim_get_chumode_string(SilcUInt32 mode, char *buf,
				 SilcUInt32 buf_size)
{
	memset(buf, 0, buf_size);
	if (mode & SILC_CHANNEL_UMODE_CHANFO)
		strcat(buf, "[founder] ");
	if (mode & SILC_CHANNEL_UMODE_CHANOP)
		strcat(buf, "[operator] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES)
		strcat(buf, "[blocks messages] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES_USERS)
		strcat(buf, "[blocks user messages] ");
	if (mode & SILC_CHANNEL_UMODE_BLOCK_MESSAGES_ROBOTS)
		strcat(buf, "[blocks robot messages] ");
	if (mode & SILC_CHANNEL_UMODE_QUIET)
		strcat(buf, "[quieted] ");
}
