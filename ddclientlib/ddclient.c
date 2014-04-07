#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "ddclient.h"

// Starts up the dot detector client part
// Input:   Address to bind to (currently unused)
//          port to listen to
// Returns: A socket to be passed to getDots()
int initDDclient(const char *serverAddress, const int serverPort) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char port[10];
    snprintf(port, sizeof(port), "%i", serverPort);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

    rv = getaddrinfo(NULL, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return -1;
    }    

    rv = bind(sockfd, p->ai_addr, p->ai_addrlen);
    if (rv == -1) {
        perror("bind");
        return -1;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

// Parses a single point formated as by the dot detector
// Input: result - A pointer to where the result should be stored
//        strPoint - The point to parse
// Returns: 1 if successful, 0 if failed
int parsePoint( float* result, char* strPoint ) {
    float x, y;
    char * parts = strtok( strPoint, "," );

    if( parts == NULL ) {
        fprintf( stderr, "parsePoint: Malformed input: \"%s\"\n", strPoint );
        return 0;
    }
    x = atof(parts);

    parts = strtok( NULL, "," );
    if( parts == NULL ) {
        fprintf( stderr, "parsePoint: Malformed input: \"%s\"\n", strPoint );
        return 0;
    }
    y = atof(parts);

    result[0] = x;
    result[1] = y;
    return 1;
}

// Parses to full string from the dot detector.
// Input: points - The pointer to an array of float points[DD_MAX_DOTS][2], where the result will be stored
//        inputString - The string as received from the dot detector
// Returns: The number of points added to the array
int parsePoints( float* points, char* inputString, int* seqNumber ) {
    int noPoints = 0;
    int inputLength = strlen( inputString );

    if( inputLength == 0 ) {
        fprintf( stderr, "Got an empty input string. That shouldn't be possible!\n" );
        return 0;
    }
    
    // Make sure this wasn't a ndd packade (no dots detected)
    if( strcmp( inputString, "ndd" ) != 0 ) {
        char* pointTok;
        int i = 0;

        while( inputString[i] != '#' ) {
            ++i;
        }
        inputString[i] = '\0';
        if( seqNumber != NULL ) {
            *seqNumber = atoi( inputString );
        }
        ++i;

        // For every point in the string
        for( pointTok = &inputString[i], i=0; pointTok[i] != '\0'; i++ ) {
            if( pointTok[i] == ' ' ) {
                pointTok[i] = '\0';
                noPoints += parsePoint( &points[noPoints*2], pointTok );
                pointTok = &pointTok[i+1];
                i = -1; // Should really be zero, but we must compensate for the i++
            }
        }
        noPoints += parsePoint( &points[noPoints*2], pointTok );
    }
    return noPoints;
}

// Get all received dots as an array of floats
// Input: sockfd - The UDP socket to read the dots from
//        resultMatrix - The array of floats where the parsed points should be stored
//        dotsUpdated - A boolean variable to indicate if the result matrix has been updated
//        seqNr - The sequence number of the current points. This may be lower then earlier
//                received sequence number. It is up to the application to ignore out of order
//                packages, should that be required.
// Return: The number of dots in the current result matrix
int getDots(int sockfd, float* resultMatrix, char* dotsUpdated, int* seqNr) {
    static char netBuf[SEND_BUF_SIZE];
    ssize_t receivedBytes = 0;
    static int dots;
    *dotsUpdated = 0;
    receivedBytes = recv( sockfd, netBuf, sizeof netBuf, MSG_DONTWAIT );
    netBuf[receivedBytes] = '\0'; // Make sure received data is null terminated
    if( receivedBytes > 0 ) {
        dots = 0;
        dots = parsePoints( resultMatrix, netBuf, seqNr );
        *dotsUpdated = 1;
    }
    return dots;
}
