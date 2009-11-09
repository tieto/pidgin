/*
 * purple - Jabber Protocol Plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "auth.h"
#include "auth_scram.h"

#include "cipher.h"
#include "debug.h"

static const struct {
	const char *mech_substr;
	const char *hash;
} mech_hashes[] = {
	{ "-SHA-1-", "sha1" },
};

static const struct {
	const char *hash;
	guint size;
} hash_sizes[] = {
	{ "sha1", 20 },
};

static const gchar *mech_to_hash(const char *mech)
{
	int i;

	g_return_val_if_fail(mech != NULL && *mech != '\0', NULL);

	for (i = 0; i < G_N_ELEMENTS(mech_hashes); ++i) {
		if (strstr(mech, mech_hashes[i].mech_substr))
			return mech_hashes[i].hash;
	}

	return NULL;
}

static guint hash_to_output_len(const gchar *hash)
{
	int i;

	g_return_val_if_fail(hash != NULL && *hash != '\0', 0);

	for (i = 0; i < G_N_ELEMENTS(hash_sizes); ++i) {
		if (g_str_equal(hash, hash_sizes[i].hash))
			return hash_sizes[i].size;
	}

	purple_debug_error("jabber", "Unknown SCRAM hash function %s\n", hash);

	return 0;
}

guchar *jabber_scram_hi(const gchar *hash, const GString *str,
                        GString *salt, guint iterations)
{
	PurpleCipherContext *context;
	guchar *result;
	guint i;
	guint hash_len;
	guchar *prev, *tmp;

	g_return_val_if_fail(hash != NULL, NULL);
	g_return_val_if_fail(str != NULL && str->len > 0, NULL);
	g_return_val_if_fail(salt != NULL && salt->len > 0, NULL);
	g_return_val_if_fail(iterations > 0, NULL);

	hash_len = hash_to_output_len(hash);
	g_return_val_if_fail(hash_len > 0, NULL);

	prev = g_new0(guint8, hash_len);
	tmp = g_new0(guint8, hash_len);
	result = g_new0(guint8, hash_len);

	context = purple_cipher_context_new_by_name("hmac", NULL);

	/* Append INT(1), a four-octet encoding of the integer 1, most significant
	 * octet first. */
	g_string_append_len(salt, "\0\0\0\1", 4);

	/* Compute U0 */
	purple_cipher_context_set_option(context, "hash", (gpointer)hash);
	purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
	purple_cipher_context_append(context, (guchar *)salt->str, salt->len);
	purple_cipher_context_digest(context, hash_len, result, NULL);

	memcpy(prev, result, hash_len);

	/* Compute U1...Ui */
	for (i = 1; i < iterations; ++i) {
		guint j;
		purple_cipher_context_set_option(context, "hash", (gpointer)hash);
		purple_cipher_context_set_key_with_len(context, (guchar *)str->str, str->len);
		purple_cipher_context_append(context, prev, hash_len);
		purple_cipher_context_digest(context, hash_len, tmp, NULL);

		for (j = 0; j < hash_len; ++j)
			result[j] ^= tmp[j];

		memcpy(prev, tmp, hash_len);
	}

	purple_cipher_context_destroy(context);
	g_free(tmp);
	g_free(prev);
	return result;
}

/*
 * Helper functions for doing the SCRAM calculations. The first argument
 * is the hash algorithm and the second (len) is the length of the output
 * buffer and key/data (the fourth argument).
 * "str" is a NULL-terminated string for hmac().
 *
 * Needless to say, these are fragile.
 */
static void
hmac(const gchar *hash_alg, gsize len, guchar *out, const guchar *key, const gchar *str)
{
	PurpleCipherContext *context;

	context = purple_cipher_context_new_by_name("hmac", NULL);
	purple_cipher_context_set_option(context, "hash", (gpointer)hash_alg);
	purple_cipher_context_set_key_with_len(context, key, len);
	purple_cipher_context_append(context, (guchar *)str, strlen(str));
	purple_cipher_context_digest(context, len, out, NULL);
	purple_cipher_context_destroy(context);
}

static void
hash(const gchar *hash_alg, gsize len, guchar *out, const guchar *data)
{
	PurpleCipherContext *context;

	context = purple_cipher_context_new_by_name(hash_alg, NULL);
	purple_cipher_context_append(context, data, len);
	purple_cipher_context_digest(context, len, out, NULL);
	purple_cipher_context_destroy(context);
}

gboolean
jabber_scram_calc_proofs(JabberScramData *data, const char *password,
                         GString *salt, guint iterations)
{
	guint hash_len = hash_to_output_len(data->hash);
	guint i;

	GString *pass = g_string_new(password);

	guchar *salted_password;
	guchar client_key[hash_len];
	guchar stored_key[hash_len];
	guchar client_signature[hash_len];
	guchar server_key[hash_len];

	data->client_proof = g_string_sized_new(hash_len);
	data->client_proof->len = hash_len;
	data->server_signature = g_string_sized_new(hash_len);
	data->server_signature->len = hash_len;

	salted_password = jabber_scram_hi(data->hash, pass, salt, iterations);
	g_string_free(pass, TRUE);
	if (!salted_password)
		return FALSE;

	/* client_key = HMAC(salted_password, "Client Key") */
	hmac(data->hash, hash_len, client_key, salted_password, "Client Key");
	/* server_key = HMAC(salted_password, "Server Key") */
	hmac(data->hash, hash_len, server_key, salted_password, "Server Key");
	g_free(salted_password);

	/* stored_key = HASH(client_key) */
	hash(data->hash, hash_len, stored_key, client_key);

	/* client_signature = HMAC(stored_key, auth_message) */
	hmac(data->hash, hash_len, client_signature, stored_key, data->auth_message->str);
	/* server_signature = HMAC(server_key, auth_message) */
	hmac(data->hash, hash_len, (guchar *)data->server_signature->str, server_key, data->auth_message->str);

	/* client_proof = client_key XOR client_signature */
	for (i = 0; i < hash_len; ++i)
		data->client_proof->str[i] = client_key[i] ^ client_signature[i];

	return TRUE;
}

static gboolean
parse_challenge(JabberScramData *data, const char *challenge,
                gchar **out_nonce, GString **out_salt, guint *out_iterations)
{
	gsize cnonce_len;
	const char *cur;
	const char *end;
	const char *val_start, *val_end;
	char *tmp, *decoded;
	gsize decoded_len;
	char *nonce;
	GString *salt;
	guint iterations;

	cur = challenge;
	end = challenge + strlen(challenge);

	if (cur[0] != 'r' || cur[1] != '=')
		return FALSE;

	val_start = cur + 2;
	val_end = strchr(val_start, ',');
	if (val_end == NULL)
		return FALSE;

	/* Ensure that the first cnonce_len bytes of the nonce are the original
	 * cnonce we sent to the server.
	 */
	cnonce_len = strlen(data->cnonce);
	if ((val_end - val_start + 1) <= cnonce_len ||
			strncmp(data->cnonce, val_start, cnonce_len) != 0)
		return FALSE;

	nonce = g_strndup(val_start, val_end - val_start + 1);

	/* The Salt, base64-encoded */
	cur = val_end + 1;
	if (cur[0] != 's' || cur[1] != '=') {
		g_free(nonce);
		return FALSE;
	}

	val_start = cur + 2;
	val_end = strchr(val_start, ',');
	if (val_end == NULL) {
		g_free(nonce);
		return FALSE;
	}

	tmp = g_strndup(val_start, val_end - val_start + 1);
	decoded = (gchar *)purple_base64_decode(tmp, &decoded_len);
	g_free(tmp);
	salt = g_string_new_len(decoded, decoded_len);
	g_free(decoded);

	/* The iteration count */
	cur = val_end + 1;
	if (cur[0] != 'i' || cur[1] != '=') {
		g_free(nonce);
		g_string_free(salt, TRUE);
		return FALSE;
	}

	val_start = cur + 2;
	val_end = strchr(val_start, ',');
	if (val_end == NULL)
		/* There could be extensions. This should possibly be a hard fail. */
		val_end = end - 1;

	/* Validate the string */
	for (tmp = (gchar *)val_start; tmp != val_end; ++tmp) {
		if (!g_ascii_isdigit(*tmp)) {
			g_free(nonce);
			g_string_free(salt, TRUE);
			return FALSE;
		}
	}

	tmp = g_strndup(val_start, val_end - val_start + 1);
	iterations = strtoul(tmp, NULL, 10);
	g_free(tmp);

	*out_nonce = nonce;
	*out_salt = salt;
	*out_iterations = iterations;
	return TRUE;
}

static gboolean
parse_success(JabberScramData *data, const char *success,
              gchar **out_verifier)
{
	const char *cur;
	const char *val_start, *val_end;
	const char *end;

	char *verifier;

	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(success != NULL, FALSE);
	g_return_val_if_fail(out_verifier != NULL, FALSE);

	cur = success;
	end = cur + strlen(cur);

	if (cur[0] != 'v' || cur[1] != '=') {
		/* TODO: Error handling */
		return FALSE;
	}

	val_start = cur + 2;
	val_end = strchr(val_start, ',');
	if (val_end == NULL)
		/* TODO: Maybe make this a strict check on not having any extensions? */
		val_end = end - 1;

	verifier = g_strndup(val_start, val_end - val_start + 1);

	*out_verifier = verifier;
	return TRUE;
}

static xmlnode *scram_start(JabberStream *js, xmlnode *mechanisms)
{
	xmlnode *reply;
	JabberScramData *data;
	guint64 cnonce;
#ifdef CHANNEL_BINDING
	gboolean binding_supported = TRUE;
#endif
	gchar *dec_out, *enc_out;

	data = js->auth_mech_data = g_new0(JabberScramData, 1);
	data->hash = mech_to_hash(js->auth_mech->name);

#ifdef CHANNEL_BINDING
	if (strstr(js->auth_mech_name, "-PLUS"))
		data->channel_binding = TRUE;
#endif
	cnonce = ((guint64)g_random_int() << 32) | g_random_int();
	data->cnonce = purple_base64_encode((guchar *)cnonce, sizeof(cnonce));

	data->auth_message = g_string_new(NULL);
	g_string_printf(data->auth_message, "n=%s,r=%s",
			js->user->node /* TODO: SaslPrep */,
			data->cnonce);

	reply = xmlnode_new("auth");
	xmlnode_set_namespace(reply, "urn:ietf:params:xml:ns:xmpp-sasl");
	xmlnode_set_attrib(reply, "mechanism", js->auth_mech->name);

	/* TODO: Channel binding */
	dec_out = g_strdup_printf("%c,,%s", 'n', data->auth_message->str);
	enc_out = purple_base64_encode((guchar *)dec_out, strlen(dec_out));
	purple_debug_misc("jabber", "initial SCRAM message '%s'\n", dec_out);

	xmlnode_insert_data(reply, enc_out, -1);

	g_free(enc_out);
	g_free(dec_out);

	return reply;
}

static xmlnode *scram_handle_challenge(JabberStream *js, xmlnode *challenge)
{
	JabberScramData *data = js->auth_mech_data;
	xmlnode *reply;

	gchar *enc_in, *dec_in;
	gchar *enc_out, *dec_out;
	gsize decoded_size;

	gchar *enc_proof;

	gchar *nonce;
	GString *salt;
	guint iterations;

	g_return_val_if_fail(data != NULL, NULL);

	enc_in = xmlnode_get_data(challenge);
	/* TODO: Error handling */
	g_return_val_if_fail(enc_in != NULL && *enc_in != '\0', NULL);

	dec_in = (gchar *)purple_base64_decode(enc_in, &decoded_size);
	g_free(enc_in);
	if (!dec_in || decoded_size != strlen(dec_in)) {
		/* Danger afoot; SCRAM shouldn't contain NUL bytes */
		/* TODO: Error handling */
		g_free(dec_in);
		return NULL;
	}

	purple_debug_misc("jabber", "decoded challenge (%" G_GSIZE_FORMAT "): %s\n",
			decoded_size, dec_in);

	g_string_append_c(data->auth_message, ',');
	g_string_append(data->auth_message, dec_in);

	if (!parse_challenge(data, dec_in, &nonce, &salt, &iterations)) {
		/* TODO: Error handling */
		return NULL;
	}

	g_string_append_c(data->auth_message, ',');
	/* "biwsCg==" is the base64 encoding of "n,,". I promise. */
	g_string_append_printf(data->auth_message, "c=%s,r=%s", "biwsCg==", nonce);
#ifdef CHANNEL_BINDING
	#error fix this
#endif

	if (!jabber_scram_calc_proofs(data, purple_connection_get_password(js->gc), salt, iterations)) {
		/* TODO: Error handling */
		return NULL;
	}

	reply = xmlnode_new("response");
	xmlnode_set_namespace(reply, "urn:ietf:params:xml:ns:xmpp-sasl");

	enc_proof = purple_base64_encode((guchar *)data->client_proof->str, data->client_proof->len);
	dec_out = g_strdup_printf("c=%s,r=%s,p=%s", "biwsCg==", nonce, enc_proof);
	enc_out = purple_base64_encode((guchar *)dec_out, strlen(dec_out));

	purple_debug_misc("jabber", "decoded response (%" G_GSIZE_FORMAT "): %s\n",
			strlen(dec_out), dec_out);

	xmlnode_insert_data(reply, enc_out, -1);

	g_free(enc_out);
	g_free(dec_out);
	g_free(enc_proof);

	return reply;
}

static gboolean scram_handle_success(JabberStream *js, xmlnode *packet)
{
	JabberScramData *data = js->auth_mech_data;
	char *enc_in, *dec_in;
	char *enc_server_signature;
	guchar *server_signature;
	gsize decoded_size;

	enc_in = xmlnode_get_data(packet);
	/* TODO: Error handling */
	g_return_val_if_fail(enc_in != NULL && *enc_in != '\0', FALSE);

	dec_in = (gchar *)purple_base64_decode(enc_in, &decoded_size);
	g_free(enc_in);
	if (!dec_in || decoded_size != strlen(dec_in)) {
		/* Danger afoot; SCRAM shouldn't contain NUL bytes */
		/* TODO: Error handling */
		g_free(dec_in);
		return FALSE;
	}

	purple_debug_misc("jabber", "decoded success (%" G_GSIZE_FORMAT "): %s\n",
			decoded_size, dec_in);

	if (!parse_success(data, dec_in, &enc_server_signature)) {
		/* TODO: Error handling */
		return FALSE;
	}

	server_signature = purple_base64_decode(enc_server_signature, &decoded_size);
	if (server_signature == NULL) {
		/* TODO: Error handling */
		return FALSE;
	}

	if (decoded_size != data->server_signature->len) {
		/* TODO: Error handling */
		purple_debug_error("jabber", "SCRAM server signature wrong length (was "
				"(was %" G_GSIZE_FORMAT ", expected %" G_GSIZE_FORMAT ")\n",
				decoded_size, data->server_signature->len);
		return FALSE;
	}

	if (0 != memcmp(server_signature, data->server_signature->str, decoded_size)) {
		/* TODO: Error handling */
		purple_debug_error("jabber", "SCRAM server signature did not match!\n");
		return FALSE;
	}

	/* Hooray */
	return TRUE;
}

static void scram_dispose(JabberStream *js)
{
	if (js->auth_mech_data) {
		JabberScramData *data = js->auth_mech_data;

		g_free(data->cnonce);
		if (data->auth_message)
			g_string_free(data->auth_message, TRUE);
		if (data->client_proof)
			g_string_free(data->client_proof, TRUE);
		if (data->server_signature)
			g_string_free(data->server_signature, TRUE);
		g_free(data);
		js->auth_mech_data = NULL;
	}
}

static JabberSaslMech scram_sha1_mech = {
	50, /* priority */
	"SCRAM-SHA-1", /* name */
	scram_start,
	scram_handle_challenge,
	scram_handle_success,
	NULL, /* handle_failure */
	scram_dispose
};

#ifdef CHANNEL_BINDING
/* With channel binding */
static JabberSaslMech scram_sha1_plus_mech = {
	scram_sha1_mech.priority + 1, /* priority */
	"SCRAM-SHA-1-PLUS", /* name */
	scram_start,
	scram_handle_challenge,
	scram_handle_success,
	NULL, /* handle_failure */
	scram_dispose
};
#endif

/* For tests */
JabberSaslMech *jabber_scram_get_sha1(void)
{
	return &scram_sha1_mech;
}

JabberSaslMech **jabber_auth_get_scram_mechs(gint *count)
{
	static JabberSaslMech *mechs[] = {
		&scram_sha1_mech,
#ifdef CHANNEL_BINDING
		&scram_sha1_plus_mech,
#endif
	};

	g_return_val_if_fail(count != NULL, NULL);

	*count = G_N_ELEMENTS(mechs);
	return mechs;
}
