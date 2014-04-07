#ifndef __DDCLIENT_H__
#define __DDCLIENT_H__

#ifndef DD_SEND_BUF_SIZE
#define DD_SEND_BUF_SIZE 1472 // Max payload of a single UDP package with MTU 1500 (the common default MTU)
#endif

#ifndef DD_MAX_DOTS
#define DD_MAX_DOTS 150
#endif

// Starts up the dot detector client part
// Input:   Address to bind to (currently unused)
//          port to listen to
// Returns: A socket to be passed to getDots()
int initDDclient( const char *serverAddress, const int serverPort );

// Parses a single point formated as by the dot detector
// Input: result - A pointer to where the result should be stored
//        strPoint - The point to parse
// Returns: 1 if successful, 0 if failed
int parsePoint( float* result, char* strPoint );

// Parses to full string from the dot detector.
// Input: points - The pointer to an array of float points[MAX_POINTS][2], where the result will be stored
//        inputString - The string as received from the dot detector
// Returns: The number of points added to the array
int parsePoints( float* points, char* inputString, int* seqNumber );

// Get all received dots as an array of floats
// Input: sockfd - The UDP socket to read the dots from
//        resultMatrix - The array of floats where the parsed points should be stored
//        dotsUpdated - A boolean variable to indicate if the result matrix has been updated
//        seqNr - The sequence number of the current points. This may be lower then earlier
//                received sequence number. It is up to the application to ignore out of order
//                packages, should that be required.
// Return: The number of dots in the current result matrix
int getDots(int sockfd, float* resultMatrix, char* dotsUpdated, int* seqNr);

#endif
