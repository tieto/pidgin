/*
	File:		Utilities.c
	
	Description: Miscellaneous Utility routines.

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

#ifndef __UTILITIES__
    #include "Utilities.h"
#endif

const OSType    kApplicationSignature  = FOUR_CHAR_CODE('cvmj');
const ResType   kOpenResourceType = FOUR_CHAR_CODE('open');
const StringPtr kApplicationName = "\pYourAppNameHere";

//////////
//
// GetOneFileWithPreview
// Display the appropriate file-opening dialog box, with an optional QuickTime preview pane. If the user
// selects a file, return information about it using the theFSSpecPtr parameter.
//
// Note that both StandardGetFilePreview and NavGetFile use the function specified by theFilterProc as a
// file filter. This framework always passes NULL in the theFilterProc parameter. If you use this function
// in your own code, keep in mind that on Windows the function specifier must be of type FileFilterUPP and 
// on Macintosh it must be of type NavObjectFilterUPP. (You can use the QTFrame_GetFileFilterUPP to create
// a function specifier of the appropriate type.) Also keep in mind that Navigation Services expects a file 
// filter function to return true if a file is to be displayed, while the Standard File Package expects the
// filter to return false if a file is to be displayed.
//
//////////

OSErr GetOneFileWithPreview (short theNumTypes, TypeListPtr theTypeList, FSSpecPtr theFSSpecPtr, void *theFilterProc)
{
	
#if TARGET_OS_WIN32
	StandardFileReply	myReply;
#endif
#if TARGET_OS_MAC
	NavReplyRecord		myReply;
	NavDialogOptions		myDialogOptions;
	NavTypeListHandle	myOpenList = NULL;
	NavEventUPP		myEventUPP = NewNavEventUPP(HandleNavEvent);
#endif
	OSErr				myErr = noErr;
	
	if (theFSSpecPtr == NULL)
		return(paramErr);

#if TARGET_OS_WIN32
	// prompt the user for a file
	StandardGetFilePreview((FileFilterUPP)theFilterProc, theNumTypes, (ConstSFTypeListPtr)theTypeList, &myReply);
	if (!myReply.sfGood)
		return(userCanceledErr);
	
	// make an FSSpec record
	myErr = FSMakeFSSpec(myReply.sfFile.vRefNum, myReply.sfFile.parID, myReply.sfFile.name, theFSSpecPtr);
#endif

#if TARGET_OS_MAC
	// specify the options for the dialog box
	NavGetDefaultDialogOptions(&myDialogOptions);
	myDialogOptions.dialogOptionFlags -= kNavNoTypePopup;
	myDialogOptions.dialogOptionFlags -= kNavAllowMultipleFiles;
	BlockMoveData(kApplicationName, myDialogOptions.clientName, kApplicationName[0] + 1);
	
	// create a handle to an 'open' resource
	myOpenList = (NavTypeListHandle)CreateOpenHandle(kApplicationSignature, theNumTypes, theTypeList);
	if (myOpenList != NULL)
		HLock((Handle)myOpenList);
	
	// prompt the user for a file
	myErr = NavGetFile(NULL, &myReply, &myDialogOptions, myEventUPP, NULL, (NavObjectFilterUPP)theFilterProc, myOpenList, NULL);
	if ((myErr == noErr) && myReply.validRecord) {
		AEKeyword		myKeyword;
		DescType		myActualType;
		Size			myActualSize = 0;
		
		// get the FSSpec for the selected file
		if (theFSSpecPtr != NULL)
			myErr = AEGetNthPtr(&(myReply.selection), 1, typeFSS, &myKeyword, &myActualType, theFSSpecPtr, sizeof(FSSpec), &myActualSize);

		NavDisposeReply(&myReply);
	}
	
	if (myOpenList != NULL) {
		HUnlock((Handle)myOpenList);
		DisposeHandle((Handle)myOpenList);
	}
	
	DisposeNavEventUPP(myEventUPP);
#endif
 
	return(myErr);
}

//////////
//
// CreateOpenHandle
// Get the 'open' resource or dynamically create a NavTypeListHandle.
//
//////////

Handle CreateOpenHandle (OSType theApplicationSignature, short theNumTypes, TypeListPtr theTypeList)
{
	Handle			myHandle = NULL;
	
	// see if we have an 'open' resource...
	myHandle = Get1Resource('open', 128);
	if ( myHandle != NULL && ResError() == noErr ) {
		DetachResource( myHandle );
		return myHandle;
	} else {
		myHandle = NULL;
	}
	
	// nope, use the passed in types and dynamically create the NavTypeList
	if (theTypeList == NULL)
		return myHandle;
	
	if (theNumTypes > 0) {
		myHandle = NewHandle(sizeof(NavTypeList) + (theNumTypes * sizeof(OSType)));
		if (myHandle != NULL) {
			NavTypeListHandle 	myOpenResHandle	= (NavTypeListHandle)myHandle;
			
			(*myOpenResHandle)->componentSignature = theApplicationSignature;
			(*myOpenResHandle)->osTypeCount = theNumTypes;
			BlockMoveData(theTypeList, (*myOpenResHandle)->osType, theNumTypes * sizeof(OSType));
		}
	}
	
	return myHandle;
}

//////////
//
// HandleNavEvent
// A callback procedure that handles events while a Navigation Service dialog box is displayed.
//
//////////

PASCAL_RTN void HandleNavEvent(NavEventCallbackMessage theCallBackSelector, NavCBRecPtr theCallBackParms, void *theCallBackUD)
{
#pragma unused(theCallBackUD)
	
	if (theCallBackSelector == kNavCBEvent) {
		switch (theCallBackParms->eventData.eventDataParms.event->what) {
			case updateEvt:
#if TARGET_OS_MAC
				// Handle Update Event
#endif
				break;
			case nullEvent:
				// Handle Null Event
				break;
		}
	}
}

//////////
//
// PutFile
// Save a file under the specified name. Return Boolean values indicating whether the user selected a file
// and whether the selected file is replacing an existing file.
//
//////////

OSErr PutFile (ConstStr255Param thePrompt, ConstStr255Param theFileName, FSSpecPtr theFSSpecPtr, Boolean *theIsSelected, Boolean *theIsReplacing)
{
	NavReplyRecord		myReply;
	NavDialogOptions		myDialogOptions;
	NavEventUPP		myEventUPP = NewNavEventUPP(HandleNavEvent);
	OSErr				myErr = noErr;

	if ((theFSSpecPtr == NULL) || (theIsSelected == NULL) || (theIsReplacing == NULL))
		return(paramErr);
	
	// assume we are not replacing an existing file
	*theIsReplacing = false;
	
        *theIsSelected = false;
        
	// specify the options for the dialog box
	NavGetDefaultDialogOptions(&myDialogOptions);
	myDialogOptions.dialogOptionFlags += kNavNoTypePopup;
	myDialogOptions.dialogOptionFlags += kNavDontAutoTranslate;
	BlockMoveData(theFileName, myDialogOptions.savedFileName, theFileName[0] + 1);
	BlockMoveData(thePrompt, myDialogOptions.message, thePrompt[0] + 1);
	
	// prompt the user for a file
	myErr = NavPutFile(NULL, &myReply, &myDialogOptions, myEventUPP, MovieFileType, sigMoviePlayer, NULL);
	if ((myErr == noErr) && myReply.validRecord) 
    {
		AEKeyword		myKeyword;
		DescType		myActualType;
		Size			myActualSize = 0;
		
		// get the FSSpec for the selected file
		if (theFSSpecPtr != NULL)
			myErr = AEGetNthPtr(&(myReply.selection), 1, typeFSS, &myKeyword, &myActualType, theFSSpecPtr, sizeof(FSSpec), &myActualSize);

                *theIsSelected = myReply.validRecord;
                if (myReply.validRecord)
                {
                    *theIsReplacing = myReply.replacing;
                }

		NavDisposeReply(&myReply);
	}
		

	DisposeNavEventUPP(myEventUPP);

	return(myErr);
}

//////////
//
// DrawLobsterPICTtoGWorld
// Draws our lobster image to the specified gworld
//
//////////

void DrawLobsterPICTtoGWorld(CGrafPtr destGWorld, Rect *srcRect)
{
        // put the overlay into the GWorld, move the picture to the bottom right of the movie
    PicHandle	pict = GetPicture(129);
    if (pict)
    {
        CGrafPtr	oldPort;
        GDHandle	oldDevice;
        Rect		frame = (**pict).picFrame;
        
        GetGWorld(&oldPort, &oldDevice);
        SetGWorld(destGWorld, nil);
        // normalize coordinates
        OffsetRect(&frame, -frame.left, -frame.left);
        // grow frame to be as big as source rect
        OffsetRect(&frame, srcRect->right - frame.right, srcRect->bottom - frame.bottom);
        DrawPicture(pict, &frame);
        SetGWorld(oldPort, oldDevice);
        ReleaseResource((Handle)pict);
    }
}

