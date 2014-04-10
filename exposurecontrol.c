
#include <stdlib.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libv4l2.h>
#include <errno.h>
#include <math.h>
#include "exposurecontrol.h"


static int setAbsoluteExposure( int captureDevice, int absoluteValue );
static int setControl( int captureDevice, int controlId, int controlValue );

int updateAbsoluteExposure( int captureDevice, int newTime ) {

    static int lastKnownTime = -1;

    if( lastKnownTime != newTime ) {
        lastKnownTime = newTime;
        return setAbsoluteExposure( captureDevice, newTime );
    }
}

int calibrateExposureLow( int captureDevice, int detectedDots, int* exposure, int maximumExposure, float lastKnownFPS ) {

    static int total_time_counter = 0; // Used to make sure we don't keep running up and down forever
    static int consistency_counter = 0; // Used to make sure the result wasn't just a fluke
    static int dots_last_run = 0; // The number of dots we found last run;
    static int delta_up = 5;
    static int delta_down = 5;

    if( delta_up == 0 ) {
        delta_up = maximumExposure - *exposure;
    }

    if( delta_down == 0 ) {
        delta_down = *exposure / 2;
    }

    if( *exposure >= maximumExposure ) {
        total_time_counter = 0;
        consistency_counter = 0;
        dots_last_run = 0;
        return -1;
    }

    if( *exposure == 0 ) {
        total_time_counter = 0;
        consistency_counter = 0;
        dots_last_run = 0;
        return -2;
    }

    if( detectedDots == 1 && dots_last_run == 1 ) {
        if( ++consistency_counter >= 50 ) {
            // We are done
            // Reset state variables in case we want to run calibration again later
            total_time_counter = 0;
            consistency_counter = 0;
            dots_last_run = 0;
            return 0;
        }
        else {
            return 1; // Looking good, but not sure yet
        }
    } 
    
    if( consistency_counter > 0 ) {
        consistency_counter = 0;
        delta_up = fmax( delta_up / 2, 1 );
        delta_down = fmax ( delta_down / 2, 1 );
    }
    if( detectedDots == 0 ) { // We didn't find any dots, let's up exposure some
        if( dots_last_run == 0 ) {
            *exposure += ( maximumExposure - *exposure ) / 5 + 1;
        }
        else {
            *exposure += delta_up;
        }
    }
    else { // At least one dot was found
        if( dots_last_run == 0 ) { 
            // We didn't find any dots last run. Lets up exposure some more
            *exposure += delta_up;
        } else { 
            // We found more then one dot last run. Lets reduce exposure somewhat
            *exposure -= delta_down;
        }
    }

    if( *exposure < 0 ) {
        //This shuldn't be possible, something is very wrong
        fprintf( stderr, "ERROR! Exposure calibration broke!\n" );
        total_time_counter = 0;
        return -1;
    }

    dots_last_run = detectedDots;

    setAbsoluteExposure( captureDevice, *exposure );

    ++total_time_counter;
    if( total_time_counter > 300 ) {
        // Can't find consistent results. Giving up
        total_time_counter = 0;
        consistency_counter = 0;
        dots_last_run = 0;
        return -3;
    }

    return 1;
}

int enableAutoExposure( int captureDevice ) {

    if( setControl( captureDevice, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_AUTO ) == -1 ) {
        return -1;
    }
    return 0;
}

int disableAutoExposure( int captureDevice ) {

    if( setControl( captureDevice, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL ) == -1 ) {
        return -1;
    }
    return 0;
}

static int setAbsoluteExposure( int captureDevice, int absoluteValue ) {

    if(( setControl( captureDevice, V4L2_CID_EXPOSURE_ABSOLUTE, absoluteValue )) == -1 ) {
        return 0;
    }
    return 1;
}

static int setControl( int captureDevice, int controlId, int controlValue ) {

    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;

    memset(&queryctrl, 0, sizeof( queryctrl ));
    queryctrl.id = controlId;

    // Reset errno
    errno = 0;

    if( -1 == ioctl( captureDevice, VIDIOC_QUERYCTRL, &queryctrl ) ) {
        if( errno != EINVAL ) {
            fprintf( stderr, "OPTION is not supported.\n");
            return -1;
            //exit( EXIT_FAILURE );
        } else {
            fprintf( stderr, "OPTION  is not supported.\n");
            return -1;
        }
    } else if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED ) {
        fprintf( stderr, "OPTION is not supported.\n");
        return -1;
    } else {
        memset(&control, 0, sizeof( control )); //skriv nollor till hela control för att kunna sätta egna värden att skriva.
        control.id = controlId;
        //control.value = queryctrl.default_value;
        control.value = controlValue;

        if(-1 == ioctl( captureDevice, VIDIOC_S_CTRL, &control ) ) {
            fprintf( stderr, "VIDIOC_S_CTRL\n");
            //exit( EXIT_FAILURE );
            return -1;
        }
    }

    return 0;

}
