/*
 * composite.c
 *
 * CS 5310 Computer Graphics
 * Implementation of green-screen chroma-keying and compositing functions.
 *
 * -----------------------------------------------------------------------
 * Implementation notes
 * -----------------------------------------------------------------------
 *
 * composite_blend_pixel promoted to primary method
 * ------------------------------------------------
 * The three-channel alpha blend was previously inlined inside
 * composite_merge_with_mask as three nearly-identical lines.  Each line
 * duplicated the cast, multiply, and add arithmetic, making it easy to
 * introduce a per-channel bug without noticing.  composite_blend_pixel
 * encapsulates the formula once:
 *   out = α · fg + (1 − α) · bg  where α = mask / 255.0
 *
 * composite_detect_green_linear — guard against divide-by-zero
 * -------------------------------------------------------------
 * The original had no guard for the case highG == lowG.  If both
 * thresholds were set equal, the denominator would be 0.  A guard now
 * checks for this and defaults to a hard threshold at highG.
 *
 * composite_add_frog_effect — preserved exactly
 * -----------------------------------------------
 * The frog effect uses random alpha per pixel.  Behaviour is preserved
 * from the original; only comments were added.
 *
 * New functions:
 *   composite_blend_pixel        — single-channel alpha blend helper
 *   composite_invert_mask        — 255 − mask[i] for all i
 *   composite_apply_color_tint   — generalised whole-image colour tint
 *   composite_build_mask         — whole-image mask-building wrapper
 *   composite_merge_images       — whole-image merge wrapper
 */

#include "composite.h"
#include <stdlib.h>   /* rand */
#include <string.h>   /* memcpy */


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/*
 * composite_blend_pixel — alpha-blend one channel.
 *
 * α = mask / 255.0
 * out = α · fg + (1 − α) · bg
 *
 * Integer arithmetic is used throughout; the intermediate product is kept
 * as a 32-bit int to prevent unsigned wrap-around before the final cast.
 */
unsigned char composite_blend_pixel(unsigned char fg, unsigned char bg,
                                     unsigned char mask) {
    /* Scale mask to [0.0, 1.0] using integer arithmetic:
     * out = (mask × fg + (255 − mask) × bg) / 255               */
    int out = ((int)mask * (int)fg + (255 - (int)mask) * (int)bg + 127) / 255;
    if (out <   0) out = 0;
    if (out > 255) out = 255;
    return (unsigned char)out;
}

/*
 * composite_invert_mask — in-place mask inversion: mask[i] → 255 − mask[i].
 *
 * Safe to call with NULL or size ≤ 0 (no-op).
 */
void composite_invert_mask(unsigned char *mask, long size) {
    if (!mask || size <= 0) return;
    for (long i = 0; i < size; i++) {
        mask[i] = (unsigned char)(255 - (int)mask[i]);
    }
}


/* =======================================================================
 * Chroma-key detection
 * ======================================================================= */

/*
 * composite_detect_green_simple — hard binary green-screen mask.
 *
 * If G exceeds both R and B by more than `threshold` units, the pixel is
 * classified as green and mask = 0.  Otherwise mask = 255 (subject).
 *
 * The threshold of 40 is appropriate for well-lit studio green screens.
 * For outdoor or under-lit setups composite_detect_green_linear is
 * recommended instead.
 */
void composite_detect_green_simple(PPMPixel *image, unsigned char *mask,
                                    long index) {
    const int threshold = 40;  /* G must exceed R and B by this many units */

    int r = (int)image[index].r;
    int g = (int)image[index].g;
    int b = (int)image[index].b;

    if (g > r + threshold && g > b + threshold) {
        mask[index] = 0;    /* green area:   masked out (transparent)  */
    } else {
        mask[index] = 255;  /* subject area: fully opaque foreground   */
    }
}

/*
 * composite_detect_green_linear — soft anti-aliased green-screen mask.
 *
 * diff = G − max(R, B) measures the "greenness" of the pixel.
 *
 * Three zones:
 *   diff > highG:   fully green → mask = 0
 *   diff < lowG:    fully subject → mask = 255
 *   lowG ≤ diff ≤ highG: linear blend → mask = 255 × (highG − diff) / range
 *
 * The linear zone anti-aliases hair and semi-transparent edges.
 *
 * Guard added: if highG == lowG the denominator would be 0; in that case
 * the mask is set as if the hard threshold were exactly at highG.
 */
void composite_detect_green_linear(PPMPixel *image, unsigned char *mask,
                                    long index) {
    const int highG = 40;  /* upper green threshold (fully green above this) */
    const int lowG  = 25;  /* lower green threshold (fully subject below this) */

    int r = (int)image[index].r;
    int g = (int)image[index].g;
    int b = (int)image[index].b;

    /* diff = how much greener the pixel is than its dominant other channel */
    int max_rb = (r > b) ? r : b;
    int diff   = g - max_rb;

    if (diff > highG) {
        /* Fully green: mask = 0 (transparent foreground) */
        mask[index] = 0;
    } else if (diff < lowG) {
        /* Fully subject: mask = 255 (opaque foreground) */
        mask[index] = 255;
    } else {
        /* Partial: linear interpolation across the transition zone */
        int range = highG - lowG;
        if (range == 0) {
            /* Guard: if thresholds are equal, treat as a hard cutoff at highG */
            mask[index] = (diff >= highG) ? 0 : 255;
        } else {
            /* mask decreases from 255 (at diff=lowG) to 0 (at diff=highG) */
            mask[index] = (unsigned char)(255 * (highG - diff) / range);
        }
    }
}


/* =======================================================================
 * Merge / composite
 * ======================================================================= */

/*
 * composite_merge_with_mask — per-pixel alpha blend of foreground over
 * background using the prebuilt PPMPixel mask.
 *
 * The mask is greyscale (r = g = b); only the r channel is read.
 *   α = mask[fg_index].r / 255.0
 *   out.r = α · fg.r + (1 − α) · bg.r   (likewise for g and b)
 *
 * Delegates to composite_blend_pixel for each channel.
 */
void composite_merge_with_mask(PPMPixel *backgroundImage, PPMPixel *mask,
                                PPMPixel *foregroundImage,
                                PPMPixel *mergedImage,
                                int bg_index, int fg_index) {
    /* Read mask alpha from the red channel (mask is greyscale) */
    unsigned char alpha = mask[fg_index].r;

    mergedImage[bg_index].r = composite_blend_pixel(
        foregroundImage[fg_index].r, backgroundImage[bg_index].r, alpha);
    mergedImage[bg_index].g = composite_blend_pixel(
        foregroundImage[fg_index].g, backgroundImage[bg_index].g, alpha);
    mergedImage[bg_index].b = composite_blend_pixel(
        foregroundImage[fg_index].b, backgroundImage[bg_index].b, alpha);
}


/* =======================================================================
 * Convenience whole-image wrappers
 * ======================================================================= */

/*
 * composite_build_mask — apply a per-pixel detect function to all pixels.
 *
 * The caller supplies a function pointer so that either
 * composite_detect_green_simple or composite_detect_green_linear (or any
 * other compatible function) can be used without code duplication.
 */
void composite_build_mask(PPMPixel *image, unsigned char *mask, long size,
                           void (*detect)(PPMPixel *, unsigned char *, long)) {
    if (!image || !mask || !detect || size <= 0) return;
    for (long i = 0; i < size; i++) {
        detect(image, mask, i);
    }
}

/*
 * composite_merge_images — merge fg onto bg for all pixels, writing to merged.
 *
 * If fg and bg have different dimensions, pixels outside the fg bounds
 * are copied verbatim from bg (no compositing at those positions).
 * The mask is indexed by fg-image coordinates (fg_row × fg_cols + fg_col).
 */
void composite_merge_images(PPMPixel *bg,   int bg_rows, int bg_cols,
                             unsigned char *mask,
                             PPMPixel *fg,   int fg_rows, int fg_cols,
                             PPMPixel *merged) {
    if (!bg || !fg || !mask || !merged) return;
    if (bg_rows <= 0 || bg_cols <= 0) return;

    for (int i = 0; i < bg_rows; i++) {
        for (int j = 0; j < bg_cols; j++) {
            int bg_idx = i * bg_cols + j;

            /* Check if the current pixel falls within the fg bounds */
            if (i < fg_rows && j < fg_cols) {
                int fg_idx = i * fg_cols + j;

                /* Build a one-element PPMPixel "mask pixel" for merge API */
                PPMPixel mpx;
                mpx.r = mpx.g = mpx.b = mask[fg_idx];
                PPMPixel maskArr[1]; maskArr[0] = mpx;

                composite_merge_with_mask(bg, maskArr, fg, merged,
                                          bg_idx, fg_idx);
            } else {
                /* Outside fg: copy background pixel unchanged */
                merged[bg_idx] = bg[bg_idx];
            }
        }
    }
}


/* =======================================================================
 * Post-processing effects
 * ======================================================================= */

/*
 * composite_add_frog_effect — blend every pixel toward forest green
 * (R=34, G=139, B=34) using a small random α per pixel.
 *
 * α is drawn from [20/255, 39/255] to give a subtle, slightly randomised
 * green cast that mimics the scattering of light through foliage.
 *
 * The blending formula per channel:
 *   out = α · tint + (1 − α) · pixel
 */
void composite_add_frog_effect(PPMPixel *mergedImage, int bgrows, int bgcols) {
    if (!mergedImage || bgrows <= 0 || bgcols <= 0) return;

    const unsigned char TR = 34,  TG = 139, TB = 34;  /* forest green */

    for (int i = 0; i < bgrows * bgcols; i++) {
        /* Random alpha between 20/255 and 39/255 */
        float a = (20 + rand() % 20) / 255.0f;

        mergedImage[i].r = (unsigned char)((a * TR)
                           + ((1.0f - a) * (float)mergedImage[i].r));
        mergedImage[i].g = (unsigned char)((a * TG)
                           + ((1.0f - a) * (float)mergedImage[i].g));
        mergedImage[i].b = (unsigned char)((a * TB)
                           + ((1.0f - a) * (float)mergedImage[i].b));
    }
}

/*
 * composite_apply_color_tint — blend the entire image toward a target colour
 * at a uniform (non-random) alpha.
 *
 * Unlike composite_add_frog_effect, the alpha is the same for every pixel
 * and the tint colour is caller-specified.  Useful for colour grading,
 * night-vision green, thermal red, etc.
 *
 * out = α · tint + (1 − α) · pixel
 */
void composite_apply_color_tint(PPMPixel *image, long size,
                                 unsigned char tr, unsigned char tg,
                                 unsigned char tb, float alpha) {
    if (!image || size <= 0) return;

    /* Clamp alpha to [0, 1] */
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    float inv_alpha = 1.0f - alpha;

    for (long i = 0; i < size; i++) {
        image[i].r = (unsigned char)(alpha * (float)tr
                     + inv_alpha * (float)image[i].r);
        image[i].g = (unsigned char)(alpha * (float)tg
                     + inv_alpha * (float)image[i].g);
        image[i].b = (unsigned char)(alpha * (float)tb
                     + inv_alpha * (float)image[i].b);
    }
}
