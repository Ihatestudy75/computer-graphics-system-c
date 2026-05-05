#include "primitive.h"
#include "raster.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>   /* memcpy */

/* -----------------------------------------------------------------------
 * Internal draw helper — write a pixel at (row, col) respecting the
 * Line's z-buffer flag and performing the depth test via image_depthTest.
 *
 * Using this helper avoids duplicating the depth-test logic inside every
 * loop.  All draw functions that emit individual pixels should use it.
 *
 * @param img    target Image
 * @param row    pixel row
 * @param col    pixel column
 * @param c      colour to write
 * @param z      1/z value for this pixel
 * @param usez   non-zero = perform depth test; 0 = write unconditionally
 * ----------------------------------------------------------------------- */
static inline void draw_pixel(Image *img, int row, int col,
                               Color c, float z, int usez) {
    if (usez) {
        /* Only write if the new depth passes the image's depth test */
        if (!image_depthTest(img, row, col, z)) return;
        image_setz(img, row, col, z);
    }
    image_setColor(img, row, col, c);
}


/* =======================================================================
 * Point functions
 * ======================================================================= */

/*
 * point_set2D — set x, y and fill z=0, h=1 for a standard 2D point.
 *
 * z is zeroed because 2D primitives are drawn at the image plane; h=1
 * keeps the point in Cartesian (non-projective) form.
 */
void point_set2D(Point *p, double x, double y) {
    if (!p) return;
    p->val[0] = x;    /* x coordinate → screen column */
    p->val[1] = y;    /* y coordinate → screen row    */
    p->val[2] = 0.0;  /* z = 0 for 2D points          */
    p->val[3] = 1.0;  /* h = 1 → Cartesian point      */
}

/*
 * point_set3D — set x, y, z and set the homogeneous coordinate to 1.
 *
 * Used when depth information matters (e.g., 3D line drawing with z-buffer).
 */
void point_set3D(Point *p, double x, double y, double z) {
    if (!p) return;
    p->val[0] = x;
    p->val[1] = y;
    p->val[2] = z;
    p->val[3] = 1.0;  /* standard Cartesian point */
}

/*
 * point_set — set all four components of a homogeneous point explicitly.
 *
 * Call this when constructing a fully general projective point, e.g. after
 * a perspective projection where h may differ from 1.
 */
void point_set(Point *p, double x, double y, double z, double h) {
    if (!p) return;
    p->val[0] = x;
    p->val[1] = y;
    p->val[2] = z;
    p->val[3] = h;
}

/*
 * point_normalize — divide x and y by the homogeneous coordinate h so
 * that h becomes 1.0 (Cartesian normalisation).
 *
 * The guard h > 0 prevents division by zero for points at infinity and
 * avoids flipping points that project behind the eye (h < 0).
 *
 * Only x and y are divided; z is left as-is so that the depth value
 * stored in the point is not corrupted.
 */
void point_normalize(Point *p) {
    if (!p) return;
    if (p->val[3] > 0.0) {
        p->val[0] /= p->val[3];   /* x = x / h */
        p->val[1] /= p->val[3];   /* y = y / h */
        p->val[3] = 1.0;           /* h = 1     */
    }
    /* if h <= 0, the point is behind the eye; leave it unchanged */
}

/*
 * point_copy — copy all four val[] components from *from to *to.
 *
 * A struct assignment (*to = *from) would do the same, but this function
 * provides a named, NULL-safe interface consistent with the rest of the API.
 */
void point_copy(Point *to, Point *from) {
    if (!to || !from) return;
    to->val[0] = from->val[0];
    to->val[1] = from->val[1];
    to->val[2] = from->val[2];
    to->val[3] = from->val[3];
}

/*
 * point_distance2D — Euclidean distance between two Points in the XY plane.
 *
 * Z is ignored; this is a screen-space distance measure.
 */
double point_distance2D(const Point *a, const Point *b) {
    if (!a || !b) return 0.0;
    double dx = b->val[0] - a->val[0];
    double dy = b->val[1] - a->val[1];
    return sqrt(dx * dx + dy * dy);
}

/*
 * point_equal — exact equality test on all four components.
 *
 * Returns 1 if a and b are identical, 0 otherwise (including NULL inputs).
 */
int point_equal(const Point *a, const Point *b) {
    if (!a || !b) return 0;
    return (a->val[0] == b->val[0]) &&
           (a->val[1] == b->val[1]) &&
           (a->val[2] == b->val[2]) &&
           (a->val[3] == b->val[3]);
}

/*
 * point_draw — rasterise a Point into an Image at the pixel closest to
 * (val[0], val[1]) using the given Color.
 *
 * The point is normalised first.  Coordinates are rounded to the nearest
 * integer pixel centre (floor(x + 0.5)).  Out-of-bounds pixels are
 * silently ignored by image_setColor.
 *
 * The z value (val[2]) is written to the depth buffer via image_setz so
 * that points participate in depth sorting correctly.
 */
void point_draw(Point *p, Image *img, Color c) {
    if (!p || !img) return;

    /* Normalise to Cartesian coordinates before converting to pixels */
    point_normalize(p);

    int col = (int)floor(p->val[0] + 0.5);  /* x → column, nearest pixel */
    int row = (int)floor(p->val[1] + 0.5);  /* y → row,    nearest pixel */
    float z = (float)p->val[2];

    image_setColor(img, row, col, c);
    image_setz(img, row, col, z);
}

/*
 * point_drawf — rasterise a Point using an FPixel value.
 *
 * Writes the full FPixel (RGB + z) directly to the image's data array.
 * Performs explicit bounds checking because this is a lower-level function
 * that bypasses the image accessor API.
 */
void point_drawf(Point *p, Image *img, FPixel fp) {
    if (!p || !img || !img->data) return;

    /* Use truncation (integer cast) for sub-pixel alignment */
    int col = (int)p->val[0];
    int row = (int)p->val[1];

    if (row < 0 || row >= img->rows || col < 0 || col >= img->cols) return;

    /* Write directly to the flat pixel array */
    size_t idx = (size_t)row * (size_t)img->cols + (size_t)col;
    img->data[idx].rgb[0] = fp.rgb[0];
    img->data[idx].rgb[1] = fp.rgb[1];
    img->data[idx].rgb[2] = fp.rgb[2];
    img->data[idx].z      = fp.z;
}

/*
 * point_print — write "(x, y, z, h)" to the FILE stream fp.
 *
 * fp may be stdout, stderr, or a file opened with fopen.  The function
 * uses fprintf so it works with any FILE destination.
 */
void point_print(Point *p, FILE *fp) {
    if (!p || !fp) return;
    fprintf(fp, "Point: (%.4f, %.4f, %.4f, %.4f)\n",
            p->val[0], p->val[1], p->val[2], p->val[3]);
}

/*
 * point_rotate — rotate a Point around the image-coordinate origin by
 * angle radians, then draw it.
 *
 * 2D rotation matrix applied to (val[0], val[1]):
 *   x' = x·cos(angle) − y·sin(angle)
 *   y' = x·sin(angle) + y·cos(angle)
 *
 * The point is modified in place before being passed to point_draw.
 * Note: this rotates around (0,0), not around the image centre.  If you
 * want to rotate around a different pivot, translate first.
 */
void point_rotate(Point *p, double angle, Color c, Image *img) {
    if (!p || !img) return;

    double cosA = cos(angle);
    double sinA = sin(angle);

    /* Compute rotated coordinates into temporaries to avoid aliasing */
    double xr = p->val[0] * cosA - p->val[1] * sinA;
    double yr = p->val[0] * sinA + p->val[1] * cosA;

    p->val[0] = xr;
    p->val[1] = yr;

    point_draw(p, img, c);  /* draw the rotated point */
}


/* =======================================================================
 * Line functions
 * ======================================================================= */

/*
 * line_set2D — initialise a 2D Line from four scalar coordinates.
 *
 * Both endpoints are set as 2D points (z=0, h=1).  zBuffer defaults to
 * 1 (enabled) as specified.
 */
void line_set2D(Line *l, double x0, double y0, double x1, double y1) {
    if (!l) return;
    point_set2D(&l->from, x0, y0);
    point_set2D(&l->to,   x1, y1);
    l->zBuffer = 1;  /* default: z-buffering on */
}

/*
 * line_set — initialise a Line from two Point structs.
 *
 * Both points are deep-copied.  zBuffer defaults to 1.
 */
void line_set(Line *l, Point from, Point to) {
    if (!l) return;
    point_copy(&l->from, &from);
    point_copy(&l->to,   &to);
    l->zBuffer = 1;  /* default: z-buffering on */
}

/*
 * line_setz — set the z-buffer enable flag.
 *
 * This is the primary setter; line_zBuffer delegates here.
 */
void line_setz(Line *l, int zflag) {
    if (l) l->zBuffer = zflag;
}

/*
 * line_zBuffer — API-compatible alias for line_setz.
 *
 * Both names are kept to satisfy the spec requirement while avoiding
 * duplication of the implementation.
 */
void line_zBuffer(Line *l, int zflag) {
    line_setz(l, zflag);
}

/*
 * line_normalize — normalise the from and to endpoints by their h values.
 *
 * Delegates to point_normalize for each endpoint independently.
 */
void line_normalize(Line *l) {
    if (!l) return;
    point_normalize(&l->from);
    point_normalize(&l->to);
}

/*
 * line_copy — deep-copy all fields from *from into *to.
 *
 * Copies zBuffer and both endpoint Points.
 */
void line_copy(Line *to, Line *from) {
    if (!to || !from) return;
    point_copy(&to->from, &from->from);
    point_copy(&to->to,   &from->to);
    to->zBuffer = from->zBuffer;
}

/*
 * line_length2D — return the pixel-space length of the line segment.
 *
 * Delegates to point_distance2D on the two endpoints.
 */
double line_length2D(const Line *l) {
    if (!l) return 0.0;
    return point_distance2D(&l->from, &l->to);
}

/*
 * line_print — write a human-readable description to fp.
 */
void line_print(const Line *l, FILE *fp) {
    if (!l || !fp) return;
    fprintf(fp, "Line: zBuffer=%d\n", l->zBuffer);
    fprintf(fp, "  from: (%.4f, %.4f, %.4f, %.4f)\n",
            l->from.val[0], l->from.val[1],
            l->from.val[2], l->from.val[3]);
    fprintf(fp, "  to:   (%.4f, %.4f, %.4f, %.4f)\n",
            l->to.val[0], l->to.val[1],
            l->to.val[2], l->to.val[3]);
}

/*
 * line_draw — draw a line segment using Bresenham's algorithm.
 *
 * Algorithm outline:
 *   1. Copy the line so the original is not modified.
 *   2. Liang-Barsky clip to the image rectangle; discard if outside.
 *   3. Convert clipped endpoints to integer pixel coordinates.
 *   4. Compute dz (depth increment per dominant-axis step) for linear
 *      z-interpolation.
 *   5. Bresenham loop: draw each pixel, optionally gated by depth test.
 *
 * Bug fixes vs. original:
 *   • Removed `curZ >= 1.0f` from the depth-test condition; valid 1/z
 *     values can be less than 1 (e.g., for distant objects).
 *   • Depth test now uses image_depthTest() which respects the Image's
 *     DepthType setting rather than hard-coding DEPTH_GREATER.
 */
void line_draw(Line *l, Image *img, Color c) {
    if (!l || !img) return;

    /* Work on a clipped copy so the caller's Line is unchanged */
    Line seg;
    line_copy(&seg, l);
    if (!raster_liang_barsky_clip(&seg, img)) return;  /* fully outside */

    /* Integer pixel coordinates for the Bresenham loop */
    int x0 = (int)(seg.from.val[0]);
    int y0 = (int)(seg.from.val[1]);
    int x1 = (int)(seg.to.val[0]);
    int y1 = (int)(seg.to.val[1]);

    /* Starting and ending 1/z depths for linear interpolation */
    float z0 = (float)seg.from.val[2];
    float z1 = (float)seg.to.val[2];

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;   /* x step direction */
    int sy = (y0 < y1) ? 1 : -1;   /* y step direction */
    int err = dx - dy;              /* Bresenham error term */

    /* dz: depth increment per step along the dominant axis */
    float dz   = 0.0f;
    float curZ = z0;
    if (dx >= dy) {
        if (dx > 0) dz = (z1 - z0) / (float)dx;
    } else {
        if (dy > 0) dz = (z1 - z0) / (float)dy;
    }

    /* Bresenham rasterisation loop */
    for (;;) {
        /* draw_pixel performs the depth test and write atomically */
        draw_pixel(img, y0, x0, c, curZ, seg.zBuffer);

        if (x0 == x1 && y0 == y1) break;  /* reached the endpoint */

        int err2 = err * 2;
        if (err2 > -dy) { err -= dy; x0 += sx; }
        if (err2 <  dx) { err += dx; y0 += sy; }

        curZ += dz;  /* linearly interpolate depth along the segment */
    }
}

/*
 * line_draw_dashed — draw a dashed line using the same Bresenham core as
 * line_draw, but with a repeating dash-gap pattern controlling which pixels
 * are emitted.
 *
 * Pattern: DASH_LEN pixels drawn, GAP_LEN pixels skipped, repeat.
 * The pattern counter advances on every Bresenham step regardless of
 * whether a pixel is drawn, so gap lengths are consistent.
 */
void line_draw_dashed(Line *l, Image *img, Color c) {
    if (!l || !img) return;

    Line seg;
    line_copy(&seg, l);
    if (!raster_liang_barsky_clip(&seg, img)) return;

    int x0 = (int)(seg.from.val[0]);
    int y0 = (int)(seg.from.val[1]);
    int x1 = (int)(seg.to.val[0]);
    int y1 = (int)(seg.to.val[1]);

    float z0 = (float)seg.from.val[2];
    float z1 = (float)seg.to.val[2];

    int dx  = abs(x1 - x0);
    int dy  = abs(y1 - y0);
    int sx  = (x0 < x1) ? 1 : -1;
    int sy  = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    float dz   = 0.0f;
    float curZ = z0;
    if (dx >= dy) { if (dx > 0) dz = (z1 - z0) / (float)dx; }
    else          { if (dy > 0) dz = (z1 - z0) / (float)dy; }

    /* Dash pattern parameters */
    const int DASH_LEN = 5;  /* pixels drawn per dash  */
    const int GAP_LEN  = 3;  /* pixels skipped per gap */
    const int PERIOD   = DASH_LEN + GAP_LEN;
    int dashCount = 0;       /* advances every step    */

    for (;;) {
        /* Draw only during the dash portion of the pattern */
        if (dashCount % PERIOD < DASH_LEN) {
            draw_pixel(img, y0, x0, c, curZ, seg.zBuffer);
        }

        if (x0 == x1 && y0 == y1) break;

        int err2 = err * 2;
        if (err2 > -dy) { err -= dy; x0 += sx; }
        if (err2 <  dx) { err += dx; y0 += sy; }

        curZ += dz;
        dashCount++;
    }
}


/* =======================================================================
 * Circle functions
 * ======================================================================= */

/*
 * circle_set — initialise a Circle from a centre Point and a radius.
 *
 * The centre is deep-copied; r is stored directly.
 */
void circle_set(Circle *c, Point center, double r) {
    if (!c) return;
    point_copy(&c->c, &center);
    c->r = r;
}

/*
 * circle_draw — draw the outline of a circle using the Bresenham midpoint
 * circle algorithm.
 *
 * One octant is computed; 8-way symmetry produces the other seven
 * mirror-image pixels.  This keeps the pixel count O(r) rather than O(r²).
 *
 * The error term initialisation err = 3 − 2r corresponds to the decision
 * variable for the first step of the midpoint circle algorithm.
 */
void circle_draw(Circle *c, Image *img, Color color) {
    if (!c || !img) return;

    int cx = (int)round(c->c.val[0]);  /* centre column */
    int cy = (int)round(c->c.val[1]);  /* centre row    */
    int r  = (int)round(c->r);
    if (r < 0) return;

    int x   = 0;
    int y   = r;
    int err = 3 - 2 * r;  /* initial error term for midpoint algorithm */

    while (x <= y) {
        /* Eight-way symmetry: emit all mirror-image pixels per octant step */
        Point p;
        point_set2D(&p, cx + x, cy + y); point_draw(&p, img, color);
        point_set2D(&p, cx - x, cy + y); point_draw(&p, img, color);
        point_set2D(&p, cx + x, cy - y); point_draw(&p, img, color);
        point_set2D(&p, cx - x, cy - y); point_draw(&p, img, color);
        point_set2D(&p, cx + y, cy + x); point_draw(&p, img, color);
        point_set2D(&p, cx - y, cy + x); point_draw(&p, img, color);
        point_set2D(&p, cx + y, cy - x); point_draw(&p, img, color);
        point_set2D(&p, cx - y, cy - x); point_draw(&p, img, color);

        /* Update error term: move x right; conditionally move y down */
        if (err < 0) {
            err += 4 * x + 6;
        } else {
            err += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/*
 * circle_drawFill — draw a solid filled circle using horizontal scanlines.
 *
 * The midpoint algorithm is used to find the x-extent of the circle at
 * each y level.  A horizontal span from (cx-x, cy±y) to (cx+x, cy±y) and
 * from (cx-y, cy±x) to (cx+y, cy±x) is drawn, filling the interior.
 *
 * This is more efficient than testing every pixel in the bounding square
 * (O(r) scanline steps vs O(r²) interior test).
 *
 * Bug fix: original implementation was a copy of the dashed variant with
 * an unrelated dashCount; replaced with the correct scanline fill.
 */
void circle_drawFill(Circle *c, Image *img, Color color) {
    if (!c || !img) return;

    int cx = (int)round(c->c.val[0]);
    int cy = (int)round(c->c.val[1]);
    int r  = (int)round(c->r);
    if (r < 0) return;

    int x   = 0;
    int y   = r;
    int err = 3 - 2 * r;

    while (x <= y) {
        /*
         * Draw horizontal spans at the four y levels covered by this
         * (x, y) pair in the midpoint algorithm.  Using line_set2D +
         * line_draw is clean but slow; directly iterating columns is
         * faster for a scanline fill.
         */
        for (int col = cx - x; col <= cx + x; col++) {
            Point p;
            point_set2D(&p, col, cy + y); point_draw(&p, img, color);
            point_set2D(&p, col, cy - y); point_draw(&p, img, color);
        }
        for (int col = cx - y; col <= cx + y; col++) {
            Point p;
            point_set2D(&p, col, cy + x); point_draw(&p, img, color);
            point_set2D(&p, col, cy - x); point_draw(&p, img, color);
        }

        if (err < 0) {
            err += 4 * x + 6;
        } else {
            err += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/*
 * circle_draw_dashed — draw a dashed circle outline.
 *
 * Uses the same Bresenham midpoint algorithm as circle_draw but gates
 * each group of 8 symmetric pixels behind a dash counter.
 *
 * Bug fix: original was performing a filled bounding-box scan
 * (not a dashed outline at all).  Corrected to draw the outline with
 * a dash-gap pattern.
 */
void circle_draw_dashed(Circle *c, Image *img, Color color) {
    if (!c || !img) return;

    int cx = (int)round(c->c.val[0]);
    int cy = (int)round(c->c.val[1]);
    int r  = (int)round(c->r);
    if (r < 0) return;

    int x   = 0;
    int y   = r;
    int err = 3 - 2 * r;

    const int DASH_LEN = 5;
    const int GAP_LEN  = 3;
    const int PERIOD   = DASH_LEN + GAP_LEN;
    int dashCount = 0;

    while (x <= y) {
        /* Only draw the 8 symmetric pixels during the dash phase */
        if (dashCount % PERIOD < DASH_LEN) {
            Point p;
            point_set2D(&p, cx + x, cy + y); point_draw(&p, img, color);
            point_set2D(&p, cx - x, cy + y); point_draw(&p, img, color);
            point_set2D(&p, cx + x, cy - y); point_draw(&p, img, color);
            point_set2D(&p, cx - x, cy - y); point_draw(&p, img, color);
            point_set2D(&p, cx + y, cy + x); point_draw(&p, img, color);
            point_set2D(&p, cx - y, cy + x); point_draw(&p, img, color);
            point_set2D(&p, cx + y, cy - x); point_draw(&p, img, color);
            point_set2D(&p, cx - y, cy - x); point_draw(&p, img, color);
        }

        if (err < 0) {
            err += 4 * x + 6;
        } else {
            err += 4 * (x - y) + 10;
            y--;
        }
        x++;
        dashCount++;  /* advance the pattern counter every Bresenham step */
    }
}

/*
 * circle_draw_fill_flood — draw the circle outline then flood-fill the
 * interior starting from the centre pixel.
 *
 * The flood fill (4-connected) propagates until it reaches the outline
 * colour.  This method handles non-convex shapes but is slower than the
 * scanline fill for simple circles.
 */
void circle_draw_fill_flood(Circle *c, Image *img, Color color) {
    if (!c || !img) return;

    int cx = (int)round(c->c.val[0]);
    int cy = (int)round(c->c.val[1]);

    /* Read the current interior colour before drawing the outline */
    Color targetColor = image_getColor(img, cy, cx);

    /* Draw the circle boundary */
    circle_draw(c, img, color);

    /* Flood-fill only if the centre is not already the fill colour */
    if (!color_equal(&targetColor, &color)) {
        raster_flood_fill(img, cy, cx, color, targetColor);
    }
}


/* =======================================================================
 * Ellipse functions
 * ======================================================================= */

/*
 * ellipse_set — initialise an Ellipse from centre, radii, and angle.
 *
 * The centre is deep-copied.  ra should be ≥ rb for the major/minor
 * axis convention to be meaningful, but the algorithm works either way.
 */
void ellipse_set(Ellipse *e, Point center, double ra, double rb, double angle) {
    if (!e) return;
    point_copy(&e->c, &center);
    e->ra    = ra;
    e->rb    = rb;
    e->angle = angle;
}

/*
 * ellipse_draw — draw the outline of a (possibly rotated) ellipse.
 *
 * The midpoint ellipse algorithm operates in the un-rotated frame:
 *
 *   Phase 1 (x-dominated, gradient |dy/dx| < 1):
 *     start at (0, rb); advance x while x·b² ≤ y·a².
 *
 *   Phase 2 (y-dominated, gradient |dy/dx| > 1):
 *     continue from where phase 1 left off; advance until y < 0.
 *
 * After each step the four symmetric points (±x, ±y) are rotated by
 * e->angle via point_rotate so that the ellipse can be drawn at any
 * orientation.  All four quadrant reflections are emitted per step.
 */
void ellipse_draw(Ellipse *e, Image *img, Color c) {
    if (!e || !img) return;

    int    cx = (int)round(e->c.val[0]);
    int    cy = (int)round(e->c.val[1]);
    double a2 = e->ra * e->ra;          /* a squared */
    double b2 = e->rb * e->rb;          /* b squared */

    /* ----- Phase 1: x-dominated region (start at top of ellipse) ----- */
    int  x   = 0;
    int  y   = (int)round(e->rb);
    long err = (long)round(b2 - a2 * e->rb + 0.25 * a2);

    while ((double)x * b2 <= (double)y * a2) {
        Point p;
        /* Rotate each of the four symmetric points around the centre */
        point_set2D(&p, cx + x, cy + y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx - x, cy + y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx + x, cy - y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx - x, cy - y); point_rotate(&p, e->angle, c, img);

        /* Update error term for midpoint ellipse algorithm */
        if (err < 0) {
            err += (long)(b2 * (2 * x + 3));
        } else {
            err += (long)(b2 * (2 * x + 3) + a2 * (-2 * y + 2));
            y--;
        }
        x++;
    }

    /* ----- Phase 2: y-dominated region (finish the ellipse) ----- */
    while (y >= 0) {
        Point p;
        point_set2D(&p, cx + x, cy + y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx - x, cy + y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx + x, cy - y); point_rotate(&p, e->angle, c, img);
        point_set2D(&p, cx - x, cy - y); point_rotate(&p, e->angle, c, img);

        if (err > 0) {
            err += (long)(a2 * (3 - 2 * y));
        } else {
            err += (long)(b2 * (2 * x + 3) + a2 * (3 - 2 * y));
        }
        y--;
    }
}

/*
 * ellipse_draw_dashed — dashed ellipse outline using the same midpoint
 * algorithm as ellipse_draw with a dash-gap pattern.
 *
 * Clean-up: removed the commented-out dead code from the original.
 * Both phases now share the same dashCount so the pattern is continuous
 * across the phase boundary.
 */
void ellipse_draw_dashed(Ellipse *e, Image *img, Color c) {
    if (!e || !img) return;

    int    cx = (int)round(e->c.val[0]);
    int    cy = (int)round(e->c.val[1]);
    double a2 = e->ra * e->ra;
    double b2 = e->rb * e->rb;

    int  x   = 0;
    int  y   = (int)round(e->rb);
    long err = (long)round(b2 - a2 * e->rb + 0.25 * a2);

    const int DASH_LEN = 5;
    const int GAP_LEN  = 3;
    const int PERIOD   = DASH_LEN + GAP_LEN;
    int dashCount = 0;

    /* Phase 1 */
    while ((double)x * b2 <= (double)y * a2) {
        if (dashCount % PERIOD < DASH_LEN) {
            Point p;
            point_set2D(&p, cx + x, cy + y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx - x, cy + y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx + x, cy - y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx - x, cy - y); point_rotate(&p, e->angle, c, img);
        }
        dashCount++;

        if (err < 0) {
            err += (long)(b2 * (2 * x + 3));
        } else {
            err += (long)(b2 * (2 * x + 3) + a2 * (-2 * y + 2));
            y--;
        }
        x++;
    }

    /* Phase 2 — dashCount continues from Phase 1 for a seamless pattern */
    while (y >= 0) {
        if (dashCount % PERIOD < DASH_LEN) {
            Point p;
            point_set2D(&p, cx + x, cy + y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx - x, cy + y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx + x, cy - y); point_rotate(&p, e->angle, c, img);
            point_set2D(&p, cx - x, cy - y); point_rotate(&p, e->angle, c, img);
        }
        dashCount++;

        if (err > 0) {
            err += (long)(a2 * (3 - 2 * y));
        } else {
            err += (long)(b2 * (2 * x + 3) + a2 * (3 - 2 * y));
        }
        y--;
    }
}

/*
 * ellipse_draw_fill — filled ellipse by testing each pixel in the bounding
 * square of max(ra, rb) against the rotated ellipse equation.
 *
 * The test rotates the candidate (dx, dy) back into the un-rotated ellipse
 * frame and checks the standard ellipse inequality:
 *   (x'²/a²) + (y'²/b²) ≤ 1
 *
 * This correctly handles arbitrary rotation at the cost of O((max_r)²)
 * iterations.  For axis-aligned ellipses a scanline approach would be
 * faster, but correctness is prioritised here.
 */
void ellipse_draw_fill(Ellipse *e, Image *img, Color c) {
    if (!e || !img) return;

    int    cx   = (int)round(e->c.val[0]);
    int    cy   = (int)round(e->c.val[1]);
    double cosA = cos(e->angle);
    double sinA = sin(e->angle);
    int    maxR = (int)ceil(fmax(e->ra, e->rb));

    /* Check each pixel in the bounding square */
    for (int dy = -maxR; dy <= maxR; dy++) {
        for (int dx = -maxR; dx <= maxR; dx++) {
            /*
             * Rotate (dx, dy) into the un-rotated ellipse coordinate frame.
             * The rotation that maps the world frame to the ellipse frame is
             * the transpose (inverse) of the rotation that maps the ellipse
             * frame to the world frame.
             */
            double xr =  dx * cosA + dy * sinA;  /* rotate back by -angle */
            double yr = -dx * sinA + dy * cosA;

            /* Standard axis-aligned ellipse test */
            if ((xr * xr) / (e->ra * e->ra) +
                (yr * yr) / (e->rb * e->rb) <= 1.0) {
                Point p;
                point_set2D(&p, cx + dx, cy + dy);
                point_draw(&p, img, c);
            }
        }
    }
}

/*
 * ellipse_draw_fill_flood — draw the ellipse outline then flood-fill the
 * interior from the centre.
 *
 * Draws the outline first so the flood fill has a closed boundary to
 * stop against.
 */
void ellipse_draw_fill_flood(Ellipse *e, Image *img, Color c) {
    if (!e || !img) return;

    int cx = (int)round(e->c.val[0]);
    int cy = (int)round(e->c.val[1]);

    /* Read the current colour at the centre before drawing */
    Color targetColor = image_getColor(img, cy, cx);

    ellipse_draw(e, img, c);  /* draw boundary */

    /* Flood-fill only if the centre is not already the fill colour */
    if (!color_equal(&targetColor, &c)) {
        raster_flood_fill(img, cy, cx, c, targetColor);
    }
}


/* =======================================================================
 * Polyline functions
 * ======================================================================= */

/*
 * polyline_create — heap-allocate and return an empty Polyline.
 *
 * numVertex=0, vertex=NULL, zBuffer=1.  Returns NULL if malloc fails.
 */
Polyline *polyline_create(void) {
    Polyline *pl = (Polyline *)malloc(sizeof(Polyline));
    if (!pl) {
        fprintf(stderr, "[polyline_create] malloc failed\n");
        return NULL;
    }
    pl->numVertex = 0;
    pl->vertex    = NULL;
    pl->zBuffer   = 1;  /* default: z-buffering on */
    return pl;
}

/*
 * polyline_createp — allocate a Polyline and populate it with a copy
 * of the given vertex array.
 *
 * Returns NULL if malloc fails.  The vertex array is deep-copied so the
 * caller can free vlist after this call.
 */
Polyline *polyline_createp(int numV, Point *vlist) {
    if (numV < 0 || !vlist) return polyline_create();  /* degenerate case */

    Polyline *pl = (Polyline *)malloc(sizeof(Polyline));
    if (!pl) {
        fprintf(stderr, "[polyline_createp] malloc failed for struct\n");
        return NULL;
    }

    pl->zBuffer   = 1;
    pl->numVertex = numV;
    pl->vertex    = NULL;

    if (numV > 0) {
        pl->vertex = (Point *)malloc((size_t)numV * sizeof(Point));
        if (!pl->vertex) {
            fprintf(stderr, "[polyline_createp] malloc failed for vertex\n");
            free(pl);
            return NULL;
        }
        for (int i = 0; i < numV; i++) {
            point_copy(&pl->vertex[i], &vlist[i]);
        }
    }

    return pl;
}

/*
 * polyline_free — free vertex[] and the Polyline struct itself.
 *
 * Safe to call with NULL.  After this call the caller's pointer is
 * dangling; it should be set to NULL.
 */
void polyline_free(Polyline *pl) {
    if (!pl) return;
    free(pl->vertex);  /* free(NULL) is well-defined (no-op) */
    free(pl);
}

/*
 * polyline_init — initialise a pre-existing Polyline to an empty state.
 *
 * Does NOT free any existing vertex memory; call polyline_clear first if
 * the Polyline may already hold data.
 */
void polyline_init(Polyline *pl) {
    if (!pl) return;
    pl->numVertex = 0;
    pl->vertex    = NULL;
    pl->zBuffer   = 1;
}

/*
 * polyline_set — replace the vertex list with a deep copy of vlist.
 *
 * Existing vertex memory is freed via polyline_clear.  On allocation
 * failure the Polyline is left in an empty (valid) state.
 */
void polyline_set(Polyline *pl, int numV, Point *vlist) {
    if (!pl) return;

    polyline_clear(pl);  /* free any existing vertex data */

    if (numV <= 0 || !vlist) return;  /* nothing to set */

    pl->vertex = (Point *)malloc((size_t)numV * sizeof(Point));
    if (!pl->vertex) {
        fprintf(stderr, "[polyline_set] malloc failed for vertex array\n");
        return;
    }

    pl->numVertex = numV;
    for (int i = 0; i < numV; i++) {
        point_copy(&pl->vertex[i], &vlist[i]);
    }
}

/*
 * polyline_clear — free the vertex array and reset the count to zero.
 *
 * The Polyline struct itself is NOT freed.  Leaves the Polyline in the
 * same state as after polyline_init.
 */
void polyline_clear(Polyline *pl) {
    if (!pl) return;
    free(pl->vertex);   /* free(NULL) is safe */
    pl->vertex    = NULL;
    pl->numVertex = 0;
}

/*
 * polyline_setz — primary setter for the z-buffer flag.
 */
void polyline_setz(Polyline *pl, int zflag) {
    if (pl) pl->zBuffer = zflag;
}

/*
 * polyline_zBuffer — API-compatible alias for polyline_setz.
 */
void polyline_zBuffer(Polyline *pl, int zflag) {
    polyline_setz(pl, zflag);
}

/*
 * polyline_copy — deep-copy all vertex data from *from to *to.
 *
 * Existing vertex memory in *to is freed.  zBuffer is also copied.
 */
void polyline_copy(Polyline *to, Polyline *from) {
    if (!to || !from) return;

    polyline_clear(to);  /* free to's existing vertices */

    to->zBuffer   = from->zBuffer;
    to->numVertex = from->numVertex;

    if (from->numVertex > 0 && from->vertex) {
        to->vertex = (Point *)malloc((size_t)from->numVertex * sizeof(Point));
        if (!to->vertex) {
            fprintf(stderr, "[polyline_copy] malloc failed\n");
            to->numVertex = 0;
            return;
        }
        for (int i = 0; i < from->numVertex; i++) {
            point_copy(&to->vertex[i], &from->vertex[i]);
        }
    }
}

/*
 * polyline_print — write the Polyline's zBuffer flag and all vertex
 * coordinates to the FILE stream fp.
 */
void polyline_print(Polyline *pl, FILE *fp) {
    if (!pl || !fp) return;
    fprintf(fp, "Polyline: numVertex=%d  zBuffer=%d\n",
            pl->numVertex, pl->zBuffer);
    for (int i = 0; i < pl->numVertex; i++) {
        fprintf(fp, "  [%d] (%.4f, %.4f, %.4f, %.4f)\n", i,
                pl->vertex[i].val[0], pl->vertex[i].val[1],
                pl->vertex[i].val[2], pl->vertex[i].val[3]);
    }
}

/*
 * polyline_normalize — call point_normalize on every vertex.
 *
 * Used after matrix transformation when the homogeneous coordinate may
 * differ from 1.0.
 */
void polyline_normalize(Polyline *pl) {
    if (!pl) return;
    for (int i = 0; i < pl->numVertex; i++) {
        point_normalize(&pl->vertex[i]);
    }
}

/*
 * polyline_draw — draw the polyline as a sequence of connected line
 * segments using line_draw.
 *
 * Each segment from vertex[i] to vertex[i+1] inherits the Polyline's
 * zBuffer flag.  A Polyline with fewer than 2 vertices draws nothing.
 */
void polyline_draw(Polyline *pl, Image *img, Color c) {
    if (!pl || !img || pl->numVertex < 2) return;

    for (int i = 0; i < pl->numVertex - 1; i++) {
        Line l;
        line_set(&l, pl->vertex[i], pl->vertex[i + 1]);
        line_setz(&l, pl->zBuffer);  /* propagate z-buffer flag */
        line_draw(&l, img, c);
    }
}

/*
 * polyline_add — append a single Point to the vertex list.
 *
 * Grows the vertex array with realloc.  The return value is checked
 * before overwriting pl->vertex so that a realloc failure leaves the
 * Polyline intact rather than producing a memory leak.
 */
void polyline_add(Polyline *pl, Point v) {
    if (!pl) return;

    int   newCount = pl->numVertex + 1;
    Point *tmp = (Point *)realloc(pl->vertex,
                                  (size_t)newCount * sizeof(Point));
    if (!tmp) {
        fprintf(stderr, "[polyline_add] realloc failed\n");
        return;  /* leave pl unchanged */
    }

    pl->vertex = tmp;
    point_copy(&pl->vertex[pl->numVertex], &v);
    pl->numVertex = newCount;
}
