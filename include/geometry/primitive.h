#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "image.h"   /* Image, Color, FPixel */
#include <stdio.h>   /* FILE                 */


/* =======================================================================
 * Point / Vector
 * ======================================================================= */

/**
 * Point — a 4-element homogeneous-coordinate vector.
 *
 * Fields
 * ------
 * val[0]  x coordinate (screen column)
 * val[1]  y coordinate (screen row)
 * val[2]  z coordinate (depth)
 * val[3]  h homogeneous weight (1.0 for an ordinary Cartesian point)
 *
 * Homogeneous representation allows 2D and 3D transformations (translate,
 * rotate, scale, project) to be expressed as 4×4 matrix multiplications.
 */
typedef struct {
    double val[4]; /* (x, y, z, h) homogeneous coordinates */
} Point;

/**
 * Vector — a direction in 2D or 3D model space.
 *
 * Stored identically to Point (val[4]) so that matrix transformations
 * work on both.  The semantic difference is that a Vector's w component
 * is typically 0.0 (indicating no translation under affine transforms),
 * while a Point's w is 1.0.
 */
typedef Point Vector;


/* -----------------------------------------------------------------------
 * Point — constructors
 * ----------------------------------------------------------------------- */

/**
 * point_set2D — initialise a Point for 2D use.
 *
 * Sets val = {x, y, 0.0, 1.0}.  The z component is zeroed and the
 * homogeneous coordinate is set to 1.0 (Cartesian point convention).
 *
 * @param p  destination Point (must not be NULL)
 * @param x  x coordinate
 * @param y  y coordinate
 */
void point_set2D(Point *p, double x, double y);

/**
 * point_set3D — initialise a Point for 3D use.
 *
 * Sets val = {x, y, z, 1.0}.  The homogeneous coordinate is 1.0.
 *
 * @param p  destination Point
 * @param x  x coordinate
 * @param y  y coordinate
 * @param z  z coordinate (depth)
 */
void point_set3D(Point *p, double x, double y, double z);

/**
 * point_set — initialise all four components of a Point explicitly.
 *
 * Use this when constructing a projective point (h ≠ 1) or when the
 * caller needs direct control over all four values.
 *
 * @param p  destination Point
 * @param x  x component
 * @param y  y component
 * @param z  z component
 * @param h  homogeneous weight
 */
void point_set(Point *p, double x, double y, double z, double h);


/* -----------------------------------------------------------------------
 * Point — transforms and queries
 * ----------------------------------------------------------------------- */

/**
 * point_normalize — divide x and y by the homogeneous coordinate h.
 *
 * After this call val[0] = x/h, val[1] = y/h, val[3] = 1.0.
 * If h ≤ 0 the function is a no-op to avoid division by zero or sign
 * inversion (points at or behind the eye).
 *
 * @param p  Point to normalise in place
 */
void point_normalize(Point *p);

/**
 * point_copy — copy all four components from *from into *to.
 *
 * @param to    destination Point (must not be NULL)
 * @param from  source      Point (must not be NULL)
 */
void point_copy(Point *to, Point *from);

/**
 * point_distance2D — return the Euclidean distance between two Points
 * in the XY plane (z is ignored).
 *
 * Useful for hit-testing, clipping decisions, and dash-pattern length
 * calculations.
 *
 * @param a  first  Point
 * @param b  second Point
 * @return   sqrt((bx-ax)^2 + (by-ay)^2)
 */
double point_distance2D(const Point *a, const Point *b);

/**
 * point_equal — return 1 if two Points are identical in all four
 * components, 0 otherwise.
 *
 * Uses exact double equality; intended for checking boundary conditions
 * in rasterisation loops, not for comparing arithmetic results.
 *
 * @param a  first  Point
 * @param b  second Point
 * @return   1 if equal, 0 if not
 */
int point_equal(const Point *a, const Point *b);


/* -----------------------------------------------------------------------
 * Point — drawing
 * ----------------------------------------------------------------------- */

/**
 * point_draw — rasterise a Point into an Image using a Color.
 *
 * The point is normalised before drawing.  Pixel coordinates are obtained
 * by rounding val[0] (x → column) and val[1] (y → row).  If the point
 * lies outside the image bounds it is silently ignored.
 *
 * @param p    Point to draw
 * @param img  target Image
 * @param c    fill colour
 */
void point_draw(Point *p, Image *img, Color c);

/**
 * point_drawf — rasterise a Point using an FPixel value.
 *
 * Lower-level variant of point_draw that writes the full FPixel (including
 * the z field) directly to the image's data array.  Bounds-checked.
 *
 * @param p   Point to draw
 * @param img target Image
 * @param fp  FPixel to write
 */
void point_drawf(Point *p, Image *img, FPixel fp);


/* -----------------------------------------------------------------------
 * Point — debug / utility
 * ----------------------------------------------------------------------- */

/**
 * point_print — write a human-readable representation of the Point to fp.
 *
 * Output format:  Point: (x, y, z, h)
 * fp may be stdout, stderr, or any FILE opened with fopen.
 *
 * @param p   Point to print
 * @param fp  destination FILE stream
 */
void point_print(Point *p, FILE *fp);

/**
 * point_rotate — rotate a Point around the image centre by angle radians
 * and draw the result into an Image.
 *
 * The rotation is a 2D in-plane rotation: the new coordinates are
 *   x' = x·cos(angle) − y·sin(angle)
 *   y' = x·sin(angle) + y·cos(angle)
 * The rotated point is then passed to point_draw.
 *
 * @param p      Point to rotate (modified in place)
 * @param angle  rotation angle in radians
 * @param c      draw colour
 * @param img    target Image
 */
void point_rotate(Point *p, double angle, Color c, Image *img);


/* =======================================================================
 * Line
 * ======================================================================= */

/**
 * Line — a line segment between two Points.
 *
 * Fields
 * ------
 * zBuffer  1 = depth-test each pixel before writing (default)
 *          0 = draw unconditionally (2D / overlay mode)
 * from     starting endpoint
 * to       ending   endpoint
 *
 * The two endpoint fields are named 'from' and 'to' in this implementation
 * (the spec names them 'a' and 'b'; both name choices are acceptable).
 */
typedef struct {
    int   zBuffer; /* z-buffer enable flag; 1 = on, 0 = off */
    Point from;    /* starting endpoint                       */
    Point to;      /* ending   endpoint                       */
} Line;


/* -----------------------------------------------------------------------
 * Line — constructors
 * ----------------------------------------------------------------------- */

/**
 * line_set2D — initialise a Line from four scalar 2D coordinates.
 *
 * Both endpoints are set as 2D points (z=0, h=1).  zBuffer defaults to 1.
 *
 * @param l   destination Line
 * @param x0  starting x
 * @param y0  starting y
 * @param x1  ending   x
 * @param y1  ending   y
 */
void line_set2D(Line *l, double x0, double y0, double x1, double y1);

/**
 * line_set — initialise a Line from two Point structs.
 *
 * Copies both points into the Line.  zBuffer defaults to 1.
 *
 * @param l     destination Line
 * @param from  starting Point (copied by value)
 * @param to    ending   Point (copied by value)
 */
void line_set(Line *l, Point from, Point to);


/* -----------------------------------------------------------------------
 * Line — mutators and transforms
 * ----------------------------------------------------------------------- */

/**
 * line_setz — set the z-buffer flag (primary setter, spec name).
 *
 * @param l      Line to modify
 * @param zflag  new flag value (0 = off, non-zero = on)
 */
void line_setz(Line *l, int zflag);

/**
 * line_zBuffer — alias for line_setz; provided for API compatibility.
 *
 * @param l      Line to modify
 * @param zflag  new flag value
 */
void line_zBuffer(Line *l, int zflag);

/**
 * line_normalize — normalise both endpoint Points by their h components.
 *
 * Delegates to point_normalize for each endpoint.
 *
 * @param l  Line to normalise in place
 */
void line_normalize(Line *l);

/**
 * line_copy — deep-copy a Line struct.
 *
 * All fields (zBuffer + both Points) are copied.
 *
 * @param to    destination Line
 * @param from  source      Line
 */
void line_copy(Line *to, Line *from);

/**
 * line_length2D — return the Euclidean length of the line segment in the
 * XY plane.
 *
 * Delegates to point_distance2D on the two endpoints.
 *
 * @param l  source Line
 * @return   segment length in pixels
 */
double line_length2D(const Line *l);


/* -----------------------------------------------------------------------
 * Line — drawing
 * ----------------------------------------------------------------------- */

/**
 * line_draw — draw a line segment into an Image using Bresenham's
 * algorithm with Liang-Barsky clipping and optional z-buffering.
 *
 * Steps:
 *   1. Clip to image bounds using Liang-Barsky (discard if fully outside).
 *   2. Interpolate z linearly along the major axis.
 *   3. For each pixel: if zBuffer is on, depth-test before writing.
 *
 * @param l    Line to draw
 * @param img  target Image
 * @param c    line colour
 */
void line_draw(Line *l, Image *img, Color c);

/**
 * line_draw_dashed — draw a dashed line segment.
 *
 * Same rasterisation as line_draw but pixels are conditionally drawn
 * based on a repeating dash-gap pattern (dash=5px, gap=3px by default).
 *
 * @param l    Line to draw
 * @param img  target Image
 * @param c    line colour
 */
void line_draw_dashed(Line *l, Image *img, Color c);


/* -----------------------------------------------------------------------
 * Line — debug
 * ----------------------------------------------------------------------- */

/**
 * line_print — write a human-readable description of the Line to fp.
 *
 * Prints zBuffer flag followed by both endpoints.
 *
 * @param l   Line to print
 * @param fp  destination FILE stream
 */
void line_print(const Line *l, FILE *fp);


/* =======================================================================
 * Circle
 * ======================================================================= */

/**
 * Circle — a 2D circle defined by a centre and a radius.
 *
 * Fields
 * ------
 * r  radius in pixels (double for sub-pixel precision in set functions)
 * c  centre Point (only val[0] and val[1] are used for 2D drawing)
 */
typedef struct {
    double r; /* radius                    */
    Point  c; /* centre (x = col, y = row) */
} Circle;


/* -----------------------------------------------------------------------
 * Circle — constructor
 * ----------------------------------------------------------------------- */

/**
 * circle_set — initialise a Circle from a centre Point and radius.
 *
 * @param c       destination Circle
 * @param center  centre Point (copied)
 * @param r       radius (must be ≥ 0)
 */
void circle_set(Circle *c, Point center, double r);


/* -----------------------------------------------------------------------
 * Circle — drawing
 * ----------------------------------------------------------------------- */

/**
 * circle_draw — draw the outline of a circle using the midpoint (Bresenham)
 * circle algorithm.
 *
 * Eight-way symmetry is exploited: one octant is computed and mirrored to
 * produce all eight symmetric pixels per step, keeping the pixel count
 * proportional to the circumference rather than the area.
 *
 * @param c      Circle to draw
 * @param img    target Image
 * @param color  outline colour
 */
void circle_draw(Circle *c, Image *img, Color color);

/**
 * circle_drawFill — draw a solid filled circle.
 *
 * Uses horizontal scanline spans derived from the midpoint algorithm:
 * for each y level the x extent of the circle is computed and a
 * horizontal line is drawn, which is faster than testing every pixel
 * in the bounding box.
 *
 * @param c      Circle to fill
 * @param img    target Image
 * @param color  fill colour
 */
void circle_drawFill(Circle *c, Image *img, Color color);

/**
 * circle_draw_dashed — draw a dashed circle outline.
 *
 * Draws the same eight symmetric pixels per step as circle_draw but
 * applies a dash-gap pattern gated by a pixel counter so alternating
 * segments of the circumference are skipped.
 *
 * @param c      Circle to draw
 * @param img    target Image
 * @param color  dash colour
 */
void circle_draw_dashed(Circle *c, Image *img, Color color);

/**
 * circle_draw_fill_flood — draw a circle outline then flood-fill its
 * interior.
 *
 * Draws the outline first, then starts a 4-connected flood fill from
 * the centre point.  The fill stops when it hits the outline colour.
 *
 * @param c      Circle to fill
 * @param img    target Image
 * @param color  fill colour (also used as the outline colour)
 */
void circle_draw_fill_flood(Circle *c, Image *img, Color color);


/* =======================================================================
 * Ellipse
 * ======================================================================= */

/**
 * Ellipse — a 2D (possibly rotated) ellipse.
 *
 * Fields
 * ------
 * ra     semi-major axis radius (in the un-rotated frame)
 * rb     semi-minor axis radius
 * c      centre Point
 * angle  rotation of the major axis counter-clockwise from the x-axis,
 *        in radians (0.0 = axis-aligned)
 */
typedef struct {
    double ra;    /* semi-major axis radius */
    double rb;    /* semi-minor axis radius */
    Point  c;     /* centre                 */
    double angle; /* rotation angle (rad)   */
} Ellipse;


/* -----------------------------------------------------------------------
 * Ellipse — constructor
 * ----------------------------------------------------------------------- */

/**
 * ellipse_set — initialise an Ellipse.
 *
 * @param e       destination Ellipse
 * @param center  centre Point (copied)
 * @param ra      semi-major axis radius (≥ rb)
 * @param rb      semi-minor axis radius
 * @param angle   rotation in radians (0 = axis-aligned)
 */
void ellipse_set(Ellipse *e, Point center, double ra, double rb, double angle);


/* -----------------------------------------------------------------------
 * Ellipse — drawing
 * ----------------------------------------------------------------------- */

/**
 * ellipse_draw — draw the outline of a (possibly rotated) ellipse.
 *
 * Uses the midpoint ellipse algorithm in the un-rotated frame; each
 * boundary point is then rotated by e->angle around the centre before
 * being drawn.  Four-way symmetry reduces computation by a factor of 4.
 *
 * @param e    Ellipse to draw
 * @param img  target Image
 * @param c    outline colour
 */
void ellipse_draw(Ellipse *e, Image *img, Color c);

/**
 * ellipse_draw_dashed — draw a dashed ellipse outline.
 *
 * Same algorithm as ellipse_draw with a dash-gap pattern applied to
 * alternating boundary points.
 *
 * @param e    Ellipse to draw
 * @param img  target Image
 * @param c    dash colour
 */
void ellipse_draw_dashed(Ellipse *e, Image *img, Color c);

/**
 * ellipse_draw_fill — draw a solid filled (possibly rotated) ellipse.
 *
 * Tests every pixel in the bounding square of max(ra, rb) against the
 * rotated ellipse equation; those inside are drawn.  For axis-aligned
 * ellipses a scanline approach would be faster, but this method handles
 * arbitrary rotation correctly.
 *
 * @param e    Ellipse to fill
 * @param img  target Image
 * @param c    fill colour
 */
void ellipse_draw_fill(Ellipse *e, Image *img, Color c);

/**
 * ellipse_draw_fill_flood — draw the ellipse outline then flood-fill the
 * interior from the centre point.
 *
 * @param e    Ellipse to fill
 * @param img  target Image
 * @param c    fill colour (also used as outline colour)
 */
void ellipse_draw_fill_flood(Ellipse *e, Image *img, Color c);


/* =======================================================================
 * Polyline
 * ======================================================================= */

/**
 * Polyline — an ordered sequence of Points connected by line segments.
 *
 * Fields
 * ------
 * zBuffer    1 = depth-test each pixel (default), 0 = 2D overlay mode
 * numVertex  number of Points currently stored in vertex[]
 * vertex     heap-allocated array of numVertex Points; NULL when empty
 *
 * Memory management
 * -----------------
 * polyline_create / polyline_free  — manage both the struct and vertex[].
 * polyline_init / polyline_set / polyline_clear — operate on a
 *   pre-existing Polyline struct and manage only the vertex[] memory.
 */
typedef struct {
    int    zBuffer;   /* z-buffer enable flag (default 1)     */
    int    numVertex; /* number of vertices in vertex[]        */
    Point *vertex;    /* heap-allocated vertex array (or NULL) */
} Polyline;


/* -----------------------------------------------------------------------
 * Polyline — heap-level constructors / destructor
 * ----------------------------------------------------------------------- */

/**
 * polyline_create — allocate and return an empty Polyline.
 *
 * numVertex is 0 and vertex is NULL.  zBuffer defaults to 1.
 * Returns NULL if malloc fails.
 *
 * @return  newly allocated Polyline, or NULL on failure
 */
Polyline *polyline_create(void);

/**
 * polyline_createp — allocate a Polyline and copy in a vertex list.
 *
 * @param numV   number of points in vlist
 * @param vlist  source array of Points (copied; must have ≥ numV elements)
 * @return       newly allocated Polyline, or NULL on failure
 */
Polyline *polyline_createp(int numV, Point *vlist);

/**
 * polyline_free — free the vertex array and the Polyline struct itself.
 *
 * After this call the pointer is dangling; set it to NULL.
 * Safe to call with a NULL pointer.
 *
 * @param pl  Polyline to destroy
 */
void polyline_free(Polyline *pl);


/* -----------------------------------------------------------------------
 * Polyline — struct-level init / set / clear
 * ----------------------------------------------------------------------- */

/**
 * polyline_init — initialise a pre-existing Polyline to an empty state.
 *
 * Sets numVertex=0, vertex=NULL, zBuffer=1.  Does NOT allocate or free
 * any memory; call this only on a fresh (or already-cleared) Polyline.
 *
 * @param pl  Polyline to initialise (must not be NULL)
 */
void polyline_init(Polyline *pl);

/**
 * polyline_set — replace the vertex list of a Polyline with a copy of
 * vlist.
 *
 * Existing vertex memory is freed first.  After the call the Polyline
 * owns a freshly allocated copy of the numV points.
 *
 * @param pl     destination Polyline
 * @param numV   number of points in vlist
 * @param vlist  source Point array
 */
void polyline_set(Polyline *pl, int numV, Point *vlist);

/**
 * polyline_clear — free the vertex array and reset size to zero.
 *
 * The Polyline struct itself is NOT freed.  Safe to call on an already-
 * empty Polyline.
 *
 * @param pl  Polyline to clear
 */
void polyline_clear(Polyline *pl);


/* -----------------------------------------------------------------------
 * Polyline — mutators and transforms
 * ----------------------------------------------------------------------- */

/**
 * polyline_setz — set the z-buffer flag (primary setter).
 *
 * @param pl     Polyline to modify
 * @param zflag  new flag value (0 = off, non-zero = on)
 */
void polyline_setz(Polyline *pl, int zflag);

/**
 * polyline_zBuffer — alias for polyline_setz; provided for API
 * compatibility with the spec name.
 *
 * @param pl     Polyline to modify
 * @param zflag  new flag value
 */
void polyline_zBuffer(Polyline *pl, int zflag);

/**
 * polyline_copy — deep-copy a Polyline.
 *
 * The destination's existing vertex memory is freed before the copy.
 * After the call *to owns a freshly allocated vertex array.
 *
 * @param to    destination Polyline
 * @param from  source      Polyline
 */
void polyline_copy(Polyline *to, Polyline *from);

/**
 * polyline_normalize — normalise every vertex by its h component.
 *
 * Delegates to point_normalize on each element of vertex[].
 *
 * @param pl  Polyline to normalise in place
 */
void polyline_normalize(Polyline *pl);

/**
 * polyline_add — append a single Point to the vertex list.
 *
 * The vertex array is grown with realloc.  If realloc fails the Polyline
 * is left unchanged and an error is printed to stderr.
 *
 * @param pl  target Polyline
 * @param v   Point to append (copied)
 */
void polyline_add(Polyline *pl, Point v);


/* -----------------------------------------------------------------------
 * Polyline — drawing and debug
 * ----------------------------------------------------------------------- */

/**
 * polyline_draw — draw the polyline as a series of connected line segments.
 *
 * Each consecutive pair of vertices is drawn with line_draw, inheriting
 * the Polyline's zBuffer flag.  A Polyline with fewer than 2 vertices
 * produces no output.
 *
 * @param pl   Polyline to draw
 * @param img  target Image
 * @param c    line colour
 */
void polyline_draw(Polyline *pl, Image *img, Color c);

/**
 * polyline_print — write a human-readable description of the Polyline to
 * the FILE stream fp.
 *
 * Prints the zBuffer flag and the coordinates of every vertex.
 *
 * @param pl  Polyline to describe
 * @param fp  destination FILE stream (e.g. stdout)
 */
void polyline_print(Polyline *pl, FILE *fp);

#endif /* PRIMITIVE_H */
