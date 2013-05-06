#include "module.h"

MODULE = Purple::Cipher  PACKAGE = Purple::Cipher  PREFIX = purple_cipher_
PROTOTYPES: ENABLE

BOOT:
{
	HV *stash = gv_stashpv("Purple::Cipher::BatchMode", 1);
	HV *cipher_caps = gv_stashpv("Purple::Cipher::Caps", 1);

	static const constiv *civ, const_iv[] = {
#define const_iv(name) {#name, (IV)PURPLE_CIPHER_BATCH_MODE_##name}
		const_iv(ECB),
		const_iv(CBC),
#undef const_iv
	};

	static const constiv bm_const_iv[] = {
#define const_iv(name) {#name, (IV)PURPLE_CIPHER_CAPS_##name}
		const_iv(SET_OPT),
		const_iv(GET_OPT),
		const_iv(INIT),
		const_iv(RESET),
		const_iv(UNINIT),
		const_iv(SET_IV),
		const_iv(APPEND),
		const_iv(DIGEST),
		const_iv(GET_DIGEST_SIZE),
		const_iv(ENCRYPT),
		const_iv(DECRYPT),
		const_iv(SET_SALT),
		const_iv(GET_SALT_SIZE),
		const_iv(SET_KEY),
		const_iv(GET_KEY_SIZE),
		const_iv(SET_BATCH_MODE),
		const_iv(GET_BATCH_MODE),
		const_iv(GET_BLOCK_SIZE),
		const_iv(UNKNOWN),
#undef const_iv
	};

	for (civ = const_iv + sizeof(const_iv) / sizeof(const_iv[0]); civ-- > const_iv; )
		newCONSTSUB(stash, (char *)civ->name, newSViv(civ->iv));

	for (civ = bm_const_iv + sizeof(bm_const_iv) / sizeof(bm_const_iv[0]); civ-- > bm_const_iv; )
		newCONSTSUB(cipher_caps, (char *)civ->name, newSViv(civ->iv));
}

const gchar *
purple_cipher_get_name(cipher)
	Purple::Cipher cipher

guint
purple_cipher_get_capabilities(cipher)
	Purple::Cipher cipher

gboolean
purple_cipher_digest_region(name, data_sv, data_len, digest)
	const gchar *name
	SV *data_sv
	size_t data_len
	SV *digest
	PREINIT:
		guchar *buff = NULL;
		guchar *data = NULL;
		ssize_t digest_len;
		size_t max_digest_len = 100;
	CODE:
		data = (guchar *)SvPV(data_sv, data_len);
		SvUPGRADE(digest, SVt_PV);
		buff = (guchar *)SvGROW(digest, max_digest_len);
		digest_len = purple_cipher_digest_region(name, data, data_len, buff, max_digest_len);
		if(digest_len == -1) {
			SvSetSV_nosteal(digest, &PL_sv_undef);
			RETVAL = 0;
		} else {
			SvCUR_set(digest, digest_len);
			SvPOK_only(digest);
			RETVAL = 1;
		}
	OUTPUT:
		RETVAL

gchar_own*
purple_cipher_http_digest_calculate_response(algorithm, method, digest_uri, qop, entity, nonce, nonce_count, client_nonce, session_key)
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
purple_cipher_http_digest_calculate_session_key(algorithm, username, realm, password, nonce, client_nonce)
	const gchar* algorithm
	const gchar* username
	const gchar* realm
	const gchar* password
	const gchar* nonce
	const gchar* client_nonce

MODULE = Purple::Cipher  PACKAGE = Purple::Ciphers  PREFIX = purple_ciphers_
PROTOTYPES: ENABLE

Purple::Cipher
purple_ciphers_find_cipher(name)
	gchar * name

Purple::Cipher
purple_ciphers_register_cipher(name, ops)
	gchar * name
	Purple::Cipher::Ops ops

gboolean
purple_ciphers_unregister_cipher(cipher)
	Purple::Cipher cipher

void
purple_ciphers_get_ciphers()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_ciphers_get_ciphers(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Cipher")));
	}

Purple::Handle
purple_ciphers_get_handle()

MODULE = Purple::Cipher  PACKAGE = Purple::Cipher::Context  PREFIX = purple_cipher_context_
PROTOTYPES: ENABLE

void
purple_cipher_context_set_option(context, name, value)
	Purple::Cipher::Context context
	gchar *name
	gpointer value

gpointer
purple_cipher_context_get_option(context, name)
	Purple::Cipher::Context context
	gchar *name

Purple::Cipher::Context
purple_cipher_context_new(klass, cipher, extra = NULL)
	Purple::Cipher cipher
	void *extra
	C_ARGS: cipher, extra

Purple::Cipher::Context
purple_cipher_context_new_by_name(klass, name, extra = NULL)
	gchar *name
	void *extra
	C_ARGS: name, extra

void
purple_cipher_context_reset(context, extra = NULL)
	Purple::Cipher::Context context
	gpointer extra

void
purple_cipher_context_destroy(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_iv(Purple::Cipher::Context context, guchar *iv, size_t length(iv))
	PROTOTYPE: $$

void
purple_cipher_context_append(Purple::Cipher::Context context, guchar *data, size_t length(data))
	PROTOTYPE: $$

gboolean
purple_cipher_context_digest(context, digest)
	Purple::Cipher::Context context
	SV *digest
	PREINIT:
		guchar *buff = NULL;
		size_t digest_size;
	CODE:
		digest_size = purple_cipher_context_get_digest_size(context);
		SvUPGRADE(digest, SVt_PV);
		buff = (guchar *)SvGROW(digest, digest_size);
		if (purple_cipher_context_digest(context, buff, digest_size)) {
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
purple_cipher_context_digest_to_str(context, digest_s)
	Purple::Cipher::Context context
	SV *digest_s
	PREINIT:
		gchar *buff = NULL;
		size_t digest_size, str_len;
	CODE:
		digest_size = purple_cipher_context_get_digest_size(context);
		str_len = 2 * digest_size;
		SvUPGRADE(digest_s, SVt_PV);
		buff = SvGROW(digest_s, str_len + 1);
		if (purple_cipher_context_digest_to_str(context, buff, str_len + 1)) {
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
purple_cipher_context_encrypt(context, input, output)
	Purple::Cipher::Context context
	SV *input
	SV *output
	PREINIT:
		size_t input_len, output_len;
		ssize_t ret;
		guchar *buff = NULL;
		guchar *data = NULL;
	CODE:
		data = (guchar *)SvPV(input, input_len);
		output_len = input_len + purple_cipher_context_get_block_size(context);
		SvUPGRADE(output, SVt_PV);
		buff = (guchar *)SvGROW(output, output_len);
		ret = purple_cipher_context_encrypt(context, data, input_len, buff, output_len);
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
purple_cipher_context_decrypt(context, input, output)
	Purple::Cipher::Context context
	SV *input
	SV *output
	PREINIT:
		size_t input_len, output_len;
		ssize_t ret;
		guchar *buff = NULL;
		guchar *data = NULL;
	CODE:
		data = (guchar *)SvPV(input, input_len);
		output_len = input_len + purple_cipher_context_get_block_size(context);
		SvUPGRADE(output, SVt_PV);
		buff = (guchar *)SvGROW(output, output_len);
		ret = purple_cipher_context_decrypt(context, data, input_len, buff, output_len);
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
purple_cipher_context_set_salt(context, salt, len)
	Purple::Cipher::Context context
	guchar *salt
	size_t len

size_t
purple_cipher_context_get_salt_size(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_key(context, key, len)
	Purple::Cipher::Context context
	guchar *key
	size_t len

size_t
purple_cipher_context_get_key_size(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_data(context, data)
	Purple::Cipher::Context context
	gpointer data

gpointer
purple_cipher_context_get_data(context)
	Purple::Cipher::Context context

Purple::Cipher::BatchMode
purple_cipher_context_get_batch_mode(context)
	Purple::Cipher::Context context

size_t
purple_cipher_context_get_block_size(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_batch_mode(context, mode)
	Purple::Cipher::Context context
	Purple::Cipher::BatchMode mode

