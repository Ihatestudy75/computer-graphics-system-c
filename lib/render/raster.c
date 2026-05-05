#include "raster.h"
#include "module.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>

/* Flood fill algorithm */
void raster_flood_fill(Image *img, int r, int c, Color fillColor, Color borderColor){
    (void)img;
    (void)r;
    (void)c;
    (void)fillColor;
    (void)borderColor;
    // TODO: optional real implementation
}

/* Make sure the image and coordinates are valid */
static inline int valid_rc(const Image *img, int r, int c) {
    return img && r >= 0 && r < img->rows && c >= 0 && c < img->cols;
}

/* Check if the new depth value passes the depth test */
static inline int depth_pass(const Image *img, float newz, float oldz) {
    return (img->d == DEPTH_LESS) ? (newz <= oldz) : (newz >= oldz);
}

/* Test if a line segment intersects the clipping boundary */
static int cliptest(double p, double q, double *u1, double *u2){
    double r;

    if(p == 0.0){
        return q >= 0.0;
    }

    r = q / p;
    if(p < 0.0){
        if(r > *u2) return 0;
        if(r > *u1) *u1 = r;
    } else {
        if(r < *u1) return 0;
        if(r < *u2) *u2 = r;
    }

    return 1;
}

/* Normalize homogeneous point (x,y,z,h) -> (x/h,y/h,z,1) if needed */
static inline Point point_normalized(Point p) {
    double h = p.val[3];
    if (fabs(h) > 1e-12 && fabs(h - 1.0) > 1e-12) {
        p.val[0] /= h;
        p.val[1] /= h;
        p.val[3] = 1.0;
    }
    return p;
}

/* Write pixel with optional z-buffer: DEPTH_LESS or DEPTH_GREATER */
static inline void put_pixel(Polygon *p, Image *img, int r, int c, float z, Color col, DrawState *ds) {
    if (!valid_rc(img, r, c)) return;

    int zBufferEnabled = p && p->zBuffer;
    if(ds) zBufferEnabled = zBufferEnabled && ds->zBuffer;
    
    if (zBufferEnabled) {
        if (z < 1.0f) return;
        float oldz = image_getz(img, r, c);
        if (depth_pass(img, z, oldz)) {
            image_setz(img, r, c, z);
            image_setColor(img, r, c, col);
        }
    } else {
        image_setColor(img, r, c, col);
    }
}

/* 
    Compute the edge function for a point (px, py) with respect to the edge (ax, ay) -> (bx, by) 
    To determine which side of the edge the point is on, compute:
        edge_fn = (px - ax) * (by - ay) - (py - ay) * (bx - ax)
        If edge_fn > 0, the point is on one side; 
        if edge_fn < 0, it's on the other side; if edge_fn = 0, it's on the edge.    
*/
static inline float edge_fn(double ax, double ay, double bx, double by, double px, double py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

/* Fill a triangle defined by vertices A0, B0, C0 with optional per-vertex colors cA, cB, cC (NULL to use fillColor) */
static void barycentricFillTriangle(
    Polygon *p, Image *img,
    Point A0, Point B0, Point C0,
    Color fillColor,
    Color *cA, Color *cB, Color *cC   // 允许为 NULL：NULL 就用 fillColor
){
    Point A = point_normalized(A0);
    Point B = point_normalized(B0);
    Point C = point_normalized(C0);

    double ax = A.val[0], ay = A.val[1], az = A.val[2];
    double bx = B.val[0], by = B.val[1], bz = B.val[2];
    double cx = C.val[0], cy = C.val[1], cz = C.val[2];

    int cmin = (int)floor(fmin(ax, fmin(bx, cx))); // leftmost x among the vertices
    int cmax = (int)ceil (fmax(ax, fmax(bx, cx))); // rightmost x among the vertices
    int rmin = (int)floor(fmin(ay, fmin(by, cy))); // lowest y among the vertices
    int rmax = (int)ceil (fmax(ay, fmax(by, cy))); // highest y among the vertices

    if (cmin < 0) cmin = 0;
    if (rmin < 0) rmin = 0;
    if (cmax > img->cols - 1) cmax = img->cols - 1;
    if (rmax > img->rows - 1) rmax = img->rows - 1;

    double area = edge_fn(ax, ay, bx, by, cx, cy);
    if (fabs(area) < 1e-12) return;

    const double eps = -1e-9;

    for (int r = rmin; r <= rmax; r++) {
        for (int c = cmin; c <= cmax; c++) {
            double px = c + 0.5;
            double py = r + 0.5;

            double w0 = edge_fn(bx, by, cx, cy, px, py) / area;
            double w1 = edge_fn(cx, cy, ax, ay, px, py) / area;
            double w2 = 1.0 - w0 - w1;

            if (w0 >= eps && w1 >= eps && w2 >= eps) {
                float z = (float)(w0 * az + w1 * bz + w2 * cz);

                Color out = fillColor;
                if (cA && cB && cC) {
                    out.c[0] = (float)(w0 * cA->c[0] + w1 * cB->c[0] + w2 * cC->c[0]);
                    out.c[1] = (float)(w0 * cA->c[1] + w1 * cB->c[1] + w2 * cC->c[1]);
                    out.c[2] = (float)(w0 * cA->c[2] + w1 * cB->c[2] + w2 * cC->c[2]);
                }

                put_pixel(p, img, r, c, z, out, NULL);
            }
        }
    }
}

/* Fill a polygon using barycentric coordinates */
void raster_barycentric_fill(Polygon *p, Image *img, Color fillColor) {
    if (!p || !img) return;
    if (p->nVertex < 3 || !p->vertex) return;

    Point v0 = p->vertex[0];

    for (int i = 1; i < p->nVertex - 1; i++) {
        Color *c0 = NULL, *c1 = NULL, *c2 = NULL;

        // 如果有 per-vertex color，就用它做 gradient
        if (p->color) {
            c0 = &p->color[0];
            c1 = &p->color[i];
            c2 = &p->color[i + 1];
        }

        barycentricFillTriangle(p, img, v0, p->vertex[i], p->vertex[i + 1],
                                fillColor, c0, c1, c2);
    }
}

/* Initialize an EdgeVec */
static void edgevec_init(EdgeVec *v) { 
    v->e = NULL; v->n = 0; v->cap = 0; 
}

/* Free an EdgeVec */
static void edgevec_free(EdgeVec *v) { 
    free(v->e); v->e = NULL; v->n = 0; v->cap = 0; 
}

/* Push an edge into an EdgeVec */
static void edgevec_push(EdgeVec *v, EdgeRec ed) {
    if (v->n >= v->cap) { // If we need more space, double the capacity
        int newCap = (v->cap == 0) ? 8 : v->cap * 2;

        // Reallocate the edge array with the new capacity
        EdgeRec *ne = (EdgeRec*)realloc(v->e, newCap * sizeof(EdgeRec));
        
        if (!ne) return; // out of memory -> drop edge (rare for coursework sizes)
        
        // Update the EdgeVec with the new array and capacity
        v->e = ne;
        v->cap = newCap;
    }

    // Add the new edge to the array and increment the count
    v->e[v->n++] = ed;
}

/* Compare edges by x coordinate (for sorting in AET) */
static int cmp_edge_x(const void *a, const void *b) {
    const EdgeRec *ea = (const EdgeRec*)a;
    const EdgeRec *eb = (const EdgeRec*)b;

    // Primary sort by x coordinate
    if (ea->x < eb->x) return -1; // ea is to the left of eb
    if (ea->x > eb->x) return 1;  // ea is to the right of eb

    // Tie-breaker: smaller slope first (optional, helps stability)
    if (ea->inverseSlope < eb->inverseSlope) return -1;
    if (ea->inverseSlope > eb->inverseSlope) return 1;
    return 0;
}

static inline float safe_inv_z(double z){
    if(fabs(z) < 1e-6) return 0.0f;
    return (float)(1.0 / z);
}

static inline Color color_over_z(Color c, double z){
    Color out;
    float invz = safe_inv_z(z);
    out.c[0] = c.c[0] * invz;
    out.c[1] = c.c[1] * invz;
    out.c[2] = c.c[2] * invz;
    return out;
}

static inline Color color_from_over_z(Color cOverZ, float z){
    Color out;
    out.c[0] = cOverZ.c[0] * z;
    out.c[1] = cOverZ.c[1] * z;
    out.c[2] = cOverZ.c[2] * z;

    for(int i = 0; i < 3; i++){
        if(out.c[i] < 0.0f) out.c[i] = 0.0f;
        if(out.c[i] > 1.0f) out.c[i] = 1.0f;
    }

    return out;
}

/* Fill a polygon using scanline algorithm (ET/AET) with z-buffering */
void raster_scanline_fill(Polygon *p, Image *img, Color fillColor, DrawState *ds) {
    if (!p || !img) return;
    if (p->nVertex < 3 || !p->vertex) return;

    // 1) Determine scanline range in y (use normalized coordinates)
    double yMinF =  DBL_MAX;
    double yMaxF = -DBL_MAX;
    double zMaxF = -DBL_MAX;

    for (int i = 0; i < p->nVertex; i++) {
        Point vi = point_normalized(p->vertex[i]);
        
        // Find the min and max y among the vertices
        if(vi.val[1] < yMinF) yMinF = vi.val[1];
        if(vi.val[1] > yMaxF) yMaxF = vi.val[1];
        if(vi.val[2] > zMaxF) zMaxF = vi.val[2];
    }

    int yStartAll = (int)ceil(yMinF);
    int yEndAll   = (int)floor(yMaxF);

    // Clip to image
    if (yEndAll < 0 || yStartAll > img->rows - 1) return;
    if (yStartAll < 0) yStartAll = 0;
    if (yEndAll > img->rows - 1) yEndAll = img->rows - 1;

    // 2) Build Edge Table (ET): one EdgeVec per scanline 
    int H = img->rows;
    EdgeVec *ET = (EdgeVec*)malloc(H * sizeof(EdgeVec));
    if (!ET) return;

    for (int y = 0; y < H; y++) edgevec_init(&ET[y]);

    // For each edge of the polygon, compute its intersection with scanlines and add to ET
    for (int i = 0; i < p->nVertex; i++) {
        Point a0 = point_normalized(p->vertex[i]);
        Point b0 = point_normalized(p->vertex[(i + 1) % p->nVertex]); // next vertex (wrap around)
        Color ca0 = fillColor;
        Color cb0 = fillColor;

        if(p->color){
            ca0 = p->color[i];
            cb0 = p->color[(i + 1) % p->nVertex];
        }

        // Grab world-space normals and positions for Phong (stored before VTM transform)
        Vector na0, nb0;
        Point  pa0, pb0;
        int hasPhong = (p->normal != NULL && p->worldVertex != NULL);
        if(hasPhong){
            na0 = p->normal[i];
            nb0 = p->normal[(i + 1) % p->nVertex];
            pa0 = p->worldVertex[i];
            pb0 = p->worldVertex[(i + 1) % p->nVertex];
        } else {
            vector_set(&na0, 0,0,1); vector_set(&nb0, 0,0,1);
            point_set(&pa0, 0,0,0,1); point_set(&pb0, 0,0,0,1);
        }

        // Skip horizontal edges
        if (fabs(a0.val[1] - b0.val[1]) < 1e-12) continue;

        // Order by y
        int lowerIdx = (a0.val[1] < b0.val[1]) ? i : (i + 1) % p->nVertex;
        int upperIdx = (a0.val[1] < b0.val[1]) ? (i + 1) % p->nVertex : i;
        Point lower = (a0.val[1] < b0.val[1]) ? a0 : b0;
        Point upper = (a0.val[1] < b0.val[1]) ? b0 : a0;
        Color cLower = (a0.val[1] < b0.val[1]) ? ca0 : cb0;
        Color cUpper = (a0.val[1] < b0.val[1]) ? cb0 : ca0;
        (void)lowerIdx; (void)upperIdx;
        Vector nLower = (a0.val[1] < b0.val[1]) ? na0 : nb0;
        Vector nUpper = (a0.val[1] < b0.val[1]) ? nb0 : na0;
        Point  pLower = (a0.val[1] < b0.val[1]) ? pa0 : pb0;
        Point  pUpper = (a0.val[1] < b0.val[1]) ? pb0 : pa0;

        double x0 = lower.val[0], y0 = lower.val[1], z0 = lower.val[2];
        double x1 = upper.val[0], y1 = upper.val[1], z1 = upper.val[2];

        double dy = y1 - y0;
        double dx = x1 - x0;
        double dz = z1 - z0;

        float invSlope = (float)(dx / dy);
        float dzPerScanline = (float)(dz / dy);
        Color c0OverZ = color_over_z(cLower, z0);
        Color c1OverZ = color_over_z(cUpper, z1);
        Color dcPerScan;

        for(int channel = 0; channel < 3; channel++){
            dcPerScan.c[channel] = (c1OverZ.c[channel] - c0OverZ.c[channel]) / (float)dy;
        }

        // Phong: compute per-scanline delta for normal and world position
        Vector dnPerScan; Point dpPerScan;
        for(int k = 0; k < 4; k++){
            dnPerScan.val[k] = (float)((nUpper.val[k] - nLower.val[k]) / dy);
            dpPerScan.val[k] = (float)((pUpper.val[k] - pLower.val[k]) / dy);
        }

        // Edge active y range:
        int yStart = (int)ceil(y0);
        int yEndEx = (int)ceil(y1);
        int yMax   = yEndEx - 1; // to avoid double-counting shared vertices

        // Completely outside
        if (yMax < 0 || yStart > img->rows - 1) continue;

        // Clamp start into image bounds for ET indexing
        if (yStart < 0) yStart = 0;
        if (yStart > img->rows - 1) continue;

        // Compute x,z at yStart (intersection on that scanline)
        float t0 = (float)(yStart - y0);
        float xAtStart = (float)(x0 + t0 * (dx / dy));
        float zAtStart = (float)(z0 + t0 * (dz / dy));
        Color cAtStart;
        for(int channel = 0; channel < 3; channel++){
            cAtStart.c[channel] = c0OverZ.c[channel] + t0 * dcPerScan.c[channel];
        }

        // Phong start values
        Vector nAtStart; Point pAtStart;
        for(int k = 0; k < 4; k++){
            nAtStart.val[k] = (float)(nLower.val[k] + t0 * dnPerScan.val[k]);
            pAtStart.val[k] = (float)(pLower.val[k] + t0 * dpPerScan.val[k]);
        }

        // Texture: perspective-correct (s/z, t/z) interpolation along edge
        float sL = 0.0f, tL = 0.0f, sU = 0.0f, tU = 0.0f;
        if(p->texCoord){
            int li = (a0.val[1] < b0.val[1]) ? i : (i+1) % p->nVertex;
            int ui = (a0.val[1] < b0.val[1]) ? (i+1) % p->nVertex : i;
            sL = p->texCoord[li].s; tL = p->texCoord[li].t;
            sU = p->texCoord[ui].s; tU = p->texCoord[ui].t;
        }
        float sOverZLower = (fabs(z0) > 1e-6) ? sL / (float)z0 : 0.0f;
        float tOverZLower = (fabs(z0) > 1e-6) ? tL / (float)z0 : 0.0f;
        float sOverZUpper = (fabs(z1) > 1e-6) ? sU / (float)z1 : 0.0f;
        float tOverZUpper = (fabs(z1) > 1e-6) ? tU / (float)z1 : 0.0f;
        float dsOverZPerScan = (float)((sOverZUpper - sOverZLower) / dy);
        float dtOverZPerScan = (float)((tOverZUpper - tOverZLower) / dy);
        float sOverZAtStart = sOverZLower + t0 * dsOverZPerScan;
        float tOverZAtStart = tOverZLower + t0 * dtOverZPerScan;

        // Create the edge record and add to ET[yStart]
        EdgeRec e;
        e.yMax = yMax;
        e.x = xAtStart;
        e.inverseSlope = invSlope;
        e.zIntersect = zAtStart;
        e.dzPerScanline = dzPerScanline;
        e.cIntersect = cAtStart;
        e.dcPerScan = dcPerScan;
        e.nIntersect = nAtStart;
        e.dnPerScan  = dnPerScan;
        e.pIntersect = pAtStart;
        e.dpPerScan  = dpPerScan;
        e.sOverZ         = sOverZAtStart;
        e.tOverZ         = tOverZAtStart;
        e.dsOverZPerScan = dsOverZPerScan;
        e.dtOverZPerScan = dtOverZPerScan;

        edgevec_push(&ET[yStart], e);
    }

    // 3) Active Edge Table (AET)
    EdgeVec AET;
    edgevec_init(&AET);

    // 4) Scanline processing from yStartAll to yEndAll
    for (int y = yStartAll; y <= yEndAll; y++) {
        // Add edges starting at this y
        if (ET[y].n > 0) {
            for (int i = 0; i < ET[y].n; i++) edgevec_push(&AET, ET[y].e[i]);
        }

        // Remove edges where y > yMax (expired)
        int write = 0;
        for (int i = 0; i < AET.n; i++) {
            if (y <= AET.e[i].yMax) {
                AET.e[write++] = AET.e[i];
            }
        }
        AET.n = write;

        if (AET.n < 2) { // Not enough edges to fill, just update and continue
            // Update edges and continue
            for (int i = 0; i < AET.n; i++) {
                AET.e[i].x += AET.e[i].inverseSlope; // move to next scanline
                AET.e[i].zIntersect += AET.e[i].dzPerScanline; // update z for next scanline
            }
            continue;
        }

        // Sort by x each scanline
        qsort(AET.e, AET.n, sizeof(EdgeRec), cmp_edge_x);

        // Fill spans between pairs
        for (int i = 0; i + 1 < AET.n; i += 2) {
            EdgeRec L = AET.e[i];
            EdgeRec R = AET.e[i + 1];

            float xL = L.x, zL = L.zIntersect;
            float xR = R.x, zR = R.zIntersect;
            Color cL = L.cIntersect;
            Color cR = R.cIntersect;

            int cStart = (int)ceilf(xL);
            int cEnd   = (int)floorf(xR);

            if (cEnd < 0 || cStart > img->cols - 1) continue;
            if (cStart < 0) cStart = 0;
            if (cEnd > img->cols - 1) cEnd = img->cols - 1;

            float spanWidth = xR - xL;

            // z interpolation
            float dzPerColumn = (fabsf(spanWidth) > 1e-6f) ? (zR - zL) / spanWidth : 0.0f;
            float curZ = zL + (cStart - xL) * dzPerColumn;

            // Gouraud color interpolation
            Color dcPerColumn, curCOverZ;
            for(int ch = 0; ch < 3; ch++){
                dcPerColumn.c[ch] = (fabsf(spanWidth) > 1e-6f) ? (cR.c[ch] - cL.c[ch]) / spanWidth : 0.0f;
                curCOverZ.c[ch]   = cL.c[ch] + (cStart - xL) * dcPerColumn.c[ch];
            }

            // Phong: interpolate normal and world position across the span
            Vector dnPerColumn, curN;
            Point  dpPerColumn, curP;
            for(int k = 0; k < 4; k++){
                dnPerColumn.val[k] = (fabsf(spanWidth) > 1e-6f) ? (R.nIntersect.val[k] - L.nIntersect.val[k]) / spanWidth : 0.0f;
                curN.val[k]        = (float)(L.nIntersect.val[k] + (cStart - xL) * dnPerColumn.val[k]);
                dpPerColumn.val[k] = (fabsf(spanWidth) > 1e-6f) ? (R.pIntersect.val[k] - L.pIntersect.val[k]) / spanWidth : 0.0f;
                curP.val[k]        = (float)(L.pIntersect.val[k] + (cStart - xL) * dpPerColumn.val[k]);
            }

            // Texture: perspective-correct (s/z, t/z) interpolation across the span
            float dsOverZCol = (fabsf(spanWidth) > 1e-6f) ? (R.sOverZ - L.sOverZ) / spanWidth : 0.0f;
            float dtOverZCol = (fabsf(spanWidth) > 1e-6f) ? (R.tOverZ - L.tOverZ) / spanWidth : 0.0f;
            float curSOverZ  = L.sOverZ + (cStart - xL) * dsOverZCol;
            float curTOverZ  = L.tOverZ + (cStart - xL) * dtOverZCol;

            for (int c = cStart; c <= cEnd; c++) {
                Color pixelColor = fillColor;

                if(ds) {
                    if(ds->shade == ShadeConstant) {
                        pixelColor = ds->color;
                    } else if(ds->shade == ShadeFlat) {
                        pixelColor = fillColor;
                    } else if(ds->shade == ShadeGouraud && p->color) {
                        pixelColor = color_from_over_z(curCOverZ, curZ);
                    } else if(ds->shade == ShadePhong && p->normal && ds->lighting) {
                        // Normalize the interpolated normal
                        Vector N = curN;
                        float len = sqrtf((float)(N.val[0]*N.val[0] + N.val[1]*N.val[1] + N.val[2]*N.val[2]));
                        if(len > 1e-6f){ N.val[0]/=len; N.val[1]/=len; N.val[2]/=len; }
                        // View vector from surface point to viewer
                        Vector V;
                        vector_set(&V,
                            ds->viewer.val[0] - curP.val[0],
                            ds->viewer.val[1] - curP.val[1],
                            ds->viewer.val[2] - curP.val[2]);
                        float vlen = sqrtf((float)(V.val[0]*V.val[0]+V.val[1]*V.val[1]+V.val[2]*V.val[2]));
                        if(vlen > 1e-6f){ V.val[0]/=vlen; V.val[1]/=vlen; V.val[2]/=vlen; }
                        lighting_shading(ds->lighting, &N, &V, &curP,
                            &ds->bodyColor, &ds->surfaceColor,
                            ds->surfaceCoeff, p->oneSided, &pixelColor);
                    } else if(ds->shade == ShadeDepth) {
                        float depth = 0.5f;
                        if(curZ > 0.0f) {
                            float backDepth = fmaxf(15.0f, 1.25f * (float)zMaxF);
                            depth = 1.0f - (curZ / backDepth);
                        }
                        if(depth < 0.0f) depth = 0.0f;
                        if(depth > 1.0f) depth = 1.0f;
                        pixelColor.c[0] = ds->color.c[0] * depth;
                        pixelColor.c[1] = ds->color.c[1] * depth;
                        pixelColor.c[2] = ds->color.c[2] * depth;
                    }

                    // Texture mapping: applied on top of any shading mode
                    // Perspective-correct: recover s and t from s/z and t/z
                    if(ds->texture && p->texCoord){
                        float s = curSOverZ * curZ;
                        float t = curTOverZ * curZ;
                        // Clamp to [0,1] (wrap could also use fmodf)
                        s = fmaxf(0.0f, fminf(0.9999f, s));
                        t = fmaxf(0.0f, fminf(0.9999f, t));
                        int tr = (int)(t * (ds->texture->rows - 1));
                        int tc = (int)(s * (ds->texture->cols - 1));
                        Color texColor = image_getColor(ds->texture, tr, tc);
                        // Modulate: blend texture with lighting result (multiply)
                        pixelColor.c[0] *= texColor.c[0];
                        pixelColor.c[1] *= texColor.c[1];
                        pixelColor.c[2] *= texColor.c[2];
                    }
                }

                float zTest = (curZ > 0.0f) ? (1.0f + 1.0f / curZ) : 0.0f;
                put_pixel(p, img, y, c, zTest, pixelColor, ds);

                curZ += dzPerColumn;
                for(int ch = 0; ch < 3; ch++) curCOverZ.c[ch] += dcPerColumn.c[ch];
                for(int k = 0; k < 4; k++){
                    curN.val[k] += dnPerColumn.val[k];
                    curP.val[k] += dpPerColumn.val[k];
                }
                curSOverZ += dsOverZCol;
                curTOverZ += dtOverZCol;
            }
        }

        // Update x, z, color, normal, position, texture coords for next scanline
        for (int i = 0; i < AET.n; i++) {
            AET.e[i].x          += AET.e[i].inverseSlope;
            AET.e[i].zIntersect += AET.e[i].dzPerScanline;
            for(int ch = 0; ch < 3; ch++)
                AET.e[i].cIntersect.c[ch] += AET.e[i].dcPerScan.c[ch];
            for(int k = 0; k < 4; k++){
                AET.e[i].nIntersect.val[k] += AET.e[i].dnPerScan.val[k];
                AET.e[i].pIntersect.val[k] += AET.e[i].dpPerScan.val[k];
            }
            AET.e[i].sOverZ += AET.e[i].dsOverZPerScan;
            AET.e[i].tOverZ += AET.e[i].dtOverZPerScan;
        }
    }

    // Cleanup
    for (int y = 0; y < H; y++) edgevec_free(&ET[y]);
    free(ET);
    edgevec_free(&AET);
}

/* Bresenham's line algorithm */
void raster_bresenham_line(Point p0, Point p1, Image *img, Color c){
    (void)p0;
    (void)p1;
    (void)img;
    (void)c;
    // TODO: optional real implementation
}

/* Midpoint circle algorithm */
void raster_midpoint_circle(Point center, double radius, Image *img, Color c){
    (void)center;
    (void)radius;
    (void)img;
    (void)c;
    // TODO: optional real implementation
}

/* Midpoint ellipse algorithm */
void raster_midpoint_ellipse(Point center, double ra, double rb, Image *img, Color c){
    (void)center;
    (void)ra;
    (void)rb;
    (void)img;
    (void)c;
    // TODO: optional real implementation
}

/* Bresenham's circle algorithm */
void raster_bresenham_circle(Point center, double radius, Image *img, Color c){
    (void)center;
    (void)radius;
    (void)img;
    (void)c;
    // TODO: optional real implementation
}

/* Bresenham's ellipse algorithm */
void raster_bresenham_ellipse(Point center, double ra, double rb, Image *img, Color c){
    (void)center;
    (void)ra;
    (void)rb;
    (void)img;
    (void)c;
    // TODO: optional real implementation
}

/* Line clipping algorithm 1: Subdivision method (call line_draw() recursively) */
void raster_line_clip_subdivide(Point p0, Point p1, Image *img, Color c){
    (void)p0;
    (void)p1;
    (void)img;
    (void)c;
    // TODO: optional real implementation
    /*
        Loop:
            if A and B are both visible, draw line AB (A->B) and return;
            
            if A and B are both trivially invisible, then both A and B are outside the same edge of the clip window, so return;
            
            Compute the midpoint C of AB;
            line_draw(A, C);
            line_draw(C, B);
            until A and B are both visible or both invisible.
        This recursive subdivision will eventually reach a base case where the line segment is either fully visible or fully invisible, at which point it will either draw the line or discard it.
    */
}

/* Line clipping algorithm 2: Liang-Barsky method */
int raster_liang_barsky_clip(Line *l, Image *img){
    double x0, y0, z0, x1, y1, z1, dx, dy, dz;
    double u1 = 0.0, u2 = 1.0;

    if(l == NULL || img == NULL) return 0;

    x0 = l->from.val[0];
    y0 = l->from.val[1];
    z0 = l->from.val[2];
    x1 = l->to.val[0];
    y1 = l->to.val[1];
    z1 = l->to.val[2];
    dx = x1 - x0;
    dy = y1 - y0;
    dz = z1 - z0;

    if(!cliptest(-dx, x0, &u1, &u2)) return 0;
    if(!cliptest( dx, (img->cols - 1) - x0, &u1, &u2)) return 0;
    if(!cliptest(-dy, y0, &u1, &u2)) return 0;
    if(!cliptest( dy, (img->rows - 1) - y0, &u1, &u2)) return 0;

    if(u2 < u1) return 0;

    point_set(&l->from,
        x0 + u1 * dx,
        y0 + u1 * dy,
        z0 + u1 * dz,
        1.0);
    point_set(&l->to,
        x0 + u2 * dx,
        y0 + u2 * dy,
        z0 + u2 * dz,
        1.0);

    return 1;
}

/* Draw a line using the Liang-Barsky clipping algorithm */
void raster_line_clip_liang_barsky_draw(Point p0, Point p1, Image *img, Color c){
    Line l;
    int x0, y0, x1, y1;
    int dx, dy, sx, sy, err;

    line_set(&l, p0, p1);
    if(!raster_liang_barsky_clip(&l, img)) return;

    x0 = (int)(l.from.val[0]);
    y0 = (int)(l.from.val[1]);
    x1 = (int)(l.to.val[0]);
    y1 = (int)(l.to.val[1]);
    dx = abs(x1 - x0);
    dy = abs(y1 - y0);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    while(1){
        Point p;
        point_set2D(&p, x0, y0);
        point_draw(&p, img, c);

        if(x0 == x1 && y0 == y1) break;

        if(2 * err > -dy){
            err -= dy;
            x0 += sx;
        }
        if(2 * err < dx){
            err += dx;
            y0 += sy;
        }
    }

    /*
        The Liang-Barsky algorithm uses the parametric form of the line and inequalities to determine the portion of the line that is visible within the clipping window. 
        It calculates the entering and leaving points of the line segment with respect to the clipping boundaries and then determines if there is a visible portion to draw.
    
        Used as a ray tracing method for line clipping, 
        it can be more efficient than the Cohen-Sutherland algorithm because it reduces the number of intersection calculations needed by using the parametric form of the line and directly computing the intersection points with the clipping boundaries.

        Parametric Line Equaltion: P(t) = A + t(B - A), where t ranges from 0 to 1 for the line segment AB.
        A: starting point of the line segment (p0)
        B: ending point of the line segment (p1)
        B-A: the direction vector of the line segment

        Given A = (x0, y0) and B = (x1, y1), 
        the parametric equations for the line segment are:
        x(t) = x0 + t(x1 - x0)
        y(t) = y0 + t(y1 - y0)
        z(t) = z0 + t(z1 - z0) // for 3D line segments with depth

        Basic idea: For each edge of the clipping window, we compute the values of t where the line intersects
    
        Constrains:
        For x, xmin <= x(t) <= xmax
        For y, ymin <= y(t) <= ymax
        For z, zmin <= z(t) <= zmax (if depth clipping is needed)
    */

    /*
        Implementations:
        Given A = (x0, y0) and B = (x1, y1), we can define:
        p1 = -(x1 - x0) // left edge
        p2 = (x1 - x0)  // right edge
        p3 = -(y1 - y0) // bottom edge
        p4 = (y1 - y0)  // top edge
        p5 = -(z1 - z0) // near plane (if depth clipping)
        p6 = (z1 - z0)  // far plane (if depth clipping)

        q1 = x0 - xmin
        q2 = xmax - x0
        q3 = y0 - ymin
        q4 = ymax - y0
        q5 = z0 - zmin (if depth clipping)
        q6 = zmax - z0 (if depth clipping)

        if(p_i == 0), line from A to B is parallel to the edge i
            if q_i < 0, then the line is outside the edge and can be rejected (trivially invisible)
            if q_i >= 0, then the line is inside or on the edge and we continue checking other edges
        
        if p_i < 0, the direction of the line is towards the inside of the edge, 
        e.g. for the left edge, if p1 < 0 and p2 > 0, so A is inside the left edge and B is outside, which is towards the inside of the left edge
        
        if(p_i > 0), the direction of the line is towards the outside of the edge,
        e.g. for the left edge, if p1 > 0, it means the line is going from left(A) to right(B), which is towards the outside of the left edge
    
        For any non-zero pi, t_i = q_i / p_i gives the parameter value at which the line intersects the edge i.

        Find t_enter = max(0, all t_i where p_i < 0) // the largest t for entering edges
        Find t_leave = min(1, all t_i where p_i > 0) // the smallest t for leaving edges
        if t_enter > t_leave, the line is completely outside and can be rejected (trivially invisible)
        if t_enter <= t_leave, the visible portion of the line is from P(t_enter) to P(t_leave), so we can compute these points and draw the line segment between them.
        
        To find t_enter and t_leave, we can initialize:
        t_enter = 0, t_leave = 1
        Then for each edge, we update t_enter and t_leave based on the values of p_i and q_i as described above,
        each cooresponding to {left, right, bottom, top, front, back} edges of the clipping window.
    */

    /*
        for each boundary edge of the clipping window:
            compute p_i and q_i
            if p_i == 0:
                if q_i < 0, reject line (trivially invisible)
                else continue checking next edge
            else if p_i < 0:
                t = q_i / p_i
                t_enter = max(t_enter, t) // update entering parameter
            else if p_i > 0:
                t = q_i / p_i
                t_leave = min(t_leave, t) // update leaving parameter
        if t_enter > t_leave, reject line (trivially invisible)
        else:
            compute visible endpoints: P(t_enter) and P(t_leave)
            draw line segment between P(t_enter) and P(t_leave)
    */
}
