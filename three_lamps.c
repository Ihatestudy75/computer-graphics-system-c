#include <stdio.h>
#include <math.h>
#include "core/image.h"
#include "math/matrix.h"
#include "math/view.h"
#include "render/module.h"

/* Set up the view parameters */
static void setup(View3D *view, int rows, int cols){
    point_set3D(&view->vrp, 0.0, 1.9, -5.6);
    vector_set(&view->vpn, 0.0, -0.45, 5.6);
    vector_set(&view->vup, 0.0, 1.0, 0.0);
    view->d = 1.8;
    view->du = 2.2;
    view->dv = 2.2 * rows / cols;
    view->f = 0.0;
    view->b = 30.0;
    view->screenx = cols;
    view->screeny = rows;
}

/* Add a box to the scene */
static void add_box(Module *scene, double x, double y, double z, double sx, double sy, double sz,
                    Color *body, Color *surface, float coeff){
    module_identity(scene);
    module_scale(scene, sx, sy, sz);
    module_translate(scene, x, y, z);
    module_bodyColor(scene, body);
    module_surfaceColor(scene, surface);
    module_surfaceCoeff(scene, coeff);
    module_cube(scene, 1);
}

/* Add a cylinder to the scene */
static void add_cylinder(Module *scene, double x, double y, double z, double sx, double sy, double sz,
                         Color *body, Color *surface, float coeff, int sides){
    Point top, bottom;
    point_set3D(&top, 0.0, 0.5, 0.0);
    point_set3D(&bottom, 0.0, -0.5, 0.0);

    module_identity(scene);
    module_scale(scene, sx, sy, sz);
    module_translate(scene, x, y, z);
    module_bodyColor(scene, body);
    module_surfaceColor(scene, surface);
    module_surfaceCoeff(scene, coeff);
    module_cylinder(scene, &top, &bottom, sides);
}

/*
 * One stylized lamp model.
 * The variant comes from:
 *   - shadeTopR / shadeBottomR / shadeH
 *   - stemH
 *   - base radii
 *   - slight tilt around Z
 *   - color set
 */
static void add_lamp_variant(Module *scene,
                             double cx, double cz,
                             double shadeTopR, double shadeBottomR, double shadeH,
                             double stemH,
                             double baseRX, double baseRY, double baseRZ,
                             double tiltDeg,
                             Color *shadeBody, Color *shadeSurface,
                             Color *stemBody,  Color *stemSurface,
                             Color *baseBody,  Color *baseSurface){

    Point top, bottom;
    double tilt = tiltDeg * M_PI / 180.0;

    double baseY = -0.77;
    double stemCenterY = baseY + 0.10 + stemH * 0.5;
    double shadeCenterY = stemCenterY + stemH * 0.5 + shadeH * 0.5 - 0.05;

    /* base */
    add_cylinder(scene, cx, baseY, cz, baseRX, baseRY, baseRZ, baseBody, baseSurface, 22.0f, 48);

    /* stem */
    add_cylinder(scene, cx, stemCenterY, cz, 0.06, stemH, 0.06, stemBody, stemSurface, 38.0f, 28);

    /* shade */
    point_set3D(&top, 0.0, shadeH * 0.5, 0.0);
    point_set3D(&bottom, 0.0, -shadeH * 0.5, 0.0);

    /* Start to build the shade */
    module_identity(scene);
    if(fabs(tiltDeg) > 0.001){
        module_rotateZ(scene, cos(tilt), sin(tilt));
    }
    module_translate(scene, cx, shadeCenterY, cz);
    module_bodyColor(scene, shadeBody);
    module_surfaceColor(scene, shadeSurface);
    module_surfaceCoeff(scene, 7.0f);
    module_tube(scene, &top, shadeTopR, &bottom, shadeBottomR, 48);

    /* top cap glow disk: fake but subtle */
    add_cylinder(scene, cx, shadeCenterY + shadeH * 0.5 - 0.005, cz,
                 shadeTopR * 0.88, 0.012, shadeTopR * 0.88,
                 shadeSurface, shadeSurface, 3.0f, 48);
}

int main(void){
    const int rows = 900;
    const int cols = 900;

    Image *src = image_create(rows, cols);
    image_fillrgb(src, 0.02f, 0.018f, 0.022f);

    Color ambient;
    Color wallBody, wallSurf;
    Color floorBody, floorSurf;
    Color tableBody, tableSurf;

    Color brass, brassSpec;
    Color darkBase, darkBaseSpec;

    Color shadeA, shadeASurf;
    Color shadeB, shadeBSurf;
    Color shadeC, shadeCSurf;

    Color warmPoint;
    Color fillPoint;
    Color spotColor;

    color_set(&ambient,   0.10f, 0.08f, 0.06f);

    color_set(&wallBody,  0.20f, 0.17f, 0.14f);
    color_set(&wallSurf,  0.28f, 0.23f, 0.18f);

    color_set(&floorBody, 0.06f, 0.05f, 0.05f);
    color_set(&floorSurf, 0.10f, 0.09f, 0.09f);

    color_set(&tableBody, 0.34f, 0.18f, 0.08f);
    color_set(&tableSurf, 0.52f, 0.30f, 0.14f);

    color_set(&brass,     0.76f, 0.49f, 0.16f);
    color_set(&brassSpec, 0.95f, 0.82f, 0.50f);

    color_set(&darkBase,     0.12f, 0.07f, 0.05f);
    color_set(&darkBaseSpec, 0.22f, 0.14f, 0.10f);

    /* three lamp variants */
    color_set(&shadeA,     0.70f, 0.39f, 0.15f);
    color_set(&shadeASurf, 0.98f, 0.92f, 0.55f);

    color_set(&shadeB,     0.56f, 0.28f, 0.12f);
    color_set(&shadeBSurf, 0.95f, 0.82f, 0.46f);

    color_set(&shadeC,     1.00f, 0.82f, 0.42f);
    color_set(&shadeCSurf, 1.00f, 0.98f, 0.78f);

    color_set(&ambient,   0.1f, 0.05f, 0.04f); // A dim ambient light with a warm tint to create a cozy atmosphere and enhance the colors of the lamps
    color_set(&warmPoint, 1.50f, 0.96f, 0.78f); // A strong warm point light to create a cozy atmosphere and enhance the colors of the lamps
    color_set(&fillPoint, 0.18f, 0.16f, 0.14f); // A dim fill light to add some subtle details in the shadows
    color_set(&spotColor, 1.5f, 1.05f, 1.15f); // A strong spotlight with a slightly warm tint to enhance the highlights on the lamps

    Lighting *light = lighting_create();

    /* required ambient */
    lighting_add(light, LightAmbient, &ambient, NULL, NULL, 0.0f, 0.0f);

    /* Add the point and spot lights */
    Point pMain, pFill, spotPos;
    Vector spotDir;
    point_set3D(&pMain,  0.0, 2.0, -0.55);
    point_set3D(&pFill, -3.0, 1.2, -1.8);
    point_set3D(&spotPos, 0.0, 1.15, -2.45);
    vector_set(&spotDir, 0.0, -0.10, 2.50); // Direction from the light to the scene (pointing to the middle lamp)

    lighting_add(light, LightPoint, &warmPoint, NULL, &pMain, 0.0f, 0.0f);
    lighting_add(light, LightPoint, &fillPoint, NULL, &pFill, 0.0f, 0.0f);
    lighting_add(light, LightSpot, &spotColor, &spotDir, &spotPos, cos(M_PI / 3.5), 1.0f);

    /* ---------- scene ---------- */
    Module *scene = module_create();

    /* back wall */
    add_box(scene, 0.0, 1.25, 3.0, 7.0, 5.0, 0.08, &wallBody, &wallSurf, 6.0f);

    /* floor / dark platform */
    add_box(scene, 0.0, -1.85, 0.6, 8.5, 0.08, 6.5, &floorBody, &floorSurf, 4.0f);

    /* long table */
    add_box(scene, 0.0, -1.00, 0.25, 6.0, 0.14, 2.6, &tableBody, &tableSurf, 16.0f);

    /* table legs */
    add_box(scene, -2.55, -1.62, -0.85, 0.18, 1.1, 0.18, &tableBody, &tableSurf, 12.0f);
    add_box(scene,  2.55, -1.62, -0.85, 0.18, 1.1, 0.18, &tableBody, &tableSurf, 12.0f);
    add_box(scene, -2.55, -1.62,  1.15, 0.18, 1.1, 0.18, &tableBody, &tableSurf, 12.0f);
    add_box(scene,  2.55, -1.62,  1.15, 0.18, 1.1, 0.18, &tableBody, &tableSurf, 12.0f);


    /* Lamp variants */
    /* left */
    add_lamp_variant(scene,
                    -2.25, -0.05,
                    0.42, 0.88, 0.88,
                    0.78,
                    0.42, 0.10, 0.42,
                    -6.0,
                    &shadeA, &shadeASurf,
                    &brass,  &brassSpec,
                    &darkBase, &darkBaseSpec);

    /* center */
    add_lamp_variant(scene,
                    0.0, 0.05,
                    0.35, 0.76, 1.12,
                    0.98,
                    0.48, 0.11, 0.48,
                    0.0,
                    &shadeC, &shadeCSurf,
                    &brass,  &brassSpec,
                    &darkBase, &darkBaseSpec);

    /* right */
    add_lamp_variant(scene,
                    2.25, -0.10,
                    0.28, 0.62, 1.16,
                    1.12,
                    0.36, 0.09, 0.36,
                    8.0,
                    &shadeB, &shadeBSurf,
                    &brass,  &brassSpec,
                    &darkBase, &darkBaseSpec);

                
    /* Set up the view, draw the scene, and write the output image */
    View3D view;
    Matrix VTM, GTM;
    DrawState ds;

    setup(&view, rows, cols);
    matrix_setView3D(&VTM, &view);
    matrix_identity(&GTM);

    drawstate_init(&ds);
    drawstate_setViewer(&ds, &view.vrp);
    drawstate_setShading(&ds, ShadeGouraud);

    module_draw(scene, &VTM, &GTM, &ds, light, src);

    image_write(src, "three_lamps.ppm");

    module_delete(scene);
    lighting_delete(light);
    image_free(src);

    return 0;
}
