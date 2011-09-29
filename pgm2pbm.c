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

typedef struct {
  int width, height;
  unsigned char *content;
} GrayImage;

void error(char *);
void read_pgm_image(GrayImage *, char *);
void write_pbm_image(GrayImage *, char *);

main(int argc, char **argv)
{
  GrayImage gim;
  
  if(argc != 3)
    error("main: wrong command line architecture");
  
  /* read PGM image */  
  read_pgm_image(&gim, argv[1]);
  write_pbm_image(&gim, argv[2]);
}

void read_pgm_image(GrayImage *gim, char *fn)
{
  FILE *fp;
  char temp[2000];
  int level;
  
  fp = fopen(fn, "r");
  if(!fp) error("read_pgm_image: Cannot open PGM image");

  /* PBM image identifier */
  fscanf(fp, "P5\n");
  
  /* possible comments */
  do {
    fgets(temp, 2000, fp);
  } while(temp[0] == '#');
    
  /* image size */
  sscanf(temp, "%d %d\n", &(gim->width), &(gim->height));
  
  /* image gray level */
  fgets(temp, 2000, fp);
  sscanf(temp, "%d\n", &level);
  
  /* image content */
  gim->content = (unsigned char *)
    ckalloc(sizeof(unsigned char)*gim->width*gim->height);
  if(!gim->content) 
    error("read_pgm_image: cannot allocate memory");
  fread(gim->content, sizeof(unsigned char), gim->width*gim->height, fp);
  
  fclose(fp);
}

void write_pbm_image(GrayImage *gim, char *fn)
{
  FILE *fp;
  register int x, total;
  unsigned char *gptr;
  int buffer;
  int bits_written;
  
  fp = fopen(fn, "w");
  if(!fp) error("write_pbm_image: cannot open PBM file");

  /* PBM image identifier */
  fprintf(fp, "P4\n");
  
  /* image size */
  fprintf(fp, "%d %d\n", gim->width, gim->height);
    
  total = gim->width*gim->height; 
  gptr = gim->content;
  bits_written = 0; buffer = 0;
  for(x = 0; x < total; x++) {
    if(!gptr[x]) buffer <<= 1;
    else buffer = (buffer << 1) | 1;
    bits_written++;
    if(bits_written == 8) {
      fputc(buffer, fp);
      bits_written = 0; buffer = 0;
    }
  }
  
  if(bits_written) {
    buffer <<= 8-bits_written;
    fputc(buffer, fp);
  }
  
  fclose(fp);
}

void error(char *msg)
{
  printf("%s\n", msg);
  exit(-1);
}

