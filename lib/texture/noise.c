
#include "noise.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif


/* =======================================================================
 * Utility / primary helper methods
 * ======================================================================= */

/*
 * noise_pixel — build an FPixel from (r, g, b) with depth = 1.0.
 *
 * Promoted from static: callers outside noise.c can now use this helper.
 */
FPixel noise_pixel(float r, float g, float b) {
    FPixel color = {{r, g, b}, 1.0f};
    return color;
}

/*
 * noise_clamp01 — clamp v to [0.0, 1.0].
 */
float noise_clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/*
 * noise_normalise — map a raw noise value from (−1, +1) to [0, 1].
 *
 * (v + 1) / 2 centres the range; noise_clamp01 handles the rare case
 * where fBm exceeds ±1 due to constructive octave interference.
 */
float noise_normalise(float v) {
    return noise_clamp01((v + 1.0f) / 2.0f);
}


/* =======================================================================
 * Fractal set renderers
 * ======================================================================= */

/*
 * noise_mandelbrot — escape-time Mandelbrot set renderer.
 *
 * Complex iteration: z ← z² + c, z₀ = 0.
 * Pixel (j, i) maps to c = (x0 + j/cols·dx, y0 + i/rows·dy).
 */
void noise_mandelbrot(Image *dst, float x0, float y0, float dx) {
    if (!dst || dst->rows <= 0 || dst->cols <= 0) return;

    const int maxIter = 1000;

    /* dy is derived from dx so the aspect ratio matches the image */
    float dy = dx * ((float)dst->rows / (float)dst->cols);

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            /* Map pixel (j, i) to the complex plane */
            float c_real = x0 + ((float)j / (float)dst->cols) * dx;
            float c_imag = y0 + ((float)i / (float)dst->rows) * dy;

            /* Iterate z = z² + c starting from z₀ = 0 */
            float z_real = 0.0f, z_imag = 0.0f;
            int iter = 0;
            while (z_real*z_real + z_imag*z_imag <= 4.0f && iter < maxIter) {
                /* z² = (a+bi)² = a²−b² + 2abi */
                float tmp = z_real*z_real - z_imag*z_imag + c_real;
                z_imag    = 2.0f * z_real * z_imag + c_imag;
                z_real    = tmp;
                iter++;
            }

            FPixel color;
            if (iter == maxIter) {
                /* Inside the set: solid blue */
                color = noise_pixel(0.0f, 0.0f, 0.8f);
            } else {
                /* Outside: colour by fractal noise at the escape position */
                float t = (float)iter / (float)maxIter;
                color = noise_fractal_coloring(z_real, z_imag, t);
            }
            image_setf(dst, i, j, color);
        }
    }
}

/*
 * noise_julia — escape-time Julia set renderer.
 *
 * The starting z comes from the pixel position; c is fixed for the whole image.
 */
void noise_julia(Image *dst, float x0, float y0, float dx,
                  float c_real, float c_imag) {
    if (!dst || dst->rows <= 0 || dst->cols <= 0) return;

    const int maxIter = 1000;
    float dy = dx * ((float)dst->rows / (float)dst->cols);

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            /* Map pixel to starting z value on the complex plane */
            float z_real = x0 + ((float)j / (float)dst->cols) * dx;
            float z_imag = y0 + ((float)i / (float)dst->rows) * dy;

            /* Iterate z = z² + c with the fixed Julia constant */
            int iter = 0;
            while (z_real*z_real + z_imag*z_imag <= 4.0f && iter < maxIter) {
                float tmp = z_real*z_real - z_imag*z_imag + c_real;
                z_imag    = 2.0f * z_real * z_imag + c_imag;
                z_real    = tmp;
                iter++;
            }

            FPixel color = noise_pixel(0.0f, 0.0f, 0.0f);
            if (iter == maxIter) {
                /* Inside: dark blue */
                color = noise_pixel(0.0f, 0.0f, 0.6f);
            } else {
                /* Outside: gold-to-pink gradient driven by iteration ratio */
                float t    = (float)iter / (float)maxIter;
                float gold = 0.5f + 0.5f * sinf(3.0f + t * 10.0f);
                if (gold > 0.5f) {
                    /* Gold / amber hues */
                    color.rgb[0] = 1.0f;
                    color.rgb[1] = gold * 0.84f;
                    color.rgb[2] = 0.3f * gold;
                } else {
                    /* Pinkish / violet hues */
                    color.rgb[0] = 0.6f;
                    color.rgb[1] = t * 0.2f;
                    color.rgb[2] = 0.7f;
                }
            }
            image_setf(dst, i, j, color);
        }
    }
}

/*
 * noise_smooth_coloring — normalised iteration count for smooth fractal colour.
 *
 * Eliminates banding by using the fractional part of the escape distance:
 *   mu     = log₂(log₂(|z|))
 *   smooth = iter + 1 − (mu − floor(mu))
 *   t      = smooth / maxIter
 */
FPixel noise_smooth_coloring(float z_real, float z_imag,
                               int iter, int maxIter) {
    float magnitude  = sqrtf(z_real*z_real + z_imag*z_imag);
    float mu         = log2f(log2f(magnitude + 1e-6f));  /* +ε avoids log(0) */
    float smoothVal  = 1.0f - (mu - floorf(mu));
    float t          = ((float)iter + 1.0f - smoothVal) / (float)maxIter;
    t = noise_clamp01(t);

    /* Pink-to-yellow gradient */
    FPixel color;
    color.rgb[0] = 0.6f;
    color.rgb[1] = t;
    color.rgb[2] = 0.5f * (1.0f - t);
    color.z      = 1.0f;
    return color;
}

/*
 * noise_fractal_coloring — colour driven by fractal noise at the escape point.
 *
 * Evaluates 4-octave fBm at (z_real, z_imag), normalises to [0,1], and
 * maps the result to a light-yellow-to-orange gradient.
 */
FPixel noise_fractal_coloring(float z_real, float z_imag, float t) {
    (void)t;  /* reserved for future gradient mixing */
    float n = noise_normalise(noise_fractal(z_real, z_imag, 4, 0.5f));

    FPixel color;
    color.rgb[0] = 0.75f;
    color.rgb[1] = 0.8f * (1.0f - n) + 0.5f * n;  /* light to medium yellow */
    color.rgb[2] = 0.2f * (1.0f - n);              /* fades to 0 for orange  */
    color.z      = 1.0f;
    return color;
}


/* =======================================================================
 * Core noise primitives
 * ======================================================================= */

/*
 * noise_fade — Ken Perlin's quintic smoothstep: 6t⁵ − 15t⁴ + 10t³.
 *
 * Zero first and second derivatives at t=0 and t=1 ensure a C² continuous
 * noise field across all lattice boundaries.
 */
float noise_fade(float t) {
    /* Horner's method for: 6t⁵ − 15t⁴ + 10t³ = t³(t(6t−15)+10) */
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/*
 * noise_lerp — linear interpolation between a and b at parameter t.
 */
float noise_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/*
 * noise_random_gradient — pseudo-random unit gradient at lattice point (ix,iy).
 *
 * Hash (ix, iy) with large prime multipliers and bit-mixing, map to an
 * angle in [0°, 360°), return (cos θ, sin θ).
 */
void noise_random_gradient(int ix, int iy, float *gx, float *gy) {
    if (!gx || !gy) return;
    /* Large-prime multiplicative hash with bit-mixing for good distribution */
    unsigned int h = (unsigned int)ix * 374761393u + (unsigned int)iy * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h =  h ^ (h >> 16);

    /* Map hash to angle in [0, 2π) */
    float angle = (float)(h % 360) * (float)(M_PI / 180.0);
    *gx = cosf(angle);
    *gy = sinf(angle);
}

/*
 * noise_gradient_magnitude — magnitude of the unit gradient at (ix, iy).
 *
 * Because noise_random_gradient produces unit vectors this always returns
 * 1.0.  The function is provided as a primary method for API completeness.
 */
float noise_gradient_magnitude(int ix, int iy) {
    float gx, gy;
    noise_random_gradient(ix, iy, &gx, &gy);
    return sqrtf(gx*gx + gy*gy);  /* always 1.0 for unit gradients */
}

/*
 * noise_perlin — 2-D Perlin gradient noise.
 *
 * Bug fix: the (x1,y1) corner dot product previously duplicated inline
 * arithmetic (sx−1, sy−1) instead of using the named distance variables
 * that were declared but never assigned.  All four corners now follow the
 * same pattern: declare distance variables, compute the dot product.
 */
float noise_perlin(float x, float y) {
    /* Identify the containing lattice cell */
    int x0 = (int)floorf(x);  int x1 = x0 + 1;
    int y0 = (int)floorf(y);  int y1 = y0 + 1;

    /* Fractional offset within the cell */
    float sx = x - (float)x0;
    float sy = y - (float)y0;

    /* Apply the fade curve for smooth interpolation weights */
    float u = noise_fade(sx);
    float v = noise_fade(sy);

    /* Gradient vectors at the four lattice corners */
    float g00x, g00y, g10x, g10y, g01x, g01y, g11x, g11y;
    noise_random_gradient(x0, y0, &g00x, &g00y);
    noise_random_gradient(x1, y0, &g10x, &g10y);
    noise_random_gradient(x0, y1, &g01x, &g01y);
    noise_random_gradient(x1, y1, &g11x, &g11y);

    /* Displacement vectors from each corner to the sample point */
    float dx00 = sx,       dy00 = sy;        /* (x0,y0) → (x,y) */
    float dx10 = sx-1.0f,  dy10 = sy;        /* (x1,y0) → (x,y) */
    float dx01 = sx,       dy01 = sy-1.0f;   /* (x0,y1) → (x,y) */
    float dx11 = sx-1.0f,  dy11 = sy-1.0f;   /* (x1,y1) → (x,y) */

    /* Dot product of gradient and displacement at each corner */
    float d00 = g00x*dx00 + g00y*dy00;
    float d10 = g10x*dx10 + g10y*dy10;
    float d01 = g01x*dx01 + g01y*dy01;
    float d11 = g11x*dx11 + g11y*dy11;

    /* Bilinear interpolation using the faded weights */
    float ix0 = noise_lerp(d00, d10, u);  /* interpolate along bottom edge */
    float ix1 = noise_lerp(d01, d11, u);  /* interpolate along top    edge */
    return noise_lerp(ix0, ix1, v);        /* interpolate between the rows  */
}

/*
 * noise_fractal — fractal Brownian motion (fBm).
 *
 * Sums octaves of Perlin noise with geometrically decreasing amplitude
 * and geometrically increasing frequency.
 */
float noise_fractal(float x, float y, int octaves, float persistence) {
    float value     = 0.0f;
    float amplitude = 1.0f;
    float frequency = 2.0f;  /* start at 2× to avoid the DC component */

    for (int i = 0; i < octaves; i++) {
        value     += noise_perlin(x * frequency, y * frequency) * amplitude;
        amplitude *= persistence;  /* halve amplitude each octave */
        frequency *= 2.0f;         /* double frequency each octave */
    }
    return value;
}

/*
 * noise_turbulence — turbulence via |Perlin| summation.
 *
 * Taking the absolute value introduces sharp "crease" discontinuities that
 * simulate turbulent flow.  Unlike fBm, turbulence values are non-negative.
 */
float noise_turbulence(float x, float y, int octaves) {
    float value     = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;  /* turbulence starts at the base frequency */

    for (int i = 0; i < octaves; i++) {
        value     += fabsf(noise_perlin(x * frequency, y * frequency)) * amplitude;
        amplitude *= 0.5f;   /* fixed 0.5 decay for turbulence */
        frequency *= 2.0f;
    }
    return value;
}


/* =======================================================================
 * Whole-image procedural texture generators
 * ======================================================================= */

/*
 * noise_generate_fractal_image — greyscale fBm image.
 *
 * Each pixel: n = noise_normalise(fBm(x, y)); colour = (n, n, n).
 */
void noise_generate_fractal_image(Image *dst, float scale,
                                   int octaves, float persistence) {
    if (!dst) return;
    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            /* Normalise fBm output to [0,1] */
            float n = noise_normalise(noise_fractal(x, y, octaves, persistence));

            image_setf(dst, i, j, noise_pixel(n, n, n));
        }
    }
}

/*
 * noise_generate_ink_landscape — ink-wash monochromatic landscape.
 *
 * Applies a power curve for contrast then multiplies by a fog layer.
 */
void noise_generate_ink_landscape(Image *dst, float scale,
                                   int octaves, float persistence) {
    if (!dst) return;
    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            /* High-frequency terrain layer */
            float h = noise_normalise(noise_fractal(x, y, octaves, persistence));

            /* Enhance contrast with a power curve (>1 darkens midtones) */
            h = powf(h, 1.8f);

            /* Low-frequency fog layer multiplied in for aerial perspective */
            float fog = noise_normalise(
                            noise_fractal(x * 0.5f, y * 0.5f, 3, 0.5f));
            h = noise_clamp01(h * fog);

            /* Near-black ink palette with a faint blue tint */
            image_setf(dst, i, j, noise_pixel(h*0.95f, h*0.96f, h*0.97f));
        }
    }
}

/*
 * noise_generate_marble_texture — sinusoidal vein marble simulation.
 *
 * sin(x·π + fBm·2) creates the characteristic curved marble veins.
 */
void noise_generate_marble_texture(Image *dst, float scale,
                                    int octaves, float persistence) {
    if (!dst) return;
    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            float n = noise_fractal(x, y, octaves, persistence);

            /* Sine modulation produces the marble vein pattern */
            float marble = noise_clamp01(0.5f * (1.0f + sinf(x * (float)M_PI
                                                              + n * 2.0f)));

            /* White marble palette: near-white with a slight warm cast */
            image_setf(dst, i, j,
                       noise_pixel(marble*0.9f + 0.1f,
                                   marble*0.9f + 0.1f,
                                   marble));
        }
    }
}

/*
 * noise_generate_ripple — concentric ripple rings disturbed by fBm.
 *
 * The ring spacing is controlled by the multiplier (10.0); the fBm term
 * (×5) warps the rings to give organic, water-like distortion.
 */
void noise_generate_ripple(Image *dst, float scale,
                             int octaves, float persistence) {
    if (!dst) return;

    /* Centre of the ripple system in parameter space */
    float centerX = scale / 2.0f;
    float centerY = scale / 2.0f;

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            float n = noise_fractal(x, y, octaves, persistence);

            /* Radial distance from centre, perturbed by noise */
            float rad = sqrtf((x-centerX)*(x-centerX) + (y-centerY)*(y-centerY));
            float ripple = noise_clamp01(0.5f * (1.0f + sinf(rad * 10.0f
                                                              + n * 5.0f)));

            /* Blue water palette */
            image_setf(dst, i, j,
                       noise_pixel(ripple*0.2f, ripple*0.5f, ripple*0.8f));
        }
    }
}

/*
 * noise_generate_fire_texture — hot-colour fire using turbulence.
 *
 * Turbulence is normalised then mapped to a black→red→yellow gradient.
 */
void noise_generate_fire_texture(Image *dst, float scale,
                                  int octaves, float persistence) {
    (void)persistence;  /* turbulence uses a fixed 0.5 decay internally */
    if (!dst) return;

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            /* Normalise turbulence from ≥ 0 to [0, 1] */
            float t = noise_clamp01((noise_turbulence(x, y, octaves) + 1.0f)
                                    / 2.0f);

            FPixel color;
            if (t < 0.5f) {
                /* Dark red / ember region */
                color = noise_pixel(t * 2.0f, t, 0.0f);
            } else {
                /* Bright yellow / flame tip region */
                color = noise_pixel(1.0f, (t - 0.5f) * 2.0f, 0.0f);
            }
            image_setf(dst, i, j, color);
        }
    }
}

/*
 * noise_generate_wood_texture — concentric annual ring pattern via fBm.
 *
 * sin((radial_dist + fBm×10) × π) produces rings with organic variation.
 */
void noise_generate_wood_texture(Image *dst, float scale,
                                  int octaves, float persistence) {
    if (!dst) return;

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            float n = noise_fractal(x, y, octaves, persistence);

            /* Sine over radial distance creates the ring pattern */
            float radial = sqrtf(x*x + y*y);
            float wood   = noise_clamp01(0.5f * (1.0f + sinf(
                               (radial + n * 10.0f) * (float)M_PI)));

            /* Warm brown wood palette */
            image_setf(dst, i, j,
                       noise_pixel(wood*0.6f + 0.4f,
                                   wood*0.4f + 0.2f,
                                   wood*0.2f));
        }
    }
}

/*
 * noise_generate_cloud_texture — soft cloud / sky texture.
 *
 * Low-persistence fBm gives the large, gentle shapes of cumulus clouds.
 * Values above 0.55 are rendered as white/grey cloud; below as sky blue.
 */
void noise_generate_cloud_texture(Image *dst, float scale,
                                   int octaves, float persistence) {
    if (!dst) return;

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            float n = noise_normalise(noise_fractal(x, y, octaves, persistence));

            FPixel color;
            if (n > 0.55f) {
                /* Cloud: scale brightness by (n − 0.55) / 0.45 ∈ [0,1] */
                float cloud = (n - 0.55f) / 0.45f;
                cloud = noise_clamp01(cloud);
                /* White cloud core fading to grey at edges */
                color = noise_pixel(0.85f + cloud*0.15f,
                                    0.87f + cloud*0.13f,
                                    0.90f + cloud*0.10f);
            } else {
                /* Sky: saturated blue lightened by height (y-based tint) */
                float sky = n / 0.55f;  /* ∈ [0,1] inside sky region */
                color = noise_pixel(0.25f + sky*0.15f,
                                    0.50f + sky*0.10f,
                                    0.85f + sky*0.10f);
            }
            image_setf(dst, i, j, color);
        }
    }
}

/*
 * noise_generate_stone_texture — grey stone surface with fine fBm detail.
 *
 * High octave count and low persistence produce the coarse, irregular
 * surface variation of natural stone.  A slight warm tint prevents the
 * texture from reading as purely synthetic grey.
 */
void noise_generate_stone_texture(Image *dst, float scale,
                                   int octaves, float persistence) {
    if (!dst) return;

    for (int i = 0; i < dst->rows; i++) {
        for (int j = 0; j < dst->cols; j++) {
            float x = (float)j / (float)dst->cols * scale;
            float y = (float)i / (float)dst->rows * scale;

            float n = noise_normalise(noise_fractal(x, y, octaves, persistence));

            /* Stone palette: dark-to-light grey with a warm (reddish) tint */
            image_setf(dst, i, j,
                       noise_pixel(n * 0.65f + 0.15f,  /* R: slightly warm */
                                   n * 0.60f + 0.15f,  /* G: medium grey   */
                                   n * 0.55f + 0.15f)); /* B: coolest grey  */
        }
    }
}
