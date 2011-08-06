/*
 *  camdata.h
 *  basecame
 *
 *  Created by CS194 on Mon Apr 26 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once

#ifdef __APPLE_CC__
//	#include <Carbon/Carbon.h>
	#include <QuickTime/QuickTime.h>
#else
//	#include <Carbon.h>
	#include <QuickTime.h>
#endif

typedef struct
{
	GWorldPtr 				gw;
	GWorldPtr 				overlay;
	GWorldPtr 				histoWorld;

	Rect 					bounds;
	WindowPtr				window;
	ImageSequence 			drawSeq;
	UInt32					redMin, redMax;
	UInt32					greenMin, greenMax;
	UInt32					blueMin, blueMax;

	long					selectedIndex;
	OSType					effect;
	TimeBase				effectTimeBase;
	QTAtomContainer			effectParams;
	ImageDescriptionHandle 	effectDesc, effectDesc2;
}	mungDataRecord;
typedef mungDataRecord *mungDataPtr;

OSErr DisposeMungData(void);
OSErr InitializeMungData(Rect bounds);
void SetCurrentClamp(short index);

//void BlitOneMungData(mungDataRecord *theMungData);

//void AdjustColorClampEndpoints(short hMouseCoord);
//void IncrementCurrentClamp(void);
//void DecrementCurrentClamp(void);
void SetMungDataColorDefaults(void);
CGrafPtr GetMungDataWindowPort(void);

GWorldPtr GetMungDataOffscreen(void);
//OSType GetMungDataEffectType(void);

//long GetCurrentClamp(void);
//void SetCurrentClamp(short index);

void GetMungDataBoundsRect(Rect *movieRect);
//CGrafPtr GetMungDataWindowPort(void);

void SetMungDataDrawSeq(ImageSequence theDrawSeq);
//ImageSequence GetMungDataDrawSeq(void);
