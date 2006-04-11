/**
 * @file search.c
 *
 * gaim
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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


#include <libgadu.h>

#include "gg-utils.h"
#include "search.h"

/* GGPSearchForm *ggp_search_form_new() {{{ */
GGPSearchForm *ggp_search_form_new()
{
	GGPSearchForm *form;

	form = g_new0(GGPSearchForm, 1);
	form->uin = NULL;
	form->lastname = NULL;
	form->firstname = NULL;
	form->nickname = NULL;
	form->city = NULL;
	form->birthyear = NULL;
	form->gender = NULL;
	form->active = NULL;
	form->offset = NULL;

	form->last_uin = NULL;

	return form;
}
/* }}} */

/* void ggp_search_start(GaimConnection *gc, GGPSearchForm *form) {{{ */
void ggp_search_start(GaimConnection *gc, GGPSearchForm *form)
{
	GGPInfo *info = gc->proto_data;
	gg_pubdir50_t req;

	gaim_debug_info("gg", "It's time to perform a search...\n");

	if ((req = gg_pubdir50_new(GG_PUBDIR50_SEARCH)) == NULL) {
		gaim_debug_error("gg",
			"ggp_bmenu_show_details: Unable to create req variable.\n");
		return;
	}

	if (form->uin != NULL) {
		gaim_debug_info("gg", "    uin: %s\n", form->uin);
		gg_pubdir50_add(req, GG_PUBDIR50_UIN, form->uin);
	} else {
		if (form->lastname != NULL) {
			gaim_debug_info("gg", "    lastname: %s\n", form->lastname);
			gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, form->lastname);
		}

		if (form->firstname != NULL) {
			gaim_debug_info("gg", "    firstname: %s\n", form->firstname);
			gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, form->firstname);
		}

		if (form->nickname != NULL) {
			gaim_debug_info("gg", "    nickname: %s\n", form->nickname);
			gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, form->nickname);
		}

		if (form->city != NULL) {
			gaim_debug_info("gg", "    city: %s\n", form->city);
			gg_pubdir50_add(req, GG_PUBDIR50_CITY, form->city);
		}

		if (form->birthyear != NULL) {
			gaim_debug_info("gg", "    birthyear: %s\n", form->birthyear);
			gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, form->birthyear);
		}

		if (form->gender != NULL) {
			gaim_debug_info("gg", "    gender: %s\n", form->gender);
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, form->gender);
		}

		if (form->active != NULL) {
			gaim_debug_info("gg", "    active: %s\n", form->active);
			gg_pubdir50_add(req, GG_PUBDIR50_ACTIVE, form->active);
		}
	}

	gaim_debug_info("gg", "offset: %s\n", form->offset);
	gg_pubdir50_add(req, GG_PUBDIR50_START, g_strdup(form->offset));

	if (gg_pubdir50(info->session, req) == 0) {
		gaim_debug_warning("gg", "ggp_bmenu_show_details: Search failed.\n");
		return;
	}

	gg_pubdir50_free(req);
}
/* }}} */

/* char *ggp_search_get_result(gg_pubdir50_t res, int num, const char *field) {{{ */
char *ggp_search_get_result(gg_pubdir50_t res, int num, const char *field)
{
	char *tmp;

	tmp = charset_convert(gg_pubdir50_get(res, num, field), "CP1250", "UTF-8");

	return (tmp == NULL) ? g_strdup("") : tmp;
}
/* }}} */


/* vim: set ts=8 sts=0 sw=8 noet: */
