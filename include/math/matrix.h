
#ifndef MATRIX_H
#define MATRIX_H

#include "polygon.h"   /* Polygon (for matrix_xformPolygon) */
#include "primitive.h" /* Point, Vector, Line, Polyline     */
#include <stdio.h>     /* FILE                              */


/* =======================================================================
 * 1. Matrix structure
 * ======================================================================= */

/**
 * Matrix — a 4×4 matrix of doubles for homogeneous coordinate transforms.
 *
 * Storage: m[row][col], row-major.
 * The identity matrix has 1.0 on the main diagonal and 0.0 elsewhere.
 */
typedef struct {
    double m[4][4]; /* m[row][col], indices each in [0, 3] */
} Matrix;


/* =======================================================================
 * 2. Vector functions
 *
 * Vector is typedef'd to Point in primitive.h.  The mathematical difference:
 *   Point  → h = 1.0  (a location; undergoes translation)
 *   Vector → h = 0.0  (a direction; translation has no effect)
 *
 * Spec §7.1 specifies all functions below.
 * ======================================================================= */

/**
 * vector_set — initialise a Vector to (x, y, z, 0.0).
 *
 * The homogeneous component is always set to 0.0 so that the vector is
 * immune to translation when multiplied by an affine matrix.
 *
 * @param v  destination Vector (must not be NULL)
 * @param x  x component
 * @param y  y component
 * @param z  z component
 */
void vector_set(Vector *v, double x, double y, double z);

/**
 * vector_print — write "Vector: (x, y, z, h)" to stream fp.
 *
 * Prints all four components so that the homogeneous coordinate is
 * visible during debugging.
 *
 * @param v   Vector to print (NULL → no-op)
 * @param fp  destination FILE stream (e.g. stdout or stderr)
 */
void vector_print(Vector *v, FILE *fp);

/**
 * vector_copy — copy all four components of *src into *dest.
 *
 * No-op if dest == src (self-copy guard) or either pointer is NULL.
 *
 * @param dest  destination Vector
 * @param src   source      Vector
 */
void vector_copy(Vector *dest, Vector *src);

/**
 * vector_length — return the Euclidean length of the xyz part of v.
 *
 * L = sqrt(vx² + vy² + vz²)
 *
 * The homogeneous component (h = 0 for a proper Vector) is excluded.
 * Returns 0.0 for a NULL pointer.
 *
 * @param v  Vector whose length is computed
 * @return   non-negative length
 */
double vector_length(Vector *v);

/**
 * vector_normalize — scale v so that its Euclidean xyz length is 1.0.
 *
 * The homogeneous component (val[3]) is NOT modified; it remains 0.0
 * as required for a direction vector.  No-op if the length is 0.
 *
 * @param v  Vector to normalise in place
 */
void vector_normalize(Vector *v);

/**
 * vector_dot — return the scalar dot product of vectors a and b.
 *
 * d = ax·bx + ay·by + az·bz
 *
 * Only the xyz components are used; the homogeneous components are ignored.
 * Returns 0.0 if either pointer is NULL.
 *
 * @param a  first  Vector
 * @param b  second Vector
 * @return   dot product (a scalar)
 */
double vector_dot(Vector *a, Vector *b);

/**
 * vector_cross — compute the cross product c = a × b.
 *
 * cx = ay·bz − az·by
 * cy = az·bx − ax·bz
 * cz = ax·by − ay·bx
 * ch = 0.0  (result is a direction vector)
 *
 * c must not alias a or b; the function reads all of a and b before
 * writing c.
 *
 * @param a  first  Vector (left  operand)
 * @param b  second Vector (right operand)
 * @param c  output Vector (must not alias a or b)
 */
void vector_cross(Vector *a, Vector *b, Vector *c);

/**
 * vector_reflect — compute the reflection of incident vector d about
 * surface normal n, storing the result in r.
 *
 * r = d − 2(d·n)n   (n assumed to be unit length)
 *
 * Used in specular lighting calculations: d is the light direction
 * (pointing toward the surface) and r is the reflected direction.
 *
 * @param d  incident direction Vector (pointing toward surface)
 * @param n  unit surface normal Vector
 * @param r  output reflected direction Vector
 */
void vector_reflect(Vector *d, Vector *n, Vector *r);

/**
 * vector_scale — multiply each xyz component of v by scalar s in place.
 *
 * The homogeneous component is not modified.  Useful for attenuating a
 * light contribution or scaling a direction before normalising.
 *
 * @param v  Vector to scale in place
 * @param s  scalar multiplier
 */
void vector_scale(Vector *v, double s);

/**
 * vector_add — component-wise addition: dst = a + b.
 *
 * All four components including h are added.  For proper Vectors h = 0
 * so the result's h will also be 0.
 *
 * @param a    first  Vector
 * @param b    second Vector
 * @param dst  output Vector (may alias a or b; temporaries used internally)
 */
void vector_add(Vector *a, Vector *b, Vector *dst);

/**
 * vector_subtract — component-wise subtraction: dst = a − b.
 *
 * @param a    minuend    Vector
 * @param b    subtrahend Vector
 * @param dst  output Vector (may alias a or b)
 */
void vector_subtract(Vector *a, Vector *b, Vector *dst);

/**
 * vector_equal — return 1 if all four components of a and b are equal.
 *
 * Uses exact double equality; suitable for boundary checks and constant
 * comparisons, not for comparing results of floating-point arithmetic.
 *
 * @param a  first  Vector (NULL → returns 0)
 * @param b  second Vector (NULL → returns 0)
 * @return   1 if equal, 0 otherwise
 */
int vector_equal(const Vector *a, const Vector *b);


/* =======================================================================
 * 3. Generic matrix functions   (spec §7.2)
 * ======================================================================= */

/**
 * matrix_print — write the matrix in a formatted 4×4 table to fp.
 *
 * Each row is printed as "|  v00  v01  v02  v03  |".
 * A separator line follows the last row.
 *
 * @param m   Matrix to print (NULL → no-op)
 * @param fp  destination FILE stream
 */
void matrix_print(Matrix *m, FILE *fp);

/**
 * matrix_clear — set every element to 0.0.
 *
 * @param m  Matrix to zero (NULL → no-op)
 */
void matrix_clear(Matrix *m);

/**
 * matrix_identity — set m to the 4×4 identity matrix.
 *
 * I = diag(1, 1, 1, 1)
 *
 * @param m  Matrix to set (NULL → no-op)
 */
void matrix_identity(Matrix *m);

/**
 * matrix_get — return the element at row r, column c.
 *
 * Returns 0.0 for NULL m or out-of-range indices.
 *
 * @param m  source Matrix
 * @param r  row    index in [0, 3]
 * @param c  column index in [0, 3]
 * @return   m[r][c], or 0.0 on error
 */
double matrix_get(Matrix *m, int r, int c);

/**
 * matrix_set — write value into element at row r, column c.
 *
 * No-op for NULL m or out-of-range indices.
 *
 * @param m      target Matrix
 * @param r      row    index in [0, 3]
 * @param c      column index in [0, 3]
 * @param value  value to store
 */
void matrix_set(Matrix *m, int r, int c, double value);

/**
 * matrix_copy — copy all 16 elements of *src into *dest.
 *
 * No-op if dest == src or either is NULL.
 *
 * @param dest  destination Matrix
 * @param src   source      Matrix
 */
void matrix_copy(Matrix *dest, Matrix *src);

/**
 * matrix_transpose — transpose m in place (reflect across the diagonal).
 *
 * m[r][c] ↔ m[c][r]  for all r < c.
 *
 * @param m  Matrix to transpose in place (NULL → no-op)
 */
void matrix_transpose(Matrix *m);

/**
 * matrix_multiply — store left × right in m.
 *
 * [m] = [left] × [right]
 *
 * A local temporary is used so m may safely alias left or right.
 *
 * @param left   left  operand (4×4)
 * @param right  right operand (4×4)
 * @param m      output matrix (may alias left or right)
 */
void matrix_multiply(Matrix *left, Matrix *right, Matrix *m);

/**
 * matrix_equal — return 1 if all 16 elements of a and b are equal.
 *
 * Uses exact double equality.
 *
 * @param a  first  Matrix (NULL → returns 0)
 * @param b  second Matrix (NULL → returns 0)
 * @return   1 if equal, 0 otherwise
 */
int matrix_equal(const Matrix *a, const Matrix *b);

/**
 * matrix_isIdentity — return 1 if m is the identity matrix (within a
 * small floating-point tolerance MATRIX_EPS).
 *
 * @param m  Matrix to test (NULL → returns 0)
 * @return   1 if identity, 0 otherwise
 */
int matrix_isIdentity(const Matrix *m);


/* =======================================================================
 * Geometry transform functions   (spec §7.2)
 * ======================================================================= */

/**
 * matrix_xformPoint — transform Point p by matrix m, store result in q.
 *
 * q = M · p    (column-vector convention)
 *
 * p and q MUST be different variables (spec §7.2).
 *
 * @param m  transformation Matrix
 * @param p  input  Point
 * @param q  output Point (must not alias p)
 */
void matrix_xformPoint(Matrix *m, Point *p, Point *q);

/**
 * matrix_xformVector — transform Vector v by matrix m, store result in w.
 *
 * w = M · v
 *
 * v and w MUST be different variables (spec §7.2).
 * For a proper Vector (h = 0) the translation component of m has no effect.
 *
 * @param m  transformation Matrix
 * @param v  input  Vector
 * @param w  output Vector (must not alias v)
 */
void matrix_xformVector(Matrix *m, Vector *v, Vector *w);

/**
 * matrix_xformPolygon — transform all vertices and normals of Polygon p
 * by matrix m.
 *
 * Vertices are transformed as Points (matrix_xformPoint).
 * Normals are transformed as Vectors (matrix_xformVector), preserving
 * h = 0 so normals are not translated.  If p->normal is NULL the normal
 * transform step is skipped.
 *
 * @param m  transformation Matrix
 * @param p  Polygon to transform in place
 */
void matrix_xformPolygon(Matrix *m, Polygon *p);

/**
 * matrix_xformPolyline — transform all vertices of Polyline p by m.
 *
 * @param m  transformation Matrix
 * @param p  Polyline to transform in place
 */
void matrix_xformPolyline(Matrix *m, Polyline *p);

/**
 * matrix_xformLine — transform both endpoints of Line l by m.
 *
 * @param m  transformation Matrix
 * @param l  Line to transform in place
 */
void matrix_xformLine(Matrix *m, Line *l);


/* =======================================================================
 * 4. 2D transformation helpers   (spec §7.2)
 *
 * All helpers PREMULTIPLY:  m ← T · m
 * ======================================================================= */

/**
 * matrix_scale2D — premultiply m by a 2D scale matrix S(sx, sy).
 *
 * S = | sx  0  0  0 |
 *     |  0 sy  0  0 |
 *     |  0  0  1  0 |
 *     |  0  0  0  1 |
 *
 * @param m   Matrix to premultiply
 * @param sx  x scale factor
 * @param sy  y scale factor
 */
void matrix_scale2D(Matrix *m, double sx, double sy);

/**
 * matrix_rotateZ — premultiply m by a Z-axis rotation matrix.
 *
 * R = | cth -sth  0  0 |
 *     | sth  cth  0  0 |
 *     |   0    0  1  0 |
 *     |   0    0  0  1 |
 *
 * Pass cth = cos(θ) and sth = sin(θ).  Accepting precomputed values lets
 * the caller decide how to compute the angle (e.g., from a normalised
 * direction vector) without paying for repeated trig evaluation.
 *
 * @param m    Matrix to premultiply
 * @param cth  cos(θ), rotation angle about Z
 * @param sth  sin(θ), rotation angle about Z
 */
void matrix_rotateZ(Matrix *m, double cth, double sth);

/**
 * matrix_translate2D — premultiply m by a 2D translation matrix T(tx, ty).
 *
 * T = | 1  0  0  tx |
 *     | 0  1  0  ty |
 *     | 0  0  1   0 |
 *     | 0  0  0   1 |
 *
 * @param m   Matrix to premultiply
 * @param tx  translation in x
 * @param ty  translation in y
 */
void matrix_translate2D(Matrix *m, double tx, double ty);

/**
 * matrix_shear2D — premultiply m by a 2D shear matrix Sh(shx, shy).
 *
 * Sh = | 1   shx  0  0 |
 *      | shy   1  0  0 |
 *      | 0     0  1  0 |
 *      | 0     0  0  1 |
 *
 * @param m    Matrix to premultiply
 * @param shx  shear in x direction (shifts x proportional to y)
 * @param shy  shear in y direction (shifts y proportional to x)
 */
void matrix_shear2D(Matrix *m, double shx, double shy);


/* =======================================================================
 * 5. 3D transformation helpers   (spec §7.3)
 *
 * All helpers PREMULTIPLY:  m ← T · m
 * ======================================================================= */

/**
 * matrix_translate — premultiply m by a 3D translation T(tx, ty, tz).
 *
 * T = | 1  0  0  tx |
 *     | 0  1  0  ty |
 *     | 0  0  1  tz |
 *     | 0  0  0   1 |
 *
 * @param m   Matrix to premultiply
 * @param tx  translation in x
 * @param ty  translation in y
 * @param tz  translation in z
 */
void matrix_translate(Matrix *m, double tx, double ty, double tz);

/**
 * matrix_scale — premultiply m by a 3D scale matrix S(sx, sy, sz).
 *
 * S = | sx  0   0  0 |
 *     |  0 sy   0  0 |
 *     |  0  0  sz  0 |
 *     |  0  0   0  1 |
 *
 * @param m   Matrix to premultiply
 * @param sx  x scale factor
 * @param sy  y scale factor
 * @param sz  z scale factor
 */
void matrix_scale(Matrix *m, double sx, double sy, double sz);

/**
 * matrix_rotateX — premultiply m by an X-axis rotation RX(cth, sth).
 *
 * RX = | 1    0     0    0 |
 *      | 0  cth  -sth    0 |
 *      | 0  sth   cth    0 |
 *      | 0    0     0    1 |
 *
 * @param m    Matrix to premultiply
 * @param cth  cos(θ), rotation angle about X
 * @param sth  sin(θ), rotation angle about X
 */
void matrix_rotateX(Matrix *m, double cth, double sth);

/**
 * matrix_rotateY — premultiply m by a Y-axis rotation RY(cth, sth).
 *
 * RY = |  cth  0  sth  0 |
 *      |    0  1    0  0 |
 *      | -sth  0  cth  0 |
 *      |    0  0    0  1 |
 *
 * @param m    Matrix to premultiply
 * @param cth  cos(θ), rotation angle about Y
 * @param sth  sin(θ), rotation angle about Y
 */
void matrix_rotateY(Matrix *m, double cth, double sth);

/**
 * matrix_rotateXYZ — premultiply m by a basis-change rotation matrix
 * whose rows are the orthonormal basis vectors u, v, w.
 *
 * RXYZ = | ux  uy  uz  0 |
 *        | vx  vy  vz  0 |
 *        | wx  wy  wz  0 |
 *        |  0   0   0  1 |
 *
 * u, v, w must be mutually orthogonal unit vectors.  This matrix rotates
 * world coordinates into the view (UVN) coordinate frame and is the core
 * step of both the 2D and 3D view transforms.
 *
 * @param m  Matrix to premultiply
 * @param u  first  basis vector (maps world x to camera x)
 * @param v  second basis vector (maps world y to camera y)
 * @param w  third  basis vector (maps world z to camera z / VPN direction)
 */
void matrix_rotateXYZ(Matrix *m, Vector *u, Vector *v, Vector *w);

/**
 * matrix_shearZ — premultiply m by a Z-shear matrix ShZ(shx, shy).
 *
 * ShZ = | 1  0  shx  0 |
 *       | 0  1  shy  0 |
 *       | 0  0    1  0 |
 *       | 0  0    0  1 |
 *
 * Shears x and y proportional to z; used for oblique projections.
 *
 * @param m    Matrix to premultiply
 * @param shx  x-shear factor (shift in x per unit of z)
 * @param shy  y-shear factor (shift in y per unit of z)
 */
void matrix_shearZ(Matrix *m, double shx, double shy);

/**
 * matrix_perspective — premultiply m by a perspective matrix Persp(d).
 *
 * Persp = | 1  0    0   0 |
 *         | 0  1    0   0 |
 *         | 0  0    1   0 |
 *         | 0  0  1/d   0 |
 *
 * Sets the homogeneous w component to z/d so that subsequent
 * perspective division (x/w, y/w) produces the projected coordinates.
 * d is the distance from the centre of projection to the view plane
 * (the focal length).
 *
 * @param m  Matrix to premultiply
 * @param d  projection distance (must not be 0)
 */
void matrix_perspective(Matrix *m, double d);

#endif /* MATRIX_H */
