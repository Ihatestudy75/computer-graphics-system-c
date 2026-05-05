#ifndef PARTICLE_H
#define PARTICLE_H

#include "core/image.h"
#include "geometry/primitive.h"
#include "math/matrix.h"

#define PARTICLE_MAX 4096

// Emitter type: controls initial velocity and color behavior
typedef enum {
    EmitFire,       // upward burst, red/orange/yellow, fades up
    EmitExplosion,  // radial burst in all directions, bright then dark
    EmitSmoke       // slow upward drift, grey, grows and fades
} EmitterType;

// Single particle state
typedef struct {
    float x, y, z;         // world position
    float vx, vy, vz;      // velocity
    float ax, ay, az;      // acceleration (gravity etc.)
    float r, g, b;         // current color
    float life;             // remaining life in [0,1]: 1=fresh, 0=dead
    float decay;            // life lost per update step
    float size;             // draw radius in pixels
    int   alive;            // 1 if active, 0 if dead/reusable
} Particle;

// Particle system
typedef struct {
    Particle    particles[PARTICLE_MAX];
    int         count;          // number of slots (active + dead)
    float       ex, ey, ez;     // emitter world position
    EmitterType type;
    float       emitRate;       // new particles per update call
    float       emitAccum;      // fractional accumulator
} ParticleSystem;

/* Allocate and initialize a particle system at world position (ex,ey,ez) */
ParticleSystem *particle_system_create(float ex, float ey, float ez, EmitterType type, float emitRate);

/* Free a particle system */
void particle_system_free(ParticleSystem *ps);

/* Reset all particles to dead */
void particle_system_clear(ParticleSystem *ps);

/* 
   Emit new particles and update physics for all alive particles.
   dt: time step (e.g. 0.016 for ~60fps)
*/
void particle_system_update(ParticleSystem *ps, float dt);

/*
   Draw all alive particles into the image using the given VTM.
   Each particle is drawn as a filled square of radius p->size pixels.
   VTM: the 4x4 view-transform matrix (same one passed to module_draw)
*/
void particle_system_draw(ParticleSystem *ps, Matrix *VTM, Image *src);

/* Move the emitter to a new world position */
void particle_system_setEmitter(ParticleSystem *ps, float ex, float ey, float ez);

#endif
