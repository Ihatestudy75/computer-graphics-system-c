
#include "light.h"
#include "color.h"   /* color_set                    */
#include "matrix.h"  /* vector_set, vector_normalize, vector_dot, vector_cross, point_copy, vector_copy */
#include <math.h>    /* pow, sqrt                    */
#include <stdio.h>
#include <stdlib.h>


/* =======================================================================
 * Internal helpers  (static — not visible outside this file)
 * ======================================================================= */

/*
 * light_clampf — clamp a float value to the closed interval [0, 1].
 *
 * Used only in lighting_shading to finalise the accumulated colour.
 * Intermediate per-light additions are intentionally left unclamped so
 * that multiple bright lights can sum beyond 1 before a single final clip.
 */
static inline float light_clampf(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

/*
 * light_color_add — add all three channels of *from into *to in place.
 *
 * Intentionally unclamped so the caller can accumulate contributions from
 * many lights before clamping once at the end.
 *
 * @param to    accumulator (modified)
 * @param from  increment   (read-only)
 */
static void light_color_add(Color *to, const Color *from) {
    if (!to || !from) return;
    to->c[0] += from->c[0];
    to->c[1] += from->c[1];
    to->c[2] += from->c[2];
}

/*
 * light_color_scale_add — compute a[i] * b[i] * scale for each channel
 * and ADD the result into *out.
 *
 * Replaces the original two-function sequence (light_color_scale_channels
 * into a temporary, then light_color_add) with a single pass.  No heap
 * temporary is needed; the operation is done entirely in registers.
 *
 * @param out    accumulator (modified)
 * @param a      first  colour factor
 * @param b      second colour factor  (usually the light colour CLd)
 * @param scale  scalar multiplier     (usually L̂·N̂ or (Ĥ·N̂)^n)
 */
static void light_color_scale_add(Color *out,
                                   const Color *a, const Color *b,
                                   float scale) {
    if (!out || !a || !b) return;
    out->c[0] += a->c[0] * b->c[0] * scale;
    out->c[1] += a->c[1] * b->c[1] * scale;
    out->c[2] += a->c[2] * b->c[2] * scale;
}

/*
 * apply_diffuse_specular — shared inner loop for both point and
 * directional lights.
 *
 * Given pre-built, normalised vectors n̂, v̂, l̂ and the surface/light
 * colours, computes the diffuse+specular contribution per spec eq. 22
 * and adds it into *c.
 *
 * Extracted to avoid duplicating the dot-product, back-face, and shading
 * arithmetic in lighting_apply_point and lighting_apply_directional.
 *
 * @param n         normalised surface normal    (modified for two-sided)
 * @param v         normalised view direction
 * @param l         normalised light direction   (surface → light)
 * @param lightCol  light colour CLd
 * @param Cb        body  colour
 * @param Cs        surface colour
 * @param s         specular exponent
 * @param oneSided  1 = skip back faces, 0 = two-sided
 * @param c         colour accumulator
 */
static void apply_diffuse_specular(Vector *n, Vector *v, Vector *l,
                                    const Color *lightCol,
                                    Color *Cb, Color *Cs,
                                    float s, int oneSided, Color *c) {
    double nDotV = vector_dot(n, v);

    /* Back-face handling */
    if (nDotV < 0.0) {
        if (oneSided) return;       /* one-sided: ignore back face          */
        /* Two-sided: flip normal so both dot products turn positive */
        n->val[0] = -n->val[0];
        n->val[1] = -n->val[1];
        n->val[2] = -n->val[2];
        nDotV     = -nDotV;
    }

    double nDotL = vector_dot(n, l);
    if (nDotL <= 0.0) return;      /* light is behind the surface: skip     */

    /*
     * Compute the halfway vector Ĥ = normalise(L̂ + V̂) (spec eq. 23).
     * Then (Ĥ·N̂)^n is the specular term.
     */
    Vector H = lighting_halfway_vector(l, v);
    double hDotN = vector_dot(&H, n);
    if (hDotN < 0.0) hDotN = 0.0;  /* clamp negative specular to 0         */

    /* Diffuse:  Cb · CLd · (L̂·N̂)    per channel */
    light_color_scale_add(c, Cb, lightCol, (float)nDotL);

    /* Specular: Cs · CLd · (Ĥ·N̂)^n  per channel.
     * Guard against pow(0,0)=1 (full highlight where none exists) and
     * pow(x, negative) which produces NaN / infinity. */
    if (hDotN > 0.0) {
        float fs = (s > 0.0f) ? s : 1.0f;
        light_color_scale_add(c, Cs, lightCol, (float)pow(hDotN, (double)fs));
    }
}


/* =======================================================================
 * Light — single-light functions
 * ======================================================================= */

/*
 * light_init — set all fields to safe default values.
 *
 * type = LightNone so the slot is treated as unused by lighting_shading.
 * direction defaults to (0, 0, −1) (into the scene).
 * sharpness defaults to 1 (linear spot falloff).
 */
void light_init(Light *light) {
    if (!light) return;

    light->type      = LightNone;
    color_set(&light->color, 0.0f, 0.0f, 0.0f); /* off — no contribution  */
    vector_set(&light->direction, 0.0, 0.0, -1.0); /* pointing into scene  */
    point_set3D(&light->position, 0.0, 0.0, 0.0);  /* at the world origin  */
    light->cutoff    = 0.0f;   /* spot: cos(90°) — hemisphere coverage      */
    light->sharpness = 1.0f;   /* spot: linear angular falloff              */
}

/*
 * light_copy — copy all fields from *from into *to.
 *
 * Struct assignment copies every field including any compiler-inserted
 * padding bytes, which is safe here because neither the source nor the
 * destination has heap-allocated members.
 */
void light_copy(Light *to, Light *from) {
    if (!to || !from) return;
    *to = *from;  /* full struct copy; all fields including padding         */
}

/*
 * light_print — write a readable description of a Light to fp.
 *
 * Maps LightType to a string tag for easy identification.
 */
void light_print(const Light *light, FILE *fp) {
    if (!fp) return;
    if (!light) { fprintf(fp, "Light: (null)\n"); return; }

    static const char *type_names[] = {
        "LightNone", "LightAmbient", "LightDirect", "LightPoint", "LightSpot"
    };
    const char *tname = (light->type >= 0 && light->type <= LightSpot)
                        ? type_names[light->type] : "LightUnknown";

    fprintf(fp, "Light:\n");
    fprintf(fp, "  type      = %s\n", tname);
    fprintf(fp, "  color     = (%.3f, %.3f, %.3f)\n",
            light->color.c[0], light->color.c[1], light->color.c[2]);
    fprintf(fp, "  direction = (%.3f, %.3f, %.3f)\n",
            light->direction.val[0], light->direction.val[1],
            light->direction.val[2]);
    fprintf(fp, "  position  = (%.3f, %.3f, %.3f)\n",
            light->position.val[0], light->position.val[1],
            light->position.val[2]);
    fprintf(fp, "  cutoff    = %.4f\n", light->cutoff);
    fprintf(fp, "  sharpness = %.4f\n", light->sharpness);
}

/*
 * light_isActive — return 1 if this light could produce visible output.
 *
 * A light is inactive if its type is LightNone OR if its colour is
 * exactly (0,0,0).  This lets callers skip the full switch dispatch for
 * lights that are present in the array but effectively off.
 */
int light_isActive(const Light *light) {
    if (!light) return 0;
    if (light->type == LightNone) return 0;
    if (light->color.c[0] == 0.0f &&
        light->color.c[1] == 0.0f &&
        light->color.c[2] == 0.0f) return 0;
    return 1;
}


/* =======================================================================
 * Lighting — scene-level functions
 * ======================================================================= */

/*
 * lighting_create — allocate a Lighting on the heap and initialise it.
 *
 * Returns NULL if malloc fails.
 */
Lighting *lighting_create(void) {
    Lighting *lt = (Lighting *)malloc(sizeof(Lighting));
    if (!lt) {
        fprintf(stderr,
            "[lighting_create] malloc failed for Lighting struct\n");
        return NULL;
    }
    lighting_init(lt);
    return lt;
}

/*
 * lighting_delete — free a heap-allocated Lighting.
 *
 * NULL-safe (free(NULL) is well-defined; the guard makes it explicit).
 */
void lighting_delete(Lighting *lighting) {
    if (lighting) free(lighting);
}

/*
 * lighting_init — zero nLights and call light_init on every slot.
 *
 * Initialising all MAX_LIGHTS slots (not just the active ones) ensures
 * that there is no unintentional data in unused entries, which can cause
 * confusing debug output when printing the Lighting struct.
 */
void lighting_init(Lighting *lighting) {
    if (!lighting) return;
    lighting->nLights = 0;
    for (int i = 0; i < MAX_LIGHTS; i++) {
        light_init(&lighting->light[i]);
    }
}

/*
 * lighting_clear — reset nLights to 0 without re-zeroing the slots.
 *
 * Faster than lighting_init when the caller just wants to start a new
 * frame without paying the cost of zeroing MAX_LIGHTS Light structs.
 */
void lighting_clear(Lighting *lighting) {
    if (!lighting) return;
    lighting->nLights = 0;
}

/*
 * lighting_add — append a configured Light to the Lighting array.
 *
 * Steps:
 *   1. Range-check nLights against MAX_LIGHTS.
 *   2. Grab a pointer to the next empty slot.
 *   3. light_init to put safe defaults in every field.
 *   4. Write the caller-provided values over the defaults.
 *   5. Increment nLights.
 *
 * Optional pointer parameters (c, direction, pos) are silently ignored
 * when NULL; the slot keeps the light_init defaults for those fields.
 */
void lighting_add(Lighting *lighting, LightType type,
                  Color *c, Vector *direction, Point *pos,
                  float cutoff, float sharpness) {
    if (!lighting) return;

    if (lighting->nLights >= MAX_LIGHTS) {
        fprintf(stderr,
            "[lighting_add] scene already has %d lights (MAX_LIGHTS); "
            "new light ignored\n", MAX_LIGHTS);
        return;
    }

    Light *slot = &lighting->light[lighting->nLights];
    light_init(slot);                 /* safe defaults first                 */

    slot->type      = type;
    slot->cutoff    = cutoff;
    slot->sharpness = sharpness;

    /* Optional fields — copy only when provided */
    if (c)         color_copy(&slot->color,     c);
    if (direction) vector_copy(&slot->direction, direction);
    if (pos)       point_copy(&slot->position,  pos);

    lighting->nLights++;
}

/*
 * lighting_count — return the current number of active lights.
 */
int lighting_count(const Lighting *lighting) {
    return lighting ? lighting->nLights : 0;
}

/*
 * lighting_print — print all active lights.
 */
void lighting_print(const Lighting *lighting, FILE *fp) {
    if (!fp) return;
    if (!lighting) { fprintf(fp, "Lighting: (null)\n"); return; }

    fprintf(fp, "Lighting: %d active light(s)\n", lighting->nLights);
    for (int i = 0; i < lighting->nLights; i++) {
        fprintf(fp, "  [%d] ", i);
        light_print(&lighting->light[i], fp);
    }
}


/* =======================================================================
 * Geometric helper — primary methods
 * ======================================================================= */

/*
 * lighting_light_vector — unit vector from surface P toward the light.
 *
 * L̂ = normalise(light->position − P)
 *
 * Returns a zero vector if either pointer is NULL.
 */
Vector lighting_light_vector(const Light *light, const Point *P) {
    Vector L;
    vector_set(&L, 0.0, 0.0, 0.0);
    if (!light || !P) return L;

    vector_set(&L,
               light->position.val[0] - P->val[0],
               light->position.val[1] - P->val[1],
               light->position.val[2] - P->val[2]);
    vector_normalize(&L);
    return L;
}

/*
 * lighting_halfway_vector — normalised Ĥ = normalise(L̂ + V̂).
 *
 * Spec eq. 23.  Both L and V should be unit vectors pointing away from
 * the surface (toward the light and toward the eye respectively).
 *
 * The addition is done component-wise; val[3] (h) is explicitly set to
 * 0 so the result is a proper direction vector.
 */
Vector lighting_halfway_vector(Vector *L, Vector *V) {
    Vector H;
    vector_set(&H, 0.0, 0.0, 0.0);
    if (!L || !V) return H;

    /* H = L + V (unnormalised) */
    H.val[0] = L->val[0] + V->val[0];
    H.val[1] = L->val[1] + V->val[1];
    H.val[2] = L->val[2] + V->val[2];
    H.val[3] = 0.0;  /* h = 0 for a direction vector */

    vector_normalize(&H);  /* Ĥ = (L + V) / ‖L + V‖ */
    return H;
}


/* =======================================================================
 * Per-light shading helpers  (primary methods)
 * ======================================================================= */

/*
 * lighting_apply_ambient — add the ambient term to *c.
 *
 * Ia = CLa · Cb    (spec eq. 21, per channel)
 *
 * No geometry (N, L, V) is needed for ambient light; the contribution
 * is the same regardless of surface orientation.
 */
void lighting_apply_ambient(Light *light, Color *Cb, Color *c) {
    if (!light || !Cb || !c) return;

    /*
     * Add CLa[i] · Cb[i] for each channel directly into the accumulator.
     * scale = 1.0 because ambient has no angular attenuation.
     */
    light_color_scale_add(c, Cb, &light->color, 1.0f);
}

/*
 * lighting_apply_directional — add a directional light contribution.
 *
 * Identical to the point-light equation (spec eq. 22) but L̂ comes from
 * light->direction (a fixed world-space unit vector) rather than from a
 * position relative to P.  This models sunlight or other sources at
 * effectively infinite distance.
 *
 * The direction field stores the vector pointing FROM the scene TOWARD
 * the light source, so no negation is needed.
 */
void lighting_apply_directional(Light *light, Vector *N, Vector *V,
                                 Color *Cb, Color *Cs, float s,
                                 int oneSided, Color *c) {
    if (!light || !N || !V || !Cb || !Cs || !c) return;

    /* Local normalised copies — never modify caller's vectors */
    Vector n = *N;  vector_normalize(&n);
    Vector v = *V;  vector_normalize(&v);

    /* L̂ is the fixed direction field of the light (normalised at add time) */
    Vector l = light->direction;
    vector_normalize(&l);  /* normalise defensively in case caller forgot   */

    apply_diffuse_specular(&n, &v, &l, &light->color,
                           Cb, Cs, s, oneSided, c);
}

/*
 * lighting_apply_point — add the point-light contribution.
 *
 * I = Cb·CLd·(L̂·N̂) + CLd·Cs·(Ĥ·N̂)^n   (spec eq. 22)
 *
 * L̂ = normalise(position − P)            (computed per fragment)
 */
void lighting_apply_point(Light *light, Vector *N, Vector *V, Point *P,
                           Color *Cb, Color *Cs, float s,
                           int oneSided, Color *c) {
    if (!light || !N || !V || !P || !Cb || !Cs || !c) return;

    /* Local normalised copies — never modify caller's vectors */
    Vector n = *N;  vector_normalize(&n);
    Vector v = *V;  vector_normalize(&v);

    /* L̂ computed from the light's world-space position and the surface P */
    Vector l = lighting_light_vector(light, P);

    apply_diffuse_specular(&n, &v, &l, &light->color,
                           Cb, Cs, s, oneSided, c);
}

/*
 * lighting_apply_spot — add the spotlight contribution.
 *
 * Algorithm:
 *   1. Compute L̂ from position and P (same as point light).
 *   2. α = dot(−L̂, normalise(spotDir)) — cosine of angle to cone axis.
 *   3. If α ≤ cutoff, the fragment is outside the cone → no contribution.
 *   4. Otherwise compute the full point-light result into a delta buffer,
 *      scale delta by the falloff function α^p, then add to *c.
 *
 * The delta-buffer approach avoids floating-point cancellation: we never
 * compute (new_c − old_c), which loses precision when the incremental
 * contribution is small relative to the existing accumulated colour.
 */
void lighting_apply_spot(Light *light, Vector *N, Vector *V, Point *P,
                          Color *Cb, Color *Cs, float s,
                          int oneSided, Color *c) {
    if (!light || !P || !c) return;

    /* L̂ from position to P, then negate for the cone test */
    Vector l = lighting_light_vector(light, P);  /* surface → light */

    /* Cone axis (normalised copy; light->direction must not be modified) */
    Vector spotDir = light->direction;
    vector_normalize(&spotDir);

    /*
     * α = dot(−L̂, spotDir)
     * −L̂ points from the light TOWARD the surface; the spotlight cone
     * opens in the +spotDir direction, so a fragment is inside when
     * the angle between −L̂ and spotDir is small (α large and positive).
     */
    Vector negL;
    vector_set(&negL, -l.val[0], -l.val[1], -l.val[2]);
    double alpha = vector_dot(&negL, &spotDir);

    /* Outside the cone → no contribution */
    if (alpha <= (double)light->cutoff) return;

    /*
     * Compute the full point-light result into a fresh delta accumulator
     * initialised to zero.  Scaling by the falloff is then just a scalar
     * multiply on three floats.
     */
    Color delta;
    color_set(&delta, 0.0f, 0.0f, 0.0f);

    lighting_apply_point(light, N, V, P, Cb, Cs, s, oneSided, &delta);

    /* Falloff: α^p — higher sharpness p concentrates the beam */
    float falloff = (float)pow(alpha, (double)light->sharpness);

    /* Add the attenuated delta to the running accumulator */
    delta.c[0] *= falloff;
    delta.c[1] *= falloff;
    delta.c[2] *= falloff;
    light_color_add(c, &delta);
}


/* =======================================================================
 * Full shading dispatch
 * ======================================================================= */

/*
 * lighting_shading — compute the final surface colour from all lights.
 *
 * Steps:
 *   1. Zero *c (even on early return, so the caller gets a defined value).
 *   2. Return early if any non-output pointer is NULL (but keep *c = 0).
 *   3. Iterate over light[0..nLights); dispatch to the appropriate helper.
 *      light_isActive provides a fast skip for disabled/black lights.
 *   4. Clamp *c to [0, 1] per channel.
 *
 * Spec §11: "Sum all contributions and then clip the result to [0, 1]."
 */
void lighting_shading(Lighting *lighting,
                      Vector *N, Vector *V, Point *P,
                      Color *Cb, Color *Cs,
                      float s, int oneSided,
                      Color *c) {
    /* Always initialise the output, even on early return */
    if (!c) return;
    color_set(c, 0.0f, 0.0f, 0.0f);

    /* All remaining pointers are required */
    if (!lighting || !N || !V || !P || !Cb || !Cs) return;

    /* Accumulate contributions from each active light */
    for (int i = 0; i < lighting->nLights; i++) {
        Light *light = &lighting->light[i];

        /* Fast skip for disabled or black lights */
        if (!light_isActive(light)) continue;

        switch (light->type) {

            case LightAmbient:
                /* Ia = CLa · Cb  (spec eq. 21) */
                lighting_apply_ambient(light, Cb, c);
                break;

            case LightDirect:
                /* Fixed-direction diffuse + specular */
                lighting_apply_directional(light, N, V, Cb, Cs, s,
                                           oneSided, c);
                break;

            case LightPoint:
                /* Position-based diffuse + specular  (spec eq. 22) */
                lighting_apply_point(light, N, V, P, Cb, Cs, s,
                                     oneSided, c);
                break;

            case LightSpot:
                /* Cone-gated point light with angular falloff */
                lighting_apply_spot(light, N, V, P, Cb, Cs, s,
                                    oneSided, c);
                break;

            case LightNone:
            default:
                /* Disabled slot — nothing to add */
                break;
        }
    }

    /* Final clamp: sum may exceed 1 when many lights overlap */
    c->c[0] = light_clampf(c->c[0]);
    c->c[1] = light_clampf(c->c[1]);
    c->c[2] = light_clampf(c->c[2]);
}
