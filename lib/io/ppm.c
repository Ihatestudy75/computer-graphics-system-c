// These functions read and write ppm files in the format output by cqcam and xv. 
// Written by Elizabeth Gordon 6-98
// Modified to read xv output images by Bruce Maxwell 7-98

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io/ppm.h"

#define USECPP 0

/* Read a PPM image from a file */
PPMPixel *ppm_read(int *rows, int *cols, int * colors, char *filename) {
   char tag[40]; // to hold the "P6" tag
   PPMPixel *image; // the image array
   FILE *fp; // file pointer
   int read, num[3], curchar;

   if(filename != NULL && strlen(filename))
     fp = fopen(filename, "r"); // open the file
   else
     fp = stdin; // read from standard input

   if(fp) {
     fscanf(fp, "%s\n", tag);

     // Read the "magic number" at the beginning of the ppm
     if (strncmp(tag, "P6", 40) != 0) 
     {
       fprintf(stderr, "not a ppm!\n");
       exit(1);
     }
     
     // Read the rows, columns, and color depth output by cqcam
     // need to read in three numbers and skip any lines that start with a #
     read = 0;
     while(read < 3) {
       curchar = fgetc(fp);
       if((char)curchar == '#') { // skip this line
	 while(fgetc(fp) != '\n')
	   /* do nothing */;
       }
       else {
	 ungetc(curchar, fp); // put the character back for fscanf
	 fscanf(fp, "%d", &(num[read]));
	 read++;
       }
     }
     while(fgetc(fp) != '\n')
       /* pass the last newline character */;

     *cols = num[0];
     *rows = num[1];
     *colors = num[2];

     if(*cols > 0 && *rows > 0) {
#if USECPP
       image = new PPMPixel[(*rows) * (*cols)];
#else
       image = (PPMPixel *)malloc(sizeof(PPMPixel)* (*rows) * (*cols));
#endif
       if(image) {
	 // Read the data
	 fread(image, sizeof(PPMPixel), (*rows) * (*cols), fp);

	 if(fp != stdin)
	   fclose(fp);

	 return(image);
       }
     }

   }
   
   return(NULL);
     
} // end read_ppm



/* 
  Write the modified image out as a ppm in the correct format to be read by 
  read_ppm.  xv will read these properly. 
*/
void ppm_write(PPMPixel *image, int rows, int cols, int colors, char *filename)
{
  FILE *fp;

  if(filename != NULL && strlen(filename))
    fp = fopen(filename, "w");
  else
    fp = stdout;

  if(fp) {
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n%d\n", cols, rows, colors); // usually set to 255

    fwrite(image, sizeof(PPMPixel), rows * cols, fp);
  }

  fclose(fp);

} // end write_ppm 


void createMask(PPMPixel *image, int rows, int cols, PPMPixel targetColor, int threshold, PPMPixel maskColor){
  (void)image;
  (void)rows;
  (void)cols;
  (void)targetColor;
  (void)threshold;
  (void)maskColor;

}

// Write the modified image out as a pgm in the correct format
void pgm_write(unsigned char *image, long rows, long cols, int intensities, char *filename)
{
  FILE *fp;

  if(filename != NULL && strlen(filename))
    fp = fopen(filename, "w");
  else
    fp = stdout;

  if(fp) {
    fprintf(fp, "P5\n");
    fprintf(fp, "%ld %ld\n%d\n", cols, rows, intensities);

    fwrite(image, sizeof(unsigned char), rows * cols, fp);
  }

  if(fp != stdout)
    fclose(fp);
} // end write_pgm 


/* Read a PGM image from a file */
unsigned char *pgm_read(int *rows, int *cols, int *intensities, char *filename) {
   char tag[40];
   unsigned char *image;
   FILE *fp;
   int read, num[3], curchar;

   if(filename != NULL && strlen(filename))
     fp = fopen(filename, "r");
   else
     fp = stdin;

   if(fp) {
     fscanf(fp, "%s\n", tag);

     // Read the "magic number" at the beginning of the ppm
     if (strncmp(tag, "P5", 40) != 0) {
       fprintf(stderr, "not a ppm!\n");
       exit(1);
     }
     
     // Read the rows, columns, and color depth output by cqcam
     // need to read in three numbers and skip any lines that start with a #
     read = 0;
     while(read < 3) {
       curchar = fgetc(fp);
       if((char)curchar == '#') { // skip this line
	 while(fgetc(fp) != '\n')
	   /* do nothing */;
       }
       else {
	 ungetc(curchar, fp);
	 fscanf(fp, "%d", &(num[read]));
	 read++;
       }
     }
     while(fgetc(fp) != '\n')
       /* pass the last newline character */;

     *cols = num[0];
     *rows = num[1];
     *intensities = num[2];

     if(*intensities != 255) {
       printf("Unable to read this file correctly\n");
       return(NULL);
     }

     if(*cols > 0 && *rows > 0) {
#if USECPP
       image = new unsigned char[(*rows) * (*cols)];
#else
       image = (unsigned char *)malloc(sizeof(unsigned char) * (*rows) * (*cols));
#endif
       if(image) {
	 // Read the data
	 fread(image, sizeof(unsigned char), (*rows) * (*cols), fp);

	 if(fp != stdin)
	   fclose(fp);

	 return(image);
       }
     }

   }
   
   return(NULL);
} // end read_pgm

/* Write a PGM image to a file */
void pgm_write(unsigned char *image, long rows, long cols, int intensities, char *filename);