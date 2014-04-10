
#ifndef __EXPOSURECONTROL_H__
#define __EXPOSURECONTROL_H__

#ifndef DD_MIN_FPS
#define DD_MIN_FPS 27
#endif

#ifndef DD_MAX_EXPOSURE
#define DD_MAX_EXPOSURE 330
#endif

int enableAutoExposure(int captureDevice);
int disableAutoExposure(int captureDevice);
int updateAbsoluteExposure(int captureDevice, int newTime);
int calibrateExposureLow(int captureDevice, int detectedDots, int* exposure, int maximumExposure, float lastKnownFPS);

#endif
