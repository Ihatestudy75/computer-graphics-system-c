#ifndef BEZIER_H
#define BEZIER_H

#include "image.h"     /* Image, Color, FPixel */
#include "primitive.h" /* Point, Vector, Line  */

/* Forward declaration — avoids including module.h (which includes bezier.h) */
typedef struct Module Module;

/*
 * Pixel-space threshold for the adaptive stopping criterion in
 * bezierCurve_draw.  When the largest dimension of the bounding box
 * of the four control points is ≤ this value, draw line segments
 * directly rather than subdividing further.
 *
 * Value of 10 matches the example given in the spec (§6.1).
 */
#define BEZIER_PIXEL_THRESHOLD 10.0

/*
 * Maximum recursion depth used as a hard cap in bezierCurve_draw so
 * that the adaptive criterion never causes infinite recursion for
 * degenerate control-point configurations.
 */
#define BEZIER_MAX_DIV 8


/* =======================================================================
 * BezierCurve — cubic Bézier curve (order 3, four control points)
 * ======================================================================= */

/**
 * BezierCurve — a cubic Bézier curve defined by four control points.
 *
 * Fields
 * ------
 * cp[4]    control points P0, P1, P2, P3 in homogeneous coordinates.
 *          P0 is the start point, P3 is the end point.
 *          P1 and P2 are "pullover" points that shape the curve.
 * zbuffer  1 = depth-test pixels before writing (default)
 *          0 = draw unconditionally (2D overlay mode)
 */
typedef struct {
    Point cp[4]; /* P0..P3 control points in order */
    int zbuffer; /* z-buffer enable flag (1=on, 0=off) */
} BezierCurve;


/* =======================================================================
 * BezierSurface — cubic Bézier patch (order 3 × 3, sixteen control points)
 * ======================================================================= */

/**
 * BezierSurface — a bicubic Bézier patch defined by a 4×4 grid of
 * control points.
 *
 * Fields
 * ------
 * cp[4][4]  control point grid; cp[u][v] with u = row, v = column.
 *           The four corner points cp[0][0], cp[0][3], cp[3][0], cp[3][3]
 *           lie on the surface; interior points attract but do not
 *           interpolate.
 * zbuffer   z-buffer enable flag (same semantics as BezierCurve)
 */
typedef struct {
    Point cp[4][4]; /* 4×4 control point grid; index as cp[u][v] */
    int zbuffer;    /* z-buffer enable flag (1=on, 0=off)          */
} BezierSurface;


/* =======================================================================
 * BezierCurve — initialisation and setup
 * ======================================================================= */

/**
 * bezierCurve_init — set zbuffer to 1 and place control points evenly
 * along the X-axis from (0,0,0) to (1,0,0).
 *
 * The four points are:
 *   P0 = (0.000, 0, 0, 1)
 *   P1 = (0.333, 0, 0, 1)
 *   P2 = (0.667, 0, 0, 1)
 *   P3 = (1.000, 0, 0, 1)
 *
 * This produces a degenerate (straight-line) cubic Bézier that can be
 * reshaped by calling bezierCurve_set or bezierSurface_setPoint.
 *
 * @param b  BezierCurve to initialise (must not be NULL)
 */
void bezierCurve_init(BezierCurve *b);

/**
 * bezierCurve_set — copy four control points from vlist into the curve.
 *
 * vlist[0]→P0, vlist[1]→P1, vlist[2]→P2, vlist[3]→P3.
 * The zbuffer flag is not changed.
 *
 * @param b      destination BezierCurve
 * @param vlist  source Point array with exactly 4 elements
 */
void bezierCurve_set(BezierCurve *b, Point *vlist);

/**
 * bezierCurve_zBuffer — set the z-buffer enable flag.
 *
 * @param b     BezierCurve to modify
 * @param flag  new flag value (0 = off, non-zero = on)
 */
void bezierCurve_zBuffer(BezierCurve *b, int flag);

/**
 * bezierCurve_copy — copy all data from *src into *dst.
 *
 * Copies all four control points and the zbuffer flag.
 *
 * @param dst  destination BezierCurve (must not be NULL)
 * @param src  source      BezierCurve (must not be NULL)
 */
void bezierCurve_copy(BezierCurve *dst, const BezierCurve *src);


/* =======================================================================
 * BezierCurve — geometric queries
 * ======================================================================= */

/**
 * bezierCurve_boundingBox — compute the 2D axis-aligned bounding box of
 * the four control points.
 *
 * The bounding box is the tightest axis-aligned rectangle that contains
 * all four control points.  It is used by the adaptive drawing criterion:
 * if the box's largest dimension is ≤ BEZIER_PIXEL_THRESHOLD the curve
 * is drawn as straight line segments.
 *
 * @param b       source BezierCurve
 * @param minX    output: minimum x across all control points
 * @param minY    output: minimum y across all control points
 * @param maxX    output: maximum x across all control points
 * @param maxY    output: maximum y across all control points
 */
void bezierCurve_boundingBox(const BezierCurve *b,
                              double *minX, double *minY,
                              double *maxX, double *maxY);

/**
 * bezierCurve_evaluate — evaluate the Bézier curve at parameter t ∈ [0,1]
 * using the explicit cubic Bernstein formula.
 *
 * B(t) = (1-t)³P0 + 3t(1-t)²P1 + 3t²(1-t)P2 + t³P3
 *
 * @param b  source BezierCurve
 * @param t  parameter value in [0, 1]
 * @return   Point on the curve at parameter t
 */
Point bezierCurve_evaluate(const BezierCurve *b, double t);

/**
 * bezierCurve_split — split a cubic Bézier curve at t = 0.5 using the
 * de Casteljau algorithm, producing left and right sub-curves.
 *
 * After the call:
 *   left->cp[0]  = b->cp[0]     (start of original)
 *   left->cp[3]  = midpoint on curve
 *   right->cp[0] = midpoint on curve
 *   right->cp[3] = b->cp[3]     (end of original)
 *
 * Both halves inherit b->zbuffer.
 *
 * This is the primary subdivision primitive used by bezierCurve_draw
 * and bezier_curve_de_casteljau.
 *
 * @param b      source BezierCurve to split
 * @param left   output: left  half (t ∈ [0, 0.5])
 * @param right  output: right half (t ∈ [0.5, 1])
 */
void bezierCurve_split(const BezierCurve *b,
                        BezierCurve *left, BezierCurve *right);


/* =======================================================================
 * BezierCurve — drawing and subdivision into module
 * ======================================================================= */

/**
 * bezierCurve_draw — draw the curve into an Image using an adaptive
 * de Casteljau subdivision.
 *
 * Stopping criterion (spec §6.1):
 *   If the largest dimension of the control-point bounding box is
 *   ≤ BEZIER_PIXEL_THRESHOLD pixels, draw the three line segments
 *   P0P1, P1P2, P2P3 directly as a polyline approximation.
 *   Otherwise subdivide at t = 0.5 and recurse on both halves.
 *   A hard cap of BEZIER_MAX_DIV levels prevents infinite recursion.
 *
 * @param b    BezierCurve to draw (must not be NULL)
 * @param src  target Image
 * @param col  line colour
 */
void bezierCurve_draw(BezierCurve *b, Image *src, Color col);

/**
 * bezier_curve_de_casteljau — subdivide the curve div times using the
 * de Casteljau algorithm and add the resulting line segments to Module m.
 *
 * When div == 0 the function adds the three control-polygon edges
 * (P0P1, P1P2, P2P3) as Lines to the module.
 * When div > 0 the curve is split at t = 0.5 and the function recurses
 * on both halves with div − 1.
 *
 * This is the primary entry point used by module_bezierCurve.
 *
 * @param b    source BezierCurve
 * @param m    target Module to receive the Line elements
 * @param div  number of subdivision levels (0 = just the control polygon)
 */
void bezier_curve_de_casteljau(BezierCurve *b, int div);

/**
 * bezierCurve_print — write a human-readable description of the curve
 * to the FILE stream fp.  Useful for debugging.
 *
 * @param b   BezierCurve to describe
 * @param fp  destination FILE stream (e.g. stdout)
 */
void bezierCurve_print(const BezierCurve *b, FILE *fp);


/* =======================================================================
 * BezierSurface — initialisation and setup
 * ======================================================================= */

/**
 * bezierSurface_init — set zbuffer to 1 and place control points on the
 * X-Z plane from (0,0,0) to (1,0,1).
 *
 * cp[u][v] = (u/3, 0, v/3, 1) for u,v ∈ {0,1,2,3}.
 * All y values are 0 (flat plane).
 *
 * @param b  BezierSurface to initialise (must not be NULL)
 */
void bezierSurface_init(BezierSurface *b);

/**
 * bezierSurface_set — copy 16 control points from vlist into the patch.
 *
 * Layout: vlist[u*4 + v] → cp[u][v], so the 16 points are stored in
 * row-major order (all v values for u=0 first, then u=1, etc.).
 *
 * @param b      destination BezierSurface
 * @param vlist  source Point array with exactly 16 elements
 */
void bezierSurface_set(BezierSurface *b, Point *vlist);

/**
 * bezierSurface_zBuffer — set the z-buffer enable flag.
 *
 * @param b     BezierSurface to modify
 * @param flag  new flag value (0 = off, non-zero = on)
 */
void bezierSurface_zBuffer(BezierSurface *b, int flag);

/**
 * bezierSurface_setPoint — set a single control point by index.
 *
 * Copies *p into cp[u][v].  u and v must be in [0, 3]; out-of-range
 * indices are silently ignored.
 *
 * @param b  destination BezierSurface
 * @param p  source Point to copy in
 * @param u  row    index (0–3)
 * @param v  column index (0–3)
 */
void bezierSurface_setPoint(BezierSurface *b, Point *p, int u, int v);

/**
 * bezierSurface_getPoint — retrieve a single control point by index.
 *
 * Copies cp[u][v] into *p.
 *
 * @param b  source BezierSurface
 * @param p  output Point to receive the value
 * @param u  row    index (0–3)
 * @param v  column index (0–3)
 */
void bezierSurface_getPoint(BezierSurface *b, Point *p, int u, int v);

/**
 * bezierSurface_copy — copy all 16 control points and the zbuffer flag
 * from *src into *dst.
 *
 * @param dst  destination BezierSurface (must not be NULL)
 * @param src  source      BezierSurface (must not be NULL)
 */
void bezierSurface_copy(BezierSurface *dst, const BezierSurface *src);


/* =======================================================================
 * BezierSurface — geometric queries
 * ======================================================================= */

/**
 * bezierSurface_evaluate — evaluate the patch at (u, v) ∈ [0,1]² using
 * the tensor-product Bernstein formula.
 *
 * P(u,v) = Σ_{i=0}^{3} Σ_{j=0}^{3} B_i(u) · B_j(v) · cp[i][j]
 *
 * where B_i are the cubic Bernstein basis polynomials.
 *
 * @param b  source BezierSurface
 * @param u  parameter along the u-axis ∈ [0, 1]
 * @param v  parameter along the v-axis ∈ [0, 1]
 * @return   Point on the surface at (u, v)
 */
Point bezierSurface_evaluate(const BezierSurface *b, double u, double v);

/**
 * bezierSurface_normals — compute a surface normal for each of the 16
 * control points using finite differences on the control grid.
 *
 * The partial derivatives at cp[u][v] are approximated by central
 * differences (forward/backward differences at boundaries):
 *   ∂/∂u ≈ (cp[u+1][v] − cp[u-1][v]) / 2
 *   ∂/∂v ≈ (cp[u][v+1] − cp[u][v-1]) / 2
 *
 * The normal is cross(∂/∂v, ∂/∂u), normalised to unit length.
 * Results are written into n[u*4 + v] (row-major, matching vlist layout).
 *
 * The output array n must be already allocated with at least 16 elements.
 *
 * @param b  source BezierSurface
 * @param n  output Vector array (16 elements, pre-allocated)
 */
void bezierSurface_normals(BezierSurface *b, Vector *n);


/* =======================================================================
 * BezierSurface — subdivision
 * ======================================================================= */

/**
 * bezierSurface_splitU — split the surface at u = 0.5, producing two
 * half-patches side-by-side along the u direction.
 *
 * Each row of the 4×4 grid is an independent cubic Bézier curve in v;
 * splitting each row in u gives leftTop (u ∈ [0, 0.5]) and rightTop
 * (u ∈ [0.5, 1]).
 *
 * Both halves inherit b->zbuffer.
 *
 * @param b      source BezierSurface
 * @param left   output: u ∈ [0, 0.5] half
 * @param right  output: u ∈ [0.5, 1] half
 */
void bezierSurface_splitU(const BezierSurface *b,
                           BezierSurface *left, BezierSurface *right);

/**
 * bezierSurface_splitV — split the surface at v = 0.5, producing two
 * half-patches stacked along the v direction.
 *
 * Each column of the grid is treated as a curve in u and is split at the
 * midpoint; the column data is then transposed back into the two halves.
 *
 * Both halves inherit b->zbuffer.
 *
 * @param b      source BezierSurface
 * @param top    output: v ∈ [0, 0.5] half
 * @param bottom output: v ∈ [0.5, 1] half
 */
void bezierSurface_splitV(const BezierSurface *b,
                           BezierSurface *top, BezierSurface *bottom);

/**
 * bezier_surface_de_casteljau — subdivide the patch div times and add the
 * resulting geometry to Module m.
 *
 * When div == 0:
 *   • solid == 0: add the 12 control-grid edge lines to m.
 *   • solid != 0: add two triangular polygons (the two corner triangles
 *     of the patch) to m as an approximation.
 * When div > 0: split into four sub-patches and recurse with div − 1.
 *
 * @param b     source BezierSurface
 * @param div   number of subdivision levels (0 = leaf geometry)
 * @param solid 0 = wireframe lines, non-zero = filled triangles
 */
void bezier_surface_de_casteljau(BezierSurface *b, int div);

/**
 * bezierSurface_print — write a human-readable 4×4 grid of control-point
 * coordinates to fp.  Useful for debugging.
 *
 * @param b   BezierSurface to describe
 * @param fp  destination FILE stream (e.g. stdout)
 */
void bezierSurface_print(const BezierSurface *b, FILE *fp);

#endif /* BEZIER_H */
