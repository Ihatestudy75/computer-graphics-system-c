
#include "view.h"
#include <stdio.h>
#include <math.h>

/* Tolerance used by view3D_isValid to detect near-parallel vectors */
#define VIEW_PARALLEL_EPS 1e-6


/* =======================================================================
 * View2D functions
 * ======================================================================= */

/*
 * view2D_set — fill a View2D from its constituent parameters.
 *
 * The x vector is used as-is; the caller is responsible for normalising
 * it to unit length before calling this function.
 *
 * Spec §8.1: "void view2D_set(View2D *view, Point *vrp, float dx,
 *              Vector *x, int sx, int sy) — fills out the View2D."
 */
void view2D_set(View2D *view, Point *vrp, double du,
                Vector *x, int screenx, int screeny) {
    if (!view || !vrp || !x) return;

    point_copy(&view->vrp, vrp);    /* copy the view centre point           */
    view->du      = du;             /* world-space width of the view window  */
    vector_copy(&view->x, x);      /* copy the normalised x-axis direction  */
    view->screenx = screenx;        /* image width  in pixels (C columns)    */
    view->screeny = screeny;        /* image height in pixels (R rows)       */
}

/*
 * view2D_init — set a View2D to safe, usable default values.
 *
 * Default view: world origin as centre, 1-unit-wide axis-aligned window,
 * mapping to a 640×480 image.  Suitable as a starting point before
 * selective field overrides.
 */
void view2D_init(View2D *view) {
    if (!view) return;

    /* Centre at the world origin */
    point_set2D(&view->vrp, 0.0, 0.0);

    /* 1 world unit wide */
    view->du = 1.0;

    /* Axis-aligned: x-axis points right (θv = 0 → nx=1, ny=0) */
    vector_set(&view->x, 1.0, 0.0, 0.0);

    /* Standard SD resolution */
    view->screenx = 640;
    view->screeny = 480;
}

/*
 * view2D_copy — deep-copy all fields of *src into *dest.
 *
 * Uses point_copy and vector_copy for the embedded structs so the copy
 * is correct even if Point/Vector acquire additional fields later.
 */
void view2D_copy(View2D *dest, const View2D *src) {
    if (!dest || !src || dest == src) return;

    point_copy (&dest->vrp, (Point  *)&src->vrp);
    dest->du = src->du;
    vector_copy(&dest->x,   (Vector *)&src->x);
    dest->screenx = src->screenx;
    dest->screeny = src->screeny;
}

/*
 * view2D_print — write a human-readable description to fp.
 *
 * Prints all five fields so the complete view setup is visible at once.
 */
void view2D_print(const View2D *view, FILE *fp) {
    if (!fp) return;
    if (!view) { fprintf(fp, "View2D: (null)\n"); return; }

    fprintf(fp, "View2D:\n");
    fprintf(fp, "  vrp     = (%.4f, %.4f)\n",
            view->vrp.val[0], view->vrp.val[1]);
    fprintf(fp, "  du      = %.4f\n", view->du);
    fprintf(fp, "  x-axis  = (%.4f, %.4f)\n",
            view->x.val[0], view->x.val[1]);
    fprintf(fp, "  screen  = %d × %d  (w × h)\n",
            view->screenx, view->screeny);
}

/*
 * matrix_setView2D — build the 2D view transformation matrix.
 *
 * Pipeline (spec §8.1, eq. 20), applied right-to-left to a world point:
 *
 *   Step 1  T(−V0x, −V0y)      translate view centre to origin
 *   Step 2  Rz(nx, −ny)        align view x-axis with screen x-axis
 *   Step 3  S(C/du, −R/dv)     scale to pixels; negate y to flip axis
 *   Step 4  T(C/2, R/2)        shift to screen top-left origin
 *
 * Because matrix_* helpers premultiply (m ← T·m), we call them in the
 * same order as the application steps; each call prepends its transform
 * to the front of the accumulated product.
 *
 * The derived view height is:  dv = du · R / C   (spec eq. 19)
 */
void matrix_setView2D(Matrix *m, View2D *view) {
    if (!m || !view) return;

    /* Protect against degenerate screen dimensions */
    if (view->screenx <= 0 || view->screeny <= 0 || view->du <= 0.0) {
        fprintf(stderr,
            "[matrix_setView2D] invalid view dimensions "
            "(screenx=%d, screeny=%d, du=%.4f)\n",
            view->screenx, view->screeny, view->du);
        return;
    }

    /* Screen dimensions as doubles to avoid integer division */
    double C = (double)view->screenx; /* number of columns */
    double R = (double)view->screeny; /* number of rows    */

    /* World-space view rectangle dimensions */
    double du = view->du;
    double dv = du * R / C; /* spec eq. 19: dv = du * R / C */

    /*
     * View x-axis components: (nx, ny) = (cos θv, sin θv)
     * The spec rotation is Rz(nx, −ny) — pass cth=nx, sth=−ny
     * so the rotation matrix correctly un-rotates the world frame
     * to align with the view frame.
     */
    double nx = view->x.val[0]; /* cos θv */
    double ny = view->x.val[1]; /* sin θv */

    /* --- Build the VTM step-by-step ----------------------------------- */

    /* Start from the identity */
    matrix_identity(m);

    /* Step 1: T(−V0x, −V0y) — move view centre to origin */
    matrix_translate2D(m, -view->vrp.val[0], -view->vrp.val[1]);

    /* Step 2: Rz(nx, −ny) — align view x-axis with screen x-axis.
     * Passing sth = −ny implements the rotation by −θv that brings the
     * rotated view frame back to the standard axis-aligned frame.       */
    matrix_rotateZ(m, nx, -ny);

    /* Step 3: S(C/du, −R/dv) — scale to pixel dimensions.
     * The negative y scale flips the world y-axis (positive up) to the
     * screen y-axis (positive down).                                     */
    matrix_scale2D(m, C / du, -R / dv);

    /* Step 4: T(C/2, R/2) — shift to image top-left corner */
    matrix_translate2D(m, C / 2.0, R / 2.0);
}


/* =======================================================================
 * View3D functions
 * ======================================================================= */

/*
 * view3D_set — fill a View3D from its constituent parameters.
 *
 * All ten fields are set.  vpn and vup need not be unit vectors; they
 * will be normalised inside matrix_setView3D.
 */
void view3D_set(View3D *view,
                Point *vrp, Vector *vpn, Vector *vup,
                double d, double du, double dv,
                double f, double b,
                int screenx, int screeny) {
    if (!view || !vrp || !vpn || !vup) return;

    point_copy (&view->vrp, vrp);   /* View Reference Point */
    vector_copy(&view->vpn, vpn);   /* View Plane Normal    */
    vector_copy(&view->vup, vup);   /* View Up Vector       */
    view->d       = d;              /* focal distance       */
    view->du      = du;             /* view window width    */
    view->dv      = dv;             /* view window height   */
    view->f       = f;              /* front clip distance  */
    view->b       = b;              /* back  clip distance  */
    view->screenx = screenx;        /* image width  pixels  */
    view->screeny = screeny;        /* image height pixels  */
}

/*
 * view3D_init — set a View3D to a safe default camera.
 *
 * Default: camera at the origin looking down −Z with Y up.
 * Produces a usable 640×480 render with a 60° approximate FOV.
 */
void view3D_init(View3D *view) {
    if (!view) return;

    /* Camera placed at the world origin */
    point_set3D(&view->vrp, 0.0, 0.0, 0.0);

    /* Looking in the −Z direction (into the screen) */
    vector_set(&view->vpn, 0.0, 0.0, -1.0);

    /* World Y is "up" */
    vector_set(&view->vup, 0.0, 1.0, 0.0);

    /* Reasonable perspective defaults */
    view->d  = 1.0;   /* focal distance = 1 world unit         */
    view->du = 1.0;   /* view window 1×1 world unit            */
    view->dv = 1.0;

    /* Clip planes: near at 0.1, far at 100 world units */
    view->f = 0.1;
    view->b = 100.0;

    /* Standard resolution */
    view->screenx = 640;
    view->screeny = 480;
}

/*
 * view3D_copy — deep-copy all ten fields of *src into *dest.
 */
void view3D_copy(View3D *dest, const View3D *src) {
    if (!dest || !src || dest == src) return;

    point_copy (&dest->vrp, (Point  *)&src->vrp);
    vector_copy(&dest->vpn, (Vector *)&src->vpn);
    vector_copy(&dest->vup, (Vector *)&src->vup);
    dest->d       = src->d;
    dest->du      = src->du;
    dest->dv      = src->dv;
    dest->f       = src->f;
    dest->b       = src->b;
    dest->screenx = src->screenx;
    dest->screeny = src->screeny;
}

/*
 * view3D_print — write all ten View3D fields to fp.
 *
 * Printing the full struct in one call is much more convenient than
 * inspecting individual members in a debugger.
 */
void view3D_print(const View3D *view, FILE *fp) {
    if (!fp) return;
    if (!view) { fprintf(fp, "View3D: (null)\n"); return; }

    fprintf(fp, "View3D:\n");
    fprintf(fp, "  vrp     = (%.4f, %.4f, %.4f)\n",
            view->vrp.val[0], view->vrp.val[1], view->vrp.val[2]);
    fprintf(fp, "  vpn     = (%.4f, %.4f, %.4f)\n",
            view->vpn.val[0], view->vpn.val[1], view->vpn.val[2]);
    fprintf(fp, "  vup     = (%.4f, %.4f, %.4f)\n",
            view->vup.val[0], view->vup.val[1], view->vup.val[2]);
    fprintf(fp, "  d       = %.4f\n", view->d);
    fprintf(fp, "  du      = %.4f    dv = %.4f\n", view->du, view->dv);
    fprintf(fp, "  f       = %.4f    b  = %.4f\n", view->f,  view->b);
    fprintf(fp, "  screen  = %d × %d  (w × h)\n",
            view->screenx, view->screeny);
}

/*
 * view3D_isValid — check that the View3D satisfies all spec constraints.
 *
 * Returns 1 (valid) only when:
 *   • d > 0, du > 0, dv > 0
 *   • screenx > 0, screeny > 0
 *   • 0 < f < b
 *   • VPN and VUP are not near-parallel
 *     (parallel means |VPN × VUP| ≈ 0, making the u-axis undefined)
 *
 * This is intended as a debug/setup check before calling matrix_setView3D.
 */
int view3D_isValid(const View3D *view) {
    if (!view) return 0;

    /* Positive dimensions and clip planes */
    if (view->d  <= 0.0) { fprintf(stderr, "[view3D_isValid] d <= 0\n");      return 0; }
    if (view->du <= 0.0) { fprintf(stderr, "[view3D_isValid] du <= 0\n");     return 0; }
    if (view->dv <= 0.0) { fprintf(stderr, "[view3D_isValid] dv <= 0\n");     return 0; }
    if (view->screenx <= 0) { fprintf(stderr, "[view3D_isValid] screenx <= 0\n"); return 0; }
    if (view->screeny <= 0) { fprintf(stderr, "[view3D_isValid] screeny <= 0\n"); return 0; }
    if (view->f <= 0.0) { fprintf(stderr, "[view3D_isValid] f <= 0\n");       return 0; }
    if (view->b <= view->f) { fprintf(stderr, "[view3D_isValid] b <= f\n");   return 0; }

    /*
     * VPN ∥ VUP check: if their cross product has near-zero length the
     * u-axis cannot be computed and the view frame is degenerate.
     */
    Vector tmp;
    vector_cross((Vector *)&view->vpn, (Vector *)&view->vup, &tmp);
    double crossLen = vector_length(&tmp);
    if (crossLen < VIEW_PARALLEL_EPS) {
        fprintf(stderr,
            "[view3D_isValid] VPN and VUP are (near-)parallel — "
            "view frame is degenerate\n");
        return 0;
    }

    return 1;
}

/*
 * view3D_buildUVW — compute the orthonormal camera (UVN) basis.
 *
 * This is the Gram-Schmidt process applied to VPN and VUP:
 *
 *   w = normalise(VPN)          — look axis (into the scene)
 *   u = normalise(VUP × w)      — camera right (cross gives right-hand)
 *   v = normalise(w × u)        — camera up    (orthogonalised to both)
 *
 * Operand order matters:
 *   VUP × w, NOT w × VUP:  for a standard right-handed camera with
 *     VPN = (0,0,−1) and VUP = (0,1,0), this gives u = (1,0,0) ✓
 *   w × u,  NOT u × w:     gives v = (0,1,0) ✓
 *
 * Exposed as a primary method so any code that needs the camera axes
 * (e.g. lighting, eye-position computation, view culling) can call
 * it instead of duplicating the cross-product sequence.
 */
void view3D_buildUVW(const View3D *view, Vector *u, Vector *v, Vector *w) {
    if (!view || !u || !v || !w) return;

    /*
     * w = normalise(VPN)
     * Work on a local copy so the original VPN is never modified
     * (spec §8.2: "no side-effects").
     */
    vector_copy(w, (Vector *)&view->vpn);
    vector_normalize(w);

    /*
     * u = normalise(VUP × w)
     * VUP is also copied locally before being passed to vector_cross.
     */
    Vector vup_local;
    vector_copy(&vup_local, (Vector *)&view->vup);
    vector_cross(&vup_local, w, u);
    vector_normalize(u);

    /*
     * v = normalise(w × u)
     * This orthogonalises v with respect to both w and u (Gram-Schmidt),
     * correcting for any non-perpendicularity in the original VUP.
     */
    vector_cross(w, u, v);
    vector_normalize(v);
}

/*
 * view3D_eyePosition — world-space position of the centre of projection.
 *
 * The eye sits d units from the VRP in the −w direction (opposite to the
 * look direction w = normalise(VPN)):
 *
 *   eye = VRP − d · w
 *
 * This is the point that should be stored in DrawState.viewer so that
 * lighting calculations use the correct eye position.
 */
void view3D_eyePosition(const View3D *view, Point *eye) {
    if (!view || !eye) return;

    /* Build the normalised look axis (w) without modifying view */
    Vector w;
    vector_copy(&w, (Vector *)&view->vpn);
    vector_normalize(&w);

    /* eye = VRP − d·w */
    eye->val[0] = view->vrp.val[0] - view->d * w.val[0];
    eye->val[1] = view->vrp.val[1] - view->d * w.val[1];
    eye->val[2] = view->vrp.val[2] - view->d * w.val[2];
    eye->val[3] = 1.0; /* h = 1 → Cartesian point */
}

/*
 * matrix_setView3D — build the complete 3D perspective VTM.
 *
 * The six pipeline steps are applied by premultiplying a local matrix
 * vtm (so that aliasing between m and any intermediate is impossible):
 *
 *   Step 1: identity
 *   Step 2: T(−VRP)            translate VRP to origin
 *   Step 3: Ruvw               rotate world to camera frame
 *   Step 4: Perspective(d)     encode depth for perspective division
 *   Step 5: S(−C/du, −R/dv, 1) scale & flip to screen pixels
 *   Step 6: T(C/2, R/2, 0)     shift to screen origin (top-left corner)
 *
 * No field of *view is modified (spec §8.2: "no side-effects").
 */
void matrix_setView3D(Matrix *m, View3D *view) {
    if (!m || !view) return;

    /* Screen dimensions as doubles */
    double C = (double)view->screenx;
    double R = (double)view->screeny;

    /* ---- Step 1: start from identity ---- */
    Matrix vtm;
    matrix_identity(&vtm);

    /* ---- Step 2: T(−VRP) — translate camera reference point to origin ---- */
    matrix_translate(&vtm,
                     -view->vrp.val[0],
                     -view->vrp.val[1],
                     -view->vrp.val[2]);

    /* ---- Step 3: build UVN basis and apply Ruvw ---- */
    Vector u, v, w;
    view3D_buildUVW(view, &u, &v, &w); /* delegates to the primary method */

    /*
     * matrix_rotateXYZ premultiplies vtm by the matrix:
     *   | ux  uy  uz  0 |
     *   | vx  vy  vz  0 |
     *   | wx  wy  wz  0 |
     *   |  0   0   0  1 |
     * which maps world-space coordinates to the UVN camera frame.
     */
    matrix_rotateXYZ(&vtm, &u, &v, &w);

    /* ---- Step 4: perspective(d) — encode z/d in the homogeneous w ---- */
    matrix_perspective(&vtm, view->d);

    /* ---- Step 5: S(−C/du, −R/dv, 1) — scale to pixels, flip axes ----
     * Negating x and y flips both axes from right-handed world space to
     * the left-handed screen convention (x right, y down).               */
    matrix_scale(&vtm,
                 -C / view->du,   /* x: scale columns, negate (flip x) */
                 -R / view->dv,   /* y: scale rows,    negate (flip y) */
                  1.0);           /* z: unchanged                       */

    /* ---- Step 6: T(C/2, R/2, 0) — shift to screen top-left origin ---- */
    matrix_translate(&vtm, C / 2.0, R / 2.0, 0.0);

    /* Copy the finished VTM into the caller's matrix */
    matrix_copy(m, &vtm);
}
