
#include <stdlib.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libv4l2.h>
#include <errno.h>
#include "exposurecontrol.h"


static int setAbsoluteExposure(int captureDevice, int absoluteValue);
static int setControl(int captureDevice, int controlId, int controlValue);

int updateAbsoluteExposure(int captureDevice, int newTime) {

    static int lastKnownTime = -1;
    
    if(lastKnownTime != newTime) {
        lastKnownTime = newTime;
        return setAbsoluteExposure(captureDevice, newTime);
    }
}

int calibrateExposureLow(int captureDevice, int detectedDots, int lastTestedExposure, int maximumExposure, float lastKnownFPS) {

    if(lastKnownFPS < 20) {
        return -2;
    }

    /* Increase Exposure to eradicate error margin. */

    if(detectedDots != 1 && lastTestedExposure < maximumExposure){
        setAbsoluteExposure(captureDevice, lastTestedExposure++);
        return lastTestedExposure;
    } else if(detectedDots >= 1) {
        setAbsoluteExposure(captureDevice, lastTestedExposure+20); /* Generated by fair diceroll, make a point about this in the report */
        return -1;
    } else if(lastTestedExposure == maximumExposure) {
        return 9999;
    }
}

int disableAutoExposure(int captureDevice) {

    if(setControl(captureDevice, V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL) == -1) {
        return -1;
    }
    return 0;
}

static int setAbsoluteExposure(int captureDevice, int absoluteValue) {

    if((setControl(captureDevice, V4L2_CID_EXPOSURE_ABSOLUTE, absoluteValue)) == -1) {
        return -1;
    }
    return 0;
}

static int setControl(int captureDevice, int controlId, int controlValue) {

    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;

    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = controlId;

    if (-1 == ioctl (captureDevice, VIDIOC_QUERYCTRL, &queryctrl)) {
	    if (errno != EINVAL) {
	    	fprintf (stderr, "OPTION is not supported.\n");
	    	return -1;
	    	//exit (EXIT_FAILURE);
	    } else {
	    	fprintf (stderr, "OPTION  is not supported.\n");
	    	return -1;
	    }
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    	fprintf (stderr, "OPTION is not supported.\n");
    	return -1;
    } else {
    	memset (&control, 0, sizeof (control)); //skriv nollor till hela control för att kunna sätta egna värden att skriva.
    	control.id = controlId;
    	//control.value = queryctrl.default_value;
        control.value = controlValue;

    	if (-1 == ioctl (captureDevice, VIDIOC_S_CTRL, &control)) {
    		fprintf (stderr, "VIDIOC_S_CTRL\n");
    		//exit (EXIT_FAILURE);
    	    return -1;
    	}
    }
    
    return 0;

}
