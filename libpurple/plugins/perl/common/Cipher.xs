#include "module.h"

MODULE = Purple::Cipher  PACKAGE = Purple::Cipher  PREFIX = purple_cipher_
PROTOTYPES: ENABLE

BOOT:
{
	HV *stash = gv_stashpv("Purple::Cipher::BatchMode", 1);

	static const constiv *civ, const_iv[] = {
#define const_iv(name) {#name, (IV)PURPLE_CIPHER_BATCH_MODE_##name}
		const_iv(ECB),
		const_iv(CBC),
#undef const_iv
	};

	for (civ = const_iv + sizeof(const_iv) / sizeof(const_iv[0]); civ-- > const_iv; )
		newCONSTSUB(stash, (char *)civ->name, newSViv(civ->iv));
}

const gchar *
purple_cipher_get_name(cipher)
	Purple::Cipher cipher

gchar_own*
purple_http_digest_calculate_response(algorithm, method, digest_uri, qop, entity, nonce, nonce_count, client_nonce, session_key)
	const gchar* algorithm
	const gchar* method
	const gchar* digest_uri
	const gchar* qop
	const gchar* entity
	const gchar* nonce
	const gchar* nonce_count
	const gchar* client_nonce
	const gchar* session_key

gchar_own*
purple_http_digest_calculate_session_key(algorithm, username, realm, password, nonce, client_nonce)
	const gchar* algorithm
	const gchar* username
	const gchar* realm
	const gchar* password
	const gchar* nonce
	const gchar* client_nonce

void
purple_cipher_reset(cipher)
	Purple::Cipher cipher

void
purple_cipher_set_iv(Purple::Cipher cipher, guchar *iv, size_t length(iv))
	PROTOTYPE: $$

void
purple_cipher_append(Purple::Cipher cipher, guchar *data, size_t length(data))
	PROTOTYPE: $$

gboolean
purple_cipher_digest(cipher, digest)
	Purple::Cipher cipher
	SV *digest
	PREINIT:
		guchar *buff = NULL;
		size_t digest_size;
	CODE:
		digest_size = purple_cipher_get_digest_size(cipher);
		SvUPGRADE(digest, SVt_PV);
		buff = (guchar *)SvGROW(digest, digest_size);
		if (purple_cipher_digest(cipher, buff, digest_size)) {
			SvCUR_set(digest, digest_size);
			SvPOK_only(digest);
			RETVAL = 1;
		} else {
			SvSetSV_nosteal(digest, &PL_sv_undef);
			RETVAL = 0;
		}
	OUTPUT:
		RETVAL

gboolean
purple_cipher_digest_to_str(cipher, digest_s)
	Purple::Cipher cipher
	SV *digest_s
	PREINIT:
		gchar *buff = NULL;
		size_t digest_size, str_len;
	CODE:
		digest_size = purple_cipher_get_digest_size(cipher);
		str_len = 2 * digest_size;
		SvUPGRADE(digest_s, SVt_PV);
		buff = SvGROW(digest_s, str_len + 1);
		if (purple_cipher_digest_to_str(cipher, buff, str_len + 1)) {
			SvCUR_set(digest_s, str_len);
			SvPOK_only(digest_s);
			RETVAL = 1;
		} else {
			SvSetSV_nosteal(digest_s, &PL_sv_undef);
			RETVAL = 0;
		}
	OUTPUT:
		RETVAL

gboolean
purple_cipher_encrypt(cipher, input, output)
	Purple::Cipher cipher
	SV *input
	SV *output
	PREINIT:
		size_t input_len, output_len;
		ssize_t ret;
		guchar *buff = NULL;
		guchar *data = NULL;
	CODE:
		data = (guchar *)SvPV(input, input_len);
		output_len = input_len + purple_cipher_get_block_size(cipher);
		SvUPGRADE(output, SVt_PV);
		buff = (guchar *)SvGROW(output, output_len);
		ret = purple_cipher_encrypt(cipher, data, input_len, buff, output_len);
		if (ret >= 0) {
			RETVAL = 1;
			SvPOK_only(output);
			SvCUR_set(output, ret);
		} else {
			RETVAL = 0;
			SvSetSV_nosteal(output, &PL_sv_undef);
		}
	OUTPUT:
		RETVAL

gboolean
purple_cipher_decrypt(cipher, input, output)
	Purple::Cipher cipher
	SV *input
	SV *output
	PREINIT:
		size_t input_len, output_len;
		ssize_t ret;
		guchar *buff = NULL;
		guchar *data = NULL;
	CODE:
		data = (guchar *)SvPV(input, input_len);
		output_len = input_len + purple_cipher_get_block_size(cipher);
		SvUPGRADE(output, SVt_PV);
		buff = (guchar *)SvGROW(output, output_len);
		ret = purple_cipher_decrypt(cipher, data, input_len, buff, output_len);
		if (ret >= 0) {
			RETVAL = 1;
			SvPOK_only(output);
			SvCUR_set(output, ret);
		} else {
			RETVAL = 0;
			SvSetSV_nosteal(output, &PL_sv_undef);
		}
	OUTPUT:
		RETVAL

void
purple_cipher_set_salt(cipher, salt, len)
	Purple::Cipher cipher
	guchar *salt
	size_t len

void
purple_cipher_set_key(cipher, key, len)
	Purple::Cipher cipher
	guchar *key
	size_t len

size_t
purple_cipher_get_key_size(cipher)
	Purple::Cipher cipher

Purple::Cipher::BatchMode
purple_cipher_get_batch_mode(cipher)
	Purple::Cipher cipher

size_t
purple_cipher_get_block_size(cipher)
	Purple::Cipher cipher

void
purple_cipher_set_batch_mode(cipher, mode)
	Purple::Cipher cipher
	Purple::Cipher::BatchMode mode

MODULE = Purple::Cipher  PACKAGE = Purple::AESCipher  PREFIX = purple_aes_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_aes_cipher_new()

MODULE = Purple::Cipher  PACKAGE = Purple::DES3Cipher  PREFIX = purple_des3_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_des3_cipher_new()

MODULE = Purple::Cipher  PACKAGE = Purple::DESCipher  PREFIX = purple_des_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_des_cipher_new()

MODULE = Purple::Cipher  PACKAGE = Purple::HMACCipher  PREFIX = purple_hmac_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_hmac_cipher_new()

MODULE = Purple::Cipher  PACKAGE = Purple::PBKDF2Cipher  PREFIX = purple_pbkdf2_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_pbkdf2_cipher_new()

MODULE = Purple::Cipher  PACKAGE = Purple::RC4Cipher  PREFIX = purple_rc4_cipher_
PROTOTYPES: ENABLE

Purple::Cipher
purple_rc4_cipher_new()
