
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


/** @enum mwCipherType
    Common cipher types */
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
    indicate the availability of this cipher

    @todo remove for 1.0
*/
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

  void (*offered)(struct mwCipherInstance *ci, struct mwEncryptItem *item);
  struct mwEncryptItem *(*offer)(struct mwCipherInstance *ci);
  void (*accepted)(struct mwCipherInstance *ci, struct mwEncryptItem *item);
  struct mwEncryptItem *(*accept)(struct mwCipherInstance *ci);

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


struct mwCipher *mwCipher_new_RC2_128(struct mwSession *s);


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


/** Indicates a cipher has been offered to our channel */
void mwCipherInstance_offered(struct mwCipherInstance *ci,
			      struct mwEncryptItem *item);


/** Offer a cipher */
struct mwEncryptItem *
mwCipherInstance_offer(struct mwCipherInstance *ci);


/** Indicates an offered cipher has been accepted */
void mwCipherInstance_accepted(struct mwCipherInstance *ci,
			       struct mwEncryptItem *item);


/** Accept a cipher offered to our channel */
struct mwEncryptItem *
mwCipherInstance_accept(struct mwCipherInstance *ci);


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

  These functions are reused where encryption is necessary outside of
  a channel (eg. session authentication)
*/
/* @{ */


/** generate some pseudo-random bytes
    @param keylen  count of bytes to write into key
    @param key     buffer to write keys into
*/
void mwKeyRandom(unsigned char *key, gsize keylen);


/** Setup an Initialization Vector. IV must be at least 8 bytes */
void mwIV_init(unsigned char *iv);


/** Expand a variable-length key into a 128-byte key (represented as
    an an array of 64 ints) */
void mwKeyExpand(int *ekey, const char *key, gsize keylen);


/** Encrypt data using an already-expanded key */
void mwEncryptExpanded(const int *ekey, unsigned char *iv,
		       struct mwOpaque *in,
		       struct mwOpaque *out);


/** Encrypt data using an expanded form of the given key */
void mwEncrypt(const char *key, gsize keylen, unsigned char *iv,
	       struct mwOpaque *in, struct mwOpaque *out);


/** Decrypt data using an already expanded key */
void mwDecryptExpanded(const int *ekey, unsigned char *iv,
		       struct mwOpaque *in,
		       struct mwOpaque *out);


/** Decrypt data using an expanded form of the given key */
void mwDecrypt(const char *key, gsize keylen, unsigned char *iv,
	       struct mwOpaque *in, struct mwOpaque *out);


/* @} */


/**
  @section Diffie-Hellman Functions

  These functions are reused where DH Key negotiation is necessary
  outside of a channel (eg. session authentication). These are
  wrapping a full multiple-precision integer math library, but most of
  the functionality there-of is not exposed. Currently, the math is
  provided by a copy of the public domain libmpi.

  for more information on the used MPI Library, visit
  http://www.cs.dartmouth.edu/~sting/mpi/
*/
/* @{ */


/** @struct mwMpi */
struct mwMpi;


/** prepare a new mpi value */
struct mwMpi *mwMpi_new();


/** destroy an mpi value */
void mwMpi_free(struct mwMpi *i);


/** Import a value from an opaque */
void mwMpi_import(struct mwMpi *i, struct mwOpaque *o);


/** Export a value into an opaque */
void mwMpi_export(struct mwMpi *i, struct mwOpaque *o);


/** initialize and set a big integer to the Sametime Prime value */
void mwMpi_setDHPrime(struct mwMpi *i);


/** initialize and set a big integer to the Sametime Base value */
void mwMpi_setDHBase(struct mwMpi *i);


/** sets private to a randomly generated value, and calculates public
    using the Sametime Prime and Base */
void mwMpi_randDHKeypair(struct mwMpi *private, struct mwMpi *public);


/** sets the shared key value based on the remote and private keys,
    using the Sametime Prime and Base */
void mwMpi_calculateDHShared(struct mwMpi *shared, struct mwMpi *remote,
			     struct mwMpi *private);


/* @} */


#endif


