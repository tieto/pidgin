
#define FAIM_INTERNAL
#include <aim.h>

static int reportinterval(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{
  unsigned short interval;
  rxcallback_t userfunc;

  interval = aimutil_get16(data);

  if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
    return userfunc(sess, rx, interval);

  return 0;
}

static int snachandler(struct aim_session_t *sess, aim_module_t *mod, struct command_rx_struct *rx, aim_modsnac_t *snac, unsigned char *data, int datalen)
{

  if (snac->subtype == 0x0002)
    return reportinterval(sess, mod, rx, snac, data, datalen);

  return 0;
}

faim_internal int stats_modfirst(struct aim_session_t *sess, aim_module_t *mod)
{

  mod->family = 0x000b;
  mod->version = 0x0000;
  mod->flags = 0;
  strncpy(mod->name, "stats", sizeof(mod->name));
  mod->snachandler = snachandler;

  return 0;
}
