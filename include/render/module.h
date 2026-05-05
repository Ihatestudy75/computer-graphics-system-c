#ifndef MODULE_H
#define MODULE_H

#include "matrix.h"
#include "primitive.h"
#include "polygon.h"
#include "bezier.h"
#include "image.h"
#include "light.h"

// Object type enumeration
typedef enum {
    ObjNone,
    ObjLine,
    ObjPoint,
    ObjPolyline,
    ObjPolygon,
    ObjIdentity,
    ObjMatrix,
    ObjColor,
    ObjBodyColor,
    ObjSurfaceColor,
    ObjSurfaceCoeff,
    ObjLight,
    ObjModule,
    ObjBezier,
    ObjTexture
} ObjectType;

// Element structure definition
typedef struct {
    ObjectType type;    // Type of object stored in the obj pointer
    void *obj;          // Pointer to the actual object (e.g., Line, Point, Module, etc.)
    void *next;         // Pointer to the next element in the linked list
} Element;

// Module structure definition
typedef struct Module {
    Element *head;      // Pointer to the head of the linked list of elements
    Element *tail;      // Pointer to the tail of the linked list of elements
} Module;

// Shading method enumeration
typedef enum {
    ShadeFrame,
    ShadeConstant,
    ShadeDepth,
    ShadeFlat,
    ShadeGouraud,
    ShadePhong
} ShadeMethod;

// DrawState structure definition
typedef struct DrawState {
    Color color;        // The foreground color for drawing as default
    Color flatColor;    // The flat color for polygon filling based on a shading calculation
    Color bodyColor;    // The body reflection color for polygon filling used for shading calculation
    Color surfaceColor; // The surface reflection color for polygon filling used for shading calculation
    float surfaceCoeff; // The surface coefficient representing the shininess of the surface
    ShadeMethod shade;  // The shading method
    int zBuffer;        // Whether z-buffering is enabled (1) or disabled (0)
    Point viewer;       // The position of the viewer (VRP in View3D)
    Lighting *lighting; // Pointer to the lighting structure (needed for ShadePhong per-pixel lighting)
    Image *texture;     // Pointer to a texture image for texture mapping (NULL = no texture)
} DrawState;

/* Allocate and return an initialized but empty Element*/
Element *element_create();

/* 
    Allocate an Element and store a duplicate of the data pointed to by obj in Element 
    Modules do not get duplicated.
    Handle each type of object separately in a case statement
*/
Element *element_init(ObjectType type, void *obj);

/* Free an Element and its associated object */
void element_delete(Element *e);

/* Allocate an empty Module */
Module *module_create();

/* Clear the module's list of Element, freeing memory */
void module_clear(Module *md);

/* Free all of the memory associated with a Module, including the memory pointed*/
void module_delete(Module *md);

/* Generic insert of an Element into the Module at the tail of the list */
void module_insert(Module *md, Element *e);

/* Add a pointer to the Module sub to the tail of the Module's list */
void module_module(Module *md, Module *sub);

/* Add Point p to the tail of the Module's list */
void module_point(Module *md, Point *p);

/* Add Line l to the tail of the Module's list */
void module_line(Module *md, Line *l);

/* Add Polyline pl to the tail of the Module's list */
void module_polyline(Module *md, Polyline *pl);

/* Add Polygon pg to the tail of the Module's list */
void module_polygon(Module *md, Polygon *pg);

/* 
    Add a unit cube, axis-aligned and centered on zero to the Module.
    If solid is zero, add only lines.
    If solid is non-zero, add only polygons. Each polygon has surface normal.
*/
void module_cube(Module *md, int solid);

/* 
    Add a unit pyramid centered roughly around the origin.
    Square base lies on y = -0.5, apex at y = +0.5.
    If solid is zero, add only lines.
    If solid is non-zero, add polygons.
*/
void module_pyramid(Module *md, int solid);

/* 
    Add a unit octahedron centered at the origin.
    If solid is zero, add only lines.
    If solid is non-zero, add polygons.
*/
void module_octahedron(Module *md, int solid);

/*
    Add a cylinder to the Module using the given top and bottom center points.
    The sides parameter controls the number of radial subdivisions.
*/
void module_cylinder(Module *md, Point *topCenter, Point *bottomCenter, int sides);

/*
    Add a tube/frustum aligned along the Y-axis to the Module.
    The top and bottom centers give the axis endpoints, and the radii may differ.
*/
void module_tube(Module *md, Point *topCenter, double topRadius, Point *bottomCenter, double bottomRadius, int sides);

/*
    Add a Utah teapot to the Module using the given number of divisions for the Bezier patches.
     If solid is zero, add only lines.
     If solid is non-zero, add polygons.
     Adapted from Bruce Maxwell's Project 6 test6b teapot example.
*/
void module_teapot(Module *md, int divisions, int solid);

/* 
    Use the de Casteljau algorithm to subdivide the Bezier curve div times, 
    then add the lines connecting the control points to the module 
*/
void module_bezierCurve(Module *m, BezierCurve *b, int div);

/* 
    Use the de Casteljau algorithm to subdivide the Bezier surface div times, then add to the module 
    either the lines connecting the control points if solid is 0, 
    or triangles using the four corner control points if solid is non-zero 
*/
void module_bezierSurface(Module *m, BezierSurface *b, int div, int solid);

/* Object that sets the current transform to the identity, placed at the tail of the Module's list */
void module_identity(Module *md);

/* Matrix operand to add a translation matrix to the tail of the Module's list */
void module_translate2D(Module *md, double tx, double ty);

/* Matrix operand to add a scaling matrix to the tail of the Module's list */
void module_scale2D(Module *md, double sx, double sy);

/* Matrix operand to add a rotation about the Z-axis to the tail of the Module's list */
void module_rotateZ(Module *md, float cth, float sth);

/* Matrix operand to add a 2D shear matrix to the tail of the Module's list */
void module_shear2D(Module *md, double shx, double shy);

/* 
    Draw the Module into the image using 
    the given view transformation matrix, Lighting and DrawState 
    by traversing the list of Elements 
   (For now, Lighting can be an empty structure)
*/
void module_draw(Module *md, Matrix *VTM, Matrix *GTM, DrawState *ds, Lighting *light, Image *src);

/* Matrix operand to add a 3D translation matrix to the tail of the Module's list */
void module_translate(Module *md, double tx, double ty, double tz);

/* Matrix operand to add a 3D scaling matrix to the tail of the Module's list */
void module_scale(Module *md, double sx, double sy, double sz);

/* Matrix operand to add a rotation about the X-axis to the tail of the Module's list */
void module_rotateX(Module *md, float cth, float sth);

/* Matrix operand to add a rotation about the Y-axis to the tail of the Module's list */
void module_rotateY(Module *md, float cth, float sth);

/* Matrix operand to add a rotation that orients to the orthonormal axes u, v, w */
void module_rotateXYZ(Module *md, Vector *u, Vector *v, Vector *w);

/* Add the foreground color value to the tail of the Module's list */
void module_color(Module *md, Color *c);

/* Add the body color value to the tail of the Module's list */
void module_bodyColor(Module *md, Color *c);

/* Add the surface color value to the tail of the Module's list */
void module_surfaceColor(Module *md, Color *c);

/* Add the specular coefficient value to the tail of the Module's list */
void module_surfaceCoeff(Module *md, float coeff);

/* Add a texture image to the tail of the Module's list;
   polygons drawn after this element will sample from that texture */
void module_texture(Module *md, Image *texture);

/* Create a new DrawState structure and initialize the fields */
DrawState *drawstate_create();

/* Initialize the fields of an existing DrawState structure */
void drawstate_init(DrawState *ds);

/* Set the viewer position */
void drawstate_setViewer(DrawState *ds, Point *viewer);

/* Set the texture image used for texture mapping (NULL disables texturing) */
void drawstate_setTexture(DrawState *ds, Image *texture);

/* Set the color field to c */
void drawstate_setColor(DrawState *ds, Color *c);

/* Set the body color field to c */
void drawstate_setBody(DrawState *ds, Color *c);

/* Set the surface color field to c */
void drawstate_setSurface(DrawState *ds, Color *c);

/* Set the specular coefficient field to f */
void drawstate_setSurfaceCoeff(DrawState *ds, float f);

/* Set the shading method */
void drawstate_setShading(DrawState *ds, ShadeMethod shade);

/* Copy the DrawState structure */
DrawState *drawstate_copy(DrawState *ds);

/* Add the light structure to the module */
void module_addLight(Module *md, Light *light);

/* 
    Recursively traverse the module and all sub-modules, keep track of the LTM and GTM, and apply all ObjMatrix and ObjIdentity elements.
    
    When the traversal finds an ObjLight element, copy and add it to the Lighting structure and then transform the position and direction fields of the Light by the LTM and GTM.
    
    Use xformPoint for the position and xformVector for the direction.
    
    Don’t use the VTM, and don’t need to normalize the points
    
    Any lights added to the module should end up in the Lighting structure at the end of the traversal.
    
    Note that the Lighting structure may have existing lights, so don’t clear or delete them at the start of the traversal.
    
    Call the module parseLighting function just before you call module draw
*/
void module_parseLighting(Module *md, Matrix *GTM, Lighting *lighting);

#endif

// Union of possible objects stored in an Element
// typedef union {
//     Point point;
//     Line line;
//     Polyline polyline;
//     Polygon polygon;
//     Matrix matrix;
//     Color color;
//     float coeff;
//     void *module;       // Pointer to a Module structure
// } Object;

// Element structure definition
// typedef struct {
//     ObjectType type;    // Type of object stored in the obj pointer
//     Object obj;         // Union of possible objects
//     void *next;         // Pointer to the next element in the linked list
// } Element;
