
#include "matrix.h"
#include <math.h>
#include <stdio.h>

/* Tolerance for the fuzzy identity check in matrix_isIdentity */
#define MATRIX_EPS 1e-9


/* =======================================================================
 * 2. Vector functions
 * ======================================================================= */

/*
 * vector_set — set (x, y, z, 0.0) in a Vector.
 *
 * The homogeneous component is always 0 for a direction vector so that
 * multiplying by an affine matrix does not translate it.
 */
void vector_set(Vector *v, double x, double y, double z) {
    if (!v) return;
    v->val[0] = x;
    v->val[1] = y;
    v->val[2] = z;
    v->val[3] = 0.0;  /* h = 0: Vectors are not translated by affine matrices */
}

/*
 * vector_print — write all four components to fp for debugging.
 *
 * Printing h as well makes it easy to spot vectors that have accidentally
 * been given a non-zero homogeneous component.
 */
void vector_print(Vector *v, FILE *fp) {
    if (!v || !fp) return;
    fprintf(fp, "Vector: (%.4f, %.4f, %.4f, %.4f)\n",
            v->val[0], v->val[1], v->val[2], v->val[3]);
}

/*
 * vector_copy — copy all four components from *src to *dest.
 *
 * Self-copy guard (dest == src) prevents unnecessary work.
 */
void vector_copy(Vector *dest, Vector *src) {
    if (!dest || !src || dest == src) return;
    dest->val[0] = src->val[0];
    dest->val[1] = src->val[1];
    dest->val[2] = src->val[2];
    dest->val[3] = src->val[3];
}

/*
 * vector_length — Euclidean length of the xyz part.
 *
 * L = sqrt(vx² + vy² + vz²)
 * h is excluded; for a proper Vector h = 0 so including it would not
 * change the result, but excluding it makes the semantics explicit.
 */
double vector_length(Vector *v) {
    if (!v) return 0.0;
    double x = v->val[0], y = v->val[1], z = v->val[2];
    return sqrt(x*x + y*y + z*z);
}

/*
 * vector_normalize — scale the xyz components so the length is 1.0.
 *
 * val[3] (h) is not modified; it stays at 0.0.
 * No-op if the vector is already zero-length (avoids division by zero).
 */
void vector_normalize(Vector *v) {
    if (!v) return;
    double L = vector_length(v);
    if (L < MATRIX_EPS) return;  /* avoid division by near-zero */
    double inv = 1.0 / L;        /* one division, three multiplies */
    v->val[0] *= inv;
    v->val[1] *= inv;
    v->val[2] *= inv;
    /* val[3] unchanged: h stays 0 */
}

/*
 * vector_dot — scalar (inner) product of the xyz parts.
 *
 * d = ax·bx + ay·by + az·bz
 *
 * h components are excluded because they are always 0 for proper Vectors.
 */
double vector_dot(Vector *a, Vector *b) {
    if (!a || !b) return 0.0;
    return a->val[0]*b->val[0]
         + a->val[1]*b->val[1]
         + a->val[2]*b->val[2];
}

/*
 * vector_cross — cross (vector) product c = a × b.
 *
 * The result c is a direction vector (h = 0), orthogonal to both a and b.
 * Computed into temporaries so c may safely alias a (though aliasing b
 * would still corrupt the result — callers should use distinct variables).
 *
 * Formula (spec §7.1, eq. 4):
 *   cx = ay·bz − az·by
 *   cy = az·bx − ax·bz
 *   cz = ax·by − ay·bx
 */
void vector_cross(Vector *a, Vector *b, Vector *c) {
    if (!a || !b || !c) return;

    /* Temporaries protect against aliasing a or b with c */
    double cx = a->val[1]*b->val[2] - a->val[2]*b->val[1];
    double cy = a->val[2]*b->val[0] - a->val[0]*b->val[2];
    double cz = a->val[0]*b->val[1] - a->val[1]*b->val[0];

    c->val[0] = cx;
    c->val[1] = cy;
    c->val[2] = cz;
    c->val[3] = 0.0;  /* result is a direction vector; h = 0 */
}

/*
 * vector_reflect — compute the reflection of incident vector d across
 * unit normal n, writing the result into r.
 *
 * r = d − 2(d·n)n
 *
 * This is the standard specular reflection formula.  n must be a unit
 * vector for the formula to be correct.  d points toward the surface;
 * r points away from it (the reflected ray direction).
 */
void vector_reflect(Vector *d, Vector *n, Vector *r) {
    if (!d || !n || !r) return;

    double dn2 = 2.0 * vector_dot(d, n);  /* 2(d·n) scalar */

    r->val[0] = d->val[0] - dn2 * n->val[0];
    r->val[1] = d->val[1] - dn2 * n->val[1];
    r->val[2] = d->val[2] - dn2 * n->val[2];
    r->val[3] = 0.0;  /* result is a direction vector */
}

/*
 * vector_scale — multiply each xyz component by scalar s in place.
 *
 * val[3] (h) is not modified.  Useful for scaling a direction before
 * normalisation, or for attenuating a colour-as-vector contribution.
 */
void vector_scale(Vector *v, double s) {
    if (!v) return;
    v->val[0] *= s;
    v->val[1] *= s;
    v->val[2] *= s;
    /* val[3] unchanged */
}

/*
 * vector_add — component-wise addition: dst = a + b.
 *
 * All four components are added.  For proper Vectors h = 0 so the
 * result's h is also 0.  Temporaries allow dst to alias a or b.
 */
void vector_add(Vector *a, Vector *b, Vector *dst) {
    if (!a || !b || !dst) return;
    double x = a->val[0] + b->val[0];
    double y = a->val[1] + b->val[1];
    double z = a->val[2] + b->val[2];
    double h = a->val[3] + b->val[3];
    dst->val[0] = x;  dst->val[1] = y;
    dst->val[2] = z;  dst->val[3] = h;
}

/*
 * vector_subtract — component-wise subtraction: dst = a − b.
 *
 * Point − Point = Vector (h: 1 − 1 = 0 → direction vector).
 * Point − Vector = Point (h: 1 − 0 = 1 → remains a point).
 */
void vector_subtract(Vector *a, Vector *b, Vector *dst) {
    if (!a || !b || !dst) return;
    double x = a->val[0] - b->val[0];
    double y = a->val[1] - b->val[1];
    double z = a->val[2] - b->val[2];
    double h = a->val[3] - b->val[3];
    dst->val[0] = x;  dst->val[1] = y;
    dst->val[2] = z;  dst->val[3] = h;
}

/*
 * vector_equal — exact equality test on all four components.
 *
 * Uses == which is appropriate for comparing against known constants,
 * not for arithmetic results.
 */
int vector_equal(const Vector *a, const Vector *b) {
    if (!a || !b) return 0;
    return (a->val[0] == b->val[0]) &&
           (a->val[1] == b->val[1]) &&
           (a->val[2] == b->val[2]) &&
           (a->val[3] == b->val[3]);
}


/* =======================================================================
 * 3. Generic matrix functions
 * ======================================================================= */

/*
 * matrix_print — formatted 4×4 table followed by a separator line.
 *
 * Each element is printed with two decimal places in an 8-character field
 * so that columns align for matrices with mixed positive/negative values.
 */
void matrix_print(Matrix *m, FILE *fp) {
    if (!m || !fp) return;
    for (int r = 0; r < 4; r++) {
        fprintf(fp, "| ");
        for (int c = 0; c < 4; c++) {
            fprintf(fp, "%8.4f ", m->m[r][c]);
        }
        fprintf(fp, "|\n");
    }
    fprintf(fp, "-------------------------------------------\n");
}

/*
 * matrix_clear — set all 16 elements to 0.0.
 */
void matrix_clear(Matrix *m) {
    if (!m) return;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            m->m[r][c] = 0.0;
}

/*
 * matrix_identity — set m to the 4×4 identity matrix.
 *
 * Calls matrix_clear first so all off-diagonal elements are guaranteed
 * to be 0.0 even if m contained garbage.
 */
void matrix_identity(Matrix *m) {
    if (!m) return;
    matrix_clear(m);
    m->m[0][0] = 1.0;  /* diagonal: 1 0 0 0 */
    m->m[1][1] = 1.0;  /*           0 1 0 0 */
    m->m[2][2] = 1.0;  /*           0 0 1 0 */
    m->m[3][3] = 1.0;  /*           0 0 0 1 */
}

/*
 * matrix_get — return the element at (r, c).
 *
 * Returns 0.0 for out-of-range indices so that calling code cannot read
 * outside the 4×4 array.
 */
double matrix_get(Matrix *m, int r, int c) {
    if (!m || r < 0 || r > 3 || c < 0 || c > 3) return 0.0;
    return m->m[r][c];
}

/*
 * matrix_set — write value to element (r, c).
 *
 * Silently ignores out-of-range indices.
 */
void matrix_set(Matrix *m, int r, int c, double value) {
    if (!m || r < 0 || r > 3 || c < 0 || c > 3) return;
    m->m[r][c] = value;
}

/*
 * matrix_copy — copy all 16 elements of *src into *dest.
 *
 * Self-copy guard: if dest == src no work is done.
 */
void matrix_copy(Matrix *dest, Matrix *src) {
    if (!dest || !src || dest == src) return;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            dest->m[r][c] = src->m[r][c];
}

/*
 * matrix_transpose — reflect m across its main diagonal in place.
 *
 * Only the upper-triangle elements are swapped with their lower-triangle
 * counterparts; the diagonal is unchanged.
 */
void matrix_transpose(Matrix *m) {
    if (!m) return;
    for (int r = 0; r < 4; r++) {
        for (int c = r + 1; c < 4; c++) {
            double tmp  = m->m[r][c];
            m->m[r][c] = m->m[c][r];
            m->m[c][r] = tmp;
        }
    }
}

/*
 * matrix_multiply — compute [m] = [left] × [right].
 *
 * A local temporary holds the result so that m may safely alias left or
 * right.  After the 64 multiply-add operations the result is copied into m.
 */
void matrix_multiply(Matrix *left, Matrix *right, Matrix *m) {
    if (!left || !right || !m) return;

    Matrix tmp;  /* local accumulator — safe even if m aliases an operand */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            tmp.m[r][c] = 0.0;
            for (int k = 0; k < 4; k++) {
                tmp.m[r][c] += left->m[r][k] * right->m[k][c];
            }
        }
    }
    matrix_copy(m, &tmp);  /* write result, even if m == left or m == right */
}

/*
 * matrix_equal — exact equality of all 16 elements.
 *
 * Use for testing against known constant matrices; not for arithmetic results.
 */
int matrix_equal(const Matrix *a, const Matrix *b) {
    if (!a || !b) return 0;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (a->m[r][c] != b->m[r][c]) return 0;
    return 1;
}

/*
 * matrix_isIdentity — fuzzy identity test within tolerance MATRIX_EPS.
 *
 * Checks that diagonal elements are within MATRIX_EPS of 1.0 and all
 * off-diagonal elements are within MATRIX_EPS of 0.0.
 */
int matrix_isIdentity(const Matrix *m) {
    if (!m) return 0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            double expected = (r == c) ? 1.0 : 0.0;
            double diff = m->m[r][c] - expected;
            if (diff < 0.0) diff = -diff;
            if (diff > MATRIX_EPS) return 0;
        }
    }
    return 1;
}


/* =======================================================================
 * Geometry transform functions
 * ======================================================================= */

/*
 * matrix_xformPoint — q = m · p  (column-vector, left-multiply convention).
 *
 * Computes q directly; p and q must differ (spec §7.2).
 * All four homogeneous components are transformed so that projective
 * points (h ≠ 1) are handled correctly.
 */
void matrix_xformPoint(Matrix *m, Point *p, Point *q) {
    if (!m || !p || !q) return;
    /* compute into q directly — p and q must be distinct per spec */
    for (int r = 0; r < 4; r++) {
        q->val[r] = 0.0;
        for (int c = 0; c < 4; c++) {
            q->val[r] += m->m[r][c] * p->val[c];
        }
    }
}

/*
 * matrix_xformVector — w = m · v.
 *
 * Identical in implementation to matrix_xformPoint; the distinction is
 * semantic: a Vector has h = 0 so the translation column of an affine
 * matrix (m[r][3]) contributes nothing.  v and w must differ.
 */
void matrix_xformVector(Matrix *m, Vector *v, Vector *w) {
    if (!m || !v || !w) return;
    for (int r = 0; r < 4; r++) {
        w->val[r] = 0.0;
        for (int c = 0; c < 4; c++) {
            w->val[r] += m->m[r][c] * v->val[c];
        }
    }
}

/*
 * matrix_xformPolygon — transform all vertices and (if present) normals.
 *
 * Vertices are transformed as Points.  Normals are transformed as
 * Vectors (h = 0 means translation has no effect on direction vectors).
 *
 * Bug fix: the original had a duplicate NULL check mid-function.  Removed.
 */
void matrix_xformPolygon(Matrix *m, Polygon *p) {
    if (!m || !p) return;
    if (!p->vertex) return;  /* nothing to transform */

    int n = p->nVertex;
    Point  tp;
    Vector tv;

    /* Transform every vertex as a Point: pᵢ ← m · pᵢ */
    for (int i = 0; i < n; i++) {
        matrix_xformPoint(m, &p->vertex[i], &tp);
        p->vertex[i] = tp;
    }

    /* Transform normals as Vectors if the normal array is present */
    if (p->normal) {
        for (int i = 0; i < n; i++) {
            matrix_xformVector(m, &p->normal[i], &tv);
            p->normal[i] = tv;
        }
    }
}

/*
 * matrix_xformPolyline — transform every vertex of a Polyline.
 *
 * Bug fix: the original read p->numVertex before the NULL check.  The
 * NULL check is now first so p is validated before being dereferenced.
 */
void matrix_xformPolyline(Matrix *m, Polyline *p) {
    if (!m || !p) return;
    if (!p->vertex) return;  /* empty polyline */

    int n = p->numVertex;
    Point tmp;

    for (int i = 0; i < n; i++) {
        matrix_xformPoint(m, &p->vertex[i], &tmp);
        p->vertex[i] = tmp;
    }
}

/*
 * matrix_xformLine — transform both endpoints of a Line.
 */
void matrix_xformLine(Matrix *m, Line *l) {
    if (!m || !l) return;

    Point tf, tt;
    matrix_xformPoint(m, &l->from, &tf);
    matrix_xformPoint(m, &l->to,   &tt);
    l->from = tf;
    l->to   = tt;
}


/* =======================================================================
 * 4. 2D transformation helpers (all premultiply: m ← T · m)
 * ======================================================================= */

/*
 * matrix_scale2D — premultiply by S(sx, sy, 1, 1).
 *
 * S = | sx  0  0  0 |
 *     |  0 sy  0  0 |
 *     |  0  0  1  0 |
 *     |  0  0  0  1 |
 */
void matrix_scale2D(Matrix *m, double sx, double sy) {
    if (!m) return;
    Matrix S;
    matrix_identity(&S);
    S.m[0][0] = sx;  /* scale x */
    S.m[1][1] = sy;  /* scale y */
    /* m[2][2] and m[3][3] remain 1 from identity */
    matrix_multiply(&S, m, m);  /* m ← S · m */
}

/*
 * matrix_rotateZ — premultiply by RZ(cth, sth).
 *
 * RZ = | cth -sth  0  0 |
 *      | sth  cth  0  0 |
 *      |   0    0  1  0 |
 *      |   0    0  0  1 |
 *
 * Accept precomputed (cos θ, sin θ) so callers avoid redundant trig.
 * Verify ||(cth, sth)|| ≈ 1 in debug builds; no check here for speed.
 */
void matrix_rotateZ(Matrix *m, double cth, double sth) {
    if (!m) return;
    Matrix R;
    matrix_identity(&R);
    R.m[0][0] =  cth;  R.m[0][1] = -sth;
    R.m[1][0] =  sth;  R.m[1][1] =  cth;
    matrix_multiply(&R, m, m);  /* m ← RZ · m */
}

/*
 * matrix_translate2D — premultiply by T(tx, ty, 0).
 *
 * T = | 1  0  0  tx |
 *     | 0  1  0  ty |
 *     | 0  0  1   0 |
 *     | 0  0  0   1 |
 */
void matrix_translate2D(Matrix *m, double tx, double ty) {
    if (!m) return;
    Matrix T;
    matrix_identity(&T);
    T.m[0][3] = tx;  /* translation in x */
    T.m[1][3] = ty;  /* translation in y */
    matrix_multiply(&T, m, m);  /* m ← T · m */
}

/*
 * matrix_shear2D — premultiply by Sh(shx, shy).
 *
 * Sh = | 1   shx  0  0 |
 *      | shy   1  0  0 |
 *      | 0     0  1  0 |
 *      | 0     0  0  1 |
 *
 * shx shifts x by shx·y; shy shifts y by shy·x.
 */
void matrix_shear2D(Matrix *m, double shx, double shy) {
    if (!m) return;
    Matrix Sh;
    matrix_identity(&Sh);
    Sh.m[0][1] = shx;  /* x component sheared by y */
    Sh.m[1][0] = shy;  /* y component sheared by x */
    matrix_multiply(&Sh, m, m);  /* m ← Sh · m */
}


/* =======================================================================
 * 5. 3D transformation helpers (all premultiply: m ← T · m)
 * ======================================================================= */

/*
 * matrix_translate — premultiply by T(tx, ty, tz).
 *
 * T = | 1  0  0  tx |
 *     | 0  1  0  ty |
 *     | 0  0  1  tz |
 *     | 0  0  0   1 |
 */
void matrix_translate(Matrix *m, double tx, double ty, double tz) {
    if (!m) return;
    Matrix T;
    matrix_identity(&T);
    T.m[0][3] = tx;  /* translation in x */
    T.m[1][3] = ty;  /* translation in y */
    T.m[2][3] = tz;  /* translation in z */
    matrix_multiply(&T, m, m);  /* m ← T · m */
}

/*
 * matrix_scale — premultiply by S(sx, sy, sz).
 *
 * S = | sx  0   0  0 |
 *     |  0 sy   0  0 |
 *     |  0  0  sz  0 |
 *     |  0  0   0  1 |
 */
void matrix_scale(Matrix *m, double sx, double sy, double sz) {
    if (!m) return;
    Matrix S;
    matrix_identity(&S);
    S.m[0][0] = sx;  /* scale x */
    S.m[1][1] = sy;  /* scale y */
    S.m[2][2] = sz;  /* scale z */
    matrix_multiply(&S, m, m);  /* m ← S · m */
}

/*
 * matrix_rotateX — premultiply by RX(cth, sth).
 *
 * RX = | 1    0     0    0 |
 *      | 0  cth  -sth    0 |
 *      | 0  sth   cth    0 |
 *      | 0    0     0    1 |
 */
void matrix_rotateX(Matrix *m, double cth, double sth) {
    if (!m) return;
    Matrix R;
    matrix_identity(&R);
    R.m[1][1] =  cth;  R.m[1][2] = -sth;
    R.m[2][1] =  sth;  R.m[2][2] =  cth;
    matrix_multiply(&R, m, m);  /* m ← RX · m */
}

/*
 * matrix_rotateY — premultiply by RY(cth, sth).
 *
 * RY = |  cth  0  sth  0 |
 *      |    0  1    0  0 |
 *      | -sth  0  cth  0 |
 *      |    0  0    0  1 |
 *
 * Note the sign convention: the positive rotation direction for Y-axis
 * rotation follows the right-hand rule (positive θ rotates +X toward −Z).
 */
void matrix_rotateY(Matrix *m, double cth, double sth) {
    if (!m) return;
    Matrix R;
    matrix_identity(&R);
    R.m[0][0] =  cth;  R.m[0][2] =  sth;
    R.m[2][0] = -sth;  R.m[2][2] =  cth;
    matrix_multiply(&R, m, m);  /* m ← RY · m */
}

/*
 * matrix_rotateXYZ — premultiply by the orthonormal basis rotation RXYZ.
 *
 * RXYZ = | ux  uy  uz  0 |
 *        | vx  vy  vz  0 |
 *        | wx  wy  wz  0 |
 *        |  0   0   0  1 |
 *
 * This matrix changes basis: it maps a world-space vector written in the
 * standard (i,j,k) basis to the same vector expressed in the (u,v,w)
 * basis.  It is the transpose of the matrix that has u, v, w as columns.
 *
 * In the 3D view pipeline, u/v/w are the camera's right/up/forward axes
 * (derived from VPN and VUP), and this step aligns world geometry with
 * the camera frame.
 *
 * The original used a loop with ternary conditionals that was hard to
 * verify against the spec matrix.  Explicit assignments make each element
 * traceable directly to eq. 16 in spec §7.3.
 */
void matrix_rotateXYZ(Matrix *m, Vector *u, Vector *v, Vector *w) {
    if (!m || !u || !v || !w) return;

    Matrix R;
    matrix_identity(&R);

    /*
     * Row 0 = u (right axis): maps world coordinates to camera's x-axis.
     * Row 1 = v (up axis):    maps world coordinates to camera's y-axis.
     * Row 2 = w (look axis):  maps world coordinates to camera's z-axis.
     */
    R.m[0][0] = u->val[0];  R.m[0][1] = u->val[1];  R.m[0][2] = u->val[2];
    R.m[1][0] = v->val[0];  R.m[1][1] = v->val[1];  R.m[1][2] = v->val[2];
    R.m[2][0] = w->val[0];  R.m[2][1] = w->val[1];  R.m[2][2] = w->val[2];
    /* Row 3 and column 3 are already set correctly by matrix_identity */

    matrix_multiply(&R, m, m);  /* m ← RXYZ · m */
}

/*
 * matrix_shearZ — premultiply by ShZ(shx, shy).
 *
 * ShZ = | 1  0  shx  0 |
 *       | 0  1  shy  0 |
 *       | 0  0    1  0 |
 *       | 0  0    0  1 |
 *
 * Shears x and y proportional to z.  Used for oblique projections where
 * depth creates a lateral offset in the image plane.
 */
void matrix_shearZ(Matrix *m, double shx, double shy) {
    if (!m) return;
    Matrix Sh;
    matrix_identity(&Sh);
    Sh.m[0][2] = shx;  /* x is offset by shx per unit of z */
    Sh.m[1][2] = shy;  /* y is offset by shy per unit of z */
    matrix_multiply(&Sh, m, m);  /* m ← ShZ · m */
}

/*
 * matrix_perspective — premultiply by the perspective projection Persp(d).
 *
 * Persp = | 1  0    0   0 |
 *         | 0  1    0   0 |
 *         | 0  0    1   0 |
 *         | 0  0  1/d   0 |
 *
 * Sets the homogeneous w row to z/d so that subsequent perspective
 * division (x/w, y/w after transform) produces projected image coords.
 *
 * Bug fix: the original did not guard against d == 0.  Now an error is
 * printed and the function returns without modifying m.
 */
void matrix_perspective(Matrix *m, double d) {
    if (!m) return;
    if (d == 0.0) {
        fprintf(stderr,
            "[matrix_perspective] d must not be 0 — matrix unchanged\n");
        return;
    }
    Matrix P;
    matrix_identity(&P);
    P.m[3][2] = 1.0 / d;  /* w ← z/d → enables perspective divide */
    P.m[3][3] = 0.0;       /* clear the (3,3) element — not 1 after projection */
    matrix_multiply(&P, m, m);  /* m ← Persp · m */
}
