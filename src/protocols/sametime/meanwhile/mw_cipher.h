
/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _MW_CIPHER_H
#define _MW_CIPHER_H


#include <glib.h>
#include "mw_common.h"


/* place-holders */
struct mwChannel;
struct mwSession;



/** Common cipher types */
enum mwCipherType {
  mwCipher_RC2_40   = 0x0000,
  mwCipher_RC2_128  = 0x0001,
};


struct mwCipher;
struct mwCipherInstance;


/** Obtain an instance of a given cipher, which can be used for the
    processing of a single channel. */
typedef struct mwCipherInstance *(*mwCipherInstantiator)
     (struct mwCipher *cipher, struct mwChannel *chan);


/** Generate a descriptor for use in a channel create message to
    indicate the availability of this cipher */
typedef struct mwEncryptItem *(*mwCipherDescriptor)
     (struct mwCipherInstance *instance);


/** Process (encrypt or decrypt, depending) the given data. The passed
    buffer may be freed in processing and be replaced with a freshly
    allocated buffer. The post-processed buffer must in turn be freed
    after use */
typedef int (*mwCipherProcessor)
     (struct mwCipherInstance *ci, struct mwOpaque *data);


/** A cipher. Ciphers are primarily used to provide cipher instances
    for bi-directional encryption on channels, but some may be used
    for other activities. Expand upon this structure to create a
    custom encryption provider.
    @see mwCipherInstance */
struct mwCipher {

  /** service this cipher is providing for
      @see mwCipher_getSession */
  struct mwSession *session;

  guint16 type;               /**< @see mwCipher_getType */
  const char *(*get_name)();  /**< @see mwCipher_getName */
  const char *(*get_desc)();  /**< @see mwCipher_getDesc */

  /** Generate a new Cipher Instance for use on a channel
      @see mwCipher_newInstance */
  mwCipherInstantiator new_instance;

  /** @see mwCipher_newItem */
  mwCipherDescriptor new_item;

  void (*offered)(struct mwCipherInstance *ci, struct mwEncryptItem *item);
  void (*offer)(struct mwCipherInstance *ci);
  void (*accepted)(struct mwCipherInstance *ci, struct mwEncryptItem *item);
  void (*accept)(struct mwCipherInstance *ci);

  mwCipherProcessor encrypt; /**< @see mwCipherInstance_encrypt */
  mwCipherProcessor decrypt; /**< @see mwCipherInstance_decrypt */

  /** prepare this cipher for being free'd
      @see mwCipher_free */
  void (*clear)(struct mwCipher *c);

  /** clean up a cipher instance before being free'd
      @see mwCipherInstance_free */
  void (*clear_instance)(struct mwCipherInstance *ci);
};


/** An instance of a cipher. Expand upon this structure to contain
    necessary state data
    @see mwCipher */
struct mwCipherInstance {

  /** the parent cipher.
      @see mwCipherInstance_getCipher */
  struct mwCipher *cipher;

  /** the channel this instances processes
      @see mwCipherInstance_getChannel */
  struct mwChannel *channel;
};


struct mwCipher *mwCipher_new_RC2_40(struct mwSession *s);


#if 0
/* @todo write this */
struct mwCipher *mwCipher_new_DH_RC2_128(struct mwSession *s);
#endif


struct mwSession *mwCipher_getSession(struct mwCipher *cipher);


guint16 mwCipher_getType(struct mwCipher *cipher);


const char *mwCipher_getName(struct mwCipher *cipher);


const char *mwCipher_getDesc(struct mwCipher *cipher);


struct mwCipherInstance *mwCipher_newInstance(struct mwCipher *cipher,
					      struct mwChannel *channel);


/** destroy a cipher */
void mwCipher_free(struct mwCipher* cipher);


/** reference the parent cipher of an instance */
struct mwCipher *mwCipherInstance_getCipher(struct mwCipherInstance *ci);


struct mwEncryptItem *mwCipherInstance_newItem(struct mwCipherInstance *ci);


/** Indicates a cipher has been offered to our channel */
void mwCipherInstance_offered(struct mwCipherInstance *ci,
			      struct mwEncryptItem *item);


/** Offer a cipher */
void mwCipherInstance_offer(struct mwCipherInstance *ci);


/** Indicates an offered cipher has been accepted */
void mwCipherInstance_accepted(struct mwCipherInstance *ci,
			       struct mwEncryptItem *item);


/** Accept a cipher offered to our channel */
void mwCipherInstance_accept(struct mwCipherInstance *ci);


/** encrypt data */
int mwCipherInstance_encrypt(struct mwCipherInstance *ci,
			     struct mwOpaque *data);


/** decrypt data */
int mwCipherInstance_decrypt(struct mwCipherInstance *ci,
			     struct mwOpaque *data);


/** destroy a cipher instance */
void mwCipherInstance_free(struct mwCipherInstance *ci);


/**
  @section General Cipher Functions

  This set of functions is a broken sort of RC2 implementation. But it
  works with sametime, so we're all happy, right? Primary change to
  functionality appears in the mwKeyExpand function. Hypothetically,
  using a key expanded here (but breaking it into a 128-char array
  rather than 64 ints), one could pass it at that length to openssl
  and no further key expansion would occur.

  I'm not certain if replacing this with a wrapper for calls to some
  other crypto library is a good idea or not. Proven software versus
  added dependencies...
*/
/* @{ */


/** generate some pseudo-random bytes
    @param keylen  count of bytes to write into key
    @param key     buffer to write keys into
*/
void rand_key(char *key, gsize keylen);


/** Setup an Initialization Vector */
void mwIV_init(char *iv);


/** Expand a variable-length key into a 128-byte key (represented as
    an an array of 64 ints) */
void mwKeyExpand(int *ekey, const char *key, gsize keylen);


/** Encrypt data using an already-expanded key */
void mwEncryptExpanded(const int *ekey, char *iv,
		       struct mwOpaque *in,
		       struct mwOpaque *out);


/** Encrypt data using an expanded form of the given key */
void mwEncrypt(const char *key, gsize keylen, char *iv,
	       struct mwOpaque *in, struct mwOpaque *out);


/** Decrypt data using an already expanded key */
void mwDecryptExpanded(const int *ekey, char *iv,
		       struct mwOpaque *in,
		       struct mwOpaque *out);


/** Decrypt data using an expanded form of the given key */
void mwDecrypt(const char *key, gsize keylen, char *iv,
	       struct mwOpaque *in, struct mwOpaque *out);


/* @} */


#endif


