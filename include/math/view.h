
#ifndef VIEW_H
#define VIEW_H

#include "primitive.h" /* Point, Vector           */
#include "matrix.h"    /* Matrix, all matrix_* fn */


/* =======================================================================
 * View2D — 2D orthographic view specification
 * ======================================================================= */

/**
 * View2D — describes a rectangular window in 2D world space and the
 * screen image into which it maps.
 *
 * Fields
 * ------
 * vrp      View Reference Point: the centre of the view rectangle in
 *          world coordinates (only val[0]=x and val[1]=y are used).
 * du       Width of the view rectangle in world units.
 *          The height is derived as dv = du * screeny / screenx.
 * x        Orientation of the view window's x-axis expressed as a
 *          normalised 2D vector (nx, ny) = (cos θv, sin θv).
 *          A horizontal window has x = (1, 0, 0, 0).
 * screenx  Width of the output image in pixels (C columns).
 * screeny  Height of the output image in pixels (R rows).
 */
typedef struct {
    Point  vrp;     /* centre of the view rectangle in world XY space */
    double du;      /* width of the view rectangle in world units      */
    Vector x;       /* normalised view x-axis: (cos θv, sin θv, 0, 0)  */
    int    screenx; /* image width  in pixels (C)                       */
    int    screeny; /* image height in pixels (R)                       */
} View2D;


/* =======================================================================
 * View3D — 3D perspective view specification
 * ======================================================================= */

/**
 * View3D — describes a perspective camera in 3D world space.
 *
 * Fields
 * ------
 * vrp      View Reference Point: origin of the view coordinate system
 *          (world-space point at the centre of the view plane).
 * vpn      View Plane Normal: direction from the scene toward the viewer.
 *          Need not be unit length; it is normalised internally.
 *          Must not be parallel to vup.
 * vup      View Up Vector: the world-space direction that maps to
 *          "up" in the image.  Must not be parallel to vpn.
 * d        Projection (focal) distance: positive distance along −VPN
 *          from the VRP to the centre of projection (the eye).
 *          Must be > 0.
 * du       Width of the view window in world units (centred on the VRP
 *          in the view plane).
 * dv       Height of the view window in world units.
 * f        Front clip plane distance from VRP along the positive VPN.
 *          Must satisfy 0 < f < b.
 * b        Back  clip plane distance from VRP along the positive VPN.
 *          Must satisfy b > f.
 * screenx  Width of the output image in pixels (C columns).
 * screeny  Height of the output image in pixels (R rows).
 */
typedef struct {
    Point  vrp;     /* View Reference Point (world space)                */
    Vector vpn;     /* View Plane Normal (gaze direction, un-normalised)  */
    Vector vup;     /* View Up Vector (world "up", un-normalised)         */
    double d;       /* focal / projection distance (> 0)                  */
    double du;      /* view window width  in world units                  */
    double dv;      /* view window height in world units                  */
    double f;       /* front clip plane distance along +VPN (0 < f < b)  */
    double b;       /* back  clip plane distance along +VPN (b > f)       */
    int    screenx; /* image width  in pixels (C)                         */
    int    screeny; /* image height in pixels (R)                         */
} View3D;


/* =======================================================================
 * View2D functions
 * ======================================================================= */

/**
 * view2D_set — fill a View2D struct from its components.
 *
 * All fields are copied; no memory is allocated.  The x vector should
 * already be normalised to unit length before being passed in.
 *
 * @param view    destination View2D (must not be NULL)
 * @param vrp     centre of the view rectangle in world XY (must not be NULL)
 * @param du      width of the view rectangle in world units (> 0)
 * @param x       normalised view x-axis vector (must not be NULL)
 * @param screenx image width  in pixels (> 0)
 * @param screeny image height in pixels (> 0)
 */
void view2D_set(View2D *view, Point *vrp, double du,
                Vector *x, int screenx, int screeny);

/**
 * view2D_init — initialise a View2D with safe, usable defaults.
 *
 * Defaults:
 *   vrp     = (0, 0, 0, 1)   — world origin
 *   du      = 1.0            — one world unit wide
 *   x       = (1, 0, 0, 0)  — axis-aligned (no rotation)
 *   screenx = 640, screeny = 480
 *
 * Useful for creating a throwaway View2D before setting individual fields.
 *
 * @param view  destination View2D (must not be NULL)
 */
void view2D_init(View2D *view);

/**
 * view2D_copy — copy all fields of *src into *dest.
 *
 * @param dest  destination View2D (must not be NULL)
 * @param src   source      View2D (must not be NULL)
 */
void view2D_copy(View2D *dest, const View2D *src);

/**
 * view2D_print — write a human-readable description to fp.
 *
 * Useful for debugging view setup before rendering.
 *
 * @param view  View2D to describe (NULL → prints a placeholder)
 * @param fp    destination FILE stream (e.g. stdout)
 */
void view2D_print(const View2D *view, FILE *fp);

/**
 * matrix_setView2D — build the complete 2D view transformation matrix.
 *
 * Implements the four-matrix pipeline from spec §8.1, eq. 20:
 *
 *   VTM = T(C/2, R/2) · S(C/du, −R/dv) · Rz(nx, −ny) · T(−V0x, −V0y)
 *
 * where:
 *   C   = view->screenx   (columns)
 *   R   = view->screeny   (rows)
 *   du  = view->du        (world-space width)
 *   dv  = du * R / C      (world-space height, derived from eq. 19)
 *   nx  = view->x.val[0]  (cos θv)
 *   ny  = view->x.val[1]  (sin θv)
 *
 * On entry m is set to the identity before any premultiplications so
 * the caller does not need to pre-clear it.
 *
 * @param m     output Matrix to receive the VTM (must not be NULL)
 * @param view  View2D describing the desired view (must not be NULL)
 */
void matrix_setView2D(Matrix *m, View2D *view);


/* =======================================================================
 * View3D functions
 * ======================================================================= */

/**
 * view3D_set — fill a View3D struct from its components.
 *
 * All fields are copied; no memory is allocated.  vpn and vup need not
 * be unit vectors; they are normalised internally by matrix_setView3D.
 *
 * @param view    destination View3D (must not be NULL)
 * @param vrp     View Reference Point (must not be NULL)
 * @param vpn     View Plane Normal, gaze direction (must not be NULL)
 * @param vup     View Up Vector (must not be NULL; must not be ∥ vpn)
 * @param d       focal distance (> 0)
 * @param du      view window width  in world units (> 0)
 * @param dv      view window height in world units (> 0)
 * @param f       front clip distance along +VPN (0 < f < b)
 * @param b       back  clip distance along +VPN (b > f)
 * @param screenx image width  in pixels (> 0)
 * @param screeny image height in pixels (> 0)
 */
void view3D_set(View3D *view,
                Point *vrp, Vector *vpn, Vector *vup,
                double d, double du, double dv,
                double f, double b,
                int screenx, int screeny);

/**
 * view3D_init — initialise a View3D with safe, usable defaults.
 *
 * Defaults produce a camera at the origin looking down −Z with Y up,
 * a 1×1 world-unit view window, and a 640×480 image:
 *   vrp     = (0, 0,  0, 1)
 *   vpn     = (0, 0, −1, 0)   — looking into the screen
 *   vup     = (0, 1,  0, 0)   — world Y is up
 *   d       = 1.0
 *   du = dv = 1.0
 *   f       = 0.1,  b = 100.0
 *   screenx = 640, screeny = 480
 *
 * @param view  destination View3D (must not be NULL)
 */
void view3D_init(View3D *view);

/**
 * view3D_copy — copy all fields of *src into *dest.
 *
 * @param dest  destination View3D (must not be NULL)
 * @param src   source      View3D (must not be NULL)
 */
void view3D_copy(View3D *dest, const View3D *src);

/**
 * view3D_print — write a human-readable description to fp.
 *
 * Prints all ten fields so the entire camera setup can be inspected in
 * one call during debugging.
 *
 * @param view  View3D to describe (NULL → prints a placeholder)
 * @param fp    destination FILE stream (e.g. stdout)
 */
void view3D_print(const View3D *view, FILE *fp);

/**
 * view3D_isValid — check that the View3D parameters satisfy the
 * constraints required by matrix_setView3D.
 *
 * Checks:
 *   d > 0, du > 0, dv > 0
 *   screenx > 0, screeny > 0
 *   0 < f < b
 *   VPN and VUP are not (near-)parallel
 *
 * @param view  View3D to validate
 * @return      1 if all constraints hold, 0 otherwise
 */
int view3D_isValid(const View3D *view);

/**
 * matrix_setView3D — build the complete 3D perspective view matrix.
 *
 * Implements the pipeline from spec §8.2 without any side-effects on
 * the View3D struct.  Steps applied in order:
 *
 *   1. Identity           — start from scratch
 *   2. T(−VRP)            — translate VRP to the world origin
 *   3. Ruvw               — rotate world axes to align with camera UVN frame
 *                           u = normalise(VUP × w)    (camera right)
 *                           v = normalise(w × u)       (camera up, ortho.)
 *                           w = normalise(VPN)         (into scene)
 *   4. Perspective(d)     — encode depth in the homogeneous w component
 *   5. S(−C/du, −R/dv, 1) — scale view window to pixel dimensions;
 *                           negate x and y to flip to screen convention
 *   6. T(C/2, R/2, 0)     — shift origin to top-left corner of image
 *
 * On return m holds the complete VTM.  No field of *view is modified.
 *
 * @param m     output Matrix (must not be NULL; set to identity first)
 * @param view  View3D camera specification (must not be NULL; not modified)
 */
void matrix_setView3D(Matrix *m, View3D *view);

/**
 * view3D_buildUVW — extract the orthonormal UVN camera basis from a
 * View3D, storing the three unit vectors in *u, *v, *w.
 *
 * This is the same computation performed inside matrix_setView3D but
 * exposed as a primary method so that other callers (e.g., lighting
 * calculations that need the camera axes, or code that sets ds->viewer)
 * can obtain the basis without duplicating the cross-product logic.
 *
 *   w = normalise(view->vpn)          — points from scene toward viewer
 *   u = normalise(view->vup × w)      — camera right
 *   v = normalise(w × u)              — camera up (Gram-Schmidt ortho.)
 *
 * @param view  View3D whose VPN and VUP define the camera (not modified)
 * @param u     output: camera right unit vector (must not be NULL)
 * @param v     output: camera up   unit vector (must not be NULL)
 * @param w     output: camera look unit vector (must not be NULL)
 */
void view3D_buildUVW(const View3D *view, Vector *u, Vector *v, Vector *w);

/**
 * view3D_eyePosition — compute the world-space position of the eye (the
 * centre of projection) from the View3D parameters.
 *
 * The eye sits at distance d in the −VPN direction from the VRP:
 *   eye = VRP − d · normalise(VPN)
 *
 * Useful for placing the DrawState viewer point that is needed by the
 * lighting equations.
 *
 * @param view  source View3D (not modified)
 * @param eye   output Point to receive the eye position (must not be NULL)
 */
void view3D_eyePosition(const View3D *view, Point *eye);

#endif /* VIEW_H */
