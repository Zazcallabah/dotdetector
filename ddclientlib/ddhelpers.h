#include <stdarg.h>
#include <SDL2/SDL2_gfxPrimitives.h>

void initDDhelpers( int screen_width, int screen_height, int pattern_rect_size, int triangle_offset );
void transformBase( float* v, const int from_dim, const int to_dim );
void transformDots( float* laser_point_buf, int no_dots );
void drawCalibrationPattern( SDL_Renderer* renderer );
