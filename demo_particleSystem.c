#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "image.h"
#include "module.h"
#include "light.h"
#include "view.h"
#include "particle.h"

#define ROWS 600
#define COLS 800

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void make_vtm(Matrix *VTM, int rows, int cols,
                     double cx, double cy, double cz,
                     double lx, double ly, double lz) {
    View3D view;
    point_set3D(&view.vrp, cx, cy, cz);
    vector_set(&view.vpn, lx - cx, ly - cy, lz - cz);
    vector_set(&view.vup, 0.0, 1.0, 0.0);
    view.d       = 2.0;
    view.du      = 2.0;
    view.dv      = 2.0 * ((double)rows / (double)cols);
    view.f       = 0.5;
    view.b       = 50.0;
    view.screenx = cols;
    view.screeny = rows;
    matrix_setView3D(VTM, &view);
}

/* Draw a simple 3D scene (ground grid + pillars) using module system */
static void draw_scene_base(Matrix *VTM, Lighting *light, DrawState *ds, Image *src) {
    Matrix GTM; matrix_identity(&GTM);

    /* Ground plane as a large flat polygon */
    Module *ground = module_create();
    Point gv[4];
    point_set3D(&gv[0], -6.0, 0.0, -6.0);
    point_set3D(&gv[1],  6.0, 0.0, -6.0);
    point_set3D(&gv[2],  6.0, 0.0,  6.0);
    point_set3D(&gv[3], -6.0, 0.0,  6.0);
    Vector gn; vector_set(&gn, 0.0, 1.0, 0.0);
    Vector gns[4] = {gn, gn, gn, gn};
    Polygon gpg;
    polygon_init(&gpg);
    polygon_set(&gpg, 4, gv);
    polygon_setNormals(&gpg, 4, gns);
    module_polygon(ground, &gpg);
    polygon_clear(&gpg);

    module_parseLighting(ground, &GTM, light);
    Color gc; color_set(&gc, 0.25f, 0.28f, 0.32f);
    drawstate_setBody(ds, &gc);
    drawstate_setShading(ds, ShadeFlat);
    module_draw(ground, VTM, &GTM, ds, light, src);
    module_delete(ground);

    /* Three pillars: cylinders at (-3,0,-3), (3,0,-3), (0,0,3) */
    double pillarPos[3][2] = {{-3.0,-3.0},{3.0,-3.0},{0.0,3.0}};
    Color pillarColors[3];
    color_set(&pillarColors[0], 0.6f, 0.5f, 0.4f);
    color_set(&pillarColors[1], 0.5f, 0.55f, 0.6f);
    color_set(&pillarColors[2], 0.55f, 0.45f, 0.5f);

    for(int i = 0; i < 3; i++) {
        Module *pillar = module_create();
        Point top, bot;
        point_set3D(&top, pillarPos[i][0], 3.0, pillarPos[i][1]);
        point_set3D(&bot, pillarPos[i][0], 0.0, pillarPos[i][1]);
        module_cylinder(pillar, &top, &bot, 20);
        module_parseLighting(pillar, &GTM, light);
        drawstate_setBody(ds, &pillarColors[i]);
        drawstate_setShading(ds, ShadeGouraud);
        matrix_identity(&GTM);
        module_draw(pillar, VTM, &GTM, ds, light, src);
        module_delete(pillar);
    }
}

/* ── Scene 1: fire emitter, 60 frames ───────────────────────────────────── */
static void scene1(void) {
    printf("Scene 1: fire emitter...\n");

    Image *src = image_create(ROWS, COLS);
    Matrix VTM;
    make_vtm(&VTM, ROWS, COLS, 0.0, 3.0, 8.0,  0.0, 2.0, 0.0);

    /* Fire at origin */
    ParticleSystem *fire = particle_system_create(0.0f, 0.0f, 0.0f,
                                                   EmitFire, 80.0f);

    /* Warm up: run 30 steps before recording */
    for(int w = 0; w < 30; w++)
        particle_system_update(fire, 0.016f);

    for(int f = 0; f < 60; f++) {
        image_fillrgb(src, 0.02f, 0.02f, 0.04f);

        particle_system_update(fire, 0.016f);
        particle_system_draw(fire, &VTM, src);

        char filename[64];
        sprintf(filename, "ps_fire_%02d.ppm", f);
        image_write(src, filename);
        if(f % 10 == 0) printf("  wrote %s\n", filename);
    }

    particle_system_free(fire);
    image_free(src);
    printf("Scene 1 done.\n");
}

/* ── Scene 2: explosion burst, 40 frames ────────────────────────────────── */
static void scene2(void) {
    printf("Scene 2: explosion...\n");

    Image *src = image_create(ROWS, COLS);
    Matrix VTM;
    make_vtm(&VTM, ROWS, COLS, 5.0, 3.0, 8.0,  0.0, 1.0, 0.0);

    /* Explosion: emit a big burst at frame 0 then stop */
    ParticleSystem *boom = particle_system_create(0.0f, 1.5f, 0.0f,
                                                   EmitExplosion, 0.0f);
    /* Spawn all at once */
    boom->emitRate = 600.0f;
    particle_system_update(boom, 0.016f);
    boom->emitRate = 0.0f;

    /* Also add some lingering smoke above the explosion */
    ParticleSystem *smoke = particle_system_create(0.0f, 1.0f, 0.0f,
                                                    EmitSmoke, 15.0f);

    for(int f = 0; f < 40; f++) {
        image_fillrgb(src, 0.02f, 0.02f, 0.04f);

        particle_system_update(boom,  0.025f);
        particle_system_update(smoke, 0.025f);

        /* Draw smoke first (behind), then explosion on top */
        particle_system_draw(smoke, &VTM, src);
        particle_system_draw(boom,  &VTM, src);

        char filename[64];
        sprintf(filename, "ps_explosion_%02d.ppm", f);
        image_write(src, filename);
        if(f % 10 == 0) printf("  wrote %s\n", filename);
    }

    particle_system_free(boom);
    particle_system_free(smoke);
    image_free(src);
    printf("Scene 2 done.\n");
}

/* ── Scene 3: complex — 3D pillars + fire + explosion + smoke ───────────── */
static void scene3(int nFrames) {
    printf("Scene 3: complex scene (%d frames)...\n", nFrames);

    Image *src = image_create(ROWS, COLS);

    /* Fire on top of each pillar */
    ParticleSystem *fire[3];
    double pillarPos[3][2] = {{-3.0,-3.0},{3.0,-3.0},{0.0,3.0}};
    for(int i = 0; i < 3; i++) {
        fire[i] = particle_system_create((float)pillarPos[i][0], 3.1f,
                                          (float)pillarPos[i][1],
                                          EmitFire, 50.0f);
    }

    /* Central explosion (one-shot at frame 10) */
    ParticleSystem *boom = particle_system_create(0.0f, 2.0f, 0.0f,
                                                   EmitExplosion, 0.0f);
    int boomFired = 0;

    /* Rising smoke from centre */
    ParticleSystem *smoke = particle_system_create(0.0f, 0.5f, 0.0f,
                                                    EmitSmoke, 10.0f);

    /* Warm up fires */
    for(int w = 0; w < 40; w++)
        for(int i = 0; i < 3; i++)
            particle_system_update(fire[i], 0.016f);

    /* Lighting for 3D scene */
    Lighting *light = lighting_create();
    Color lc; color_set(&lc, 1.0f, 0.9f, 0.7f);
    Point lp; point_set3D(&lp, 0.0, 8.0, 5.0);
    lighting_add(light, LightPoint, &lc, NULL, &lp, 0.0f, 0.0f);
    Color amb; color_set(&amb, 0.15f, 0.15f, 0.18f);
    lighting_add(light, LightAmbient, &amb, NULL, NULL, 0.0f, 0.0f);

    DrawState ds; drawstate_init(&ds);

    for(int f = 0; f < nFrames; f++) {
        image_fillrgb(src, 0.03f, 0.03f, 0.05f);

        /* Rotating camera */
        double t   = (nFrames > 1) ? (double)f / (double)(nFrames - 1) : 0.0;
        double ang = t * 2.0 * M_PI * 0.8;
        double cx  = 10.0 * cos(ang + 0.4);
        double cz  = 10.0 * sin(ang + 0.4);

        Matrix VTM;
        make_vtm(&VTM, ROWS, COLS, cx, 5.0, cz,  0.0, 1.5, 0.0);

        /* Draw 3D base scene */
        draw_scene_base(&VTM, light, &ds, src);

        /* Fire the explosion at frame 10 */
        if(f == 10 && !boomFired) {
            boom->emitRate = 800.0f;
            particle_system_update(boom, 0.016f);
            boom->emitRate = 0.0f;
            boomFired = 1;
        }

        /* Update all particle systems */
        float dt = 0.020f;
        for(int i = 0; i < 3; i++)
            particle_system_update(fire[i], dt);
        particle_system_update(boom,  dt);
        particle_system_update(smoke, dt);

        /* Draw particles: smoke at back, fires, explosion on top */
        particle_system_draw(smoke, &VTM, src);
        for(int i = 0; i < 3; i++)
            particle_system_draw(fire[i], &VTM, src);
        particle_system_draw(boom, &VTM, src);

        char filename[64];
        sprintf(filename, "ps_complex_%02d.ppm", f);
        image_write(src, filename);
        if(f % 5 == 0) printf("  wrote %s\n", filename);
    }

    for(int i = 0; i < 3; i++) particle_system_free(fire[i]);
    particle_system_free(boom);
    particle_system_free(smoke);
    lighting_delete(light);
    image_free(src);
    printf("Scene 3 done.\n");
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    int nFrames = 30;
    if(argc > 1) nFrames = atoi(argv[1]);
    if(nFrames < 1) nFrames = 1;

    scene1();
    scene2();
    scene3(nFrames);

    printf("Particle system demo complete.\n");
    return 0;
}
