// source: http://toxygen.net/libgadu/

#ifndef _GGP_OAUTH_PARAMETER_H
#define _GGP_OAUTH_PARAMETER_H

#include <internal.h>

typedef struct gg_oauth_parameter gg_oauth_parameter_t;

int gg_oauth_parameter_set(gg_oauth_parameter_t **list, const char *key, const char *value);
char *gg_oauth_parameter_join(gg_oauth_parameter_t *list, int header);
void gg_oauth_parameter_free(gg_oauth_parameter_t *list);

#endif /* _GGP_OAUTH_PARAMETER_H */
