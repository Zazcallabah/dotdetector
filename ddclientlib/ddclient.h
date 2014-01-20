#ifndef __DDCLIENT_H__
#define __DDCLIENT_H__

#ifndef SEND_BUF_SIZE
#define SEND_BUF_SIZE 1472 // Max payload of a single UDP package with MTU 1500 (the common default MTU)
#endif

#ifndef MAX_POINTS
#define MAX_POINTS 150
#endif

int initDDclient( const char *serverAddress, const int serverPort );
int parsePoint( int* result, char* strPoint );
int parsePoints( int* points, char* inputString, int* seqNumber );

#endif
