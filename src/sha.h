#if (SIZEOF_INT == 4)
typedef unsigned int uint32;
#elif (SIZEOF_SHORT == 4)
typedef unsigned short uint32;
#else
typedef unsigned int uint32;
#endif /* HAVEUINT32 */
 
int strprintsha(char *dest, int *hashval);
 
typedef struct {
  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long sizeHi,sizeLo;
} SHA_CTX;
 
void shaInit(SHA_CTX *ctx);
void shaUpdate(SHA_CTX *ctx, unsigned char *dataIn, int len);
void shaFinal(SHA_CTX *ctx, unsigned char hashout[20]);
void shaBlock(unsigned char *dataIn, int len, unsigned char hashout[20]);
                                                                                                         
