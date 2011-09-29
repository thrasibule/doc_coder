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

#include "doc_coder.h"
#include <string.h>

extern Codec *codec;
extern MarkList *all_marks;
extern CodingStrip *coding_strips;
extern int cur_coding_strip;
extern PixelMap *ori_buffer;
extern PixelMap *cleanup;

void reconstruct_image(void);
void count_flipped_pixel(char *);
void get_diff_picture(char *);

extern void write_into_cleanup(Mark *);
extern void error(char *);

/* Subroutine:	void reconstruct_image()
   Function:	reconstruct the lossy image transmitted. write marks
   		in coding strips back into the cleanup image
   Input:	none
   Output:	none
*/
void reconstruct_image()
{
  register int i, j;
  CodingStrip *strip_ptr;
  int total_coding_strip;
  MarkRegInfo *mark;
  
  total_coding_strip = cur_coding_strip;
  strip_ptr = coding_strips;  
  for(i = 0; i < total_coding_strip; i++, strip_ptr++) {
    mark = strip_ptr->marks;
    for(j = 0; j < strip_ptr->num_marks; j++, mark++) 
      write_into_cleanup(all_marks->marks + mark->list_entry);
  }
}

/* Subroutine:	void get_diff_picture()
   Function:	compare the lossy image (stored in cleanup) and the
   		original image (stored in ori_buffer), get the
   		difference bitmap
   Input:	none
   Output:	difference picture
*/
void get_diff_picture(char *diff)
{
  register int x, y;
  int w, h;
  char *optr, *lptr, *dptr;
  
  w = cleanup->width; h = cleanup->height;
  
  optr = ori_buffer->data; lptr = cleanup->data; dptr = diff;
  for(y = 0; y < h; y++) {
    for(x = 0; x < w; x++)
 	if(optr[x] != lptr[x]) {
	  dptr[x] = 1;
        }
	else dptr[x] = 0;
    optr += w;
    lptr += w;
    dptr += w;
  }
}

/* Subroutine:	void count_flipped_pixel()
   Function:	compare the lossily decoded image and original image, count 
   		# and % flipped pixels 
   Input:	difference picture
   Output:	none
*/
void count_flipped_pixel(char *diff)
{
  register int x, y;
  int w, h;
  char *dptr;
  
  w = ori_buffer->width; h = ori_buffer->height;
  
  codec->report.flipped_pixels = 0;
  dptr = diff;
  for(y = 0; y < h; y++) {
    for(x = 0; x < w; x++)
 	if(dptr[x]) codec->report.flipped_pixels++;
    dptr += w;
  }
  
  fprintf(stdout, "%d pixels(%.2f percent) are flipped\n", 
  	codec->report.flipped_pixels, 
  	100.*(float)codec->report.flipped_pixels/(float)(w*h));
}

