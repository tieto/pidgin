#ifndef _GGP_PUBDIR_PRPL_H
#define _GGP_PUBDIR_PRPL_H

#include <internal.h>
#include <libgadu.h>

typedef struct
{
	uin_t uin;
	gchar *label;
	enum
	{
		GGP_PUBDIR_GENDER_UNSPECIFIED,
		GGP_PUBDIR_GENDER_FEMALE,
		GGP_PUBDIR_GENDER_MALE,
	} gender;
	gchar *city;
	time_t birth;
	int age;
} ggp_pubdir_record;

typedef void (*ggp_pubdir_request_cb)(PurpleConnection *gc, int records_count,
	const ggp_pubdir_record *records, void *user_data);

void ggp_pubdir_get_info(PurpleConnection *gc, uin_t uin,
	ggp_pubdir_request_cb cb, void *user_data);

#endif /* _GGP_PUBDIR_PRPL_H */
