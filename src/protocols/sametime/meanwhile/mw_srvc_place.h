
/*
  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _MW_SRVC_PLACE_H
#define _MW_SRVC_PLACE_H


#include <glib/glist.h>
#include "mw_common.h"


/** Type identifier for the place service */
#define SERVICE_PLACE  0x80000022


/** @struct mwServicePlace */
struct mwServicePlace;


/** @struct mwPlace */
struct mwPlace;


/** @struct mwPlaceSection */
struct mwPlaceSection;


/** @struct mwPlaceAction */
struct mwPlaceAction;


struct mwPlaceHandler {

};


struct mwServicePlace *mwServicePlace_new(struct mwSession *session,
					  struct mwPlaceHandler *handler);


struct mwPlace *mwPlace_new(struct mwServicePlace *place,
			    const char *title, const char *name);


int mwPlace_open(struct mwPlace *place);


int mwPlace_close(struct mwPlace *place);


void mwPlace_free(struct mwPlace *place);


const GList *mwPlace_getSections(struct mwPlace *place);
