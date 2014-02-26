
#ifndef __EXPOSURECONTROL_H__
#define __EXPOSURECONTROL_H__

    

    int disableAutoExposure(int captureDevice);
    int setAbsoluteExposure(int captureDevice, int absoluteValue);
    void updateAbsoluteExposure(int captureDevice, int newTime);
    int calibrateExposureLow(int captureDevice, int detectedDots, int lastTestedExposure, int maximumExposure, float lastKnownFPS);
    
#endif
