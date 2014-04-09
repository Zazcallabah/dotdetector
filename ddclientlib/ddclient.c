#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>

#include "ddclient.h"

// Prototype for the listening thread function
static void* dotListenerThread( void* arg );

typedef struct DotArray {
    int size;
    unsigned long long sequence_number;
    float dot_buf[DD_MAX_DOTS];
    char updated;
} DotArray;

// This is the master dot array. Whatever is in this is the current truth
static DotArray master_dot_array;

// This is a semaphore to protect the master_dot_array
static sem_t mda_sem;

// This is the strategy that will be used when delivering packages
static enum strategy delivery_strategy;


// Starts up the dot detector client part
// Input:   Address to bind to (currently unused)
//          port to listen to
// Returns: 0 if successful, non-zero if an error occured.
//          Negative values indicates critical failures,
//          positive values indicates some non-critical failure
//          (but it's likely everything will come craching down anyway)
int initDDclient( const char *serverAddress, const int serverPort, enum strategy delivert_strat ) {
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

    delivery_strategy = delivert_strat;
    master_dot_array.size = 0;
    master_dot_array.updated = 1;

    // Prepare the semaphore which will protect the master array
    sem_init( &mda_sem, 0, 0 );

    // Prepare a thread to munch incomming dots
    pthread_t dot_listener;
    pthread_attr_t attr;

    // Set default thread attributes
    rv = pthread_attr_init( &attr );
    if( rv != 0 ) {
        fprintf( stderr, "Error: Failed to init default thread attributes\n" );
        return -2;
    }

    // Start the thread
    rv = pthread_create( &dot_listener, &attr, dotListenerThread, (void*) &sockfd );
    if( rv != 0 ) {
        fprintf( stderr, "Error: Failed to start listening thread\n" );
        return -3;
    }
    else {
        sem_wait( &mda_sem ); // Wait for listening thread to start
        sem_post( &mda_sem );
    }

    // Destroy the no longer needed thread attribute object
    rv = pthread_attr_destroy( &attr );
    if( rv != 0 ) {
        fprintf( stderr, "Warning: Failed to destroy thread attribute object. This is non-critical, but suspicious\n" );
        return 1;
    }
    return 0;
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
int parsePoints( float* points, char* inputString, unsigned long long* seqNumber ) {
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
            *seqNumber = atoll( inputString );
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

static void dotListenerCleanupHandler( void* vsockfd ) {
    //TODO Needed?
}

static void* dotListenerThread( void* vsockfd ) {
    int sockfd = *(int*) vsockfd;
    ssize_t received_bytes;
    char private_network_buffer[DD_SEND_BUF_SIZE];
    DotArray dot_array_private;
    dot_array_private.size = 0;
    sem_post( &mda_sem ); // mds_sem get to work a little extra as a startup monitor
    while( 1 ) {
        received_bytes = recv( sockfd, private_network_buffer, sizeof( private_network_buffer ), 0 );
        private_network_buffer[received_bytes] = '\0'; // Make sure received data is null terminated
        if( received_bytes > 0 ) {
            dot_array_private.size = parsePoints( &dot_array_private.dot_buf[0], private_network_buffer, &dot_array_private.sequence_number );
            sem_wait( &mda_sem ); // Make sure we have exclusive access to the master dot array
            memcpy( master_dot_array.dot_buf, dot_array_private.dot_buf, sizeof( master_dot_array.dot_buf ) );
            master_dot_array.sequence_number = dot_array_private.sequence_number;
            master_dot_array.size = dot_array_private.size;
            master_dot_array.updated = 1;
            sem_post( &mda_sem ); // Give up the exclusive rights to the master dot array
        }
    }
}

// Get all received dots as an array of floats
// Input: resultMatrix - The array of floats where the parsed points should be stored
//        dotsUpdated - A boolean variable to indicate if the result matrix has been updated
//        seqNr - The sequence number of the current points. This may be lower then earlier
//                received sequence number. It is up to the application to ignore out of order
//                packages, should that be required.
// Return: The number of dots in the current result matrix
int getDots( float* result_matrix, char* dots_updated, unsigned long long* sequence_number ) {
    static int dots = 0;
    switch( delivery_strategy ) { // Depending on current strategy, we either 
        case ORDERED:
            fprintf( stderr, "ERROR: Ordered dot fetching is not yet implemented. Getting LATEST instead\n" );
            //break //intentional fallthrough here until ORDERED is implemented

        case LATEST:
            if( master_dot_array.updated ) {
                sem_wait( &mda_sem );
                memcpy( result_matrix, master_dot_array.dot_buf, sizeof( master_dot_array.dot_buf ) );
                *dots_updated = master_dot_array.updated;
                *sequence_number = master_dot_array.sequence_number;
                master_dot_array.updated = 0;
                dots = master_dot_array.size;
                sem_post( &mda_sem );
            }
            else {
                *dots_updated = 0;
            }
            break;
    }
    return dots;
}
