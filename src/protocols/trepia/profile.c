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
#include "profile.h"

TrepiaProfile *
trepia_profile_new(void)
{
	return g_new0(TrepiaProfile, 1);
}

void
trepia_profile_destroy(TrepiaProfile *profile)
{
	if (profile->location   != NULL) g_free(profile->location);
	if (profile->login      != NULL) g_free(profile->login);
	if (profile->first_name != NULL) g_free(profile->first_name);
	if (profile->last_name  != NULL) g_free(profile->last_name);
	if (profile->profile    != NULL) g_free(profile->profile);
	if (profile->email      != NULL) g_free(profile->email);
	if (profile->aim        != NULL) g_free(profile->aim);
	if (profile->homepage   != NULL) g_free(profile->homepage);
	if (profile->country    != NULL) g_free(profile->country);
	if (profile->state      != NULL) g_free(profile->state);
	if (profile->city       != NULL) g_free(profile->city);
	if (profile->languages  != NULL) g_free(profile->languages);
	if (profile->school     != NULL) g_free(profile->school);
	if (profile->company    != NULL) g_free(profile->company);

	g_free(profile);
}

void
trepia_profile_set_id(TrepiaProfile *profile, int value)
{
	g_return_if_fail(profile != NULL);

	profile->id = value;
}

void
trepia_profile_set_location(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->location != NULL)
		g_free(profile->location);

	profile->location = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_login_time(TrepiaProfile *profile, time_t value)
{
	g_return_if_fail(profile != NULL);

	profile->login_time = value;
}

void
trepia_profile_set_login(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->login != NULL)
		g_free(profile->login);

	profile->login = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_age(TrepiaProfile *profile, int value)
{
	g_return_if_fail(profile != NULL);

	profile->age = value;
}


void
trepia_profile_set_sex(TrepiaProfile *profile, char value)
{
	g_return_if_fail(profile != NULL);

	profile->sex = value;
}

void
trepia_profile_set_first_name(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->first_name != NULL)
		g_free(profile->first_name);

	profile->first_name = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_last_name(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->last_name != NULL)
		g_free(profile->last_name);

	profile->last_name = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_profile(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->profile != NULL)
		g_free(profile->profile);

	profile->profile = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_email(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->email != NULL)
		g_free(profile->email);

	profile->email = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_icq(TrepiaProfile *profile, int value)
{
	g_return_if_fail(profile != NULL);

	profile->icq = value;
}

void
trepia_profile_set_aim(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->aim != NULL)
		g_free(profile->aim);

	profile->aim = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_msn(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->msn != NULL)
		g_free(profile->msn);

	profile->msn = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_yahoo(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->yahoo != NULL)
		g_free(profile->yahoo);

	profile->yahoo = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_homepage(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->homepage != NULL)
		g_free(profile->homepage);

	profile->homepage = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_country(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->country != NULL)
		g_free(profile->country);

	profile->country = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_state(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->state != NULL)
		g_free(profile->state);

	profile->state = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_city(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->city != NULL)
		g_free(profile->city);

	profile->city = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_languages(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->languages != NULL)
		g_free(profile->languages);

	profile->languages = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_school(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->school != NULL)
		g_free(profile->school);

	profile->school = (value == NULL ? NULL : g_strdup(value));
}

void
trepia_profile_set_company(TrepiaProfile *profile, const char *value)
{
	g_return_if_fail(profile != NULL);

	if (profile->company != NULL)
		g_free(profile->company);

	profile->company = (value == NULL ? NULL : g_strdup(value));
}

int
trepia_profile_get_id(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, 0);

	return profile->id;
}

const char *
trepia_profile_get_location(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->location;
}

time_t
trepia_profile_get_login_time(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, 0);

	return profile->login_time;
}

const char *
trepia_profile_get_login(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->login;
}

int
trepia_profile_get_age(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, 0);

	return profile->age;
}

char
trepia_profile_get_sex(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, '?');

	return profile->sex;
}

const char *
trepia_profile_get_first_name(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->first_name;
}

const char *
trepia_profile_get_last_name(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->last_name;
}

const char *
trepia_profile_get_profile(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->profile;
}

const char *
trepia_profile_get_email(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->email;
}

int
trepia_profile_get_icq(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, 0);

	return profile->icq;
}

const char *
trepia_profile_get_aim(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->aim;
}

const char *
trepia_profile_get_msn(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->msn;
}

const char *
trepia_profile_get_yahoo(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->yahoo;
}

const char *
trepia_profile_get_homepage(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->homepage;
}

const char *
trepia_profile_get_country(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->country;
}

const char *
trepia_profile_get_state(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->state;
}

const char *
trepia_profile_get_city(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->city;
}

const char *
trepia_profile_get_languages(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->languages;
}

const char *
trepia_profile_get_school(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->school;
}

const char *
trepia_profile_get_company(const TrepiaProfile *profile)
{
	g_return_val_if_fail(profile != NULL, NULL);

	return profile->company;
}

