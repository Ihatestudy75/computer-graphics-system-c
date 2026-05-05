#include "animation/particle.h"
#include "math/matrix.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

// Random float in [lo, hi]
static float randf(float lo, float hi) {
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

// Clamp float to [0,1]
static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* ── init one particle based on emitter type ─────────────────────────────── */
static void particle_spawn(Particle *p, float ex, float ey, float ez, EmitterType type) {
    p->alive = 1;

    // Small random offset around emitter
    p->x = ex + randf(-0.05f, 0.05f);
    p->y = ey + randf(-0.05f, 0.05f);
    p->z = ez + randf(-0.05f, 0.05f);

    switch(type) {

        case EmitFire:
            p->vx = randf(-0.3f,  0.3f);
            p->vy = randf( 0.8f,  2.0f);   // upward
            p->vz = randf(-0.3f,  0.3f);
            p->ax = 0.0f;
            p->ay = -0.2f;                  // light drag
            p->az = 0.0f;
            p->r  = 1.0f;
            p->g  = randf(0.2f, 0.6f);     // orange-red
            p->b  = 0.0f;
            p->life  = 1.0f;
            p->decay = randf(0.02f, 0.05f);
            p->size  = randf(2.0f, 5.0f);
            break;

        case EmitExplosion:
            {
                // Random direction on sphere
                float theta = randf(0.0f, 2.0f * 3.14159265f);
                float phi   = randf(0.0f,        3.14159265f);
                float speed = randf(1.5f, 4.0f);
                p->vx = speed * sinf(phi) * cosf(theta);
                p->vy = speed * sinf(phi) * sinf(theta);
                p->vz = speed * cosf(phi);
                p->ax = 0.0f;
                p->ay = -1.5f;              // gravity pulls down
                p->az = 0.0f;
                // Start bright yellow-white, fades to red
                p->r  = 1.0f;
                p->g  = randf(0.6f, 1.0f);
                p->b  = randf(0.0f, 0.4f);
                p->life  = 1.0f;
                p->decay = randf(0.03f, 0.08f);
                p->size  = randf(2.0f, 6.0f);
            }
            break;

        case EmitSmoke:
            p->vx = randf(-0.1f, 0.1f);
            p->vy = randf( 0.2f, 0.6f);    // slow rise
            p->vz = randf(-0.1f, 0.1f);
            p->ax = 0.0f;
            p->ay = 0.0f;
            p->az = 0.0f;
            {
                float grey = randf(0.4f, 0.8f);
                p->r = grey; p->g = grey; p->b = grey;
            }
            p->life  = 1.0f;
            p->decay = randf(0.008f, 0.02f);
            p->size  = randf(3.0f, 8.0f);
            break;
    }
}

/* ── public API ──────────────────────────────────────────────────────────── */

ParticleSystem *particle_system_create(float ex, float ey, float ez,
                                       EmitterType type, float emitRate) {
    ParticleSystem *ps = (ParticleSystem *)malloc(sizeof(ParticleSystem));
    if(!ps) return NULL;
    particle_system_clear(ps);
    ps->ex       = ex;
    ps->ey       = ey;
    ps->ez       = ez;
    ps->type     = type;
    ps->emitRate = emitRate;
    ps->emitAccum = 0.0f;
    return ps;
}

void particle_system_free(ParticleSystem *ps) {
    free(ps);
}

void particle_system_clear(ParticleSystem *ps) {
    if(!ps) return;
    memset(ps->particles, 0, sizeof(ps->particles));
    ps->count     = 0;
    ps->emitAccum = 0.0f;
}

void particle_system_setEmitter(ParticleSystem *ps, float ex, float ey, float ez) {
    if(!ps) return;
    ps->ex = ex; ps->ey = ey; ps->ez = ez;
}

/* Update: emit new particles, integrate physics, kill dead ones */
void particle_system_update(ParticleSystem *ps, float dt) {
    if(!ps) return;

    // ── 1. Emit new particles ──────────────────────────────────────────────
    ps->emitAccum += ps->emitRate * dt;
    int toSpawn = (int)ps->emitAccum;
    ps->emitAccum -= (float)toSpawn;

    for(int s = 0; s < toSpawn; s++) {
        // Find a dead slot or extend count
        int slot = -1;
        for(int i = 0; i < ps->count; i++) {
            if(!ps->particles[i].alive) { slot = i; break; }
        }
        if(slot == -1) {
            if(ps->count < PARTICLE_MAX) slot = ps->count++;
            else break; // pool full
        }
        particle_spawn(&ps->particles[slot], ps->ex, ps->ey, ps->ez, ps->type);
    }

    // ── 2. Update alive particles ──────────────────────────────────────────
    for(int i = 0; i < ps->count; i++) {
        Particle *p = &ps->particles[i];
        if(!p->alive) continue;

        // Integrate velocity + acceleration
        p->vx += p->ax * dt;
        p->vy += p->ay * dt;
        p->vz += p->az * dt;
        p->x  += p->vx * dt;
        p->y  += p->vy * dt;
        p->z  += p->vz * dt;

        // Age the particle
        p->life -= p->decay;

        // Update color by type as it ages
        switch(ps->type) {
            case EmitFire:
                // Yellow -> orange -> red -> dark as life drops
                p->g = clamp01(p->life * 0.6f);
                p->r = clamp01(0.5f + p->life * 0.5f);
                p->b = 0.0f;
                break;
            case EmitExplosion:
                // Fade from bright to dark red
                p->g = clamp01(p->g * 0.92f);
                p->b = clamp01(p->b * 0.85f);
                break;
            case EmitSmoke:
                // Stays grey but fades out (handled by alpha = life in draw)
                break;
        }

        if(p->life <= 0.0f) p->alive = 0;
    }
}

/* Draw: project each particle through VTM, paint a filled square on image */
void particle_system_draw(ParticleSystem *ps, Matrix *VTM, Image *src) {
    if(!ps || !VTM || !src) return;

    for(int i = 0; i < ps->count; i++) {
        Particle *p = &ps->particles[i];
        if(!p->alive) continue;

        // Build homogeneous world point
        Point wp;
        point_set(&wp, p->x, p->y, p->z, 1.0);

        // Transform through VTM to get screen-space homogeneous coords
        Point sp;
        matrix_xformPoint(VTM, &wp, &sp);

        // Perspective divide
        if(fabs(sp.val[3]) < 1e-6) continue;
        float sx = (float)(sp.val[0] / sp.val[3]);
        float sy = (float)(sp.val[1] / sp.val[3]);
        float sz = (float)(sp.val[2] / sp.val[3]);

        // Depth cull: only draw points in front
        if(sz <= 0.0f) continue;

        int px = (int)sx;
        int py = (int)sy;

        // Alpha based on remaining life
        float alpha = clamp01(p->life);
        int radius  = (int)(p->size * alpha + 0.5f);
        if(radius < 1) radius = 1;

        // Paint filled square of radius pixels
        for(int dy = -radius; dy <= radius; dy++) {
            for(int dx = -radius; dx <= radius; dx++) {
                int r = py + dy;
                int c = px + dx;
                if(r < 0 || r >= src->rows) continue;
                if(c < 0 || c >= src->cols) continue;

                // z-buffer check using 1 + 1/z convention (larger = closer)
                float zTest = (sz > 1e-6f) ? (1.0f + 1.0f / sz) : 0.0f;
                float oldZ  = image_getz(src, r, c);
                if(zTest < oldZ) continue;  // behind existing geometry

                // Alpha blend onto existing pixel
                FPixel existing = image_getf(src, r, c);
                FPixel out;
                out.rgb[0] = clamp01(existing.rgb[0] * (1.0f - alpha) + p->r * alpha);
                out.rgb[1] = clamp01(existing.rgb[1] * (1.0f - alpha) + p->g * alpha);
                out.rgb[2] = clamp01(existing.rgb[2] * (1.0f - alpha) + p->b * alpha);
                out.z = existing.z;
                image_setf(src, r, c, out);
                // Note: we do NOT update the z-buffer for particles so solid
                // geometry drawn afterwards isn't occluded by them
            }
        }
    }
}
