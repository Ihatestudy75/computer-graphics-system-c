#ifndef COLOR_H
#define COLOR_H

/* -----------------------------------------------------------------------
 * FPixel — per-pixel storage type used inside the Image data array.
 *
 * Fields
 * ------
 * rgb[3]  : red, green, blue components in [0.0, 1.0]
 * z       : reciprocal depth (1/z) for z-buffering; stored alongside
 *           the colour so that a single array access retrieves both.
 *           Positive values only; 1.0 is the conventional back-plane.
 * ----------------------------------------------------------------------- */
typedef struct {
    float rgb[3]; /* R, G, B in [0, 1]          */
    float z;      /* 1/z depth for z-buffering   */
} FPixel;

/* -----------------------------------------------------------------------
 * Color — a lightweight RGB colour triple used for shading and draw state.
 *
 * Using a struct (rather than typedef float Color[3]) means that a simple
 * assignment  dst = src;  performs a correct memberwise copy without
 * needing memcpy or a helper function.
 *
 * Fields
 * ------
 * c[3]  : c[0]=red, c[1]=green, c[2]=blue, all in [0.0, 1.0]
 * ----------------------------------------------------------------------- */
typedef struct {
    float c[3]; /* c[0]=R, c[1]=G, c[2]=B in [0, 1] */
} Color;


/* =======================================================================
 * Constructors / initialisation
 * ======================================================================= */

/**
 * color_set — write r, g, b into an existing Color struct.
 *
 * Each channel is clamped to [0.0, 1.0] before storage, so callers
 * do not need to pre-clamp.
 *
 * @param to   destination Color (must not be NULL)
 * @param r    red   component (clamped to [0, 1])
 * @param g    green component (clamped to [0, 1])
 * @param b    blue  component (clamped to [0, 1])
 */
void color_set(Color *to, float r, float g, float b);

/**
 * color_copy — copy all three channels from *from into *to.
 *
 * Equivalent to *to = *from when both pointers are valid.
 * No-op if either pointer is NULL.
 *
 * @param to    destination Color
 * @param from  source      Color
 */
void color_copy(Color *to, Color *from);

/**
 * color_init — set every channel to 0.0 (black, fully absorbed).
 *
 * Use this to zero-initialise a Color before accumulating light
 * contributions in a shading loop.
 *
 * @param c  Color to zero-initialise (must not be NULL)
 */
void color_init(Color *c);

/**
 * color_fromInts — build a Color from three integer values in [0, maxval].
 *
 * Useful when reading image pixels from a file where values are stored
 * as unsigned bytes (maxval=255) or 16-bit integers (maxval=65535).
 *
 * The result is clamped to [0, 1] after scaling.
 *
 * @param to      destination Color
 * @param ri      red   integer value
 * @param gi      green integer value
 * @param bi      blue  integer value
 * @param maxval  maximum integer value (e.g., 255)
 */
void color_fromInts(Color *to, int ri, int gi, int bi, int maxval);


/* =======================================================================
 * Comparison
 * ======================================================================= */

/**
 * color_equal — test whether two Colors have identical channel values.
 *
 * Uses exact float equality — appropriate for checking whether a color
 * has been set to a known constant, not for general floating-point
 * comparison after arithmetic.
 *
 * @param c1  first  Color (NULL → returns 0)
 * @param c2  second Color (NULL → returns 0)
 * @return    1 if all three channels are equal, 0 otherwise
 */
int color_equal(Color *c1, Color *c2);

/**
 * color_nearlyEqual — test whether two Colors are within tolerance eps
 * on every channel.
 *
 * Use this instead of color_equal when comparing colors that have been
 * through floating-point arithmetic (e.g., shading calculations).
 *
 * @param c1   first  Color
 * @param c2   second Color
 * @param eps  per-channel absolute tolerance (e.g., 1e-5f)
 * @return     1 if |c1->c[i] - c2->c[i]| <= eps for i in {0,1,2}; else 0
 */
int color_nearlyEqual(Color *c1, Color *c2, float eps);


/* =======================================================================
 * Arithmetic — used heavily in shading / lighting calculations
 * ======================================================================= */

/**
 * color_add — component-wise addition: dst = a + b.
 *
 * Result channels are NOT automatically clamped so that intermediate
 * sums across multiple light sources can accumulate before a final
 * color_clamp call.
 *
 * @param dst  destination Color (may alias a or b)
 * @param a    first  operand
 * @param b    second operand
 */
void color_add(Color *dst, const Color *a, const Color *b);

/**
 * color_scale — multiply every channel by a scalar: dst = s * src.
 *
 * Used for attenuating a light contribution by a geometric factor such
 * as (L · N) or a spotlight falloff coefficient.
 *
 * @param dst  destination Color (may alias src)
 * @param src  source Color
 * @param s    scalar multiplier (any float; negative values allowed)
 */
void color_scale(Color *dst, const Color *src, float s);

/**
 * color_multiply — component-wise product: dst = a * b.
 *
 * This implements the "filter" operation used in the shading equation:
 *   body contribution  = Cb * CLd * (L · N)
 * where Cb and CLd are both Colors and the product is per-channel.
 *
 * @param dst  destination Color (may alias a or b)
 * @param a    first  Color (e.g., body colour)
 * @param b    second Color (e.g., light colour)
 */
void color_multiply(Color *dst, const Color *a, const Color *b);

/**
 * color_clamp — clamp every channel of c to [0.0, 1.0] in place.
 *
 * Call this after summing all light contributions to ensure the result
 * is a legal colour value before writing it into the image.
 *
 * @param c  Color to clamp (in place)
 */
void color_clamp(Color *c);


/* =======================================================================
 * Conversion helpers
 * ======================================================================= */

/**
 * color_toInt — convert a single float channel to an integer in [0, maxval].
 *
 * The value is clamped to [0, 1] before scaling, so out-of-range floats
 * are handled gracefully.
 *
 * Example: color_toInt(src->c[0], 255) → red channel as an unsigned byte.
 *
 * @param val     float value in [0, 1] (clamped if outside)
 * @param maxval  integer maximum (e.g., 255 for 8-bit, 65535 for 16-bit)
 * @return        integer in [0, maxval]
 */
int color_toInt(float val, int maxval);

/**
 * color_toInts — convert all three channels of a Color to integers.
 *
 * Writes the results into *ri, *gi, *bi.  Each output is in [0, maxval].
 *
 * @param src     source Color
 * @param ri      pointer for the red   integer result
 * @param gi      pointer for the green integer result
 * @param bi      pointer for the blue  integer result
 * @param maxval  integer maximum (e.g., 255)
 */
void color_toInts(const Color *src, int *ri, int *gi, int *bi, int maxval);

/**
 * color_fromFPixel — extract the RGB components of an FPixel as a Color.
 *
 * Provides a typed bridge between the pixel-storage type (FPixel) and
 * the shading type (Color) without manual indexing at call sites.
 *
 * @param dst  destination Color
 * @param px   source FPixel
 */
void color_fromFPixel(Color *dst, const FPixel *px);

/**
 * color_toFPixel — write a Color's RGB into an FPixel, leaving FPixel.z
 * unchanged.
 *
 * @param dst  destination FPixel (z field is not modified)
 * @param src  source Color
 */
void color_toFPixel(FPixel *dst, const Color *src);


/* =======================================================================
 * Debug / display
 * ======================================================================= */

/**
 * color_print — print a compact representation of a Color to stdout.
 *
 * Example output:  [sky] Color(0.529, 0.808, 0.922)
 *
 * @param c      Color to display (NULL prints a placeholder)
 * @param label  short label printed before the values (NULL → "Color")
 */
void color_print(const Color *c, const char *label);

#endif /* COLOR_H */
