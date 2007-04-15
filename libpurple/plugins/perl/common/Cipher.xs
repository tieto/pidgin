#include "module.h"

MODULE = Purple::Cipher  PACKAGE = Purple::Cipher  PREFIX = purple_cipher_
PROTOTYPES: ENABLE

const gchar *
purple_cipher_get_name(cipher)
	Purple::Cipher cipher

guint
purple_cipher_get_capabilities(cipher)
	Purple::Cipher cipher

gboolean
purple_cipher_digest_region(name, data, data_len, in_len, digest, out_len)
	const gchar * name
	const guchar * data
	size_t data_len
	size_t in_len
	guchar &digest
	size_t * out_len

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

void
purple_ciphers_init()

void
purple_ciphers_uninit()

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
purple_cipher_context_new(cipher, extra)
	Purple::Cipher cipher
	void *extra

Purple::Cipher::Context
purple_cipher_context_new_by_name(name, extra)
	gchar *name
	void *extra

void
purple_cipher_context_reset(context, extra)
	Purple::Cipher::Context context
	gpointer extra

void
purple_cipher_context_destroy(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_iv(context, iv, len)
	Purple::Cipher::Context context
	guchar * iv
	size_t len

void
purple_cipher_context_append(context, data, len)
	Purple::Cipher::Context context
	guchar * data
	size_t len

gboolean
purple_cipher_context_digest(context, in_len, digest, out_len)
	Purple::Cipher::Context context
	size_t in_len
	guchar &digest
	size_t &out_len

gboolean
purple_cipher_context_digest_to_str(context, in_len, digest_s, out_len)
	Purple::Cipher::Context context
	size_t in_len
	gchar &digest_s
	size_t &out_len

gint
purple_cipher_context_encrypt(context, data, len, output, outlen)
	Purple::Cipher::Context context
	guchar &data
	size_t len
	guchar &output
	size_t &outlen

gint
purple_cipher_context_decrypt(context, data, len, output, outlen)
	Purple::Cipher::Context context
	guchar &data
	size_t len
	guchar &output
	size_t &outlen

void
purple_cipher_context_set_salt(context, salt)
	Purple::Cipher::Context context
	guchar *salt

size_t
purple_cipher_context_get_salt_size(context)
	Purple::Cipher::Context context

void
purple_cipher_context_set_key(context, key)
	Purple::Cipher::Context context
	guchar *key

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
