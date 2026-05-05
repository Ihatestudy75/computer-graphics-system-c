#ifndef RASTER_H
#define RASTER_H

#include "image.h"
#include "primitive.h"
#include "polygon.h"

typedef struct DrawState DrawState; // Forward declaration to avoid circular dependency

// Edge record structure definition for scanline fill algorithm
typedef struct{
    int yMax;               // upper y bound of the intersection with the current scanline
    float x;                // x coordinate of the intersection of the edge with the current scanline
    float inverseSlope;     // 1/m where m is the slope of the edge, m = (y2 - y1) / (x2 - x1)
    float zIntersect;       // z coordinate of the intersection of the edge with the current scanline (for z-buffering)
    float dzPerScanline;    // change in z per change in y along the edge (for z-buffering)
    Color cIntersect;       // color at the intersection of the edge with the current scanline
    Color dcPerScan;        // change in color per scanline along the edge (for Gouraud shading)
    // Phong shading: interpolate world-space normal and position across the polygon
    Vector nIntersect;      // interpolated normal at current scanline intersection
    Vector dnPerScan;       // change in normal per scanline
    Point pIntersect;       // interpolated 3D world position at current scanline intersection
    Point dpPerScan;        // change in 3D position per scanline
    // Texture mapping: interpolate (s/z, t/z) for perspective-correct UV
    float sOverZ;           // s/z at current scanline intersection
    float tOverZ;           // t/z at current scanline intersection
    float dsOverZPerScan;   // change in s/z per scanline
    float dtOverZPerScan;   // change in t/z per scanline
} EdgeRec;

// Dynamic array of edge records for the scanline fill algorithm
typedef struct EdgeVec {
    EdgeRec *e; // dynamic array of edges
    int n;      // number of edges currently in the vector
    int cap;    // capacity of the dynamic array
} EdgeVec;

/* Bresenham's line algorithm */
void raster_bresenham_line(Point p0, Point p1, Image *img, Color c);

/* Midpoint circle algorithm */
void raster_midpoint_circle(Point center, double radius, Image *img, Color c);

/* Midpoint ellipse algorithm */
void raster_midpoint_ellipse(Point center, double ra, double rb, Image *img, Color c);

/* Bresenham's circle algorithm */
void raster_bresenham_circle(Point center, double radius, Image *img, Color c);

/* Bresenham's ellipse algorithm */
void raster_bresenham_ellipse(Point center, double ra, double rb, Image *img, Color c);

/* Flood fill algorithm */
void raster_flood_fill(Image *img, int r, int c, Color fillColor, Color borderColor);

/* Scanline fill algorithm */
void raster_scanline_fill(Polygon *p, Image *img, Color fillColor, DrawState *ds);

/* Barycentric coordinates fill algorithm */
void raster_barycentric_fill(Polygon *p, Image *img, Color fillColor);

/* Line clipping algorithm 1: Subdivision method */
void raster_line_clip_subdivide(Point p0, Point p1, Image *img, Color c);

/* Liang-Barsky clipping against the image rectangle */
int raster_liang_barsky_clip(Line *l, Image *img);

/* Draw the visible part of a line using Liang-Barsky clipping */
void raster_line_clip_liang_barsky_draw(Point p0, Point p1, Image *img, Color c);

#endif
