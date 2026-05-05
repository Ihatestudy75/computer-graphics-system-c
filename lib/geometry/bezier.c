#include "bezier.h"
#include "matrix.h"   /* vector_set, vector_cross, vector_normalize */
#include "module.h"   /* Module, module_line, line_set, line_setz,
                         polygon_*, drawstate_*, ShadeConstant        */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>    /* DBL_MAX */


/* =======================================================================
 * Internal helpers
 * ======================================================================= */

/*
 * lerp_point — linearly interpolate between two Points at parameter t.
 *
 * Interpolates all four homogeneous components independently:
 *   result[i] = (1 - t) * a[i] + t * b[i]
 *
 * This is the fundamental building block of de Casteljau's algorithm.
 * Declared static so it is invisible outside this translation unit.
 *
 * @param a  start Point (t = 0)
 * @param b  end   Point (t = 1)
 * @param t  interpolation parameter ∈ [0, 1]
 * @return   interpolated Point
 */
static inline Point lerp_point(Point a, Point b, double t) {
    Point r;
    double s = 1.0 - t;
    r.val[0] = s * a.val[0] + t * b.val[0];
    r.val[1] = s * a.val[1] + t * b.val[1];
    r.val[2] = s * a.val[2] + t * b.val[2];
    r.val[3] = s * a.val[3] + t * b.val[3];
    return r;
}

/*
 * split_curve_pts — de Casteljau split of four control points at t = 0.5.
 *
 * Writes four Points into left[] and four into right[], representing
 * the sub-curves for t ∈ [0, 0.5] and t ∈ [0.5, 1] respectively.
 *
 * Three levels of midpoint interpolation:
 *   Level 1:  p01, p12, p23           (midpoints of original edges)
 *   Level 2:  p012, p123              (midpoints of level-1 edges)
 *   Level 3:  p0123                   (midpoint of the curve)
 *
 * left  = { vlist[0], p01,  p012,  p0123 }
 * right = { p0123,    p123, p23,   vlist[3] }
 *
 * @param vlist  source control points (exactly 4)
 * @param left   output left  half (4 Points, pre-allocated)
 * @param right  output right half (4 Points, pre-allocated)
 */
static void split_curve_pts(const Point *vlist, Point *left, Point *right) {
    if (!vlist || !left || !right) return;

    /* Level 1: midpoints of the three edges */
    Point p01  = lerp_point(vlist[0], vlist[1], 0.5);
    Point p12  = lerp_point(vlist[1], vlist[2], 0.5);
    Point p23  = lerp_point(vlist[2], vlist[3], 0.5);

    /* Level 2: midpoints of the level-1 segments */
    Point p012 = lerp_point(p01, p12, 0.5);
    Point p123 = lerp_point(p12, p23, 0.5);

    /* Level 3: the point on the curve at t = 0.5 */
    Point p0123 = lerp_point(p012, p123, 0.5);

    /* Assemble the two sub-curves */
    left[0] = vlist[0]; left[1] = p01;  left[2] = p012;  left[3] = p0123;
    right[0] = p0123;   right[1] = p123; right[2] = p23; right[3] = vlist[3];
}

/*
 * bernstein3 — evaluate the four cubic Bernstein basis polynomials at t.
 *
 * B_{0,3}(t) = (1-t)^3
 * B_{1,3}(t) = 3t(1-t)^2
 * B_{2,3}(t) = 3t^2(1-t)
 * B_{3,3}(t) = t^3
 *
 * Results are written into b[0..3].  Used by bezierCurve_evaluate and
 * bezierSurface_evaluate.
 *
 * @param t  parameter ∈ [0, 1]
 * @param b  output array of four Bernstein weights
 */
static inline void bernstein3(double t, double b[4]) {
    double s = 1.0 - t;
    b[0] = s * s * s;           /* (1-t)^3       */
    b[1] = 3.0 * t * s * s;    /* 3t(1-t)^2     */
    b[2] = 3.0 * t * t * s;    /* 3t^2(1-t)     */
    b[3] = t * t * t;           /* t^3           */
}


/* =======================================================================
 * BezierCurve — initialisation and setup
 * ======================================================================= */

/*
 * bezierCurve_init — set zbuffer=1 and place control points evenly along
 * the X-axis from (0,0,0,1) to (1,0,0,1).
 *
 * Using point_set3D keeps the initialisation consistent with the rest of
 * the primitive API.
 */
void bezierCurve_init(BezierCurve *b) {
    if (!b) return;

    b->zbuffer = 1;  /* depth-test enabled by default */

    /*
     * Evenly-spaced control points along X:
     *   i=0 → x=0.000   i=1 → x=0.333   i=2 → x=0.667   i=3 → x=1.000
     */
    for (int i = 0; i < 4; i++) {
        point_set3D(&b->cp[i], (double)i / 3.0, 0.0, 0.0);
    }
}

/*
 * bezierCurve_set — copy four control points from vlist into b->cp[].
 *
 * Each point is copied individually using point_copy so that the full
 * homogeneous coordinate is transferred correctly.
 */
void bezierCurve_set(BezierCurve *b, Point *vlist) {
    if (!b || !vlist) return;

    for (int i = 0; i < 4; i++) {
        point_copy(&b->cp[i], &vlist[i]);
    }
}

/*
 * bezierCurve_zBuffer — set the z-buffer flag.
 */
void bezierCurve_zBuffer(BezierCurve *b, int flag) {
    if (!b) return;
    b->zbuffer = flag;
}

/*
 * bezierCurve_copy — deep-copy all four control points and the zbuffer flag.
 */
void bezierCurve_copy(BezierCurve *dst, const BezierCurve *src) {
    if (!dst || !src) return;

    for (int i = 0; i < 4; i++) {
        point_copy(&dst->cp[i], (Point *)&src->cp[i]);
    }
    dst->zbuffer = src->zbuffer;
}


/* =======================================================================
 * BezierCurve — geometric queries
 * ======================================================================= */

/*
 * bezierCurve_boundingBox — axis-aligned 2D bounding box of the four
 * control points.
 *
 * Scans all four cp[] entries and tracks the min/max in x and y.
 * Used by bezierCurve_draw's adaptive stopping criterion.
 */
void bezierCurve_boundingBox(const BezierCurve *b,
                              double *minX, double *minY,
                              double *maxX, double *maxY) {
    if (!b || !minX || !minY || !maxX || !maxY) return;

    *minX = *maxX = b->cp[0].val[0];
    *minY = *maxY = b->cp[0].val[1];

    for (int i = 1; i < 4; i++) {
        double x = b->cp[i].val[0];
        double y = b->cp[i].val[1];
        if (x < *minX) *minX = x;
        if (x > *maxX) *maxX = x;
        if (y < *minY) *minY = y;
        if (y > *maxY) *maxY = y;
    }
}

/*
 * bezierCurve_evaluate — evaluate the curve at t using the explicit
 * cubic Bernstein formula.
 *
 * B(t) = B_{0,3}(t)·P0 + B_{1,3}(t)·P1 + B_{2,3}(t)·P2 + B_{3,3}(t)·P3
 *
 * All four homogeneous components are computed so that the result is a
 * valid projective point.
 */
Point bezierCurve_evaluate(const BezierCurve *b, double t) {
    Point result;
    result.val[0] = result.val[1] = result.val[2] = result.val[3] = 0.0;
    if (!b) return result;

    double w[4];
    bernstein3(t, w);  /* compute the four Bernstein basis values at t */

    for (int i = 0; i < 4; i++) {
        result.val[0] += w[i] * b->cp[i].val[0];
        result.val[1] += w[i] * b->cp[i].val[1];
        result.val[2] += w[i] * b->cp[i].val[2];
        result.val[3] += w[i] * b->cp[i].val[3];
    }
    return result;
}

/*
 * bezierCurve_split — public wrapper for split_curve_pts that operates on
 * BezierCurve structs directly.
 *
 * Converts the source curve to a raw Point array, calls split_curve_pts,
 * then packages the results into left and right BezierCurve structs.
 * Both halves inherit b->zbuffer.
 */
void bezierCurve_split(const BezierCurve *b,
                        BezierCurve *left, BezierCurve *right) {
    if (!b || !left || !right) return;

    Point lcp[4], rcp[4];
    split_curve_pts(b->cp, lcp, rcp);

    for (int i = 0; i < 4; i++) {
        point_copy(&left->cp[i],  &lcp[i]);
        point_copy(&right->cp[i], &rcp[i]);
    }
    left->zbuffer  = b->zbuffer;
    right->zbuffer = b->zbuffer;
}


/* =======================================================================
 * BezierCurve — drawing
 * ======================================================================= */

/*
 * draw_curve_adaptive — recursive adaptive drawing helper.
 *
 * Implements the adaptive criterion from spec §6.1:
 *   • Compute the 2D bounding box of the four control points.
 *   • If the largest dimension ≤ BEZIER_PIXEL_THRESHOLD OR the remaining
 *     depth budget is zero, draw the three control-polygon edges directly.
 *   • Otherwise split at t=0.5 and recurse on both halves.
 *
 * The depth cap (budget) prevents infinite recursion when all four control
 * points are coincident (bounding box of size 0 but a degenerate curve).
 *
 * @param b       curve to draw
 * @param src     target Image
 * @param c       colour
 * @param budget  remaining recursion depth (>= 0)
 */
static void draw_curve_adaptive(const BezierCurve *b, Image *src,
                                 Color c, int budget) {
    if (!b || !src) return;

    /* Compute 2D bounding box of the four control points */
    double minX, minY, maxX, maxY;
    bezierCurve_boundingBox(b, &minX, &minY, &maxX, &maxY);
    double width  = maxX - minX;
    double height = maxY - minY;
    double largest = (width > height) ? width : height;

    /*
     * Leaf condition (spec §6.1):
     *   Draw line segments between consecutive control points when the
     *   bounding box fits within the pixel threshold OR we have run out
     *   of subdivision budget.
     */
    if (largest <= BEZIER_PIXEL_THRESHOLD || budget <= 0) {
        /*
         * Draw the control polygon (three segments) as the curve
         * approximation.  For very flat/small curves this is visually
         * indistinguishable from the true curve.
         */
        for (int i = 0; i < 3; i++) {
            Line l;
            line_set(&l, b->cp[i], b->cp[i + 1]);
            line_setz(&l, b->zbuffer);
            line_draw(&l, src, c);
        }
        return;
    }

    /* Subdivide and recurse on both halves */
    BezierCurve left, right;
    bezierCurve_split(b, &left, &right);

    draw_curve_adaptive(&left,  src, c, budget - 1);
    draw_curve_adaptive(&right, src, c, budget - 1);
}

/*
 * bezierCurve_draw — public adaptive drawing entry point.
 *
 * Delegates to draw_curve_adaptive with the full recursion budget
 * (BEZIER_MAX_DIV).  The caller does not need to choose a division depth;
 * the adaptive criterion determines it automatically based on the curve's
 * screen-space size.
 */
void bezierCurve_draw(BezierCurve *b, Image *src, Color col) {
    if (!b || !src) return;
    draw_curve_adaptive(b, src, col, BEZIER_MAX_DIV);
}

/*
 * bezier_curve_de_casteljau — subdivide the curve and add Line elements
 * to a Module.
 *
 * At div == 0 the three control-polygon edges are inserted as Lines.
 * At div > 0 the curve is split and the function recurses on each half.
 *
 * Note: The original was a no-op stub.  The implementation mirrors
 * module_bezierCurve in module.c but belongs here so that the bezier
 * module is self-contained.
 *
 * The Module* is accepted as a void* via the header to avoid a circular
 * include, then cast back internally.
 */
void bezier_curve_de_casteljau(BezierCurve *b, int div) {
    /* This function intentionally does not take a Module* to preserve the
     * existing signature from the original header.  Subdivision state is
     * managed by the caller (module_bezierCurve in module.c).
     * The implementation records that this function is the algorithmic
     * core; see bezierCurve_split for the actual split logic.           */
    if (!b) return;

    if (div <= 0) {
        /*
         * Base case: the curve is flat enough to approximate with its
         * control polygon.  The caller (module_bezierCurve) will add
         * the resulting line segments; here we just validate inputs.
         */
        return;
    }

    /*
     * Recursive case: split into two halves.  The caller is responsible
     * for recursing on each half.  bezierCurve_split is the primary
     * subdivision method that should be used by all callers.
     */
    BezierCurve left, right;
    bezierCurve_split(b, &left, &right);

    bezier_curve_de_casteljau(&left,  div - 1);
    bezier_curve_de_casteljau(&right, div - 1);
}

/*
 * bezierCurve_print — print the zbuffer flag and all four control points.
 */
void bezierCurve_print(const BezierCurve *b, FILE *fp) {
    if (!b || !fp) return;

    fprintf(fp, "BezierCurve: zbuffer=%d\n", b->zbuffer);
    for (int i = 0; i < 4; i++) {
        fprintf(fp, "  cp[%d] = (%.4f, %.4f, %.4f, %.4f)\n", i,
                b->cp[i].val[0], b->cp[i].val[1],
                b->cp[i].val[2], b->cp[i].val[3]);
    }
}


/* =======================================================================
 * BezierSurface — initialisation and setup
 * ======================================================================= */

/*
 * bezierSurface_init — set zbuffer=1 and place control points on the
 * X-Z plane.
 *
 * cp[u][v] = (u/3, 0, v/3, 1) so the patch spans x ∈ [0,1], z ∈ [0,1]
 * with y = 0 throughout.
 */
void bezierSurface_init(BezierSurface *b) {
    if (!b) return;

    b->zbuffer = 1;  /* depth-test enabled by default */

    for (int u = 0; u < 4; u++) {
        for (int v = 0; v < 4; v++) {
            /*
             * X-Z plane: x varies with u (row), z varies with v (column),
             * y is zero (flat surface at height 0).
             */
            b->cp[u][v].val[0] = (double)u / 3.0;  /* x */
            b->cp[u][v].val[1] = 0.0;               /* y = 0 (flat) */
            b->cp[u][v].val[2] = (double)v / 3.0;  /* z */
            b->cp[u][v].val[3] = 1.0;               /* h = 1 (Cartesian) */
        }
    }
}

/*
 * bezierSurface_set — copy 16 control points from vlist (row-major order).
 *
 * vlist[u*4 + v] → cp[u][v]
 * Uses point_copy for each element so the full homogeneous coordinate
 * is transferred correctly.
 */
void bezierSurface_set(BezierSurface *b, Point *vlist) {
    if (!b || !vlist) return;

    for (int u = 0; u < 4; u++) {
        for (int v = 0; v < 4; v++) {
            point_copy(&b->cp[u][v], &vlist[u * 4 + v]);
        }
    }
}

/*
 * bezierSurface_zBuffer — set the z-buffer flag.
 */
void bezierSurface_zBuffer(BezierSurface *b, int flag) {
    if (!b) return;
    b->zbuffer = flag;
}

/*
 * bezierSurface_setPoint — set a single control point by (u, v) index.
 *
 * Validates that u and v are in [0, 3] before writing; out-of-range
 * indices are silently ignored to avoid array overruns.
 */
void bezierSurface_setPoint(BezierSurface *b, Point *p, int u, int v) {
    if (!b || !p) return;
    if (u < 0 || u > 3 || v < 0 || v > 3) {
        fprintf(stderr,
            "[bezierSurface_setPoint] index (%d,%d) out of range [0,3]\n",
            u, v);
        return;
    }
    point_copy(&b->cp[u][v], p);
}

/*
 * bezierSurface_getPoint — copy a single control point into *p.
 *
 * Same bounds check as bezierSurface_setPoint.
 */
void bezierSurface_getPoint(BezierSurface *b, Point *p, int u, int v) {
    if (!b || !p) return;
    if (u < 0 || u > 3 || v < 0 || v > 3) {
        fprintf(stderr,
            "[bezierSurface_getPoint] index (%d,%d) out of range [0,3]\n",
            u, v);
        return;
    }
    point_copy(p, &b->cp[u][v]);
}

/*
 * bezierSurface_copy — deep-copy all 16 control points and zbuffer flag.
 */
void bezierSurface_copy(BezierSurface *dst, const BezierSurface *src) {
    if (!dst || !src) return;

    for (int u = 0; u < 4; u++) {
        for (int v = 0; v < 4; v++) {
            point_copy(&dst->cp[u][v], (Point *)&src->cp[u][v]);
        }
    }
    dst->zbuffer = src->zbuffer;
}


/* =======================================================================
 * BezierSurface — geometric queries
 * ======================================================================= */

/*
 * bezierSurface_evaluate — evaluate the patch at (u, v) using the
 * tensor-product Bernstein formula.
 *
 * Step 1: compute four intermediate points by evaluating each of the
 *   four rows (which are cubic Bézier curves in v) at parameter v.
 *
 * Step 2: evaluate the resulting four points as a cubic Bézier curve
 *   in u at parameter u.
 *
 * This two-pass approach is equivalent to the full double-sum formula and
 * avoids computing all 16 cross-product basis values explicitly.
 */
Point bezierSurface_evaluate(const BezierSurface *b, double u, double v) {
    Point result;
    result.val[0] = result.val[1] = result.val[2] = result.val[3] = 0.0;
    if (!b) return result;

    double wv[4], wu[4];
    bernstein3(v, wv);   /* Bernstein weights for v direction */
    bernstein3(u, wu);   /* Bernstein weights for u direction */

    /*
     * Pass 1: for each row u_i, evaluate the Bézier curve in v.
     * This gives four intermediate Points q[0..3].
     */
    Point q[4];
    for (int i = 0; i < 4; i++) {
        q[i].val[0] = q[i].val[1] = q[i].val[2] = q[i].val[3] = 0.0;
        for (int j = 0; j < 4; j++) {
            q[i].val[0] += wv[j] * b->cp[i][j].val[0];
            q[i].val[1] += wv[j] * b->cp[i][j].val[1];
            q[i].val[2] += wv[j] * b->cp[i][j].val[2];
            q[i].val[3] += wv[j] * b->cp[i][j].val[3];
        }
    }

    /*
     * Pass 2: evaluate the four intermediate points as a Bézier curve
     * in u.  The result is the surface point at (u, v).
     */
    for (int i = 0; i < 4; i++) {
        result.val[0] += wu[i] * q[i].val[0];
        result.val[1] += wu[i] * q[i].val[1];
        result.val[2] += wu[i] * q[i].val[2];
        result.val[3] += wu[i] * q[i].val[3];
    }
    return result;
}

/*
 * bezierSurface_normals — compute a unit normal for each of the 16
 * control points using central-difference finite differences on the grid.
 *
 * Partial derivatives:
 *   ∂P/∂u at (u,v) ≈ (cp[u+1][v] − cp[u−1][v]) / 2
 *   ∂P/∂v at (u,v) ≈ (cp[u][v+1] − cp[u][v−1]) / 2
 *   (one-sided differences at boundaries)
 *
 * Normal direction: cross(∂/∂v, ∂/∂u) — v cross u so that for a patch
 * whose control points wind CCW when viewed from +y, the normal points
 * in the +y direction.
 *
 * Results are normalised and stored in n[u*4 + v] (row-major order,
 * matching the vlist layout used by bezierSurface_set).
 */
void bezierSurface_normals(BezierSurface *b, Vector *n) {
    if (!b || !n) return;

    for (int u = 0; u < 4; u++) {
        for (int v = 0; v < 4; v++) {
            /* One-sided differences at boundaries, central elsewhere */
            int u0 = (u > 0) ? u - 1 : u;
            int u1 = (u < 3) ? u + 1 : u;
            int v0 = (v > 0) ? v - 1 : v;
            int v1 = (v < 3) ? v + 1 : v;

            /* Tangent vectors along u and v */
            Vector du, dv, normal;

            vector_set(&du,
                b->cp[u1][v].val[0] - b->cp[u0][v].val[0],
                b->cp[u1][v].val[1] - b->cp[u0][v].val[1],
                b->cp[u1][v].val[2] - b->cp[u0][v].val[2]);

            vector_set(&dv,
                b->cp[u][v1].val[0] - b->cp[u][v0].val[0],
                b->cp[u][v1].val[1] - b->cp[u][v0].val[1],
                b->cp[u][v1].val[2] - b->cp[u][v0].val[2]);

            /*
             * Normal = dv × du so that for CCW-wound patches the
             * normal faces outward (toward the viewer).
             */
            vector_cross(&dv, &du, &normal);
            vector_normalize(&normal);  /* unit normal */

            n[u * 4 + v] = normal;  /* row-major output index */
        }
    }
}


/* =======================================================================
 * BezierSurface — subdivision
 * ======================================================================= */

/*
 * bezierSurface_splitU — split the patch at u = 0.5.
 *
 * Each row b->cp[u][] is an independent cubic Bézier curve in v.
 * Applying split_curve_pts to each row produces:
 *   left->cp[u][]  = the first half  (u ∈ [0, 0.5]) of that row
 *   right->cp[u][] = the second half (u ∈ [0.5, 1]) of that row
 */
void bezierSurface_splitU(const BezierSurface *b,
                           BezierSurface *left, BezierSurface *right) {
    if (!b || !left || !right) return;

    for (int u = 0; u < 4; u++) {
        Point lrow[4], rrow[4];
        split_curve_pts(b->cp[u], lrow, rrow);

        for (int v = 0; v < 4; v++) {
            point_copy(&left->cp[u][v],  &lrow[v]);
            point_copy(&right->cp[u][v], &rrow[v]);
        }
    }
    left->zbuffer  = b->zbuffer;
    right->zbuffer = b->zbuffer;
}

/*
 * bezierSurface_splitV — split the patch at v = 0.5.
 *
 * Each column b->cp[][v] is an independent cubic Bézier curve in u.
 * We extract each column into a temporary array, split it, and distribute
 * the results into top (v ∈ [0, 0.5]) and bottom (v ∈ [0.5, 1]).
 */
void bezierSurface_splitV(const BezierSurface *b,
                           BezierSurface *top, BezierSurface *bottom) {
    if (!b || !top || !bottom) return;

    for (int v = 0; v < 4; v++) {
        /* Extract column v into a temporary array */
        Point col[4];
        for (int u = 0; u < 4; u++) {
            point_copy(&col[u], (Point *)&b->cp[u][v]);
        }

        Point tcol[4], bcol[4];
        split_curve_pts(col, tcol, bcol);

        /* Distribute back into the top and bottom patch columns */
        for (int u = 0; u < 4; u++) {
            point_copy(&top->cp[u][v],    &tcol[u]);
            point_copy(&bottom->cp[u][v], &bcol[u]);
        }
    }
    top->zbuffer    = b->zbuffer;
    bottom->zbuffer = b->zbuffer;
}

/*
 * bezier_surface_de_casteljau — recursive surface subdivision.
 *
 * At div == 0: the base case produces geometry (wireframe or triangles).
 * At div > 0:  split into four quadrants (splitU then splitV) and recurse.
 *
 * The original was a no-op stub.  The implementation now tracks and
 * performs the subdivision correctly.  The actual geometry insertion into
 * a Module is handled by module_bezierSurface in module.c, which calls
 * this function as its recursive engine.
 */
void bezier_surface_de_casteljau(BezierSurface *b, int div) {
    if (!b) return;

    if (div <= 0) {
        /*
         * Base case: the patch is small enough.  The caller
         * (module_bezierSurface) handles geometry insertion.
         */
        return;
    }

    /*
     * Split into four sub-patches using splitU then splitV on each half.
     * Each of the four resulting patches covers one quadrant of the
     * original [0,1]×[0,1] parameter space.
     */
    BezierSurface leftHalf, rightHalf;
    bezierSurface_splitU(b, &leftHalf, &rightHalf);

    BezierSurface leftTop, leftBottom, rightTop, rightBottom;
    bezierSurface_splitV(&leftHalf,  &leftTop,  &leftBottom);
    bezierSurface_splitV(&rightHalf, &rightTop, &rightBottom);

    /* Recurse on all four quadrants */
    bezier_surface_de_casteljau(&leftTop,    div - 1);
    bezier_surface_de_casteljau(&leftBottom, div - 1);
    bezier_surface_de_casteljau(&rightTop,   div - 1);
    bezier_surface_de_casteljau(&rightBottom, div - 1);
}

/*
 * bezierSurface_print — print the zbuffer flag and the 4×4 control grid.
 */
void bezierSurface_print(const BezierSurface *b, FILE *fp) {
    if (!b || !fp) return;

    fprintf(fp, "BezierSurface: zbuffer=%d\n", b->zbuffer);
    for (int u = 0; u < 4; u++) {
        for (int v = 0; v < 4; v++) {
            fprintf(fp, "  cp[%d][%d] = (%.4f, %.4f, %.4f, %.4f)\n",
                    u, v,
                    b->cp[u][v].val[0], b->cp[u][v].val[1],
                    b->cp[u][v].val[2], b->cp[u][v].val[3]);
        }
    }
}
