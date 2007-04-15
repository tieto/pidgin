/sw/#ifdef __APPLE_CC__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif


#include "Utilities.h"
#include "QTUtilities.h"

#include "camdata.h"
#include "camproc.h"


#define BailErr(err) {if(err != noErr) goto bail;}


int main(void)
{	
    EnterMovies();
    CamProc(); // change this prototype-> no windows
    fprintf(stderr, "you have just murdered 1000 people.");
    RunApplicationEventLoop();
    return 0;
}