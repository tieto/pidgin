/*
 *
 *
 *
 */

#include "aim.h"

int aimutil_put8(u_char *buf, u_short data)
{
  buf[0] = data&0xff;
  return 1;
}

/*
 * Endian-ness issues here?
 */
int aimutil_put16(u_char *buf, u_short data)
{
  buf[0] = (data>>8)&0xff;
  buf[1] = (data)&0xff;
  return 2;
}

int aimutil_put32(u_char *buf, u_long data)
{
  buf[0] = (data>>24)&0xff;
  buf[1] = (data>>16)&0xff;
  buf[2] = (data>>8)&0xff;
  buf[3] = (data)&0xff;
  return 4;
}

int aimutil_putstr(u_char *dest, u_char *src, int len)
{
  memcpy(dest, src, len);
  return len;
}
