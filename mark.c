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
#include "mark.h"

#ifdef NEVER
Boundary *boundary_tracing_8(PixelCoord), *boundary_tracing(PixelCoord);
Mark extract_mark_8(PixelCoord, Boundary *);
#endif
void boundary_tracing_4(char *, int, int, PixelCoord);
void extract_mark(PixelCoord);
void extract_mark_4(PixelCoord);
int  trace_error_cluster(char *, char *, int, int, PixelCoord, int, int *);

void eliminate_speck(Window);
void edge_smoothing();
void modify_refine_mark(Mark *, Mark *, int *, int *, int *);
void modify_direct_mark(Mark *, int *);

int turn(int, int), reverse(int);
PixelCoord move(PixelCoord, int);
int neighbors_all_white(char *, int, int, PixelCoord);
void get_bitmap_margin(char *, int, int, int *, int *, int *, int *);

void get_mark_centroid(Mark *);
void get_mark_center(Mark *);
void get_mark_hole_num(Mark *);

extern void error(char *);
extern void copy_data_with_margin(char *, int, int, int, int, int, int, char *);

static Boundary boundary;
#ifdef DEBUG
static int total_modified = 0;
#endif

extern Codec *codec;
extern PixelMap *doc_buffer;
extern MarkList *all_marks;

#ifdef NEVER
/* Subroutine:	Boundary *boundary_tracing()
   Function:	call the correct boundary tracing algorithm according to
   		connectivity rule assigned
   Input:	seed
   Output:	boundary link
*/
Boundary *boundary_tracing(PixelCoord seed)
{
  Boundary *link;
  
  if(codec->connectivity == EIGHT_CONNECT) link = boundary_tracing_8(seed);
  else link = boundary_tracing_4(doc_buffer->data, 
  				doc_buffer->width, doc_buffer->height, seed);
  
  return link;
}
#endif

/* Subroutine: 	Mark extract_mark()
   Function:	call the correct mark extraction algorithm according to
   		connectivity rule assigned
   Input:	the seed
   Output:	none
*/ 
void extract_mark(PixelCoord seed)
{
  boundary_tracing_4(doc_buffer->data, 
    doc_buffer->width, doc_buffer->height, seed);
  extract_mark_4(seed);
}

#ifdef NEVER
/* Subroutine: 	Boundary *boundary_tracing_8()
   Function: 	trace a boundary with the 8-connection rule
   Input:	seed coordinates(relative to the image strip buffer)
   Output:	link of boundary points
*/
Boundary *boundary_tracing_8(PixelCoord seed)
{
  Boundary 	*head, *next;
  register int 	cur_dir, temp_dir;
  PixelCoord 	cur_posi, temp_posi;
  register int	width;

  cur_posi = move(seed, UP);
  head = get_boundary_node(cur_posi);
  next = head;

  width = doc_buffer->width+2;
  cur_dir = RIGHT;

  do {
  	temp_dir = turn(cur_dir, RIGHT);
	temp_posi = move(cur_posi, temp_dir);
  	if(!doc_buffer->data[temp_posi.y*width+temp_posi.x]) 
	    cur_dir = temp_dir;
	else {
	    temp_dir = cur_dir;
	    temp_posi = move(cur_posi, temp_dir);
	    if(!doc_buffer->data[temp_posi.y*width+temp_posi.x]) 
	        cur_dir = temp_dir;
	    else {
		temp_dir = turn(cur_dir, LEFT);
	    	temp_posi = move(cur_posi, temp_dir);
		if(!doc_buffer->data[temp_posi.y*width+temp_posi.x]) 
		    cur_dir = temp_dir;
		else cur_dir = reverse(cur_dir);
	    }
	}
	
	cur_posi = move(cur_posi, cur_dir);
	next->next = get_boundary_node(cur_posi);
	next = next->next;
  } while( cur_posi.x != seed.x || cur_posi.y != seed.y );

/*  
  #ifdef NEVER
  next = head;
  while(next) {
    fprintf(stderr, "(%d, %d)->", next->coord.x, next->coord.y);
    next = next->next;
  }
  printf("\n");
  #endif */
  return head;
}
#endif

/* Subroutine:	void boundary_tracing_4()
   Function:	trace the mark boundary using 4-connection rule
   Input:	seed point
   Output:	boundary information
*/
void boundary_tracing_4(char *pic, int pw, int ph, PixelCoord seed)
{
  register int 	cur_dir, temp_dir;
  PixelCoord 	cur_posi, temp_posi;
  register int	width;
  
  boundary.coord[0] = seed;
  boundary.length = 1;
  /* if this is an isloated black pixel, return now	*/
  if(neighbors_all_white(pic, pw, ph, seed)) return;

  width = pw+2;
  cur_dir = RIGHT;
  cur_posi = seed;

  do {
  	temp_dir = turn(cur_dir, LEFT);
	temp_posi = move(cur_posi, temp_dir);
  	if(pic[temp_posi.y*width+temp_posi.x]) 
	    cur_dir = temp_dir;
	else {
	    temp_dir = cur_dir;
	    temp_posi = move(cur_posi, cur_dir);
	    if(pic[temp_posi.y*width+temp_posi.x]) 
	    	cur_dir = temp_dir;
	    else {
		temp_dir = turn(cur_dir, RIGHT);
	    	temp_posi = move(cur_posi, temp_dir);
		if(pic[temp_posi.y*width+temp_posi.x]) 
		    cur_dir = temp_dir;
		else cur_dir = reverse(cur_dir);
	    }
	}
	
	cur_posi = move(cur_posi, cur_dir);
	boundary.coord[boundary.length] = cur_posi;
	boundary.length++;
	if(boundary.length == MAX_BOUND_POINT)
	  error("boundary_tracing_4: boundary is too long to be buffered\n");
  }  while(cur_posi.x != seed.x || cur_posi.y != seed.y);
}  

/* Subroutine:	int neighbors_all_white()
   Function:	decide if the input pixel is an isolated black point
   Input:	the pixel
   Output:	decision, TRUE or FALSE
*/
int neighbors_all_white(char *pic, int pw, int ph, PixelCoord point)
{
  register int width;
   
  width = pw + 2;
  
  if(codec->connectivity == FOUR_CONNECT) {
    if(!pic[(point.x-1)+point.y*width] &&
       !pic[(point.x+1)+point.y*width] &&
       !pic[point.x+(point.y-1)*width] &&
       !pic[point.x+(point.y+1)*width]) return TRUE;
    else return FALSE;
  }
  else {
    if(!pic[(point.x-1)+point.y*width] &&
       !pic[(point.x+1)+point.y*width] &&
       !pic[point.x+(point.y-1)*width] &&
       !pic[point.x+(point.y+1)*width] &&
       !pic[(point.x-1)+(point.y-1)*width] &&
       !pic[(point.x+1)+(point.y-1)*width] &&
       !pic[(point.x-1)+(point.y+1)*width] &&
       !pic[(point.x+1)+(point.y+1)*width]) return TRUE;
    else return FALSE;
  }
}

/* Subroutine: 	int turn()
   Function:  	get the new direction from "cur_dir" when turned "dir"
   Input:	current direction and turning direction
   Output:	the new direction after turn
*/
int turn(int cur_dir, int dir)
{
  switch(cur_dir) {
  	case UP:
	    switch(dir) {
		case LEFT:
			return LEFT;
			break;
		case RIGHT:
			return RIGHT;
			break;
	    }
	case DOWN:
	    switch(dir) {
		case LEFT:
			return RIGHT;
			break;
		case RIGHT:
			return LEFT;
			break;
	    }
	case LEFT:
	    switch(dir) {
		case LEFT:
			return DOWN;
			break;
		case RIGHT:
			return UP;
			break;
	    }
	case RIGHT:
	    switch(dir) {
		case LEFT:
			return UP;
			break;
		case RIGHT:
			return DOWN;
			break;
	    }
  }
  return 0;	/* will never reach here, just to return cleanly */
}

/* Subroutine: 	int reverse()
   Function:	reverse the current direction
   Input:	current direction
   Output:	new direction
*/
int reverse(int cur_dir) {
  switch(cur_dir) {
  	case UP:
	    return DOWN;
	    break;
	case DOWN:
	    return UP;
	    break;
	case LEFT:
	    return RIGHT;
	    break;
	case RIGHT:
	    return LEFT;
	    break;
  }
  return 0; 	/* will never reach here, just to return cleanly */
}

/* Subroutine: 	PixelCoord move()
   Function:	Move to the next position from "cur_posi" in the direction of "dir"
   Input: 	current position and direction
   Output: 	new position
*/
PixelCoord move(PixelCoord cur_posi, int dir)
{
  PixelCoord new_posi;
  
  switch(dir) {
  	case UP:
	     new_posi.x = cur_posi.x;
	     new_posi.y = cur_posi.y - 1;
	     break;
	case DOWN:
	     new_posi.x = cur_posi.x;
	     new_posi.y = cur_posi.y + 1; 
	     break;
	case LEFT:
	     new_posi.x = cur_posi.x - 1;
	     new_posi.y = cur_posi.y;
	     break;
	case RIGHT:
	     new_posi.x = cur_posi.x + 1;
	     new_posi.y = cur_posi.y;
	     break;
  }
  
  return new_posi;
}

/**************************************************************************
	Below is the subroutine mark_extraction() that is modified 
	from Paul Heckbert's seed fill algorithm from "Computer Gems",
	Acedemic Press, 1990. Four-connection is used. 
**************************************************************************/

/*
 * A Seed Fill Algorithm
 * by Paul Heckbert
 * from "Graphics Gems", Academic Press, 1990
 */
 
/* function declarations		*/
Window get_bounding_window(void);
extern char read_pixel_from_buffer(int, int);
extern void write_pixel_to_buffer(int, int, char);
void write_mark_pixel(Mark *, int, int, char);
char read_mark_pixel(Mark *, int, int);
void fill_mark_with_white(Mark *);

#define MAX 10000		/* max depth of stack */

Segment stack[MAX], *sp = stack;	/* stack of filled segments */

#define PUSH(Y, XL, XR, DY)	/* push new segment on stack */ \
    if (sp<stack+MAX && Y+(DY)>=win.ul.y && Y+(DY)<=win.lr.y) \
    {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)	/* pop segment off stack */ \
    {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}


/* Subroutine:  Mark extract_mark_4()
   Function: 	extract the mark defined by boundary using 4 connectivity
   Input: 	the seed
   Output:	none
*/ 
void  extract_mark_4(PixelCoord seed)
{
  Mark *mark;
  Window win;
  
  int l, x1, x2, dy;
  int x, y;
  char value;

  extern void write_into_cleanup(Mark *);
  extern int is_speck(Mark *);

  /* get the bounding window of the mark, in absolute coordinates */
  win = get_bounding_window();

  if(all_marks->mark_num == MAX_MARK_NUM) 
    error("extract_mark_4: MarkList buffer is full, cannot continue!\n");
    
  mark = all_marks->marks + all_marks->mark_num;
  mark->upleft = win.ul; 
  mark->width = win.lr.x-win.ul.x+1;
  mark->height = win.lr.y-win.ul.y+1;
  
  #ifdef NEVER
  /* check mark's size, if too small, don't extract it and return */
  if(mark->width <= 2 && mark->height <= 2) {
    /* enable cleanup coding since there's left-overs in the buffer */
    if(!codec->lossy) codec->cleanup_coding = TRUE;
    else eliminate_speck(win);
    codec->speck_mark++;
    return;
  }
  #endif
  
  mark->data = (char *)malloc(sizeof(char)*mark->width*mark->height);
  if(!mark->data) 
    error("extract_mark_4: Cannot allocate memory for new mark\n");
  memset(mark->data, 0, mark->width*mark->height*sizeof(char));
  
  x = seed.x; y = seed.y;	/* relative position in the strip buffer */
  PUSH(y, x, x, 1);		/* needed in some cases */
  PUSH(y+1, x, x, -1);		/* seed segment (popped 1st) */

  while (sp>stack) {
	/* pop segment off stack and fill a neighboring scan line */
	POP(y, x1, x2, dy);
	/*
	 * segment of scan line y-dy for x1<=x<=x2 was previously filled,
	 * now explore adjacent pixels in scan line y
	 */
	for (x=x1; x>=win.ul.x && (value = read_pixel_from_buffer(x, y)); x--) {
	    /* write the mark	*/
	    write_mark_pixel(mark, x-mark->upleft.x, y-mark->upleft.y, value);
	    /* reset the same pixel in doc_buffer 	*/
	    write_pixel_to_buffer(x, y, 0);
	}
	if (x>=x1) goto skip;	
	l = x+1;
	if (l<x1) PUSH(y, l, x1-1, -dy);		/* leak on left? */
	x = x1+1;
	do {
	    for (; x<=win.lr.x && (value = read_pixel_from_buffer(x, y)); x++) {
		/* write the mark data buffer	*/
	    	write_mark_pixel(mark, x-mark->upleft.x, y-mark->upleft.y, value);
	    	/* reset the corresponding positions in 	*
	     	 * the original image and the image strip	*/
	    	write_pixel_to_buffer(x, y, 0);
	    }
	    PUSH(y, l, x-1, dy);
	    if (x>x2+1) PUSH(y, x2+1, x-1, -dy);	/* leak on right? */
skip:	    for (x++; x<=x2 && !(read_pixel_from_buffer(x, y)); x++);
	    l = x;
	} while (x<=x2);
  }
  
  /* if this mark is a speck, put it into the cleanup page or erase it */
  if(is_speck(mark)) {
    if(!codec->lossy) {
      codec->cleanup_coding = TRUE;
      write_into_cleanup(mark);
      codec->report.speck_marks++;
    }
    free((void *)mark->data);
  }
  
  else { /* otherwise, put the newly extracted mark into MarkList */
    /* write the reference point, currently the LOWERLEFT corner */
    mark->ref.x = mark->upleft.x;
    mark->ref.y = mark->upleft.y+mark->height-1;
    
    /* calculate the mark's center or centroid */
    if(codec->align == CENTER) get_mark_center(mark);
    else get_mark_centroid(mark);
    
    get_mark_hole_num(mark);
    
    all_marks->mark_num++;
  }
}
 
/*************************************************************************
	The following codes are not adopted from Paul Heckbert
	--they are my own work
*************************************************************************/


/* Subroutine: 	Window get_bounding_window()
   Function: 	get the bounding box of a boundary describe by a link
   Input:	none
   Output:	the bounding window
*/
Window get_bounding_window()
{
  Window window;
  int i;
  
  window.ul = boundary.coord[0]; window.lr = boundary.coord[0];
  for(i = 0; i < boundary.length; i++) {
  	if( boundary.coord[i].x < window.ul.x ) 
		window.ul.x = boundary.coord[i].x;
	else if( boundary.coord[i].x > window.lr.x ) 
		window.lr.x = boundary.coord[i].x;
	if( boundary.coord[i].y < window.ul.y ) 
		window.ul.y = boundary.coord[i].y;
	else if( boundary.coord[i].y > window.lr.y ) 
		window.lr.y = boundary.coord[i].y;
  }
  
  return window;
}

/* Subroutine:  void write_mark_pixel()
   Function:	does coordinates translation and writes a pixel value 
   		into mark 
   Input:	the mark buffer, the position and the new value
   Output:	none
*/
void write_mark_pixel(Mark *mark, int x, int y, char value)
{
  mark->data[y*mark->width + x] = value;
}

/* Subroutine:	char read_mark_pixel()
   Function:	return the pixel value in the given mark bitmap at the given
   		position
   Input:	the mark buffer and the position
   Output:	pixel value
*/
char read_mark_pixel(Mark *mark, int x, int y)
{
  return mark->data[y*mark->width + x];
}

/* Subroutine:  void eliminate_speck()
   Function:	eliminate very small marks for lossy coding
   Input:	bounding window for the small mark
   Output:	none
*/
void eliminate_speck(Window win)
{
  register int x, y;

  for(y = win.ul.y; y <= win.lr.y; y++) 
    for(x = win.ul.x; x <= win.lr.x; x++)
	if(read_pixel_from_buffer(x, y)) 
	  write_pixel_to_buffer(x, y, 0); 
}

int is_protruding_single(int, int);
extern char read_pixel_from_buffer(int, int);
extern void write_pixel_to_buffer(int, int, char);

/* Subroutine:  void edge_smoothing()
   Function:	eliminate single protuding pixels(black or white) within the
   		entire buffer, single protruding pixels are defined as following
		one of these patterns, where X is one color and O is the other. 
		  XXX 		XOO	       OOX		OOO
		  OXO	or	XXO	or     OXX	or 	OXO
		  OOO		XOO	       OOX		XXX
   Input:	none
   Output:	none
*/
void edge_smoothing()
{
  register int x, y;
  char *dptr;
  int width, height;
  
  width = doc_buffer->width; height = doc_buffer->height;
  dptr = doc_buffer->data;
  for(y = 0; y < height; y++) {
    for(x = 0; x < width; x++)
      /* check every pixel */
      if(is_protruding_single(x, y)) dptr[x] = !dptr[x];
    dptr += (width+2);
  }
}

/* Subroutine:	int is_protruding_single()
   Function:	decides if the pixel at the given position is a single isolated 
   		protruding pixel
   Input:	mark buffer and the pixel coordinates
   Output:	decision
*/
int is_protruding_single(int x, int y)
{
  char n[9];
  /* neighbors are label as follow
     0 1 2 
     7 8 3
     6 5 4
  */
  
  n[0] = read_pixel_from_buffer(x-1, y-1);
  n[1] = read_pixel_from_buffer(x,   y-1);
  n[2] = read_pixel_from_buffer(x+1, y-1);
  n[3] = read_pixel_from_buffer(x+1, y  );
  n[4] = read_pixel_from_buffer(x+1, y+1);
  n[5] = read_pixel_from_buffer(x,   y+1);
  n[6] = read_pixel_from_buffer(x-1, y+1);
  n[7] = read_pixel_from_buffer(x-1, y  );
  n[8] = read_pixel_from_buffer(x,   y  );
  
  if(((n[0]==n[1])&&(n[1]==n[2])&&(n[2]==n[8])&&(n[8]!=n[3])&&
      (n[3]==n[4])&&(n[4]==n[5])&&(n[5]==n[6])&&(n[6]==n[7])) ||
     ((n[2]==n[3])&&(n[3]==n[4])&&(n[4]==n[8])&&(n[8]!=n[5])&&
      (n[5]==n[6])&&(n[6]==n[7])&&(n[7]==n[0])&&(n[0]==n[1])) ||
     ((n[4]==n[5])&&(n[5]==n[6])&&(n[6]==n[8])&&(n[8]!=n[7])&&
      (n[7]==n[0])&&(n[0]==n[1])&&(n[1]==n[2])&&(n[2]==n[3])) ||
     ((n[6]==n[7])&&(n[7]==n[0])&&(n[0]==n[8])&&(n[8]!=n[1])&&
      (n[1]==n[2])&&(n[2]==n[3])&&(n[3]==n[4])&&(n[4]==n[5])))
       return TRUE;
       
  else return FALSE;
}

#define CANDIDATE_LEN 	30000

void get_flip_candidates(char *, int, int, char *, int *);

#ifndef NEVER
/* Subroutine:	void modify_refine_mark()
   Function:	modify the current mark data according to its reference, flip
   		the isolated differences(single pixel or group of 2 pixels)
   Input:	the two marks
   Output:	if the modification renders a perfect mark
*/
void modify_refine_mark(Mark *ref_mark, Mark *mark, int *perfect, 
  int *rdx, int *rdy)
{
  register int x, y;
  char *tptr, *mptr, *dptr, *cptr;
  char *tdata, *mdata, *diff;
  int tw, th, mw, mh, cw, ch;
  int lm, rm, tm, bm;
  int lm1, rm1, tm1, bm1;
  PixelCoord tc, mc, cc;	/* Geometric centers */
  char candidate[CANDIDATE_LEN];
    
  tw = ref_mark->width; th = ref_mark->height;
  mw = mark->width; mh = mark->height;
  tc = ref_mark->c; mc = mark->c;
  
  /* extend the two bitmaps to a common size */
  lm = tc.x>mc.x ? 0:mc.x-tc.x; 
  rm = (tw-tc.x)>(mw-mc.x) ? 0:(mw-mc.x)-(tw-tc.x); 
  tm = tc.y>mc.y ? 0:mc.y-tc.y; 
  bm = (th-tc.y)>(mh-mc.y) ? 0:(mh-mc.y)-(th-tc.y);
  tdata = (char *)malloc(sizeof(char)*(tw+lm+rm)*(th+tm+bm));
  if(!tdata) error("modify_refine_mark: cannot allocate memory\n");
  copy_data_with_margin(ref_mark->data, tw, th, lm, rm, tm, bm, tdata);

  cc.x = tc.x+lm; cc.y = tc.y+tm;
  
  lm = mc.x>tc.x ? 0:tc.x-mc.x; 
  rm = (mw-mc.x)>(tw-tc.x) ? 0:(tw-tc.x)-(mw-mc.x); 
  tm = mc.y>tc.y ? 0:tc.y-mc.y; 
  bm = (mh-mc.y)>(th-tc.y) ? 0:(th-tc.y)-(mh-mc.y);
  mdata = (char *)malloc(sizeof(char)*(mw+lm+rm)*(mh+tm+bm));
  if(!mdata) error("modify_refine_mark: cannot allocate memory\n");
  copy_data_with_margin(mark->data, mw, mh, lm, rm, tm, bm, mdata);

  cw = mw+lm+rm; ch = mh+tm+bm;
  
  mptr = mdata; diff = tdata; 
  dptr = diff; tptr = tdata;
  /* take the difference bitmap */
  for(y = 0; y < ch; y++) 
    for(x = 0; x < cw; x++) 
	*dptr++ = ((*tptr++) != (*mptr++));
  
  /* obtain the flip candidates */
  if(cw*ch > CANDIDATE_LEN) 
    error("modify_refine_mark: candidate buffer is too short\n");
  get_flip_candidates(diff, cw, ch, candidate, perfect);

  #ifdef DEBUG
  cptr = candidate;
  for(y = 0; y < ch; y++) {
    for(x = 0; x < cw; x++) 
      if(cptr[x]) total_modified++;
    cptr += cw;
  }
  #endif
  
  /* modify the current symbol according to the candidates */
  if(*perfect) {
    /* if this's a perfect symbol, then copy its reference */
    free((void *)mark->data);
    mark->data = ref_mark->data;
    if(mw != tw || mh != th) {
      mark->width = tw; mark->height = th;
      mark->upleft.x -= tc.x-mc.x; mark->upleft.y -= tc.y-mc.y;
      mark->ref.x = mark->upleft.x; 
      mark->ref.y = mark->upleft.y+mark->height-1;
    }
    mark->c = tc;
  }
  else {
    /* if this isn't a perfect symbol, then modify its content */
    cptr = candidate; mptr = mdata;
    for(y = 0; y < ch; y++) {
      for(x = 0; x < cw; x++) 
        if(cptr[x]) mptr[x] = !mptr[x];
      mptr += cw; cptr += cw;
    }
    
    /* check if it's necessary to change the current symbol's size */
    get_bitmap_margin(mdata, cw, ch, &lm1, &rm1, &tm1, &bm1);
    if(mw != cw-lm1-rm1 || mh != ch-tm1-bm1) {
      mark->width = cw-lm1-rm1; mark->height = ch-tm1-bm1;
  
      free((void *)mark->data);
      mark->data = (char *)malloc(sizeof(char)*mark->width*mark->height);
      if(!mark->data) 
        error("modify_refine_mark: cannot allocate memory\n");
    }
    
    /* now change the new symbol's bitmap */
    copy_data_with_margin(mdata, cw, ch, -lm1, -rm1, -tm1, -bm1, mark->data);
    mark->upleft.x -= (lm-lm1); mark->upleft.y -= (tm-tm1);  
    mark->ref.x = mark->upleft.x;
    mark->ref.y = mark->upleft.y+mark->height-1;
    
    /* cutting white margin may cause the alignment to change, adjust it back */
    if(codec->align == CENTER) get_mark_center(mark);
    else get_mark_centroid(mark);
    mc = mark->c;
    *rdx = cc.x-(mc.x+lm1); *rdy = cc.y-(mc.y+tm1); 
  }
  
  free((void *)mdata);
  free((void *)tdata);
}
#else
void modify_refine_mark(Mark *ref_mark, Mark *mark, int *good, int *rdx, int *rdy)
{
  register int x, y;
  char *tptr, *mptr, *dptr;
  char *tdata, *diff;
  int tw, th, mw, mh;
  int lm, rm, tm, bm;
  PixelCoord tc, mc;            /* Geometric centers */
  int posi;
  char candidate[CANDIDATE_LEN];

  tw = ref_mark->width; th = ref_mark->height;
  mw = mark->width; mh = mark->height;

  tc = ref_mark->c; mc = mark->c;

  /* take only useful amount of bitmap from the template */
  lm = mc.x-tc.x; rm = (mw-mc.x)-(tw-tc.x);
  tm = mc.y-tc.y; bm = (mh-mc.y)-(th-tc.y);
  tdata = (char *)malloc(sizeof(char)*mw*mh);
  if(!tdata) error("modify_symbol: cannot allocate memory\n");
  copy_data_with_margin(ref_mark->data, tw, th, lm, rm, tm, bm, tdata);

  mptr = mark->data; diff = tdata;
  dptr = diff; tptr = tdata;
  /* align the two pixel maps and take the difference between the mark and
   * the template */
  for(y = 0; y < mh; y++)
    for(x = 0; x < mw; x++)
        *dptr++ = ((*tptr++) != (*mptr++));

  /* obtain the flip candidates */
  if(mw*mh > CANDIDATE_LEN)
    error("modify_refine_mark: candidate buffer is too short\n");
  get_flip_candidates(diff, mw, mh, candidate, good);

  #ifdef DEBUG
  for(y = 0; y < mh; y++) {
    for(x = 0; x < mw; x++) {
      posi = y*mw+x;
      if(candidate[posi]) total_modified++;
    }
  }
  #endif
  
  /* modify the original symbol at candidate positions, along the way check
  if all the differing positions are claimed as candidates */
  mptr = mark->data;
  for(y = 0; y < mh; y++) {
    for(x = 0; x < mw; x++) {
        posi = y*mw+x;
        if(candidate[posi]) mptr[posi] = !mptr[posi];
    }
  }

  free((void *)tdata);
}
#endif

/* Subroutine:  void get_bitmap_margin()
   Function:	scan along a bitmap's borders and find its white margin if any
   Input:       bitmap data and size
   Output:	left/right/top/bottom margins
*/
void get_bitmap_margin(char *data, int w, int h, 
	int *lm, int *rm, int *tm, int *bm)
{
  register int x, y;
  
  /* left margin */
  for(x = 0, *lm = 0; x < w; x++, (*lm)++) {
    for(y = 0; y < h; y++) 
      if(data[y*w+x]) break;
    if(y < h) break;
  }
  
  /* right margin */
  for(x = w-1, *rm = 0; x >= 0; x--, (*rm)++) {
    for(y = 0; y < h; y++) 
      if(data[y*w+x]) break;
    if(y < h) break;
  }
  
  /* top margin */
  for(y = 0, *tm = 0; y < h; y++, (*tm)++) {
    for(x = 0; x < w; x++) 
      if(data[y*w+x]) break;
    if(x < w) break;
  }
  
  /* bottom margin */
  for(y = h-1, *bm = 0; y >= 0; y--, (*bm)++) {
    for(x = 0; x < w; x++) 
      if(data[y*w+x]) break;
    if(x < w) break;
  }
}

/* Subroutine:	void modify_direct_mark()
   Function:  	modify direct mark to improve prediction. Paul Howard's method
   		is used here, pixels are flipped if they satisfy the following
		two conditions:
		1. they differ from their two immediate causal neighbors
		2. this difference is isolated in the same sense as above
   Input:	mark to be modified
   Output: 	if this modification is "good" in the sense that all qualifying
   		positions are modified
*/
void modify_direct_mark(Mark *mark, int *good)
{
  register int x, y;
  char *mptr, *pptr;
  char *poor;
  int mw, mh;
  int posi;
  char candidate[50000];
  
  mw = mark->width; mh = mark->height;

  poor = (char *)malloc(sizeof(char)*mw*mh);
  if(!poor) error("modify_direct_mark: cannot allocate memory\n");
  
  mptr = mark->data; pptr = poor;
  /* examine each pixel, assume white(zero) boundaries */
  /* first row */
  pptr[0] = (mptr[0] != 0);	/* first pixel */
  for(x = 1; x < mw; x++) 	/* other pixels */
    pptr[x] = ((mptr[x] != mptr[x-1]) && (mptr[x] != 0));
  pptr += mw;
  mptr += mw;
  
  /* other rows */
  for(y = 1; y < mh; y++) {
    pptr[0] = ((mptr[0] != 0) && (mptr[x] != mptr[x-mw])); /* first pixel */
    for(x = 1; x < mw; x++) 	/* other pixels */
	pptr[x] = ((mptr[x] != mptr[x-1]) && (mptr[x] != mptr[x-mw]));
    pptr += mw;
    mptr += mw;
  }
  
  /* obtain the flip candidates */
  if(mw*mh > 50000) 
    error("modify_direct_mark: candidate buffer is too short\n");
  get_flip_candidates(poor, mw, mh, candidate, good);
  
  /* modify the original symbol at candidate positions, along the way check
  if all the differing positions are claimed as candidates */
  mptr = mark->data;
  for(y = 0; y < mh; y++) {
    for(x = 0; x < mw; x++) {
    	posi = y*mw+x;
	if(candidate[posi]) mptr[posi] = !mptr[posi];
    }
  }
  
  free((void *)poor);
}

int is_isolated(char *, int, int, PixelCoord, int *);
void wipe_out_mark(char *, int, int, PixelCoord, Window);
extern void copy_data_with_margin(char *, int, int, int, int, int, int, char *);

/* Subroutine:	void get_flip_candidate()
   Function:	examine the input difference bitmap and identify those black
   		pixels (where the current and reference marks differ) that are
		isolated single pixels or groups of 2 pixels, mark them as flip
		candidates for future flipping
   Input:	the difference bitmap, its size
   Output:	flip candidate bitmap, and if the candidates are "good" 
   		in the sense that all different positions have been marked 
		for flip
*/
void get_flip_candidates(char *diff, int width, int height, 
  char *cand, int *good)
{
  register int x, y;
  char *ediff, *ptr;
  int ew, eh;
  char *cptr;
  PixelCoord seed;
  int iso_type;
  
  for(x = 0; x < width*height; x++) cand[x] = 0;

  ediff = (char *)malloc(sizeof(char)*(width+2)*(height+2));
  if(!ediff) 
    error("get_flip_candidates: unable to allocate memory\n");

  copy_data_with_margin(diff, width, height, 1, 1, 1, 1, ediff);
  ew = width + 2; eh = height + 2;
  ediff += ew + 1;
  
  ptr = ediff; cptr = cand; *good = TRUE;
  for(y = 0; y < height; y++) {
    for(x = 0; x < width; x++) {
      if(ptr[x]) {
        seed.x = x; seed.y = y;
	if(is_isolated(ediff, width, height, seed, &iso_type)) {
	  switch(iso_type) {
	    case SINGLE:
	      cand[seed.x+seed.y*width] = 1;
	      break;
	    case HDOUBLE:
	      cand[seed.x+seed.y*width] = 1;
	      cand[(seed.x+1)+seed.y*width] = 1;
	      break;
	    case VDOUBLE:
	      cand[seed.x+seed.y*width] = 1;
	      cand[seed.x+(seed.y+1)*width] = 1;
	      break;
	    default:
	      break;
	  }
	}
	else *good = FALSE;
      }
    }
    ptr += ew; cptr += width;
  }
  
  ediff -= ew + 1;
  free((void *)ediff);
}

int is_isolated(char *diff, int w, int h, PixelCoord seed, int *type)
{
  Window window;
  int width, height;
  int result;
  
  boundary_tracing_4(diff, w, h, seed);
  window = get_bounding_window();
  
  width = window.lr.x-window.ul.x+1;
  height = window.lr.y-window.ul.y+1;
  
  if(width == 1 && height == 1) {
    *type = SINGLE;
    result = TRUE;
  }
  else if(width == 1 && height == 2) {
    *type = VDOUBLE;
    result = TRUE;
  }
  else if(width == 2 && height == 1) {
    *type = HDOUBLE;
    result = TRUE;
  }
  else result = FALSE;

  /* reset the pixels enclosed the boundary */
  wipe_out_mark(diff, w, h, seed, window);

  return result;
}

int isolated_or_speck(char *diff, int w, int h, PixelCoord seed)
{
  Window window;
  int width, height;
  int result;
  
  boundary_tracing_4(diff, w, h, seed);
  window = get_bounding_window();
  
  width = window.lr.x-window.ul.x+1;
  height = window.lr.y-window.ul.y+1;
  
  if(width <= 2 && height <= 2) result = TRUE;
  else result = FALSE;

  /* reset the pixels enclosed the boundary */
  wipe_out_mark(diff, w, h, seed, window);

  return result;
}

void  wipe_out_mark(char *ediff, int w, int h, PixelCoord seed, Window win)
{
  int l, x1, x2, dy;
  int x, y;
  int ew;
  
  ew = w+2;
  x = seed.x; y = seed.y;	/* relative position in the strip buffer */
  PUSH(y, x, x, 1);		/* needed in some cases */
  PUSH(y+1, x, x, -1);		/* seed segment (popped 1st) */

  while (sp>stack) {
	/* pop segment off stack and fill a neighboring scan line */
	POP(y, x1, x2, dy);
	/*
	 * segment of scan line y-dy for x1<=x<=x2 was previously filled,
	 * now explore adjacent pixels in scan line y
	 */
	for (x=x1; x>=win.ul.x && (ediff[x+ew*y]); x--) {
	    /* reset the same pixel in doc_buffer 	*/
	    ediff[x+ew*y] = 0;
	}
	if (x>=x1) goto skip;	
	l = x+1;
	if (l<x1) PUSH(y, l, x1-1, -dy);		/* leak on left? */
	x = x1+1;
	do {
	    for (; x<=win.lr.x && (ediff[x+ew*y]); x++) {
	    	/* reset the corresponding positions in 	*
	     	 * the original image 	*/
	    	ediff[x+ew*y] = 0;
	    }
	    PUSH(y, l, x-1, dy);
	    if (x>x2+1) PUSH(y, x2+1, x-1, -dy);	/* leak on right? */
skip:	    for (x++; x<=x2 && !(ediff[x+ew*y]); x++);
	    l = x;
	} while (x<=x2);
  }
}


/* Subroutine:	void get_mark_center()
   Function:	calculate the input mark's geometric center
   Input:	the mark
   Output:	none
*/
void get_mark_center(Mark *mark)
{
  /* if the width/height is odd, geometric center is the true center *
   * if the width is even, geometric center is to the left side      *
   * if the height is even, geometric center is to the lower side    */
  if(mark->width % 2) mark->c.x = mark->width/2; 
  else mark->c.x = mark->width/2-1;
  mark->c.y = mark->height/2;
}

/* Subroutine:	void get_mark_centroid()
   Function:	calculate the input mark's centroid position
   Input:	the mark
   Output:	none
*/
void get_mark_centroid(Mark *mark)
{
  register int x, y;
  int tot_x, tot_y;
  int count;
  int w, h;
  char *ptr;
  
  w = mark->width; h = mark->height;
  tot_x = 0; tot_y = 0; count = 0;
  ptr = mark->data;
  for(y = 0; y < h; y++) 
    for(x = 0; x < w; x++) 
      if(*ptr++) {
        tot_x += x; tot_y +=y;
	count++;
      }

  mark->c.x = (float)tot_x/(float)count;
  mark->c.y = (float)tot_y/(float)count;
}

PixelCoord get_seed(char *, int, int);
void seed_fill(char *, int, int, PixelCoord);

/* Subroutine:	void get_mark_hole_num()
   Function:	calculate the input mark's feature: number of holes 
   Input:	mark
   Output:	none
*/
void get_mark_hole_num(Mark *mark)
{
  int w, h, done;
  int counter;
  char *filled;
  PixelCoord seed;
  
  w = mark->width; h = mark->height;
  
  if(w > h*5 || h > w*5)
  {
    mark->hole_num = 0;
    return;
  }

  filled = (char *)malloc(sizeof(char)*(w+2)*(h+2));
  if(!filled) error("get_mark_hole_num: Cannot allocate memory\n");
  copy_data_with_margin(mark->data, w, h, 1, 1, 1, 1, filled);
  
  counter = -1;
  do {
    seed = get_seed(filled, w+2, h+2);
    if(seed.x != -1 && seed.y != -1) {
      done = FALSE;
      seed_fill(filled, w+2, h+2, seed);
      counter++;
    }
    else done = TRUE;
  } while(!done);
  
  mark->hole_num = counter;
}

/* Subroutine:	PixelCoord get_seed()
   Function:	find a seed 
   Input:	map of filled pixels and its size
   Output:	seed position
*/
PixelCoord get_seed(char *filled, int w, int h)
{
  register int i;
  PixelCoord seed;
  
  for(i = 0; i < w*h; i++) {
    if(!filled[i]) {
      seed.x = i%w; seed.y = i/w;
      return seed;
    }
  }
  
  seed.x = seed.y = -1;
  return seed;
}

int already_filled(PixelCoord, char *, int);
int out_of_bound(PixelCoord, int, int);

/* Subroutine:	void seed_fill()
   Function:	execute simple-fill algorithm from the given seed
   Input:	filled map, its size and the seed's position
   Output:	none
*/
void seed_fill(char *filled, int w, int h, PixelCoord seed)
{
  register int i;
  PixelCoord neighbors[4];
  
  /* set the seed to be black */
  filled[seed.x+seed.y*w] = TRUE;
  
  /* calculate its 4 neighbors */
  neighbors[0].x = seed.x; neighbors[0].y = seed.y-1;
  neighbors[1].x = seed.x; neighbors[1].y = seed.y+1;
  neighbors[2].x = seed.x-1; neighbors[2].y = seed.y;
  neighbors[3].x = seed.x+1; neighbors[3].y = seed.y;
  
  /* flood to its 4 neighbors */
  for(i = 0; i < 4; i++) {
    if(!out_of_bound(neighbors[i], w, h) &&
       !already_filled(neighbors[i], filled, w))
      seed_fill(filled, w, h, neighbors[i]);
  }
}

int already_filled(PixelCoord posi, char *filled, int w)
{
  return filled[posi.x+posi.y*w];
}

int out_of_bound(PixelCoord posi, int w, int h)
{
  if(posi.x < 0 || posi.x >= w || posi.y < 0 || posi.y >= h)
    return TRUE;
  else return FALSE;
}

int pixels_differ(char *, char *, int, int, int);

/* Subroutine:	int trace_error_cluster()
   Function:	from a seed, trace the error cluster that it connects to. 
   		On the way compare the cluster's size to the input threshold,
		if the size exceeds the threshold, quit right away. Otherwise
		when the whole cluster is traced, return its size to the 
		call routine.
   Input:	two bitmaps, their size, the "processed" flags
   Output:	decision if the cluster is too big. If not, its actual size
*/
int trace_error_cluster(char *bmp1, char *bmp2, int w, int h, PixelCoord seed, 
	int thres, int *size)
{
  int l, x1, x2, dy;
  int x, y;
  int too_big;
  Window win;
  
  *size = 0; too_big = FALSE;
  
  win.ul.x = 0; win.ul.y = 0;
  win.lr.x = w-1; win.lr.y = h-1;
  
  x = seed.x; y = seed.y;	/* relative position in the strip buffer */
  PUSH(y, x, x, 1);		/* needed in some cases */
  PUSH(y+1, x, x, -1);		/* seed segment (popped 1st) */

  while (sp>stack && !too_big) {
	/* pop segment off stack and fill a neighboring scan line */
	POP(y, x1, x2, dy);
	/*
	 * segment of scan line y-dy for x1<=x<=x2 was previously filled,
	 * now explore adjacent pixels in scan line y
	 */
	for (x=x1; x>=win.ul.x && (pixels_differ(bmp1, bmp2, w, x, y)) && !too_big; x--) {
	    (*size)++;
	    if(*size > thres) too_big = TRUE;
	    /* set the pixels to have the same value */
	    bmp1[x+y*(w+2)] = bmp2[x+y*(w+2)];
	}
	if (x>=x1) goto skip;	
	l = x+1;
	if (l<x1) PUSH(y, l, x1-1, -dy);		/* leak on left? */
	x = x1+1;
	do {
	    for (; x<=win.lr.x && (pixels_differ(bmp1, bmp2, w, x, y)); x++) {
	        (*size)++;
	        if(*size > thres) too_big = TRUE;
	        /* set the pixels to have the same value */
	        bmp1[x+y*(w+2)] = bmp2[x+y*(w+2)];
	    }
	    PUSH(y, l, x-1, dy);
	    if (x>x2+1) PUSH(y, x2+1, x-1, -dy);	/* leak on right? */
skip:	    for (x++; x<=x2 && !(pixels_differ(bmp1, bmp2, w, x, y)); x++);
	    l = x;
	} while (x<=x2 && !too_big);
  }
  
  sp = stack;
  return too_big;
}

int pixels_differ(char *bmp1, char *bmp2, int w, int x, int y)
{
  return bmp1[y*(w+2)+x] != bmp2[y*(w+2)+x];
}
