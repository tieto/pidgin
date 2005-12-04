/*
 *  camproc.h
 *  basecame
 *
 *  Created by CS194 on Mon Apr 26 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */



//#pragma once

#ifdef __APPLE_CC__
	#include <Carbon/Carbon.h>

#else
	#include <Carbon.h>

#endif

#include "cc_interface.h"
#include "filter.h"

void Die(void);
OSErr CamProc(struct input_instance *inst, filter_bank *bank);
void QueryCam (void);


