#include "internal.h"
#include <cipher.h>
#include "ciphers.h"

static void
purple_g_checksum_init(PurpleCipherContext *context, GChecksumType type)
{
    GChecksum *checksum;

    checksum = g_checksum_new(type);
    purple_cipher_context_set_data(context, checksum);
}

static void
purple_g_checksum_reset(PurpleCipherContext *context, GChecksumType type)
{
    GChecksum *checksum;

    checksum = purple_cipher_context_get_data(context);
    g_return_if_fail(checksum != NULL);

    g_checksum_reset(checksum);
}

static void
purple_g_checksum_uninit(PurpleCipherContext *context)
{
    GChecksum *checksum;

    checksum = purple_cipher_context_get_data(context);
    g_return_if_fail(checksum != NULL);

    g_checksum_free(checksum);
}

static void
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

static gboolean
purple_g_checksum_digest(PurpleCipherContext *context, GChecksumType type,
                         guchar *digest, size_t buff_len)
{
    GChecksum *checksum;
    const gssize required_len = g_checksum_type_get_length(type);
    gsize digest_len = buff_len;

    checksum = purple_cipher_context_get_data(context);

    g_return_val_if_fail(buff_len >= required_len, FALSE);
    g_return_val_if_fail(checksum != NULL, FALSE);

    g_checksum_get_digest(checksum, digest, &digest_len);

    if (digest_len != required_len)
        return FALSE;

    purple_cipher_context_reset(context, NULL);

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
	lower##_digest(PurpleCipherContext *context, guchar digest[], \
		size_t len) \
	{ \
		return purple_g_checksum_digest(context, (type), digest, len); \
	} \
	\
	static size_t \
	lower##_get_digest_size(PurpleCipherContext *context) \
	{ \
		return g_checksum_type_get_length((type)); \
	} \
	\
	static PurpleCipherOps camel##Ops = { \
		NULL,                     /* Set option */       \
		NULL,                     /* Get option */       \
		lower##_init,             /* init */             \
		lower##_reset,            /* reset */            \
		purple_g_checksum_uninit, /* uninit */           \
		NULL,                     /* set iv */           \
		purple_g_checksum_append, /* append */           \
		lower##_digest,           /* digest */           \
		lower##_get_digest_size,  /* get digest size */  \
		NULL,                     /* encrypt */          \
		NULL,                     /* decrypt */          \
		NULL,                     /* set salt */         \
		NULL,                     /* get salt size */    \
		NULL,                     /* set key */          \
		NULL,                     /* get key size */     \
		NULL,                     /* set batch mode */   \
		NULL,                     /* get batch mode */   \
		lower##_get_block_size,   /* get block size */   \
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
