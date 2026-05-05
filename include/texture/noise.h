
#ifndef NOISE_H
#define NOISE_H

#include "image.h"   /* Image, FPixel */


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/**
 * noise_pixel — construct an FPixel from three float colour components.
 *
 * The z/depth field is set to 1.0 (back-plane default).  This function
 * was previously a static helper visible only inside noise.c; it is now
 * a public primary method so that any caller can build a procedural colour
 * without duplicating the FPixel initialisation syntax.
 *
 * @param r  red   component ∈ [0, 1]
 * @param g  green component ∈ [0, 1]
 * @param b  blue  component ∈ [0, 1]
 * @return   FPixel with rgb set and z = 1.0
 */
FPixel noise_pixel(float r, float g, float b);

/**
 * noise_clamp01 — clamp a float to the closed interval [0.0, 1.0].
 *
 * Promoted from repeated inline `if (v < 0) v = 0; if (v > 1) v = 1;`
 * pairs that appeared in every texture generator.
 *
 * @param v  float to clamp
 * @return   v clamped to [0, 1]
 */
float noise_clamp01(float v);

/**
 * noise_normalise — map a raw Perlin / fBm value from the approximate
 * range (−1, +1) into [0, 1] using the formula (v + 1) / 2.
 *
 * The result is then clamped to [0, 1] via noise_clamp01 to handle the
 * rare case where fBm exceeds ±1 due to constructive interference of
 * octaves.
 *
 * @param v  raw noise value (expected ∈ (−1, 1))
 * @return   normalised value ∈ [0, 1]
 */
float noise_normalise(float v);


/* =======================================================================
 * Fractal set renderers
 * ======================================================================= */

/**
 * noise_mandelbrot — render the Mandelbrot set into Image *dst.
 *
 * For each pixel (j, i) the complex point c = (x0 + j/cols·dx,
 * y0 + i/rows·dy) is iterated: z ← z² + c, z₀ = 0.  The iteration
 * stops when |z|² > 4 (escaped) or after maxIter = 1000 steps (inside).
 *
 * Inside pixels are coloured solid blue; escaped pixels are coloured by
 * noise_fractal_coloring using the final z value and iteration ratio.
 *
 * @param dst  destination Image (must not be NULL; rows and cols must > 0)
 * @param x0   real part of the top-left complex plane corner
 * @param y0   imaginary part of the top-left complex plane corner
 * @param dx   real-axis width of the complex plane region
 */
void noise_mandelbrot(Image *dst, float x0, float y0, float dx);

/**
 * noise_julia — render a Julia set into Image *dst.
 *
 * Each pixel maps to the starting point z = (x0 + j/cols·dx,
 * y0 + i/rows·dy).  The fixed constant c = (c_real + i·c_imag) is used
 * for all pixels.  Inside pixels are coloured dark blue; escaped pixels
 * receive a gold-to-pink gradient based on the iteration ratio.
 *
 * @param dst     destination Image
 * @param x0      real part of the view's top-left corner
 * @param y0      imaginary part
 * @param dx      real-axis width
 * @param c_real  real part of the Julia constant c
 * @param c_imag  imaginary part of the Julia constant c
 */
void noise_julia(Image *dst, float x0, float y0, float dx,
                  float c_real, float c_imag);

/**
 * noise_smooth_coloring — compute a smooth escape-time colour for a pixel
 * that has escaped the Mandelbrot / Julia set.
 *
 * Uses the "normalised iteration count" technique:
 *   magnitude = |z|
 *   mu        = log₂(log₂(magnitude))
 *   smooth    = iter + 1 − (mu − floor(mu))
 *   t         = smooth / maxIter
 *
 * This eliminates the banding artefact of integer iteration counting by
 * using the sub-iteration fractional escape distance.
 *
 * @param z_real  real part of z at escape time
 * @param z_imag  imaginary part of z at escape time
 * @param iter    number of iterations before escape
 * @param maxIter maximum allowed iterations
 * @return        smooth-coloured FPixel
 */
FPixel noise_smooth_coloring(float z_real, float z_imag,
                               int iter, int maxIter);

/**
 * noise_fractal_coloring — colour an escaped pixel using fractal noise
 * evaluated at the final z position.
 *
 * Evaluates noise_fractal(z_real, z_imag, 4 octaves, 0.5 persistence) to
 * produce a continuous colour that varies with the complex geometry of the
 * fractal boundary.  The t parameter is reserved for future gradient mixing.
 *
 * @param z_real  real part of z at escape time
 * @param z_imag  imaginary part of z at escape time
 * @param t       normalised iteration ratio (currently unused; reserved)
 * @return        fractal-coloured FPixel
 */
FPixel noise_fractal_coloring(float z_real, float z_imag, float t);


/* =======================================================================
 * Core noise primitives
 * ======================================================================= */

/**
 * noise_fade — Ken Perlin's quintic smoothstep fade curve.
 *
 * f(t) = 6t⁵ − 15t⁴ + 10t³
 *
 * This polynomial has zero first and second derivatives at t = 0 and
 * t = 1, which means the noise field has smooth (C²) transitions across
 * lattice boundaries.
 *
 * @param t  parameter ∈ [0, 1]
 * @return   smoothed parameter ∈ [0, 1]
 */
float noise_fade(float t);

/**
 * noise_lerp — linear interpolation between a and b at parameter t.
 *
 * result = a + t · (b − a)
 *
 * When t = 0 the result is a; when t = 1 the result is b.
 *
 * @param a  start value
 * @param b  end   value
 * @param t  blend parameter ∈ [0, 1]
 * @return   interpolated value
 */
float noise_lerp(float a, float b, float t);

/**
 * noise_random_gradient — generate a pseudo-random unit gradient vector
 * at the 2-D integer lattice point (ix, iy).
 *
 * Uses a hash of (ix, iy) with large prime multipliers to produce a
 * uniformly distributed angle θ ∈ [0°, 360°), then computes the unit
 * vector (cos θ, sin θ).  The same (ix, iy) always produces the same
 * gradient (deterministic, not random).
 *
 * @param ix  integer x lattice coordinate
 * @param iy  integer y lattice coordinate
 * @param gx  output: x component of the gradient (set by the function)
 * @param gy  output: y component of the gradient (set by the function)
 */
void noise_random_gradient(int ix, int iy, float *gx, float *gy);

/**
 * noise_gradient_magnitude — return the magnitude of the gradient at the
 * lattice point (ix, iy).
 *
 * Because noise_random_gradient produces unit vectors the magnitude is
 * always 1.0 by definition.  This function is provided as a primary method
 * so that callers working with spatially-varying gradient fields can query
 * the magnitude at any lattice point without duplicating the hash logic.
 *
 * @param ix  integer x lattice coordinate
 * @param iy  integer y lattice coordinate
 * @return    magnitude of the gradient vector at (ix, iy) — always 1.0
 */
float noise_gradient_magnitude(int ix, int iy);

/**
 * noise_perlin — evaluate 2-D Perlin gradient noise at (x, y).
 *
 * Algorithm:
 *   1. Identify the integer lattice cell (x0, y0) containing (x, y).
 *   2. Compute the relative position (sx, sy) = (x − x0, y − y0).
 *   3. Apply the quintic fade curve to (sx, sy) to get (u, v).
 *   4. Compute dot products of the four corner gradients with the
 *      displacement vectors from each corner to (x, y).
 *   5. Bilinear interpolation using (u, v).
 *
 * Returns a value in approximately (−1, +1); the exact range depends
 * on the gradient distribution.
 *
 * @param x  continuous x coordinate
 * @param y  continuous y coordinate
 * @return   noise value ∈ (−1, +1) approximately
 */
float noise_perlin(float x, float y);

/**
 * noise_fractal — fractal Brownian motion (fBm) by summing octaves of
 * Perlin noise.
 *
 * value = Σ_{i=0}^{octaves−1} noise_perlin(x·f, y·f) · a
 *   where f starts at 2 and doubles each octave,
 *         a starts at 1 and is multiplied by persistence each octave.
 *
 * Higher octaves add fine detail; higher persistence makes them louder.
 *
 * @param x           continuous x coordinate
 * @param y           continuous y coordinate
 * @param octaves     number of Perlin layers to sum (typical: 4–8)
 * @param persistence amplitude decay per octave (typical: 0.4–0.6)
 * @return            fBm value (approximately in (−1, +1) for small octave counts)
 */
float noise_fractal(float x, float y, int octaves, float persistence);

/**
 * noise_turbulence — turbulence function via absolute-value Perlin summation.
 *
 * value = Σ_{i=0}^{octaves−1} |noise_perlin(x·f, y·f)| · a
 *
 * Taking the absolute value creates sharp creases in the noise field that
 * resemble turbulent fluid, fire, and cloud edges.
 *
 * @param x       continuous x coordinate
 * @param y       continuous y coordinate
 * @param octaves number of layers (typical: 6–8 for fire/smoke)
 * @return        turbulence value ∈ [0, ∞) (normalise before use)
 */
float noise_turbulence(float x, float y, int octaves);


/* =======================================================================
 * Whole-image procedural texture generators
 * ======================================================================= */

/**
 * noise_generate_fractal_image — fill *dst with a greyscale fBm image.
 *
 * Each pixel's intensity = noise_normalise(noise_fractal(x, y, ...)).
 *
 * @param dst         destination Image (rows × cols)
 * @param scale       number of noise periods across the image width
 * @param octaves     fBm octave count
 * @param persistence fBm amplitude decay per octave
 */
void noise_generate_fractal_image(Image *dst, float scale,
                                   int octaves, float persistence);

/**
 * noise_generate_ink_landscape — monochromatic ink-wash landscape.
 *
 * Uses a high-contrast fBm field combined with a low-frequency fog layer
 * to produce an ink-on-paper aesthetic.  The fog is a second fBm evaluated
 * at half frequency.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale
 * @param octaves     fBm octave count (8 recommended)
 * @param persistence fBm amplitude decay (0.5 recommended)
 */
void noise_generate_ink_landscape(Image *dst, float scale,
                                   int octaves, float persistence);

/**
 * noise_generate_marble_texture — simulate veined marble.
 *
 * Evaluates sin(x·π + noise·2) to create the characteristic sinusoidal
 * vein pattern, then maps the result to a near-white palette.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale
 * @param octaves     fBm octave count (6 recommended)
 * @param persistence fBm amplitude decay (0.5 recommended)
 */
void noise_generate_marble_texture(Image *dst, float scale,
                                    int octaves, float persistence);

/**
 * noise_generate_ripple — concentric ripple pattern disturbed by fBm.
 *
 * Uses sin(radial_distance × 10 + noise × 5) to produce rings centred at
 * the image centre, warped by fractal noise.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale
 * @param octaves     fBm octave count
 * @param persistence fBm amplitude decay
 */
void noise_generate_ripple(Image *dst, float scale,
                             int octaves, float persistence);

/**
 * noise_generate_fire_texture — hot-colour fire using turbulence noise.
 *
 * Maps noise_turbulence to a black → red → yellow colour gradient.
 * Pixels below 0.5 turbulence are in the dark red range; pixels above
 * 0.5 blend from red to bright yellow.
 *
 * @param dst         destination Image
 * @param scale       turbulence frequency scale
 * @param octaves     turbulence octave count (6–8 recommended)
 * @param persistence unused (turbulence uses a fixed 0.5 decay); kept for
 *                    API symmetry with the other generators
 */
void noise_generate_fire_texture(Image *dst, float scale,
                                  int octaves, float persistence);

/**
 * noise_generate_wood_texture — concentric wood-ring pattern via fBm.
 *
 * Evaluates sin((radial_distance + noise × 10) × π) to produce annual
 * rings, then maps to a tan/brown wood palette.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale
 * @param octaves     fBm octave count (6 recommended)
 * @param persistence fBm amplitude decay (0.5 recommended)
 */
void noise_generate_wood_texture(Image *dst, float scale,
                                  int octaves, float persistence);

/**
 * noise_generate_cloud_texture — soft blue-sky cloud texture using smooth fBm.
 *
 * Evaluates noise_fractal with low persistence for gentle, smooth clouds.
 * Noise values above a threshold are coloured white/grey (cloud); below
 * the threshold are coloured sky blue.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale (4–8 recommended)
 * @param octaves     fBm octave count (5 recommended)
 * @param persistence fBm amplitude decay (0.45 recommended for soft clouds)
 */
void noise_generate_cloud_texture(Image *dst, float scale,
                                   int octaves, float persistence);

/**
 * noise_generate_stone_texture — rough grey stone surface using fBm.
 *
 * Uses fBm with high octaves and low persistence to produce a fine,
 * high-frequency surface variation appropriate for cobblestone or granite.
 * The palette varies from dark to light grey with a slight warm tint.
 *
 * @param dst         destination Image
 * @param scale       noise frequency scale (6–10 recommended)
 * @param octaves     fBm octave count (7–8 recommended)
 * @param persistence fBm amplitude decay (0.35–0.45 for rough stone)
 */
void noise_generate_stone_texture(Image *dst, float scale,
                                   int octaves, float persistence);

#endif /* NOISE_H */
