/*
 * filter.h
 *
 * CS 5310 Computer Graphics
 * Per-pixel image filter functions operating on PPMPixel flat arrays.
 *
 * -----------------------------------------------------------------------
 * Design overview
 * -----------------------------------------------------------------------
 *
 * Every filter function in this module operates on one pixel at a time
 * (specified by an integer index into a flat PPMPixel array).  This
 * single-pixel API lets callers apply any filter in any order inside their
 * own loop, combine multiple filters per pixel, or parallelise the loop.
 *
 * Colour representation
 * ----------------------
 * PPMPixel stores R, G, B as unsigned char values in [0, 255].  All
 * intermediate arithmetic uses int or double to avoid overflow, and results
 * are clamped back to [0, 255] before being stored.
 *
 * Chroma-key helpers (filter_threshold_red_pixels /
 *                     filter_threshold_green_pixels)
 * -------------------------------------------------------
 * These detect pixels whose dominant channel exceeds the other channels by
 * a configurable margin.  "Green enough" pixels are preserved; others are
 * converted to greyscale using the ITU-R BT.601 luminance formula:
 *   Y = 0.299·R + 0.587·G + 0.114·B
 *
 * New primary methods added
 * --------------------------
 *   filter_apply_greyscale        — full-image greyscale conversion
 *   filter_apply_brightness       — uniform brightness offset, clamped
 *   filter_apply_contrast         — scale channels around the midpoint 128
 *   filter_apply_invert           — bitwise invert (255 − channel)
 *   filter_apply_box_blur         — 3×3 average blur operating on a
 *                                   temporary copy to avoid in-place errors
 *   filter_clamp_byte             — clamp an int to [0, 255] and cast;
 *                                   promoted from an inline helper to a
 *                                   primary method for reuse across modules
 *   filter_luminance              — compute the ITU-R BT.601 luminance of a
 *                                   single pixel; used by greyscale and
 *                                   chroma-key helpers
 */

#ifndef FILTER_H
#define FILTER_H

#include "ppm.h"


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/**
 * filter_clamp_byte — clamp an integer to the unsigned-byte range [0, 255].
 *
 * Values below 0 map to 0; values above 255 map to 255.  Used by every
 * filter that adds an offset or scales a channel before writing back.
 *
 * @param v  integer to clamp
 * @return   clamped value as int (caller casts to unsigned char)
 */
int filter_clamp_byte(int v);

/**
 * filter_luminance — compute the ITU-R BT.601 perceptual luminance of one
 * pixel.
 *
 *   Y = 0.299·R + 0.587·G + 0.114·B
 *
 * The coefficients weight green more heavily because the human visual
 * system is most sensitive to green light.
 *
 * @param pixel  pointer to the source PPMPixel (must not be NULL)
 * @return       luminance Y ∈ [0.0, 255.0]
 */
double filter_luminance(const PPMPixel *pixel);


/* =======================================================================
 * Chroma-key / colour selection filters
 * ======================================================================= */

/**
 * filter_threshold_red_pixels — isolate strongly red pixels; convert all
 * others to greyscale.
 *
 * Algorithm:
 *   j = R − (G + B) / 2       (how much redder is this pixel than the others?)
 *   min = min(R, G, B)        (find the darkest channel for greyscale base)
 *   If j > 10:
 *     Boost R (double it if R < 128), keep G and B as min.
 *   Else:
 *     Set R = G = B = min     (greyscale using darkest channel)
 *
 * @param image      flat PPMPixel array (must not be NULL)
 * @param index      pixel index to process
 * @param imagesize  total number of pixels (unused; kept for API compat.)
 */
void filter_threshold_red_pixels(PPMPixel *image, int index, long imagesize);

/**
 * filter_threshold_green_pixels — keep pixels that are strongly green;
 * convert all others to greyscale using ITU-R BT.601 luminance.
 *
 * A pixel is "green enough" when:
 *   G > R + greenThreshold  AND  G > B + greenThreshold
 * where greenThreshold defaults to 20.
 *
 * @param image      flat PPMPixel array (must not be NULL)
 * @param index      pixel index to process
 * @param imagesize  total number of pixels (unused; kept for API compat.)
 */
void filter_threshold_green_pixels(PPMPixel *image, int index, long imagesize);


/* =======================================================================
 * Waveform / pattern overlay filters
 * ======================================================================= */

/**
 * filter_overlay_sine_wave — add a sinusoidal intensity offset to every
 * channel of one pixel.
 *
 * The offset is computed from the pixel's y-coordinate (row):
 *   offset = amplitude · sin(frequency · y · π/180 + phase)
 *
 * Each channel is then clamped to [0, 255].
 *
 * @param image      flat PPMPixel array (must not be NULL)
 * @param index      pixel index to process
 * @param x          column of the pixel (currently unused; reserved)
 * @param y          row    of the pixel (drives the sine phase)
 * @param amplitude  peak intensity offset in [-255, 255]
 * @param frequency  wave frequency in degrees-per-row units
 * @param phase      phase offset in radians
 */
void filter_overlay_sine_wave(PPMPixel *image, int index, int x, int y,
                               int amplitude, double frequency, int phase);


/* =======================================================================
 * Ramp / vignette filters
 * ======================================================================= */

/**
 * filter_apply_horizontal_ramp — multiply all channels by a left-to-right
 * ramp that goes from 0.0 (column 0) to 1.0 (column cols−1).
 *
 * rampX = x / cols
 * channel → channel × rampX
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param index  pixel index to process
 * @param x      column of the pixel
 * @param cols   total number of columns in the image
 */
void filter_apply_horizontal_ramp(PPMPixel *image, int index, int x, int cols);

/**
 * filter_apply_radial_ramp — multiply all channels by a radial vignette
 * that is brightest at the image centre and darkens toward the corners.
 *
 * ramp = 1.0 − (distance_from_centre / distance_to_corner)
 * channel → channel × ramp
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param index  pixel index to process
 * @param x      column of the pixel
 * @param y      row    of the pixel
 * @param cols   total number of columns
 * @param rows   total number of rows
 */
void filter_apply_radial_ramp(PPMPixel *image, int index, int x, int y,
                               int cols, int rows);


/* =======================================================================
 * Colour grading filters
 * ======================================================================= */

/**
 * filter_apply_sepia_tone — convert one pixel to a warm sepia-toned colour
 * using the standard sepia matrix.
 *
 * newR = 0.393·R + 0.769·G + 0.189·B   (clamped to 255)
 * newG = 0.349·R + 0.686·G + 0.168·B
 * newB = 0.272·R + 0.534·G + 0.131·B
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param index  pixel index to process
 */
void filter_apply_sepia_tone(PPMPixel *image, int index);

/**
 * filter_apply_greyscale — convert one pixel to luminance-based greyscale.
 *
 * Uses the ITU-R BT.601 formula (filter_luminance) so that the perceived
 * brightness is preserved.  R = G = B = (unsigned char)Y.
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param index  pixel index to process
 */
void filter_apply_greyscale(PPMPixel *image, int index);

/**
 * filter_apply_brightness — add a signed offset to every channel.
 *
 * Each channel is clamped to [0, 255] after the addition.
 * Positive offset brightens; negative offset darkens.
 *
 * @param image   flat PPMPixel array (must not be NULL)
 * @param index   pixel index to process
 * @param offset  signed intensity offset in [-255, 255]
 */
void filter_apply_brightness(PPMPixel *image, int index, int offset);

/**
 * filter_apply_contrast — scale each channel away from the midpoint 128.
 *
 * channel → clamp(128 + factor × (channel − 128))
 *
 * factor > 1.0 increases contrast; factor ∈ (0, 1) decreases contrast;
 * factor = 0 produces a flat grey image; factor < 0 inverts.
 *
 * @param image   flat PPMPixel array (must not be NULL)
 * @param index   pixel index to process
 * @param factor  contrast scale factor (1.0 = no change)
 */
void filter_apply_contrast(PPMPixel *image, int index, double factor);

/**
 * filter_apply_invert — negate every channel: channel → 255 − channel.
 *
 * Produces a photographic negative of the pixel.
 *
 * @param image  flat PPMPixel array (must not be NULL)
 * @param index  pixel index to process
 */
void filter_apply_invert(PPMPixel *image, int index);


/* =======================================================================
 * Spatial / neighbourhood filters (whole-image operations)
 * ======================================================================= */

/**
 * filter_apply_box_blur — apply a 3×3 average (box) blur to the entire
 * image.
 *
 * The blur reads from a temporary copy of the original pixels so that
 * output pixels do not contaminate the convolution of their neighbours.
 * Border pixels are left unchanged (no padding strategy is assumed).
 *
 * @param image  flat PPMPixel array to blur in place (must not be NULL)
 * @param rows   number of rows
 * @param cols   number of columns
 */
void filter_apply_box_blur(PPMPixel *image, int rows, int cols);

#endif /* FILTER_H */
