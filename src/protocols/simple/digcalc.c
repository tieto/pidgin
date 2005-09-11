/*
 * Taken from RFC 2617
 *    Copyright (C) The Internet Society (1999).  All Rights Reserved.

   This document and translations of it may be copied and furnished to
   others, and derivative works that comment on or otherwise explain it
   or assist in its implementation may be prepared, copied, published
   and distributed, in whole or in part, without restriction of any
   kind, provided that the above copyright notice and this paragraph are
   included on all such copies and derivative works.  However, this
   document itself may not be modified in any way, such as by removing
   the copyright notice or references to the Internet Society or other
   Internet organizations, except as needed for the purpose of
   developing Internet standards in which case the procedures for
   copyrights defined in the Internet Standards process must be
   followed, or as required to translate it into languages other than
   English.

   The limited permissions granted above are perpetual and will not be
   revoked by the Internet Society or its successors or assigns.

   This document and the information contained herein is provided on an
   "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
   TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
   BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
   HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
*/
#include "cipher.h"

#include <string.h>
#include "digcalc.h"

void CvtHex(
    IN HASH Bin,
    OUT HASHHEX Hex
    )
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
         else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
         else
            Hex[i*2+1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
};

/* calculate H(A1) as per spec */
void DigestCalcHA1(
    IN char * pszAlg,
    IN char * pszUserName,
    IN char * pszRealm,
    IN char * pszPassword,
    IN char * pszNonce,
    IN char * pszCNonce,
    OUT HASHHEX SessionKey
    )
{
      GaimCipher *cipher;
      GaimCipherContext *context;
      HASH HA1;

      cipher = gaim_ciphers_find_cipher("md5");
      context = gaim_cipher_context_new(cipher, NULL);
      gaim_cipher_context_append(context, (guchar *)pszUserName, strlen(pszUserName));
      gaim_cipher_context_append(context, (guchar *)":", 1);
      gaim_cipher_context_append(context, (guchar *)pszRealm, strlen(pszRealm));
      gaim_cipher_context_append(context, (guchar *)":", 1);
      gaim_cipher_context_append(context, (guchar *)pszPassword, strlen(pszPassword));
      gaim_cipher_context_digest(context, sizeof(HA1), HA1, NULL);
      if (strcmp(pszAlg, "md5-sess") == 0) {
            context = gaim_cipher_context_new(cipher, NULL);
            gaim_cipher_context_append(context, HA1, HASHLEN);
            gaim_cipher_context_append(context, (guchar *)":", 1);
            gaim_cipher_context_append(context, (guchar *)pszNonce, strlen(pszNonce));
            gaim_cipher_context_append(context, (guchar *)":", 1);
            gaim_cipher_context_append(context, (guchar *)pszCNonce, strlen(pszCNonce));
            gaim_cipher_context_digest(context, sizeof(HA1), HA1, NULL);
      };
      CvtHex(HA1, SessionKey);
      gaim_cipher_context_destroy(context);
};

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DigestCalcResponse(
    IN HASHHEX HA1,           /* H(A1) */
    IN char * pszNonce,       /* nonce from server */
    IN char * pszNonceCount,  /* 8 hex digits */
    IN char * pszCNonce,      /* client nonce */
    IN char * pszQop,         /* qop-value: "", "auth", "auth-int" */
    IN char * pszMethod,      /* method from the request */
    IN char * pszDigestUri,   /* requested URL */
    IN HASHHEX HEntity,       /* H(entity body) if qop="auth-int" */
    OUT HASHHEX Response      /* request-digest or response-digest */
    )
{
      GaimCipher *cipher;
      GaimCipherContext *context;
      HASH HA2;
      HASH RespHash;
       HASHHEX HA2Hex;

      // calculate H(A2)
      cipher = gaim_ciphers_find_cipher("md5");
      context = gaim_cipher_context_new(cipher, NULL);
      gaim_cipher_context_append(context, (guchar *)pszMethod, strlen(pszMethod));
      gaim_cipher_context_append(context, (guchar *)":", 1);
      gaim_cipher_context_append(context, (guchar *)pszDigestUri, strlen(pszDigestUri));
      if (strcmp(pszQop, "auth-int") == 0) {
            gaim_cipher_context_append(context, (guchar *)":", 1);
            gaim_cipher_context_append(context, HEntity, HASHHEXLEN);
      };
      gaim_cipher_context_digest(context, sizeof(HA2), HA2, NULL);
       CvtHex(HA2, HA2Hex);

      gaim_cipher_context_destroy(context);
      // calculate response
      context = gaim_cipher_context_new(cipher, NULL);
      gaim_cipher_context_append(context, HA1, HASHHEXLEN);
      gaim_cipher_context_append(context, (guchar *)":", 1);
      gaim_cipher_context_append(context, (guchar *)pszNonce, strlen(pszNonce));
      gaim_cipher_context_append(context, (guchar *)":", 1);
      if (*pszQop) {
          gaim_cipher_context_append(context, (guchar *)pszNonceCount, strlen(pszNonceCount));
          gaim_cipher_context_append(context, (guchar *)":", 1);
          gaim_cipher_context_append(context, (guchar *)pszCNonce, strlen(pszCNonce));
          gaim_cipher_context_append(context, (guchar *)":", 1);
          gaim_cipher_context_append(context, (guchar *)pszQop, strlen(pszQop));
          gaim_cipher_context_append(context, (guchar *)":", 1);
      };
      gaim_cipher_context_append(context, HA2Hex, HASHHEXLEN);
      gaim_cipher_context_digest(context, sizeof(RespHash), RespHash, NULL);
      CvtHex(RespHash, Response);
      gaim_cipher_context_destroy(context);
};
