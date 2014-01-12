#ifndef _GGP_KEYMAPPER_H
#define _GGP_KEYMAPPER_H

typedef struct _ggp_keymapper ggp_keymapper;

#include <purple.h>

ggp_keymapper *
ggp_keymapper_new(void);

void
ggp_keymapper_free(ggp_keymapper *km);

gpointer
ggp_keymapper_to_key(ggp_keymapper *km, uint64_t val);

/* The key have to be valid. */
uint64_t
ggp_keymapper_from_key(ggp_keymapper *km, gpointer key);

#endif /* _GGP_KEYMAPPER_H */
