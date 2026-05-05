#include "polygon.h"
#include "raster.h"
#include "module.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Create an allocated polygon pointer initialized with 0 vertices and vertex = NULL */
Polygon* polygon_create(){
    Polygon *p = (Polygon *)malloc(sizeof(Polygon));
    if(p){
        p->oneSided = 1;
        p->nVertex = 0;
        p->vertex = NULL;
        p->color = NULL;
        p->normal = NULL;
        p->worldVertex = NULL;
        p->texCoord = NULL;
        p->zBuffer = 1;
    } else {
        fprintf(stderr, "Failed to allocate memory for polygon\n");
    }
    return p;
}

/* Create an allocated polygon pointer with the vertex list initialized to a copy of the provided array of points */
Polygon* polygon_createp(int numV, Point *vList){
    Polygon *p = polygon_create();
    if(p){
        polygon_set(p, numV, vList);
    } else {
        fprintf(stderr, "Failed to create polygon with vertex list\n");
        return NULL;
    }

    if(p->vertex == NULL){
        fprintf(stderr, "Failed to initialize vertex list for polygon\n");
    }
    return p;
}

/* Free the internal data for a polygon and the polygon pointer */
void polygon_free(Polygon *p){
    if(!p) return; // if p is NULL, there is nothing to free

    // Free the internal data (vertex list, color list, normal list) if they exist to avoid memory leaks
    if(p->vertex) free(p->vertex);
    if(p->color) free(p->color);
    if(p->normal) free(p->normal);
    if(p->worldVertex) free(p->worldVertex);
    if(p->texCoord) free(p->texCoord);
    free(p);
}

/* Initialize the existing polygon to an empty polygon with 0 vertices and vertex = NULL */
void polygon_init(Polygon *p){
    if(p){
        p->oneSided = 1;
        p->nVertex = 0;
        p->vertex = NULL;
        p->color = NULL;
        p->normal = NULL;
        p->worldVertex = NULL;
        p->texCoord = NULL;
        p->zBuffer = 1;
    } else {
        fprintf(stderr, "Failed to initialize polygon: polygon pointer is NULL\n");
    }
}

/* Initialize the vertex list of the polygon to the given array of points */
void polygon_set(Polygon *p, int numV, Point *vList){
    if(!p){ // check if p is NULL before trying to set the vertex list
        fprintf(stderr, "Failed to set vertex list: polygon pointer is NULL\n");
        return;
    }
    // 1. Free any existing vertex list to avoid memory leaks
    if(p->vertex) free(p->vertex);
    p->nVertex = 0;
    p->vertex = NULL;

    if(p->color){
        free(p->color);
        p->color = NULL;
    }

    if(p->normal){
        free(p->normal);
        p->normal = NULL;
    }

    if(numV <= 0 || !vList){
        fprintf(stderr, "Failed to set vertex list: number of vertices must be positive and vertex list cannot be NULL\n");
        return;
    }

    // 2. Allocate new memory for the vertex list and copy the provided vertices
    p->vertex = (Point *)malloc(sizeof(Point) * numV);
    if(p->vertex){
        memcpy(p->vertex, vList, sizeof(Point) * numV);
        p->nVertex = numV; // Update the number of vertices
    } else {
        fprintf(stderr, "Failed to allocate memory for vertex list\n");
        return;
    }
}

/* Free the internal data and reset the fields */
void polygon_clear(Polygon *p){
    if (p)
    {
        if(p->vertex) free(p->vertex);
        if(p->color) free(p->color);
        if(p->normal) free(p->normal);
        if(p->worldVertex) free(p->worldVertex);
    if(p->texCoord) free(p->texCoord);
        p->vertex = NULL;
        p->color = NULL;
        p->normal = NULL;
        p->worldVertex = NULL;
        p->texCoord = NULL;
    }
    polygon_init(p);
}

/* Set the oneSided field to the given value */
void polygon_setSided(Polygon *p, int oneSided){
    if(p){
        p->oneSided = oneSided;
    } else {
        fprintf(stderr, "Failed to set oneSided: polygon pointer is NULL\n");
    }
}

/* Initialize the color array of the polygon to the given array of colors */
void polygon_setColors(Polygon *p, int numV, Color *cList){
    if(!p){
        fprintf(stderr, "Failed to set color list: polygon pointer is NULL\n");
        return;
    }

    // Free any existing color list to avoid memory leaks
    if(p->color) free(p->color);
    p->color = NULL;

    if(numV <= 0 || !cList){
        fprintf(stderr, "Failed to set color list: number of colors must be positive and color list cannot be NULL\n");
        return;
    }

    // Update the number of vertices in case it hasn't been set yet
    if(p->nVertex != numV) p->nVertex = numV;

    // Allocate new memory for the color list and copy the provided colors
    p->color = (Color *)malloc(sizeof(Color) * numV);
    if(p->color){
        for(int i = 0; i < numV; i++){
            p->color[i] = cList[i];
        }
    } else {
        fprintf(stderr, "Failed to allocate memory for color list\n");
    }
}

/* Initialize the normal array of the polygon to the given array of vectors */
void polygon_setNormals(Polygon *p, int numV, Vector *nList){
    if(!p){
        fprintf(stderr, "Failed to set normal list: polygon pointer is NULL\n");
        return;
    }

    // Free any existing normal list to avoid memory leaks
    if(p->normal) free(p->normal);
    p->normal = NULL;

    if(numV <= 0 || !nList){
        fprintf(stderr, "Failed to set normal list: number of normals must be positive and normal list cannot be NULL\n");
        return;
    }
    
    // Update the number of vertices in case it hasn't been set yet
    if(p->nVertex != numV) p->nVertex = numV;

    // Allocate new memory for the normal list and copy the provided normals
    p->normal = (Vector *)malloc(sizeof(Vector) * numV);
    if(p->normal){
        for(int i = 0; i < numV; i++){
            p->normal[i] = nList[i];
        }
    } else {
        fprintf(stderr, "Failed to allocate memory for normal list\n");
    }
}

/* Shade the polygon using the given normal list */
void polygon_shade(Polygon *p, Lighting *lighting, DrawState *ds){
    if(!p || !lighting || !ds){
        fprintf(stderr, "Failed to shade polygon: polygon, lighting, or DrawState pointer is NULL\n");
        return;
    }

    if(p->nVertex <= 0 || !p->vertex || !p->normal){
        fprintf(stderr, "Failed to shade polygon: polygon must have vertices and normals\n");
        return;
    }

    if(p->color){
        free(p->color);
        p->color = NULL;
    }

    p->color = (Color *)malloc(sizeof(Color) * p->nVertex);
    if(!p->color){
        fprintf(stderr, "Failed to allocate memory for polygon colors\n");
        return;
    }

    for(int i = 0; i < p->nVertex; i++){
        Vector v;
        vector_set(&v,
            ds->viewer.val[0] - p->vertex[i].val[0],
            ds->viewer.val[1] - p->vertex[i].val[1],
            ds->viewer.val[2] - p->vertex[i].val[2]
        );
        lighting_shading(lighting,
            &p->normal[i],
            &v,
            &p->vertex[i],
            &ds->bodyColor,
            &ds->surfaceColor,
            ds->surfaceCoeff,
            p->oneSided,
            &p->color[i]
        );
    }

    if(p->nVertex > 0){
        ds->flatColor = p->color[0];
    }
}

/* Initialize the vertex list to the given array of points, the color array to the given array of colors, 
and the normal array to the given array of vectors, and the zBuffer and oneSided fields to the given values */
void polygon_setAll(Polygon *p, int numV, Point *vList, Color *cList, Vector *nList, int zBuffer, int oneSided){
    if(!p){
        fprintf(stderr, "Failed to set all fields: polygon pointer is NULL\n");
        return;
    }

    // Set the vertex list, color list, and normal list using the existing functions
    polygon_set(p, numV, vList);
    if(cList) polygon_setColors(p, numV, cList);
    if(nList) polygon_setNormals(p, numV, nList);

    // Set the zBuffer and oneSided fields
    p->zBuffer = zBuffer;
    p->oneSided = oneSided;
}

/* Set the zBuffer field to the given value */
void polygon_setZBuffer(Polygon *p, int zBuffer){
    if(p){
        p->zBuffer = zBuffer;
    } else {
        fprintf(stderr, "Failed to set zBuffer: polygon pointer is NULL\n");
    }
}

/* Deallocated/allocate space and copy the vertex and color data from another polygon */
void polygon_copy(Polygon *dest, const Polygon *src){
    // Check if dest or src is NULL before trying to copy from it
    if(!dest || !src){
        fprintf(stderr, "Failed to copy polygon: source or destination polygon pointer is NULL\n");
        return;
    }

    // Free any existing vertex, color, and normal data in dest to avoid memory leaks
    polygon_clear(dest);

    // Copy the zBuffer and oneSided fields from src to dest
    dest->zBuffer = src->zBuffer;
    dest->oneSided = src->oneSided;

    // Reallocate space and copy the vertex list, color list, and normal list from src to dest using the existing functions
    polygon_set(dest, src->nVertex, src->vertex);
    if(src->color) polygon_setColors(dest, src->nVertex, src->color);
    if(src->normal) polygon_setNormals(dest, src->nVertex, src->normal);
    if(src->texCoord){
        dest->texCoord = (TexCoord*)malloc(sizeof(TexCoord) * src->nVertex);
        if(dest->texCoord)
            memcpy(dest->texCoord, src->texCoord, sizeof(TexCoord) * src->nVertex);
    }
    if(src->worldVertex){
        dest->worldVertex = (Point*)malloc(sizeof(Point) * src->nVertex);
        if(dest->worldVertex)
            memcpy(dest->worldVertex, src->worldVertex, sizeof(Point) * src->nVertex);
    }
}

/* Print the polygon data to the stream designated by the FILE pointer */
void polygon_print(const Polygon *p, FILE *fp){
    if(!p || !fp){
        fprintf(stderr, "Failed to print polygon: polygon pointer or file pointer is NULL\n");
        return;
    }

    fprintf(fp, "Polygon:\n");
    fprintf(fp, "  oneSided: %d\n", p->oneSided);
    fprintf(fp, "  nVertex: %d\n", p->nVertex);
    fprintf(fp, "  zBuffer: %d\n", p->zBuffer);

    fprintf(fp, "  Vertices:\n");
    for(int i = 0; i < p->nVertex; i++){
        fprintf(fp, "    Vertex %d: (%.2lf, %.2lf, %.2lf, %.2lf)\n", i, p->vertex[i].val[0], p->vertex[i].val[1], p->vertex[i].val[2], p->vertex[i].val[3]);
    }

    if(p->color){
        fprintf(fp, "  Colors:\n");
        for(int i = 0; i < p->nVertex; i++){
            fprintf(fp, "    Color %d: (%.2lf, %.2lf, %.2lf)\n", i, p->color[i].c[0], p->color[i].c[1], p->color[i].c[2]);
        }
    }

    if(p->normal){
        fprintf(fp, "  Normals:\n");
        for(int i = 0; i < p->nVertex; i++){
            fprintf(fp, "    Normal %d: (%.2lf, %.2lf, %.2lf, %.2lf)\n", i, p->normal[i].val[0], p->normal[i].val[1], p->normal[i].val[2], p->normal[i].val[3]);
        }
    }
}

/* Normalize the x and y values of each vertex by the homogeneous coordinate */
void polygon_normalize(Polygon *p){
    if(!p){
        fprintf(stderr, "Failed to normalize polygon: polygon pointer is NULL\n");
        return;
    }

    for(int i = 0; i < p->nVertex; i++){
        double h = p->vertex[i].val[3]; // homogeneous coordinate
        if(h != 0.0){
            p->vertex[i].val[0] /= h; // x
            p->vertex[i].val[1] /= h; // y
            p->vertex[i].val[3] = 1.0; // set homogeneous coordinate to 1 after normalization
        } else {
            fprintf(stderr, "Warning: vertex %d has a homogeneous coordinate of 0, cannot normalize\n", i);
        }
    }
}

/* Draw the outline of the polygon using the given color */
void polygon_draw(Polygon *p, Image *src, Color c){
    if(!p || !src || p->nVertex <= 2){
        fprintf(stderr, "Failed to draw polygon outline: polygon pointer or image pointer is NULL\n");
        return;
    }
    int nV = p->nVertex;

    // Normalize the vertices before drawing to ensure they are in the correct coordinate space
    polygon_normalize(p);
    
    for(int i = 0; i < nV; i++){
        Point a = p->vertex[i];
        Point b = p->vertex[(i + 1) % nV]; // wrap around to the first vertex

        Line l;
        line_set(&l, a, b);
        line_zBuffer(&l, p->zBuffer); // Set the z-buffer flag for the line based on the polygon's z-buffer flag
        line_draw(&l, src, c);
    }
}

/* Draw the filled polygon using color c with the scanline z-buffer rendering algorithm */
void polygon_drawFill(Polygon *p, Image *src, Color c){
    if(!p || !src || p->nVertex <= 2){
        fprintf(stderr, "Failed to draw filled polygon: polygon pointer or image pointer is NULL, or polygon has 2 or fewer vertices\n");
        return;
    }

    DrawState ds; // Create a DrawState for potential shading calculations in raster_scanline_fill
    ds.color = c; // Set the color in the DrawState to the provided color
    ds.flatColor = c; // Set the flatColor to the provided color as a default
    ds.bodyColor = c; // Set the bodyColor to the provided color as a default
    ds.surfaceColor = c; // Set the surfaceColor to the provided color as a default
    ds.surfaceCoeff = 0.0f; // Set the surface coefficient to 0
    ds.shade = ShadeConstant; // Set the shading method to constant as a default
    ds.zBuffer = p->zBuffer; // Set the z-buffer flag in the DrawState based on the polygon's z-buffer flag
    point_set(&ds.viewer, 0.0, 0.0, -1.0, 1.0); // Set the viewer position in the DrawState to a default value
    
    polygon_drawShade(p, src, &ds, NULL); // Use the polygon_drawShade function to draw the filled polygon with the provided DrawState settings
}

/* Draw the filled polygon using color c with the Barycentric coordinates method */
void polygon_drawFillB(Polygon *p, Image *src, Color c){
    if(!p || !src || p->nVertex <= 2){
        fprintf(stderr, "Failed to draw filled polygon: polygon pointer or image pointer is NULL, or polygon has 2 or fewer vertices\n");
        return;
    }
    polygon_normalize(p);
    raster_barycentric_fill(p, src, c);
}

/* Draw the polygon using the shading settings in DrawState */
void polygon_drawShade(Polygon *p, Image *src, DrawState *ds, Lighting *lighting){
    (void)lighting;

    if(!p || !src || !ds || p->nVertex <= 2){
        fprintf(stderr, "Failed to draw shaded polygon: polygon pointer, image pointer, or DrawState pointer is NULL, or polygon has 2 or fewer vertices\n");
        return;
    }

    polygon_normalize(p); // Normalize the vertices before drawing to ensure they are in the correct coordinate space

    if(ds->shade == ShadeFrame){
        polygon_draw(p, src, ds->color);
    } else if(ds->shade == ShadeFlat){
        raster_scanline_fill(p, src, ds->flatColor, ds);
    } else {
        raster_scanline_fill(p, src, ds->color, ds); // Pass DrawState and Lighting for potential shading calculations
    }
}

/* Initialize the texture coordinate array of the polygon */
void polygon_setTexCoords(Polygon *p, int numV, TexCoord *tcList){
    if(!p || numV <= 0 || !tcList){
        fprintf(stderr, "polygon_setTexCoords: invalid arguments\n");
        return;
    }
    if(p->texCoord) free(p->texCoord);
    p->texCoord = (TexCoord*)malloc(sizeof(TexCoord) * numV);
    if(p->texCoord){
        memcpy(p->texCoord, tcList, sizeof(TexCoord) * numV);
    } else {
        fprintf(stderr, "polygon_setTexCoords: malloc failed\n");
    }
}
