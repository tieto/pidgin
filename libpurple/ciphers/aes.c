/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 *
 * Written by Tomek Wasilczyk <tomkiewicz@cpw.pidgin.im>
 */

#include "internal.h"
#include "cipher.h"
#include "ciphers.h"
#include "debug.h"

#if defined(HAVE_GNUTLS)
#  define PURPLE_AES_USE_GNUTLS 1
#  include <gnutls/gnutls.h>
#  include <gnutls/crypto.h>
#elif defined(HAVE_NSS)
#  define PURPLE_AES_USE_NSS 1
#  include <nss.h>
#  include <pk11pub.h>
#  include <prerror.h>
#else
#  error "No GnuTLS or NSS support"
#endif

/* 128bit */
#define PURPLE_AES_BLOCK_SIZE 16

typedef struct
{
	guchar iv[PURPLE_AES_BLOCK_SIZE];
	guchar key[32];
	guint key_size;
	gboolean failure;
} AESContext;

typedef gboolean (*purple_aes_crypt_func)(
	const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size);

static void
purple_aes_init(PurpleCipherContext *context, void *extra)
{
	AESContext *ctx_data;

	ctx_data = g_new0(AESContext, 1);
	purple_cipher_context_set_data(context, ctx_data);

	purple_cipher_context_reset(context, extra);
}

static void
purple_aes_uninit(PurpleCipherContext *context)
{
	AESContext *ctx_data;

	purple_cipher_context_reset(context, NULL);

	ctx_data = purple_cipher_context_get_data(context);
	g_free(ctx_data);
	purple_cipher_context_set_data(context, NULL);
}

static void
purple_aes_reset(PurpleCipherContext *context, void *extra)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	g_return_if_fail(ctx_data != NULL);

	memset(ctx_data->iv, 0, sizeof(ctx_data->iv));
	memset(ctx_data->key, 0, sizeof(ctx_data->key));
	ctx_data->key_size = 32; /* 256bit */
	ctx_data->failure = FALSE;
}

static void
purple_aes_set_option(PurpleCipherContext *context, const gchar *name,
	void *value)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	purple_debug_error("cipher-aes", "set_option not supported\n");
	ctx_data->failure = TRUE;
}

static void
purple_aes_set_iv(PurpleCipherContext *context, guchar *iv, size_t len)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	if ((len > 0 && iv == NULL) ||
		(len != 0 && len != sizeof(ctx_data->iv))) {
		purple_debug_error("cipher-aes", "invalid IV length\n");
		ctx_data->failure = TRUE;
		return;
	}

	if (len == 0)
		memset(ctx_data->iv, 0, sizeof(ctx_data->iv));
	else
		memcpy(ctx_data->iv, iv, len);
}

static void
purple_aes_set_key(PurpleCipherContext *context, const guchar *key, size_t len)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	if ((len > 0 && key == NULL) ||
		(len != 0 && len != 16 && len != 24 && len != 32)) {
		purple_debug_error("cipher-aes", "invalid key length\n");
		ctx_data->failure = TRUE;
		return;
	}

	ctx_data->key_size = len;
	memset(ctx_data->key, 0, sizeof(ctx_data->key));
	if (len > 0)
		memcpy(ctx_data->key, key, len);
}

static guchar *
purple_aes_pad_pkcs7(const guchar input[], size_t in_len, size_t *out_len)
{
	int padding_len, total_len;
	guchar *padded;

	g_return_val_if_fail(input != NULL, NULL);
	g_return_val_if_fail(out_len != NULL, NULL);

	padding_len = PURPLE_AES_BLOCK_SIZE - (in_len % PURPLE_AES_BLOCK_SIZE);
	total_len = in_len + padding_len;
	g_assert((total_len % PURPLE_AES_BLOCK_SIZE) == 0);

	padded = g_new(guchar, total_len);
	*out_len = total_len;

	memcpy(padded, input, in_len);
	memset(padded + in_len, padding_len, padding_len);

	return padded;
}

static ssize_t
purple_aes_unpad_pkcs7(guchar input[], size_t in_len)
{
	int padding_len, i;
	size_t out_len;

	g_return_val_if_fail(input != NULL, -1);
	g_return_val_if_fail(in_len > 0, -1);

	padding_len = input[in_len - 1];
	if (padding_len <= 0 || padding_len > PURPLE_AES_BLOCK_SIZE ||
		padding_len > in_len) {
		purple_debug_warning("cipher-aes",
			"Invalid padding length: %d (total %" G_GSIZE_FORMAT ") - "
			"most probably, the key was invalid\n",
			padding_len, in_len);
		return -1;
	}

	out_len = in_len - padding_len;
	for (i = 0; i < padding_len; i++) {
		if (input[out_len + i] != padding_len) {
			purple_debug_warning("cipher-aes",
				"Padding doesn't match at pos %d (found %02x, "
				"expected %02x) - "
				"most probably, the key was invalid\n",
				i, input[out_len + i], padding_len);
			return -1;
		}
	}

	memset(input + out_len, 0, padding_len);
	return out_len;
}

#ifdef PURPLE_AES_USE_GNUTLS

static gnutls_cipher_hd_t
purple_aes_crypt_gnutls_init(guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32],
	guint key_size)
{
	gnutls_cipher_hd_t handle;
	gnutls_cipher_algorithm_t algorithm;
	gnutls_datum_t key_info, iv_info;
	int ret;

	if (key_size == 16)
		algorithm = GNUTLS_CIPHER_AES_128_CBC;
	else if (key_size == 24)
		algorithm = GNUTLS_CIPHER_AES_192_CBC;
	else if (key_size == 32)
		algorithm = GNUTLS_CIPHER_AES_256_CBC;
	else
		g_return_val_if_reached(NULL);

	key_info.data = key;
	key_info.size = key_size;

	iv_info.data = iv;
	iv_info.size = PURPLE_AES_BLOCK_SIZE;

	ret = gnutls_cipher_init(&handle, algorithm, &key_info, &iv_info);
	if (ret != 0) {
		purple_debug_error("cipher-aes",
			"gnutls_cipher_init failed: %d\n", ret);
		return NULL;
	}

	return handle;
}

static gboolean
purple_aes_encrypt_gnutls(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	gnutls_cipher_hd_t handle;
	int ret;

	handle = purple_aes_crypt_gnutls_init(iv, key, key_size);
	if (handle == NULL)
		return FALSE;

	ret = gnutls_cipher_encrypt2(handle, (void *)input, len, output, len);
	gnutls_cipher_deinit(handle);

	if (ret != 0) {
		purple_debug_error("cipher-aes",
			"gnutls_cipher_encrypt2 failed: %d\n", ret);
		return FALSE;
	}

	return TRUE;
}

static gboolean
purple_aes_decrypt_gnutls(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	gnutls_cipher_hd_t handle;
	int ret;

	handle = purple_aes_crypt_gnutls_init(iv, key, key_size);
	if (handle == NULL)
		return FALSE;

	ret = gnutls_cipher_decrypt2(handle, input, len, output, len);
	gnutls_cipher_deinit(handle);

	if (ret != 0) {
		purple_debug_error("cipher-aes",
			"gnutls_cipher_decrypt2 failed: %d\n", ret);
		return FALSE;
	}

	return TRUE;
}

#elif defined(PURPLE_AES_USE_NSS)

typedef struct
{
	PK11SlotInfo *slot;
	PK11SymKey *sym_key;
	SECItem *sec_param;
	PK11Context *enc_context;
} purple_aes_encrypt_nss_context;

static void
purple_aes_encrypt_nss_context_cleanup(purple_aes_encrypt_nss_context *ctx)
{
	g_return_if_fail(ctx != NULL);

	if (ctx->enc_context != NULL)
		PK11_DestroyContext(ctx->enc_context, TRUE);
	if (ctx->sec_param != NULL)
		SECITEM_FreeItem(ctx->sec_param, TRUE);
	if (ctx->sym_key != NULL)
		PK11_FreeSymKey(ctx->sym_key);
	if (ctx->slot != NULL)
		PK11_FreeSlot(ctx->slot);

	memset(ctx, 0, sizeof(purple_aes_encrypt_nss_context));
}

static gboolean
purple_aes_crypt_nss(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size,
	CK_ATTRIBUTE_TYPE operation)
{
	purple_aes_encrypt_nss_context ctx;
	CK_MECHANISM_TYPE cipher_mech = CKM_AES_CBC;
	SECItem key_item, iv_item;
	SECStatus ret;
	int outlen = 0;
	unsigned int outlen_tmp = 0;

	memset(&ctx, 0, sizeof(purple_aes_encrypt_nss_context));

	if (NSS_NoDB_Init(NULL) != SECSuccess) {
		purple_debug_error("cipher-aes",
			"NSS_NoDB_Init failed: %d\n", PR_GetError());
		return FALSE;
	}

	ctx.slot = PK11_GetBestSlot(cipher_mech, NULL);
	if (ctx.slot == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_GetBestSlot failed: %d\n", PR_GetError());
		return FALSE;
	}

	key_item.type = siBuffer;
	key_item.data = key;
	key_item.len = key_size;
	ctx.sym_key = PK11_ImportSymKey(ctx.slot, cipher_mech,
		PK11_OriginUnwrap, CKA_ENCRYPT, &key_item, NULL);
	if (ctx.sym_key == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_ImportSymKey failed: %d\n", PR_GetError());
		purple_aes_encrypt_nss_context_cleanup(&ctx);
		return FALSE;
	}

	iv_item.type = siBuffer;
	iv_item.data = iv;
	iv_item.len = PURPLE_AES_BLOCK_SIZE;
	ctx.sec_param = PK11_ParamFromIV(cipher_mech, &iv_item);
	if (ctx.sec_param == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_ParamFromIV failed: %d\n", PR_GetError());
		purple_aes_encrypt_nss_context_cleanup(&ctx);
		return FALSE;
	}

	ctx.enc_context = PK11_CreateContextBySymKey(cipher_mech, operation,
		ctx.sym_key, ctx.sec_param);
	if (ctx.enc_context == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_CreateContextBySymKey failed: %d\n",
				PR_GetError());
		purple_aes_encrypt_nss_context_cleanup(&ctx);
		return FALSE;
	}

	ret = PK11_CipherOp(ctx.enc_context, output, &outlen, len, input, len);
	if (ret != SECSuccess) {
		purple_debug_error("cipher-aes",
			"PK11_CipherOp failed: %d\n", PR_GetError());
		purple_aes_encrypt_nss_context_cleanup(&ctx);
		return FALSE;
	}

	ret = PK11_DigestFinal(ctx.enc_context, output + outlen, &outlen_tmp,
		len - outlen);
	if (ret != SECSuccess) {
		purple_debug_error("cipher-aes",
			"PK11_DigestFinal failed: %d\n", PR_GetError());
		purple_aes_encrypt_nss_context_cleanup(&ctx);
		return FALSE;
	}

	purple_aes_encrypt_nss_context_cleanup(&ctx);

	outlen += outlen_tmp;
	if (outlen != len) {
		purple_debug_error("cipher-aes",
			"resulting length doesn't match: %d (expected: %d)\n",
			outlen, len);
		return FALSE;
	}

	return TRUE;
}

static gboolean
purple_aes_encrypt_nss(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	return purple_aes_crypt_nss(input, output, len, iv, key, key_size,
		CKA_ENCRYPT);
}

static gboolean
purple_aes_decrypt_nss(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	return purple_aes_crypt_nss(input, output, len, iv, key, key_size,
		CKA_DECRYPT);
}

#endif /* PURPLE_AES_USE_NSS */

static ssize_t
purple_aes_encrypt(PurpleCipherContext *context, const guchar input[],
	size_t in_len, guchar output[], size_t out_size)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);
	purple_aes_crypt_func encrypt_func;
	guchar *input_padded;
	size_t out_len = 0;
	gboolean succ;

	if (ctx_data->failure)
		return -1;

	input_padded = purple_aes_pad_pkcs7(input, in_len, &out_len);

	if (out_len > out_size) {
		purple_debug_error("cipher-aes", "Output buffer too small\n");
		memset(input_padded, 0, out_len);
		g_free(input_padded);
		return -1;
	}

#if defined(PURPLE_AES_USE_GNUTLS)
	encrypt_func = purple_aes_encrypt_gnutls;
#elif defined(PURPLE_AES_USE_NSS)
	encrypt_func = purple_aes_encrypt_nss;
#else
#  error "No matching encrypt_func"
#endif

	succ = encrypt_func(input_padded, output, out_len, ctx_data->iv,
		ctx_data->key, ctx_data->key_size);

	memset(input_padded, 0, out_len);
	g_free(input_padded);

	if (!succ) {
		memset(output, 0, out_len);
		return -1;
	}

	return out_len;
}

static ssize_t
purple_aes_decrypt(PurpleCipherContext *context, const guchar input[],
	size_t in_len, guchar output[], size_t out_size)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);
	purple_aes_crypt_func decrypt_func;
	gboolean succ;
	ssize_t out_len;

	if (ctx_data->failure)
		return -1;

	if (in_len > out_size) {
		purple_debug_error("cipher-aes", "Output buffer too small\n");
		return -1;
	}

	if ((in_len % PURPLE_AES_BLOCK_SIZE) != 0 || in_len == 0) {
		purple_debug_error("cipher-aes", "Malformed data\n");
		return -1;
	}

#if defined(PURPLE_AES_USE_GNUTLS)
	decrypt_func = purple_aes_decrypt_gnutls;
#elif defined(PURPLE_AES_USE_NSS)
	decrypt_func = purple_aes_decrypt_nss;
#else
#  error "No matching encrypt_func"
#endif

	succ = decrypt_func(input, output, in_len, ctx_data->iv, ctx_data->key,
		ctx_data->key_size);

	if (!succ) {
		memset(output, 0, in_len);
		return -1;
	}

	out_len = purple_aes_unpad_pkcs7(output, in_len);
	if (out_len < 0) {
		memset(output, 0, in_len);
		return -1;
	}

	return out_len;
}

static size_t
purple_aes_get_key_size(PurpleCipherContext *context)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	return ctx_data->key_size;
}

static void
purple_aes_set_batch_mode(PurpleCipherContext *context,
	PurpleCipherBatchMode mode)
{
	AESContext *ctx_data = purple_cipher_context_get_data(context);

	if (mode == PURPLE_CIPHER_BATCH_MODE_CBC)
		return;

	purple_debug_error("cipher-aes", "unsupported batch mode\n");
	ctx_data->failure = TRUE;
}

static PurpleCipherBatchMode
purple_aes_get_batch_mode(PurpleCipherContext *context)
{
	return PURPLE_CIPHER_BATCH_MODE_CBC;
}

static size_t
purple_aes_get_block_size(PurpleCipherContext *context)
{
	return PURPLE_AES_BLOCK_SIZE;
}

static PurpleCipherOps AESOps = {
	purple_aes_set_option,     /* set_option */
	NULL,                      /* get_option */
	purple_aes_init,           /* init */
	purple_aes_reset,          /* reset */
	NULL,                      /* reset_state */
	purple_aes_uninit,         /* uninit */
	purple_aes_set_iv,         /* set_iv */
	NULL,                      /* append */
	NULL,                      /* digest */
	NULL,                      /* get_digest_size */
	purple_aes_encrypt,        /* encrypt */
	purple_aes_decrypt,        /* decrypt */
	NULL,                      /* set_salt */
	NULL,                      /* get_salt_size */
	purple_aes_set_key,        /* set_key */
	purple_aes_get_key_size,   /* get_key_size */
	purple_aes_set_batch_mode, /* set_batch_mode */
	purple_aes_get_batch_mode, /* get_batch_mode */
	purple_aes_get_block_size, /* get_block_size */
	NULL, NULL, NULL, NULL     /* reserved */
};

PurpleCipherOps *
purple_aes_cipher_get_ops(void) {
	return &AESOps;
}
