/*
 *  camproc.c
 *  basecame
 *
 *  Created by CS194 on Mon Apr 26 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */


#include "camdata.h"
#include "Utilities.h"
#include "QTUtilities.h"
#include "stdio.h"
#include "math.h"
#include <gtk/gtk.h>
#include "cc_interface.h"
#include "filter.h"


extern int detection_mode;
extern mungDataPtr myMungData;
extern int x_click;
extern int y_click;


#define kMinimumIdleDurationInMillis	kEventDurationMillisecond
#define BailErr(x) {if (x != noErr) goto bail;}
#define PRE_CALIBRATE_MODE 0
#define CALIBRATE_MODE 1
#define SCAN_MODE 2

#define CALIB_TOP 190
#define CALIB_BOTTOM 200
#define CALIB_LEFT 200
#define CALIB_RIGHT 210

#define CALIB_RADIUS 5

#define NUM_FRAMES_EYE_SEARCH 50
#define EYE_UNCONFIDENCE_LIMIT 7

#define BLINK_THRESHOLD 75
#define BLINK_LENGTH 10
#define WHITE_THRESH 25
#define WHITE_COUNT_MAX 200


struct input_instance* instance;
int scan_region_left;
int scan_region_right;
int scan_region_top;
int scan_region_bottom;

int lum_thresh=150;
int face_left;
int face_right;
int face_top;
int face_bottom;
int old_left_eye_x=50;
int old_left_eye_y=50;
int old_right_eye_x=50;
int old_right_eye_y=50;
int left_eye_x;
int left_eye_y;
int right_eye_x;
int right_eye_y;
int eye_search_frame_count=0;
int bozo_bit=0;
int eye_unconfidence=0;
int last_eye_count_left=0;
int last_eye_count_right=0;
int mouth_ctr_x;
int mouth_ctr_y;
int mouth_size, mouth_left, mouth_right, mouth_top, mouth_bottom;
int white_count;
guint8 head_size_old;
int left_eye_blink_count;
int right_eye_blink_count;

int left_eye_top, left_eye_bottom, left_eye_right, left_eye_left;
int right_eye_top, right_eye_bottom, right_eye_right, right_eye_left;

filter_bank *bank;

static SeqGrabComponent		mSeqGrab		= NULL;
static SGChannel 			mSGChanVideo 	= NULL;
static SGDataUPP 			mMyDataProcPtr 	= NULL;
static EventLoopTimerRef 	mSGTimerRef 	= 0;
static ImageSequence 		mDecomSeq 		= 0;
static EventLoopTimerUPP	mSGTimerUPP		= nil;
static Rect					mMungRect 		= {0, 0, 480, 640};
int					lower_left_corner_x = 200;
int					lower_left_corner_y = 200;
int					upper_right_corner_x = 210;
int					upper_right_corner_y = 190;


static pascal OSErr MiniMungDataProc(SGChannel c, Ptr p, long len, long *offset, long chRefCon, TimeValue time, short writeType, long refCon);
static pascal void SGIdlingTimer(EventLoopTimerRef inTimer, void *inUserData);
static void DetectLobster(GWorldPtr mungDataOffscreen);
int SkinDetect(double Y, double E, double S);
void ScanSkin(PixMapHandle p);
void drawbox(int top, int bottom, int left, int right, int color);
void SkinStats (PixMapHandle p, int top, int bottom, int left, int right);
void SetEyeSearchRegions(void);


typedef enum  {RED, GREEN, BLUE} color;
color saved_best=-1;

int filenum=0;


OSErr CamProc(struct input_instance *inst, filter_bank *f)
{
	OSStatus error;
	OSErr err = noErr;

	BailErr(err = InitializeMungData(mMungRect));

	bank = f;

	instance=inst;
	mMyDataProcPtr 	= NewSGDataUPP(MiniMungDataProc);
	mSeqGrab 		= OpenDefaultComponent(SeqGrabComponentType, 0);
    BailErr((err = CreateNewSGChannelForRecording(	mSeqGrab,
                                    mMyDataProcPtr,
                                    GetMungDataOffscreen(), // drawing destination
                                    &mMungRect,
                                    &mSGChanVideo,
                                    NULL)));

bail:
    return err;
}

void QueryCam (void)
{
	SGIdle(mSeqGrab);
}

static pascal void SGIdlingTimer(EventLoopTimerRef inTimer, void *inUserData)
{
#pragma unused(inUserData)

    if (mSeqGrab)
    {
        SGIdle(mSeqGrab);
    }

    // Reschedule the event loop timer
    SetEventLoopTimerNextFireTime(inTimer, kMinimumIdleDurationInMillis);
}

static pascal OSErr MiniMungDataProc(SGChannel c, Ptr p, long len, long *offset, long chRefCon, TimeValue time, short writeType, long refCon)
{


#pragma unused(offset,chRefCon,time,writeType,refCon)
	ComponentResult err = noErr;
	CodecFlags 		ignore;
    GWorldPtr 		gWorld;




	if (!myMungData) goto bail;

    gWorld = GetMungDataOffscreen();
	if(gWorld)
	{
		if (mDecomSeq == 0)	// init a decompression sequence
		{
			Rect bounds;

			GetMungDataBoundsRect(&bounds);

            BailErr( CreateDecompSeqForSGChannelData(c, &bounds, gWorld, &mDecomSeq));

			if(1)
            //if ((!mUseOverlay) && (GetCurrentClamp() == -1) && (!mUseEffect))
            {
				ImageSequence drawSeq;

                err = CreateDecompSeqForGWorldData(	gWorld,
                                                    &bounds,
                                                    nil,
                                                    GetMungDataWindowPort(),
                                                    &drawSeq);
				SetMungDataDrawSeq(drawSeq);
            }
		}

        // decompress data to our offscreen gworld
		BailErr(DecompressSequenceFrameS(mDecomSeq,p,len,0,&ignore,nil));

		// image is now in the GWorld - manipulate it at will!
		//if ((mUseOverlay) || (GetCurrentClamp() != -1) || (mUseEffect))
        //{
			// use our custom decompressor to "decompress" the data
			// to the screen with overlays or color clamping
          //  BlitOneMungData(myMungData);
        //}
		//else
        //{
			// we are doing a motion detect grab, so
			// search for lobsters in our image data
            DetectLobster(gWorld);
        //}

	}

bail:
	return err;
}

void Die()
{
	//RemoveEventLoopTimer(mSGTimerRef);
       // mSGTimerRef = nil;
      //  DisposeEventLoopTimerUPP(mSGTimerUPP);
       	DoCloseSG(mSeqGrab, mSGChanVideo, mMyDataProcPtr);

}


float Y_mean=-1;
float Y_dev,E_mean,E_dev,S_mean,S_dev;
/*
	extern colorBuf[480][640];
*/
extern unsigned int (*colorBuf)[644];
extern struct input_instance input_data;

static void DetectLobster(GWorldPtr mungDataOffscreen)
{
    CGrafPtr	oldPort;
    GDHandle	oldDevice;
    int			x, y;
    Rect		bounds;
    PixMapHandle	pix = GetGWorldPixMap(mungDataOffscreen);
    UInt32		* baseAddr;
    UInt32		reds = 0;
    Str255		tempString;
    int			minX = 10000, maxX = -10000;
    int			minY = 10000, maxY = -10000;
	Rect		tempRect;
    float		percent;
    OSErr		err;
	CodecFlags 	ignore;
	color best;
    long R_total=0;
	long G_total=0;
	long B_total=0;



	//fprintf(stderr, "Starting to find some lobsters...\n");


    GetPortBounds(mungDataOffscreen, &bounds);
    OffsetRect(&bounds, -bounds.left, -bounds.top);


	UInt32			color;


	int sum_x,sum_y=0;
	int count=0;
	int k,j;
	long			R;
	long			G;
	long			B;
	int search_width=200;
	int search_height=200;


	colorBuf = GetPixBaseAddr(pix);

	switch (detection_mode) {

	case PRE_CALIBRATE_MODE:
		//drawbox(CALIB_TOP, CALIB_BOTTOM, CALIB_LEFT, CALIB_RIGHT);
		break;

	case CALIBRATE_MODE:
		SkinStats(pix, y_click-CALIB_RADIUS, y_click+CALIB_RADIUS, x_click-CALIB_RADIUS, x_click+CALIB_RADIUS);
		scan_region_left=x_click-CALIB_RADIUS;//10;
		scan_region_right=x_click+CALIB_RADIUS;//630;
		scan_region_top=y_click-CALIB_RADIUS;//10;
		scan_region_bottom=y_click+CALIB_RADIUS;//470;
		ScanSkin(pix);
		detection_mode=SCAN_MODE;
		//fprintf(stderr, "scan left: %d scan right: %d \n",scan_region_left,scan_region_right);
		head_size_old=50;
		break;

	case SCAN_MODE:
		ScanSkin(pix);
		drawbox(face_top, face_bottom, face_left, face_right,1);
		//drawbox(scan_region_top, scan_region_bottom, scan_region_left, scan_region_right);
		drawbox((left_eye_y-5),(left_eye_y+5),(left_eye_x-5),(left_eye_x+5),0);
		drawbox((right_eye_y-5),(right_eye_y+5),(right_eye_x-5),(right_eye_x+5),0);
		int face_scale=instance->face.head_size;
		int mouth_width=face_scale;
		int mouth_height=face_scale;
	//	if (bozo_bit==1) drawbax((mouth_ctr_y-mouth_height),(mouth_ctr_y+mouth_height),(mouth_ctr_x-mouth_width),(mouth_ctr_x+mouth_width));
		filter(&instance->face, bank);
		break;
	}

	//fprintf(stderr, "Lobsters found...\n");


}




void ScanSkin(PixMapHandle p)
{
	int y,x,j,k;
	int right_eye_x_sum,right_eye_y_sum,left_eye_x_sum,left_eye_y_sum,right_eye_pt_count,left_eye_pt_count;
	right_eye_x_sum=right_eye_y_sum=left_eye_x_sum=left_eye_y_sum=right_eye_pt_count=left_eye_pt_count=0;
	long R,G,B,sum_x,sum_y;
	int count;
	double Y,E,S,lum;
	double min_lum_mouth=766;
	double min_lum_left=766;
	double min_lum_right=766;
	UInt32			color;
	UInt32		* baseAddr;
	int max_horz=0;
	int max_vert=0;
	sum_x=sum_y=count=0;
	int horz_count[480];
	int vert_count[640];



	memset(horz_count,0,480*sizeof(int));
	memset(vert_count,0,640*sizeof(int));

	if (eye_search_frame_count<NUM_FRAMES_EYE_SEARCH) eye_search_frame_count++;
	else if (eye_search_frame_count==NUM_FRAMES_EYE_SEARCH && bozo_bit==0)
	{
	bozo_bit=1;
	//fprintf(stderr, "GOOD You flipped the bozo bit (to good)\n");
	}

	SetEyeSearchRegions();


	for (y = scan_region_top; y < scan_region_bottom; y++)  // change this to only calculate in bounding box
    {
        baseAddr = (UInt32*)(GetPixBaseAddr(p) + y * GetPixRowBytes(p));
        for (x = scan_region_left; x < scan_region_right; x++)
        {
			color=baseAddr[x];
            R = (color & 0x00FF0000) >> 16;
            G = (color & 0x0000FF00) >> 8;
            B = (color & 0x000000FF) >> 0;
			Y=.253*R+.684*G+.063*B;
			E=.5*R-.5*G;
			S=.25*R+.25*G-.5*B;
			lum=R+G+B;

			if (y>left_eye_top && y<left_eye_bottom)
			{
				if (x > left_eye_left && x<left_eye_right)
				{
					if (lum < lum_thresh) {
					left_eye_x_sum+=x;
					left_eye_y_sum+=y;
					left_eye_pt_count++;
					//colorBuf[y][x]=0x0000FF00;
					}
				}
			}
			if (y>right_eye_top && y<right_eye_bottom)
			{
				if (x > right_eye_left && x < right_eye_right)
				{
					if (lum < lum_thresh) {
					right_eye_x_sum+=x;
					right_eye_y_sum+=y;
					right_eye_pt_count++;
					//colorBuf[y][x]=0x0000FF00;
					}
				}
			}

			if(SkinDetect(Y,E,S))
			{
				sum_x+=x;
				sum_y+=y;
				count++;

				++horz_count[y];
				++vert_count[x];

				if (horz_count[y]>max_horz) max_horz=horz_count[y];
				if (vert_count[x]>max_vert) max_vert=vert_count[x];

				//colorBuf[y][x]=0x00FF0000;
			}

		}
	}


	left_eye_x=left_eye_x_sum/left_eye_pt_count;
	left_eye_y=left_eye_y_sum/left_eye_pt_count;
	right_eye_x=right_eye_x_sum/right_eye_pt_count;
	right_eye_y=right_eye_y_sum/right_eye_pt_count;



	int width=right_eye_x-left_eye_x;
	int height=right_eye_y-left_eye_y;
	double face_ang;
	if (width!=0) face_ang=atan((double)height/width);
	else face_ang=0;
	face_ang=face_ang*180/pi;
	//fprintf(stderr,"face angle: %f \n",face_ang);

	if ((left_eye_pt_count<5 || right_eye_pt_count<5 || width==0 || face_ang > 30 || face_ang < -30
		|| left_eye_y < (face_top+.15*(face_bottom-face_top))
		|| right_eye_y < (face_top+.15*(face_bottom-face_top)))
		&& bozo_bit==1){
		eye_unconfidence++;
		left_eye_x=old_left_eye_x;
		left_eye_y=old_left_eye_y;
		right_eye_x=old_right_eye_x;
		right_eye_y=old_right_eye_y;
	}
	else {
		eye_unconfidence=0;
		old_left_eye_x=left_eye_x;
		old_left_eye_y=left_eye_y;
		old_right_eye_x=right_eye_x;
		old_right_eye_y=right_eye_y;
	}


	if (eye_unconfidence==EYE_UNCONFIDENCE_LIMIT){
		bozo_bit=0;
		eye_search_frame_count=0;
		//fprintf(stderr, "Recalibrating eyes\n");
	}

	if ((last_eye_count_left-left_eye_pt_count> BLINK_THRESHOLD) && eye_unconfidence==0)
	{
	left_eye_blink_count=BLINK_LENGTH;
	}
	if (left_eye_blink_count>0){
		instance->face.left_eye_open=0;
		left_eye_blink_count--;
	}
	else instance->face.left_eye_open=1;

	if ((last_eye_count_right-right_eye_pt_count> BLINK_THRESHOLD) && eye_unconfidence==0)
	{
	right_eye_blink_count=BLINK_LENGTH;
	}
	if (right_eye_blink_count>0){
		instance->face.right_eye_open=0;
		right_eye_blink_count--;
	}
	else instance->face.right_eye_open=1;

	if (instance->face.right_eye_open==0) instance->face.left_eye_open=0;
	if (instance->face.left_eye_open==0) instance->face.right_eye_open=0;

	last_eye_count_left=left_eye_pt_count;
	last_eye_count_right=right_eye_pt_count;

	float x_shift=0;
	if (width!=0) x_shift= (float)height/(float)width; // --> note dependence on earlier data here


if (bozo_bit==1){
	int mouth_search_start_y=face_top+(.6*(face_bottom-face_top));
	int mouth_search_end_y=face_bottom;
	int mouth_search_start_x=(left_eye_x+right_eye_x)/2 + (-x_shift*(mouth_search_start_y-((right_eye_y+left_eye_y)/2))) ;

for (y=mouth_search_start_y; y < mouth_search_end_y; y++)
{
	x=mouth_search_start_x+((y - mouth_search_start_y)*(-x_shift));
	baseAddr = (UInt32*)(GetPixBaseAddr(p) + y * GetPixRowBytes(p));
	//colorBuf[y][x] = 0x0000FF00;
	color=baseAddr[x];
	R = (color & 0x00FF0000) >> 16;
	G = (color & 0x0000FF00) >> 8;
	B = (color & 0x000000FF) >> 0;
	lum=R+G+B;

	if (lum<min_lum_mouth) {
		min_lum_mouth=lum;
		mouth_ctr_x=x;
		mouth_ctr_y=y;
	}
}

	mouth_size=(face_right-face_left)*100/640;
	mouth_left=mouth_ctr_x-mouth_size;
	if (mouth_left < face_left) mouth_left=face_left;
	mouth_right=mouth_ctr_x+mouth_size;
	if (mouth_right > face_right) mouth_right=face_right;
	mouth_top=mouth_ctr_y-mouth_size;
	if (mouth_top < face_top) mouth_top=face_top;
	mouth_bottom=mouth_ctr_y+mouth_size;
	if (mouth_bottom > face_bottom) mouth_bottom=face_bottom;

	white_count=0;

	for (y=mouth_top; y< mouth_bottom; y++){
		baseAddr = (UInt32*)(GetPixBaseAddr(p) + y * GetPixRowBytes(p));
		for (x=mouth_left; x< mouth_right; x++){
			color=baseAddr[x];
			R = (color & 0x00FF0000) >> 16;
			G = (color & 0x0000FF00) >> 8;
			B = (color & 0x000000FF) >> 0;
			if ((abs(R-G) < WHITE_THRESH) && (abs(G-B) < WHITE_THRESH) && (abs(R-B) < WHITE_THRESH))
			{
				white_count++;
				//colorBuf[y][x]=0x0000FF00;
			}
		}
	}

	}
else white_count=10;

// This next section finds the face region and sets the face_* parameters.

	int scan;
	float thresh=.3;
	scan=scan_region_left+1;
	if (scan<0) scan=0;
	//fprintf(stderr,"threshold value: %d boxtop value: %d \n", (max_horz), horz_count[scan_region_top]);
	while(1)
	{
		if (vert_count[scan]>=(thresh*max_vert))
			{
				face_left=scan;
				break;
			}
		scan++;
	}

	scan=scan_region_right-1;
	if (scan>=640) scan=639;
	while(1)
	{
		if (vert_count[scan]>=(thresh*max_vert))
			{
				face_right=scan;
				break;
			}
		scan--;
	}

	scan=scan_region_top+1;
	if (scan<0) scan=0;
	while(1)
	{
		if (horz_count[scan]>=(thresh*max_horz))
			{
				face_top=scan;
				break;
			}
		scan++;
	}


	scan=scan_region_bottom-1;
	if (scan>=480) scan=479;
	while(1)
	{
		if (horz_count[scan]>=(thresh*max_horz))
			{
				face_bottom=scan;
				break;
			}
		scan--;
	}

	// Base scan region on face region here
	scan_region_left=face_left-10;
	if (scan_region_left <= 0) scan_region_left=1;
	scan_region_right=face_right+10;
	if (scan_region_right >= 640) scan_region_right=639;
	scan_region_top=face_top-10;
	if (scan_region_top <= 0) scan_region_top=1;
	scan_region_bottom=face_bottom+10;
	if (scan_region_bottom >= 480) scan_region_bottom=479;


	// Calculate some stats

	// face size
	width=face_right-face_left;
	guint8 temp=width*100/640;
	instance->face.head_size=temp;

	// face location
	temp=((double)100/(double)640)*(double)(face_right+face_left)/2;
	instance->face.x=temp;
	temp=((double)100/(double)480)*(double)(face_top+face_bottom)/2;
	instance->face.y=temp;

	// face angle-Z
	instance->face.head_z_rot=face_ang+50;

	// face angle-Y
	int center=(face_right+face_left)/2;
	int right_eye_strad=right_eye_x-center;
	int left_eye_strad=center-left_eye_x;
	double y_ang;
	if (right_eye_strad > left_eye_strad) y_ang= (double)right_eye_strad/(double)left_eye_strad;
	else y_ang=(double)left_eye_strad/(double)right_eye_strad;
	y_ang=y_ang*5;
	if (y_ang >= 10) y_ang=30;
	if (y_ang <= 1) y_ang=1;

	if (right_eye_strad > left_eye_strad) y_ang=-y_ang;
	temp = (guint8) 50 + y_ang;
	instance->face.head_y_rot=temp;

	if (abs (temp-50) > 15) instance->face.head_size=head_size_old;
	else head_size_old=instance->face.head_size;

	temp = (guint8) 100 * white_count / WHITE_COUNT_MAX;
	if (temp > 100) temp=100;
	instance->face.mouth_open = temp;

}






 // draw bounding box for either calibration or face


void SetEyeSearchRegions(void)
{
			if (bozo_bit==0)
			{
				left_eye_top=face_top+(.25*(face_bottom-face_top));
				left_eye_bottom=face_top+(.6*(face_bottom-face_top));
				left_eye_right=((face_left+face_right)/2);
				left_eye_left=face_left+.15*(face_right-face_left);

				right_eye_top=face_top+(.25*(face_bottom-face_top));
				right_eye_bottom=face_top+(.6*(face_bottom-face_top));
				right_eye_right=face_right-.15*(face_right-face_left);
				right_eye_left=((face_left+face_right)/2);
			}

			if (bozo_bit==1)
			{
				left_eye_top=left_eye_y-20;
				left_eye_bottom=left_eye_y+20;
				left_eye_left=left_eye_x-20;
				left_eye_right=left_eye_x+20;

				right_eye_top=right_eye_y-20;
				right_eye_bottom=right_eye_y+20;
				right_eye_left=right_eye_x-20;
				right_eye_right=right_eye_x+20;
			}
}


void drawbox(int top, int bottom, int left, int right, int color)
{
	int y, x, j;

	unsigned int col;


	if (color==1)
		col=0x00FFFF00;
	else
		col=0x00FF00FF;

	if (top<0) top =0;
	if (top>=480) top=479;
	if (bottom<0) bottom =0;
	if (bottom>=480) bottom=479;
	if (left<0) left =0;
	if (left>=640) left=639;
	if (right<0) right =0;
	if (right>=640) right=639;

	if (color==1){

	for (y=top; y<bottom; y++)
	{
		for (j=0;j<5;j++){
			colorBuf[y][left+j] =  col;
			colorBuf[y][right-j] = col;
		}

	}

	for (x=left; x<right; x++)
	{

	for (j=0;j<5;j++){

		colorBuf[bottom-j][x] = col;
		colorBuf[top+j][x] = col;

		}

	}


	} else {



	for (y=top; y<bottom; y++)
	{
		for (x=left;x<right;x++){
			colorBuf[y][x] =  col;
			colorBuf[y][x] = col;
		}

	}

	}
}



void SkinStats (PixMapHandle p, int top, int bottom, int left, int right)
{
		double Y_sum,E_sum,S_sum;
		int R,G,B;
		int count=0;
		Y_sum=E_sum=S_sum=0;
		double			Y,E,S;
		UInt32			color;
		int			x, y;
		UInt32		* baseAddr;

		for (y=top; y<bottom; y++)
			{
				baseAddr = (UInt32*)(GetPixBaseAddr(p) + y * GetPixRowBytes(p));
				for (x=left; x<right; x++)
				{
				count++;
				color=baseAddr[x];

				R = (color & 0x00FF0000) >> 16;
				G = (color & 0x0000FF00) >> 8;
				B = (color & 0x000000FF) >> 0;
				Y=.253*R+.684*G+.063*B;
				E=.5*R-.5*G;
				S=.25*R+.25*G-.5*B;
				Y_sum+=Y;
				E_sum+=E;
				S_sum+=S;
				}
			}

		Y_mean=Y_sum/count;
		E_mean=E_sum/count;
		S_mean=S_sum/count;

		Y_sum=E_sum=S_sum=0;

		for (y=top; y<bottom; y++)
			{
			baseAddr = (UInt32*)(GetPixBaseAddr(p) + y * GetPixRowBytes(p));
			for (x=left; x<right; x++)
				{
				color=baseAddr[x];
				R = (color & 0x00FF0000) >> 16;
				G = (color & 0x0000FF00) >> 8;
				B = (color & 0x000000FF) >> 0;
				Y=.253*R+.684*G+.063*B;
				E=.5*R-.5*G;
				S=.25*R+.25*G-.5*B;

				Y_sum+=(Y-Y_mean)*(Y-Y_mean);
				E_sum+=(E-E_mean)*(E-E_mean);
				S_sum+=(S-S_mean)*(S-S_mean);

				}
			}

		Y_dev=sqrt(Y_sum/(count-1));
		E_dev=sqrt(E_sum/(count-1));
		S_dev=sqrt(S_sum/(count-1));

		//fprintf(stderr,"Y: %f, %f\n E: %f, %f\nS: %f, %f\n",Y_mean,E_mean,S_mean,Y_dev,E_dev,S_dev);

}

int SkinDetect(double Y, double E, double S)
{
	if (E>(E_mean-(2*E_dev)) && E<(E_mean+(2*E_dev))) return 1;
	else return 0;
}

