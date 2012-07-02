#ifndef _GGP_ACCOUNT_H
#define _GGP_ACCOUNT_H

#include <internal.h>

typedef struct
{
	gchar *id;
	gpointer data;
	size_t size;
	int length;
} ggp_account_token;

/**
 * token must be free'd with ggp_account_token_free
 */
typedef void (*ggp_account_token_cb)(PurpleConnection *gc,
	ggp_account_token *token, gpointer user_data);

void ggp_account_token_request(PurpleConnection *gc,
	ggp_account_token_cb callback, void *user_data);
gboolean ggp_account_token_validate(ggp_account_token *token,
	const gchar *value);
void ggp_account_token_free(ggp_account_token *token);


void ggp_account_register(PurpleAccount *account);

void ggp_account_chpass(PurpleConnection *gc);

#endif /* _GGP_ACCOUNT_H */
