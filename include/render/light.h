#ifndef LIGHT_H
#define LIGHT_H

#include "image.h"     /* Image, Color, FPixel */
#include "primitive.h" /* Point, Vector        */
#include <stdio.h>     /* FILE                 */

/*
 * Maximum number of lights in a single scene.
 * Increasing this number increases the fixed memory footprint of each
 * Lighting struct by (MAX_LIGHTS - 64) × sizeof(Light) bytes.
 */
#define MAX_LIGHTS 64


/* =======================================================================
 * LightType enumeration
 * ======================================================================= */

/**
 * LightType — the category of a light source, controlling which shading
 * equation is applied.
 *
 * LightNone       placeholder / disabled light slot
 * LightAmbient    uniform background illumination (eq. 21)
 * LightDirect     directional (infinite-distance) light; direction fixed
 * LightPoint      omni-directional point source; attenuated by L̂·N̂ (eq. 22)
 * LightSpot       point source with a cone cutoff and angular falloff
 */
typedef enum {
    LightNone,     /* disabled / empty slot                                */
    LightAmbient,  /* ambient:     Ia = CLa · Cb                          */
    LightDirect,   /* directional: fixed L, same diffuse+specular as point */
    LightPoint,    /* point:       I = Cb·CLd·(L̂·N̂) + CLd·Cs·(Ĥ·N̂)^n  */
    LightSpot      /* spot:        point × falloff = α^p, α = −L̂·spotDir  */
} LightType;


/* =======================================================================
 * Light structure
 * ======================================================================= */

/**
 * Light — a single light source with all parameters needed for any type.
 *
 * Fields
 * ------
 * type       which lighting equation to apply (see LightType)
 * color      CLa / CLd — the light's emitted colour in [0, 1]³
 * direction  unit vector pointing FROM the scene TOWARD the light source
 *            used by: LightDirect (fixed L), LightSpot (cone axis)
 * position   world-space origin of the light source
 *            used by: LightPoint, LightSpot
 * cutoff     LightSpot only: cosine of the cone half-angle.
 *            A pixel is within the cone when α = −L̂·direction > cutoff.
 *            Example: cutoff = cos(30°) ≈ 0.866 for a 30° half-angle cone.
 * sharpness  LightSpot only: falloff exponent p in α^p.
 *            Higher values produce a sharper, more concentrated beam.
 */
typedef struct {
    LightType type;      /* light category                                  */
    Color     color;     /* emitted light colour (CLa or CLd)               */
    Vector    direction; /* fixed light direction or spot cone axis          */
    Point     position;  /* world-space position (point and spot lights)     */
    float     cutoff;    /* spot: cosine of cone half-angle                  */
    float     sharpness; /* spot: falloff exponent p                         */
} Light;


/* =======================================================================
 * Lighting container
 * ======================================================================= */

/**
 * Lighting — a fixed-size array of Lights for a scene.
 *
 * Fields
 * ------
 * nLights   number of active lights in light[]; always in [0, MAX_LIGHTS]
 * light[]   the Light objects; indices [0, nLights) are valid
 *
 * The array is fixed-size (no heap allocation) so Lighting can be stack-
 * allocated or embedded in larger structures without a separate malloc.
 */
typedef struct {
    int   nLights;            /* number of active lights ∈ [0, MAX_LIGHTS] */
    Light light[MAX_LIGHTS];  /* the light sources; only [0..nLights) valid */
} Lighting;


/* =======================================================================
 * Light — single-light functions
 * ======================================================================= */

/**
 * light_init — set a Light to default values.
 *
 * Defaults:
 *   type      = LightNone
 *   color     = (0, 0, 0)          — black / off
 *   direction = (0, 0, −1, 0)     — pointing into the scene
 *   position  = (0, 0, 0, 1)      — at the origin
 *   cutoff    = 0.0               — spot cone covers hemisphere
 *   sharpness = 1.0               — linear falloff
 *
 * @param light  Light to initialise (NULL → no-op)
 */
void light_init(Light *light);

/**
 * light_copy — copy all fields of *from into *to.
 *
 * Uses struct assignment so every field (including padding) is copied.
 * No-op if either pointer is NULL.
 *
 * @param to    destination Light
 * @param from  source      Light
 */
void light_copy(Light *to, Light *from);

/**
 * light_print — write a human-readable description of the light to fp.
 *
 * Prints the type, colour, direction, position, cutoff, and sharpness.
 * Useful for debugging scene setup before rendering.
 *
 * @param light  Light to describe (NULL → prints placeholder)
 * @param fp     destination FILE stream (e.g. stdout)
 */
void light_print(const Light *light, FILE *fp);

/**
 * light_isActive — return 1 if the light has a non-None type and a
 * non-zero colour (i.e. it would actually contribute to the scene).
 *
 * Useful for skipping disabled lights in tight inner loops without
 * full switch dispatch.
 *
 * @param light  Light to test
 * @return       1 if active, 0 if disabled or NULL
 */
int light_isActive(const Light *light);


/* =======================================================================
 * Lighting — scene-level functions
 * ======================================================================= */

/**
 * lighting_create — heap-allocate and initialise a new Lighting struct.
 *
 * Returns NULL and prints to stderr if malloc fails.
 *
 * @return  newly allocated Lighting, or NULL on failure
 */
Lighting *lighting_create(void);

/**
 * lighting_delete — free a heap-allocated Lighting struct.
 *
 * Safe to call with NULL (no-op).
 *
 * @param lighting  Lighting to free
 */
void lighting_delete(Lighting *lighting);

/**
 * lighting_init — initialise every Light slot and set nLights to 0.
 *
 * Calls light_init on all MAX_LIGHTS slots so there are no garbage
 * values in unused entries.
 *
 * @param lighting  Lighting to initialise (NULL → no-op)
 */
void lighting_init(Lighting *lighting);

/**
 * lighting_clear — reset nLights to 0 without re-initialising the slots.
 *
 * Faster than lighting_init when the caller just wants to discard all
 * current lights and start fresh.  The existing Light data in the array
 * is left intact but will not be visited by lighting_shading.
 *
 * @param lighting  Lighting to clear (NULL → no-op)
 */
void lighting_clear(Lighting *lighting);

/**
 * lighting_add — append a new light to the Lighting array.
 *
 * Fields that are not applicable to the light type (e.g. direction for
 * LightAmbient) may be passed as NULL.  The light is initialised to
 * defaults first, then the provided values are written, so un-passed
 * optional fields keep safe defaults.
 *
 * Does nothing if nLights == MAX_LIGHTS.
 *
 * @param lighting   scene Lighting structure
 * @param type       type of light to add
 * @param c          light colour (NULL → black)
 * @param direction  fixed direction or cone axis (NULL → default (0,0,-1))
 * @param pos        world-space position (NULL → origin)
 * @param cutoff     spot cone cosine threshold
 * @param sharpness  spot falloff exponent
 */
void lighting_add(Lighting *lighting, LightType type,
                  Color *c, Vector *direction, Point *pos,
                  float cutoff, float sharpness);

/**
 * lighting_count — return the number of active lights.
 *
 * Convenience accessor; avoids direct field access.
 *
 * @param lighting  scene Lighting (NULL → returns 0)
 * @return          number of active lights ∈ [0, MAX_LIGHTS]
 */
int lighting_count(const Lighting *lighting);

/**
 * lighting_print — write all active lights to fp.
 *
 * Iterates over light[0..nLights) and calls light_print for each.
 *
 * @param lighting  Lighting to describe (NULL → prints placeholder)
 * @param fp        destination FILE stream
 */
void lighting_print(const Lighting *lighting, FILE *fp);


/* =======================================================================
 * Per-light shading helpers  (primary methods)
 *
 * Each function computes the contribution of one light and ADDS it to
 * the running colour total *c.  The caller is responsible for zeroing
 * *c before the first call and clamping after the last.
 * ======================================================================= */

/**
 * lighting_apply_ambient — add the ambient contribution to *c.
 *
 * Ia = CLa · Cb    (spec eq. 21, per channel)
 *
 * @param light  LightAmbient source
 * @param Cb     body (diffuse) colour of the surface
 * @param c      running colour accumulator (modified in place)
 */
void lighting_apply_ambient(Light *light, Color *Cb, Color *c);

/**
 * lighting_apply_directional — add the contribution of a directional
 * (infinite-distance) light to *c.
 *
 * Same body+surface equation as a point light (spec eq. 22) but the
 * light direction L is taken directly from light->direction (already
 * a unit vector in world space) rather than computed from a position.
 *
 * Useful for sunlight and other distant-source approximations.
 *
 * @param light     LightDirect source (direction must be normalised)
 * @param N         surface normal      (need not be unit length)
 * @param V         view direction      (need not be unit length)
 * @param Cb        body colour
 * @param Cs        surface (specular) colour
 * @param s         specular exponent n
 * @param oneSided  1 = skip back faces, 0 = flip normal for back faces
 * @param c         running colour accumulator
 */
void lighting_apply_directional(Light *light, Vector *N, Vector *V,
                                 Color *Cb, Color *Cs, float s,
                                 int oneSided, Color *c);

/**
 * lighting_apply_point — add the point-light contribution to *c.
 *
 * I = Cb·CLd·(L̂·N̂) + CLd·Cs·(Ĥ·N̂)^n        (spec eq. 22)
 *
 * L̂ is computed from the light position and surface point P.
 * Ĥ = normalise(L̂ + V̂)                        (spec eq. 23)
 *
 * @param light     LightPoint source
 * @param N         surface normal      (need not be unit length)
 * @param V         view direction      (need not be unit length)
 * @param P         world-space surface position
 * @param Cb        body colour
 * @param Cs        surface (specular) colour
 * @param s         specular exponent n
 * @param oneSided  1 = skip back faces, 0 = flip normal for back faces
 * @param c         running colour accumulator
 */
void lighting_apply_point(Light *light, Vector *N, Vector *V, Point *P,
                           Color *Cb, Color *Cs, float s,
                           int oneSided, Color *c);

/**
 * lighting_apply_spot — add the spotlight contribution to *c.
 *
 * Computes α = dot(−L̂, spotDir).  If α ≤ cutoff the fragment is outside
 * the cone and no contribution is added.  Otherwise the point-light
 * result is scaled by the falloff function α^p.
 *
 * @param light     LightSpot source
 * @param N         surface normal      (need not be unit length)
 * @param V         view direction      (need not be unit length)
 * @param P         world-space surface position
 * @param Cb        body colour
 * @param Cs        surface (specular) colour
 * @param s         specular exponent n
 * @param oneSided  1 = skip back faces, 0 = flip normal for back faces
 * @param c         running colour accumulator
 */
void lighting_apply_spot(Light *light, Vector *N, Vector *V, Point *P,
                          Color *Cb, Color *Cs, float s,
                          int oneSided, Color *c);


/* =======================================================================
 * Geometric helpers  (primary methods)
 * ======================================================================= */

/**
 * lighting_halfway_vector — compute the normalised halfway vector Ĥ.
 *
 * Ĥ = normalise(L + V)                          (spec eq. 23)
 *
 * Both L and V should be unit vectors pointing AWAY from the surface
 * (toward the light and toward the eye respectively).
 *
 * Returns a zero vector if either pointer is NULL.
 *
 * @param L  unit light direction (surface → light)
 * @param V  unit view  direction (surface → eye)
 * @return   normalised halfway vector Ĥ
 */
Vector lighting_halfway_vector(Vector *L, Vector *V);

/**
 * lighting_light_vector — compute the unit vector from surface point P
 * toward the light source position.
 *
 * L̂ = normalise(light->position − P)
 *
 * Extracting this as a primary method avoids duplicating the subtraction
 * and normalisation in lighting_apply_point and lighting_apply_spot.
 *
 * @param light  LightPoint or LightSpot source
 * @param P      world-space surface position
 * @return       unit vector from P toward light->position
 */
Vector lighting_light_vector(const Light *light, const Point *P);


/* =======================================================================
 * Full shading dispatch
 * ======================================================================= */

/**
 * lighting_shading — compute the final surface colour from all lights.
 *
 * Algorithm (spec §11 shading function):
 *   1. Zero *c.
 *   2. For each active light in lighting[], call the appropriate
 *      per-type helper (ambient / directional / point / spot).
 *   3. Clamp every channel of *c to [0, 1].
 *
 * @param lighting  scene Lighting (NULL → *c stays zero)
 * @param N         surface normal at the point (need not be unit length)
 * @param V         view direction at the point  (need not be unit length)
 * @param P         world-space position of the surface point
 * @param Cb        body (diffuse) colour  ∈ [0,1]³
 * @param Cs        surface (specular) colour ∈ [0,1]³
 * @param s         specular shininess exponent n
 * @param oneSided  1 = one-sided (back faces unlit), 0 = two-sided
 * @param c         output colour (must not be NULL; zeroed then written)
 */
void lighting_shading(Lighting *lighting,
                      Vector *N, Vector *V, Point *P,
                      Color *Cb, Color *Cs,
                      float s, int oneSided,
                      Color *c);

#endif /* LIGHT_H */
