#ifndef _GGP_PUBDIR_PRPL_H
#define _GGP_PUBDIR_PRPL_H

#include <internal.h>
#include <libgadu.h>

typedef enum
{
	GGP_PUBDIR_GENDER_UNSPECIFIED,
	GGP_PUBDIR_GENDER_FEMALE,
	GGP_PUBDIR_GENDER_MALE,
} ggp_pubdir_gender;

typedef struct
{
	uin_t uin;
	gchar *label;
	gchar *nickname;
	gchar *first_name;
	gchar *last_name;
	ggp_pubdir_gender gender;
	gchar *city;
	unsigned int province;
	time_t birth;
	unsigned int age;
} ggp_pubdir_record;

typedef struct _ggp_pubdir_search_form ggp_pubdir_search_form;

typedef void (*ggp_pubdir_request_cb)(PurpleConnection *gc, int records_count,
	const ggp_pubdir_record *records, int next_offset, void *user_data);

void ggp_pubdir_get_info(PurpleConnection *gc, uin_t uin,
	ggp_pubdir_request_cb cb, void *user_data);

void ggp_pubdir_get_info_prpl(PurpleConnection *gc, const char *name);
void ggp_pubdir_request_buddy_alias(PurpleConnection *gc, PurpleBuddy *buddy);

void ggp_pubdir_search(PurpleConnection *gc,
	const ggp_pubdir_search_form *form);

void ggp_pubdir_set_info(PurpleConnection *gc);

#endif /* _GGP_PUBDIR_PRPL_H */
