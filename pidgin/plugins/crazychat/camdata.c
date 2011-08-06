/*
 *  camdata.c
 *  basecame
 *
 *  Created by CS194 on Mon Apr 26 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include "camdata.h"
//#include "AppBlit_Component.h"
#include "QTUtilities.h"
#include "Utilities.h"


#define	BailNULL(n)  if (!n) goto bail;
#define	BailError(n) if (n) goto bail;
#define	BailNil(n) if (!n) goto bail;
#define BailErr(x) {if (x != noErr) goto bail;}
#define bitdepth 32

mungDataPtr myMungData = NULL;
long		mWorlds[20];
UInt32		mRedCount[256], mGreenCound[256], mBlueCount[256];

static void DecompressSequencePreflight(GWorldPtr srcGWorld,
                                        ImageSequence *imageSeq,
                                        GWorldPtr destGWorld,
                                        Rect *srcRect);
//static void DrawRGBHistogram(mungDataRecord *theMungData);
//static void CreateEffectDescription(mungDataRecord *theMungData);
//static void CreateEffectDecompSequence(mungDataRecord *theMungData);
//static void AddGWorldDataSourceToEffectDecompSeq(mungDataRecord *theMungData);
//static void MakeEffectTimeBaseForEffect(mungDataRecord *theMungData);
//static void DrawUsingEffect(mungDataRecord *theMungData);


OSErr InitializeMungData(Rect bounds)
{
	OSErr err = noErr;

	if(myMungData)
    {
        DisposeMungData();
	}

	myMungData = (mungDataPtr)NewPtrClear(sizeof(mungDataRecord));
	if (myMungData == nil)
    {
        err = MemError();
        goto bail;
    }

	myMungData->effect = 0; // always


	BailErr(QTNewGWorld(&(myMungData->gw),bitdepth,&bounds,0,0,0));
	LockPixels(GetGWorldPixMap(myMungData->gw));

    SetMungDataColorDefaults();



	myMungData->selectedIndex = 0;
	myMungData->overlay = NULL;

	SetCurrentClamp(-1);



	myMungData->bounds = bounds;

	SetRect(&bounds, 0, 0, 256*2+4, 128*3 + 20);
	BailErr(QTNewGWorld(&(myMungData->histoWorld),bitdepth,&bounds,0,0,0));
	LockPixels(GetGWorldPixMap(myMungData->histoWorld));

bail:
	return err;
}

OSErr DisposeMungData(void)
{  // check this out
    OSErr err = noErr;

    if(myMungData)
    {
        //if(myMungData->drawSeq)
        //{
        //    CDSequenceEnd(myMungData->drawSeq);
        //}

        if(myMungData->gw)
        {
            DisposeGWorld(myMungData->gw);
            myMungData->gw = nil;
        }

        if(myMungData->overlay)
        {
            DisposeGWorld(myMungData->overlay);
            myMungData->overlay = nil;
        }

        if(myMungData->histoWorld)
        {
            DisposeGWorld(myMungData->histoWorld);
            myMungData->histoWorld = nil;
        }

        if (myMungData->effectTimeBase)
        {
            DisposeTimeBase(myMungData->effectTimeBase);
        }
        if (myMungData->effectParams)
        {
            QTDisposeAtomContainer(myMungData->effectParams);
        }
        if (myMungData->effectDesc)
        {
            DisposeHandle((Handle)myMungData->effectDesc);
        }

        DisposePtr((Ptr)myMungData);
        myMungData = nil;
    }
    return err;
}

static void DecompressSequencePreflight(GWorldPtr srcGWorld,
                                        ImageSequence *imageSeq,
                                        GWorldPtr destGWorld,
                                        Rect *srcRect)
// might not need this one

{
    ImageDescriptionHandle imageDesc = nil;

    BailErr(MakeImageDescriptionForPixMap (GetGWorldPixMap(srcGWorld), &imageDesc));

    // use our built-in decompressor to draw
   // (**imageDesc).cType = kCustomDecompressorType;

// *********** MIGHT BE MAKING A BIG MISTAKE ******************
    // pass a compressed sample so a codec can perform preflighting before the first DecompressSequenceFrameWhen call

    BailErr(DecompressSequenceBegin(imageSeq,
                                    imageDesc,
                                    destGWorld,
                                    0,
                                    srcRect,
                                    nil,
                                    srcCopy,
                                    nil,
                                    0,
                                    codecNormalQuality,
                                    bestSpeedCodec));

bail:
    if (imageDesc)
    {
        DisposeHandle((Handle)imageDesc);
    }
}

ImageSequence GetMungDataDrawSeq()
{
	return myMungData->drawSeq;
}

void SetMungDataColorDefaults()
{
	if(myMungData)
    {
        myMungData->redMin = 2;
        myMungData->redMax = 254;
        myMungData->greenMin = 2;
        myMungData->greenMax = 254;
        myMungData->blueMin = 2;
        myMungData->blueMax = 254;
    }
}

void GetMungDataBoundsRect(Rect *boundsRect)
// might not need this one
{
	MacSetRect (boundsRect,
			myMungData->bounds.left,
			myMungData->bounds.top,
			myMungData->bounds.right,
			myMungData->bounds.bottom
	);
}

void SetCurrentClamp(short index)     // :crazy:20040426
{
    myMungData->selectedIndex = index;
}

GWorldPtr GetMungDataOffscreen()
{
    return (myMungData->gw);
}

void SetMungDataDrawSeq(ImageSequence theDrawSeq)
{
	myMungData->drawSeq = theDrawSeq;
}


CGrafPtr GetMungDataWindowPort()
{
	return GetWindowPort(myMungData->window);
}
