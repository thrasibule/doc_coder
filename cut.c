/*  Copyright 2004 The Regents of the University of California */
/* All Rights Reserved */

/* Permission to copy, modify and distribute any part of this JBIG2 codec for */
/* educational, research and non-profit purposes, without fee, and without a */
/* written agreement is hereby granted, provided that the above copyright */
/* notice, this paragraph and the following three paragraphs appear in all */
/* copies. */

/* Those desiring to incorporate this JBIG2 codec into commercial products */
/* or use for commercial purposes should contact the Technology Transfer */
/* Office, University of California, San Diego, 9500 Gilman Drive, Mail Code */
/* 0910, La Jolla, CA 92093-0910, Ph: (858) 534-5815, FAX: (858) 534-7345, */
/* E-MAIL:invent@ucsd.edu. */

/* IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR */
/* DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING */
/* LOST PROFITS, ARISING OUT OF THE USE OF THIS JBIG2 CODEC, EVEN IF THE */
/* UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* THE JBIG2 CODEC PROVIDED HEREIN IS ON AN "AS IS" BASIS, AND THE */
/* UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, */
/* UPDATES, ENHANCEMENTS, OR MODIFICATIONS.  THE UNIVERSITY OF CALIFORNIA MAKES */
/* NO REPRESENTATIONS AND EXTENDS NO WARRANTIES OF ANY KIND, EITHER IMPLIED OR */
/* EXPRESS, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF */
/* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, OR THAT THE USE OF THE */
/* JBIG2 CODEC WILL NOT INFRINGE ANY PATENT, TRADEMARK OR OTHER RIGHTS. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH	""

typedef struct {
  int width, height;
  unsigned char *content;
} BinImage;

typedef struct {
  int x, y;
} PixelCoord;

typedef struct{
  PixelCoord ul;
  int width, height;
} Rectangle;

void error(char *);
void read_pbm_image(BinImage *, char *);
void cut_rectangle(BinImage *, BinImage *, Rectangle *);
void write_pbm_image(BinImage *, char *);

void main(int argc, char **argv)
{
  char ofn[100], nfn[100];
  BinImage bim, rim;
  Rectangle rect;
  
  if(argc != 2)
    error("main: wrong command line architecture");
  
  /* read color image */  
  strcpy(ofn, argv[1]);
  read_pbm_image(&bim, ofn);
  
  printf("input upper-left corner of rectangle:");
  scanf("%d %d", &rect.ul.x, &rect.ul.y);
  
  printf("input size of rectangle:");
  scanf("%d %d", &rect.width, &rect.height);

  if(rect.ul.x + rect.width > bim.width) {
    printf("Rectangle specified is out of range horizontally, "
           "will shrink its width\n");
    rect.width = bim.width-rect.ul.x;
  }

  if(rect.ul.y + rect.height > bim.height) {
    printf("Rectangle specified is out of range vertically, "
           "will shrink its height\n");
    rect.height = bim.height-rect.ul.y;
  }

  /* binarize manually */
  cut_rectangle(&bim, &rim, &rect);
  
  /* write binarized image */
  printf("input PBM file name:");
  scanf("%s", nfn);
  if(strcmp(nfn+strlen(nfn)-4, ".pbm")) 
    strcat(nfn, ".pbm");
  
  write_pbm_image(&rim, nfn);
}

void read_pbm_image(BinImage *im, char *fn)
{
  FILE *fp;
  char line[2000];
  register int x, y;
  int buffer, bits_to_go;
  int bytes_per_line;
  unsigned char *ptr;
  
  strcpy(line, PATH);
  strcat(line, fn);
  fp = fopen(line, "r");
  if(!fp) error("read_pbm_image: Cannot open input PBM image file!");
  
  /* PBM file identifier */
  fgets(line, 2000, fp);
  if(strncmp(line, "P4", strlen("P4"))) 
    error("read_pbm_image: Input file is not in PBM format\n");

  /* skip possible comments */
  fgets(line, 2000, fp);
  while(line[0] == '#' || line[0] == '\n')
    fgets(line, 2000, fp);

  /* image size */
  sscanf(line, "%d %d", &im->width, &im->height);

  /* skip possible comments */
  fgets(line, 2000, fp);
  while(line[0] == '#' || line[0] == '\n')
    fgets(line, 2000, fp);

  /* read data */
  im->content = (unsigned char *)
    malloc(sizeof(unsigned char)*im->width*im->height);
  if(!im->content) error("read_pbm_image: Cannot allocate memory");
  memset(im->content, 0, im->width*im->height);

  bytes_per_line = im->width >> 3;
  if(im->width % 8) bytes_per_line++;
  fseek(fp, -bytes_per_line*im->height, SEEK_END);
    
  bits_to_go = 0; ptr = im->content;
  for(y = 0; y < im->height; y++) {
    for(x = 0; x < im->width; x++) {
      if(!bits_to_go) {
        buffer = fgetc(fp);
	bits_to_go = 8;
      }
      if(buffer & 0x80) ptr[x] = 1;
      buffer <<= 1; bits_to_go--;
    }
    bits_to_go = 0; ptr += im->width;
  }
}

void cut_rectangle(BinImage *oim, BinImage *nim, Rectangle *rect)
{
  register int y;
  unsigned char *optr, *nptr;
  
  nim->width = rect->width; nim->height = rect->height;
  nim->content = (unsigned char *)
    malloc(sizeof(unsigned char)*nim->width*nim->height);
  if(!nim->content) 
    error("cut_rectangle: Cannot allocate memory");
  
  optr = oim->content + oim->width*rect->ul.y+rect->ul.x;
  nptr = nim->content;
  
  for(y = 0; y < rect->height; y++) {
    memcpy(nptr, optr, rect->width);
    optr += oim->width;
    nptr += nim->width;
  } 
}

void write_pbm_image(BinImage *bim, char *fn)
{
  FILE *fp;
  char line[1000];
  register int x, y;
  int buffer;
  int bits_to_go;
  unsigned char *bptr;
  
  strcpy(line, PATH);
  strcat(line, fn);
  fp = fopen(line, "w");
  if(!fp) error("write_pbm_image: Cannot open PBM image");

  /* PBM image identifier */
  fprintf(fp, "P4\n");
  
  /* image size */
  fprintf(fp, "%d %d\n", bim->width, bim->height);
  
  /* image content */
  bptr = bim->content;
  for(y = 0; y < bim->height; y++) {
    bits_to_go = 8; buffer = 0;
    for(x = 0; x < bim->width; x++) {
      if(bptr[x]) buffer = (buffer << 1) | 1;
      else buffer <<= 1;
      bits_to_go--;
      if(!bits_to_go) {
        fputc(buffer, fp);
	bits_to_go = 8;	buffer = 0;
      } 
    }
    if(bits_to_go != 8) {
      buffer <<= bits_to_go;
      fputc(buffer, fp);
    }
    bptr += bim->width;
  }
  
  fclose(fp);
}

void error(char *msg)
{
  printf("%s\n", msg);
  exit(-1);
}
