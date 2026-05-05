#ifndef IMAGE_H
#define IMAGE_H

#include "color.h"   /* FPixel, Color */

/* -----------------------------------------------------------------------
 * Enumerations
 * ----------------------------------------------------------------------- */

/**
 * Origin — describes which corner of the image is (row=0, col=0).
 *
 * ORIGIN_UPPER_LEFT  — row 0 is the topmost row   (default, matches PPM)
 * ORIGIN_LOWER_LEFT  — row 0 is the bottommost row (OpenGL-style)
 */
typedef enum {
    ORIGIN_UPPER_LEFT = 0,
    ORIGIN_LOWER_LEFT = 1
} Origin;

/**
 * DepthType — determines which depth value "wins" during z-buffer tests.
 *
 * DEPTH_LESS     — smaller 1/z is closer (standard perspective)
 * DEPTH_GREATER  — larger  1/z is closer (pre-divided reciprocal depth)
 */
typedef enum {
    DEPTH_LESS    = 0,
    DEPTH_GREATER = 1
} DepthType;


/* -----------------------------------------------------------------------
 * Image data structure
 * ----------------------------------------------------------------------- */

/**
 * Image — a rectangular grid of floating-point pixels plus auxiliary
 *         alpha and depth channels.
 *
 * Fields
 * ------
 * data     : flat (rows × cols) array of FPixel holding RGB + per-pixel z
 * rows     : height of the image in pixels
 * cols     : width  of the image in pixels
 * alpha    : flat (rows × cols) float array; alpha[i] ∈ [0,1]
 * depth    : flat (rows × cols) float array; depth[i] = 1/z
 * o        : origin convention used by all public API functions
 * d        : depth comparison direction used by rasteriser
 * maxval   : maximum integer value written to file (default 255 for PPM)
 * filename : optional copy of the source filename (heap-allocated or NULL)
 */
typedef struct {
    FPixel *data;    /* RGB pixel data (rows × cols)          */
    int     rows;    /* number of rows                         */
    int     cols;    /* number of columns                      */
    float  *alpha;   /* per-pixel alpha channel (rows × cols) */
    float  *depth;   /* per-pixel 1/z depth    (rows × cols)  */

    Origin    o;     /* coordinate-system origin               */
    DepthType d;     /* depth-buffer comparison direction      */

    int   maxval;    /* output max value (default 255)         */
    char *filename;  /* optional stored filename (may be NULL) */
} Image;


/* -----------------------------------------------------------------------
 * Constructor / Destructor functions
 * ----------------------------------------------------------------------- */

/**
 * image_init — initialise all fields of an already-allocated Image to
 * safe defaults (rows=0, cols=0, all pointers NULL).
 *
 * Call this on a stack-allocated Image before any other function, or
 * after a placement-new-style allocation pattern.
 *
 * @param src  pointer to an Image struct (must not be NULL)
 */
void image_init(Image *src);

/**
 * image_create — heap-allocate a new Image and fill it with pixels.
 *
 * If rows ≤ 0 or cols ≤ 0, the struct is allocated and initialised but
 * no pixel data is allocated (data/alpha/depth remain NULL).
 *
 * @param rows  desired height in pixels
 * @param cols  desired width  in pixels
 * @return      pointer to the new Image, or NULL on allocation failure
 */
Image *image_create(int rows, int cols);

/**
 * image_alloc — allocate (or re-allocate) pixel data for an existing
 * Image structure.
 *
 * Any existing data is freed before the new allocation.  On success all
 * pixels are reset to defaults (black, alpha=1, depth=1).  Returns 0 on
 * success and non-zero on failure; the Image remains in a safe, empty
 * state after failure.
 *
 * @param src   pointer to an initialised Image struct
 * @param rows  desired height
 * @param cols  desired width
 * @return      0 on success, non-zero on failure
 */
int image_alloc(Image *src, int rows, int cols);

/**
 * image_dealloc — free pixel data (data, alpha, depth) and reset size
 * fields to zero.  Does NOT free the Image struct itself.
 *
 * Safe to call on an already-empty Image.
 *
 * @param src  pointer to an Image (may be NULL → no-op)
 */
void image_dealloc(Image *src);

/**
 * image_free — fully destroy a heap-allocated Image: frees pixel data,
 * the filename string if present, and the Image struct itself.
 *
 * After this call the pointer is dangling; the caller should set it to
 * NULL.
 *
 * @param src  pointer returned by image_create / image_read (may be NULL)
 */
void image_free(Image *src);


/* -----------------------------------------------------------------------
 * I/O functions
 * ----------------------------------------------------------------------- */

/**
 * image_read — read a PPM (P6) image from disk and return a new Image.
 *
 * The alpha channel is initialised to 1.0 and the depth channel to 1.0
 * for every pixel.  The file type is inferred from the extension:
 * currently ".ppm" is supported; the function returns NULL for unknown
 * types.
 *
 * @param filename  path to the image file
 * @return          a newly allocated Image, or NULL on failure
 */
Image *image_read(char *filename);

/**
 * image_write — write an Image to disk in PPM P6 format (or another
 * format inferred from the filename extension).
 *
 * Pixel values are clamped to [0,1] and scaled to [0, src->maxval]
 * before writing.
 *
 * @param src       source Image (must not be NULL)
 * @param filename  destination file path
 * @return          0 on success, non-zero on failure
 */
int image_write(Image *src, char *filename);


/* -----------------------------------------------------------------------
 * Utility / fill functions
 * ----------------------------------------------------------------------- */

/**
 * image_reset — set every pixel to black (RGB 0,0,0), alpha 1.0, depth 1.0.
 *
 * This is the canonical "clear" operation before rendering a new frame.
 *
 * @param src  target Image (may be NULL → no-op)
 */
void image_reset(Image *src);

/**
 * image_fill — set every pixel's FPixel to val.
 *
 * If val contains a z field it is also written to the depth array so
 * that both stores remain consistent.
 *
 * @param src  target Image
 * @param val  FPixel value to broadcast
 */
void image_fill(Image *src, FPixel val);

/**
 * image_fillrgb — set the RGB components of every pixel to (r, g, b).
 *
 * Alpha and depth channels are left unchanged.
 *
 * @param src  target Image
 * @param r    red   component ∈ [0,1] (not clamped; caller's responsibility)
 * @param g    green component ∈ [0,1]
 * @param b    blue  component ∈ [0,1]
 */
void image_fillrgb(Image *src, float r, float g, float b);

/**
 * image_filla — set the alpha channel of every pixel to a.
 *
 * The value is clamped to [0, 1] before writing.
 *
 * @param src  target Image
 * @param a    alpha value ∈ [0,1]
 */
void image_filla(Image *src, float a);

/**
 * image_fillz — set the depth channel of every pixel to z.
 *
 * Both the depth array and FPixel.z are updated to keep them in sync.
 *
 * @param src  target Image
 * @param z    depth value (1/z convention; 1.0 = back plane)
 */
void image_fillz(Image *src, float z);

/**
 * image_fillColor — set every pixel to the given Color, leaving alpha
 * and depth unchanged.  Convenience wrapper for image_fillrgb.
 *
 * @param src  target Image
 * @param val  Color to broadcast
 */
void image_fillColor(Image *src, Color val);


/* -----------------------------------------------------------------------
 * Per-pixel access functions (FPixel level)
 * ----------------------------------------------------------------------- */

/**
 * image_getf — return the FPixel at image position (r, c).
 *
 * Row r is interpreted according to src->o.  Returns a black FPixel
 * with default values for out-of-bounds coordinates.
 *
 * @param src  source Image
 * @param r    row    (in API coordinate system)
 * @param c    column (0 = left)
 * @return     copy of the FPixel at that location
 */
FPixel image_getf(Image *src, int r, int c);

/**
 * image_setf — write an FPixel into the image at position (r, c).
 *
 * RGB values are clamped to [0, 1].  The z field of val is NOT
 * automatically written to the depth array; use image_setz for that.
 *
 * @param src  target Image
 * @param r    row    (API coordinate system)
 * @param c    column
 * @param val  FPixel to write (RGB clamped, z ignored for depth array)
 */
void image_setf(Image *src, int r, int c, FPixel val);


/* -----------------------------------------------------------------------
 * Per-pixel access functions (individual channel level)
 * ----------------------------------------------------------------------- */

/**
 * image_getc — return a single RGB band value at pixel (r, c).
 *
 * @param src  source Image
 * @param r    row
 * @param c    column
 * @param b    band index: 0=R, 1=G, 2=B
 * @return     channel value ∈ [0,1], or 0.0 for invalid inputs
 */
float image_getc(Image *src, int r, int c, int b);

/**
 * image_setc — set a single RGB band value at pixel (r, c).
 *
 * @param src  target Image
 * @param r    row
 * @param c    column
 * @param b    band index: 0=R, 1=G, 2=B
 * @param val  new value (clamped to [0,1])
 */
void image_setc(Image *src, int r, int c, int b, float val);

/**
 * image_geta — return the alpha value at pixel (r, c).
 *
 * @return alpha ∈ [0,1], or 0.0 for invalid inputs
 */
float image_geta(Image *src, int r, int c);

/**
 * image_seta — set the alpha value at pixel (r, c).
 *
 * @param val  alpha ∈ [0,1] (clamped)
 */
void image_seta(Image *src, int r, int c, float val);

/**
 * image_getz — return the depth (1/z) value at pixel (r, c).
 *
 * @return depth value, or 0.0 for invalid inputs
 */
float image_getz(Image *src, int r, int c);

/**
 * image_setz — set the depth (1/z) value at pixel (r, c).
 *
 * Both the depth array and the FPixel.z field are updated.
 *
 * @param val  new 1/z depth value
 */
void image_setz(Image *src, int r, int c, float val);

/**
 * image_getb — generalised band accessor: 0-2 = RGB, 3 = alpha, 4 = depth.
 *
 * This is a primary method (not just a helper) that consolidates all
 * single-channel reads behind one dispatch function.
 *
 * @param src  source Image
 * @param r    row
 * @param c    column
 * @param b    band index (0=R,1=G,2=B,3=alpha,4=depth)
 * @return     channel value
 */
float image_getb(Image *src, int r, int c, int b);

/**
 * image_setb — generalised band mutator: 0-2 = RGB, 3 = alpha, 4 = depth.
 *
 * This is a primary method that consolidates all single-channel writes.
 * For band 4 (depth) both the depth array and FPixel.z are updated.
 *
 * @param src  target Image
 * @param r    row
 * @param c    column
 * @param b    band index (0=R,1=G,2=B,3=alpha,4=depth)
 * @param val  new value
 */
void image_setb(Image *src, int r, int c, int b, float val);


/* -----------------------------------------------------------------------
 * Color-level access functions
 * ----------------------------------------------------------------------- */

/**
 * image_setColor — copy a Color struct into the RGB channels of pixel
 * (r, c).  Alpha and depth are not modified.
 *
 * @param src  target Image
 * @param r    row
 * @param c    column
 * @param val  Color to write (components clamped to [0,1])
 */
void image_setColor(Image *src, int r, int c, Color val);

/**
 * image_getColor — build and return a Color from the RGB channels of
 * pixel (r, c).
 *
 * @param src  source Image
 * @param r    row
 * @param c    column
 * @return     Color struct (components ∈ [0,1])
 */
Color image_getColor(Image *src, int r, int c);


/* -----------------------------------------------------------------------
 * Utility / query functions
 * ----------------------------------------------------------------------- */

/**
 * image_getRows — return the number of rows in the image.
 *
 * @param src  source Image (NULL → returns 0)
 * @return     row count
 */
int image_getRows(const Image *src);

/**
 * image_getCols — return the number of columns in the image.
 *
 * @param src  source Image (NULL → returns 0)
 * @return     column count
 */
int image_getCols(const Image *src);

/**
 * image_copy — duplicate an Image, allocating fresh pixel arrays for
 * the copy.  The copy inherits all metadata (origin, depth type, maxval,
 * filename).
 *
 * @param src  source Image to clone
 * @return     newly allocated Image, or NULL on failure / NULL src
 */
Image *image_copy(const Image *src);

/**
 * image_setOrigin — change the coordinate-system origin used by all
 * public API functions.  Existing pixel data is not rearranged in memory.
 *
 * @param src  target Image
 * @param o    new Origin value
 */
void image_setOrigin(Image *src, Origin o);

/**
 * image_setDepthType — change the depth comparison direction stored in
 * the Image for use by the rasteriser.
 *
 * @param src  target Image
 * @param d    new DepthType value
 */
void image_setDepthType(Image *src, DepthType d);

/**
 * image_depthTest — test whether a new depth value 'z' passes the
 * depth test for pixel (r, c) according to src->d.
 *
 * Returns 1 (true) if the new value is closer than the stored value,
 * 0 otherwise.  Does not modify any image data.
 *
 * @param src  Image to query
 * @param r    row
 * @param c    column
 * @param z    candidate 1/z value
 * @return     1 if the candidate passes the test, 0 if it fails
 */
int image_depthTest(Image *src, int r, int c, float z);

/**
 * image_print — print a compact text summary of the Image metadata to
 * stdout.  Useful for debugging.
 *
 * @param src     Image to describe
 * @param label   short label printed before the summary (may be NULL)
 */
void image_print(const Image *src, const char *label);

#endif /* IMAGE_H */
