/*
  Extension 8: a small planet scene built from polygonal spheres and rings.
*/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/image.h"
#include "geometry/polygon.h"
#include "math/matrix.h"
#include "math/view.h"
#include "render/module.h"

static Point sphere_point(double theta, double phi) {
  Point p;
  double cphi = cos(phi);

  point_set3D(&p, cphi * cos(theta), sin(phi), cphi * sin(theta));
  return p;
}

static Color shade_color(Color base, double theta, double phi) {
  Color out;
  double nx = cos(phi) * cos(theta);
  double ny = sin(phi);
  double nz = cos(phi) * sin(theta);
  double lx = -0.45;
  double ly = 0.65;
  double lz = -0.62;
  double len = sqrt(lx * lx + ly * ly + lz * lz);
  double dot;
  double shade;

  lx /= len;
  ly /= len;
  lz /= len;

  dot = nx * lx + ny * ly + nz * lz;
  if(dot < 0.0) dot = 0.0;

  shade = 0.50 + 0.60 * dot;
  if(shade > 1.0) shade = 1.0;

  color_set(&out,
            fmin(1.0, base.c[0] * shade + 0.06 * dot),
            fmin(1.0, base.c[1] * shade + 0.06 * dot),
            fmin(1.0, base.c[2] * shade + 0.08 * dot));
  return out;
}

static void paint_space(Image *src) {
  Color c;
  Color star;

  for(int r = 0; r < src->rows; r++) {
    double t = (double)r / (double)(src->rows - 1);
    color_set(&c, 0.025 + 0.030 * (1.0 - t), 0.030 + 0.035 * (1.0 - t), 0.050 + 0.055 * (1.0 - t));
    for(int col = 0; col < src->cols; col++) {
      image_setColor(src, r, col, c);
    }
  }

  color_set(&star, 0.85, 0.90, 1.00);
  for(int i = 0; i < 95; i++) {
    int col = (i * 137 + 29) % src->cols;
    int r = (i * 71 + 43) % src->rows;
    image_setColor(src, r, col, star);
    if(i % 9 == 0 && col + 1 < src->cols) image_setColor(src, r, col + 1, star);
  }
}

static void add_sphere(Module *m, int slices, int stacks, Color *low,
                       Color *mid, Color *high) {
  Polygon p;
  polygon_init(&p);

  for(int j = 0; j < stacks; j++) {
    double phi0 = -M_PI / 2.0 + j * M_PI / stacks;
    double phi1 = -M_PI / 2.0 + (j + 1) * M_PI / stacks;

    for(int i = 0; i < slices; i++) {
      double theta0 = i * 2.0 * M_PI / slices;
      double theta1 = (i + 1) * 2.0 * M_PI / slices;
      double thetaMid = 0.5 * (theta0 + theta1);
      double phiMid = 0.5 * (phi0 + phi1);
      Color *band = (j < stacks / 3) ? low : ((j < 2 * stacks / 3) ? mid : high);
      Color patchColor;
      Point v[4];

      if((i + 2 * j) % 11 == 0) {
        band = high;
      } else if((2 * i + j) % 13 == 0) {
        band = low;
      }

      patchColor = shade_color(*band, thetaMid, phiMid);
      module_color(m, &patchColor);

      v[0] = sphere_point(theta0, phi0);
      v[1] = sphere_point(theta1, phi0);
      v[2] = sphere_point(theta1, phi1);
      v[3] = sphere_point(theta0, phi1);

      polygon_set(&p, 4, v);
      module_polygon(m, &p);
    }
  }

  polygon_clear(&p);
}

static void add_ring(Module *m, double innerRadius, double outerRadius, int slices,
                     Color *color) {
  Polygon p;
  polygon_init(&p);
  module_color(m, color);

  for(int i = 0; i < slices; i++) {
    double theta0 = i * 2.0 * M_PI / slices;
    double theta1 = (i + 1) * 2.0 * M_PI / slices;
    Point v[4];

    point_set3D(&v[0], innerRadius * cos(theta0), 0.0, innerRadius * sin(theta0));
    point_set3D(&v[1], outerRadius * cos(theta0), 0.0, outerRadius * sin(theta0));
    point_set3D(&v[2], outerRadius * cos(theta1), 0.0, outerRadius * sin(theta1));
    point_set3D(&v[3], innerRadius * cos(theta1), 0.0, innerRadius * sin(theta1));

    polygon_set(&p, 4, v);
    module_polygon(m, &p);
  }

  polygon_clear(&p);
}

static void place_planet(Module *scene, Module *planet, double scale,
                         double rx, double ry, double rz,
                         double tx, double ty, double tz) {
  module_identity(scene);
  module_scale(scene, scale, scale, scale);
  module_rotateX(scene, cos(rx), sin(rx));
  module_rotateY(scene, cos(ry), sin(ry));
  module_rotateZ(scene, cos(rz), sin(rz));
  module_translate(scene, tx, ty, tz);
  module_module(scene, planet);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  const int rows = 520;
  const int cols = 760;
  Image *src = image_create(rows, cols);
  Matrix VTM, GTM;
  View3D view;
  DrawState *ds;
  Module *sun;
  Module *earth;
  Module *mars;
  Module *gasGiant;
  Module *moon;
  Module *scene;

  Color sunLow, sunMid, sunHigh;
  Color ocean, cloud, land;
  Color marsDark, marsMid, marsCap;
  Color gasDark, gasMid, gasLight, ringColor;
  Color moonLow, moonMid, moonHigh;

  color_set(&sunLow, 1.00, 0.35, 0.04);
  color_set(&sunMid, 1.00, 0.72, 0.10);
  color_set(&sunHigh, 1.00, 0.96, 0.42);
  color_set(&ocean, 0.05, 0.42, 0.95);
  color_set(&cloud, 0.96, 0.98, 0.96);
  color_set(&land, 0.18, 0.72, 0.28);
  color_set(&marsDark, 0.60, 0.17, 0.08);
  color_set(&marsMid, 0.95, 0.40, 0.16);
  color_set(&marsCap, 1.00, 0.80, 0.62);
  color_set(&gasDark, 0.70, 0.44, 0.92);
  color_set(&gasMid, 0.95, 0.62, 0.32);
  color_set(&gasLight, 1.00, 0.92, 0.56);
  color_set(&ringColor, 0.98, 0.88, 0.58);
  color_set(&moonLow, 0.48, 0.50, 0.54);
  color_set(&moonMid, 0.68, 0.70, 0.74);
  color_set(&moonHigh, 0.90, 0.91, 0.93);

  paint_space(src);

  matrix_identity(&GTM);
  matrix_identity(&VTM);

  point_set3D(&(view.vrp), 0.0, 4.3, -12.0);
  vector_set(&(view.vpn), 0.0, -3.0, 11.0);
  vector_set(&(view.vup), 0.0, 1.0, 0.0);
  view.d = 2.2;
  view.du = 2.05;
  view.dv = 1.40;
  view.f = 0.0;
  view.b = 32.0;
  view.screenx = cols;
  view.screeny = rows;
  matrix_setView3D(&VTM, &view);

  sun = module_create();
  add_sphere(sun, 36, 18, &sunLow, &sunMid, &sunHigh);

  earth = module_create();
  add_sphere(earth, 32, 16, &ocean, &land, &cloud);

  mars = module_create();
  add_sphere(mars, 28, 14, &marsDark, &marsMid, &marsCap);

  gasGiant = module_create();
  add_sphere(gasGiant, 40, 18, &gasDark, &gasMid, &gasLight);
  module_identity(gasGiant);
  module_scale(gasGiant, 1.0, 1.0, 1.0);
  module_rotateX(gasGiant, cos(0.48), sin(0.48));
  add_ring(gasGiant, 1.25, 2.0, 72, &ringColor);

  moon = module_create();
  add_sphere(moon, 20, 10, &moonLow, &moonMid, &moonHigh);

  scene = module_create();
  place_planet(scene, sun, 2.10, 0.0, 0.15, 0.0, -3.8, 0.45, 4.3);
  place_planet(scene, earth, 1.00, -0.35, 0.8, 0.15, -0.55, -0.45, 2.4);
  place_planet(scene, moon, 0.32, -0.15, 0.2, 0.0, 0.65, 0.18, 1.85);
  place_planet(scene, mars, 0.72, 0.2, -0.55, 0.0, 2.30, -0.92, 3.3);
  place_planet(scene, gasGiant, 1.45, 0.08, -0.45, -0.08, 3.65, 0.75, 5.6);

  ds = drawstate_create();
  ds->shade = ShadeConstant;
  ds->zBuffer = 1;

  matrix_identity(&GTM);
  module_draw(scene, &VTM, &GTM, ds, NULL, src);
  image_write(src, "extension8_planets.ppm");

  module_delete(sun);
  module_delete(earth);
  module_delete(mars);
  module_delete(gasGiant);
  module_delete(moon);
  module_delete(scene);
  image_free(src);
  free(ds);

  return 0;
}
