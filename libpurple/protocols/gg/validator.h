#ifndef _GGP_VALIDATOR_H
#define _GGP_VALIDATOR_H

#include <internal.h>
#include <request.h>

gboolean ggp_validator_token(PurpleRequestField *field, gchar **errmsg,
	void *token);

gboolean ggp_validator_password(PurpleRequestField *field, gchar **errmsg,
	void *user_data);

gboolean ggp_validator_password_equal(PurpleRequestField *field, gchar **errmsg,
	void *field2);

#endif /* _GGP_VALIDATOR_H */
