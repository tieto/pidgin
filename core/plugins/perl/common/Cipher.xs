#include "module.h"

MODULE = Gaim::Cipher  PACKAGE = Gaim::Cipher  PREFIX = gaim_cipher_
PROTOTYPES: ENABLE

const gchar *
gaim_cipher_get_name(cipher)
	Gaim::Cipher cipher

guint
gaim_cipher_get_capabilities(cipher)
	Gaim::Cipher cipher

gboolean
gaim_cipher_digest_region(name, data, data_len, in_len, digest, out_len)
	const gchar * name
	const guchar * data
	size_t data_len
	size_t in_len
	guchar &digest
	size_t * out_len

MODULE = Gaim::Cipher  PACKAGE = Gaim::Ciphers  PREFIX = gaim_ciphers_
PROTOTYPES: ENABLE

Gaim::Cipher
gaim_ciphers_find_cipher(name)
	gchar * name

Gaim::Cipher
gaim_ciphers_register_cipher(name, ops)
	gchar * name
	Gaim::Cipher::Ops ops

gboolean
gaim_ciphers_unregister_cipher(cipher)
	Gaim::Cipher cipher

void
gaim_ciphers_get_ciphers()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_ciphers_get_ciphers(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Cipher")));
	}

gpointer
gaim_ciphers_get_handle()

void
gaim_ciphers_init()

void
gaim_ciphers_uninit()

MODULE = Gaim::Cipher  PACKAGE = Gaim::Cipher::Context  PREFIX = gaim_cipher_context_
PROTOTYPES: ENABLE

void
gaim_cipher_context_set_option(context, name, value)
	Gaim::Cipher::Context context
	gchar *name
	gpointer value

gpointer
gaim_cipher_context_get_option(context, name)
	Gaim::Cipher::Context context
	gchar *name

Gaim::Cipher::Context
gaim_cipher_context_new(cipher, extra)
	Gaim::Cipher cipher
	void *extra

Gaim::Cipher::Context
gaim_cipher_context_new_by_name(name, extra)
	gchar *name
	void *extra

void
gaim_cipher_context_reset(context, extra)
	Gaim::Cipher::Context context
	gpointer extra

void
gaim_cipher_context_destroy(context)
	Gaim::Cipher::Context context

void
gaim_cipher_context_set_iv(context, iv, len)
	Gaim::Cipher::Context context
	guchar * iv
	size_t len

void
gaim_cipher_context_append(context, data, len)
	Gaim::Cipher::Context context
	guchar * data
	size_t len

gboolean
gaim_cipher_context_digest(context, in_len, digest, out_len)
	Gaim::Cipher::Context context
	size_t in_len
	guchar &digest
	size_t &out_len

gboolean
gaim_cipher_context_digest_to_str(context, in_len, digest_s, out_len)
	Gaim::Cipher::Context context
	size_t in_len
	gchar &digest_s
	size_t &out_len

gint
gaim_cipher_context_encrypt(context, data, len, output, outlen)
	Gaim::Cipher::Context context
	guchar &data
	size_t len
	guchar &output
	size_t &outlen

gint
gaim_cipher_context_decrypt(context, data, len, output, outlen)
	Gaim::Cipher::Context context
	guchar &data
	size_t len
	guchar &output
	size_t &outlen

void
gaim_cipher_context_set_salt(context, salt)
	Gaim::Cipher::Context context
	guchar *salt

size_t
gaim_cipher_context_get_salt_size(context)
	Gaim::Cipher::Context context

void
gaim_cipher_context_set_key(context, key)
	Gaim::Cipher::Context context
	guchar *key

size_t
gaim_cipher_context_get_key_size(context)
	Gaim::Cipher::Context context

void
gaim_cipher_context_set_data(context, data)
	Gaim::Cipher::Context context
	gpointer data

gpointer
gaim_cipher_context_get_data(context)
	Gaim::Cipher::Context context
