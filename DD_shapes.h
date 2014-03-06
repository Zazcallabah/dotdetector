#ifndef __DD_SHAPES_H__
#define __DD_SHAPES_H__

typedef struct BoundingBox {
    CvPoint topLeft;
    CvPoint topRight;
    CvPoint bottomRight;
    CvPoint bottomLeft;
} BoundingBox;

#endif
