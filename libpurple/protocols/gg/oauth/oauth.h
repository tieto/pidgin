// source: http://toxygen.net/libgadu/

#ifndef _GGP_OAUTH_H
#define _GGP_OAUTH_H

#include <internal.h>
#include <libgadu.h>

char *gg_oauth_generate_header(const char *method, const char *url, const const char *consumer_key, const char *consumer_secret, const char *token, const char *token_secret);

#endif /* _GGP_OAUTH_H */
