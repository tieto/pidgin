/**
 * @file search.h
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


#ifndef _GAIM_GG_SEARCH_H
#define _GAIM_GG_SEARCH_H

#include "connection.h"

#include <libgadu.h>
#include "gg.h"


typedef struct {

	char *uin;
	char *lastname;
	char *firstname;
	char *nickname;
	char *city;
	char *birthyear;
	char *gender;
	char *active;
	char *offset;

	char *last_uin;

} GGPSearchForm;

/**
 * Create a new GGPSearchForm structure, and set the fields
 * to the sane defaults.
 *
 * @return Newly allocated GGPSearchForm.
 */
GGPSearchForm *
ggp_search_form_new(void);

/**
 * Initiate a search in the public directory.
 *
 * @param gc   GaimConnection.
 * @param form Filled in GGPSearchForm.
 */
void
ggp_search_start(GaimConnection *gc, GGPSearchForm *form);

/*
 * Return converted to the UTF-8 value of the specified field.
 *
 * @param res    Public directory look-up result.
 * @param num    Id of the record.
 * @param fileld Name of the field.
 * 
 * @return UTF-8 encoded value of the field.
 */
char *
ggp_search_get_result(gg_pubdir50_t res, int num, const char *field);


#endif /* _GAIM_GG_SEARCH_H */

/* vim: set ts=8 sts=0 sw=8 noet: */
