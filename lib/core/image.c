#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* -----------------------------------------------------------------------
 * Internal macros and helpers
 * ----------------------------------------------------------------------- */

/*
 * INDEX — convert an (internal) row and column to a flat array offset.
 *
 * The internal row is already translated by map_row(); always pass the
 * mapped row here, never the raw API row.
 */
#define INDEX(src, r_internal, c) ((size_t)(r_internal) * (size_t)(src)->cols + (size_t)(c))

/*
 * clamp01 — clamp a float to the closed interval [0.0, 1.0].
 *
 * Used when writing colour or alpha values to ensure they stay valid.
 */
static inline float clamp01(float val) {
    if (val < 0.0f) return 0.0f;
    if (val > 1.0f) return 1.0f;
    return val;
}

/*
 * map_row — translate an API row index to an internal (upper-left) row.
 *
 * When the image origin is ORIGIN_UPPER_LEFT the mapping is the identity.
 * When the origin is ORIGIN_LOWER_LEFT row 0 in the API corresponds to
 * the last row in memory, so we flip: internal_row = (rows - 1) - api_row.
 *
 * @param src    Image whose origin field guides the mapping
 * @param r_api  row index as supplied by the caller
 * @return       row index into the internal data array
 */
static inline int map_row(const Image *src, int r_api) {
    if (!src) return r_api;
    if (src->o == ORIGIN_UPPER_LEFT) return r_api;
    return src->rows - 1 - r_api;   /* flip for lower-left origin */
}

/*
 * valid_rc — return 1 if (r_api, c) is inside the image, 0 otherwise.
 *
 * Validates after applying map_row so that origin flipping is accounted
 * for.  All public accessors call this before touching the data arrays.
 *
 * @param src    Image to validate against
 * @param r_api  API row (before origin mapping)
 * @param c      column
 */
static inline int valid_rc(const Image *src, int r_api, int c) {
    if (!src || !src->data) return 0;
    if (c < 0 || c >= src->cols)  return 0;
    int r = map_row(src, r_api);
    if (r < 0 || r >= src->rows)  return 0;
    return 1;
}


/* -----------------------------------------------------------------------
 * Constructor / Destructor functions
 * ----------------------------------------------------------------------- */

/*
 * image_init — set all Image fields to safe, "empty" defaults.
 *
 * This must be called on any stack-allocated Image before use, and is
 * called internally by image_create.  It does NOT allocate memory.
 */
void image_init(Image *src) {
    if (!src) return;

    src->rows     = 0;
    src->cols     = 0;
    src->data     = NULL;
    src->alpha    = NULL;
    src->depth    = NULL;

    /* Default coordinate convention: upper-left origin, 1/z depth */
    src->o = ORIGIN_UPPER_LEFT;
    src->d = DEPTH_GREATER;

    /* PPM default maximum integer output value */
    src->maxval   = 255;
    src->filename = NULL;
}

/*
 * image_create — heap-allocate and initialise a new Image.
 *
 * Steps:
 *   1. malloc the Image struct and check for failure.
 *   2. image_init to zero out all fields.
 *   3. If rows/cols are positive, image_alloc to create pixel arrays.
 *
 * Returns NULL only if malloc fails; a zero-size image (rows or cols ≤ 0)
 * is still returned as a valid, empty Image struct.
 */
Image *image_create(int rows, int cols) {
    /* Allocate the Image struct itself */
    Image *img = (Image *)malloc(sizeof(Image));
    if (!img) {
        fprintf(stderr, "[image_create] malloc failed for Image struct\n");
        return NULL;
    }

    /* Safe default state before any allocation */
    image_init(img);

    /* A zero-size image is allowed: return the empty struct */
    if (rows <= 0 || cols <= 0) return img;

    /* Allocate and initialise pixel data */
    if (image_alloc(img, rows, cols) != 0) {
        fprintf(stderr, "[image_create] image_alloc failed (%d x %d)\n",
                rows, cols);
        free(img);
        return NULL;
    }

    return img;
}

/*
 * image_alloc — (re-)allocate pixel data arrays for an existing Image.
 *
 * Any previously allocated data is freed first.  On failure the Image is
 * left in a valid, empty state (data/alpha/depth = NULL, rows=cols=0).
 *
 * Returns:
 *   0  — success; all pixels reset to default values
 *   1  — src pointer was NULL
 *   2  — malloc failed for one or more arrays
 */
int image_alloc(Image *src, int rows, int cols) {
    if (!src) {
        fprintf(stderr, "[image_alloc] NULL Image pointer\n");
        return 1;
    }

    /* Free any previously allocated pixel data */
    image_dealloc(src);

    /* Treat non-positive dimensions as a request for an empty image */
    if (rows <= 0 || cols <= 0) {
        src->rows = 0;
        src->cols = 0;
        return 0;
    }

    src->rows = rows;
    src->cols = cols;
    size_t n  = (size_t)rows * (size_t)cols;

    src->data  = (FPixel *)malloc(n * sizeof(FPixel));
    src->alpha = (float  *)malloc(n * sizeof(float));
    src->depth = (float  *)malloc(n * sizeof(float));

    if (!src->data || !src->alpha || !src->depth) {
        fprintf(stderr, "[image_alloc] malloc failed for pixel arrays "
                        "(%d x %d)\n", rows, cols);
        /* Partial allocation — clean up to leave a safe empty state */
        image_dealloc(src);
        return 2;
    }

    /* Initialise all pixels to default values (black, alpha=1, depth=1) */
    image_reset(src);
    return 0;
}

/*
 * image_dealloc — free the pixel data arrays and reset dimension fields.
 *
 * The Image struct itself is NOT freed; use image_free for that.
 * Safe to call on an already-empty Image (all pointer checks are guarded).
 */
void image_dealloc(Image *src) {
    if (!src) return;

    free(src->data);   src->data  = NULL;
    free(src->alpha);  src->alpha = NULL;
    free(src->depth);  src->depth = NULL;

    src->rows = 0;
    src->cols = 0;
}

/*
 * image_free — fully destroy a heap-allocated Image.
 *
 * Frees pixel data arrays, the filename string (if dynamically
 * allocated), and the Image struct itself.  The caller's pointer
 * becomes dangling; set it to NULL after calling this.
 */
void image_free(Image *src) {
    if (!src) return;

    image_dealloc(src);                    /* free pixel arrays             */
    if (src->filename) free(src->filename); /* free optional filename string */
    free(src);                              /* free the Image struct itself  */
}


/* -----------------------------------------------------------------------
 * I/O functions
 * ----------------------------------------------------------------------- */

/*
 * image_read — load an image from disk.
 *
 * The file format is inferred from the filename extension:
 *   .ppm  — binary PPM (P6) is supported.
 *
 * On success:
 *   • RGB channels are normalised to [0, 1].
 *   • Alpha channel is initialised to 1.0 (fully opaque).
 *   • Depth channel is initialised to 1.0 (back plane).
 *
 * Returns NULL if the file cannot be opened, the format is unrecognised,
 * or memory allocation fails.
 */
Image *image_read(char *filename) {
    if (!filename) {
        fprintf(stderr, "[image_read] NULL filename\n");
        return NULL;
    }

    /* Determine format from the file extension */
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        fprintf(stderr, "[image_read] No extension in filename: %s\n",
                filename);
        return NULL;
    }

    /* ---- PPM binary (P6) reader ---- */
    if (strcmp(ext, ".ppm") == 0) {
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            fprintf(stderr, "[image_read] Cannot open file: %s\n", filename);
            return NULL;
        }

        /* Read the three-line PPM header, skipping comment lines */
        char magic[8] = {0};
        int  cols = 0, rows = 0, maxval = 0;
        int  scanned = 0;

        /* Parse magic number */
        if (fscanf(fp, "%7s", magic) != 1) {
            fprintf(stderr, "[image_read] Failed to read PPM magic\n");
            fclose(fp); return NULL;
        }
        if (strcmp(magic, "P6") != 0) {
            fprintf(stderr, "[image_read] Unsupported PPM format: %s "
                            "(only P6 is supported)\n", magic);
            fclose(fp); return NULL;
        }

        /*
         * Skip comment lines (lines starting with '#') between header
         * tokens.  This handles well-formatted PPM files with comment
         * metadata.
         */
        {
            int ch;
            /* Skip whitespace then check for comment lines */
            do {
                ch = fgetc(fp);
            } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

            while (ch == '#') {
                /* consume the rest of the comment line */
                while ((ch = fgetc(fp)) != '\n' && ch != EOF);
                /* skip whitespace before next token */
                do { ch = fgetc(fp); }
                while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
            }
            ungetc(ch, fp);
        }

        scanned = fscanf(fp, "%d %d %d", &cols, &rows, &maxval);
        if (scanned != 3 || cols <= 0 || rows <= 0 || maxval <= 0) {
            fprintf(stderr, "[image_read] Invalid PPM header in %s\n",
                    filename);
            fclose(fp); return NULL;
        }

        /* Consume the single whitespace character after maxval */
        fgetc(fp);

        /* Allocate the Image */
        Image *img = image_create(rows, cols);
        if (!img) {
            fclose(fp); return NULL;
        }

        img->maxval = maxval;
        img->o      = ORIGIN_UPPER_LEFT;
        img->d      = DEPTH_GREATER;

        /* Store the filename for reference */
        img->filename = (char *)malloc(strlen(filename) + 1);
        if (img->filename) strcpy(img->filename, filename);

        /* Read binary pixel data row by row */
        float scale = 1.0f / (float)maxval;
        size_t n    = (size_t)rows * (size_t)cols;

        for (size_t i = 0; i < n; i++) {
            unsigned char rgb[3];
            if (fread(rgb, 1, 3, fp) != 3) {
                fprintf(stderr, "[image_read] Unexpected EOF in %s at "
                                "pixel %zu\n", filename, i);
                image_free(img);
                fclose(fp);
                return NULL;
            }
            /* Normalise to [0, 1] */
            img->data[i].rgb[0] = (float)rgb[0] * scale;
            img->data[i].rgb[1] = (float)rgb[1] * scale;
            img->data[i].rgb[2] = (float)rgb[2] * scale;
            img->data[i].z      = 1.0f;   /* FPixel depth field */
            img->alpha[i]       = 1.0f;   /* fully opaque       */
            img->depth[i]       = 1.0f;   /* back plane default */
        }

        fclose(fp);
        return img;
    }

    fprintf(stderr, "[image_read] Unsupported file extension: %s\n", ext);
    return NULL;
}

/*
 * image_write — save an Image to disk in PPM P6 format.
 *
 * The format is inferred from the filename extension (same as image_read).
 * Floating-point channels are clamped to [0,1] and scaled to [0,255]
 * (or [0, src->maxval] if a different maxval has been set).
 *
 * Returns 0 on success, non-zero on failure.
 */
int image_write(Image *src, char *filename) {
    if (!src || !src->data) {
        fprintf(stderr, "[image_write] NULL Image or NULL data\n");
        return 1;
    }
    if (!filename) {
        fprintf(stderr, "[image_write] NULL filename\n");
        return 1;
    }

    const char *ext = strrchr(filename, '.');
    if (!ext) {
        fprintf(stderr, "[image_write] No extension in filename: %s\n",
                filename);
        return 1;
    }

    /* ---- PPM binary (P6) writer ---- */
    if (strcmp(ext, ".ppm") == 0) {
        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            fprintf(stderr, "[image_write] Cannot open for writing: %s\n",
                    filename);
            return 1;
        }

        int maxval = (src->maxval > 0) ? src->maxval : 255;

        /* Write PPM header */
        fprintf(fp, "P6\n%d %d\n%d\n", src->cols, src->rows, maxval);

        /* Write pixel data in upper-left order (internal memory order) */
        float scale = (float)maxval;
        size_t n    = (size_t)src->rows * (size_t)src->cols;

        for (size_t i = 0; i < n; i++) {
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(clamp01(src->data[i].rgb[0]) * scale);
            rgb[1] = (unsigned char)(clamp01(src->data[i].rgb[1]) * scale);
            rgb[2] = (unsigned char)(clamp01(src->data[i].rgb[2]) * scale);
            fwrite(rgb, 1, 3, fp);
        }

        fclose(fp);
        return 0;
    }

    fprintf(stderr, "[image_write] Unsupported file extension: %s\n", ext);
    return 1;
}


/* -----------------------------------------------------------------------
 * Utility / fill functions
 * ----------------------------------------------------------------------- */

/*
 * image_reset — clear all pixels to the default state:
 *   RGB = (0, 0, 0)  (black)
 *   alpha = 1.0      (fully opaque)
 *   depth = 1.0      (back plane)
 *
 * This is the standard "clear frame" operation called before rendering.
 */
void image_reset(Image *src) {
    if (!src || !src->data) return;

    /* Use fill helpers so logic stays in one place */
    FPixel black = {{0.0f, 0.0f, 0.0f}, 1.0f};
    image_fill(src, black);   /* sets RGB + FPixel.z */
    image_filla(src, 1.0f);   /* sets alpha array    */
    image_fillz(src, 1.0f);   /* sets depth array    */
}

/*
 * image_fill — broadcast an FPixel value to every pixel in the image.
 *
 * The FPixel.z field is also propagated to the separate depth array so
 * that both stores are consistent.
 */
void image_fill(Image *src, FPixel val) {
    if (!src || !src->data) return;

    size_t n = (size_t)src->rows * (size_t)src->cols;
    for (size_t i = 0; i < n; i++) {
        src->data[i] = val;
    }
    /* Keep depth array in sync with the z field of the FPixel */
    if (src->depth) {
        for (size_t i = 0; i < n; i++) {
            src->depth[i] = val.z;
        }
    }
}

/*
 * image_fillrgb — set the RGB channels of every pixel to (r, g, b).
 *
 * Alpha and depth are left unchanged.  Values are not clamped here;
 * it is the caller's responsibility to supply values in [0, 1].
 */
void image_fillrgb(Image *src, float r, float g, float b) {
    if (!src || !src->data) return;

    size_t n = (size_t)src->rows * (size_t)src->cols;
    for (size_t i = 0; i < n; i++) {
        src->data[i].rgb[0] = r;
        src->data[i].rgb[1] = g;
        src->data[i].rgb[2] = b;
    }
}

/*
 * image_filla — set the alpha channel of every pixel to a.
 *
 * a is clamped to [0, 1] before writing.
 */
void image_filla(Image *src, float a) {
    if (!src || !src->alpha) return;

    a = clamp01(a);
    size_t n = (size_t)src->rows * (size_t)src->cols;
    for (size_t i = 0; i < n; i++) {
        src->alpha[i] = a;
    }
}

/*
 * image_fillz — set the depth channel of every pixel to z.
 *
 * Both the depth array and FPixel.z are updated to keep them consistent.
 */
void image_fillz(Image *src, float z) {
    if (!src || !src->data) return;

    size_t n = (size_t)src->rows * (size_t)src->cols;
    for (size_t i = 0; i < n; i++) {
        src->data[i].z = z;
        if (src->depth) src->depth[i] = z;
    }
}

/*
 * image_fillColor — broadcast a Color to every pixel's RGB channels.
 *
 * Convenience wrapper around image_fillrgb.  Alpha and depth unchanged.
 */
void image_fillColor(Image *src, Color val) {
    image_fillrgb(src, val.c[0], val.c[1], val.c[2]);
}


/* -----------------------------------------------------------------------
 * Per-pixel access functions (FPixel level)
 * ----------------------------------------------------------------------- */

/*
 * image_getf — retrieve the FPixel at API position (r, c).
 *
 * Returns a black FPixel with default z if the coordinates are invalid.
 */
FPixel image_getf(Image *src, int r, int c) {
    FPixel p = {{0.0f, 0.0f, 0.0f}, 1.0f}; /* safe default */
    if (!src || !src->data) return p;
    if (!valid_rc(src, r, c))  return p;

    int    rm  = map_row(src, r);
    size_t idx = INDEX(src, rm, c);
    return src->data[idx];
}

/*
 * image_setf — write an FPixel into the image at API position (r, c).
 *
 * RGB values are clamped to [0, 1].  FPixel.z is written to the data
 * array but NOT propagated to the depth array; call image_setz separately
 * if you need to update the depth buffer.
 */
void image_setf(Image *src, int r, int c, FPixel val) {
    if (!src || !src->data) return;
    if (!valid_rc(src, r, c))  return;

    /* Clamp RGB channels */
    val.rgb[0] = clamp01(val.rgb[0]);
    val.rgb[1] = clamp01(val.rgb[1]);
    val.rgb[2] = clamp01(val.rgb[2]);

    int    rm  = map_row(src, r);
    size_t idx = INDEX(src, rm, c);
    src->data[idx] = val;
}


/* -----------------------------------------------------------------------
 * Per-pixel access functions (individual channels)
 * ----------------------------------------------------------------------- */

/*
 * image_getb — generalised single-channel read.
 *
 * Band index mapping:
 *   0 = red   (from FPixel.rgb[0])
 *   1 = green (from FPixel.rgb[1])
 *   2 = blue  (from FPixel.rgb[2])
 *   3 = alpha (from the separate alpha array)
 *   4 = depth (from the separate depth array)
 *
 * Returns 0.0f for invalid coordinates or unknown band indices.
 *
 * This is a primary method — all other per-channel getters delegate here.
 */
float image_getb(Image *src, int r, int c, int b) {
    if (!src || !src->data) return 0.0f;
    if (!valid_rc(src, r, c))  return 0.0f;

    int    rm  = map_row(src, r);
    size_t idx = INDEX(src, rm, c);

    switch (b) {
        case 0: return clamp01(src->data[idx].rgb[0]);          /* R     */
        case 1: return clamp01(src->data[idx].rgb[1]);          /* G     */
        case 2: return clamp01(src->data[idx].rgb[2]);          /* B     */
        case 3: return src->alpha ? clamp01(src->alpha[idx])    /* alpha */
                                  : 1.0f;
        case 4: return src->depth ? src->depth[idx] : 1.0f;     /* depth */
        default:
            fprintf(stderr, "[image_getb] Unknown band index: %d\n", b);
            return 0.0f;
    }
}

/*
 * image_setb — generalised single-channel write.
 *
 * Same band-index mapping as image_getb.  For band 4 (depth) both the
 * depth array and FPixel.z are updated to keep the two depth stores in
 * sync.  RGB channels (0-2) are clamped; alpha is clamped; depth is not.
 *
 * This is a primary method — all other per-channel setters delegate here.
 */
void image_setb(Image *src, int r, int c, int b, float val) {
    if (!src || !src->data) return;
    if (!valid_rc(src, r, c))  return;

    int    rm  = map_row(src, r);
    size_t idx = INDEX(src, rm, c);

    switch (b) {
        case 0: src->data[idx].rgb[0] = clamp01(val);   break; /* R     */
        case 1: src->data[idx].rgb[1] = clamp01(val);   break; /* G     */
        case 2: src->data[idx].rgb[2] = clamp01(val);   break; /* B     */
        case 3:                                                  /* alpha */
            if (src->alpha) src->alpha[idx] = clamp01(val);
            break;
        case 4:                                                  /* depth */
            if (src->depth) src->depth[idx] = val;
            src->data[idx].z = val;   /* keep FPixel.z in sync */
            break;
        default:
            fprintf(stderr, "[image_setb] Unknown band index: %d\n", b);
            break;
    }
}

/*
 * image_getc — return a single RGB band value at pixel (r, c).
 *
 * b must be in {0, 1, 2}.  Delegates to image_getb.
 */
float image_getc(Image *src, int r, int c, int b) {
    if (b < 0 || b > 2) {
        fprintf(stderr, "[image_getc] Band index %d out of range [0,2]\n", b);
        return 0.0f;
    }
    return image_getb(src, r, c, b);
}

/*
 * image_setc — set a single RGB band value at pixel (r, c).
 *
 * b must be in {0, 1, 2}.  Delegates to image_setb.
 */
void image_setc(Image *src, int r, int c, int b, float val) {
    if (b < 0 || b > 2) {
        fprintf(stderr, "[image_setc] Band index %d out of range [0,2]\n", b);
        return;
    }
    image_setb(src, r, c, b, val);
}

/*
 * image_geta — return the alpha value at pixel (r, c).
 *
 * Delegates to image_getb with band index 3.
 */
float image_geta(Image *src, int r, int c) {
    return image_getb(src, r, c, 3);
}

/*
 * image_seta — set the alpha value at pixel (r, c).
 *
 * Delegates to image_setb with band index 3.
 */
void image_seta(Image *src, int r, int c, float val) {
    image_setb(src, r, c, 3, val);
}

/*
 * image_getz — return the depth (1/z) value at pixel (r, c).
 *
 * Delegates to image_getb with band index 4.
 */
float image_getz(Image *src, int r, int c) {
    return image_getb(src, r, c, 4);
}

/*
 * image_setz — set the depth (1/z) value at pixel (r, c).
 *
 * Delegates to image_setb with band index 4; both the depth array and
 * FPixel.z are updated.
 */
void image_setz(Image *src, int r, int c, float val) {
    image_setb(src, r, c, 4, val);
}


/* -----------------------------------------------------------------------
 * Color-level access functions
 * ----------------------------------------------------------------------- */

/*
 * image_setColor — write a Color into the RGB channels of pixel (r, c).
 *
 * Alpha and depth are not modified.  Colour components are clamped to
 * [0, 1] via image_setb.
 */
void image_setColor(Image *src, int r, int c, Color val) {
    if (!src || !src->data) return;
    if (!valid_rc(src, r, c))  return;

    image_setb(src, r, c, 0, val.c[0]);
    image_setb(src, r, c, 1, val.c[1]);
    image_setb(src, r, c, 2, val.c[2]);
}

/*
 * image_getColor — build a Color from the RGB channels of pixel (r, c).
 *
 * Returns a black Color for invalid coordinates.
 */
Color image_getColor(Image *src, int r, int c) {
    Color col = {{0.0f, 0.0f, 0.0f}};
    if (!src || !src->data) return col;
    if (!valid_rc(src, r, c))  return col;

    col.c[0] = image_getb(src, r, c, 0);
    col.c[1] = image_getb(src, r, c, 1);
    col.c[2] = image_getb(src, r, c, 2);
    return col;
}


/* -----------------------------------------------------------------------
 * Utility / query functions
 * ----------------------------------------------------------------------- */

/*
 * image_getRows — return the number of rows (height) in the image.
 */
int image_getRows(const Image *src) {
    return src ? src->rows : 0;
}

/*
 * image_getCols — return the number of columns (width) in the image.
 */
int image_getCols(const Image *src) {
    return src ? src->cols : 0;
}

/*
 * image_copy — create a deep copy of the Image.
 *
 * All pixel arrays (data, alpha, depth) are freshly allocated and copied.
 * Metadata fields (origin, depth type, maxval, filename) are also
 * duplicated.  Returns NULL if src is NULL or allocation fails.
 */
Image *image_copy(const Image *src) {
    if (!src) return NULL;

    /* Create a new empty image of the same size */
    Image *dst = image_create(src->rows, src->cols);
    if (!dst) return NULL;

    /* Copy pixel arrays */
    size_t n = (size_t)src->rows * (size_t)src->cols;
    if (src->data && dst->data)
        memcpy(dst->data,  src->data,  n * sizeof(FPixel));
    if (src->alpha && dst->alpha)
        memcpy(dst->alpha, src->alpha, n * sizeof(float));
    if (src->depth && dst->depth)
        memcpy(dst->depth, src->depth, n * sizeof(float));

    /* Copy metadata */
    dst->o      = src->o;
    dst->d      = src->d;
    dst->maxval = src->maxval;

    if (src->filename) {
        dst->filename = (char *)malloc(strlen(src->filename) + 1);
        if (dst->filename) strcpy(dst->filename, src->filename);
    }

    return dst;
}

/*
 * image_setOrigin — change the coordinate-system origin for the image.
 *
 * Existing pixel data is NOT rearranged; this only changes how future
 * API row indices are interpreted by map_row().
 */
void image_setOrigin(Image *src, Origin o) {
    if (src) src->o = o;
}

/*
 * image_setDepthType — change the depth comparison direction.
 *
 * This is stored for the rasteriser (e.g. image_depthTest) to use.
 */
void image_setDepthType(Image *src, DepthType d) {
    if (src) src->d = d;
}

/*
 * image_depthTest — determine whether a candidate depth value passes the
 * current depth test for pixel (r, c).
 *
 * When src->d == DEPTH_GREATER (default for 1/z storage), a larger
 * incoming value is closer to the viewer and wins.
 * When src->d == DEPTH_LESS, a smaller incoming value wins.
 *
 * Returns 1 (true)  if z passes the test (new value is closer).
 * Returns 0 (false) if z fails  the test (stored value is at least as
 *                   close, so the pixel should not be updated).
 *
 * Does not modify any image data.
 */
int image_depthTest(Image *src, int r, int c, float z) {
    if (!src || !valid_rc(src, r, c)) return 0;

    float stored = image_getz(src, r, c);

    if (src->d == DEPTH_GREATER) return z > stored;  /* larger 1/z = closer */
    else                          return z < stored;  /* smaller  z = closer */
}

/*
 * image_print — print a short human-readable summary of the Image to
 * stdout.  Useful for debugging and unit tests.
 *
 * Example output:
 *   [myImage] Image 640x480  origin=UPPER_LEFT  depth=GREATER  maxval=255
 */
void image_print(const Image *src, const char *label) {
    const char *lbl = label ? label : "Image";
    if (!src) {
        printf("[%s] (null)\n", lbl);
        return;
    }
    printf("[%s] Image %dx%d  origin=%s  depth=%s  maxval=%d  file=%s\n",
           lbl,
           src->cols, src->rows,
           src->o == ORIGIN_UPPER_LEFT ? "UPPER_LEFT" : "LOWER_LEFT",
           src->d == DEPTH_GREATER     ? "GREATER"    : "LESS",
           src->maxval,
           src->filename ? src->filename : "(none)");
}
