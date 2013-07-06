/*
 * Contact Availability Prediction plugin for Purple
 *
 * Copyright (C) 2006 Geoffrey Foster.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifndef _CAP_STATISTICS_H_
#define _CAP_STATISTICS_H_

#include "buddylist.h"
#include <gdk/gdk.h>
#include <glib.h>
#include <time.h>

/* Data Structures */
typedef struct _CapStatistics CapStatistics;
typedef struct _CapPrediction CapPrediction;

struct _CapStatistics {
	gdouble minute_stats[1440];
	CapPrediction *prediction;
	time_t last_seen;			/**< The time buddy was last seen online */
	time_t last_message;		/**< The time you last messaged them */
	const char *last_message_status_id; /**< The status id of the buddy when you last messaged them */
	const char *last_status_id;	/**< The last seen status id of the buddy */
	PurpleBuddy *buddy;			/**< The buddy that these statistics are associated with */
	guint timeout_source_id;
};

struct _CapPrediction {
	double probability;
	time_t generated_at;
};

#endif
