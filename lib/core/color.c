
#include "color.h"
#include <stdio.h>   /* printf                */
#include <math.h>    /* fabsf                 */

/* -----------------------------------------------------------------------
 * Internal helper
 * ----------------------------------------------------------------------- */

/*
 * clamp01 — restrict a float to the closed interval [0.0, 1.0].
 *
 * Declared static so it is not visible (or linkable) outside this
 * translation unit.  Using an inline hint lets the compiler substitute
 * the body directly at each call site, eliminating function-call overhead
 * in tight pixel loops.
 *
 * @param val  any float
 * @return     val clamped to [0.0, 1.0]
 */
static inline float clamp01(float val) {
    if (val < 0.0f) return 0.0f;
    if (val > 1.0f) return 1.0f;
    return val;
}


/* =======================================================================
 * Constructors / initialisation
 * ======================================================================= */

/*
 * color_set — write clamped r, g, b values into an existing Color.
 *
 * Each channel is individually clamped to [0, 1] before storage, so
 * callers can pass raw shading sums without pre-clamping.
 *
 * Spec reference: "void color_set(Color *to, float r, float g, float b)
 *                  – sets the Color data."
 */
void color_set(Color *to, float r, float g, float b) {
    if (!to) return;            /* guard against NULL destination */

    to->c[0] = clamp01(r);     /* red   channel */
    to->c[1] = clamp01(g);     /* green channel */
    to->c[2] = clamp01(b);     /* blue  channel */
}

/*
 * color_copy — copy all three channels from *from into *to.
 *
 * Equivalent to the struct assignment  *to = *from  when both pointers
 * are non-NULL.  The explicit function exists so that other modules can
 * copy a Color without knowing its internal layout.
 *
 * Spec reference: "void color_copy(Color *to, Color *from)
 *                  – copies the Color data."
 */
void color_copy(Color *to, Color *from) {
    if (!to || !from) return;   /* guard against NULL pointers */
    *to = *from;                /* struct assignment copies all three channels */
}

/*
 * color_init — zero every channel (produces black / no-energy colour).
 *
 * Use before accumulating multiple light contributions so that the
 * running total starts from a known state:
 *
 *     Color result;
 *     color_init(&result);
 *     for (each light) { ... color_add(&result, &result, &contribution); }
 *     color_clamp(&result);
 */
void color_init(Color *c) {
    if (!c) return;

    c->c[0] = 0.0f;  /* red   = 0 */
    c->c[1] = 0.0f;  /* green = 0 */
    c->c[2] = 0.0f;  /* blue  = 0 */
}

/*
 * color_fromInts — convert integer pixel values to a float Color.
 *
 * Divides each integer by maxval to produce a value in [0, 1], then
 * clamps in case the source value exceeds maxval.  This centralises
 * the "/ 255.0f" arithmetic used when loading image files.
 *
 * Example:
 *   unsigned char buf[3];
 *   fread(buf, 1, 3, fp);
 *   color_fromInts(&col, buf[0], buf[1], buf[2], 255);
 */
void color_fromInts(Color *to, int ri, int gi, int bi, int maxval) {
    if (!to || maxval <= 0) return;

    float inv = 1.0f / (float)maxval;   /* pre-compute reciprocal once */
    to->c[0] = clamp01((float)ri * inv);
    to->c[1] = clamp01((float)gi * inv);
    to->c[2] = clamp01((float)bi * inv);
}


/* =======================================================================
 * Comparison
 * ======================================================================= */

/*
 * color_equal — exact equality test on all three channels.
 *
 * Uses == which is appropriate for checking against compile-time
 * constants (e.g., "is this pixel still pure black?").  Do NOT use
 * this after floating-point arithmetic — use color_nearlyEqual instead.
 *
 * Returns 0 if either pointer is NULL (NULL ≠ NULL in this context).
 *
 * Spec reference: "int color_equal(Color *c1, Color *c2)"
 */
int color_equal(Color *c1, Color *c2) {
    if (!c1 || !c2) return 0;  /* NULL pointers are never equal */

    return (c1->c[0] == c2->c[0]) &&
           (c1->c[1] == c2->c[1]) &&
           (c1->c[2] == c2->c[2]);
}

/*
 * color_nearlyEqual — fuzzy equality test using a per-channel tolerance.
 *
 * Returns 1 only if |c1->c[i] - c2->c[i]| <= eps for all i in {0,1,2}.
 * fabsf() is used so that the comparison is symmetric.
 *
 * Typical usage:  color_nearlyEqual(&a, &b, 1e-5f)
 */
int color_nearlyEqual(Color *c1, Color *c2, float eps) {
    if (!c1 || !c2) return 0;

    return (fabsf(c1->c[0] - c2->c[0]) <= eps) &&
           (fabsf(c1->c[1] - c2->c[1]) <= eps) &&
           (fabsf(c1->c[2] - c2->c[2]) <= eps);
}


/* =======================================================================
 * Arithmetic — used in shading / lighting calculations
 * ======================================================================= */

/*
 * color_add — component-wise addition:  dst = a + b.
 *
 * The result is NOT clamped so that multiple light contributions can
 * accumulate freely before a final color_clamp() call:
 *
 *     color_add(&total, &total, &ambient_contrib);
 *     color_add(&total, &total, &diffuse_contrib);
 *     color_clamp(&total);
 *
 * dst may safely alias a or b (the values are read before being written
 * when dst == a, because we compute all three channels before storing).
 */
void color_add(Color *dst, const Color *a, const Color *b) {
    if (!dst || !a || !b) return;

    /* Compute into temporaries first so aliasing (dst == a) is safe */
    float r = a->c[0] + b->c[0];
    float g = a->c[1] + b->c[1];
    float bl = a->c[2] + b->c[2];
    dst->c[0] = r;
    dst->c[1] = g;
    dst->c[2] = bl;
}

/*
 * color_scale — multiply every channel by a scalar:  dst = s * src.
 *
 * Used to apply geometric attenuation to a light contribution, e.g.:
 *
 *     float ndotl = vector_dot(&N, &L);     // (L · N)
 *     color_scale(&diffuse, &light_color, ndotl);
 *
 * Negative scalars are allowed (their contribution is simply zeroed by
 * color_clamp if the sum goes below 0).
 */
void color_scale(Color *dst, const Color *src, float s) {
    if (!dst || !src) return;

    dst->c[0] = src->c[0] * s;
    dst->c[1] = src->c[1] * s;
    dst->c[2] = src->c[2] * s;
}

/*
 * color_multiply — component-wise product:  dst = a * b.
 *
 * This is the "filter" multiplication from the shading equation:
 *
 *   Body term:    I_body   = Cb * CLd * (L·N)
 *   Surface term: I_surface= Cs * CLd * (H·N)^n
 *
 * where both Cb (body colour) and CLd (light colour) are Colors, and
 * the multiplication is applied independently to each RGB channel.
 *
 * dst may safely alias a or b.
 */
void color_multiply(Color *dst, const Color *a, const Color *b) {
    if (!dst || !a || !b) return;

    float r  = a->c[0] * b->c[0];
    float g  = a->c[1] * b->c[1];
    float bl = a->c[2] * b->c[2];
    dst->c[0] = r;
    dst->c[1] = g;
    dst->c[2] = bl;
}

/*
 * color_clamp — clamp all three channels to [0.0, 1.0] in place.
 *
 * Call once after summing all light contributions (ambient + diffuse +
 * specular) to produce a legal colour value before writing to the image:
 *
 *     color_add(&total, &total, &specular_contrib);
 *     color_clamp(&total);
 *     image_setColor(img, r, c, total);
 */
void color_clamp(Color *c) {
    if (!c) return;

    c->c[0] = clamp01(c->c[0]);
    c->c[1] = clamp01(c->c[1]);
    c->c[2] = clamp01(c->c[2]);
}


/* =======================================================================
 * Conversion helpers
 * ======================================================================= */

/*
 * color_toInt — convert a single float channel to an integer in [0, maxval].
 *
 * The value is clamped before scaling, so out-of-range floats (e.g., 1.001
 * from floating-point rounding) produce maxval rather than maxval+1.
 *
 * Rounding is performed by truncation (cast to int), which matches the
 * behaviour used when writing PPM files.
 *
 * Example:  color_toInt(0.5f, 255) → 127
 */
int color_toInt(float val, int maxval) {
    if (maxval <= 0) return 0;
    return (int)(clamp01(val) * (float)maxval);
}

/*
 * color_toInts — convert all three channels of a Color to integers.
 *
 * Each output is written via the provided pointers, which must all be
 * non-NULL.  Delegates to color_toInt for consistent rounding.
 *
 * Example:
 *   int r, g, b;
 *   color_toInts(&col, &r, &g, &b, 255);
 *   fputc((unsigned char)r, fp);
 */
void color_toInts(const Color *src, int *ri, int *gi, int *bi, int maxval) {
    if (!src || !ri || !gi || !bi) return;

    *ri = color_toInt(src->c[0], maxval);  /* red   */
    *gi = color_toInt(src->c[1], maxval);  /* green */
    *bi = color_toInt(src->c[2], maxval);  /* blue  */
}

/*
 * color_fromFPixel — copy the RGB components of an FPixel into a Color.
 *
 * This bridges the two types without requiring the caller to know the
 * internal field names of FPixel.
 *
 * FPixel.z (depth) is intentionally ignored — Color has no depth field.
 */
void color_fromFPixel(Color *dst, const FPixel *px) {
    if (!dst || !px) return;

    dst->c[0] = clamp01(px->rgb[0]);   /* red   from FPixel.rgb[0] */
    dst->c[1] = clamp01(px->rgb[1]);   /* green from FPixel.rgb[1] */
    dst->c[2] = clamp01(px->rgb[2]);   /* blue  from FPixel.rgb[2] */
}

/*
 * color_toFPixel — write a Color's RGB into an FPixel's rgb[] array.
 *
 * FPixel.z is NOT modified; the caller is responsible for setting the
 * depth value separately (e.g., via image_setz).
 *
 * Values are clamped before writing so that out-of-range shading results
 * cannot corrupt the FPixel.
 */
void color_toFPixel(FPixel *dst, const Color *src) {
    if (!dst || !src) return;

    dst->rgb[0] = clamp01(src->c[0]);  /* red   → FPixel.rgb[0] */
    dst->rgb[1] = clamp01(src->c[1]);  /* green → FPixel.rgb[1] */
    dst->rgb[2] = clamp01(src->c[2]);  /* blue  → FPixel.rgb[2] */
    /* dst->z is left unchanged intentionally */
}


/* =======================================================================
 * Debug / display
 * ======================================================================= */

/*
 * color_print — print a human-readable summary of a Color to stdout.
 *
 * Format:  [<label>] Color(<r>, <g>, <b>)
 * Example: [sky]     Color(0.529, 0.808, 0.922)
 *
 * Useful inside debuggers and unit-test diagnostics.
 *
 * @param c      Color to display (NULL prints a placeholder)
 * @param label  short tag to identify the color (NULL → "Color")
 */
void color_print(const Color *c, const char *label) {
    const char *lbl = label ? label : "Color";

    if (!c) {
        printf("[%s] (null)\n", lbl);
        return;
    }

    printf("[%s] Color(%.4f, %.4f, %.4f)\n",
           lbl, c->c[0], c->c[1], c->c[2]);
}
