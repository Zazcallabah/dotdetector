//TODO: refactor Pattern and Rectangle structs. Ugly as fuck

#include <cv.h>
#include <highgui.h>
#include <stdlib.h>
#include <stdio.h>
#include "DD_shapes.h"

char draw = 0;
char drawNoise = 0;
BoundingBox* box;
//********************  FUNCTION drawRect  *************************
//*******  This function draws CvRect rect on image img ************

void drawRect (CvRect rect, IplImage* img){
    /*
       Corners of rectangle rect:

       1-----2
       |     |
       |     |
       0-----3
       rect.x and rect.y = bottom left corner
       */

    char text[50];
    CvFont f;
    CvPoint pt[4], center;

    pt[0].x = rect.x;
    pt[0].y = rect.y;

    pt[1].x = rect.x;
    pt[1].y = rect.y + rect.height;

    pt[2].x = rect.x + rect.width;
    pt[2].y = rect.y + rect.height;

    pt[3].x = rect.x + rect.width;
    pt[3].y = rect.y;

    center.x = rect.x + rect.width/2;
    center.y = rect.y + rect.height/2;

    sprintf(text, "%d, %d", center.x, center.y);

    cvLine(img, pt[0], pt[1], cvScalar(0,0,255,0),2,8,0);
    cvLine(img, pt[1], pt[2], cvScalar(0,0,255,0),2,8,0);
    cvLine(img, pt[2], pt[3], cvScalar(0,0,255,0),2,8,0);
    cvLine(img, pt[3], pt[0], cvScalar(0,0,255,0),2,8,0);

    cvInitFont(&f, CV_FONT_HERSHEY_COMPLEX, 0.5, 0.5, 0, 1, 8 );

    cvPutText(img, text, center, &f, cvScalar (255,255,255,0));
    cvCircle(img, center, 3, cvScalar(255,0,0,0), -1, 8, 2);

}

//------------------------------end of drawRect-------------------------------

typedef struct Triangle {

    CvPoint pt[3];
    CvSeq* c;
} Triangle;

typedef struct Rectangle {

    int x;
    int y;
    CvPoint pt[4];
    CvSeq* c;
} Rectangle;

typedef struct Pentagon {

    CvPoint pt[5];
    CvSeq* c;
} Pentagon;

	
typedef struct Pattern {

    CvPoint center;
    CvSeq* c;
    Rectangle r;
    CvRect cv_rect;
} Pattern;

typedef struct Metric {

    int total;
    int numberOfPatterns;
    char keep;
    Pattern p_list[10];
} Metric;


//**************************** RATIO CHECK  ******************************
//*************************************************************************

static char ratioCheck(CvSeq* c1, CvSeq* c2, double areaRatioMax, double heightRatioMax){

double area1, area2;
double height1, height2;
double areaRatio, heightRatio;

CvRect r1, r2;

area1=fabs(cvContourArea(c1, CV_WHOLE_SEQ,0));
area2=fabs(cvContourArea(c2, CV_WHOLE_SEQ,0));

r1 = ((CvContour *) c1)->rect;
r2 = ((CvContour *) c2)->rect;
height1 = r1.height;
height2 = r2.height;

if(height1 > height2) areaRatio = height1/height2;
else areaRatio = height2/height1;

if(area1 > area2) areaRatio = area1/area2;
else areaRatio = area2/area1;

if(areaRatio <= areaRatioMax && heightRatio <= heightRatioMax) {
return 1;
}
return 0;
}
//-----------------------end areaRatioCheck---------------------------------


//***********************  FUNCTION triangleInRectangleTest  *********************************
//**  This function returns 1 iff all points in Triangle t is inside CvSeq (contour) c. ***
//********************************************************************************************

static char triangleInRectangleTest(CvSeq* c, struct Triangle* t) {
    if( cvPointPolygonTest( c, cvPointTo32f( t->pt[0]), 0) > 0 ) {
        if ( cvPointPolygonTest( c, cvPointTo32f( t->pt[1]), 0 ) > 0 ){
            if ( cvPointPolygonTest( c, cvPointTo32f( t->pt[2] ), 0 ) > 0 ){
                return 1;
            }
        }
    } else
        return 0;
}
//---------------------------------------------------------------------------------------------


// ******************************** FIND CORNER ************************************

int findCorner(CvPoint* c_list, int corner){

    int i, min=32000, value, ret;
    
    for(i=0; i<4; i++){
        if(corner == 0) value = sqrt( pow(c_list[i].x, 2) + pow(c_list[i].y, 2));           //find top left
        if(corner == 1) value = sqrt( pow(640-c_list[i].x, 2) + pow(c_list[i].y, 2));       //find top right
        if(corner == 2) value = sqrt( pow(640-c_list[i].x, 2) + pow(480-c_list[i].y, 2));   //find bottom right
        if(corner == 3) value = sqrt( pow(c_list[i].x, 2) + pow(480 - c_list[i].y, 2));     //find bottom left
        
        if(value < min){    
            min = value; 
            ret = i;
        }
    }    
    
return ret;

}
    //------------------------ end findCorner -------------------------------

    //--------------------------- findBox_r ---------------------------------
            //(do not touch, ever)
void findBox_r(struct Pattern* p_list, CvPoint* ret){
    int i, tempx, tempy, min=32000, value;
    CvPoint tl, bl, tr, br;
    struct Rectangle s_list[4];
    
    for(i=0; i<4; i++){
        s_list[i] = p_list[i].r;
    }
    
    //find top left
    for(i=0; i<4; i++){
        value = sqrt( pow(p_list[i].center.x, 2) + pow(p_list[i].center.y, 2)); //Uses x,y coords of the PATTERNS
        if(value < min){    
            min = value; 
            bl= s_list[i].pt[ findCorner(&s_list[i].pt[0], 0) ]; //Checks x,y coords of the corners of a single pattern
        }
    }
    min = 32000;
    
    //find top right
    for(i=0; i<4; i++){
        value = sqrt( pow(640-p_list[i].center.x, 2) + pow(p_list[i].center.y, 2)); 
        if(value < min){ 
            min = value; 
            br = s_list[i].pt[  findCorner(&s_list[i].pt[0], 1) ]; 
        }
    }
    min = 32000;    

    //find bottom right
    for(i=0; i<4; i++){
        value = sqrt( pow(640-p_list[i].center.x, 2) + pow(480-p_list[i].center.y, 2)); 
        if(value < min){ 
            min = value; 
            tr=s_list[i].pt[  findCorner(&s_list[i].pt[0], 2) ]; 
        }
    }
    min = 32000;

    //find bottom left
    for(i=0; i<4; i++){
        value = sqrt( pow(p_list[i].center.x, 2) + pow(480 - p_list[i].center.y, 2)); 
        if(value < min){ 
            min = value; 
            tl=s_list[i].pt[ findCorner(&s_list[i].pt[0], 3) ];
        }
    }

    ret[0] = bl;
    ret[1] = br;
    ret[2] = tl;
    ret[3] = tr;
    //--------------------------- end findBox_r ---------------------------------

}

    //--------------------------- findBox ---------------------------------
void findBox(CvPoint* s_list){
    int i, tempx, tempy, min=32000, value;
    CvPoint tl, bl, tr, br;

    //find top left
    for(i=0; i<4; i++){
        value = sqrt( pow(s_list[i].x, 2) + pow(480 - s_list[i].y, 2)); 
        if(value < min){ min = value; tl=s_list[i]; }
    }
    min = 32000;
    //find bottom left
    for(i=0; i<4; i++){
        value = sqrt( pow(s_list[i].x, 2) + pow(s_list[i].y, 2)); 
        if(value < min){ min = value; bl=s_list[i]; }
    }
    min = 32000;
    //find top right
    for(i=0; i<4; i++){
        value = sqrt( pow(640-s_list[i].x, 2) + pow(480-s_list[i].y, 2)); 
        if(value < min){ min = value; tr=s_list[i]; }
    }
    min = 32000;
    //find bottom right
    for(i=0; i<4; i++){
        value = sqrt( pow(640-s_list[i].x, 2) + pow(s_list[i].y, 2)); 
        if(value < min){ min = value; br=s_list[i]; }
    }

    s_list[0] = bl;
    s_list[1] = br;
    s_list[2] = tl;
    s_list[3] = tr;
    //--------------------------- end findBox ---------------------------------

}

//*************************  SHAPE PROCESSING  ****************************
//*************************************************************************
struct Metric shapeProcessing(IplImage* img, IplImage* source, int thresh){

    int t_counter=0;
    int r_counter=0;
    int p_counter=0;
    int s_counter=0;
    int i,j,k;
    double area;

    CvSeq* contour;
    CvSeq* result;   //hold sequence of points of a contour
    CvMemStorage *storage = cvCreateMemStorage(0); //storage area for all contours
    IplImage* tempImage;
    struct Metric m;
    
    
    struct Rectangle  rectList[4];
    CvPoint s_list[4];
    CvPoint polygon[20];
    CvPoint polygon2[20];
    CvPoint pt1,pt2[2],pt3[3],pt4[4],pt5[5];

    struct Pentagon p_list[1000];
    struct Triangle t_list[1000];
    struct Rectangle r_list[1000];

    tempImage = cvCreateImage(cvGetSize(img), 8, 1); 
    cvThreshold(source, tempImage, thresh, 255, CV_THRESH_BINARY);
    cvFindContours(tempImage, storage, &contour, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint (0,0));

    m.total=0;
    m.keep=0;

    while(contour){

        //obtain a sequence of points of the countour, pointed by the variable 'contour'
        result = cvApproxPoly(contour, sizeof(CvContour), storage, CV_POLY_APPROX_DP, cvContourPerimeter(contour)*0.04, 0);
        area = fabs(cvContourArea(result, CV_WHOLE_SEQ, 0));

        //*************************  FIND NOISE  *******************************

        if(drawNoise){

            if(result->total==1 && area < 10000){ //<-------- Check for noise

                if(drawNoise){
                    pt1 = *( (CvPoint*) cvGetSeqElem(result, 0));

                    cvCircle(img, pt1, 4, cvScalar(0,0,255,0), -1, 8, 2);
                }
            }

            if(result->total==2 && area < 10000){

                if(drawNoise){
                    pt2[0] = *( (CvPoint*) cvGetSeqElem(result, 0));
                    pt2[1] = *( (CvPoint*) cvGetSeqElem(result, 1));

                    cvLine(img, pt2[0], pt2[1], cvScalar(0,255,255,0),4,8,0);
                }
            }
        }
        //---------------------------------------------------------------------------


        //*************************  FIND TRIANGLES  *******************************

        if(result->total==3 && ( (area < 10000) || (drawNoise && area <=10)) ){  //to reduce noise
            if(area > 50){
                //iterating through each point
                for(i=0;i<3;i++){
                    t_list[t_counter].pt[i]  = *( (CvPoint*) cvGetSeqElem(result, i));
                }
                t_list[t_counter].c = contour;
                t_counter = t_counter + 1;
            } else {
                for(i=0;i<3;i++){
                    pt3[i]  = *( (CvPoint*) cvGetSeqElem(result, i));
                }
            }

            if(draw && area>10){
                cvLine(img, t_list[t_counter].pt[0], t_list[t_counter].pt[1], cvScalar(255,0,0,0),4,8,0);
                cvLine(img, t_list[t_counter].pt[1], t_list[t_counter].pt[2], cvScalar(255,0,0,0),4,8,0);
                cvLine(img, t_list[t_counter].pt[2], t_list[t_counter].pt[0], cvScalar(255,0,0,0),4,8,0);
            } 
            if(drawNoise && area<=10){ 
                cvLine(img, pt3[0], pt3[1], cvScalar(255,0,0,0),4,8,0);
                cvLine(img, pt3[1], pt3[2], cvScalar(255,0,0,0),4,8,0);
                cvLine(img, pt3[2], pt3[0], cvScalar(255,0,0,0),4,8,0);
            }
        }

        //---------------------------------------------------------------------------

        //*************************  FIND RECTANGLES  *******************************

        if(result->total==4 && ( (area < 100000) || (drawNoise && area <=10))){
            if(area > 50){
                //iterating through each point
                for(i=0;i<4;i++){
                    r_list[r_counter].pt[i] = *( (CvPoint*)cvGetSeqElem(result, i));
                }
                r_list[r_counter].c = contour;
                r_counter = r_counter + 1;
            } else {
                for(i=0;i<4;i++){
                    pt4[i]  = *( (CvPoint*) cvGetSeqElem(result, i));
                }
            }

            if(draw && area>10){
                cvLine(img, r_list[r_counter].pt[0], r_list[r_counter].pt[1], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, r_list[r_counter].pt[1], r_list[r_counter].pt[2], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, r_list[r_counter].pt[2], r_list[r_counter].pt[3], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, r_list[r_counter].pt[3], r_list[r_counter].pt[0], cvScalar(0,255,0,0),4,8,0);
            }

            if(drawNoise && area<=10){ 
                cvLine(img, pt4[0], pt4[1], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[1], pt4[2], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[2], pt4[3], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[3], pt4[0], cvScalar(0,255,0,0),4,8,0);
            }
        }

        //---------------------------------------------------------------------------

        //*************************  FIND PENTAGONS  *******************************

        if(result->total==5 && ( (area < 10000) || (drawNoise && area <=10))){
            if(area > 50){
                //iterating through each point
                for(i=0;i<5;i++){
                    p_list[p_counter].pt[i] = *( (CvPoint*)cvGetSeqElem(result, i));
                }
                p_list[p_counter].c = contour;
                p_counter = p_counter + 1;
            } else {
                for(i=0;i<5;i++){
                    pt5[i]  = *( (CvPoint*) cvGetSeqElem(result, i));
                }
            }

            if(draw && area>10){
                cvLine(img, p_list[p_counter].pt[0], p_list[p_counter].pt[1], cvScalar(255,255,0,0),4,8,0);
                cvLine(img, p_list[p_counter].pt[1], p_list[p_counter].pt[2], cvScalar(255,255,0,0),4,8,0);
                cvLine(img, p_list[p_counter].pt[2], p_list[p_counter].pt[3], cvScalar(255,255,0,0),4,8,0);
                cvLine(img, p_list[p_counter].pt[3], p_list[p_counter].pt[4], cvScalar(255,255,0,0),4,8,0);
                cvLine(img, p_list[p_counter].pt[4], p_list[p_counter].pt[0], cvScalar(255,255,0,0),4,8,0);
            }

            if(drawNoise && area<=10){ 
                cvLine(img, pt4[0], pt4[1], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[1], pt4[2], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[2], pt4[3], cvScalar(0,255,0,0),4,8,0);
                cvLine(img, pt4[3], pt4[0], cvScalar(0,255,0,0),4,8,0);
            }
        }

        //---------------------------------------------------------------------------

/*
        //*************************  FIND POLYGONS  ***** aawww yeaaahhh ************
        int total = result->total;
        if(total > 5 && area > 100){

            for(i=0; i<total; i++){

                polygon[i] = *( (CvPoint*)cvGetSeqElem(result, i));
            }
            p_counter++;

            if(draw){
                for(j=0; i<total-1; i++){
                    //polygon2[i] = polygon[i+1];
                    cvLine(img, polygon[i], polygon[i+1], cvScalar(255,20*i,255,0),1,8,0);
                }
                cvLine(img, polygon[i], polygon[0], cvScalar(255,20*i,255,0),4,8,0);
            }
        }
*/
        //obtain the next contour
        contour = contour->h_next; 
    } // End of while(1)


    //****************************  FIND CALIBRATION SHAPES  **********************************

    for(i=0; i<r_counter; i++) {

        for(j=0; j<t_counter; j++) {
            //Check if the triangle is inside the rectangle
            if( triangleInRectangleTest( r_list[i].c, &t_list[j]) ) {

                if(ratioCheck(r_list[i].c, t_list[j].c, 7, 2)) {

                    CvRect rect = ((CvContour *) r_list[i].c)->rect;
                    
                    //Store CvRect for use in drawRect
                    m.p_list[s_counter].cv_rect = rect;
                    
                    //Store corners of the rectangle
                    for(k=0; k<4; k++){
                        m.p_list[s_counter].r.pt[k] = r_list[i].pt[k]; 
                    }
                    
                    //Store center coords pf the pattern
                    m.p_list[s_counter].center.x = rect.x + rect.width/2;
                    m.p_list[s_counter].center.y = rect.y + rect.height/2; 
                    
                    s_counter++;
                    m.keep=1;
                }
            }
        }
    }
    m.numberOfPatterns = s_counter;
    m.total = r_counter + t_counter + p_counter;
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&tempImage);
    return m;
    //------------------------------------------------------------------------------------------
}

//------------------------------end of shapeProcessing--------------------------------------

//****************************** FIND DISTANCE *********************************************
// finds distance between 2 CvPoints using pythagoras
int findDistance(CvPoint p1, CvPoint p2){

    int xDist, yDist;

    xDist = abs(p1.x-p2.x);
    yDist = abs(p1.y-p2.y);

    return sqrt( pow(xDist,2) + pow(yDist,2) );
}
//-----------------------------------------------------------------------------------------


//*******************************  REDUCE PATTERNS  ***************************************
//*****************************************************************************************

int reducePatterns(Pattern* allPatterns, Pattern* reducedPatterns, int numberOfPatterns){

int i,j;
int reducedCounter=0;
int newPattern;
Pattern temp;

    for(i=0; i<numberOfPatterns; i++){
    
        newPattern = 1;
        temp = allPatterns[i];
        //Check if this pattern is within 10px of another pattern in the reduced pattern list.
        //If not, it is added to the reduced pattern list
        for(j=0; j<reducedCounter; j++){
            if(findDistance(temp.center, reducedPatterns[j].center) < 10){
                newPattern=0;
            }
        }
         
        if(newPattern){ 
            reducedPatterns[reducedCounter] = temp;
            reducedCounter++;
        }    
    }
    return reducedCounter;
}


//-----------------------------------------------------------------------------------------


//********************************  SET THRESHOLDS  ***************************************
//*****************************************************************************************

void setThresholds(int* threshold, struct Metric* metric, int numberOfIntervals){
    int i,j; //General purpose counters
    int markerCounter=0;
    int r;
    int min = metric[0].total;
    int min_i=0;

    char change = 1;
    char findMin = 1;

    //First we check if any layers are below the minimum metric (5 currently). Any layer below
    //minimum metric is changed
    for(i=0; i<numberOfIntervals; i++){
        if(metric[i].keep) {
            //printf("Layer %d locked.\n", i);
        }
        if(metric[i].total <= 5 && !metric[i].keep){//Check if this layer is below minimum metric

            r=rand() % 256;
            //Randomize a threshold and make sure no other layer is using that threshold
            for(j=0; j<numberOfIntervals; j++){
                if(r == threshold[j]){
                    r=rand() % 256;
                    j=-1;
                    change = 1;
                }
            }
            //printf("Threshold %d set to %d\n", i, r);
            threshold[i] = r;//Set new threshold for this layer
            findMin = 0;
        }
    }

    //If no layers are below the minimum metric we change the layer with the lowest metric
    if(findMin){
        //Find layer with the lowest metric
        for(i=0; i<numberOfIntervals; i++){

            if(metric[i].total < min && !metric[i].keep){
                min = metric[i].total;
                min_i = i;
            }
        }
        //printf("Min = %d\n", min);

        r=rand() % 256;
        //Randomize a threshold and make sure no other layer is using that threshold
        for(j=0; j<numberOfIntervals; j++){
            if(r == threshold[j]){
                r=rand() % 256;
                j=-1;
            }
        }
        //printf("Threshold %d set to %d\n", min_i, r);
        threshold[min_i] = r;//Set new threshold for this layer
    }

}

//------------------------------ end of setThresholds --------------------------------------



//***************************  MAIN  ***********************************
//**********************************************************************

int shapeDetector(BoundingBox* result, CvCapture* capture, int numberOfIntervals){

    int numberOfWindows = 0;
    int interval, start_t=45, end_t, span_t=65;
    int w_counter=0;
    int threshold[100];
    int i,j;
	int frameCounter=0;
    int totalNumberOfPatterns=0;
    int numberOfReducedPatterns=0;


    char dynamicThresholding=1;
    char run=1;
    char showWindow [100];
    char got_result = 0; //Used to know if we had great success of just got canceled

    struct Metric metric[100];
    IplImage* imgGrayScale;
    IplImage* img;

    Pattern allPatterns[100];
    Pattern reducedPatterns[10];

    CvPoint s_list[4];

    CvSeq*   contourArray[100];
    CvMemStorage *storage = cvCreateMemStorage(0); //storage area for all contours

    box = result;

    srand(time(NULL));

    for(i=0; i<100; i++) {
        showWindow[i] = 0;
    }

    //*********************  SET UP IMAGES AND DISPLAY WINDOWS  ***********************
    //*********************************************************************************

    img = cvQueryFrame(capture);
    imgGrayScale = cvCreateImage(cvGetSize(img), 8, 1);

    switch( numberOfWindows ) {
        case 3:
            cvNamedWindow("Threshold 2", CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL);
        case 2:
            cvNamedWindow("Threshold 3", CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL);
        case 1:
            cvNamedWindow("Threshold 1", CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL);
    }

    cvNamedWindow("Tracked", CV_WINDOW_AUTOSIZE | CV_WINDOW_KEEPRATIO | CV_GUI_NORMAL);
    cvCreateTrackbar("Threshold lower",   "Tracked", &start_t, 255,    NULL);
    cvCreateTrackbar("Threshold upper",   "Tracked", &end_t,  255,    NULL);
    //---------------------------------------------------------------------------------

    span_t = end_t - start_t;
    interval = span_t/numberOfIntervals;

    for(i=0; i<numberOfIntervals; i++){
        threshold[i] = start_t+((i+1)*interval);
    }

    while(run){ //Main loop

        //*********************  IMAGE PRE-PROCESSING  ****************************
	frameCounter++;
        img = cvQueryFrame(capture);

        //converting the original image into grayscale
        cvCvtColor(img,imgGrayScale,CV_BGR2GRAY);

        //---------------------------------------------------------------------------

        // Awesome shapeProcessing function calls
        for(i=0; i<numberOfIntervals; i++){
            metric[i] = shapeProcessing(img, imgGrayScale, threshold[i]);
                
	        //Append patterns found in the layers to allPatterns list
	        for(j=0; j<metric[i].numberOfPatterns; j++){
	        
	            allPatterns[totalNumberOfPatterns] = metric[i].p_list[j];
	            totalNumberOfPatterns++;
	        }            
        }


        // Reduce patterns
        numberOfReducedPatterns = reducePatterns(allPatterns, reducedPatterns, totalNumberOfPatterns);
    
        for(i=0; i<numberOfReducedPatterns; i++){
            drawRect(reducedPatterns[i].cv_rect, img);
        }        

        if(numberOfReducedPatterns == 4){
        
            findBox_r(&reducedPatterns[0], &s_list[0]);

            box->topLeft = s_list[0];
            box->topRight = s_list[1];
            box->bottomLeft = s_list[2];
            box->bottomRight = s_list[3];
            got_result = 1;
            run = 0;
        }        
    
        // Adjust thresholds 
        if(dynamicThresholding) {
            setThresholds(&threshold[0], &metric[0], numberOfIntervals);
        }
        else{
            span_t = end_t - start_t;
            interval = span_t/numberOfIntervals;

            for(i=0; i<numberOfIntervals; i++){
                threshold[i] = start_t+((i+1)*interval);
            }
        }

        numberOfReducedPatterns=0;
        totalNumberOfPatterns=0;

        //show the image in which identified shapes are marked   
        cvShowImage("Tracked",img);

        int input;
        input = cvWaitKey(10) & 0xff; //wait for a key press. also needed to repaint windows. Masks away bits higher then interesting

        switch(input){

            case 27: //esc-key
            case 'q':
            case 'e': run=0; break;
            case 'd': draw = !draw; break;
        }

    } //end of main while(run) loop

    //cleaning up
    switch( numberOfWindows ) {
        case 3:
            cvDestroyWindow("Threshold 2");
        case 2:
            cvDestroyWindow("Threshold 3");
        case 1:
            cvDestroyWindow("Threshold 1");
    }
    cvDestroyWindow("Tracked");
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&imgGrayScale);
    //cvReleaseImage(&img);
    printf("Number of frames: %d\n", frameCounter);
    return got_result;
}
