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

#include "aescipher.h"
#include "debug.h"
#include "enums.h"

#include <string.h>

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

/******************************************************************************
 * Structs
 *****************************************************************************/
#define PURPLE_AES_CIPHER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_AES_CIPHER, PurpleAESCipherPrivate))

typedef struct {
	guchar iv[PURPLE_AES_BLOCK_SIZE];
	guchar key[32];
	guint key_size;
	gboolean failure;
} PurpleAESCipherPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_NONE,
	PROP_BATCH_MODE,
	PROP_IV,
	PROP_KEY,
	PROP_LAST,
};

/******************************************************************************
 * Cipher Stuff
 *****************************************************************************/

typedef gboolean (*purple_aes_cipher_crypt_func)(
	const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size);

static void
purple_aes_cipher_reset(PurpleCipher *cipher)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);

	g_return_if_fail(priv != NULL);

	memset(priv->iv, 0, sizeof(priv->iv));
	memset(priv->key, 0, sizeof(priv->key));
	priv->key_size = 32; /* 256bit */
	priv->failure = FALSE;
}

static void
purple_aes_cipher_set_iv(PurpleCipher *cipher, guchar *iv, size_t len)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);

	if ((len > 0 && iv == NULL) ||
		(len != 0 && len != sizeof(priv->iv))) {
		purple_debug_error("cipher-aes", "invalid IV length\n");
		priv->failure = TRUE;
		return;
	}

	if (len == 0)
		memset(priv->iv, 0, sizeof(priv->iv));
	else
		memcpy(priv->iv, iv, len);

	g_object_notify(G_OBJECT(cipher), "iv");
}

static void
purple_aes_cipher_set_key(PurpleCipher *cipher, const guchar *key, size_t len)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);

	if ((len > 0 && key == NULL) ||
		(len != 0 && len != 16 && len != 24 && len != 32)) {
		purple_debug_error("cipher-aes", "invalid key length\n");
		priv->failure = TRUE;
		return;
	}

	priv->key_size = len;
	memset(priv->key, 0, sizeof(priv->key));
	if (len > 0)
		memcpy(priv->key, key, len);

	g_object_notify(G_OBJECT(cipher), "key");
}

static guchar *
purple_aes_cipher_pad_pkcs7(const guchar input[], size_t in_len, size_t *out_len)
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
purple_aes_cipher_unpad_pkcs7(guchar input[], size_t in_len)
{
	int padding_len, i;
	size_t out_len;

	g_return_val_if_fail(input != NULL, -1);
	g_return_val_if_fail(in_len > 0, -1);

	padding_len = input[in_len - 1];
	if (padding_len <= 0 || padding_len > PURPLE_AES_BLOCK_SIZE ||
		padding_len > in_len) {
		purple_debug_warning("cipher-aes",
			"Invalid padding length: %d (total %lu) - "
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
purple_aes_cipher_gnutls_crypt_init(guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32],
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
purple_aes_cipher_gnutls_encrypt(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	gnutls_cipher_hd_t handle;
	int ret;

	handle = purple_aes_cipher_gnutls_crypt_init(iv, key, key_size);
	if (handle == NULL)
		return FALSE;

	ret = gnutls_cipher_encrypt2(handle, (guchar *)input, len, output, len);
	gnutls_cipher_deinit(handle);

	if (ret != 0) {
		purple_debug_error("cipher-aes",
			"gnutls_cipher_encrypt2 failed: %d\n", ret);
		return FALSE;
	}

	return TRUE;
}

static gboolean
purple_aes_cipher_gnutls_decrypt(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	gnutls_cipher_hd_t handle;
	int ret;

	handle = purple_aes_cipher_gnutls_crypt_init(iv, key, key_size);
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

#endif /* PURPLE_AES_USE_GNUTLS */

#ifdef PURPLE_AES_USE_NSS

typedef struct {
	PK11SlotInfo *slot;
	PK11SymKey *sym_key;
	SECItem *sec_param;
	PK11Context *enc_context;
} PurpleAESCipherNSSContext;

static void
purple_aes_cipher_nss_cleanup(PurpleAESCipherNSSContext *context)
{
	g_return_if_fail(context != NULL);

	if (context->enc_context != NULL)
		PK11_DestroyContext(context->enc_context, TRUE);
	if (context->sec_param != NULL)
		SECITEM_FreeItem(context->sec_param, TRUE);
	if (context->sym_key != NULL)
		PK11_FreeSymKey(context->sym_key);
	if (context->slot != NULL)
		PK11_FreeSlot(context->slot);

	memset(context, 0, sizeof(PurpleAESCipherNSSContext));
}

static gboolean
purple_aes_cipher_nss_crypt(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size,
	CK_ATTRIBUTE_TYPE operation)
{
	PurpleAESCipherNSSContext context;
	CK_MECHANISM_TYPE cipher_mech = CKM_AES_CBC;
	SECItem key_item, iv_item;
	SECStatus ret;
	int outlen = 0;
	unsigned int outlen_tmp = 0;

	memset(&context, 0, sizeof(PurpleAESCipherNSSContext));

	if (NSS_NoDB_Init(NULL) != SECSuccess) {
		purple_debug_error("cipher-aes",
			"NSS_NoDB_Init failed: %d\n", PR_GetError());
		return FALSE;
	}

	context.slot = PK11_GetBestSlot(cipher_mech, NULL);
	if (context.slot == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_GetBestSlot failed: %d\n", PR_GetError());
		return FALSE;
	}

	key_item.type = siBuffer;
	key_item.data = key;
	key_item.len = key_size;
	context.sym_key = PK11_ImportSymKey(context.slot, cipher_mech,
		PK11_OriginUnwrap, CKA_ENCRYPT, &key_item, NULL);
	if (context.sym_key == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_ImportSymKey failed: %d\n", PR_GetError());
		purple_aes_cipher_nss_cleanup(&context);
		return FALSE;
	}

	iv_item.type = siBuffer;
	iv_item.data = iv;
	iv_item.len = PURPLE_AES_BLOCK_SIZE;
	context.sec_param = PK11_ParamFromIV(cipher_mech, &iv_item);
	if (context.sec_param == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_ParamFromIV failed: %d\n", PR_GetError());
		purple_aes_cipher_nss_cleanup(&context);
		return FALSE;
	}

	context.enc_context = PK11_CreateContextBySymKey(cipher_mech, operation,
		context.sym_key, context.sec_param);
	if (context.enc_context == NULL) {
		purple_debug_error("cipher-aes",
			"PK11_CreateContextBySymKey failed: %d\n",
				PR_GetError());
		purple_aes_cipher_nss_cleanup(&context);
		return FALSE;
	}

	ret = PK11_CipherOp(context.enc_context, output, &outlen, len, input, len);
	if (ret != SECSuccess) {
		purple_debug_error("cipher-aes",
			"PK11_CipherOp failed: %d\n", PR_GetError());
		purple_aes_cipher_nss_cleanup(&context);
		return FALSE;
	}

	ret = PK11_DigestFinal(context.enc_context, output + outlen, &outlen_tmp,
		len - outlen);
	if (ret != SECSuccess) {
		purple_debug_error("cipher-aes",
			"PK11_DigestFinal failed: %d\n", PR_GetError());
		purple_aes_cipher_nss_cleanup(&context);
		return FALSE;
	}

	purple_aes_cipher_nss_cleanup(&context);

	outlen += outlen_tmp;
	if (outlen != len) {
		purple_debug_error("cipher-aes",
			"resulting length doesn't match: %d (expected: %lu)\n",
			outlen, len);
		return FALSE;
	}

	return TRUE;
}

static gboolean
purple_aes_cipher_nss_encrypt(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	return purple_aes_cipher_nss_crypt(input, output, len, iv, key, key_size,
		CKA_ENCRYPT);
}

static gboolean
purple_aes_cipher_nss_decrypt(const guchar *input, guchar *output, size_t len,
	guchar iv[PURPLE_AES_BLOCK_SIZE], guchar key[32], guint key_size)
{
	return purple_aes_cipher_nss_crypt(input, output, len, iv, key, key_size,
		CKA_DECRYPT);
}

#endif /* PURPLE_AES_USE_NSS */

static ssize_t
purple_aes_cipher_encrypt(PurpleCipher *cipher, const guchar input[],
	size_t in_len, guchar output[], size_t out_size)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);
	purple_aes_cipher_crypt_func encrypt_func;
	guchar *input_padded;
	size_t out_len = 0;
	gboolean succ;

	if (priv->failure)
		return -1;

	input_padded = purple_aes_cipher_pad_pkcs7(input, in_len, &out_len);

	if (out_len > out_size) {
		purple_debug_error("cipher-aes", "Output buffer too small\n");
		memset(input_padded, 0, out_len);
		g_free(input_padded);
		return -1;
	}

#if defined(PURPLE_AES_USE_GNUTLS)
	encrypt_func = purple_aes_cipher_gnutls_encrypt;
#elif defined(PURPLE_AES_USE_NSS)
	encrypt_func = purple_aes_cipher_nss_encrypt;
#else
#  error "No matching encrypt_func"
#endif

	succ = encrypt_func(input_padded, output, out_len, priv->iv,
		priv->key, priv->key_size);

	memset(input_padded, 0, out_len);
	g_free(input_padded);

	if (!succ) {
		memset(output, 0, out_len);
		return -1;
	}

	return out_len;
}

static ssize_t
purple_aes_cipher_decrypt(PurpleCipher *cipher, const guchar input[],
	size_t in_len, guchar output[], size_t out_size)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);
	purple_aes_cipher_crypt_func decrypt_func;
	gboolean succ;
	ssize_t out_len;

	if (priv->failure)
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
	decrypt_func = purple_aes_cipher_gnutls_decrypt;
#elif defined(PURPLE_AES_USE_NSS)
	decrypt_func = purple_aes_cipher_nss_decrypt;
#else
#  error "No matching encrypt_func"
#endif

	succ = decrypt_func(input, output, in_len, priv->iv, priv->key,
		priv->key_size);

	if (!succ) {
		memset(output, 0, in_len);
		return -1;
	}

	out_len = purple_aes_cipher_unpad_pkcs7(output, in_len);
	if (out_len < 0) {
		memset(output, 0, in_len);
		return -1;
	}

	return out_len;
}

static size_t
purple_aes_cipher_get_key_size(PurpleCipher *cipher)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);

	return priv->key_size;
}

static void
purple_aes_cipher_set_batch_mode(PurpleCipher *cipher,
	PurpleCipherBatchMode mode)
{
	PurpleAESCipherPrivate *priv = PURPLE_AES_CIPHER_GET_PRIVATE(cipher);

	if (mode == PURPLE_CIPHER_BATCH_MODE_CBC) {
		g_object_notify(G_OBJECT(cipher), "batch_mode");
		return;
	}

	purple_debug_error("cipher-aes", "unsupported batch mode\n");
	priv->failure = TRUE;
}

static PurpleCipherBatchMode
purple_aes_cipher_get_batch_mode(PurpleCipher *cipher)
{
	return PURPLE_CIPHER_BATCH_MODE_CBC;
}

static size_t
purple_aes_cipher_get_block_size(PurpleCipher *cipher)
{
	return PURPLE_AES_BLOCK_SIZE;
}

static const gchar*
purple_aes_cipher_get_name(PurpleCipher *cipher)
{
	return "aes";
}

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
purple_aes_cipher_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_BATCH_MODE:
			g_value_set_enum(value,
							 purple_cipher_get_batch_mode(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_aes_cipher_set_property(GObject *obj, guint param_id,
								const GValue *value, GParamSpec *pspec)
{
	PurpleCipher *cipher = PURPLE_CIPHER(obj);

	switch(param_id) {
		case PROP_BATCH_MODE:
			purple_cipher_set_batch_mode(cipher,
										 g_value_get_enum(value));
			break;
		case PROP_IV:
			{
				guchar *iv = (guchar *)g_value_get_string(value);
				purple_cipher_set_iv(cipher, iv, strlen((gchar*)iv));
			}
			break;
		case PROP_KEY:
			purple_cipher_set_key(cipher, (guchar *)g_value_get_string(value),
				purple_aes_cipher_get_key_size(cipher));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_aes_cipher_class_init(PurpleAESCipherClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleCipherClass *cipher_class = PURPLE_CIPHER_CLASS(klass);
	GParamSpec *pspec;

	obj_class->get_property = purple_aes_cipher_get_property;
	obj_class->set_property = purple_aes_cipher_set_property;

	cipher_class->reset = purple_aes_cipher_reset;
	cipher_class->set_iv = purple_aes_cipher_set_iv;
	cipher_class->encrypt = purple_aes_cipher_encrypt;
	cipher_class->decrypt = purple_aes_cipher_decrypt;
	cipher_class->set_key = purple_aes_cipher_set_key;
	cipher_class->get_key_size = purple_aes_cipher_get_key_size;
	cipher_class->set_batch_mode = purple_aes_cipher_set_batch_mode;
	cipher_class->get_batch_mode = purple_aes_cipher_get_batch_mode;
	cipher_class->get_block_size = purple_aes_cipher_get_block_size;
	cipher_class->get_name = purple_aes_cipher_get_name;

	pspec = g_param_spec_enum("batch_mode", "batch_mode", "batch_mode",
							  PURPLE_TYPE_CIPHER_BATCH_MODE, 0,
							  G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_BATCH_MODE, pspec);

	pspec = g_param_spec_string("iv", "iv", "iv", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_IV, pspec);

	pspec = g_param_spec_string("key", "key", "key", NULL,
								G_PARAM_WRITABLE);
	g_object_class_install_property(obj_class, PROP_KEY, pspec);

	g_type_class_add_private(klass, sizeof(PurpleAESCipherPrivate));
}

static void
purple_aes_cipher_init(PurpleCipher *cipher) {
	purple_cipher_reset(cipher);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
purple_aes_cipher_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleAESCipherClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_aes_cipher_class_init,
			NULL,
			NULL,
			sizeof(PurpleAESCipher),
			0,
			(GInstanceInitFunc)purple_aes_cipher_init,
			NULL
		};

		type = g_type_register_static(PURPLE_TYPE_CIPHER,
									  "PurpleAESCipher",
									  &info, 0);
	}

	return type;
}

PurpleCipher *
purple_aes_cipher_new(void) {
	return g_object_new(PURPLE_TYPE_AES_CIPHER, NULL);
}
