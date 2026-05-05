#include <math.h>
#include <stdio.h>

#include "image.h"
#include "light.h"
#include "module.h"
#include "polygon.h"
#include "view.h"

#define ROWS 600
#define PANEL_COLS 320
#define COLS (PANEL_COLS * 3)

static void setup_view(Matrix *vtm, Point *viewer) {
    View3D view;

    point_set3D(&view.vrp, 2.0, 1.4, -4.2);
    vector_set(&view.vpn, -2.0, -1.0, 4.2);
    vector_set(&view.vup, 0.0, 1.0, 0.0);
    view.d = 1.15;
    view.du = 2.0;
    view.dv = 2.0 * (double)ROWS / (double)PANEL_COLS;
    view.f = 0.2;
    view.b = 20.0;
    view.screenx = PANEL_COLS;
    view.screeny = ROWS;

    matrix_setView3D(vtm, &view);
    *viewer = view.vrp;
}

static Lighting *make_lights(void) {
    Lighting *lights = lighting_create();
    Color ambient, key, rim;
    Point keyPos, rimPos;

    color_set(&ambient, 0.06f, 0.06f, 0.08f);
    lighting_add(lights, LightAmbient, &ambient, NULL, NULL, 0.0f, 0.0f);

    color_set(&key, 1.00f, 0.92f, 0.78f);
    point_set3D(&keyPos, -0.35, 1.25, -1.15);
    lighting_add(lights, LightPoint, &key, NULL, &keyPos, 0.0f, 0.0f);

    color_set(&rim, 0.22f, 0.38f, 0.95f);
    point_set3D(&rimPos, 2.8, 2.2, 0.0);
    lighting_add(lights, LightPoint, &rim, NULL, &rimPos, 0.0f, 0.0f);

    return lights;
}

static Point sphere_point(double radius, double theta, double phi) {
    Point p;
    point_set3D(&p,
                radius * cos(phi) * cos(theta),
                radius * sin(phi),
                radius * cos(phi) * sin(theta));
    return p;
}

static Vector sphere_normal(double theta, double phi) {
    Vector n;
    vector_set(&n, cos(phi) * cos(theta), sin(phi), cos(phi) * sin(theta));
    vector_normalize(&n);
    return n;
}

static Vector face_normal(Point *p) {
    Vector a, b, n;

    vector_set(&a,
               p[1].val[0] - p[0].val[0],
               p[1].val[1] - p[0].val[1],
               p[1].val[2] - p[0].val[2]);
    vector_set(&b,
               p[2].val[0] - p[0].val[0],
               p[2].val[1] - p[0].val[1],
               p[2].val[2] - p[0].val[2]);
    vector_cross(&a, &b, &n);
    vector_normalize(&n);
    return n;
}

static void add_sphere_polygon(Module *scene, int count, Point *p, Vector *n, int smoothNormals) {
    Polygon poly;
    Vector normals[4];

    if(smoothNormals) {
        for(int i = 0; i < count; i++) {
            normals[i] = n[i];
        }
    } else {
        Vector fn = face_normal(p);
        for(int i = 0; i < count; i++) {
            normals[i] = fn;
        }
    }

    polygon_init(&poly);
    polygon_set(&poly, count, p);
    polygon_setNormals(&poly, count, normals);
    polygon_setSided(&poly, 0);
    module_polygon(scene, &poly);
    polygon_clear(&poly);
}

static void add_sphere(Module *scene, double radius, int slices, int stacks, int smoothNormals) {
    for(int j = 0; j < stacks; j++) {
        double phi0 = -M_PI / 2.0 + M_PI * (double)j / (double)stacks;
        double phi1 = -M_PI / 2.0 + M_PI * (double)(j + 1) / (double)stacks;

        for(int i = 0; i < slices; i++) {
            double theta0 = 2.0 * M_PI * (double)i / (double)slices;
            double theta1 = 2.0 * M_PI * (double)(i + 1) / (double)slices;

            Point p[4];
            Vector n[4];

            if(j == 0) {
                p[0] = sphere_point(radius, 0.0, phi0);
                p[1] = sphere_point(radius, theta1, phi1);
                p[2] = sphere_point(radius, theta0, phi1);

                n[0] = sphere_normal(0.0, phi0);
                n[1] = sphere_normal(theta1, phi1);
                n[2] = sphere_normal(theta0, phi1);

                add_sphere_polygon(scene, 3, p, n, smoothNormals);
            } else if(j == stacks - 1) {
                p[0] = sphere_point(radius, theta0, phi0);
                p[1] = sphere_point(radius, theta1, phi0);
                p[2] = sphere_point(radius, 0.0, phi1);

                n[0] = sphere_normal(theta0, phi0);
                n[1] = sphere_normal(theta1, phi0);
                n[2] = sphere_normal(0.0, phi1);

                add_sphere_polygon(scene, 3, p, n, smoothNormals);
            } else {
                p[0] = sphere_point(radius, theta0, phi0);
                p[1] = sphere_point(radius, theta1, phi0);
                p[2] = sphere_point(radius, theta1, phi1);
                p[3] = sphere_point(radius, theta0, phi1);

                n[0] = sphere_normal(theta0, phi0);
                n[1] = sphere_normal(theta1, phi0);
                n[2] = sphere_normal(theta1, phi1);
                n[3] = sphere_normal(theta0, phi1);

                add_sphere_polygon(scene, 4, p, n, smoothNormals);
            }
        }
    }
}

static Module *make_scene(int smoothNormals) {
    Module *scene = module_create();
    Color blue, white;

    color_set(&blue, 0.14f, 0.38f, 0.95f);
    color_set(&white, 1.0f, 1.0f, 1.0f);

    module_bodyColor(scene, &blue);
    module_surfaceColor(scene, &white);
    module_surfaceCoeff(scene, 160.0f);
    module_translate(scene, 0.0, -0.1, 1.8);
    add_sphere(scene, 1.0, 16, 10, smoothNormals);

    return scene;
}

static Image *render_scene(ShadeMethod shade, int smoothNormals) {
    Image *img = image_create(ROWS, PANEL_COLS);
    image_fillrgb(img, 0.025f, 0.028f, 0.035f);

    Matrix vtm, gtm;
    Point viewer;
    setup_view(&vtm, &viewer);
    matrix_identity(&gtm);

    DrawState ds;
    drawstate_init(&ds);
    drawstate_setViewer(&ds, &viewer);
    drawstate_setShading(&ds, shade);

    Lighting *lights = make_lights();
    Module *scene = make_scene(smoothNormals);
    module_draw(scene, &vtm, &gtm, &ds, lights, img);

    module_delete(scene);
    lighting_delete(lights);
    return img;
}

static void copy_panel(Image *dst, Image *src, int colOffset) {
    for(int r = 0; r < ROWS; r++) {
        for(int c = 0; c < PANEL_COLS; c++) {
            Color color = image_getColor(src, r, c);
            image_setColor(dst, r, c + colOffset, color);
        }
    }
}

int main(void) {
    Image *flat = render_scene(ShadeFlat, 0);
    Image *gouraud = render_scene(ShadeGouraud, 1);
    Image *phong = render_scene(ShadePhong, 1);
    Image *combined = image_create(ROWS, COLS);
    Color divider;

    image_fillrgb(combined, 0.025f, 0.028f, 0.035f);
    copy_panel(combined, flat, 0);
    copy_panel(combined, gouraud, PANEL_COLS);
    copy_panel(combined, phong, PANEL_COLS * 2);

    color_set(&divider, 0.95f, 0.95f, 0.90f);
    for(int r = 0; r < ROWS; r++) {
        image_setColor(combined, r, PANEL_COLS - 1, divider);
        image_setColor(combined, r, PANEL_COLS, divider);
        image_setColor(combined, r, PANEL_COLS * 2 - 1, divider);
        image_setColor(combined, r, PANEL_COLS * 2, divider);
    }

    image_write(flat, "demo_flat.ppm");
    image_write(gouraud, "demo_gouraud.ppm");
    image_write(phong, "demo_phong.ppm");
    image_write(combined, "demo_flat_gouraud_phong.ppm");
    image_write(combined, "demo_phong_vs_gouraud.ppm");

    image_free(flat);
    image_free(gouraud);
    image_free(phong);
    image_free(combined);

    printf("wrote demo_flat.ppm\n");
    printf("wrote demo_gouraud.ppm\n");
    printf("wrote demo_phong.ppm\n");
    printf("wrote demo_flat_gouraud_phong.ppm\n");
    printf("wrote demo_phong_vs_gouraud.ppm\n");
    return 0;
}
