#include <cv.h> 
#include <highgui.h> 
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <locale.h>
#include "DD_shapes.h"
#include <fcntl.h>
#include "shapedetector.h"
#include "profiling.h"
#include "exposurecontrol.h"

// Most functions in OpenCV uses BGR-A for some reason, so that's how these are defined
#define RED 0, 0, 255, 0
#define GREEN 0, 255, 0, 0
#define BLUE 255, 0, 0, 0
#define WHITE 255, 255, 255, 0

#define DEFAULT_SERVER_ADDRESS "127.0.0.1"
#define DEFAULT_SERVER_PORT 10001

#define POINT_SIZE 16
#define SEND_BUF_SIZE 1472 // Max payload of a single UDP package with MTU 1500 (the common default MTU)

// States
#define GRAB_DOTS 1
#define SELECT_MASK 2
#define SELECT_TRANSFORM 3

#ifndef imagewindowname
#define imagewindowname "imagewindow"
#endif

#ifndef configwindowname
#define configwindowname "configwindow"
#endif

#ifndef warpwindowname
#define warpwindowname "warpwindow"
#endif

// Corners of a bounding box
//
// 0 ------- 1
// |        /
// |       /
// 3 ---- 2

#define TOP_LEFT 0
#define TOP_RIGHT 1
#define BOTTOM_RIGHT 2
#define BOTTOM_LEFT 3

// A translation table for the points. Intended to be used as
// "Select %s point", pointTranslationTable[clickParams.currentPoint]
static char* pointTranslationTable[] = {
    "top left",
    "top right",
    "bottom right",
    "bottom left"
};

#define DD_COLOR( c ) c.blue, c.green, c.red, c.alpha
// cvScalar, used to handle colors in OpenCV uses doules internally.
// This causes problems when using a slider to set the color as that
// wants the address to an int. Thus, we store colors in a struct of
// ints and use cvScalar() to create the scalars when needed.
struct color {
    int red;
    int green;
    int blue;
    int alpha;
};
typedef struct color Color;

typedef struct ClickParams {
    int currentPoint;
    struct BoundingBox * DD_box;
    struct BoundingBox * DD_transformation_to;
    struct CvMat * transMat;
} ClickParams;

static char state = GRAB_DOTS;


static const char* red_lable = "Red";
static const char* green_lable = "Green";
static const char* blue_lable = "Blue";
static const char* min_area_lable = "Min area";
static const char* exposure_lable = "Exposure";

// The definition of a single element in the send queue
typedef struct SendQueue {
    float point[2]; // x and y
    struct SendQueue *next;
} SendQueue;

// Starts up the network part
int initNetwork( const char *serverAddress, const int serverPort )
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char port[10];
    snprintf( port, sizeof( port ), "%i", serverPort );

    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    rv = getaddrinfo( serverAddress, port, &hints, &servinfo );
    if( rv != 0 ) {
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( rv ));
        return -1;
    }

    // loop through all the results and make a socket
    for( p = servinfo; p != NULL; p = p->ai_next ) {
        sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol );
        if( sockfd == -1 ) {
            perror("socket");
            continue;
        }

        break;
    }

    if( p == NULL ) {
        fprintf( stderr, "talker: failed to bind socket\n");
        return -1;
    }

    rv = connect( sockfd, p->ai_addr, p->ai_addrlen );
    if( rv == -1 ) {
        perror( "connect" );
        return -1;
    }

    freeaddrinfo( servinfo );
    return sockfd;
}

// Sets up the send queue
SendQueue *initSendQueue() {
    SendQueue *q = malloc( sizeof( SendQueue ));
    q->next = NULL;
    return q;
}

// Adds a single point to the send queue
void addPointToSendQueue( const float p[2], SendQueue *q ) {
    SendQueue *newEntry = malloc( sizeof( SendQueue ) );
    newEntry->point[0] = p[0];
    newEntry->point[1] = p[1];
    newEntry->next = NULL;

    // Advance to last entry
    while( q->next != NULL ) {
        q = q->next;
    }
    q->next = newEntry;
}

// Sends the send queue over the network in text format "sss#x.xx,y.yy x.xx,y.yy"
// Sends "ndd" if there are no dots detected
void sendQueue( int sockfd, SendQueue *q )
{
    static unsigned long long seq = 0; //Global sequence number. Will simply roll around should it hit max. Deal with it on client side
    static char sentEmpty = 0;
    int ret, len = 0, seqLen = 0;
    char buf[SEND_BUF_SIZE];

    // Force numberic locale to C, else the protocol format can break
    if( setlocale( LC_NUMERIC, "C") == NULL ) {
        fprintf( stderr, "Failed to set locale to C\n");
    }

    buf[0] = '\0';

    // Add sequence number to package
    seqLen = snprintf( &buf[len], sizeof( buf ) - strlen( buf ), "%llu#", seq++ );
    len = seqLen;

    // Skip first entry
    q = q->next;
    while( q != NULL &&( SEND_BUF_SIZE - strlen( buf )) >= POINT_SIZE ) { //One point is estimated to be at most 16 byte. "xxxx.xx,yyyy.yy "
        ret = snprintf( &buf[len], sizeof( buf ) - strlen( buf ), "%.2f,%.2f ", q->point[0], q->point[1]);
        if( ret < 0 ) {
            printf( "Foo\n" );
            break;
        }
        len += ret;

        q = q->next;
    }
    if( len > seqLen ) {
        buf[--len] = '\0'; //Remove the trailing space

        // Resetting empty queue counter( see below for explanation )
        sentEmpty = 0;

        //	printf("Sending: \"%s\"\n", buf );
        send( sockfd, buf, len, 0 );
    }
    else {
        if( sentEmpty < 5 ) {

            // We keep track of how many times( in a row ) we have sent an empty queue.
            // We do this so that we don't keep sending 'ndd' forever, but we still want
            // to send it a few times( we are using UDP after all )
            sentEmpty++; 

            //If no dots are detected we send "ndd", No Dots Detected. Sending "" didn't seem to work.
            buf[0] = 'n';
            buf[1] = 'd';
            buf[2] = 'd';
            buf[3] = '\0';
            len = 3;

            //	printf("Sending: \"%s\"\n", buf );
            send( sockfd, buf, len, 0 );
        }
    }
}

// Removes all elements from the send queue, freeing up the memory allocated to them
// Will not destroy the actual queue. New elements can still be added after running this
void clearSendQueue( SendQueue *q ) {
    SendQueue *toBeFreed;
    SendQueue *first = q;

    // Skip first entry
    q = q->next;
    while( q != NULL ) {
        toBeFreed = q;
        q = q->next;
        free( toBeFreed );
    }
    first->next = NULL;
}

// Destroys the send queue, freeing up all memory allocated to it.
// Not to be confused with clearSendQueue()
void destroySendQueue( SendQueue *q ) {
    clearSendQueue( q );
    free( q );
}

// Draws a circle on the specified x,y with specified radius on target image
void drawCircle( int x, int y, int radius, IplImage *target ) {
    CvPoint center = { x, y };

    cvCircle( target, center, 2, cvScalar( GREEN ), -1, 8, 0 );
    cvCircle( target, center, radius, cvScalar( BLUE ), 1, 8, 0 );
}

/* Return 1 if the difference is negative, otherwise 0.  */
int timeval_subtract( struct timeval *result, struct timeval *t2, struct timeval *t1 ) {
    long int diff =( t2->tv_usec + 1000000 * t2->tv_sec ) - ( t1->tv_usec + 1000000 * t1->tv_sec );
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return( diff<0 );
}

// A function to calculate the transformation matrix used for perspective transformation
void calculateTransformationMatrix( BoundingBox* from, BoundingBox* to, CvMat* transMat ) {
    CvPoint2D32f from_arr[] = { 
        cvPointTo32f( from->topLeft ),
        cvPointTo32f( from->topRight ),
        cvPointTo32f( from->bottomRight ),
        cvPointTo32f( from->bottomLeft )
    };
    CvPoint2D32f to_arr[] = {
        cvPointTo32f( to->topLeft ),
        cvPointTo32f( to->topRight ),
        cvPointTo32f( to->bottomRight ),
        cvPointTo32f( to->bottomLeft )
    };
    cvGetPerspectiveTransform( from_arr, to_arr, transMat );
}

// The callback function for a click while in calibration mode
// Sets the state to GRAB_DOTS after the last point is calibrated
void calibrateClick( int event, int x, int y, int flags, void* param ) {
    ClickParams * clickParams =( ClickParams*) param;
    int* currentCalibrationPoint =( int* ) &clickParams->currentPoint;
    BoundingBox* DD_box = clickParams->DD_box;

    if( state == SELECT_MASK || state == SELECT_TRANSFORM ) { //State is zero when finding dots
        if( event == CV_EVENT_LBUTTONDOWN ) {
            if(*currentCalibrationPoint <= BOTTOM_LEFT ) { // This should be a unneccessary clause

                printf( "Calibrating %s point\n", pointTranslationTable[*currentCalibrationPoint]);
                CvPoint p = { x, y };
                switch( *currentCalibrationPoint ) { //TODO Potentiell minnesläcka här. Sätt x och y direkt istället
                    case TOP_LEFT:      DD_box->topLeft      = p; break;
                    case TOP_RIGHT:     DD_box->topRight     = p; break;
                    case BOTTOM_RIGHT:  DD_box->bottomRight  = p; break;
                    case BOTTOM_LEFT:   DD_box->bottomLeft   = p; break;
                }
                ++( *currentCalibrationPoint );
            }
        }
        else if( event == CV_EVENT_RBUTTONDOWN ) { //Allow skipping already well calibrated points
            printf( "Keeping old calibration of %s point\n", pointTranslationTable[*currentCalibrationPoint]);
            ++( *currentCalibrationPoint );
        }
        if( *currentCalibrationPoint > BOTTOM_LEFT ) {

            // When all corners are set, finish up and go back to detecting dots
            switch( state ) {
                case SELECT_MASK: /*Do nothing for now*/ 
                    break;
                case SELECT_TRANSFORM: 
                    calculateTransformationMatrix(
                            DD_box, clickParams->DD_transformation_to,
                            clickParams->transMat 
                            );
                    break;
            }
            state = GRAB_DOTS;
        }
    }
}

// Creates or destroys the warp window
void toggleWarpOutput( char state ) {
    if( state ) {
        cvNamedWindow( warpwindowname, CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL );
    }
    else {
        cvDestroyWindow( warpwindowname );
    }
}

void toggleCalibrationMode( char* calibrate_exposure, int* currentExposure ) {
    if( *calibrate_exposure ) {
        printf( "cancelled\n");
        *calibrate_exposure = 0;
    }
    else {
        printf( "Starting calibration... " );
        *calibrate_exposure = 1; 
        *currentExposure = 150;
    }
}

// Paints the edges of the calibration area
void paintOverlayPoints( IplImage* grabbedImage, BoundingBox* DD_box ) {
    cvCircle( grabbedImage, DD_box->topLeft, 2, cvScalar( BLUE ), -1, 8, 0 ); 
    cvLine( grabbedImage, DD_box->topLeft, DD_box->topRight, cvScalar( GREEN ), 1, 8, 0 );
    cvCircle( grabbedImage, DD_box->topRight, 2, cvScalar( BLUE ), -1, 8, 0 );
    cvLine( grabbedImage, DD_box->topRight, DD_box->bottomRight, cvScalar( GREEN ), 1, 8, 0 );
    cvCircle( grabbedImage, DD_box->bottomLeft, 2, cvScalar( BLUE ), -1, 8, 0 );
    cvLine( grabbedImage, DD_box->bottomRight, DD_box->bottomLeft, cvScalar( GREEN ), 1, 8, 0 );
    cvCircle( grabbedImage, DD_box->bottomRight, 2, cvScalar( BLUE ), -1, 8, 0 );
    cvLine( grabbedImage, DD_box->bottomLeft, DD_box->topLeft, cvScalar( GREEN ), 1, 8, 0 );
}

// Activates the shape detector to automagically detect the corners of the screen.
// It look for a rectangular shape with a triangle inside it for each corner.
// Once it finds four( or more ) it calculates a new transformateion matrix and returns
void makeCalibrate( BoundingBox* from, BoundingBox* to, CvMat* t, CvCapture* c, int captureControl, int i ){

    if(( enableAutoExposure( captureControl ) ) == -1 ) {
        fprintf( stderr, "ERROR: Cannot enable auto exposure \n" );
        //return EXIT_FAILURE;
    }
    if( shapeDetector( from, c, i ) ) {
        calculateTransformationMatrix( from, to, t );
    }
    if( ( disableAutoExposure( captureControl ) ) == -1 ) {
        fprintf( stderr, "ERROR: Cannot disable auto exposure \n" );
        //return EXIT_FAILURE;
    }
}

int openCamera( CvCapture** capture_ptr, int* capture_control ) {
    static int cam_index = 9; // Does anyone ever have more then 10 cams at the same time?
    int i=0;
    char device_path[15];



    if( *capture_ptr != NULL ) { //A camera has been opened already. We must close it first
        cvReleaseCapture( capture_ptr );
        close( *capture_control );
    }
    
    while( ( *capture_ptr = cvCaptureFromCAM( cam_index ) ) == NULL ) {
        cam_index = --cam_index == -1 ? 9 : cam_index;
        if( i++ > 10 ) {
            //We have made a full cycle and haven't found any camera. Lets fail
            fprintf( stderr, "No camera found. Failing :(\n" );
            return 0;
        }
    }

    snprintf( device_path, sizeof( device_path ), "/dev/video%d", cam_index );
    printf("control: %s\n", device_path );

    /* Disable auto exposure for the device and set it low */
    if( ( *capture_control = open( device_path, O_RDWR | O_NONBLOCK, 0 )) < 0 ) {
        fprintf( stderr, "ERROR opening V4L2 interface from %s \n", device_path ); 
    }

    cam_index = --cam_index == -1 ? 9 : cam_index;
    return 1;
}

// Runs the dot detector and sends detected dots to server on port TODO Implement headless. Needs more config options and/or possibly a config file first though
int run( const char *serverAddress, const int serverPort, char headless ) {
    char calibrate_exposure = 0, show = ~0, flip = 0, vflip = 0, done = 0, warp = 0; //"Boolean" values used in this loop
    char noiceReduction = 2; //Small counter, so char is still ok.
    int i, sockfd; //Generic counter
    int dp = 0, minDist = 29, param1 = 0, param2 = 5; // Configuration variables for circle detection 
    int minDotRadius = 1;
    int detected_dots; //Detected dot counter
    int returnValue = EXIT_SUCCESS;
    int captureControl; //File descriptor for low-level camera controls
    int currentExposure = 150;
    int maxExposure = 1250; //Maximum exposure supported by the camera TODO Get this from the actual camera
    Color min = { 0, 70, 0, 0 }; //Minimum color to detect
    Color max = { 255, 255, 255, 0 }; //Maximum color to detect
    CvScalar colorWhite = cvScalar( WHITE ); //Color to draw detected dots on black and white surface
    BoundingBox DD_mask; //The box indicating what should and what should not be considered for dot search
    BoundingBox DD_transform; //The box indicating the plane we are looking at( and as such is the plane we would transform from )
    BoundingBox DD_transform_to; //The plane we are transforming to
    CvCapture *capture = NULL; //The camera
    CvMemStorage *storage; //Low level memory area used for dynamic structures in OpenCV
    CvSeq *seq; //Sequence to store detected dots in
    IplImage *grabbedImage = NULL; //Raw image from camera( plus some overlay in the end )
    IplImage *imgThreshold = NULL; //Image with detected dots
    IplImage *mask = NULL; //Mask to be able to remove uninteresting areas
    IplImage *coloredMask = NULL; //Mask to be able to indicate above mask on output image
    CvFont font; //Font for drawing text on images
    SendQueue *queue; //Head of the linked list that is the send queue
    char strbuf[255]; //Generic buffer for text formatting( with sprintf())
    struct timeval oldTime, time, diff; //Structs for measuring FPS
    float lastKnownFPS = 0; //Calculated FPS
    CvMat* pointRealMat = cvCreateMat( 1,1,CV_32FC2 ); //Single point matrix for point transformation
    CvMat* pointTransMat = cvCreateMat( 1,1,CV_32FC2 ); //Single point matrix for point transformation
    CvMat* transMat = cvCreateMat( 3,3,CV_32FC1 ); //Translation matrix for transforming input to a straight rectangle
    ClickParams clickParams = { TOP_LEFT, NULL, &DD_transform_to, transMat }; //Struct holding data needed by mouse-click callback function

    // Set up network
    sockfd = initNetwork( serverAddress, serverPort );
    if( sockfd == -1 ) {
        fprintf( stderr, "ERROR: initNetwork returned -1\n");
        return EXIT_FAILURE;
    }
    queue = initSendQueue();

    if( openCamera( &capture, &captureControl ) == 0 ) {
        fprintf( stderr, "ERROR: capture is NULL \n" );
        return EXIT_FAILURE;
    }

    if( ( disableAutoExposure( captureControl ) ) == -1 ) {
        fprintf( stderr, "ERROR: Cannot disable auto exposure \n" );
        //return EXIT_FAILURE;
    }

    if( ( updateAbsoluteExposure( captureControl, currentExposure ) ) == 0 ) {
        fprintf( stderr, "ERROR: Cannot set exposure\n");
    }

    // Create a window in which the captured images will be presented
    cvNamedWindow( imagewindowname, CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL );

    // Create a window to hold the configuration sliders and the detection frame TODO This is kind of a hack. Make a better solution
    cvNamedWindow( configwindowname, CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL );

    // Create a window to hold the transformed image. Handy to see how the dots are translated, but not needed for functionality
    if( warp ) cvNamedWindow( warpwindowname, CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL );

    // Create sliders to adjust the lower color boundry
    cvCreateTrackbar( red_lable  , configwindowname, &min.red,   255, NULL );
    cvCreateTrackbar( green_lable, configwindowname, &min.green, 255, NULL );
    cvCreateTrackbar( blue_lable , configwindowname, &min.blue,  255, NULL );

    //Create sliters for the contour based dot detection
    cvCreateTrackbar( min_area_lable, configwindowname, &minDotRadius,255, NULL );

    /* Slider for manual exposure setting */
    cvCreateTrackbar( exposure_lable, configwindowname, &currentExposure, maxExposure, NULL );

    //Create the memory storage
    storage = cvCreateMemStorage( 0 );

    // void cvInitFont( font, font_face, hscale, vscale, shear=0, thickness=1, line_type=8 )
    cvInitFont( &font, CV_FONT_HERSHEY_PLAIN, 1, 1, 0, 1, 8 );

    // Grab an initial image to be able to fetch image size before the main loop.
    grabbedImage = cvQueryFrame( capture );

    //Move the two windows so both are visible at the same time
    cvMoveWindow( imagewindowname, 0, 10 );
    cvMoveWindow( configwindowname, grabbedImage->width+2, 10 );

    //TODO Move these three inits to a function
    // Set masking defaults TODO load from file? Specify file for this file loading?
    DD_mask.topLeft.x = 0;  
    DD_mask.topLeft.y = 0;

    DD_mask.topRight.x = grabbedImage->width-1;
    DD_mask.topRight.y = 0;

    DD_mask.bottomLeft.x = 0;
    DD_mask.bottomLeft.y = grabbedImage->height-1;

    DD_mask.bottomRight.x = grabbedImage->width-1;
    DD_mask.bottomRight.y = grabbedImage->height-1;

    // Set transformation defaults TODO load from file? Specify file for this file loading?
    DD_transform.topLeft.x = 0;  
    DD_transform.topLeft.y = 0;

    DD_transform.topRight.x = grabbedImage->width-1;
    DD_transform.topRight.y = 0;

    DD_transform.bottomLeft.x = 0;
    DD_transform.bottomLeft.y = grabbedImage->height-1;

    DD_transform.bottomRight.x = grabbedImage->width-1;
    DD_transform.bottomRight.y = grabbedImage->height-1;

    // Set the transformation destination
    DD_transform_to.topLeft.x = 0;  
    DD_transform_to.topLeft.y = 0;

    DD_transform_to.topRight.x = grabbedImage->width-1;
    DD_transform_to.topRight.y = 0;

    DD_transform_to.bottomLeft.x = 0;
    DD_transform_to.bottomLeft.y = grabbedImage->height-1;

    DD_transform_to.bottomRight.x = grabbedImage->width-1;
    DD_transform_to.bottomRight.y = grabbedImage->height-1;

    calculateTransformationMatrix( &DD_transform, &DD_transform_to, transMat );

    // Set callback function for mouse clicks
    cvSetMouseCallback( imagewindowname, calibrateClick, ( void* ) &clickParams );

    gettimeofday( &oldTime, NULL );

    // Main loop. Grabbs an image from cam, detects dots, sends dots,and prints dots to images and shows to user
    while( !done ) {

        //PROFILING_PRO_STAMP(); //Uncomment this and the one in the end of the while-loop, and comment all other PROFILING_* to profile main-loop

        // ------ Common actions
        cvClearMemStorage( storage );
        detected_dots = 0;

        //Grab a fram from the camera
        PROFILING_PRO_STAMP();
        grabbedImage = cvQueryFrame( capture );
        PROFILING_POST_STAMP( "cvQueryFrame");

        if( grabbedImage == NULL ) {
            fprintf( stderr, "ERROR: frame is null...\n" );
            getchar();
            returnValue = EXIT_FAILURE;
            break;
        }

        //Flip images to act as a mirror. 
        if( show && flip ) {
            cvFlip( grabbedImage, grabbedImage, 1 );
        }
        if( show && vflip ) {
            cvFlip( grabbedImage, grabbedImage, 0 );
        }

        // ------ State based actions
        switch( state ) {
            case GRAB_DOTS:

                //Create detection image
                imgThreshold = cvCreateImage( cvGetSize( grabbedImage ), 8, 1 );
                cvInRangeS( grabbedImage, cvScalar( DD_COLOR( min )), cvScalar( DD_COLOR( max )), imgThreshold );

                //Mask away anything not in our calibration area
                mask = cvCreateImage( cvGetSize( grabbedImage ), 8, 1 );
                cvZero( mask );
                cvFillConvexPoly( mask, ( CvPoint* ) &DD_mask, 4, cvScalar( WHITE ), 1, 0 );
                cvAnd( imgThreshold, mask, imgThreshold, NULL );

                // Invert mask, increase the number of channels in it and overlay on grabbedImage //TODO Tint the mask red before overlaying
                cvNot( mask, mask );
                coloredMask = cvCreateImage( cvGetSize( grabbedImage ), grabbedImage->depth, grabbedImage->nChannels );
                cvCvtColor( mask, coloredMask, CV_GRAY2BGR );
                cvAddWeighted( grabbedImage, 0.95, coloredMask, 0.05, 0.0, grabbedImage );


                // Reduce noise. 
                // Erode is kind of floor() of pixels, dilate is kind of ceil()
                // I'm not sure which gives the best result.
                switch( noiceReduction ) {
                    case 0: break; //No noice reduction at all
                    case 1: cvErode( imgThreshold, imgThreshold, NULL, 2 ); break;
                    case 2: cvDilate( imgThreshold, imgThreshold, NULL, 2 ); break;
                }

                // Warp the warp-image. We are reusing the coloredMask variable to save some space
                PROFILING_PRO_STAMP();
                if( show && warp ) cvWarpPerspective( grabbedImage, coloredMask, transMat, CV_INTER_LINEAR+CV_WARP_FILL_OUTLIERS, cvScalarAll( 0 ));
                PROFILING_POST_STAMP( "Warping perspective" );


                // Find all dots in the image
                PROFILING_PRO_STAMP();

                // Clear old data from seq
                seq = 0;

                // Find the dots
                cvFindContours(
                        imgThreshold,
                        storage,
                        &seq,
                        sizeof( CvContour ),
                        CV_RETR_LIST,
                        CV_CHAIN_APPROX_SIMPLE,
                        cvPoint( 0,0 )
                        );
                // cvFindContours destroys the original image, so we wipe it here
                // and then repaints the detected dots later
                cvZero( imgThreshold );

                PROFILING_POST_STAMP( "Dot detection" );

                //Process all detected dots
                PROFILING_PRO_STAMP();
                for( ; seq != 0; seq = seq->h_next ) {

                    // Calculate radius of the detected contour
                    CvRect rect =( ( CvContour * )seq )->rect;
                    float relCenterX = rect.width / 2;
                    float relCenterY = rect.height / 2;

                    // Make sure the dot is big enough
                    if( relCenterX < minDotRadius || relCenterY < minDotRadius ) {
                        continue;
                    }

                    // Note that we have found another dot
                    ++detected_dots;

                    // Transform the detected dot according to transformation matrix.
                    float absCenter[] = { rect.x + relCenterX, rect.y + relCenterY };
                    pointRealMat->data.fl = absCenter;
                    cvPerspectiveTransform( pointRealMat, pointTransMat, transMat );

                    // Draw the detected contour back to imgThreshold
                    // Draw the detected dot both to real image and to warped( if warp is active )
                    if( show ) {
                        cvDrawContours( imgThreshold, seq, colorWhite, colorWhite, -1, CV_FILLED, 8, cvPoint( 0,0 ) );
                        drawCircle( absCenter[0], absCenter[1], ( relCenterX + relCenterY ) / 2, grabbedImage );
                        if( warp ) {
                            drawCircle( pointTransMat->data.fl[0], pointTransMat->data.fl[1], ( relCenterX + relCenterY ) / 2, coloredMask );
                        }
                    }

                    // Add detected dot to to send queue
                    addPointToSendQueue( pointTransMat->data.fl, queue ); 
                }

                PROFILING_POST_STAMP("Painting dots");

                //Calculate framerate
                gettimeofday( &time, NULL );
                timeval_subtract( &diff, &time, &oldTime );
                lastKnownFPS = lastKnownFPS * 0.7 + ( 1000000.0 / diff.tv_usec ) * 0.3; //We naïvly assume we have more then 1 fps
                oldTime = time;

                //Send the dots detected this frame to the server
                PROFILING_PRO_STAMP();
                sendQueue( sockfd, queue );
                clearSendQueue( queue );
                PROFILING_POST_STAMP( "Sending dots" );

                /* If calibrating, do the calibration */
                if( calibrate_exposure ) {
                    int ret;
                    ret = calibrateExposureLow( captureControl, detected_dots, &currentExposure, DD_MAX_EXPOSURE, lastKnownFPS );
                    switch( ret ) {
                        case 0: // We are done. Let's leave calibration mode
                            calibrate_exposure = 0;
                            printf( "done\n" );
                            break;

                        case -1: // We hit the upper limit with no detected dots
                            fprintf( stderr, "Reached upper limit (%d). Aborting!\n", DD_MAX_EXPOSURE );
                            calibrate_exposure = 0;
                            break;

                        case -2: // We hit lower limit with more then one dot detected
                            fprintf( stderr, "Too bright. More then one dot found even with minimal exposure. Aborting!\n");
                            calibrate_exposure = 0;
                            break;

                        case -3: //No conclusive results.
                            fprintf( stderr, "No conclusive results. Giving up\n" );
                            calibrate_exposure = 0;
                            break;
                    }
                }

                break; //End of GRAB_DOTS

            case SELECT_TRANSFORM:
                //Falling through here. Poor man's multi-case clause. Not putting this in default as we might
                //want to do different things in these two some day.
            case SELECT_MASK:
                snprintf( strbuf, sizeof( strbuf ), "Select %s point", pointTranslationTable[clickParams.currentPoint]);
                cvDisplayOverlay( imagewindowname, strbuf, 5 );
                break; //End of SELECT_MASK and SELECT_TRANSFORM
        }

        // Paint the corners of the detecting area and the calibration area
        paintOverlayPoints( grabbedImage, &DD_transform );

        //Print some statistics to the image
        if( show ) {
            snprintf( strbuf, sizeof( strbuf ), "Dots: %i", detected_dots ); //Print number of detected dots to the screen
            cvPutText( grabbedImage, strbuf, cvPoint( 10, 20 ), &font, cvScalar( WHITE ));
            snprintf( strbuf, sizeof( strbuf ), "FPS: %.1f", lastKnownFPS );
            cvPutText( grabbedImage, strbuf, cvPoint( 10, 40 ), &font, cvScalar( WHITE ));
            cvCircle( grabbedImage, cvPoint( 15, 55 ), minDotRadius, cvScalar( min.blue, min.green, min.red, min.alpha ), -1, 8, 0 ); // Colors given in order BGR-A, Blue, Green, Red, Alpha
        }

        //Show images 
        PROFILING_PRO_STAMP();
        if( show ) {
            cvShowImage( configwindowname, imgThreshold );
            cvShowImage( imagewindowname, grabbedImage );
            if( warp ) cvShowImage( warpwindowname, coloredMask );
        }
        PROFILING_POST_STAMP("Showing images");

        //Release the temporary images
        cvReleaseImage( &imgThreshold );
        cvReleaseImage( &mask );
        cvReleaseImage( &coloredMask );

        /* Update exposure if needed */
        updateAbsoluteExposure( captureControl, currentExposure );
        cvSetTrackbarPos( exposure_lable, configwindowname, currentExposure );

        //If ESC key pressed, Key=0x10001B under OpenCV 0.9.7( linux version ),
        //remove higher bits using AND operator
        i = ( cvWaitKey( 10 ) & 0xff );
        switch( i ) {
            case 'g': 
                makeCalibrate( &DD_transform, &DD_transform_to, transMat, capture, captureControl, 20 );
                updateAbsoluteExposure( captureControl, currentExposure+1 );
                break;

            case 'e': 
                toggleCalibrationMode( &calibrate_exposure, &currentExposure );
                break; /* Toggles calibration mode */

            case 'c':
                openCamera( &capture, &captureControl );
                break;

            case 's': 
                show = ~show;
                break; //Toggles updating of the image. Can be useful for performance of slower machines... Or as frame freeze

            case 'm': 
                state = SELECT_MASK;
                clickParams.currentPoint = TOP_LEFT;
                clickParams.DD_box = &DD_mask;
                break; //Starts selection of masking area. Will return to dot detection once all four points are set

            case 't':
                state = SELECT_TRANSFORM;
                clickParams.currentPoint = TOP_LEFT;
                clickParams.DD_box = &DD_transform;
                break; //Starts selection of the transformation area. Returns to dot detection when done.

            case 'f':
                flip = ~flip;
                break; //Toggles horizontal flipping of the image
            case 'v':
                vflip = ~vflip;
                break; //Toggles vertical flipping of the image

            case 'w':
                warp = ~warp;
                toggleWarpOutput( warp );
                break; //Toggles showing the warped image

            case 'n':
                noiceReduction = ( noiceReduction + 1 ) % 3;
                break; //Cycles noice reduction algorithm

            case 'q': //falling through here to quit

            case  27: 
                done = 1;
                break; //ESC. Kills the whole thing( in a nice and controlled manner )
        }
        fflush( stdout ); //Make sure everything in the buffer is printed before we go on

        //PROFILING_POST_STAMP("Main loop");
    } //End of main while-loop

    // Release the capture device and do some housekeeping
    cvReleaseImage( &grabbedImage );
    cvReleaseCapture( &capture );
    cvReleaseMemStorage( &storage );
    cvDestroyWindow( imagewindowname );
    cvDestroyWindow( configwindowname );
    if( warp ) cvDestroyWindow( warpwindowname ); //If now warp it is already destroyed
    destroySendQueue( queue );
    close( sockfd );
    close( captureControl );
    return returnValue;
}

int main( int argc, char **argv ) {
    int ret;
    long int serverPort = DEFAULT_SERVER_PORT;
    char *serverAddress = NULL;
    switch( argc ) {
        case 3: 
            serverPort = strtol( argv[2], NULL, 10 );
            if( serverPort >= 65535 || serverPort <= 1024 ) {
                printf( "Invalid port number! Using %i\n", DEFAULT_SERVER_PORT );
                serverPort = DEFAULT_SERVER_PORT;
            }
        case 2:
            serverAddress = strdup( argv[1]);
        default:
            if( serverAddress == NULL ) {
                printf( "Invalid hostname! Using %s\n", DEFAULT_SERVER_ADDRESS );
                serverAddress = strdup( DEFAULT_SERVER_ADDRESS );
            }
    }
    printf( "Server address: %s\n", serverAddress );
    printf( "Server port: %d\n", ( int)serverPort );

    ret = run( serverAddress, serverPort, 1 );
    free( serverAddress );
    return ret;
}

