#include "ddhelpers.h"

static int pattern_size = 180;
static int tri_offset = 30;
static int screen_w = 0;
static int screen_h = 0;

void initDDhelpers( int screen_width, int screen_height, int pattern_rect_size, int triangle_offset ) {
    screen_w = screen_width;
    screen_h = screen_height;
    pattern_size = pattern_rect_size;
    tri_offset = triangle_offset;
}

void transformBase( float* v, const int from_dim, const int to_dim ) {
    *v = ( *v * (float)to_dim ) / (float)from_dim;
}

void transformDots( float* laser_point_buf, int no_dots ) {
    int i;
    for( i=0; i<no_dots*2; i += 2 ) {
        transformBase( &laser_point_buf[i], 640, screen_w ); // x
        transformBase( &laser_point_buf[i+1], 480, screen_h ); // y
    }
}

static void setRect( SDL_Rect* rect, int x, int y, int w, int h ) {
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
}

static void setPoly( Sint16* poly, size_t n, ... ) {
    int i;
    va_list values;
    va_start( values, n );
    for( i=0; i<n; ++i ) {
        poly[i] = (Sint16) va_arg( values, int );
    }
    va_end( values );
}

void drawCalibrationPattern( SDL_Renderer* renderer ) {
    static char first_run = 1;
    int i, j;

    static SDL_Rect rects[4];
    static Sint16 triangles_x[4][3];
    static Sint16 triangles_y[4][3];
    if( first_run ) {
        first_run = 0;
        setRect( &rects[0], 0                      , 0                      , pattern_size, pattern_size );
        setRect( &rects[1], screen_w - pattern_size, 0                      , pattern_size, pattern_size );
        setRect( &rects[2], screen_w - pattern_size, screen_h - pattern_size, pattern_size, pattern_size );
        setRect( &rects[3], 0                      , screen_h - pattern_size, pattern_size, pattern_size );


        setPoly( &triangles_x[0][0], 12,
                // Top left trinagle
                tri_offset                              ,
                pattern_size - tri_offset               ,
                tri_offset                              ,

                // Top right triangle
                screen_w + tri_offset - pattern_size    ,
                screen_w - tri_offset                   ,
                screen_w - tri_offset                   ,

                // Bottom right tringle
                screen_w + tri_offset - pattern_size    ,
                screen_w - tri_offset                   ,
                screen_w - tri_offset                   ,

                // Bottom left triangle
                tri_offset                              ,
                tri_offset                              ,
                pattern_size - tri_offset               
               );

        setPoly( &triangles_y[0][0], 12,
                // Top left trinagle
                tri_offset                              ,
                tri_offset                              ,
                pattern_size - tri_offset               ,

                // Top right triangle
                tri_offset                              ,
                tri_offset                              ,
                pattern_size - tri_offset               ,

                // Bottom right tringle
                screen_h - tri_offset                   ,
                screen_h + tri_offset - pattern_size    ,
                screen_h - tri_offset                   ,

                // Bottom left triangle
                screen_h + tri_offset - pattern_size    ,
                screen_h - tri_offset                   ,
                screen_h - tri_offset
               );
    }

    SDL_RenderFillRects( renderer, rects, 4 );
    for( i=0; i<4; ++i ) {
        filledPolygonRGBA( renderer, &triangles_x[i][0], &triangles_y[i][0], 3, 0x00, 0x00, 0x00, 0xFF );
    }

}

