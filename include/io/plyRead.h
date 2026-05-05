#ifndef PLYREAD_H
#define PLYREAD_H

#include "core/color.h"
#include "geometry/polygon.h"

int readPLY(char filename[], int *nPolygons, Polygon **plist, Color **clist, int estNormals);

#endif
