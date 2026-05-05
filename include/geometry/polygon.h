#ifndef POLYGON_H
#define POLYGON_H

#include "image.h"
#include "primitive.h"
#include "light.h"

typedef struct DrawState DrawState; // Forward declaration to avoid circular dependency

// 2D texture coordinate structure (s, t) in [0,1]
typedef struct {
    float s;
    float t;
} TexCoord;

// Polygon structure definition
typedef struct{
    int oneSided;       // 1 for one-sided, 2 for two-sided
    int nVertex;        // number of vertices
    Point *vertex;      // array of vertices (screen-space after VTM)
    Color *color;       // array of colors for each vertex (for Gouraud shading)
    Vector *normal;     // array of normals for each vertex (world-space, for lighting)
    Point *worldVertex; // array of world-space vertex positions (for Phong per-pixel lighting)
    TexCoord *texCoord; // array of (s,t) texture coordinates per vertex
    int zBuffer;        // 1 for z-buffering enabled (default), 0 for disabled
} Polygon;

/* Create an allocated polygon pointer initialized with 0 vertices and vertex = NULL */
Polygon* polygon_create();

/* Create an allocated polygon pointer with the vertex list initialized to a copy of the provided array of points */
Polygon* polygon_createp(int numV, Point *vList);

/* Free the internal data for a polygon and the polygon pointer */
void polygon_free(Polygon *p);

/* Initialize the existing polygon to an empty polygon with 0 vertices and vertex = NULL */
void polygon_init(Polygon *p);

/* Initialize the vertex list of the polygon to the given array of points */
void polygon_set(Polygon *p, int numV, Point *vList);

/* Free the internal data and reset the fields */
void polygon_clear(Polygon *p);

/* Set the oneSided field to the given value */
void polygon_setSided(Polygon *p, int oneSided);

/* Initialize the color array of the polygon to the given array of colors */
void polygon_setColors(Polygon *p, int numV, Color *cList);

/* Initialize the normal array of the polygon to the given array of vectors */
void polygon_setNormals(Polygon *p, int numV, Vector *nList);

/* Initialize the texture coordinate array of the polygon */
void polygon_setTexCoords(Polygon *p, int numV, TexCoord *tcList);

/* Shade the polygon using the given normal list */
void polygon_shade(Polygon *p, Lighting *lighting, DrawState *ds);

/* 
    Initialize the vertex list to the given array of points, the color array to the given array of colors, 
    and the normal array to the given array of vectors, and the zBuffer and oneSided fields to the given values 
*/
void polygon_setAll(Polygon *p, int numV, Point *vList, Color *cList, Vector *nList, int zBuffer, int oneSided);

/* Set the zBuffer field to the given value */
void polygon_setZBuffer(Polygon *p, int zBuffer);

/* Deallocated/allocate space and copy the vertex, color, and normal data from another polygon */
void polygon_copy(Polygon *dest, const Polygon *src);

/* Print the polygon data to the stream designated by the FILE pointer */
void polygon_print(const Polygon *p, FILE *fp);

/* Normalize the x and y values of each vertex by the homogeneous coordinate */
void polygon_normalize(Polygon *p);

/* Draw the outline of the polygon using the given color */
void polygon_draw(Polygon *p, Image *src, Color c);

/* Draw the filled polygon using color c with the scanline z-buffer rendering algorithm */
void polygon_drawFill(Polygon *p, Image *src, Color c);

/* Draw the filled polygon using color c with the Barycentric coordinates method */
void polygon_drawFillB(Polygon *p, Image *src, Color c);

/* Draw the polygon using the shading settings in DrawState */
void polygon_drawShade(Polygon *p, Image *src, DrawState *ds, Lighting *lighting);

#endif
