#include "module.h"
#include "raster.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void module_setFaceNormals(Polygon *p, Point *pt, int nVertex){
    if(p == NULL || pt == NULL || nVertex < 3) return;

    Vector a, b, normal;
    Vector normals[4];

    vector_set(&a,
               pt[1].val[0] - pt[0].val[0],
               pt[1].val[1] - pt[0].val[1],
               pt[1].val[2] - pt[0].val[2]);
    vector_set(&b,
               pt[2].val[0] - pt[0].val[0],
               pt[2].val[1] - pt[0].val[1],
               pt[2].val[2] - pt[0].val[2]);
    vector_cross(&a, &b, &normal);
    vector_normalize(&normal);

    for(int i = 0; i < nVertex && i < 4; i++){
        normals[i] = normal;
    }
    polygon_setNormals(p, nVertex, normals);
}

/* Allocate and return an initialized but empty Element*/
Element *element_create(){
    Element *e = (Element *)malloc(sizeof(Element));
    if (e == NULL) {
        fprintf(stderr, "Error allocating memory for Element\n");
        exit(1);
    }
    e->type = ObjNone;
    e->obj = NULL;
    e->next = NULL;
    return e;
}

/* 
    Allocate an Element and store a duplicate of the data pointed to by obj in Element 
    Modules do not get duplicated.
    Handle each type of object separately in a case statement
*/
Element *element_init(ObjectType type, void *obj){
    Element *e = element_create();
    e->type = type;
    switch (type) {
        case ObjNone:{
            e->obj = NULL;
            break;
        }
        case ObjLine:{
            e->obj = malloc(sizeof(Line));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Line *)(e->obj) = *(Line *)obj; // Copy the Line data
            break;
        }
        case ObjPoint:{
            e->obj = malloc(sizeof(Point));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Point *)(e->obj) = *(Point *)obj; // Copy the Point data
            break;
        }
        case ObjPolyline:{
            Polyline *src = (Polyline *)obj;
            Polyline *des = polyline_create();
            if(des == NULL) {
                free(e);
                return NULL;
            }
            polyline_copy(des, src);
            e->obj = des; // Store the pointer to the new Polyline
            break;
        }
        case ObjPolygon: {
            Polygon *src = (Polygon *)obj;
            Polygon *des = polygon_create();
            if(des == NULL) {
                free(e);
                return NULL;
            }
            polygon_copy(des, src);
            e->obj = des; // Store the pointer to the new Polygon
            break;
        }
        case ObjIdentity:{
            e->obj = NULL; // No data needed for identity
            break;
        }
        case ObjMatrix:{
            e->obj = malloc(sizeof(Matrix));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Matrix *)(e->obj) = *(Matrix *)obj; // Copy the Matrix data
            break;
        }
        case ObjColor:{
            e->obj = malloc(sizeof(Color));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Color *)(e->obj) = *(Color *)obj; // Copy the Color data
            break;
        }
        case ObjBodyColor:{
            e->obj = malloc(sizeof(Color));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Color *)(e->obj) = *(Color *)obj; // Copy the Color data
            break;
        }
        case ObjSurfaceColor:{
            e->obj = malloc(sizeof(Color));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(Color *)(e->obj) = *(Color *)obj; // Copy the Color data
            break;
        }
        case ObjSurfaceCoeff:{
            e->obj = malloc(sizeof(float));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            *(float *)(e->obj) = *(float *)obj; // Copy the float data
            break;
        }
        case ObjLight:{
            e->obj = malloc(sizeof(Light));
            if (e->obj == NULL) {
                free(e);
                return NULL;
            }
            light_copy((Light *)e->obj, (Light *)obj);
            break;
        }
        case ObjModule:{
            e->obj = obj; // Do not duplicate Modules, just store the pointer
            break;
        }
        case ObjBezier:
            e->obj = NULL; // Bezier not yet deep-copied; store NULL to avoid garbage pointer
            break;
        case ObjTexture:
            e->obj = obj; // Texture is a shared resource; store pointer only (caller owns)
            break;
        default:{
            fprintf(stderr, "Unsupported ObjectType in element_init\n");
            free(e);
            return NULL;
        }
    }
    e->next = NULL;
    return e;
}

/* Free an Element and its associated object */
void element_delete(Element *e){
    if(e == NULL) return;

    switch(e->type){
        case ObjPoint:
        case ObjLine:
        case ObjMatrix:
        case ObjColor:
        case ObjBodyColor:
        case ObjSurfaceColor:
        case ObjSurfaceCoeff:
        case ObjLight:
            free(e->obj);
            break;

        case ObjPolyline:
            polyline_free((Polyline *)e->obj);
            break;

        case ObjPolygon:
            polygon_free((Polygon *)e->obj);
            break;

        case ObjModule:
            break;
        case ObjIdentity:
        case ObjNone:
        case ObjBezier: // Bezier not owned by element; nothing to free
        case ObjTexture: // Texture not owned by element; caller manages lifetime
            break;
        default:
            fprintf(stderr, "Unsupported ObjectType in element_delete\n");
            break;
    }

    free(e);
}

/* Allocate an empty Module */
Module *module_create(){
    Module *md = (Module *)malloc(sizeof(Module));
    if (md == NULL) {
        fprintf(stderr, "Error allocating memory for Module\n");
        return NULL;
    }
    md->head = NULL;
    md->tail = NULL;
    return md;
}

/* Clear the module's list of Element, freeing memory */
void module_clear(Module *md){
    if(md == NULL) return;
    Element *current = md->head;
    while (current != NULL) {
        Element *next = current->next;
        element_delete(current);
        current = next;
    }
    md->head = NULL;
    md->tail = NULL;
}

/* Free all of the memory associated with a Module, including the memory pointed*/
void module_delete(Module *md){
    if(md == NULL) return;
    module_clear(md);
    free(md);
}

/* Generic insert of an Element into the Module at the tail of the list */
void module_insert(Module *md, Element *e){
    if(md == NULL || e == NULL) return;
    e->next = NULL;
    if(md->head == NULL) {
        md->head = e;
        md->tail = e;
    } else {
        md->tail->next = e;
        md->tail = e;
    }
}

/* Add a pointer to the Module sub to the tail of the Module's list */
void module_module(Module *md, Module *sub){
    if(md == NULL || sub == NULL) return;
    Element *e = element_init(ObjModule, sub);
    module_insert(md, e);
}

/* Add Point p to the tail of the Module's list */
void module_point(Module *md, Point *p){
    if(md == NULL || p == NULL) return;
    Element *e = element_init(ObjPoint, p);
    module_insert(md, e);
}

/* Add Line l to the tail of the Module's list */
void module_line(Module *md, Line *l){
    if(md == NULL || l == NULL) return;
    Element *e = element_init(ObjLine, l);
    module_insert(md, e);
}

/* Add Polyline pl to the tail of the Module's list */
void module_polyline(Module *md, Polyline *pl){
    if(md == NULL || pl == NULL) return;
    Element *e = element_init(ObjPolyline, pl);
    module_insert(md, e);
}

/* Add Polygon pg to the tail of the Module's list */
void module_polygon(Module *md, Polygon *pg){
    if(md == NULL || pg == NULL) return;
    Element *e = element_init(ObjPolygon, pg);
    module_insert(md, e);
}

/* Object that sets the current transform to the identity, placed at the tail of the Module's list */
void module_identity(Module *md){
    if(md == NULL) return;
    Element *e = element_init(ObjIdentity, NULL);
    module_insert(md, e);
}

/* Matrix operand to add a translation matrix to the tail of the Module's list */
void module_translate2D(Module *md, double tx, double ty){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_translate2D(&m, tx, ty);

    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a scaling matrix to the tail of the Module's list */
void module_scale2D(Module *md, double sx, double sy){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_scale(&m, sx, sy, 1.0);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a rotation about the Z-axis to the tail of the Module's list */
void module_rotateZ(Module *md, float cth, float sth){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_rotateZ(&m, cth, sth);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a 2D shear matrix to the tail of the Module's list */
void module_shear2D(Module *md, double shx, double shy){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_shear2D(&m, shx, shy);    
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* 
    Draw the Module into the image using 
    the given view transformation matrix, Lighting and DrawState 
    by traversing the list of Elements 
   (For now, Lighting can be an empty structure)
*/
void module_draw(Module *md, Matrix *VTM, Matrix *GTM, DrawState *ds, Lighting *light, Image *src){
    // This function will be implemented in the next steps, for now it is just a placeholder
    if(md == NULL || VTM == NULL || GTM == NULL || ds == NULL || src == NULL) return;
    
    Matrix LTM;
    matrix_identity(&LTM); // Local Transformation Matrix starts as identity

    // Traverse the list of Elements in the Module
    for(Element *e = md->head; e != NULL; e = e->next) {
        // Process each element based on its type
        switch (e->type) {
            case ObjColor:{
                // Update the DrawState color
                drawstate_setColor(ds, (Color *)e->obj);
                break;
            }
            case ObjMatrix: {
                // Multiply the current LTM by the matrix in the element
                Matrix temp;
                matrix_multiply((Matrix *)e->obj, &LTM, &temp);
                matrix_copy(&LTM, &temp);
                break;
            }
            case ObjIdentity:{
                matrix_identity(&LTM);
                break;
            }
            case ObjModule:{
                Matrix TM;
                matrix_multiply(GTM, &LTM, &TM); // Compute the new GTM for the submodule

                // Update the DrawState
                DrawState *tempDS = drawstate_copy(ds);
                if(tempDS == NULL) {
                    return;
                }

                module_draw((Module *)e->obj, VTM, &TM, tempDS, light, src);
                free(tempDS);
                break;
            }
            case ObjPoint:{
                Point x, tmp;

                point_copy(&x, (Point *)e->obj);

                matrix_xformPoint(&LTM, &x, &tmp);
                x = tmp;
                matrix_xformPoint(GTM, &x, &tmp);
                x = tmp;
                matrix_xformPoint(VTM, &x, &tmp);
                x = tmp;

                point_normalize(&x);

                point_draw(&x, src, ds->color);
                break;
            }
            case ObjLine:{
                Line l;

                line_copy(&l, (Line *)e->obj);

                matrix_xformLine(&LTM, &l);
                matrix_xformLine(GTM, &l);
                matrix_xformLine(VTM, &l);

                line_normalize(&l);

                line_draw(&l, src, ds->color);
                break;
            }
            case ObjPolyline:{
                Polyline pl;
                polyline_init(&pl);
                polyline_copy(&pl, (Polyline *)e->obj);

                matrix_xformPolyline(&LTM, &pl);
                matrix_xformPolyline(GTM, &pl);
                matrix_xformPolyline(VTM, &pl);

                polyline_normalize(&pl);
                polyline_draw(&pl, src, ds->color);

                polyline_clear(&pl);
                break;
            }
            case ObjPolygon:{
                Lighting *savedLighting = ds->lighting; // remember caller's lighting
                Vector *savedNormals = NULL;
                Polygon pg;
                polygon_init(&pg);
                polygon_copy(&pg, (Polygon *)e->obj);

                matrix_xformPolygon(&LTM, &pg);
                matrix_xformPolygon(GTM, &pg);

                // Do lighting and shading calculations before applying the VTM, so that the lighting is calculated in world coordinates
                if(ds->shade == ShadeFlat || ds->shade == ShadeGouraud) {
                    polygon_shade(&pg, light, ds);
                } else if(ds->shade == ShadePhong && pg.normal) {
                    // Save world-space vertex positions before VTM for per-pixel lighting in rasterizer
                    if(pg.worldVertex) free(pg.worldVertex);
                    pg.worldVertex = (Point*)malloc(sizeof(Point) * pg.nVertex);
                    if(pg.worldVertex) {
                        memcpy(pg.worldVertex, pg.vertex, sizeof(Point) * pg.nVertex);
                        // Normalize homogeneous coords so w==1.0 for correct per-pixel positions
                        for(int wi = 0; wi < pg.nVertex; wi++)
                            point_normalize(&pg.worldVertex[wi]);
                    }
                    savedNormals = (Vector*)malloc(sizeof(Vector) * pg.nVertex);
                    if(savedNormals) {
                        memcpy(savedNormals, pg.normal, sizeof(Vector) * pg.nVertex);
                    }
                    ds->lighting = light;
                }

                matrix_xformPolygon(VTM, &pg);
                if(savedNormals && pg.normal) {
                    memcpy(pg.normal, savedNormals, sizeof(Vector) * pg.nVertex);
                }
                if(savedNormals) {
                    free(savedNormals);
                }

                polygon_normalize(&pg);

                polygon_drawShade(&pg, src, ds, light); // Use the shading settings in DrawState for drawing

                // Restore ds->lighting to its value before this polygon so subsequent
                // polygons (which may have had lighting set by the caller) are unaffected
                ds->lighting = savedLighting;

                polygon_clear(&pg);
                break;
            }
            case ObjBodyColor:{
                drawstate_setBody(ds, (Color *)e->obj);
                break;
            }
            case ObjSurfaceColor:{
                drawstate_setSurface(ds, (Color *)e->obj);
                break;
            }
            case ObjSurfaceCoeff:{
                drawstate_setSurfaceCoeff(ds, *(float *)e->obj);
                break;
            }
            case ObjLight:
                break;
            case ObjTexture:{
                // Set the current texture in DrawState; following polygons will use it
                ds->texture = (Image *)e->obj;
                break;
            }
            case ObjBezier: // TODO: Implement Bezier curve initialization
                break;
            default:{
                fprintf(stderr, "Unsupported ObjectType in module_draw\n");
                break;
            }
        }
    }
}

/* Matrix operand to add a 3D translation matrix to the tail of the Module's list */
void module_translate(Module *md, double tx, double ty, double tz){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_translate(&m, tx, ty, tz);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a 3D scaling matrix to the tail of the Module's list */
void module_scale(Module *md, double sx, double sy, double sz){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_scale(&m, sx, sy, sz);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a rotation about the X-axis to the tail of the Module's list */
void module_rotateX(Module *md, float cth, float sth){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_rotateX(&m, cth, sth);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* Matrix operand to add a rotation about the Y-axis to the tail of the Module's list */
void module_rotateY(Module *md, float cth, float sth){
    if(md == NULL) return;
    Matrix m;
    matrix_identity(&m);
    matrix_rotateY(&m, cth, sth);
    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}
    
/* Matrix operand to add a rotation that orients to the orthonormal axes u, v, w */
void module_rotateXYZ(Module *md, Vector *u, Vector *v, Vector *w){
    if(md == NULL || u == NULL || v == NULL || w == NULL) return;

    Matrix m;
    matrix_identity(&m);
    matrix_rotateXYZ(&m, u, v, w);

    Element *e = element_init(ObjMatrix, &m);
    module_insert(md, e);
}

/* 
    Add a unit cube, axis-aligned and centered on zero to the Module.
    If solid is zero, add only lines.
    If solid is non-zero, add only polygons. Each polygon has surface normal.
*/
void module_cube(Module *md, int solid){
    // 1. Define the 8 vertices of the cube
    Point vL[8];
    point_set(&vL[0], -0.5, -0.5, -0.5, 1); // 0
    point_set(&vL[1], 0.5, -0.5, -0.5, 1);  // 1
    point_set(&vL[2], 0.5, 0.5, -0.5, 1);   // 2
    point_set(&vL[3], -0.5, 0.5, -0.5, 1);  // 3
    point_set(&vL[4], -0.5, -0.5, 0.5, 1);  // 4
    point_set(&vL[5], 0.5, -0.5, 0.5, 1);   // 5
    point_set(&vL[6], 0.5, 0.5, 0.5, 1);    // 6
    point_set(&vL[7], -0.5, 0.5, 0.5, 1);   // 7

    // 2. If solid is zero, add lines for the edges of the cube
    if (solid == 0) {
        Line l;
        line_set(&l, vL[0], vL[1]);
        module_line(md, &l);
        line_set(&l, vL[1], vL[2]);
        module_line(md, &l);
        line_set(&l, vL[2], vL[3]);
        module_line(md, &l);
        line_set(&l, vL[3], vL[0]);
        module_line(md, &l);
        line_set(&l, vL[4], vL[5]);
        module_line(md, &l);
        line_set(&l, vL[5], vL[6]);
        module_line(md, &l);
        line_set(&l, vL[6], vL[7]);
        module_line(md, &l);
        line_set(&l, vL[7], vL[4]);
        module_line(md, &l);
        line_set(&l, vL[0], vL[4]);
        module_line(md, &l);
        line_set(&l, vL[1], vL[5]);
        module_line(md, &l);
        line_set(&l, vL[2], vL[6]);
        module_line(md, &l);
        line_set(&l, vL[3], vL[7]);
        module_line(md, &l);
    } else {
        Polygon p;
        Point face[4];
        Vector n[4];

        polygon_init(&p);

        // front face  z = +0.5
        face[0] = vL[4]; face[1] = vL[5]; face[2] = vL[6]; face[3] = vL[7];
        for(int i = 0; i < 4; i++) vector_set(&n[i], 0, 0, 1);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        // back face  z = -0.5
        face[0] = vL[0]; face[1] = vL[3]; face[2] = vL[2]; face[3] = vL[1];
        for(int i = 0; i < 4; i++) vector_set(&n[i], 0, 0, -1);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        // left face  x = -0.5
        face[0] = vL[0]; face[1] = vL[4]; face[2] = vL[7]; face[3] = vL[3];
        for(int i = 0; i < 4; i++) vector_set(&n[i], -1, 0, 0);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        // right face  x = +0.5
        face[0] = vL[1]; face[1] = vL[2]; face[2] = vL[6]; face[3] = vL[5];
        for(int i = 0; i < 4; i++) vector_set(&n[i], 1, 0, 0);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        // top face  y = +0.5
        face[0] = vL[3]; face[1] = vL[7]; face[2] = vL[6]; face[3] = vL[2];
        for(int i = 0; i < 4; i++) vector_set(&n[i], 0, 1, 0);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        // bottom face  y = -0.5
        face[0] = vL[0]; face[1] = vL[1]; face[2] = vL[5]; face[3] = vL[4];
        for(int i = 0; i < 4; i++) vector_set(&n[i], 0, -1, 0);
        polygon_set(&p, 4, face);
        polygon_setNormals(&p, 4, n);
        module_polygon(md, &p);
        polygon_clear(&p);
    }
}

/* 
    Add a unit pyramid centered roughly around the origin.
    Square base lies on y = -0.5, apex at y = +0.5.
    If solid is zero, add only lines.
    If solid is non-zero, add polygons.
*/
void module_pyramid(Module *md, int solid){
    if(md == NULL) return;

    Point v[5];
    point_set(&v[0], -0.5, -0.5, -0.5, 1.0);  // base back-left
    point_set(&v[1],  0.5, -0.5, -0.5, 1.0);  // base back-right
    point_set(&v[2],  0.5, -0.5,  0.5, 1.0);  // base front-right
    point_set(&v[3], -0.5, -0.5,  0.5, 1.0);  // base front-left
    point_set(&v[4],  0.0,  0.5,  0.0, 1.0);  // apex

    if(solid == 0){
        Line l;

        /* base square */
        line_set(&l, v[0], v[1]); module_line(md, &l);
        line_set(&l, v[1], v[2]); module_line(md, &l);
        line_set(&l, v[2], v[3]); module_line(md, &l);
        line_set(&l, v[3], v[0]); module_line(md, &l);

        /* side edges */
        line_set(&l, v[0], v[4]); module_line(md, &l);
        line_set(&l, v[1], v[4]); module_line(md, &l);
        line_set(&l, v[2], v[4]); module_line(md, &l);
        line_set(&l, v[3], v[4]); module_line(md, &l);
    }
    else{
        Polygon p;
        Point face[4];

        polygon_init(&p);

        /* base */
        face[0] = v[0]; face[1] = v[1]; face[2] = v[2]; face[3] = v[3];
        polygon_set(&p, 4, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        /* 4 triangular sides */
        face[0] = v[0]; face[1] = v[1]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[1]; face[1] = v[2]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[2]; face[1] = v[3]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[3]; face[1] = v[0]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
    }
}

/* 
    Add a unit octahedron centered at the origin.
    If solid is zero, add only lines.
    If solid is non-zero, add polygons.
*/
void module_octahedron(Module *md, int solid){
    if(md == NULL) return;

    Point v[6];
    point_set(&v[0],  0.0,  0.6,  0.0, 1.0);  // top
    point_set(&v[1],  0.0, -0.6,  0.0, 1.0);  // bottom
    point_set(&v[2], -0.6,  0.0,  0.0, 1.0);  // left
    point_set(&v[3],  0.6,  0.0,  0.0, 1.0);  // right
    point_set(&v[4],  0.0,  0.0,  0.6, 1.0);  // front
    point_set(&v[5],  0.0,  0.0, -0.6, 1.0);  // back

    if(solid == 0){
        Line l;

        /* top connections */
        line_set(&l, v[0], v[2]); module_line(md, &l);
        line_set(&l, v[0], v[3]); module_line(md, &l);
        line_set(&l, v[0], v[4]); module_line(md, &l);
        line_set(&l, v[0], v[5]); module_line(md, &l);

        /* bottom connections */
        line_set(&l, v[1], v[2]); module_line(md, &l);
        line_set(&l, v[1], v[3]); module_line(md, &l);
        line_set(&l, v[1], v[4]); module_line(md, &l);
        line_set(&l, v[1], v[5]); module_line(md, &l);

        /* middle ring edges */
        line_set(&l, v[2], v[4]); module_line(md, &l);
        line_set(&l, v[4], v[3]); module_line(md, &l);
        line_set(&l, v[3], v[5]); module_line(md, &l);
        line_set(&l, v[5], v[2]); module_line(md, &l);
    }
    else{
        Polygon p;
        Point face[3];

        polygon_init(&p);

        /* upper 4 triangles */
        face[0] = v[0]; face[1] = v[2]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[0]; face[1] = v[4]; face[2] = v[3];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[0]; face[1] = v[3]; face[2] = v[5];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[0]; face[1] = v[5]; face[2] = v[2];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        /* lower 4 triangles */
        face[0] = v[1]; face[1] = v[4]; face[2] = v[2];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[1]; face[1] = v[3]; face[2] = v[4];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[1]; face[1] = v[5]; face[2] = v[3];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
        polygon_init(&p);

        face[0] = v[1]; face[1] = v[2]; face[2] = v[5];
        polygon_set(&p, 3, face);
        module_polygon(md, &p);
        polygon_clear(&p);
    }
}

/*
    Add a cylinder to the Module using the given top and bottom center points.
    Adapted from Bruce Maxwell's Project 6 test6b cylinder example.
*/
void module_cylinder(Module *md, Point *topCenter, Point *bottomCenter, int sides){
    if(md == NULL || topCenter == NULL || bottomCenter == NULL) return;
    if(sides < 3) sides = 3;

    Polygon p;
    Point xtop, xbot;

    polygon_init(&p);
    point_copy(&xtop, topCenter);
    point_copy(&xbot, bottomCenter);

    for(int i = 0; i < sides; i++){
        Point pt[4];
        double x1 = cos(i * M_PI * 2.0 / sides);
        double z1 = sin(i * M_PI * 2.0 / sides);
        double x2 = cos(((i + 1) % sides) * M_PI * 2.0 / sides);
        double z2 = sin(((i + 1) % sides) * M_PI * 2.0 / sides);

        point_copy(&pt[0], &xtop);
        point_set3D(&pt[1], xtop.val[0] + x1, xtop.val[1], xtop.val[2] + z1);
        point_set3D(&pt[2], xtop.val[0] + x2, xtop.val[1], xtop.val[2] + z2);
        polygon_set(&p, 3, pt);
        module_setFaceNormals(&p, pt, 3);
        module_polygon(md, &p);

        point_copy(&pt[0], &xbot);
        point_set3D(&pt[1], xbot.val[0] + x2, xbot.val[1], xbot.val[2] + z2);
        point_set3D(&pt[2], xbot.val[0] + x1, xbot.val[1], xbot.val[2] + z1);
        polygon_set(&p, 3, pt);
        module_setFaceNormals(&p, pt, 3);
        module_polygon(md, &p);

        point_set3D(&pt[0], xbot.val[0] + x1, xbot.val[1], xbot.val[2] + z1);
        point_set3D(&pt[1], xbot.val[0] + x2, xbot.val[1], xbot.val[2] + z2);
        point_set3D(&pt[2], xtop.val[0] + x2, xtop.val[1], xtop.val[2] + z2);
        point_set3D(&pt[3], xtop.val[0] + x1, xtop.val[1], xtop.val[2] + z1);
        polygon_set(&p, 4, pt);
        module_setFaceNormals(&p, pt, 4);
        module_polygon(md, &p);
    }

    polygon_clear(&p);
}

/*
    Add an open tube/frustum aligned along the Y-axis to the Module.
    This creates only the side surface, not the top or bottom caps.
*/
void module_tube(Module *md, Point *topCenter, double topRadius, Point *bottomCenter, double bottomRadius, int sides){
    if(md == NULL || topCenter == NULL || bottomCenter == NULL) return;
    if(sides < 3) sides = 3;
    if(topRadius < 0.0) topRadius = 0.0;
    if(bottomRadius < 0.0) bottomRadius = 0.0;

    Polygon p;
    Point xtop, xbot;

    polygon_init(&p);
    point_copy(&xtop, topCenter);
    point_copy(&xbot, bottomCenter);

    /* Stitch neighboring top/bottom rim points into quad side panels. */
    for(int i = 0; i < sides; i++){
        Point pt[4];
        double x1 = cos(i * M_PI * 2.0 / sides);
        double z1 = sin(i * M_PI * 2.0 / sides);
        double x2 = cos(((i + 1) % sides) * M_PI * 2.0 / sides);
        double z2 = sin(((i + 1) % sides) * M_PI * 2.0 / sides);

        point_set3D(&pt[0], xbot.val[0] + bottomRadius * x1, xbot.val[1], xbot.val[2] + bottomRadius * z1);
        point_set3D(&pt[1], xbot.val[0] + bottomRadius * x2, xbot.val[1], xbot.val[2] + bottomRadius * z2);
        point_set3D(&pt[2], xtop.val[0] + topRadius * x2, xtop.val[1], xtop.val[2] + topRadius * z2);
        point_set3D(&pt[3], xtop.val[0] + topRadius * x1, xtop.val[1], xtop.val[2] + topRadius * z1);
        polygon_set(&p, 4, pt);
        module_setFaceNormals(&p, pt, 4);
        polygon_setSided(&p, 0);
        module_polygon(md, &p);
    }

    polygon_clear(&p);
}

/*
    Add a Utah teapot to the Module using the given number of divisions for the Bezier patches.
     If solid is zero, add only lines.
     If solid is non-zero, add polygons.
     Adapted from Bruce Maxwell's Project 6 test6b teapot example.
*/
void module_teapot(Module *md, int divisions, int solid){
    (void)solid;

    if(!md) return;

    /*
      Minimal placeholder teapot for API compatibility. It builds a rounded,
      shaded object from existing primitives so test9teapot can compile and
      render. A full Utah teapot implementation would replace this with the
      standard Bezier patch control net.
    */
    Module *body = module_create();
    if(!body) return;

    Point top, bottom;
    point_set3D(&top, 0.0, 0.9, 0.0);
    point_set3D(&bottom, 0.0, -0.7, 0.0);
    module_scale(body, 1.25, 1.0, 1.25);
    module_cylinder(body, &top, &bottom, 24 + divisions * 4);

    module_module(md, body);
}

static inline Point module_lerp_point(Point a, Point b, double t) {
    Point res;
    for(int i = 0; i < 4; i++){
        res.val[i] = (1 - t) * a.val[i] + t * b.val[i];
    }
    return res;
}

static inline void module_split_bezier_curve(Point *vlist, Point *left, Point *right) {
    if(!vlist || !left || !right) return;

    Point p0 = vlist[0];
    Point p1 = vlist[1];
    Point p2 = vlist[2];
    Point p3 = vlist[3];

    Point p01 = module_lerp_point(p0, p1, 0.5);
    Point p12 = module_lerp_point(p1, p2, 0.5);
    Point p23 = module_lerp_point(p2, p3, 0.5);
    Point p012 = module_lerp_point(p01, p12, 0.5);
    Point p123 = module_lerp_point(p12, p23, 0.5);
    Point p0123 = module_lerp_point(p012, p123, 0.5);

    left[0] = p0;
    left[1] = p01;
    left[2] = p012;
    left[3] = p0123;

    right[0] = p0123;
    right[1] = p123;
    right[2] = p23;
    right[3] = p3;
}

/* Use de Casteljau subdivision to add a Bezier curve to a Module. */
void module_bezierCurve(Module *m, BezierCurve *b, int div){
    if(!m || !b) return;

    if(div <= 0){
        Line l;
        for(int i = 0; i < 3; i++){
            line_set(&l, b->cp[i], b->cp[i + 1]);
            line_setz(&l, b->zbuffer);
            module_line(m, &l);
        }
        return;
    }

    BezierCurve leftHalf, rightHalf;
    Point leftCP[4], rightCP[4];
    Point vlist[4];

    for(int i = 0; i < 4; i++){
        vlist[i] = b->cp[i];
    }

    module_split_bezier_curve(vlist, leftCP, rightCP);

    for(int i = 0; i < 4; i++){
        leftHalf.cp[i] = leftCP[i];
        rightHalf.cp[i] = rightCP[i];
    }

    leftHalf.zbuffer = b->zbuffer;
    rightHalf.zbuffer = b->zbuffer;

    module_bezierCurve(m, &leftHalf, div - 1);
    module_bezierCurve(m, &rightHalf, div - 1);
}

/* Use de Casteljau subdivision to add a Bezier surface to a Module. */
void module_bezierSurface(Module *m, BezierSurface *b, int div, int solid){
    if(!m || !b) return;

    if(div <= 0){
        if(solid == 0){
            for(int i = 0; i < 4; i++){
                for(int j = 0; j < 3; j++){
                    Line l;
                    line_set(&l, b->cp[i][j], b->cp[i][j + 1]);
                    line_setz(&l, b->zbuffer);
                    module_line(m, &l);
                }
            }

            for(int j = 0; j < 4; j++){
                for(int i = 0; i < 3; i++){
                    Line l;
                    line_set(&l, b->cp[i][j], b->cp[i + 1][j]);
                    line_setz(&l, b->zbuffer);
                    module_line(m, &l);
                }
            }
        }
        else{
            Point triangle1[3] = {b->cp[0][0], b->cp[0][3], b->cp[3][3]};
            Point triangle2[3] = {b->cp[0][0], b->cp[3][3], b->cp[3][0]};

            Polygon pg1, pg2;
            polygon_init(&pg1);
            polygon_init(&pg2);

            polygon_set(&pg1, 3, triangle1);
            polygon_setZBuffer(&pg1, b->zbuffer);
            module_setFaceNormals(&pg1, triangle1, 3);
            module_polygon(m, &pg1);

            polygon_set(&pg2, 3, triangle2);
            polygon_setZBuffer(&pg2, b->zbuffer);
            module_setFaceNormals(&pg2, triangle2, 3);
            module_polygon(m, &pg2);

            polygon_clear(&pg1);
            polygon_clear(&pg2);
        }
        return;
    }

    Point leftRows[4][4];
    Point rightRows[4][4];

    for(int i = 0; i < 4; i++){
        module_split_bezier_curve(b->cp[i], leftRows[i], rightRows[i]);
    }

    BezierSurface leftTop, rightTop, leftBottom, rightBottom;

    for(int j = 0; j < 4; j++){
        Point col[4], top[4], bottom[4];

        for(int i = 0; i < 4; i++){
            col[i] = leftRows[i][j];
        }
        module_split_bezier_curve(col, top, bottom);
        for(int i = 0; i < 4; i++){
            leftTop.cp[i][j] = top[i];
            leftBottom.cp[i][j] = bottom[i];
        }

        for(int i = 0; i < 4; i++){
            col[i] = rightRows[i][j];
        }
        module_split_bezier_curve(col, top, bottom);
        for(int i = 0; i < 4; i++){
            rightTop.cp[i][j] = top[i];
            rightBottom.cp[i][j] = bottom[i];
        }
    }

    leftTop.zbuffer = b->zbuffer;
    rightTop.zbuffer = b->zbuffer;
    leftBottom.zbuffer = b->zbuffer;
    rightBottom.zbuffer = b->zbuffer;

    module_bezierSurface(m, &leftTop, div - 1, solid);
    module_bezierSurface(m, &rightTop, div - 1, solid);
    module_bezierSurface(m, &leftBottom, div - 1, solid);
    module_bezierSurface(m, &rightBottom, div - 1, solid);
}

/* Add the foreground color value to the tail of the Module's list */
void module_color(Module *md, Color *c){
    if(md == NULL || c == NULL) return;
    Element *e = element_init(ObjColor, c);
    module_insert(md, e);
}

/* Add the body color value to the tail of the Module's list */
void module_bodyColor(Module *md, Color *c){
    if(md == NULL || c == NULL) return;
    Element *e = element_init(ObjBodyColor, c);
    module_insert(md, e);
}

/* Add the surface color value to the tail of the Module's list */
void module_surfaceColor(Module *md, Color *c){
    if(md == NULL || c == NULL) return;
    Element *e = element_init(ObjSurfaceColor, c);
    module_insert(md, e);
}

/* Add the specular coefficient value to the tail of the Module's list */
void module_surfaceCoeff(Module *md, float coeff){
    if(md == NULL) return;
    Element *e = element_init(ObjSurfaceCoeff, &coeff);
    module_insert(md, e);
}

/* Create a new DrawState structure and initialize the fields */
DrawState *drawstate_create(){
    DrawState *ds = (DrawState *)malloc(sizeof(DrawState));
    if (ds == NULL) {
        fprintf(stderr, "Error allocating memory for DrawState\n");
        exit(1);
    }
    drawstate_init(ds);
    return ds;
}

/* Initialize the fields of an existing DrawState structure */
void drawstate_init(DrawState *ds){
    if(!ds) return;
    Color white;
    color_set(&white, 1.0, 1.0, 1.0);
    ds->color = white; // Default color is white
    ds->flatColor = white; // Default flat color is white
    ds->bodyColor = white; // Default body color is white
    ds->surfaceColor = white; // Default surface color is white
    ds->surfaceCoeff = 10.0; // Default shininess coefficient
    ds->shade = ShadeGouraud; // Default shading method
    ds->zBuffer = 1; // Default z-buffering enabled
    point_set(&ds->viewer, 0, 0, -1, 1); // Default viewer position
    ds->lighting = NULL; // No lighting by default (set for ShadePhong)
    ds->texture  = NULL; // No texture by default
}

/* Set the viewer position */
void drawstate_setViewer(DrawState *ds, Point *viewer){
    if(ds == NULL || viewer == NULL) return;
    ds->viewer = *viewer;
}

/* Set the color field to c */
void drawstate_setColor(DrawState *ds, Color *c){
    if(ds == NULL || c == NULL) return;
    ds->color = *c;
}

/* Set the body color field to c */
void drawstate_setBody(DrawState *ds, Color *c){
    if(ds == NULL || c == NULL) return;
    ds->bodyColor = *c;
}

/* Set the surface color field to c */
void drawstate_setSurface(DrawState *ds, Color *c){
    if(ds == NULL || c == NULL) return;
    ds->surfaceColor = *c;
}

/* Set the specular coefficient field to f */
void drawstate_setSurfaceCoeff(DrawState *ds, float f){
    if(ds == NULL) return;
    ds->surfaceCoeff = f;
}

/* Set the shading method */
void drawstate_setShading(DrawState *ds, ShadeMethod shade){
    if(ds == NULL) return;
    ds->shade = shade;
}

/* Copy the DrawState structure */
DrawState *drawstate_copy(DrawState *ds){
    if(ds == NULL) return NULL;
    DrawState *new_ds = drawstate_create();
    if(new_ds == NULL) return NULL;
    *new_ds = *ds;
    return new_ds;
}

/* Add the light structure to the module */
void module_addLight(Module *md, Light *light){
    if(md == NULL || light == NULL) return;
    Element *e = element_init(ObjLight, light);
    module_insert(md, e);
}

static void module_parseLighting_recur(Module *md, Matrix *GTM, Lighting *lighting){
    if(md == NULL || GTM == NULL || lighting == NULL) return;

    Matrix LTM;
    matrix_identity(&LTM);

    for(Element *e = md->head; e != NULL; e = e->next){
        switch(e->type){
            case ObjMatrix:{
                Matrix temp;
                matrix_multiply((Matrix *)e->obj, &LTM, &temp);
                matrix_copy(&LTM, &temp);
                break;
            }
            case ObjIdentity:
                matrix_identity(&LTM);
                break;
            case ObjModule:{
                Matrix TM;
                matrix_multiply(GTM, &LTM, &TM);
                module_parseLighting_recur((Module *)e->obj, &TM, lighting);
                break;
            }
            case ObjLight:{
                Light light;
                Light *src = (Light *)e->obj;
                light_copy(&light, src);

                if(light.type == LightPoint || light.type == LightSpot){
                    Point p;
                    matrix_xformPoint(&LTM, &light.position, &p);
                    matrix_xformPoint(GTM, &p, &light.position);
                }

                if(light.type == LightDirect || light.type == LightSpot){
                    Vector v;
                    matrix_xformVector(&LTM, &light.direction, &v);
                    matrix_xformVector(GTM, &v, &light.direction);
                    vector_normalize(&light.direction);
                }

                lighting_add(lighting, light.type, &light.color, &light.direction, &light.position, light.cutoff, light.sharpness);
                break;
            }
            default:
                break;
        }
    }
}

/* Parse module-contained lights into the Lighting structure */
void module_parseLighting(Module *md, Matrix *GTM, Lighting *lighting){
    module_parseLighting_recur(md, GTM, lighting);
}

/* Add a texture image element to the module's list */
void module_texture(Module *md, Image *texture){
    if(md == NULL) return;
    Element *e = element_init(ObjTexture, texture);
    if(e) module_insert(md, e);
}

/* Set the texture image on a DrawState directly */
void drawstate_setTexture(DrawState *ds, Image *texture){
    if(ds == NULL) return;
    ds->texture = texture;
}
