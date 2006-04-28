/*

  silcgaim_chat.c

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
#include "wb.h"

/***************************** Channel Routines ******************************/

GList *silcgaim_chat_info(GaimConnection *gc)
{
	GList *ci = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Channel:");
	pce->identifier = "channel";
	pce->required = TRUE;
	ci = g_list_append(ci, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Passphrase:");
	pce->identifier = "passphrase";
	pce->secret = TRUE;
	ci = g_list_append(ci, pce);

	return ci;
}

GHashTable *silcgaim_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

	return defaults;
}

static void
silcgaim_chat_getinfo(GaimConnection *gc, GHashTable *components);

static void
silcgaim_chat_getinfo_res(SilcClient client,
			  SilcClientConnection conn,
			  SilcChannelEntry *channels,
			  SilcUInt32 channels_count,
			  void *context)
{
	GHashTable *components = context;
	GaimConnection *gc = client->application;
	const char *chname;
	char tmp[256];

	chname = g_hash_table_lookup(components, "channel");
	if (!chname)
		return;

	if (!channels) {
		g_snprintf(tmp, sizeof(tmp),
			   _("Channel %s does not exist in the network"), chname);
		gaim_notify_error(gc, _("Channel Information"),
				  _("Cannot get channel information"), tmp);
		return;
	}

	silcgaim_chat_getinfo(gc, components);
}


static void
silcgaim_chat_getinfo(GaimConnection *gc, GHashTable *components)
{
	SilcGaim sg = gc->proto_data;
	const char *chname;
	char *buf, tmp[256], *tmp2;
	GString *s;
	SilcChannelEntry channel;
	SilcHashTableList htl;
	SilcChannelUser chu;

	if (!components)
		return;

	chname = g_hash_table_lookup(components, "channel");
	if (!chname)
		return;
	channel = silc_client_get_channel(sg->client, sg->conn,
					  (char *)chname);
	if (!channel) {
		silc_client_get_channel_resolve(sg->client, sg->conn,
						(char *)chname,
						silcgaim_chat_getinfo_res,
						components);
		return;
	}

	s = g_string_new("");
	tmp2 = g_markup_escape_text(channel->channel_name, -1);
	g_string_append_printf(s, _("<b>Channel Name:</b> %s"), tmp2);
	g_free(tmp2);
	if (channel->user_list && silc_hash_table_count(channel->user_list))
		g_string_append_printf(s, _("<br><b>User Count:</b> %d"),
				       (int)silc_hash_table_count(channel->user_list));

	silc_hash_table_list(channel->user_list, &htl);
	while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
		if (chu->mode & SILC_CHANNEL_UMODE_CHANFO) {
			tmp2 = g_markup_escape_text(chu->client->nickname, -1);
			g_string_append_printf(s, _("<br><b>Channel Founder:</b> %s"),
					       tmp2);
			g_free(tmp2);
			break;
		}
	}
	silc_hash_table_list_reset(&htl);

	if (channel->channel_key)
		g_string_append_printf(s, _("<br><b>Channel Cipher:</b> %s"),
				       silc_cipher_get_name(channel->channel_key));
	if (channel->hmac)
		/* Definition of HMAC: http://en.wikipedia.org/wiki/HMAC */
		g_string_append_printf(s, _("<br><b>Channel HMAC:</b> %s"),
				       silc_hmac_get_name(channel->hmac));

	if (channel->topic) {
		tmp2 = g_markup_escape_text(channel->topic, -1);
		g_string_append_printf(s, _("<br><b>Channel Topic:</b><br>%s"), tmp2);
		g_free(tmp2);
	}

	if (channel->mode) {
		g_string_append_printf(s, _("<br><b>Channel Modes:</b> "));
		silcgaim_get_chmode_string(channel->mode, tmp, sizeof(tmp));
		g_string_append(s, tmp);
	}

	if (channel->founder_key) {
		char *fingerprint, *babbleprint;
		unsigned char *pk;
		SilcUInt32 pk_len;
		pk = silc_pkcs_public_key_encode(channel->founder_key, &pk_len);
		fingerprint = silc_hash_fingerprint(NULL, pk, pk_len);
		babbleprint = silc_hash_babbleprint(NULL, pk, pk_len);

		g_string_append_printf(s, _("<br><b>Founder Key Fingerprint:</b><br>%s"), fingerprint);
		g_string_append_printf(s, _("<br><b>Founder Key Babbleprint:</b><br>%s"), babbleprint);

		silc_free(fingerprint);
		silc_free(babbleprint);
		silc_free(pk);
	}

	buf = g_string_free(s, FALSE);
	gaim_notify_formatted(gc, NULL, _("Channel Information"), NULL, buf, NULL, NULL);
	g_free(buf);
}


static void
silcgaim_chat_getinfo_menu(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat = (GaimChat *)node;
	silcgaim_chat_getinfo(chat->account->gc, chat->components);
}


#if 0   /* XXX For now these are not implemented.  We need better
	   listview dialog from Gaim for these. */
/************************** Channel Invite List ******************************/

static void
silcgaim_chat_invitelist(GaimBlistNode *node, gpointer data);
{

}


/**************************** Channel Ban List *******************************/

static void
silcgaim_chat_banlist(GaimBlistNode *node, gpointer data);
{

}
#endif


/************************* Channel Authentication ****************************/

typedef struct {
	SilcGaim sg;
	SilcChannelEntry channel;
	GaimChat *c;
	SilcBuffer pubkeys;
} *SilcGaimChauth;

static void
silcgaim_chat_chpk_add(void *user_data, const char *name)
{
	SilcGaimChauth sgc = (SilcGaimChauth)user_data;
	SilcGaim sg = sgc->sg;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcPublicKey public_key;
	SilcBuffer chpks, pk, chidp;
	unsigned char mode[4];
	SilcUInt32 m;

	/* Load the public key */
	if (!silc_pkcs_load_public_key(name, &public_key, SILC_PKCS_FILE_PEM) &&
	    !silc_pkcs_load_public_key(name, &public_key, SILC_PKCS_FILE_BIN)) {
		silcgaim_chat_chauth_show(sgc->sg, sgc->channel, sgc->pubkeys);
		silc_buffer_free(sgc->pubkeys);
		silc_free(sgc);
		gaim_notify_error(client->application,
				  _("Add Channel Public Key"),
				  _("Could not load public key"), NULL);
		return;
	}

	pk = silc_pkcs_public_key_payload_encode(public_key);
	chpks = silc_buffer_alloc_size(2);
	SILC_PUT16_MSB(1, chpks->head);
	chpks = silc_argument_payload_encode_one(chpks, pk->data,
						 pk->len, 0x00);
	silc_buffer_free(pk);

	m = sgc->channel->mode;
	m |= SILC_CHANNEL_MODE_CHANNEL_AUTH;

	/* Send CMODE */
	SILC_PUT32_MSB(m, mode);
	chidp = silc_id_payload_encode(sgc->channel->id, SILC_ID_CHANNEL);
	silc_client_command_send(client, conn, SILC_COMMAND_CMODE,
				 ++conn->cmd_ident, 3,
				 1, chidp->data, chidp->len,
				 2, mode, sizeof(mode),
				 9, chpks->data, chpks->len);
	silc_buffer_free(chpks);
	silc_buffer_free(chidp);
	silc_buffer_free(sgc->pubkeys);
	silc_free(sgc);
}

static void
silcgaim_chat_chpk_cancel(void *user_data, const char *name)
{
	SilcGaimChauth sgc = (SilcGaimChauth)user_data;
	silcgaim_chat_chauth_show(sgc->sg, sgc->channel, sgc->pubkeys);
	silc_buffer_free(sgc->pubkeys);
	silc_free(sgc);
}

static void
silcgaim_chat_chpk_cb(SilcGaimChauth sgc, GaimRequestFields *fields)
{
	SilcGaim sg = sgc->sg;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	GaimRequestField *f;
	const GList *list;
	SilcPublicKey public_key;
	SilcBuffer chpks, pk, chidp;
	SilcUInt16 c = 0, ct;
	unsigned char mode[4];
	SilcUInt32 m;

	f = gaim_request_fields_get_field(fields, "list");
	if (!gaim_request_field_list_get_selected(f)) {
		/* Add new public key */
		gaim_request_file(sg->gc, _("Open Public Key..."), NULL, FALSE,
				  G_CALLBACK(silcgaim_chat_chpk_add),
				  G_CALLBACK(silcgaim_chat_chpk_cancel), sgc);
		return;
	}

	list = gaim_request_field_list_get_items(f);
	chpks = silc_buffer_alloc_size(2);

	for (ct = 0; list; list = list->next, ct++) {
		public_key = gaim_request_field_list_get_data(f, list->data);
		if (gaim_request_field_list_is_selected(f, list->data)) {
			/* Delete this public key */
			pk = silc_pkcs_public_key_payload_encode(public_key);
			chpks = silc_argument_payload_encode_one(chpks, pk->data,
								 pk->len, 0x01);
			silc_buffer_free(pk);
			c++;
		}
		silc_pkcs_public_key_free(public_key);
	}
	if (!c) {
		silc_buffer_free(chpks);
		return;
	}
	SILC_PUT16_MSB(c, chpks->head);

	m = sgc->channel->mode;
	if (ct == c)
		m &= ~SILC_CHANNEL_MODE_CHANNEL_AUTH;

	/* Send CMODE */
	SILC_PUT32_MSB(m, mode);
	chidp = silc_id_payload_encode(sgc->channel->id, SILC_ID_CHANNEL);
	silc_client_command_send(client, conn, SILC_COMMAND_CMODE,
				 ++conn->cmd_ident, 3,
				 1, chidp->data, chidp->len,
				 2, mode, sizeof(mode),
				 9, chpks->data, chpks->len);
	silc_buffer_free(chpks);
	silc_buffer_free(chidp);
	silc_buffer_free(sgc->pubkeys);
	silc_free(sgc);
}

static void
silcgaim_chat_chauth_ok(SilcGaimChauth sgc, GaimRequestFields *fields)
{
	SilcGaim sg = sgc->sg;
	GaimRequestField *f;
	const char *curpass, *val;
	int set;

	f = gaim_request_fields_get_field(fields, "passphrase");
	val = gaim_request_field_string_get_value(f);
	curpass = gaim_blist_node_get_string((GaimBlistNode *)sgc->c, "passphrase");

	if (!val && curpass)
		set = 0;
	else if (val && !curpass)
		set = 1;
	else if (val && curpass && strcmp(val, curpass))
		set = 1;
	else
		set = -1;

	if (set == 1) {
		silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
					 sgc->channel->channel_name, "+a", val, NULL);
		gaim_blist_node_set_string((GaimBlistNode *)sgc->c, "passphrase", val);
	} else if (set == 0) {
		silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
					 sgc->channel->channel_name, "-a", NULL);
		gaim_blist_node_remove_setting((GaimBlistNode *)sgc->c, "passphrase");
	}

	silc_buffer_free(sgc->pubkeys);
	silc_free(sgc);
}

void silcgaim_chat_chauth_show(SilcGaim sg, SilcChannelEntry channel,
			       SilcBuffer channel_pubkeys)
{
	SilcUInt16 argc;
	SilcArgumentPayload chpks;
	unsigned char *pk;
	SilcUInt32 pk_len, type;
	char *fingerprint, *babbleprint;
	SilcPublicKey pubkey;
	SilcPublicKeyIdentifier ident;
	char tmp2[1024], t[512];
	GaimRequestFields *fields;
	GaimRequestFieldGroup *g;
	GaimRequestField *f;
	SilcGaimChauth sgc;
	const char *curpass = NULL;

	sgc = silc_calloc(1, sizeof(*sgc));
	if (!sgc)
		return;
	sgc->sg = sg;
	sgc->channel = channel;

	fields = gaim_request_fields_new();

	if (sgc->c)
	  curpass = gaim_blist_node_get_string((GaimBlistNode *)sgc->c, "passphrase");

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_string_new("passphrase", _("Channel Passphrase"),
					  curpass, FALSE);
	gaim_request_field_string_set_masked(f, TRUE);
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_label_new("l1", _("Channel Public Keys List"));
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	g_snprintf(t, sizeof(t),
		   _("Channel authentication is used to secure the channel from "
		     "unauthorized access. The authentication may be based on "
		     "passphrase and digital signatures. If passphrase is set, it "
		     "is required to be able to join. If channel public keys are set "
		     "then only users whose public keys are listed are able to join."));

	if (!channel_pubkeys) {
		f = gaim_request_field_list_new("list", NULL);
		gaim_request_field_group_add_field(g, f);
		gaim_request_fields(sg->gc, _("Channel Authentication"),
				    _("Channel Authentication"), t, fields,
				    _("Add / Remove"), G_CALLBACK(silcgaim_chat_chpk_cb),
				    _("OK"), G_CALLBACK(silcgaim_chat_chauth_ok), sgc);
		return;
	}
	sgc->pubkeys = silc_buffer_copy(channel_pubkeys);

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_list_new("list", NULL);
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	SILC_GET16_MSB(argc, channel_pubkeys->data);
	chpks = silc_argument_payload_parse(channel_pubkeys->data + 2,
					    channel_pubkeys->len - 2, argc);
	if (!chpks)
		return;

	pk = silc_argument_get_first_arg(chpks, &type, &pk_len);
	while (pk) {
		fingerprint = silc_hash_fingerprint(NULL, pk + 4, pk_len - 4);
		babbleprint = silc_hash_babbleprint(NULL, pk + 4, pk_len - 4);
		silc_pkcs_public_key_payload_decode(pk, pk_len, &pubkey);
		ident = silc_pkcs_decode_identifier(pubkey->identifier);

		g_snprintf(tmp2, sizeof(tmp2), "%s\n  %s\n  %s",
			   ident->realname ? ident->realname : ident->username ?
			   ident->username : "", fingerprint, babbleprint);
		gaim_request_field_list_add(f, tmp2, pubkey);

		silc_free(fingerprint);
		silc_free(babbleprint);
		silc_pkcs_free_identifier(ident);
		pk = silc_argument_get_next_arg(chpks, &type, &pk_len);
	}

	gaim_request_field_list_set_multi_select(f, FALSE);
	gaim_request_fields(sg->gc, _("Channel Authentication"),
			    _("Channel Authentication"), t, fields,
			    _("Add / Remove"), G_CALLBACK(silcgaim_chat_chpk_cb),
			    _("OK"), G_CALLBACK(silcgaim_chat_chauth_ok), sgc);

	silc_argument_payload_free(chpks);
}

static void
silcgaim_chat_chauth(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "+C", NULL);
}


/************************** Channel Private Groups **************************/

/* Private groups are "virtual" channels.  They are groups inside a channel.
   This is implemented by using channel private keys.  By knowing a channel
   private key user becomes part of that group and is able to talk on that
   group.  Other users, on the same channel, won't be able to see the
   messages of that group.  It is possible to have multiple groups inside
   a channel - and thus having multiple private keys on the channel. */

typedef struct {
	SilcGaim sg;
	GaimChat *c;
	const char *channel;
} *SilcGaimCharPrv;

static void
silcgaim_chat_prv_add(SilcGaimCharPrv p, GaimRequestFields *fields)
{
	SilcGaim sg = p->sg;
	char tmp[512];
	GaimRequestField *f;
	const char *name, *passphrase, *alias;
	GHashTable *comp;
	GaimGroup *g;
	GaimChat *cn;

	f = gaim_request_fields_get_field(fields, "name");
	name = gaim_request_field_string_get_value(f);
	if (!name) {
		silc_free(p);
		return;
	}
	f = gaim_request_fields_get_field(fields, "passphrase");
	passphrase = gaim_request_field_string_get_value(f);
	f = gaim_request_fields_get_field(fields, "alias");
	alias = gaim_request_field_string_get_value(f);

	/* Add private group to buddy list */
	g_snprintf(tmp, sizeof(tmp), "%s [Private Group]", name);
	comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(comp, g_strdup("channel"), g_strdup(tmp));
	g_hash_table_replace(comp, g_strdup("passphrase"), g_strdup(passphrase));

	cn = gaim_chat_new(sg->account, alias, comp);
	g = (GaimGroup *)p->c->node.parent;
	gaim_blist_add_chat(cn, g, (GaimBlistNode *)p->c);

	/* Associate to a real channel */
	gaim_blist_node_set_string((GaimBlistNode *)cn, "parentch", p->channel);

	/* Join the group */
	silcgaim_chat_join(sg->gc, comp);

	silc_free(p);
}

static void
silcgaim_chat_prv_cancel(SilcGaimCharPrv p, GaimRequestFields *fields)
{
	silc_free(p);
}

static void
silcgaim_chat_prv(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	SilcGaimCharPrv p;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *g;
	GaimRequestField *f;
	char tmp[512];

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	p = silc_calloc(1, sizeof(*p));
	if (!p)
		return;
	p->sg = sg;

	p->channel = g_hash_table_lookup(chat->components, "channel");
	p->c = gaim_blist_find_chat(sg->account, p->channel);

	fields = gaim_request_fields_new();

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_string_new("name", _("Group Name"),
					  NULL, FALSE);
	gaim_request_field_group_add_field(g, f);

	f = gaim_request_field_string_new("passphrase", _("Passphrase"),
					  NULL, FALSE);
	gaim_request_field_string_set_masked(f, TRUE);
	gaim_request_field_group_add_field(g, f);

	f = gaim_request_field_string_new("alias", _("Alias"),
					  NULL, FALSE);
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	g_snprintf(tmp, sizeof(tmp),
		   _("Please enter the %s channel private group name and passphrase."),
		   p->channel);
	gaim_request_fields(gc, _("Add Channel Private Group"), NULL, tmp, fields,
			    _("Add"), G_CALLBACK(silcgaim_chat_prv_add),
			    _("Cancel"), G_CALLBACK(silcgaim_chat_prv_cancel), p);
}


/****************************** Channel Modes ********************************/

static void
silcgaim_chat_permanent_reset(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "-f", NULL);
}

static void
silcgaim_chat_permanent(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;
	const char *channel;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	if (!sg->conn)
		return;

	/* XXX we should have ability to define which founder
	   key to use.  Now we use the user's own public key
	   (default key). */

	/* Call CMODE */
	channel = g_hash_table_lookup(chat->components, "channel");
	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE", channel,
				 "+f", NULL);
}

typedef struct {
	SilcGaim sg;
	const char *channel;
} *SilcGaimChatInput;

static void
silcgaim_chat_ulimit_cb(SilcGaimChatInput s, const char *limit)
{
	SilcChannelEntry channel;
	int ulimit = 0;

	channel = silc_client_get_channel(s->sg->client, s->sg->conn,
					  (char *)s->channel);
	if (!channel)
		return;
	if (limit)
		ulimit = atoi(limit);

	if (!limit || !(*limit) || *limit == '0') {
		if (limit && ulimit == channel->user_limit) {
			silc_free(s);
			return;
		}
		silc_client_command_call(s->sg->client, s->sg->conn, NULL, "CMODE",
					 s->channel, "-l", NULL);

		silc_free(s);
		return;
	}

	if (ulimit == channel->user_limit) {
		silc_free(s);
		return;
	}

	/* Call CMODE */
	silc_client_command_call(s->sg->client, s->sg->conn, NULL, "CMODE",
				 s->channel, "+l", limit, NULL);

	silc_free(s);
}

static void
silcgaim_chat_ulimit(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	SilcGaimChatInput s;
	SilcChannelEntry channel;
	const char *ch;
	char tmp[32];

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	if (!sg->conn)
		return;

	ch = g_strdup(g_hash_table_lookup(chat->components, "channel"));
	channel = silc_client_get_channel(sg->client, sg->conn, (char *)ch);
	if (!channel)
		return;

	s = silc_calloc(1, sizeof(*s));
	if (!s)
		return;
	s->channel = ch;
	s->sg = sg;
	g_snprintf(tmp, sizeof(tmp), "%d", (int)channel->user_limit);
	gaim_request_input(gc, _("User Limit"), NULL,
			   _("Set user limit on channel. Set to zero to reset user limit."),
			   tmp, FALSE, FALSE, NULL,
			   _("OK"), G_CALLBACK(silcgaim_chat_ulimit_cb),
			   _("Cancel"), G_CALLBACK(silcgaim_chat_ulimit_cb), s);
}

static void
silcgaim_chat_resettopic(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "-t", NULL);
}

static void
silcgaim_chat_settopic(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "+t", NULL);
}

static void
silcgaim_chat_resetprivate(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "-p", NULL);
}

static void
silcgaim_chat_setprivate(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "+p", NULL);
}

static void
silcgaim_chat_resetsecret(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "-s", NULL);
}

static void
silcgaim_chat_setsecret(GaimBlistNode *node, gpointer data)
{
	GaimChat *chat;
	GaimConnection *gc;
	SilcGaim sg;

	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT(node));

	chat = (GaimChat *) node;
	gc = gaim_account_get_connection(chat->account);
	sg = gc->proto_data;

	silc_client_command_call(sg->client, sg->conn, NULL, "CMODE",
				 g_hash_table_lookup(chat->components, "channel"),
				 "+s", NULL);
}

typedef struct {
	SilcGaim sg;
	SilcChannelEntry channel;
} *SilcGaimChatWb;

static void
silcgaim_chat_wb(GaimBlistNode *node, gpointer data)
{
	SilcGaimChatWb wb = data;
	silcgaim_wb_init_ch(wb->sg, wb->channel);
	silc_free(wb);
}

GList *silcgaim_chat_menu(GaimChat *chat)
{
	GHashTable *components = chat->components;
	GaimConnection *gc = gaim_account_get_connection(chat->account);
	SilcGaim sg = gc->proto_data;
	SilcClientConnection conn = sg->conn;
	const char *chname = NULL;
	SilcChannelEntry channel = NULL;
	SilcChannelUser chu = NULL;
	SilcUInt32 mode = 0;

	GList *m = NULL;
	GaimMenuAction *act;

	if (components)
		chname = g_hash_table_lookup(components, "channel");
	if (chname)
		channel = silc_client_get_channel(sg->client, sg->conn,
						  (char *)chname);
	if (channel) {
		chu = silc_client_on_channel(channel, conn->local_entry);
		if (chu)
			mode = chu->mode;
	}

	if (strstr(chname, "[Private Group]"))
		return NULL;

	act = gaim_menu_action_new(_("Get Info"),
	                           GAIM_CALLBACK(silcgaim_chat_getinfo_menu),
	                           NULL, NULL);
	m = g_list_append(m, act);

#if 0   /* XXX For now these are not implemented.  We need better
	   listview dialog from Gaim for these. */
	if (mode & SILC_CHANNEL_UMODE_CHANOP) {
		act = gaim_menu_action_new(_("Invite List"),
		                           GAIM_CALLBACK(silcgaim_chat_invitelist),
		                           NULL, NULL);
		m = g_list_append(m, act);

		act = gaim_menu_action_new(_("Ban List"),
		                           GAIM_CALLBACK(silcgaim_chat_banlist),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}
#endif

	if (chu) {
		act = gaim_menu_action_new(_("Add Private Group"),
		                           GAIM_CALLBACK(silcgaim_chat_prv),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	if (mode & SILC_CHANNEL_UMODE_CHANFO) {
		act = gaim_menu_action_new(_("Channel Authentication"),
		                           GAIM_CALLBACK(silcgaim_chat_chauth),
		                           NULL, NULL);
		m = g_list_append(m, act);

		if (channel->mode & SILC_CHANNEL_MODE_FOUNDER_AUTH) {
			act = gaim_menu_action_new(_("Reset Permanent"),
			                           GAIM_CALLBACK(silcgaim_chat_permanent_reset),
			                           NULL, NULL);
			m = g_list_append(m, act);
		} else {
			act = gaim_menu_action_new(_("Set Permanent"),
			                           GAIM_CALLBACK(silcgaim_chat_permanent),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (mode & SILC_CHANNEL_UMODE_CHANOP) {
		act = gaim_menu_action_new(_("Set User Limit"),
		                           GAIM_CALLBACK(silcgaim_chat_ulimit),
		                           NULL, NULL);
		m = g_list_append(m, act);

		if (channel->mode & SILC_CHANNEL_MODE_TOPIC) {
			act = gaim_menu_action_new(_("Reset Topic Restriction"),
			                           GAIM_CALLBACK(silcgaim_chat_resettopic),
			                           NULL, NULL);
			m = g_list_append(m, act);
		} else {
			act = gaim_menu_action_new(_("Set Topic Restriction"),
			                           GAIM_CALLBACK(silcgaim_chat_settopic),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}

		if (channel->mode & SILC_CHANNEL_MODE_PRIVATE) {
			act = gaim_menu_action_new(_("Reset Private Channel"),
			                           GAIM_CALLBACK(silcgaim_chat_resetprivate),
			                           NULL, NULL);
			m = g_list_append(m, act);
		} else {
			act = gaim_menu_action_new(_("Set Private Channel"),
			                           GAIM_CALLBACK(silcgaim_chat_setprivate),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}

		if (channel->mode & SILC_CHANNEL_MODE_SECRET) {
			act = gaim_menu_action_new(_("Reset Secret Channel"),
			                           GAIM_CALLBACK(silcgaim_chat_resetsecret),
			                           NULL, NULL);
			m = g_list_append(m, act);
		} else {
			act = gaim_menu_action_new(_("Set Secret Channel"),
			                           GAIM_CALLBACK(silcgaim_chat_setsecret),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (channel) {
		SilcGaimChatWb wb;
		wb = silc_calloc(1, sizeof(*wb));
		wb->sg = sg;
		wb->channel = channel;
		act = gaim_menu_action_new(_("Draw On Whiteboard"),
		                           GAIM_CALLBACK(silcgaim_chat_wb),
		                           (void *)wb, NULL);
		m = g_list_append(m, act);
	}

	return m;
}


/******************************* Joining Etc. ********************************/

void silcgaim_chat_join_done(SilcClient client,
			     SilcClientConnection conn,
			     SilcClientEntry *clients,
			     SilcUInt32 clients_count,
			     void *context)
{
	GaimConnection *gc = client->application;
	SilcGaim sg = gc->proto_data;
	SilcChannelEntry channel = context;
	GaimConversation *convo;
	SilcUInt32 retry = SILC_PTR_TO_32(channel->context);
	SilcHashTableList htl;
	SilcChannelUser chu;
	GList *users = NULL, *flags = NULL;
	char tmp[256];

	if (!clients && retry < 1) {
		/* Resolving users failed, try again. */
		channel->context = SILC_32_TO_PTR(retry + 1);
		silc_client_get_clients_by_channel(client, conn, channel,
						   silcgaim_chat_join_done, channel);
		return;
	}

	/* Add channel to Gaim */
	channel->context = SILC_32_TO_PTR(++sg->channel_ids);
	serv_got_joined_chat(gc, sg->channel_ids, channel->channel_name);
	convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT,
							channel->channel_name, sg->account);
	if (!convo)
		return;

	/* Add all users to channel */
	silc_hash_table_list(channel->user_list, &htl);
	while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
		GaimConvChatBuddyFlags f = GAIM_CBFLAGS_NONE;
		if (!chu->client->nickname)
			continue;
		chu->context = SILC_32_TO_PTR(sg->channel_ids);

		if (chu->mode & SILC_CHANNEL_UMODE_CHANFO)
			f |= GAIM_CBFLAGS_FOUNDER;
		if (chu->mode & SILC_CHANNEL_UMODE_CHANOP)
			f |= GAIM_CBFLAGS_OP;
		users = g_list_append(users, g_strdup(chu->client->nickname));
		flags = g_list_append(flags, GINT_TO_POINTER(f));

		if (chu->mode & SILC_CHANNEL_UMODE_CHANFO) {
			if (chu->client == conn->local_entry)
				g_snprintf(tmp, sizeof(tmp),
					   _("You are channel founder on <I>%s</I>"),
					   channel->channel_name);
			else
				g_snprintf(tmp, sizeof(tmp),
					   _("Channel founder on <I>%s</I> is <I>%s</I>"),
					   channel->channel_name, chu->client->nickname);

			gaim_conversation_write(convo, NULL, tmp,
						GAIM_MESSAGE_SYSTEM, time(NULL));

		}
	}
	silc_hash_table_list_reset(&htl);

	gaim_conv_chat_add_users(GAIM_CONV_CHAT(convo), users, NULL, flags, FALSE);
	g_list_free(users);
	g_list_free(flags);

	/* Set topic */
	if (channel->topic)
		gaim_conv_chat_set_topic(GAIM_CONV_CHAT(convo), NULL, channel->topic);

	/* Set nick */
	gaim_conv_chat_set_nick(GAIM_CONV_CHAT(convo), conn->local_entry->nickname);
}

char *silcgaim_get_chat_name(GHashTable *data)
{
	return g_strdup(g_hash_table_lookup(data, "channel"));
}	

void silcgaim_chat_join(GaimConnection *gc, GHashTable *data)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	const char *channel, *passphrase, *parentch;

	if (!conn)
		return;

	channel = g_hash_table_lookup(data, "channel");
	passphrase = g_hash_table_lookup(data, "passphrase");

	/* Check if we are joining a private group.  Handle it
	   purely locally as it's not a real channel */
	if (strstr(channel, "[Private Group]")) {
		SilcChannelEntry channel_entry;
		SilcChannelPrivateKey key;
		GaimChat *c;
		SilcGaimPrvgrp grp;

		c = gaim_blist_find_chat(sg->account, channel);
		parentch = gaim_blist_node_get_string((GaimBlistNode *)c, "parentch");
		if (!parentch)
			return;

		channel_entry = silc_client_get_channel(sg->client, sg->conn,
							(char *)parentch);
		if (!channel_entry ||
		    !silc_client_on_channel(channel_entry, sg->conn->local_entry)) {
			char tmp[512];
			g_snprintf(tmp, sizeof(tmp),
				   _("You have to join the %s channel before you are "
				     "able to join the private group"), parentch);
			gaim_notify_error(gc, _("Join Private Group"),
					  _("Cannot join private group"), tmp);
			return;
		}

		/* Add channel private key */
		if (!silc_client_add_channel_private_key(client, conn,
							 channel_entry, channel,
							 NULL, NULL,
							 (unsigned char *)passphrase,
							 strlen(passphrase), &key))
			return;

		/* Join the group */
		grp = silc_calloc(1, sizeof(*grp));
		if (!grp)
			return;
		grp->id = ++sg->channel_ids + SILCGAIM_PRVGRP;
		grp->chid = SILC_PTR_TO_32(channel_entry->context);
		grp->parentch = parentch;
		grp->channel = channel;
		grp->key = key;
		sg->grps = g_list_append(sg->grps, grp);
		serv_got_joined_chat(gc, grp->id, channel);
		return;
	}

	/* XXX We should have other properties here as well:
	   1. whether to try to authenticate to the channel
	     1a. with default key,
	     1b. with specific key.
	   2. whether to try to authenticate to become founder.
	     2a. with default key,
	     2b. with specific key.

	   Since now such variety is not possible in the join dialog
	   we always use -founder and -auth options, which try to
	   do both 1 and 2 with default keys. */

	/* Call JOIN */
	if ((passphrase != NULL) && (*passphrase != '\0'))
		silc_client_command_call(client, conn, NULL, "JOIN",
					 channel, passphrase, "-auth", "-founder", NULL);
	else
		silc_client_command_call(client, conn, NULL, "JOIN",
					 channel, "-auth", "-founder", NULL);
}

void silcgaim_chat_invite(GaimConnection *gc, int id, const char *msg,
			  const char *name)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcHashTableList htl;
	SilcChannelUser chu;
	gboolean found = FALSE;

	if (!conn)
		return;

	/* See if we are inviting on a private group.  Invite
	   to the actual channel */
	if (id > SILCGAIM_PRVGRP) {
		GList *l;
		SilcGaimPrvgrp prv;

		for (l = sg->grps; l; l = l->next)
			if (((SilcGaimPrvgrp)l->data)->id == id)
				break;
		if (!l)
			return;
		prv = l->data;
		id = prv->chid;
	}

	/* Find channel by id */
	silc_hash_table_list(conn->local_entry->channels, &htl);
	while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
		if (SILC_PTR_TO_32(chu->channel->context) == id ) {
			found = TRUE;
			break;
		}
	}
	silc_hash_table_list_reset(&htl);
	if (!found)
		return;

	/* Call INVITE */
	silc_client_command_call(client, conn, NULL, "INVITE",
				 chu->channel->channel_name,
				 name, NULL);
}

void silcgaim_chat_leave(GaimConnection *gc, int id)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcHashTableList htl;
	SilcChannelUser chu;
	gboolean found = FALSE;
	GList *l;
	SilcGaimPrvgrp prv;

	if (!conn)
		return;

	/* See if we are leaving a private group */
	if (id > SILCGAIM_PRVGRP) {
		SilcChannelEntry channel;

		for (l = sg->grps; l; l = l->next)
			if (((SilcGaimPrvgrp)l->data)->id == id)
				break;
		if (!l)
			return;
		prv = l->data;
		channel = silc_client_get_channel(sg->client, sg->conn,
						  (char *)prv->parentch);
		if (!channel)
			return;
		silc_client_del_channel_private_key(client, conn,
						    channel, prv->key);
		silc_free(prv);
		sg->grps = g_list_remove(sg->grps, prv);
		serv_got_chat_left(gc, id);
		return;
	}

	/* Find channel by id */
	silc_hash_table_list(conn->local_entry->channels, &htl);
	while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
		if (SILC_PTR_TO_32(chu->channel->context) == id ) {
			found = TRUE;
			break;
		}
	}
	silc_hash_table_list_reset(&htl);
	if (!found)
		return;

	/* Call LEAVE */
	silc_client_command_call(client, conn, NULL, "LEAVE",
				 chu->channel->channel_name, NULL);

	serv_got_chat_left(gc, id);

	/* Leave from private groups on this channel as well */
	for (l = sg->grps; l; l = l->next)
		if (((SilcGaimPrvgrp)l->data)->chid == id) {
			prv = l->data;
			silc_client_del_channel_private_key(client, conn,
							    chu->channel,
							    prv->key);
			serv_got_chat_left(gc, prv->id);
			silc_free(prv);
			sg->grps = g_list_remove(sg->grps, prv);
			if (!sg->grps)
				break;
		}
}

int silcgaim_chat_send(GaimConnection *gc, int id, const char *msg, GaimMessageFlags msgflags)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcHashTableList htl;
	SilcChannelUser chu;
	SilcChannelEntry channel = NULL;
	SilcChannelPrivateKey key = NULL;
	SilcUInt32 flags;
	int ret;
	char *msg2, *tmp;
	gboolean found = FALSE;
	gboolean sign = gaim_account_get_bool(sg->account, "sign-verify", FALSE);

	if (!msg || !conn)
		return 0;

	flags = SILC_MESSAGE_FLAG_UTF8;

	tmp = msg2 = gaim_unescape_html(msg);

	if (!g_ascii_strncasecmp(msg2, "/me ", 4))
	{
		msg2 += 4;
		if (!*msg2) {
			g_free(tmp);
			return 0;
		}
		flags |= SILC_MESSAGE_FLAG_ACTION;
	} else if (strlen(msg) > 1 && msg[0] == '/') {
		if (!silc_client_command_call(client, conn, msg + 1))
			gaim_notify_error(gc, _("Call Command"), _("Cannot call command"),
							  _("Unknown command"));
		g_free(tmp);
		return 0;
	}


	if (sign)
		flags |= SILC_MESSAGE_FLAG_SIGNED;

	/* Get the channel private key if we are sending on
	   private group */
	if (id > SILCGAIM_PRVGRP) {
		GList *l;
		SilcGaimPrvgrp prv;

		for (l = sg->grps; l; l = l->next)
			if (((SilcGaimPrvgrp)l->data)->id == id)
				break;
		if (!l) {
			g_free(tmp);
			return 0;
		}
		prv = l->data;
		channel = silc_client_get_channel(sg->client, sg->conn,
						  (char *)prv->parentch);
		if (!channel) {
			g_free(tmp);
			return 0;
		}
		key = prv->key;
	}

	if (!channel) {
		/* Find channel by id */
		silc_hash_table_list(conn->local_entry->channels, &htl);
		while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
			if (SILC_PTR_TO_32(chu->channel->context) == id ) {
				found = TRUE;
				break;
			}
		}
		silc_hash_table_list_reset(&htl);
		if (!found) {
			g_free(tmp);
			return 0;
		}
		channel = chu->channel;
	}

	/* Send channel message */
	ret = silc_client_send_channel_message(client, conn, channel, key,
					       flags, (unsigned char *)msg2,
					       strlen(msg2), TRUE);
	if (ret) {
		serv_got_chat_in(gc, id, gaim_connection_get_display_name(gc), 0, msg,
				 time(NULL));
	}
	g_free(tmp);

	return ret;
}

void silcgaim_chat_set_topic(GaimConnection *gc, int id, const char *topic)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcHashTableList htl;
	SilcChannelUser chu;
	gboolean found = FALSE;

	if (!conn)
		return;

	/* See if setting topic on private group.  Set it
	   on the actual channel */
	if (id > SILCGAIM_PRVGRP) {
		GList *l;
		SilcGaimPrvgrp prv;

		for (l = sg->grps; l; l = l->next)
			if (((SilcGaimPrvgrp)l->data)->id == id)
				break;
		if (!l)
			return;
		prv = l->data;
		id = prv->chid;
	}

	/* Find channel by id */
	silc_hash_table_list(conn->local_entry->channels, &htl);
	while (silc_hash_table_get(&htl, NULL, (void *)&chu)) {
		if (SILC_PTR_TO_32(chu->channel->context) == id ) {
			found = TRUE;
			break;
		}
	}
	silc_hash_table_list_reset(&htl);
	if (!found)
		return;

	/* Call TOPIC */
	silc_client_command_call(client, conn, NULL, "TOPIC",
				 chu->channel->channel_name, topic, NULL);
}

GaimRoomlist *silcgaim_roomlist_get_list(GaimConnection *gc)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	GList *fields = NULL;
	GaimRoomlistField *f;

	if (!conn)
		return NULL;

	if (sg->roomlist)
		gaim_roomlist_unref(sg->roomlist);

	sg->roomlist_canceled = FALSE;

	sg->roomlist = gaim_roomlist_new(gaim_connection_get_account(gc));
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "channel", TRUE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_INT,
				    _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING,
				    _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);
	gaim_roomlist_set_fields(sg->roomlist, fields);

	/* Call LIST */
	silc_client_command_call(client, conn, "LIST");

	gaim_roomlist_set_in_progress(sg->roomlist, TRUE);

	return sg->roomlist;
}

void silcgaim_roomlist_cancel(GaimRoomlist *list)
{
	GaimConnection *gc = gaim_account_get_connection(list->account);
	SilcGaim sg;

	if (!gc)
		return;
	sg = gc->proto_data;

	gaim_roomlist_set_in_progress(list, FALSE);
	if (sg->roomlist == list) {
		gaim_roomlist_unref(sg->roomlist);
		sg->roomlist = NULL;
		sg->roomlist_canceled = TRUE;
	}
}
