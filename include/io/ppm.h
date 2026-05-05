#ifndef PPMIO_H
#define PPMIO_H

// PPM and PGM image input/output functions
typedef struct {
  unsigned char r;  // red color component
  unsigned char g;  // green color component
  unsigned char b;  // blue color component
} PPMPixel;

/* Read a PPM image from a file */
PPMPixel *ppm_read(int *rows, int *cols, int * colors, char *filename);

/* Write a PPM image to a file */
void ppm_write(PPMPixel *image, int rows, int cols, int colors, char *filename);

/* Read a PGM image from a file */
unsigned char *pgm_read(int *rows, int *cols, int *intensities, char *filename);

/* Write a PGM image to a file */
void pgm_write(unsigned char *image, long rows, long cols, int intensities, char *filename);

#endif
