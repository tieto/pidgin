#include "validator.h"

#include "account.h"
#include "utils.h"

gboolean ggp_validator_token(PurpleRequestField *field, gchar **errmsg,
	void *token)
{
	const char *value;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value = purple_request_field_string_get_value(field);
	
	if (value != NULL && ggp_account_token_validate(token, value))
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Captcha validation failed"));
	return FALSE;
}

gboolean ggp_validator_password(PurpleRequestField *field, gchar **errmsg,
	void *user_data)
{
	const char *value;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value = purple_request_field_string_get_value(field);
	
	if (value != NULL && ggp_password_validate(value))
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Password can contain 6-15 alphanumeric characters"));
	return FALSE;
}

gboolean ggp_validator_password_equal(PurpleRequestField *field, gchar **errmsg,
	void *field2_p)
{
	const char *value1, *value2;
	PurpleRequestField *field2 = field2_p;
	
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field2 != NULL, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	g_return_val_if_fail(purple_request_field_get_type(field2) ==
		PURPLE_REQUEST_FIELD_STRING, FALSE);
	
	value1 = purple_request_field_string_get_value(field);
	value2 = purple_request_field_string_get_value(field2);
	
	if (g_strcmp0(value1, value2) == 0)
		return TRUE;
	
	if (errmsg)
		*errmsg = g_strdup(_("Passwords do not match"));
	return FALSE;
}
