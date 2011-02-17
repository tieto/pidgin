#include <cipher.h>

#if GLIB_CHECK_VERSION(2,16,0)

void
purple_g_checksum_init(PurpleCipherContext *context, GChecksumType type)
{
    GChecksum *checksum;

    checksum = g_checksum_new(type);
    purple_cipher_context_set_data(context, checksum);
}

void
purple_g_checksum_reset(PurpleCipherContext *context, GChecksumType type)
{
    GChecksum *checksum;

    checksum = purple_cipher_context_get_data(context);
    g_return_if_fail(checksum != NULL);

#if GLIB_CHECK_VERSION(2,18,0)
    g_checksum_reset(checksum);
#else
    g_checksum_free(checksum);
    checksum = g_checksum_new(type);
    purple_cipher_context_set_data(context, checksum);
#endif
}

void
purple_g_checksum_uninit(PurpleCipherContext *context)
{
    GChecksum *checksum;

    checksum = purple_cipher_context_get_data(context);
    g_return_if_fail(checksum != NULL);

    g_checksum_free(checksum);
}

void
purple_g_checksum_append(PurpleCipherContext *context, const guchar *data,
                         gsize len)
{
    GChecksum *checksum;

    checksum = purple_cipher_context_get_data(context);
    g_return_if_fail(checksum != NULL);

    while (len >= G_MAXSSIZE) {
        g_checksum_update(checksum, data, G_MAXSSIZE);
        len -= G_MAXSSIZE;
        data += G_MAXSSIZE;
    }

    if (len)
        g_checksum_update(checksum, data, len);
}

gboolean
purple_g_checksum_digest(PurpleCipherContext *context, GChecksumType type,
                         gsize len, guchar *digest, gsize *out_len)
{
    GChecksum *checksum;
    const gssize required_length = g_checksum_type_get_length(type);

    checksum = purple_cipher_context_get_data(context);

    g_return_val_if_fail(len >= required_length, FALSE);
    g_return_val_if_fail(checksum != NULL, FALSE);

    g_checksum_get_digest(checksum, digest, &len);

    purple_cipher_context_reset(context, NULL);

    if (out_len)
        *out_len = len;

    return TRUE;
}

/******************************************************************************
 * Macros
 *****************************************************************************/
#define PURPLE_G_CHECKSUM_IMPLEMENTATION(lower, camel, type, block_size) \
	static size_t \
	lower##_get_block_size(PurpleCipherContext *context) { \
		return (block_size); \
	} \
	\
	static void \
	lower##_init(PurpleCipherContext *context, gpointer extra) { \
		purple_g_checksum_init(context, (type)); \
	} \
	\
	static void \
	lower##_reset(PurpleCipherContext *context, gpointer extra) { \
		purple_g_checksum_reset(context, (type)); \
	} \
	\
	static gboolean \
	lower##_digest(PurpleCipherContext *context, gsize in_len, \
	                 guchar digest[], size_t *out_len) \
	{ \
		return purple_g_checksum_digest(context, (type), in_len, digest, \
		                                out_len); \
	} \
	\
	static PurpleCipherOps camel##Ops = { \
		.init = lower##_init, \
		.reset = lower##_reset, \
		.uninit = purple_g_checksum_uninit, \
		.append = purple_g_checksum_append, \
		.digest = lower##_digest, \
		.get_block_size = lower##_get_block_size, \
	}; \
	\
	PurpleCipherOps * \
	purple_##lower##_cipher_get_ops(void) { \
		return &camel##Ops; \
	}

/******************************************************************************
 * Macro Expansion
 *****************************************************************************/
PURPLE_G_CHECKSUM_IMPLEMENTATION(md5, MD5, G_CHECKSUM_MD5, 64);
PURPLE_G_CHECKSUM_IMPLEMENTATION(sha1, SHA1, G_CHECKSUM_SHA1, 64);
PURPLE_G_CHECKSUM_IMPLEMENTATION(sha256, SHA256, G_CHECKSUM_SHA256, 64);

#endif /* GLIB_CHECK_VERSION(2,16,0) */

