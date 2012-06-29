#ifndef _GGP_ACCOUNT_H
#define _GGP_ACCOUNT_H

#include <internal.h>

typedef struct
{
	gchar *id;
	gpointer data;
	size_t size;
} ggp_account_token;

/**
 * token must be free'd with ggp_account_token_free
 */
typedef void (*ggp_account_token_cb)(PurpleConnection *gc, ggp_account_token *token, gpointer user_data);

void ggp_account_token_request(PurpleConnection *gc, ggp_account_token_cb callback, void *user_data);
void ggp_account_token_free(ggp_account_token *token);


void ggp_account_register(PurpleAccount *account);

#endif /* _GGP_ACCOUNT_H */
