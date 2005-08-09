/*
	File:		QTUtilities.c
	
	Description: Miscellaneous QuickTime utility routines.

	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
				copyrights in this original Apple software (the "Apple Software"), to use,
				reproduce, modify and redistribute the Apple Software, with or without
				modifications, in source and/or binary forms; provided that if you redistribute
				the Apple Software in its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all such redistributions of
				the Apple Software.  Neither the name, trademarks, service marks or logos of
				Apple Computer, Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from Apple.  Except as
				expressly stated in this notice, no other rights or licenses, express or implied,
				are granted by Apple herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works in which the Apple
				Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
				WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
				WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
				PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
				COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
				CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
				GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
				OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
				(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
				ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
				
	Change History (most recent first):

*/

#include "QTUtilities.h"
#include "Utilities.h"

#define BailErr(x) {if (x != noErr) goto bail;}

//////////
//
// GetMovieFromFile
// Opens a movie file, then creates a new movie for the file
//
//////////

OSErr GetMovieFromFile(FSSpec *fsspecPtr, Movie *theMovie)
{
	short	resRefNum = -1;
    OSErr	result;

    *theMovie = NULL;
    
	result = OpenMovieFile(fsspecPtr, &resRefNum, 0);
	if (result == noErr)
    {
        short actualResId = DoTheRightThing;
        
        result = NewMovieFromFile(theMovie, 
                                resRefNum, 
                                &actualResId, 
                                (unsigned char *) 0,
                                0,
                                (Boolean *) 0);
        CloseMovieFile(resRefNum);
    }
    
    return result;
}

//////////
//
// GetAMovieFile
// Prompt the user for a movie file, then open
// the file and create a movie for it.
//
//////////

OSErr GetAMovieFile(Movie *theMovie)
{
    OSType 	myTypeList[2] = {kQTFileTypeMovie, kQTFileTypeQuickTimeImage};
    FSSpec	theFSSpec;
    OSErr	result = noErr;

    *theMovie = nil;
    
    result = GetOneFileWithPreview(2, myTypeList, &theFSSpec, NULL);
    if (result != userCanceledErr)
    {            
        result = GetMovieFromFile(&theFSSpec, theMovie);
    }
    
    return result;
}

//////////
//
// NormalizeMovieRect
//
//////////

void NormalizeMovieRect(Movie theMovie)
{
    Rect movieBounds;
    
	GetMovieBox(theMovie, &movieBounds);
	OffsetRect(&movieBounds, -movieBounds.left, -movieBounds.top);
	movieBounds.right = movieBounds.left + 640;
	movieBounds.bottom = movieBounds.top + 480;
	SetMovieBox(theMovie, &movieBounds);
}

//////////
//
// EraseRectAndAlpha
// Zeros out a section of the GWorld, including alpha.
//
//////////

void EraseRectAndAlpha(GWorldPtr gWorld, Rect *pRect)
{
	PixMapHandle	pixMap = GetGWorldPixMap(gWorld);
	long			rows;
	Ptr				rowBaseAddr;


    LockPixels(pixMap);
	rows = pRect->bottom - pRect->top;
    
    rowBaseAddr = GetPixBaseAddr(pixMap) + (GetPixRowBytes(pixMap) & 0x3fff) * pRect->top + pRect->left * GetPixDepth(pixMap) / 8;
	do
	{
		long	cols;
		UInt32	*baseAddr;
		
		cols = pRect->right - pRect->left;
		baseAddr = (UInt32*)rowBaseAddr;
		rowBaseAddr += (**pixMap).rowBytes & 0x3fff;
		do
		{
			*baseAddr++ = 0;
		} while (--cols);
	} while (--rows);

    UnlockPixels(pixMap);

} // EraseRectAndAlpha

//////////
//
// CreateDecompSeqForSGChannelData
// Create a decompression sequence for the passed
// Sequence Grabber channel data
//
//////////

OSErr CreateDecompSeqForSGChannelData(SGChannel sgChannel, Rect *srcBounds, GWorldPtr imageDestination, ImageSequence *imageSeqID)
{
	OSErr err = noErr;
	
	ImageDescriptionHandle	imageDesc = (ImageDescriptionHandle)NewHandle(sizeof(ImageDescription));
	if (imageDesc)
	{

		err = SGGetChannelSampleDescription(sgChannel,(Handle)imageDesc);
		// The original version of this code had a bug - it passed in a Crop Rect to DecompressSequenceBegin instead of a scaling matrix
		// This only worked because of another bug inside QT that reated the crop Rect as a destination rect for DV
		// the following code does the right thing in all cases.
		
		if (err == noErr)
		{
			MatrixRecord 	mr;
			Rect 			fromR;

			fromR.left = 0;
			fromR.top = 0;
			fromR.right = (**imageDesc).width;
			fromR.bottom = (**imageDesc).height;
			RectMatrix(&mr, &fromR, srcBounds);
			
			err = DecompressSequenceBegin(imageSeqID, imageDesc, imageDestination, 0, nil, &mr,srcCopy,nil,0, codecNormalQuality, bestSpeedCodec);
		}
		
		DisposeHandle((Handle)imageDesc);
	}
	else
	{
		err = MemError();
	}
	
	return err;
}


//////////
//
// CreateDecompSeqForGWorldData
// Create a decompression sequence for the specified gworld data
//
//////////

OSErr CreateDecompSeqForGWorldData(GWorldPtr srcGWorld, Rect *srcBounds, MatrixRecordPtr mr, GWorldPtr imageDestination, ImageSequence *imageSeqID)
{
    OSErr err;
    
    ImageDescriptionHandle imageDesc = (ImageDescriptionHandle)NewHandle(sizeof(ImageDescription));
    if (imageDesc)
    {
        err = MakeImageDescriptionForPixMap (GetGWorldPixMap(srcGWorld), &imageDesc);
        err = DecompressSequenceBegin(	imageSeqID, 
                                        imageDesc, 
                                        imageDestination, 
                                        0, 
                                        srcBounds, 
                                        mr, 
                                        srcCopy, 
                                        nil, 
                                        0, 
                                        codecNormalQuality, 
                                        bestSpeedCodec);
        DisposeHandle((Handle)imageDesc);
    }
    else
    {
        err = MemError();
    }
    
    return err;
}

//////////
//
// CreateNewSGChannelForRecording
// - create a new Sequence Grabber video channel
// - let the use configure the channel
// - set the channel bounds, usage
// - set a data proc for the channel
// - start recording data
//
//////////

OSErr CreateNewSGChannelForRecording(ComponentInstance seqGrab, SGDataUPP dataProc, CGrafPtr drawPort, Rect *theRect, SGChannel *sgChannel, long refCon)
{
	OSErr err = noErr;
	
	BailErr((err = SGInitialize(seqGrab)));

	// tell it we're not making a movie
	BailErr((err = SGSetDataRef(seqGrab,0,0,seqGrabDontMakeMovie)));
	// It wants a port, even if its not drawing to it
	BailErr((err = SGSetGWorld(seqGrab, drawPort, GetMainDevice())));
	BailErr((err = SGNewChannel(seqGrab, VideoMediaType, sgChannel)));
	
	// let the user configure the video channel
	//BailErr((err = SGSettingsDialog(seqGrab, *sgChannel, 0, nil, 0, nil, 0)));    // ************************
// ************************************************************	
	
	
	
	BailErr((err = SGSetChannelBounds(*sgChannel, theRect)));
	// set usage for new video channel to avoid playthrough
	BailErr((err = SGSetChannelUsage(*sgChannel, seqGrabRecord ))); //note we don't set seqGrabPlayDuringRecord
    BailErr((err = SGSetDataProc(seqGrab, dataProc, refCon)));
	BailErr((err = SGStartRecord(seqGrab)));

bail:
    return err;
}

//////////
//
// DoCloseSG
// Terminate recording for our SG channel - dispose of the channel and 
// the associated SG component instance
//
//////////

void DoCloseSG(ComponentInstance seqGrab, SGChannel sgChannel, SGDataUPP dataProc)
{
	if(seqGrab) 
    {
		SGStop(seqGrab);
        SGSetDataProc(seqGrab, NULL ,NULL);
        if (dataProc)
        {
            DisposeSGDataUPP(dataProc);
        }

        SGDisposeChannel(seqGrab, sgChannel);
		CloseComponent(seqGrab);
	}
}


