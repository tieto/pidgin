/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is SHA 180-1 Reference Implementation (Compact version)
 * 
 * The Initial Developer of the Original Code is Paul Kocher of
 * Cryptography Research.  Portions created by Paul Kocher are 
 * Copyright (C) 1995-9 by Cryptography Research, Inc.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 */
#ifndef _SHA_H_
#define _SHA_H_

#include <glib.h>

typedef struct {
	guint32 H[5];
 	guint32 W[80];
	int lenW;
	guint32 sizeHi;
	guint32 sizeLo;
} SHA_CTX;

int strprintsha(char *dest, int *hashval);
void shaInit(SHA_CTX *ctx);
void shaUpdate(SHA_CTX *ctx, unsigned char *dataIn, int len);
void shaFinal(SHA_CTX *ctx, unsigned char hashout[20]);
void shaBlock(unsigned char *dataIn, int len, unsigned char hashout[20]);

#endif
