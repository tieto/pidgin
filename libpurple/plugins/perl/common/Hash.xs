#include "module.h"

MODULE = Purple::Hash  PACKAGE = Purple::Hash  PREFIX = purple_hash_
PROTOTYPES: ENABLE

const gchar *
purple_hash_get_name(hash)
	Purple::Hash hash

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
purple_hash_reset(hash)
	Purple::Hash hash

void
purple_hash_set_iv(Purple::Hash hash, guchar *iv, size_t length(iv))
	PROTOTYPE: $$

void
purple_hash_append(Purple::Hash hash, guchar *data, size_t length(data))
	PROTOTYPE: $$

gboolean
purple_hash_digest(hash, digest)
	Purple::Hash hash
	SV *digest
	PREINIT:
		guchar *buff = NULL;
		size_t digest_size;
	CODE:
		digest_size = purple_hash_get_digest_size(hash);
		SvUPGRADE(digest, SVt_PV);
		buff = (guchar *)SvGROW(digest, digest_size);
		if (purple_hash_digest(hash, buff, digest_size)) {
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
purple_hash_digest_to_str(hash, digest_s)
	Purple::Hash hash
	SV *digest_s
	PREINIT:
		gchar *buff = NULL;
		size_t digest_size, str_len;
	CODE:
		digest_size = purple_hash_get_digest_size(hash);
		str_len = 2 * digest_size;
		SvUPGRADE(digest_s, SVt_PV);
		buff = SvGROW(digest_s, str_len + 1);
		if (purple_hash_digest_to_str(hash, buff, str_len + 1)) {
			SvCUR_set(digest_s, str_len);
			SvPOK_only(digest_s);
			RETVAL = 1;
		} else {
			SvSetSV_nosteal(digest_s, &PL_sv_undef);
			RETVAL = 0;
		}
	OUTPUT:
		RETVAL

size_t
purple_hash_get_block_size(hash)
	Purple::Hash hash
