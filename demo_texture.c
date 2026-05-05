
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "image.h"
#include "module.h"
#include "light.h"
#include "view.h"
#include "noise.h"
#include "polygon.h"

#define ROWS 600
#define COLS 800
#define TEX_SZ 512

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
    view.b       = 30.0;
    view.screenx = cols;
    view.screeny = rows;
    matrix_setView3D(VTM, &view);
}

/* Add a textured quad (two triangles) to a module.
   verts[4] = four 3D corners (CCW), tc[4] = (s,t) for each corner,
   normal pointing outward. */
static void add_textured_quad(Module *md,
                               Point *verts, TexCoord *tc, Vector *norm) {
    /* Triangle 0: verts 0,1,2 */
    Polygon pg;
    polygon_init(&pg);
    Point tri0[3] = {verts[0], verts[1], verts[2]};
    Vector n3[3]  = {*norm, *norm, *norm};
    TexCoord t3[3]= {tc[0],  tc[1],  tc[2]};
    polygon_set(&pg, 3, tri0);
    polygon_setNormals(&pg, 3, n3);
    polygon_setTexCoords(&pg, 3, t3);
    module_polygon(md, &pg);
    polygon_clear(&pg);

    /* Triangle 1: verts 0,2,3 */
    polygon_init(&pg);
    Point tri1[3] = {verts[0], verts[2], verts[3]};
    TexCoord t3b[3]= {tc[0],   tc[2],   tc[3]};
    polygon_set(&pg, 3, tri1);
    polygon_setNormals(&pg, 3, n3);
    polygon_setTexCoords(&pg, 3, t3b);
    module_polygon(md, &pg);
    polygon_clear(&pg);
}

/* Build a simple Lighting with one point light + ambient */
static Lighting *make_lighting(float lx, float ly, float lz,
                                float lr, float lg, float lb,
                                float ambLevel) {
    Lighting *L = lighting_create();
    Color lc; color_set(&lc, lr, lg, lb);
    Point lp; point_set3D(&lp, lx, ly, lz);
    lighting_add(L, LightPoint,   &lc,  NULL, &lp, 0.0f, 0.0f);
    Color ac; color_set(&ac, ambLevel, ambLevel, ambLevel);
    lighting_add(L, LightAmbient, &ac,  NULL, NULL, 0.0f, 0.0f);
    return L;
}

/* ── Scene 1: textured ground quad ──────────────────────────────────────── */
static void scene1(void) {
    /* Generate wood texture */
    Image *tex = image_create(TEX_SZ, TEX_SZ);
    noise_generate_wood_texture(tex, 5.0f, 6, 0.5f);
    image_write(tex, "tex_wood.ppm");

    Image *src = image_create(ROWS, COLS);
    image_fillrgb(src, 0.1f, 0.12f, 0.15f);

    /* A large flat quad on the ground plane */
    Point verts[4];
    point_set3D(&verts[0], -3.0, 0.0, -3.0);
    point_set3D(&verts[1],  3.0, 0.0, -3.0);
    point_set3D(&verts[2],  3.0, 0.0,  3.0);
    point_set3D(&verts[3], -3.0, 0.0,  3.0);
    TexCoord tc[4] = {{0,0},{1,0},{1,1},{0,1}};
    Vector norm; vector_set(&norm, 0.0, 1.0, 0.0);

    Module *scene = module_create();
    add_textured_quad(scene, verts, tc, &norm);

    Matrix VTM, GTM;
    matrix_identity(&GTM);
    make_vtm(&VTM, ROWS, COLS, 0.0, 5.0, 6.0,  0.0, 0.0, 0.0);

    Lighting *light = make_lighting(2.0f, 6.0f, 4.0f,
                                    1.0f, 0.95f, 0.85f, 0.2f);
    module_parseLighting(scene, &GTM, light);

    DrawState ds;
    drawstate_init(&ds);
    Color white; color_set(&white, 1.0f, 1.0f, 1.0f);
    drawstate_setBody(&ds, &white);
    drawstate_setSurface(&ds, &white);
    drawstate_setShading(&ds, ShadeGouraud);
    ds.texture = tex;
    point_set3D(&ds.viewer, 0.0, 5.0, 6.0);

    module_draw(scene, &VTM, &GTM, &ds, light, src);
    image_write(src, "tex_scene1.ppm");
    printf("wrote tex_scene1.ppm\n");

    module_delete(scene);
    lighting_delete(light);
    image_free(tex);
    image_free(src);
}

/* ── Scene 2: marble-textured cube ──────────────────────────────────────── */
static void scene2(void) {
    Image *tex = image_create(TEX_SZ, TEX_SZ);
    noise_generate_marble_texture(tex, 4.0f, 6, 0.5f);
    image_write(tex, "tex_marble.ppm");

    Image *src = image_create(ROWS, COLS);
    image_fillrgb(src, 0.04f, 0.04f, 0.06f);

    /* Build cube manually so we can set per-face UV coords */
    /* 6 faces: front, back, left, right, top, bottom */
    /* Each face: 4 corners, CCW from outside */
    float h = 1.0f; /* half-size */
    float F[6][4][3] = {
        /* front  z=+h */ {{-h,-h, h},{h,-h, h},{h,h, h},{-h,h, h}},
        /* back   z=-h */ {{ h,-h,-h},{-h,-h,-h},{-h,h,-h},{h,h,-h}},
        /* left   x=-h */ {{-h,-h,-h},{-h,-h, h},{-h,h, h},{-h,h,-h}},
        /* right  x=+h */ {{ h,-h, h},{ h,-h,-h},{ h,h,-h},{ h,h, h}},
        /* top    y=+h */ {{-h, h, h},{ h, h, h},{ h,h,-h},{-h,h,-h}},
        /* bottom y=-h */ {{-h,-h,-h},{ h,-h,-h},{ h,-h,h},{-h,-h, h}}
    };
    float N[6][3] = {
        {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
    };
    TexCoord tc[4] = {{0,0},{1,0},{1,1},{0,1}};

    Module *scene = module_create();
    for(int face = 0; face < 6; face++) {
        Point verts[4];
        Vector norm;
        for(int v = 0; v < 4; v++)
            point_set3D(&verts[v], F[face][v][0], F[face][v][1], F[face][v][2]);
        vector_set(&norm, N[face][0], N[face][1], N[face][2]);
        add_textured_quad(scene, verts, tc, &norm);
    }

    Matrix VTM, GTM;
    matrix_identity(&GTM);
    matrix_rotateY(&GTM, cos(0.6), sin(0.6));
    Matrix tmp; matrix_identity(&tmp);
    matrix_rotateX(&tmp, cos(0.35), sin(0.35));
    matrix_multiply(&tmp, &GTM, &GTM);

    make_vtm(&VTM, ROWS, COLS, 0.0, 2.0, 5.0,  0.0, 0.0, 0.0);

    Lighting *light = make_lighting(4.0f, 5.0f, 4.0f,
                                    1.0f, 1.0f, 1.0f, 0.15f);
    module_parseLighting(scene, &GTM, light);

    DrawState ds;
    drawstate_init(&ds);
    Color white; color_set(&white, 1.0f, 1.0f, 1.0f);
    drawstate_setBody(&ds, &white);
    drawstate_setShading(&ds, ShadeGouraud);
    ds.texture = tex;
    point_set3D(&ds.viewer, 0.0, 2.0, 5.0);

    module_draw(scene, &VTM, &GTM, &ds, light, src);
    image_write(src, "tex_scene2.ppm");
    printf("wrote tex_scene2.ppm\n");

    module_delete(scene);
    lighting_delete(light);
    image_free(tex);
    image_free(src);
}

/* ── Scene 3: complex — Bezier surface + cylinder, two textures, animation  */
static void scene3(int nFrames) {
    /* Pre-generate textures */
    Image *woodTex = image_create(TEX_SZ, TEX_SZ);
    noise_generate_wood_texture(woodTex, 6.0f, 6, 0.5f);

    Image *fireTex = image_create(TEX_SZ, TEX_SZ);
    noise_generate_fire_texture(fireTex, 4.0f, 6, 0.5f);

    Image *src = image_create(ROWS, COLS);

    /* Bezier surface (floor / terrain) */
    BezierSurface bs;
    double bpts[16][3] = {
        {-3,0,-3},{-1,0.3,-3},{ 1,0.1,-3},{3,0,-3},
        {-3,0,-1},{-1,0.6,-1},{ 1,0.5,-1},{3,0,-1},
        {-3,0, 1},{-1,0.4, 1},{ 1,0.6, 1},{3,0, 1},
        {-3,0, 3},{-1,0.2, 3},{ 1,0.1, 3},{3,0, 3}
    };
    for(int i = 0; i < 16; i++)
        point_set3D(&bs.cp[i/4][i%4], bpts[i][0], bpts[i][1], bpts[i][2]);

    /* cylinder: standing vertical */
    Point cTop, cBot;
    point_set3D(&cTop, 0.0, 3.0, 0.0);
    point_set3D(&cBot, 0.0, 0.0, 0.0);

    for(int f = 0; f < nFrames; f++) {
        image_fillrgb(src, 0.03f, 0.03f, 0.05f);

        double t   = (nFrames > 1) ? (double)f / (double)(nFrames - 1) : 0.0;
        double ang = t * 2.0 * M_PI;
        double cx  = 7.0 * cos(ang + 0.5);
        double cz  = 7.0 * sin(ang + 0.5);

        Matrix VTM, GTM;
        matrix_identity(&GTM);
        make_vtm(&VTM, ROWS, COLS, cx, 4.0, cz,  0.0, 0.5, 0.0);

        Lighting *light = lighting_create();
        Color lc1; color_set(&lc1, 1.0f, 0.85f, 0.6f);
        Point lp1; point_set3D(&lp1, 4.0*cos(ang), 5.0, 4.0*sin(ang));
        lighting_add(light, LightPoint, &lc1, NULL, &lp1, 0.0f, 0.0f);
        Color lc2; color_set(&lc2, 0.3f, 0.5f, 1.0f);
        Point lp2; point_set3D(&lp2, -3.0, 3.0, 0.0);
        lighting_add(light, LightPoint, &lc2, NULL, &lp2, 0.0f, 0.0f);
        Color amb; color_set(&amb, 0.1f, 0.1f, 0.12f);
        lighting_add(light, LightAmbient, &amb, NULL, NULL, 0.0f, 0.0f);

        /* Draw Bezier terrain with wood texture */
        Module *terrain = module_create();
        module_bezierSurface(terrain, &bs, 4, 1);
        module_parseLighting(terrain, &GTM, light);

        DrawState ds;
        drawstate_init(&ds);
        Color white; color_set(&white, 1.0f, 1.0f, 1.0f);
        drawstate_setBody(&ds, &white);
        drawstate_setShading(&ds, ShadeGouraud);
        ds.texture = woodTex;
        point_set3D(&ds.viewer, cx, 4.0, cz);

        matrix_identity(&GTM);
        module_draw(terrain, &VTM, &GTM, &ds, light, src);
        module_delete(terrain);

        /* Draw cylinder with fire texture */
        Module *cyl = module_create();
        module_cylinder(cyl, &cTop, &cBot, 32);
        module_parseLighting(cyl, &GTM, light);

        ds.texture = fireTex;
        drawstate_setShading(&ds, ShadeGouraud);

        matrix_identity(&GTM);
        module_draw(cyl, &VTM, &GTM, &ds, light, src);
        module_delete(cyl);

        lighting_delete(light);

        char filename[64];
        sprintf(filename, "tex_scene3_%02d.ppm", f);
        image_write(src, filename);
        printf("wrote %s\n", filename);
    }

    image_free(woodTex);
    image_free(fireTex);
    image_free(src);
}

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    int nFrames = 12;
    if(argc > 1) nFrames = atoi(argv[1]);
    if(nFrames < 1) nFrames = 1;

    scene1();
    scene2();
    scene3(nFrames);

    printf("Texture mapping demo complete.\n");
    return 0;
}