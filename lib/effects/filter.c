/*
 * filter.c
 *
 * CS 5310 Computer Graphics
 * Implementation of all per-pixel image filter functions.
 *
 * -----------------------------------------------------------------------
 * Implementation notes
 * -----------------------------------------------------------------------
 *
 * filter_clamp_byte / filter_luminance promoted to primary methods
 * ----------------------------------------------------------------
 * Both were originally inlined at their call sites or computed locally in
 * each function.  Making them primary methods removes duplication across
 * filter_apply_sepia_tone, filter_threshold_green_pixels,
 * filter_apply_greyscale, and the new brightness / contrast helpers.
 *
 * filter_threshold_red_pixels — improved comments
 * ------------------------------------------------
 * The original computation was correct but unexplained.  Every step now
 * has an inline comment relating it to the algorithm description.
 *
 * filter_overlay_sine_wave — x parameter no longer suppressed
 * ------------------------------------------------------------
 * The original silently suppressed x with (void)x.  The implementation
 * still only uses y (the sine depends on the row), but the parameter is
 * now documented as "reserved" so the API is forward-compatible if a
 * 2D Lissajous-style wave is added later.
 *
 * filter_apply_horizontal_ramp — comment fixed
 * ---------------------------------------------
 * The original comment said "color = color * (y / rows)" but the code
 * computed x / cols.  The comment is now correct.
 *
 * New functions
 *   filter_apply_greyscale   — delegates to filter_luminance
 *   filter_apply_brightness  — clamp(channel + offset)
 *   filter_apply_contrast    — scale around midpoint 128
 *   filter_apply_invert      — 255 - channel
 *   filter_apply_box_blur    — 3×3 average via a temporary copy
 */

#include "filter.h"
#include <math.h>
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memcpy       */


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/*
 * filter_clamp_byte — clamp an int to [0, 255].
 *
 * Promoted from an inline ternary expression that was duplicated in
 * filter_overlay_sine_wave, filter_apply_brightness, filter_apply_contrast,
 * and filter_apply_sepia_tone.
 */
int filter_clamp_byte(int v) {
    if (v <   0) return 0;
    if (v > 255) return 255;
    return v;
}

/*
 * filter_luminance — ITU-R BT.601 perceptual luminance.
 *
 *   Y = 0.299·R + 0.587·G + 0.114·B
 *
 * The coefficients emphasise green (most visible to the human eye) over
 * red (moderately visible) and blue (least visible).  Result ∈ [0, 255].
 */
double filter_luminance(const PPMPixel *pixel) {
    return 0.299 * pixel->r + 0.587 * pixel->g + 0.114 * pixel->b;
}


/* =======================================================================
 * Chroma-key / colour selection filters
 * ======================================================================= */

/*
 * filter_threshold_red_pixels — isolate strongly red pixels.
 *
 * Steps:
 *   1. j = R − (G + B)/2  measures how much redder this pixel is than
 *      the average of the other channels.
 *   2. min = min(R, G, B) gives a greyscale base that avoids crushing
 *      dark pixels to pure black.
 *   3. If j > 10 (the pixel is "red enough"), boost R; otherwise
 *      collapse all three channels to min.
 */
void filter_threshold_red_pixels(PPMPixel *image, int index, long imagesize) {
    (void)imagesize;  /* parameter kept for API compatibility */

    int r = (int)image[index].r;
    int g = (int)image[index].g;
    int b = (int)image[index].b;

    /* How much redder than the average of the other two channels? */
    int j = r - (g + b) / 2;

    /* Greyscale base: the minimum channel, halved if it is bright enough
     * to avoid the output appearing washed out. */
    int mn = (g < b) ? g : b;
    mn = (r < mn) ? r : mn;
    mn = (mn < 128) ? mn : mn / 2;

    if (j > 10) {
        /* Red pixel: boost R if it is below 128, keep G and B grey */
        image[index].r = (unsigned char)((r < 128) ? r * 2 : r);
    } else {
        /* Non-red pixel: convert to greyscale using the minimum channel */
        image[index].r = (unsigned char)mn;
    }
    /* Both branches set G and B to the grey base */
    image[index].g = (unsigned char)mn;
    image[index].b = (unsigned char)mn;
}

/*
 * filter_threshold_green_pixels — keep green pixels; greyscale the rest.
 *
 * A pixel is "green enough" when its G channel exceeds both R and B by
 * at least greenThreshold units.  All non-green pixels are converted to
 * greyscale using filter_luminance (ITU-R BT.601).
 */
void filter_threshold_green_pixels(PPMPixel *image, int index, long imagesize) {
    (void)imagesize;

    const int greenThreshold = 20;  /* how much G must exceed R and B */

    int r = (int)image[index].r;
    int g = (int)image[index].g;
    int b = (int)image[index].b;

    if (g > r + greenThreshold && g > b + greenThreshold) {
        /* Green pixel — leave it unchanged */
        return;
    }

    /* Non-green pixel — convert to greyscale */
    unsigned char grey = (unsigned char)filter_luminance(&image[index]);
    image[index].r = grey;
    image[index].g = grey;
    image[index].b = grey;
}


/* =======================================================================
 * Waveform / pattern overlay filters
 * ======================================================================= */

/*
 * filter_overlay_sine_wave — add a row-driven sinusoidal offset to each
 * channel.
 *
 * offset = amplitude · sin(frequency · y · π/180 + phase)
 *
 * The phase argument is in radians; the frequency argument is effectively
 * in "degrees per row" because y is first converted to radians by the
 * × π/180 factor.
 *
 * x is retained for future use (e.g. a 2-D sine pattern).  Using both
 * x and y would allow Lissajous-style interference patterns.
 */
void filter_overlay_sine_wave(PPMPixel *image, int index, int x, int y,
                               int amplitude, double frequency, int phase) {
    (void)x;  /* reserved: currently only y drives the wave */

    /* Convert y from degrees to radians and evaluate the wave */
    int offset = (int)(amplitude * sin(frequency * y * 3.14159265 / 180.0
                                       + (double)phase));

    /* Add the offset to each channel and clamp to [0, 255] */
    image[index].r = (unsigned char)filter_clamp_byte((int)image[index].r + offset);
    image[index].g = (unsigned char)filter_clamp_byte((int)image[index].g + offset);
    image[index].b = (unsigned char)filter_clamp_byte((int)image[index].b + offset);
}


/* =======================================================================
 * Ramp / vignette filters
 * ======================================================================= */

/*
 * filter_apply_horizontal_ramp — multiply all channels by x / cols.
 *
 * The ramp goes from 0.0 at the left edge (x = 0) to nearly 1.0 at the
 * right edge (x = cols − 1).  It darkens the left side of the image.
 *
 * Comment fix: the original comment said "y / rows" but the code (and
 * the intended effect) uses x / cols.
 */
void filter_apply_horizontal_ramp(PPMPixel *image, int index, int x, int cols) {
    /* rampX ∈ [0, 1) — increases from left to right */
    double rampX = (double)x / (double)cols;
    image[index].r = (unsigned char)((double)image[index].r * rampX);
    image[index].g = (unsigned char)((double)image[index].g * rampX);
    image[index].b = (unsigned char)((double)image[index].b * rampX);
}

/*
 * filter_apply_radial_ramp — vignette: bright centre, dark edges.
 *
 * The ramp value at (x, y) is:
 *   distance = sqrt((x − cx)² + (y − cy)²)
 *   maxDist  = sqrt(cx² + cy²)   (corner distance from centre)
 *   ramp     = 1.0 − distance / maxDist
 *
 * At the image centre ramp ≈ 1 (no darkening); at the corners ramp = 0.
 */
void filter_apply_radial_ramp(PPMPixel *image, int index, int x, int y,
                               int cols, int rows) {
    double cx = cols / 2.0;  /* image centre x */
    double cy = rows / 2.0;  /* image centre y */

    double dx = (double)x - cx;
    double dy = (double)y - cy;
    double distance    = sqrt(dx * dx + dy * dy);
    double maxDistance = sqrt(cx * cx + cy * cy);  /* centre to corner */

    /* ramp ∈ [0, 1]: 1 at centre, 0 at the farthest corner */
    double ramp = (maxDistance > 0.0) ? 1.0 - (distance / maxDistance) : 1.0;

    image[index].r = (unsigned char)((double)image[index].r * ramp);
    image[index].g = (unsigned char)((double)image[index].g * ramp);
    image[index].b = (unsigned char)((double)image[index].b * ramp);
}


/* =======================================================================
 * Colour grading filters
 * ======================================================================= */

/*
 * filter_apply_sepia_tone — warm sepia colour grading via a 3×3 matrix.
 *
 * The sepia matrix is:
 *   | 0.393  0.769  0.189 |
 *   | 0.349  0.686  0.168 |
 *   | 0.272  0.534  0.131 |
 *
 * Intermediate values can exceed 255 (e.g. a white pixel gives newR ≈ 351)
 * so each output channel is clamped before being stored.
 *
 * Note: original R, G, B are captured before any modification so that the
 * computation is correct regardless of store-ordering.
 */
void filter_apply_sepia_tone(PPMPixel *image, int index) {
    /* Capture original channels before any modification */
    double r = (double)image[index].r;
    double g = (double)image[index].g;
    double b = (double)image[index].b;

    int newR = (int)(0.393 * r + 0.769 * g + 0.189 * b);
    int newG = (int)(0.349 * r + 0.686 * g + 0.168 * b);
    int newB = (int)(0.272 * r + 0.534 * g + 0.131 * b);

    image[index].r = (unsigned char)filter_clamp_byte(newR);
    image[index].g = (unsigned char)filter_clamp_byte(newG);
    image[index].b = (unsigned char)filter_clamp_byte(newB);
}

/*
 * filter_apply_greyscale — perceptual greyscale using ITU-R BT.601.
 *
 * Delegates luminance calculation to filter_luminance so the formula is
 * defined in one place.
 */
void filter_apply_greyscale(PPMPixel *image, int index) {
    unsigned char grey = (unsigned char)filter_luminance(&image[index]);
    image[index].r = grey;
    image[index].g = grey;
    image[index].b = grey;
}

/*
 * filter_apply_brightness — add a signed offset to every channel.
 *
 * Each channel is clamped to [0, 255] after the addition via
 * filter_clamp_byte to prevent unsigned wrap-around.
 *
 * Positive offset brightens; negative offset darkens.
 */
void filter_apply_brightness(PPMPixel *image, int index, int offset) {
    image[index].r = (unsigned char)filter_clamp_byte((int)image[index].r + offset);
    image[index].g = (unsigned char)filter_clamp_byte((int)image[index].g + offset);
    image[index].b = (unsigned char)filter_clamp_byte((int)image[index].b + offset);
}

/*
 * filter_apply_contrast — scale each channel around the midpoint 128.
 *
 * channel → clamp(128 + factor × (channel − 128))
 *
 * With factor > 1 bright pixels get brighter and dark pixels get darker.
 * With factor ∈ (0, 1) the image tends toward a flat medium grey.
 * Negative factor inverts polarity.
 */
void filter_apply_contrast(PPMPixel *image, int index, double factor) {
    int newR = (int)(128.0 + factor * ((double)image[index].r - 128.0));
    int newG = (int)(128.0 + factor * ((double)image[index].g - 128.0));
    int newB = (int)(128.0 + factor * ((double)image[index].b - 128.0));

    image[index].r = (unsigned char)filter_clamp_byte(newR);
    image[index].g = (unsigned char)filter_clamp_byte(newG);
    image[index].b = (unsigned char)filter_clamp_byte(newB);
}

/*
 * filter_apply_invert — photographic negative: channel → 255 − channel.
 *
 * Because the channels are unsigned bytes and 255 − x is always in [0, 255]
 * for x ∈ [0, 255], no clamping is needed.
 */
void filter_apply_invert(PPMPixel *image, int index) {
    image[index].r = (unsigned char)(255 - (int)image[index].r);
    image[index].g = (unsigned char)(255 - (int)image[index].g);
    image[index].b = (unsigned char)(255 - (int)image[index].b);
}


/* =======================================================================
 * Spatial / neighbourhood filters
 * ======================================================================= */

/*
 * filter_apply_box_blur — 3×3 average (box) blur of the entire image.
 *
 * The function allocates a temporary copy of the original pixel array so
 * that the 9-pixel average at each site reads unmodified source values.
 * Without the copy, pixels written early in the scan would contaminate
 * the convolution of pixels written later ("in-place blur drift").
 *
 * Border pixels (first/last row and column) are left unchanged because
 * their full 3×3 neighbourhood extends outside the image.  Alternative
 * strategies (clamp, wrap, mirror) could be added with an extra parameter.
 *
 * Time complexity: O(rows × cols).
 * Space complexity: O(rows × cols) for the temporary copy.
 */
void filter_apply_box_blur(PPMPixel *image, int rows, int cols) {
    if (!image || rows <= 0 || cols <= 0) return;

    /* Allocate a read-only temporary copy of the source pixels */
    size_t n = (size_t)rows * (size_t)cols;
    PPMPixel *src = (PPMPixel *)malloc(n * sizeof(PPMPixel));
    if (!src) return;  /* allocation failure — leave image unchanged */
    memcpy(src, image, n * sizeof(PPMPixel));

    /* Process only interior pixels (i and j in [1, size−2]) */
    for (int i = 1; i < rows - 1; i++) {
        for (int j = 1; j < cols - 1; j++) {
            int sumR = 0, sumG = 0, sumB = 0;

            /* Accumulate the 9-pixel 3×3 neighbourhood from the copy */
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    int ni = i + di;
                    int nj = j + dj;
                    int idx = ni * cols + nj;
                    sumR += (int)src[idx].r;
                    sumG += (int)src[idx].g;
                    sumB += (int)src[idx].b;
                }
            }

            /* Write the 9-pixel average to the output pixel */
            int out = i * cols + j;
            image[out].r = (unsigned char)(sumR / 9);
            image[out].g = (unsigned char)(sumG / 9);
            image[out].b = (unsigned char)(sumB / 9);
        }
    }

    free(src);
}
