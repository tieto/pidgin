#include <aim.h>

struct aim_tlv_t *aim_grabtlv(u_char *src)
{
  struct aim_tlv_t *dest = NULL;

  dest = aim_createtlv();

  dest->type = src[0] << 8;
  dest->type += src[1];

  dest->length = src[2] << 8;
  dest->length += src[3];

  dest->value = (u_char *) malloc(dest->length*sizeof(u_char));
  memset(dest->value, 0, dest->length*sizeof(u_char));

  memcpy(dest->value, &(src[4]), dest->length*sizeof(u_char));
  
  return dest;
}

struct aim_tlv_t *aim_grabtlvstr(u_char *src)
{
  struct aim_tlv_t *dest = NULL;

  dest = aim_createtlv();

  dest->type = src[0] << 8;
  dest->type += src[1];

  dest->length = src[2] << 8;
  dest->length += src[3];

  dest->value = (u_char *) malloc((dest->length+1)*sizeof(u_char));
  memset(dest->value, 0, (dest->length+1)*sizeof(u_char));

  memcpy(dest->value, &(src[4]), dest->length*sizeof(u_char));
  dest->value[dest->length] = '\0';

  return dest;
}

int aim_puttlv (u_char *dest, struct aim_tlv_t *newtlv)
{
  int i=0;

  dest[i++] = newtlv->type >> 8;
  dest[i++] = newtlv->type & 0x00FF;
  dest[i++] = newtlv->length >> 8;
  dest[i++] = newtlv->length & 0x00FF;
  memcpy(&(dest[i]), newtlv->value, newtlv->length);
  i+=newtlv->length;
  return i;
}

struct aim_tlv_t *aim_createtlv(void)
{
  struct aim_tlv_t *newtlv = NULL;
  newtlv = (struct aim_tlv_t *)malloc(sizeof(struct aim_tlv_t));
  memset(newtlv, 0, sizeof(struct aim_tlv_t));
  return newtlv;
}

int aim_freetlv(struct aim_tlv_t **oldtlv)
{
  if (!oldtlv)
    return -1;
  if (!*oldtlv)
    return -1;
  if ((*oldtlv)->value)
    free((*oldtlv)->value);
  free(*(oldtlv));
  (*oldtlv) = NULL;

  return 0;
}

int aim_puttlv_16(u_char *buf, u_short t, u_short v)
{
  int curbyte=0;
  curbyte += aimutil_put16(buf+curbyte, t&0xffff);
  curbyte += aimutil_put16(buf+curbyte, 0x0002);
  curbyte += aimutil_put16(buf+curbyte, v&0xffff);
  return curbyte;
}
