/**
 * @file profile.h Trepia profile API
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _TREPIA_PROFILE_H_
#define _TREPIA_PROFILE_H_

#include <glib.h>
#include <time.h>

typedef struct
{
	int type;           /* c */

	int id;             /* a */
	char *location;     /* p */
	time_t login_time;  /* b */
	char *login;        /* d */
	int age;            /* m */
	char sex;           /* n */
	char *first_name;   /* g */
	char *last_name;    /* h */
	char *profile;      /* o */
	char *email;        /* e */
	int   icq;          /* i */
	char *aim;          /* j */
	char *msn;          /* k */
	char *yahoo;        /* l */
	char *homepage;     /* f */
	char *country;      /* r */
	char *state;        /* s */
	char *city;         /* t */
	char *languages;    /* u */
	char *school;       /* v */
	char *company;      /* w */

} TrepiaProfile;

TrepiaProfile *trepia_profile_new(void);
void trepia_profile_destroy(TrepiaProfile *profile);

void trepia_profile_set_type(TrepiaProfile *profile, int value);
void trepia_profile_set_id(TrepiaProfile *profile, int value);
void trepia_profile_set_location(TrepiaProfile *profile, const char *value);
void trepia_profile_set_login_time(TrepiaProfile *profile, time_t value);
void trepia_profile_set_login(TrepiaProfile *profile, const char *value);
void trepia_profile_set_age(TrepiaProfile *profile, int value);
void trepia_profile_set_sex(TrepiaProfile *profile, char value);
void trepia_profile_set_first_name(TrepiaProfile *profile, const char *value);
void trepia_profile_set_last_name(TrepiaProfile *profile, const char *value);
void trepia_profile_set_profile(TrepiaProfile *profile, const char *value);
void trepia_profile_set_email(TrepiaProfile *profile, const char *value);
void trepia_profile_set_icq(TrepiaProfile *profile, int value);
void trepia_profile_set_aim(TrepiaProfile *profile, const char *value);
void trepia_profile_set_msn(TrepiaProfile *profile, const char *value);
void trepia_profile_set_yahoo(TrepiaProfile *profile, const char *value);
void trepia_profile_set_homepage(TrepiaProfile *profile, const char *value);
void trepia_profile_set_country(TrepiaProfile *profile, const char *value);
void trepia_profile_set_state(TrepiaProfile *profile, const char *value);
void trepia_profile_set_city(TrepiaProfile *profile, const char *value);
void trepia_profile_set_languages(TrepiaProfile *profile, const char *value);
void trepia_profile_set_school(TrepiaProfile *profile, const char *value);
void trepia_profile_set_company(TrepiaProfile *profile, const char *value);

int trepia_profile_get_type(const TrepiaProfile *profile);
int trepia_profile_get_id(const TrepiaProfile *profile);
const char *trepia_profile_get_location(const TrepiaProfile *profile);
time_t trepia_profile_get_login_time(const TrepiaProfile *profile);
const char *trepia_profile_get_login(const TrepiaProfile *profile);
int trepia_profile_get_age(const TrepiaProfile *profile);
char trepia_profile_get_sex(const TrepiaProfile *profile);
const char *trepia_profile_get_first_name(const TrepiaProfile *profile);
const char *trepia_profile_get_last_name(const TrepiaProfile *profile);
const char *trepia_profile_get_profile(const TrepiaProfile *profile);
const char *trepia_profile_get_email(const TrepiaProfile *profile);
int trepia_profile_get_icq(const TrepiaProfile *profile);
const char *trepia_profile_get_aim(const TrepiaProfile *profile);
const char *trepia_profile_get_msn(const TrepiaProfile *profile);
const char *trepia_profile_get_yahoo(const TrepiaProfile *profile);
const char *trepia_profile_get_homepage(const TrepiaProfile *profile);
const char *trepia_profile_get_country(const TrepiaProfile *profile);
const char *trepia_profile_get_state(const TrepiaProfile *profile);
const char *trepia_profile_get_city(const TrepiaProfile *profile);
const char *trepia_profile_get_languages(const TrepiaProfile *profile);
const char *trepia_profile_get_school(const TrepiaProfile *profile);
const char *trepia_profile_get_company(const TrepiaProfile *profile);

#endif /* _TREPIA_PROFILE_H_ */
