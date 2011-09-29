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
#include "dictionary.h"

#define FOUND		TRUE
#define UNFOUND		FALSE

extern Codec *codec;
extern Dictionary *dictionary;
extern MarkList *all_marks;
extern Mark *all_word_marks;

void  match_mark_with_dict(Mark *, int *, float *);
int   prescreen_two_marks(Mark *, Mark *);
int   prescreen_two_marks_feature(Mark *, Mark *);
float match_two_marks_XOR(Mark *, Mark *);
float match_two_marks_WXOR(Mark *, Mark *);
float match_two_marks_WAN(Mark *, Mark *);
float match_two_marks_ENT(Mark *, Mark *);
float match_two_marks_LOCAL(Mark *, Mark *);
float match_two_marks_CLUSTER(Mark *, Mark *);

extern void copy_data_with_margin(char *, int, int, int, int, int, int, char *);
extern void error(char *);

/* Subroutine:	void match_mark_with_dict()
   Function:	match the input mark with every dictionary entry, and return 
   		the matching index
   Input:	mark to be matched
   Output:	the matching index(or -1) and the mismatch score
*/
void match_mark_with_dict(Mark *mark, int *match_index, float *mm)
{
  register int i, dict_entry_num;
  float lowest_mismatch, cur_mismatch;
  Mark *dict_mark;
  DictionaryEntry *entry_ptr;
    
  dict_entry_num = dictionary->total_mark_num;
  lowest_mismatch = 1.;  cur_mismatch = 1.;
  for(i = 0; i < dict_entry_num; i++) {
    entry_ptr = dictionary->entries + i;
    dict_mark = entry_ptr->mark;
    if(codec->prescreen_two_marks(mark, dict_mark)) {
       /* after prescreening with size, match the pixel maps */ 
    	cur_mismatch = codec->match_two_marks(mark, dict_mark);
    	if(cur_mismatch < lowest_mismatch) {
		*match_index = i;
		lowest_mismatch = cur_mismatch;
    	}
    }
    if(cur_mismatch == 0.) /* already found a full match */
    	break;
  }
  
  if(lowest_mismatch > codec->mismatch_thres) 
    *match_index = -1;
  *mm = lowest_mismatch;
}

/* Subroutine:	int prescreen_two_marks()
   Function:	prescreen two marks by their size difference
   Input:	two marks to be screened
   Output:	binary decision if they pass the pre-screening or not
*/
int prescreen_two_marks(Mark *mark1, Mark *mark2)
{ 
//  if(abs(mark1->width - mark2->width ) <= 2 &&
//     abs(mark1->height- mark2->height) <= 2) return TRUE;
  if(abs(mark1->width - mark2->width ) <= 0 &&
     abs(mark1->height- mark2->height) <= 0) return TRUE;
  else return FALSE;
}

/* Subroutine:	int prescreen_two_marks_feature()
   Function:	prescreen two marks by more properties, besides size, also
   		check their feature values (for now only number of holes)
   Input:	two marks to be screened
   Output:	binary decision if they pass the pre-screening or not
*/
int prescreen_two_marks_feature(Mark *mark1, Mark *mark2)
{ 
//  if(abs(mark1->width - mark2->width ) <= 2 &&
//     abs(mark1->height- mark2->height) <= 2 &&
//     mark1->hole_num == mark2->hole_num) return TRUE;
  if(abs(mark1->width - mark2->width ) <= 0 &&
     abs(mark1->height- mark2->height) <= 0 &&
     mark1->hole_num == mark2->hole_num) return TRUE;
  else return FALSE;
}

/* Subroutine: 	float match_two_marks_XOR()
   Function:	calculate and return the mismatch score of two marks using
                Hamming distance as criterion
   Input:	two marks to be matched, mark1 is the input mark and mark2
                is the "template" or "reference" mark
   Output: 	the matching(mismatch) score
*/
float match_two_marks_XOR(Mark *mark1, Mark *mark2)
{
  float score;
  register int x, y;
  char *ptr1, *ptr2;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2;		/* align these points */

  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
  
  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c1.x>c2.x ? 0:c2.x-c1.x; 
  rm = (w1-c1.x)>(w2-c2.x) ? 0:(w2-c2.x)-(w1-c1.x);
  tm = c1.y>c2.y ? 0:c2.y-c1.y; 
  bm = (h1-c1.y)>(h2-c2.y) ? 0:(h2-c2.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*(w1+lm+rm)*(h1+tm+bm));
  if(!ptr1) 
    error("match_two_marks_XOR: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm, rm, tm, bm, ptr1);

  lm = c2.x>c1.x ? 0:c1.x-c2.x; 
  rm = (w2-c2.x)>(w1-c1.x) ? 0:(w1-c1.x)-(w2-c2.x);
  tm = c2.y>c1.y ? 0:c1.y-c2.y; 
  bm = (h2-c2.y)>(h1-c1.y) ? 0:(h1-c1.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*(w2+lm+rm)*(h2+tm+bm));
  if(!ptr2) 
    error("match_two_marks_XOR: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm, rm, tm, bm, ptr2);

  /* align the two pixel maps according to their lower-left corner */
  score = 0.;
  w = w2+lm+rm; h = h2+tm+bm;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) 
    	score += (ptr1[x+y*w] != ptr2[x+y*w]);

  free((void *)ptr1);
  free((void *)ptr2);
  return score/(float)(w*h);
}

extern void get_flip_candidates(char *, int, int, char *, int *);

/* Subroutine: 	float match_two_marks_LOSSY()
   Function:	calculate and return the mismatch score of two marks using
                Hamming distance as criterion. Used only in LOSSY compression. 
		Return the mismatch score as if shape unifying had been 
		carried out.
   Input:	two marks to be matched, mark1 is the input mark and mark2
                is the "template" or "reference" mark
   Output: 	the matching(mismatch) score
*/
float match_two_marks_LOSSY(Mark *mark1, Mark *mark2)
{
  int score;
  register int x, y;
  char *ptr1, *ptr2;
  char *err;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2, c;		/* Geometric centers */
  char cand[20000];
  int perf;
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
  
  w = w1 > w2? w1:w2; h = h1 > h2? h1:h2;
  if(w % 2) c.x = w/2; 
  else c.x = w/2-1;
  c.y = h/2;

  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c.x-c1.x; rm = (w-c.x)-(w1-c1.x);
  tm = c.y-c1.y; bm = (h-c.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr1) error("match_two_marks_LOSSY: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm, rm, tm, bm, ptr1);

  lm = c.x-c2.x; rm = (w-c.x)-(w2-c2.x);
  tm = c.y-c2.y; bm = (h-c.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr2) error("match_two_marks_LOSSY: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm, rm, tm, bm, ptr2);

  /* allocate memory for error map */
  err = (char *)malloc(sizeof(char)*w*h);
  if(!err) error("match_two_marks_LOSSY: cannot allocate memory\n");

  /* calculate error bitmap */
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) 
    	err[x+y*w] = (ptr1[x+y*w] != ptr2[x+y*w]);

  if(w*h > 20000) 
    error("match_two_marks_LOSSY: candidate buffer is too short\n");
  get_flip_candidates(err, w, h, cand, &perf);
  
  /* calculate mismatch score */
  score = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) 
    	if(err[x+y*w] && !cand[x+y*w]) score++;
    
  free((void *)ptr1);
  free((void *)ptr2);
  free((void *)err);
  
  return (float)score/(float)(w*h);
}

int black_pixel(char *, int, int, int, int);

/* Subroutine: 	float match_two_marks_WXOR()
   Function:	calculate and return the mismatch score of two marks using
                weighted Hamming distance as criterion
   Input:	two marks to be matched
   Output: 	the matching(mismatch) score
*/
float match_two_marks_WXOR(Mark *mark1, Mark *mark2)
{
  int score, total1, total2, total;
  register int x, y;
  char *ptr1, *ptr2;
  char *err, *eptr;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2, c;		/* Geometric centers */
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
  
  w = w1 > w2? w1:w2; h = h1 > h2? h1:h2;
  if(w % 2) c.x = w/2; 
  else c.x = w/2-1;
  c.y = h/2;

  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c.x-c1.x; rm = (w-c.x)-(w1-c1.x);
  tm = c.y-c1.y; bm = (h-c.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr1) error("match_two_marks_WXOR: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm, rm, tm, bm, ptr1);

  lm = c.x-c2.x; rm = (w-c.x)-(w2-c2.x);
  tm = c.y-c2.y; bm = (h-c.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr2) error("match_two_marks_WXOR: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm, rm, tm, bm, ptr2);

  /* allocate memory for error map */
  err = (char *)malloc(sizeof(char)*w*h);
  if(!err) error("match_two_marks_WXOR: cannot allocate memory\n");
  eptr = err;

  /* calculate error map and total # of black pixels in both marks */
  total1 = total2 = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) {
      if(ptr1[x+y*w]) total1++;
      if(ptr2[x+y*w]) total2++;
      *eptr++ = (ptr1[x+y*w] != ptr2[x+y*w]);
    }

  /* calculate weighted sum of mismatch pixels */
  eptr = err; score = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) 
      if(err[x+y*w]) {
	score++;
	score += black_pixel(err, w, h, x-1, y-1);
	score += black_pixel(err, w, h, x,   y-1);
	score += black_pixel(err, w, h, x+1, y-1);
	score += black_pixel(err, w, h, x-1, y  );
	score += black_pixel(err, w, h, x+1, y  );
	score += black_pixel(err, w, h, x-1, y+1);
	score += black_pixel(err, w, h, x,   y+1);
	score += black_pixel(err, w, h, x+1, y+1);
      }
  
  free((void *)ptr1);
  free((void *)ptr2);
  free((void *)err);

  total = (total1+total2) >> 1;
  return (float)score/(float)(9*total);
}

/* Subroutine: 	float match_two_marks_WAN()
   Function:	calculate and return the mismatch score of two marks using
                "weighted and not" Hamming distance as criterion
   Input:	two marks to be matched
   Output: 	the matching(mismatch) score
*/
float match_two_marks_WAN(Mark *mark1, Mark *mark2)
{
  int score1, score2;
  int total1, total2, total;
  register int x, y;
  char *ptr1, *ptr2;
  char *err1, *err2, *eptr1, *eptr2;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2, c;		/* Geometric centers */
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;

  w = w1 > w2? w1:w2; h = h1 > h2? h1:h2;
  if(w % 2) c.x = w/2; 
  else c.x = w/2-1;
  c.y = h/2;

  /* zero extend the two bitmaps to the same size */ 
  lm = c.x-c1.x; rm = (w-c.x)-(w1-c1.x);
  tm = c.y-c1.y; bm = (h-c.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr1) 
    error("match_two_marks_WAN: unable to allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm, rm, tm, bm, ptr1);
  
  lm = c.x-c2.x; rm = (w-c.x)-(w2-c2.x);
  tm = c.y-c2.y; bm = (h-c.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr2) 
    error("match_two_marks_WAN: unable to allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm, rm, tm, bm, ptr2);

  /* allocate memory for error maps */
  err1 = (char *)malloc(sizeof(char)*w*h);
  err2 = (char *)malloc(sizeof(char)*w*h);
  if(!err1 || !err2) 
    error("match_two_marks_WAN: cannot allocate memory\n");
  eptr1 = err1; eptr2 = err2;

  /* calculate the 2 error maps and total # of black pixels in both marks */
  total1 = total2 = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) {
      if(ptr1[x+y*w]) {
	total1++;
	*eptr1++ = !ptr2[x+y*w];
      }
      else *eptr1++ = 0;
      if(ptr2[x+y*w]) {
	total2++;
	*eptr2++ = !ptr1[x+y*w];
      }
      else *eptr2++ = 0;
    }

  /* calculate weighted sum of mismatch pixels */
  score1 = score2 = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) { 
      if(err1[x+y*w]) {
	score1++;
	score1 += black_pixel(err1, w, h, x-1, y-1);
	score1 += black_pixel(err1, w, h, x,   y-1);
	score1 += black_pixel(err1, w, h, x+1, y-1);
	score1 += black_pixel(err1, w, h, x-1, y  );
	score1 += black_pixel(err1, w, h, x+1, y  );
	score1 += black_pixel(err1, w, h, x-1, y+1);
	score1 += black_pixel(err1, w, h, x,   y+1);
	score1 += black_pixel(err1, w, h, x+1, y+1);
      }
      if(err2[x+y*w]) {
	score2++;
	score2 += black_pixel(err2, w, h, x-1, y-1);
	score2 += black_pixel(err2, w, h, x,   y-1);
	score2 += black_pixel(err2, w, h, x+1, y-1);
	score2 += black_pixel(err2, w, h, x-1, y  );
	score2 += black_pixel(err2, w, h, x+1, y  );
	score2 += black_pixel(err2, w, h, x-1, y+1);
	score2 += black_pixel(err2, w, h, x,   y+1);
	score2 += black_pixel(err2, w, h, x+1, y+1);
      }
    }
 
  free((void *)ptr1);
  free((void *)ptr2);
  free((void *)err1);
  free((void *)err2);

  total = (total1+total2) >> 1;
  return (float)(score1+score2)/(float)(9*total);
}

/* decide if the pixel at the given position is black */
int black_pixel(char *bitmap, int w, int h, int x, int y)
{
  /* borders are always taken as white */
  if(x < 0 || x >= w || y < 0 || y >= h) return FALSE;
  
  if(bitmap[x+y*w]) return TRUE;
  else return FALSE;
}

/* Subroutine: 	float match_two_marks_ENT()
   Function:	calculate and return the first-order entropy based mismatch 
   		score of two marks 
   Input:	two marks to be matched, mark1 is the input mark and mark2
                is the "template" or "reference" mark
   Output: 	the mismatch score
*/
float match_two_marks_ENT(Mark *mark1, Mark *mark2)
{
  float score;
  register int x, y;
  char *ptr1, *ptr2;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2, c;		/* Geometric centers */
  int count0,  count1;
  int count01, count10;
  extern float ent(float);
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
    
  w = w1 > w2? w1:w2; h = h1 > h2? h1:h2;
  if(w % 2) c.x = w/2; 
  else c.x = w/2-1;
  c.y = h/2;

  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c.x-c1.x; rm = (w-c.x)-(w1-c1.x);
  tm = c.y-c1.y; bm = (h-c.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr1) error("match_two_marks_XOR: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm, rm, tm, bm, ptr1);

  lm = c.x-c2.x; rm = (w-c.x)-(w2-c2.x);
  tm = c.y-c2.y; bm = (h-c.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*w*h);
  if(!ptr2) error("match_two_marks_XOR: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm, rm, tm, bm, ptr2);

  /* align the two pixel maps according to their lower-left corner */
  count0 = count1 = 0;
  count01 = count10 = 0;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) {
    	if(ptr2[x+y*w]) {
	  count1++;
	  if(!ptr1[x+y*w]) count01++;
	}
	else {
	  count0++;
	  if(ptr1[x+y*w]) count10++;
	}
    }
    
  free((void *)ptr1);
  free((void *)ptr2);
  
  score = ent((float)count01/(float)count1)*(float)count1 +
          ent((float)count10/(float)count0)*(float)count0;
  
  return score/(float)(w*h);
}


/* Subroutine:	float match_two_marks_LOCAL()
   Function:	match two marks based on their local properties
   Input:	two marks to be matched
   Output:	TRUE for match or FALSE for mismatch
*/
float match_two_marks_LOCAL(Mark *mark1, Mark *mark2) 
{
  register int x, y;
  char *bmp1, *bmp2;
  char *ptr1, *ptr2;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2;		/* align these points */
  int strict, done, count;
  int score;
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
    
  /* for smaller marks, we apply a more strict match, which is no neighboring
     pixel can have a different color if the current pixel is already 
     different; otherwise, at most 1 neighboring pixel can differ in addition
     to the current pixel */
  if(w1 <= 12 && h1 <= 12) strict = TRUE;
  else strict = FALSE;
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
  
  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c1.x>c2.x ? 0:c2.x-c1.x; 
  rm = (w1-c1.x)>(w2-c2.x) ? 0:(w2-c2.x)-(w1-c1.x);
  tm = c1.y>c2.y ? 0:c2.y-c1.y; 
  bm = (h1-c1.y)>(h2-c2.y) ? 0:(h2-c2.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*(w1+lm+rm+2)*(h1+tm+bm+2));
  if(!ptr1) 
    error("match_two_marks_LOCAL: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm+1, rm+1, tm+1, bm+1, ptr1);
  bmp1 = ptr1;
  
  lm = c2.x>c1.x ? 0:c1.x-c2.x; 
  rm = (w2-c2.x)>(w1-c1.x) ? 0:(w1-c1.x)-(w2-c2.x);
  tm = c2.y>c1.y ? 0:c1.y-c2.y; 
  bm = (h2-c2.y)>(h1-c1.y) ? 0:(h1-c1.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*(w2+lm+rm+2)*(h2+tm+bm+2));
  if(!ptr2) 
    error("match_two_marks_LOCAL: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm+1, rm+1, tm+1, bm+1, ptr2);
  bmp2 = ptr2;
  
  /* tighter threshold */
  if(strict) {
    score = 0; done = FALSE;
    w = w2+lm+rm; h = h2+tm+bm;
    ptr1 += w+2+1; ptr2 += w+2+1;
    for(y = 0; y < h; y++) { 
      for(x = 0; x < w && !done; x++) {
        if(ptr1[x] != ptr2[x]) {
          /* check its 4-neighbors */
	  if(ptr1[x-1] != ptr2[x-1]) {score = -1; done = TRUE;}
	  else if(ptr1[x+1] != ptr2[x+1]) {score = -1; done = TRUE;}
	  else if(ptr1[x-w-2] != ptr2[x-w-2]) {score = -1; done = TRUE;}
	  else if(ptr1[x+w+2] != ptr2[x+w+2]) {score = -1; done = TRUE;}
          else score++;
	}
      }
      if(done) break;
      ptr1 += (w+2); ptr2 += (w+2);
    }
  }
  
  /* looser threshold */
  else {
    score = 0; done = FALSE;
    w = w2+lm+rm; h = h2+tm+bm;
    ptr1 += w+2+1; ptr2 += w+2+1;
    for(y = 0; y < h; y++) { 
      for(x = 0; x < w && !done; x++) { 
        if(ptr1[x] != ptr2[x]) {
          /* check its 4-neighbors */
          count = 0;
	  if(ptr1[x-1] != ptr2[x-1]) count++;
	  if(ptr1[x+1] != ptr2[x+1]) count++;
	  if(ptr1[x-w-2] != ptr2[x-w-2]) count++;
	  if(ptr1[x+w+2] != ptr2[x+w+2]) count++;
	
	  if(count > 1) {score = -1; done = TRUE;}
	  else score += 2;
	}
      }
      if(done) break;
      ptr1 += (w+2); ptr2 += (w+2);
    }
  }
  
  free((void *)bmp1);
  free((void *)bmp2);
  if(score == -1) return -1.;
  else return (float)score/(float)(w*h);
}

/* Subroutine:	float match_two_marks_CLUSTER()
   Function:	subroutine for fast match between two marks based on 
   		``local'' properties, i.e., if the difference bitmap contains
		errors bigger than a certain size, then the matching process
		will quit promptly and declare a mismatch. Only marks similiar
		enough will be examined thoroughly and declared as a match
		with a matching score
   Input:	two marks to be matched
   Output:	mismatch score, -1.0 for marks that don't match
*/
float match_two_marks_CLUSTER(Mark *mark1, Mark *mark2) 
{
  register int x, y;
  char *bmp1, *bmp2;
  char *ptr1, *ptr2;
  int w1, w2, h1, h2;
  int lm, rm, tm, bm;
  register int w, h;
  PixelCoord c1, c2;		/* align these points */
  PixelCoord seed;
  int thres, done, count;
  int score;

  extern int trace_error_cluster(char *, char *, int, int, PixelCoord, int, int *);
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
    
  /* use a stricter threshold for smaller marks and a looser threshold 
     for bigger ones */
  if(w1 <= 12 && h1 <= 12) thres = codec->lth;
  else thres = codec->hth;
  
  w1 = mark1->width; h1 = mark1->height;
  w2 = mark2->width; h2 = mark2->height;
  
  c1 = mark1->c; c2 = mark2->c;
  
  /* zero-extend both bitmaps (reference) to the same common size */ 
  lm = c1.x>c2.x ? 0:c2.x-c1.x; 
  rm = (w1-c1.x)>(w2-c2.x) ? 0:(w2-c2.x)-(w1-c1.x);
  tm = c1.y>c2.y ? 0:c2.y-c1.y; 
  bm = (h1-c1.y)>(h2-c2.y) ? 0:(h2-c2.y)-(h1-c1.y);
  ptr1 = (char *)malloc(sizeof(char)*(w1+lm+rm+2)*(h1+tm+bm+2));
  if(!ptr1) 
    error("match_two_marks_CLUSTER: cannot allocate memory\n");
  copy_data_with_margin(mark1->data, w1, h1, lm+1, rm+1, tm+1, bm+1, ptr1);
  bmp1 = ptr1;
  
  lm = c2.x>c1.x ? 0:c1.x-c2.x; 
  rm = (w2-c2.x)>(w1-c1.x) ? 0:(w1-c1.x)-(w2-c2.x);
  tm = c2.y>c1.y ? 0:c1.y-c2.y; 
  bm = (h2-c2.y)>(h1-c1.y) ? 0:(h1-c1.y)-(h2-c2.y);
  ptr2 = (char *)malloc(sizeof(char)*(w2+lm+rm+2)*(h2+tm+bm+2));
  if(!ptr2) 
    error("match_two_marks_CLUSTER: cannot allocate memory\n");
  copy_data_with_margin(mark2->data, w2, h2, lm+1, rm+1, tm+1, bm+1, ptr2);
  bmp2 = ptr2;

  w = w2+lm+rm; h = h2+tm+bm;    
  score = 0; done = FALSE;
  ptr1 += w+2+1; ptr2 += w+2+1;
  for(y = 0; y < h; y++) { 
    for(x = 0; x < w && !done; x++) {
      if(ptr1[x] != ptr2[x]) {
	seed.x = x; seed.y = y;
        done = trace_error_cluster(bmp1+w+3, bmp2+w+3, w, h, seed, thres, &count);
        if(!done) score += count;
      }
    }
    if(done) break;
    ptr1 += (w+2); ptr2 += (w+2);
  }
  
  free((void *)bmp1);
  free((void *)bmp2);
  
  if(done) return 1.;
  else return (float)score/(float)(w*h);
}
