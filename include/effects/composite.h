/*
 * composite.h
 *
 * CS 5310 Computer Graphics
 * Green-screen chroma-keying, alpha-blend compositing, and image effects.
 *
 * -----------------------------------------------------------------------
 * Overview
 * -----------------------------------------------------------------------
 *
 * Compositing — also called "green screen" or "chroma keying" — is the
 * process of cutting a foreground subject out of a uniformly coloured
 * background and pasting it onto a different background image.
 *
 * Pipeline (typical usage)
 * -------------------------
 *   1. For each pixel of the foreground image call one of:
 *        composite_detect_green_simple  — hard binary mask
 *        composite_detect_green_linear  — soft anti-aliased mask
 *      to fill an unsigned char mask array (0 = green, 255 = foreground).
 *
 *   2. For each pixel call composite_merge_with_mask to alpha-blend the
 *      foreground onto the background using the mask as the alpha channel.
 *
 *   3. Optionally apply a post-processing effect such as
 *      composite_add_frog_effect to tint the merged result.
 *
 * Alpha blending formula
 * -----------------------
 *   α = mask / 255.0
 *   out = α · foreground + (1 − α) · background
 *
 * New primary methods added
 * --------------------------
 *   composite_blend_pixel        — perform the alpha-blend for a single
 *                                  channel; promoted from the duplicated
 *                                  inline arithmetic in composite_merge_with_mask.
 *   composite_invert_mask        — invert all mask values (0 ↔ 255); useful
 *                                  when the chroma background should become
 *                                  the foreground.
 *   composite_apply_color_tint   — tint the whole merged image toward a
 *                                  specified RGB colour; generalises the
 *                                  frog-effect blending to any target colour.
 *   composite_build_mask         — convenience wrapper: iterate all pixels
 *                                  and fill a mask array using the chosen
 *                                  detect function.
 *   composite_merge_images       — convenience wrapper: iterate all pixels
 *                                  and merge foreground onto background using
 *                                  the prebuilt mask.
 */

#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "ppm.h"
#include <stdlib.h>  /* size_t, rand */


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/**
 * composite_blend_pixel — compute one channel of an alpha-blend.
 *
 * out = α · fg + (1 − α) · bg
 *
 * where α = mask / 255.0.  The result is rounded to the nearest integer
 * and clamped to [0, 255].
 *
 * Promoted from the inline arithmetic that was duplicated three times
 * inside composite_merge_with_mask.
 *
 * @param fg    foreground channel value [0, 255]
 * @param bg    background channel value [0, 255]
 * @param mask  alpha mask byte [0, 255]; 255 = fully foreground
 * @return      blended channel value [0, 255] as unsigned char
 */
unsigned char composite_blend_pixel(unsigned char fg, unsigned char bg,
                                     unsigned char mask);

/**
 * composite_invert_mask — invert every byte in a mask array in place.
 *
 * mask[i] → 255 − mask[i]
 *
 * Useful when the detected colour region (mask=0) should be the region
 * that is kept rather than the region that is replaced.
 *
 * @param mask  byte array to invert in place (must not be NULL)
 * @param size  number of elements in mask
 */
void composite_invert_mask(unsigned char *mask, long size);


/* =======================================================================
 * Chroma-key detection
 * ======================================================================= */

/**
 * composite_detect_green_simple — binary green-screen mask at one pixel.
 *
 * Tests whether the green channel exceeds both the red and the blue
 * channels by at least a fixed threshold.
 *
 *   if G > R + threshold AND G > B + threshold:  mask = 0   (green area)
 *   else:                                         mask = 255 (subject area)
 *
 * The binary output is suitable for clean, well-lit green screens.  For
 * screens with lighting variation use composite_detect_green_linear.
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param mask   output byte array, same length as image (must not be NULL)
 * @param index  pixel index to process
 */
void composite_detect_green_simple(PPMPixel *image, unsigned char *mask,
                                    long index);

/**
 * composite_detect_green_linear — soft (anti-aliased) green-screen mask.
 *
 * Computes diff = G − max(R, B) and applies a linear ramp between lowG
 * and highG to produce a smooth mask value.
 *
 *   if diff > highG:  mask = 0                                (fully green)
 *   if diff < lowG:   mask = 255                              (fully subject)
 *   else:             mask = 255 × (highG − diff) / (highG − lowG)  (partial)
 *
 * The partial values provide spatial anti-aliasing at the subject boundary
 * and reduce the "crawling edge" artefact visible with the binary mask.
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param mask   output byte array  (must not be NULL)
 * @param index  pixel index to process
 */
void composite_detect_green_linear(PPMPixel *image, unsigned char *mask,
                                    long index);


/* =======================================================================
 * Merge / composite
 * ======================================================================= */

/**
 * composite_merge_with_mask — alpha-blend one foreground pixel onto one
 * background pixel using the pre-built mask.
 *
 * The mask value at fg_index is treated as the alpha channel:
 *   α = mask[fg_index].r / 255.0   (mask is greyscale: r == g == b)
 *   out = α · foreground + (1 − α) · background
 *
 * Result is written to mergedImage[bg_index].
 *
 * @param backgroundImage  background pixel array
 * @param mask             PPMPixel mask array (greyscale; r channel = α)
 * @param foregroundImage  foreground pixel array
 * @param mergedImage      output composite pixel array
 * @param bg_index         pixel index in background and merged images
 * @param fg_index         pixel index in foreground and mask images
 */
void composite_merge_with_mask(PPMPixel *backgroundImage, PPMPixel *mask,
                                PPMPixel *foregroundImage,
                                PPMPixel *mergedImage,
                                int bg_index, int fg_index);


/* =======================================================================
 * Convenience whole-image wrappers
 * ======================================================================= */

/**
 * composite_build_mask — fill a mask array by applying a detect function
 * to every pixel of a foreground image.
 *
 * Iterates all pixels [0, size) and calls detect(image, mask, i) for each.
 * Useful to replace the manual loop in calling code.
 *
 * @param image   flat PPMPixel foreground array (must not be NULL)
 * @param mask    output byte array, length = size (must not be NULL)
 * @param size    total number of pixels
 * @param detect  per-pixel detection function pointer; must not be NULL
 *                — signature: void detect(PPMPixel*, unsigned char*, long)
 */
void composite_build_mask(PPMPixel *image, unsigned char *mask, long size,
                           void (*detect)(PPMPixel *, unsigned char *, long));

/**
 * composite_merge_images — merge a foreground image onto a background
 * image for all pixels, writing results to mergedImage.
 *
 * Background and foreground may differ in size; fg_cols and bg_cols are
 * used to compute per-image flat indices.  Pixels outside the foreground
 * bounds copy the background pixel unchanged.
 *
 * @param bg      background pixel array
 * @param bg_rows number of rows in the background
 * @param bg_cols number of columns in the background
 * @param mask    foreground mask array (length = fg_rows × fg_cols)
 * @param fg      foreground pixel array
 * @param fg_rows number of rows in the foreground
 * @param fg_cols number of columns in the foreground
 * @param merged  output composite array (length = bg_rows × bg_cols)
 */
void composite_merge_images(PPMPixel *bg,   int bg_rows, int bg_cols,
                             unsigned char *mask,
                             PPMPixel *fg,   int fg_rows, int fg_cols,
                             PPMPixel *merged);


/* =======================================================================
 * Post-processing effects
 * ======================================================================= */

/**
 * composite_add_frog_effect — tint the entire image toward forest green
 * (34, 139, 34) using a small random alpha per pixel.
 *
 * Each pixel is blended toward the frog colour with α drawn uniformly
 * from [20/255, 39/255].  The randomness prevents a flat, uniform tint.
 *
 * @param mergedImage  pixel array to tint in place (must not be NULL)
 * @param bgrows       number of rows
 * @param bgcols       number of columns
 */
void composite_add_frog_effect(PPMPixel *mergedImage, int bgrows, int bgcols);

/**
 * composite_apply_color_tint — tint the entire image toward any target
 * colour at a uniform alpha.
 *
 * Generalises composite_add_frog_effect: any (R, G, B) target colour can
 * be blended in with a caller-specified fixed alpha ∈ [0, 1].
 *
 * out[i].channel = (unsigned char)(alpha × tint + (1 − alpha) × pixel.channel)
 *
 * @param image  pixel array to tint in place (must not be NULL)
 * @param size   total number of pixels
 * @param tr     tint red   channel ∈ [0, 255]
 * @param tg     tint green channel ∈ [0, 255]
 * @param tb     tint blue  channel ∈ [0, 255]
 * @param alpha  blend factor ∈ [0, 1]; 0 = no tint, 1 = full tint
 */
void composite_apply_color_tint(PPMPixel *image, long size,
                                 unsigned char tr, unsigned char tg,
                                 unsigned char tb, float alpha);

#endif /* COMPOSITE_H */
