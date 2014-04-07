#include <stdarg.h>
#ifndef NO_SDL
#include <SDL2/SDL2_gfxPrimitives.h>
#endif

// Setup information needed for the other functions provided here
// to work. Can be called more then once in case some information
// is changed (e.g. the resolution changes).
// To remove SDL2 and SDL2_gfx dependency, define NO_SDL when comiling.
// Input: screen_width - The width of the drawing area in pixels
//        screen_height - The height of the drawing area in pixels
//
//        Only available when compiling with SDL2 and SDL2_gfx:
//        pattern_rect_size - The size of one side of the square used for
//                            automatic calibration (see figure below)
//        triangle_offset - The offset from the square to the start of the
//                          triangle (see figure below)
//
//         pattern_rect_size
//                 v------^------v
//                 ---------------
//                 |             |
//                 | |\          |
//                 | |  \        |
//                 | |    \      |
//                 | |      \    |
//                 | |________\  |
// triangl_offset {|             |
//                 ---------------
//
#ifdef NO_SDL
void initDDhelpers( int screen_width, int screen_height );
#else
void initDDhelpers( int screen_width, int screen_height, int pattern_rect_size, int triangle_offset );
#endif

// Transforms the coordinates of the dots given by dotdetector to
// the resolution the program runs in (as set up with initDDhelper()).
// Input: laser_pointer_buf - The same float array that dotdetector returns
//                            the detected dots in
//        no_dots - The number of dots detected
void transformDots( float* laser_point_buf, int no_dots );

// Draws the calibration pattern as described in the description
// for initDDhelper(). It depends on SDL2 and SDL2_gfx. It is
// up to the programmer calling this function to make sure
// nothing is drawn over the pattern (hint: call this as the last
// thing before presenting to screen).
// Only available when compiling with SDL2 and SDL2_gfx support.
// Input: renderer - The SDL2 renderere to draw to
#ifndef NO_SDL
void drawCalibrationPattern( SDL_Renderer* renderer );
#endif
