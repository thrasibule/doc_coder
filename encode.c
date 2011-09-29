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
#include "encode.h"
#include "entropy.h"
#include "mq.h"

void copy_buffer(PixelMap *, PixelMap *);
void free_buffer_memory(PixelMap *);
void process_buffer(void);
void extract_marks_from_buffer(void);
void put_dots_back(void);
void put_words_together(void);
void free_marks_not_needed(void);

void init_cleanup_image(void);
void reset_cleanup_image(void);

void encode_page_info(void);
void encode_end_of_page(void);
void encode_end_of_file(void);
void encode_end_of_stripe(int);

extern void extract_mark(PixelCoord);
extern void write_mark_on_template(Mark *, char *, int, int, PixelCoord);
extern int  is_speck(Mark *);
extern void error(char *);

extern void write_segment_header(SegHeader *);
extern void output_byte(int);
extern void output_short(int);
extern void output_int(int);

extern Codec *codec;
extern PixelMap *doc_buffer;
extern MarkList *all_marks;	/* list of all the extracted marks */
extern WordList *all_words;	/* list of words */
extern Mark *all_word_marks;    /* list of all words' bitmaps */
extern PixelMap *cleanup;
extern int symbol_ID_in_bits;

/* Subroutine:	void copy_buffer()
   Function:	copy the source buffer (PixelMap format) into the destination
   Input: 	source and dest buffer pointer
   Output:	none
*/
void copy_buffer(PixelMap *src, PixelMap *dest)
{
  char *sptr, *dptr;
  int ew, eh;
  
  if(src->width != dest->width || src->height != dest->height)
    error("copy_buffer: Cannot operate on buffers with different sizes\n");
    
  ew = src->width + 2; eh = src->height+2;
  sptr = src->data-ew-1; dptr = dest->data-ew-1;
  memcpy(dptr, sptr, ew*eh);
}

/* Subroutine:  void free_doc_buffer_memory()
   Function:	free the memory allocated to doc_buffer
   Input:	the buffer pointer
   Output:	none
*/
void free_buffer_memory(PixelMap *buffer)
{
  buffer->data -= (buffer->width+2+1);
  free((void *)buffer->data);
}

/* Subroutine:	void process_buffer()
   Function:	do three things:
   		1. extract marks from the buffer
		2. put the dots in "i" and "j" back together with the stems
		3. put marks together to form words   
   Input:	none;
   Output:	none;
*/
void process_buffer()
{
  extract_marks_from_buffer();
  put_dots_back();
//  put_words_together();
}

/* Subroutine: 	void extract_marks_from_buffer()
   Function:	process the current doc_buffer(usually the whole image)
   		strip by strip  
   Input:	none;
   Output:	none;
*/
void extract_marks_from_buffer()
{
  register int x, scan_line;
  char *data_ptr;
  PixelCoord seed;
  int width, height;
  
  codec->report.speck_marks = 0;
  width = doc_buffer->width; height = doc_buffer->height;
  data_ptr = doc_buffer->data;
  for(scan_line = 0; scan_line < height; scan_line++) {
    for(x = 0; x < width; x++)
      if(data_ptr[x]) {	/* found a seed */
	seed.x = x; seed.y = scan_line;
	extract_mark(seed);
      }
    data_ptr += width+2;
  }
}

static int typ_w, typ_h;

void get_typical_mark_size(int *, int *);
int  is_a_dot(Mark *);
void find_stem(Mark *, int *);
int  is_empty(int, int *, int);
void combine_dot_and_stem(Mark *, Mark *);
void delete_empty_marks(int *, int);

/* Subroutine:	void put_dots_back()
   Function:	scan the list of all marks extracted and put dots on top of 
   		"i" and "j" back together with their stems.
   Input:	none
   Output:	none
*/
void put_dots_back()
{
  register int i;
  Mark *cur_mark, *stem_mark;
  int stem;
  int empty_marks[MAX_MARK_NUM], empty_mark_num;
  
  /* get typical mark size */
  get_typical_mark_size(&typ_w, &typ_h);

  /* for those mark that are likely to be dots, i.e., have size significant
     smaller than the typical size, see if they're part of "i" or "j" by trying
     to find their stems */    
  cur_mark = all_marks->marks; empty_mark_num = 0;
  for(i = 0; i < all_marks->mark_num; i++, cur_mark++) {
    if(is_a_dot(cur_mark)) {
      find_stem(cur_mark, &stem);
      if(stem != -1) { /* found a stem for the dot */
        if(is_empty(stem, empty_marks, empty_mark_num)) 
	  printf("WARNING: two dots try to share one stem, ignored!\n"); 
	else {
  	  stem_mark = all_marks->marks + stem;
	  combine_dot_and_stem(cur_mark, stem_mark);
          empty_marks[empty_mark_num++] = stem;
        }
      }
    } 
  }

  delete_empty_marks(empty_marks, empty_mark_num);
}

/* Subroutine:	void get_typical_mark_size()
   Function:	scan through the entire mark list and find the most typical
   		mark size 
   Input:	none
   Output:	typical width and height
*/   
void get_typical_mark_size(int *tw, int *th)
{
  register int i;
  int mw, mh;  /* biggest width and height */
  int *w_count, *h_count;
  Mark *cur_mark;
  int max;
  
  /* find biggest width and height */
  mw = mh = 0; cur_mark = all_marks->marks;
  for(i = 0; i < all_marks->mark_num; i++, cur_mark++) {
    if(cur_mark->width  > mw) mw = cur_mark->width;
    if(cur_mark->height > mh) mh = cur_mark->height;
  }
  
  w_count = (int *)malloc(sizeof(int)*(mw+1));
  h_count = (int *)malloc(sizeof(int)*(mh+1));
  if(!w_count || !h_count)
    error("get_typical_mark_size: cannot allocate memory\n");
  for(i = 0; i <= mw; i++) w_count[i] = 0;
  for(i = 0; i <= mh; i++) h_count[i] = 0;
  
  /* get size histograms */
  cur_mark = all_marks->marks;
  for(i = 0; i < all_marks->mark_num; i++, cur_mark++) {
    w_count[cur_mark->width]++;
    h_count[cur_mark->height]++;
  }
  
  /* find typical width */
  for(i = 0, max = 0; i <= mw; i++) 
    if(w_count[i] > max) {
      max = w_count[i]; *tw = i;
    }
  
  /* find typical height */
  for(i = 0, max = 0; i <= mh; i++) 
    if(h_count[i] > max) {
      max = h_count[i]; *th = i;
    }

  free((void *)w_count);
  free((void *)h_count);
}

/* Subroutine:	int is_a_dot()
   Function:	decide if the input mark is likely to be a dot by deciding if
   		its size is much smaller compared with the typical mark size 
   Input:	current mark
   Output:	binary decision
*/
int is_a_dot(Mark *mark)
{
  /* if the mark is not speckal noise but is small enough, it's a dot */
  if(!is_speck(mark) && 
     (mark->width <= (typ_w/3)) && (mark->height <= (typ_h/3))) 
    return TRUE;
  else return FALSE;
}

/* Subroutine:	void find_stem()
   Function:    scan through the mark list and see if can find a stem for the
   		input dot mark
   Input:	dot mark
   Output:	stem mark index if any
*/
void find_stem(Mark *dot, int *stem)
{
  register int i;
  int start;
  Mark *cur;
  int scan_line;
  
  /* we need only examine marks coming after the current dot because stems
     can only be extracted after dots */
  start = (dot - all_marks->marks) + 1;
  cur = dot + 1;
  scan_line = dot->upleft.y+typ_h;
  for(i = start; i < all_marks->mark_num; i++, cur++) 
    /* a valid stem is a mark that 
       1. vertically lies below the dot and within a certain distance;
       2. horizontally encloses the dot;
       3. not a dot itself. */
    if(cur->upleft.y < scan_line) {
      if(dot->upleft.x > cur->upleft.x && 
         dot->upleft.x < cur->upleft.x + cur->width &&
	 !is_a_dot(cur)) {
      	*stem = i; return;
      }
    }
  
  *stem = -1;
}

/* Subroutine:	int is_empty()
   Function:	decide if the input mark is already in the empty_marks buffer 
   Input:	current mark, empty_marks buffer and size
   Output:	binary decision
*/
int is_empty(int mark, int *empty_marks, int empty_mark_num)
{
  register int i;
  
  for(i = 0; i < empty_mark_num; i++)
    if(mark == empty_marks[i]) return TRUE;
  
  return FALSE;
}

/* Subroutine:	void combine_dot_and_stem()
   Function:	combine the bitmap of the dot mark and the stem mark into one
   Input:	dot and stem
   Output:	none
*/
void combine_dot_and_stem(Mark *dot, Mark *stem)
{
  int nw, nh;
  PixelCoord ul, br, posi;
  char *data;
  extern void get_mark_center(Mark *);
  extern void get_mark_centroid(Mark *);
  
  if(dot->upleft.x < stem->upleft.x) 
    ul.x = dot->upleft.x;
  else ul.x = stem->upleft.x;
  ul.y = dot->upleft.y;
  
  if(dot->ref.x+dot->width > stem->ref.x+stem->width) 
    br.x = dot->ref.x+dot->width;
  else br.x = stem->ref.x+stem->width;
  br.y = stem->ref.y+1;
  
  nw = br.x-ul.x; nh = br.y-ul.y;
  data = (char *)malloc(nw*nh*sizeof(char));
  if(!data) error("combine_dot_and_stem: cannot allocate memory\n");
  memset((void *)data, 0, nw*nh);
  
  posi.x = dot->upleft.x-ul.x; posi.y = dot->upleft.y-ul.y;
  write_mark_on_template(dot, data, nw, nh, posi);
  posi.x = stem->upleft.x-ul.x; posi.y = stem->upleft.y-ul.y;
  write_mark_on_template(stem, data, nw, nh, posi);
  
  free((void *)dot->data);
  dot->width = nw; dot->height = nh;
  dot->upleft = ul;
  dot->ref.x = ul.x; dot->ref.y = ul.y+nh-1;
  dot->data = data;
  if(codec->align == CENTER) get_mark_center(dot); 
  else get_mark_centroid(dot);
}

void sort_empty_marks(int *, int);

/* Subroutine:	void delete_empty_marks()
   Function:	delete the empty marks in the all_marks buffer. After the dots
   		are put back with the stems, the stem marks are empty
   Input:	buffer of empty mark indices and its size
   Output:	none
*/
void delete_empty_marks(int *empty_marks, int empty_mark_num)
{
  register int i, j;
  
  if(!empty_mark_num) return;
  
  sort_empty_marks(empty_marks, empty_mark_num);
  
  for(i = 0; i < empty_mark_num; i++) 
    free((void *)all_marks->marks[empty_marks[i]].data);

  for(i = 0; i < empty_mark_num-1; i++) {
    for(j = empty_marks[i]+1; j < empty_marks[i+1]; j++) 
      all_marks->marks[j-i-1] = all_marks->marks[j];
  }
  
  for(j = empty_marks[empty_mark_num-1]+1; j < all_marks->mark_num; j++) 
    all_marks->marks[j-empty_mark_num] = all_marks->marks[j];
    
  all_marks->mark_num -= empty_mark_num;
}

/* Subroutine:	void sort_empty_marks()
   Function:	sort the values in empty_marks buffer
   Input:	empty_marks buffer and its size
   Output:	none
*/
void sort_empty_marks(int *marks, int mark_num)
{
  register int i, j;
  int temp;

  for(i = 0; i < mark_num-1; i++)
    for(j = mark_num-1; j >= i+1; j--)
      if(marks[j] < marks[j-1]) {
	temp = marks[j];
	marks[j] = marks[j-1];
	marks[j-1] = temp;
     }  
}

TextLine text_lines[MAX_TEXT_LINE_NUM];
int text_line_num;

void get_text_lines(void);
void get_horiz_offset_prob(int *);
void get_letter_space(int *, int *);
void add_new_word(int *, int);

/* Subroutine:	void put_words_together();
   Function:	examine the marks extracted and try to integrate their bitmaps
   		into words
   Input:	none
   Output:	none
*/
void put_words_together()
{
  register int i, j;
  int *horiz_offset_prob;
  int letter_space;
  TextLine *cur_line;
  int letter_count;
  int letters[LONGEST_WORD];
  Mark *cur_mark;
  int offset;
  
  /* first, organize the marks into text lines */
  get_text_lines();
  
  /* find the common letter space upper limit, this way all bitmaps that are
     spaced below this limit are considered to belong to one word */
  horiz_offset_prob = (int *)malloc(sizeof(int)*doc_buffer->width);
  if(!horiz_offset_prob)
    error("put_words_together: cannot allocate memory");
   
  get_horiz_offset_prob(horiz_offset_prob);
  get_letter_space(horiz_offset_prob, &letter_space);
  
  free((void *)horiz_offset_prob);

  /* now integrate marks into words */
  for(i = 0, cur_line = text_lines; i < text_line_num; i++, cur_line++) {
    letters[0] = cur_line->marks[0];
    letter_count = 1;
    for(j = 1; j < cur_line->mark_num; j++) {
      cur_mark = all_marks->marks + cur_line->marks[j-1];
      offset = all_marks->marks[cur_line->marks[j]].ref.x-
      	       cur_mark->ref.x-cur_mark->width;
      if(offset <= letter_space) {
        if(letter_count == LONGEST_WORD)
	  error("put_words_together: word buffer is too short\n");
        letters[letter_count++] = cur_line->marks[j];
      }
      else {
        add_new_word(letters, letter_count);
        letters[0] = cur_line->marks[j];
	letter_count = 1;
      }
    }
    if(letter_count) add_new_word(letters, letter_count);
  }
}

void add_text_line(int, int *);

/* Subroutine:  void get_text_lines()
   Function:	organize the extracted marks into text lines, within each line,
   		make sure marks are sorted horizontally
   Input:	none
   Output:	none
*/
void get_text_lines()
{
  register int i, j;
  int scan_line;
  int processed[MAX_MARK_NUM];
  Mark *cur_mark;
  int count;
  int marks_on_line[MAX_MARK_NUM];
  
  memset(processed, 0, all_marks->mark_num*sizeof(int));
  
  text_line_num = 0;
  for(i = 0; i < all_marks->mark_num; i++) {
    if(!processed[i]) {
      cur_mark = all_marks->marks + i;
      /* scan line cuts across the current mark's bitmap */
      scan_line = (cur_mark->upleft.y + (cur_mark->ref.y << 1))/3;
      add_text_line(scan_line, processed);
    }
  }
  
  for(i = 0, count = 0; i < text_line_num; i++) {
    for(j = 0; j < text_lines[i].mark_num; j++) 
      marks_on_line[count++] = text_lines[i].marks[j];
  }
}

/* Subroutine:	void get_horiz_offset_prob()
   Function:	get the histogram of the horizontal offset between consecutive
   		marks in the page
   Input:	none
   Output:	probability table
*/
void get_horiz_offset_prob(int *prob)
{
  register int i, j;
  int *marks, mark_num;
  int offset;
  Mark *cur_mark;
  TextLine *cur_line;
  
  for(i = 0; i < doc_buffer->width; i++) prob[i] = 0;
  
  cur_line = text_lines;
  for(i = 0; i < text_line_num; i++, cur_line++) {
    mark_num = cur_line->mark_num;
    marks = cur_line->marks;
    for(j = 0; j < mark_num-1; j++) {
      cur_mark = all_marks->marks + marks[j];
      offset = 	
        all_marks->marks[marks[j+1]].ref.x-cur_mark->ref.x-cur_mark->width;
      if(offset >= 0) prob[offset]++;
    }
  }
}

/* Subroutine:	void get_letter_space()
   Function:	from the input histogram of horizontal offsets, guess the 
   		upper limit of space between letters (as opposed to words)
   Input:	offset histogram
   Output:	letter space
*/
void get_letter_space(int *prob, int *space)
{
  register int i;
  int max_prob, letter_space;
  int thres;
  
  letter_space = 0; max_prob = prob[0];
  for(i = 1; i < doc_buffer->width; i++) 
    if(prob[i] > max_prob) {
      max_prob = prob[i];
      letter_space = i;
    }
  
  thres = max_prob >> 6; /* one 64-th of the maximum probabity */ 
  for(i = letter_space+1; i < doc_buffer->width; i++) 
    if(prob[i] < thres) break;
  
  *space = --i;
}

void get_marks_on_scan_line(int, int *, int *, int *);
void sort_marks_on_scan_line(int *, int);

/* Subroutine:	void add_text_line()
   Function:	find all the marks on the input scan line and add them into
   		a new text_line 
   Input:	text line y coord, buffer of marks and its size
   Output:	none
*/
void add_text_line(int scan_line, int *processed)
{
  register int j;
  TextLine *cur;
  int *marks;
  int mark_num;
    
  if(text_line_num == MAX_TEXT_LINE_NUM) 
    error("add_text_line: text_lines buffer is full\n");
  
  cur = text_lines + text_line_num;
  marks = text_lines[text_line_num].marks;
  get_marks_on_scan_line(scan_line, processed, marks, &mark_num);
  sort_marks_on_scan_line(marks, mark_num);
  cur->y = scan_line; cur->mark_num = mark_num;
  text_line_num++;
      
  for(j = 0; j < mark_num; j++) processed[marks[j]] = TRUE;    
}

/* Subroutine:	void get_marks_on_scan_line()
   Function:	return all the marks on the current scan line, meaning those
   		being cut across by the current scan line
   Input:	the scan line and the buffer indicating if the marks are
   		processed already
   Output:	buffer and number of all the marks on the scan line
*/
void get_marks_on_scan_line(int scan_line, int *processed, 
	int *marks, int *mark_num)
{
  register int i;
  Mark *cur;
  
  *mark_num = 0;
  for(i = 0, cur = all_marks->marks; i < all_marks->mark_num; i++, cur++)
    if(!processed[i]) 
      if(cur->upleft.y <= scan_line && cur->ref.y >= scan_line) {
        if(*mark_num == LONGEST_TEXT_LINE)
	  error("get_marks_on_scan_line: text line contains too many marks\n");
        marks[(*mark_num)++] = i;
      }
}

/* Subroutine:	void sort_marks_on_scan_line()
   Function:	sort the input marks by their horizontal position
   Input:	mark buffer and its size
   Output:	none
*/
void sort_marks_on_scan_line(int *marks, int mark_num)
{
  register int i, j;
  int temp;

  for(i = 0; i < mark_num-1; i++)
    for(j = mark_num-1; j >= i+1; j--)
      if(all_marks->marks[marks[j]].ref.x <
         all_marks->marks[marks[j-1]].ref.x ) {
          temp = marks[j];
          marks[j] = marks[j-1];
          marks[j-1] = temp;
      }
}

/* Subroutine:	void add_new_word()
   Function:	add into the word list a new word defined by the input letters
   Input:	letters in word
   Output:	none
*/
void add_new_word(int *letters, int letter_num)
{
  register int i;
  Mark *cur_mark;
  PixelCoord ul, br, posi;
  int ww, wh;
  Mark *word;
  Word *cur_word;
  
  if(all_words->word_num == MAX_WORD_NUM)
    error("add_new_word: word list buffer is full\n");
  
  /* if there's only one letter in the word, then this word is just
     a mark */
  if(letter_num == 1) {
    cur_mark = all_marks->marks + letters[0];
    cur_word = all_words->words + all_words->word_num;
    cur_word->letters[0] = letters[0];
    cur_word->letter_num = 1;
    cur_word->bitmap = cur_mark;
  }
  
  /* if there's multiple letters in the word, then write these multiple
     marks into the new word */
  else {
    cur_mark = all_marks->marks + letters[0];
    ul = cur_mark->upleft;
    for(i = 1; i < letter_num; i++) {
      cur_mark = all_marks->marks + letters[i];
      if(cur_mark->upleft.y < ul.y)
        ul.y = cur_mark->upleft.y;
    }
  
    cur_mark = all_marks->marks + letters[letter_num-1]; 
    br = cur_mark->ref;
    br.x += cur_mark->width; br.y++;
    for(i = letter_num-2; i >= 0; i--) {
      cur_mark = all_marks->marks + letters[i];  
      if(cur_mark->ref.x + cur_mark->width > br.x)
        br.x = cur_mark->ref.x + cur_mark->width;
      if(cur_mark->ref.y+1 > br.y)
        br.y = cur_mark->ref.y+1;
    }
  
    ww = br.x-ul.x; wh = br.y-ul.y;
//    word = (Mark *)malloc(sizeof(Mark));
//    if(!word) error("add_new_word: Cannot allocate memory\n");
    word = all_word_marks + all_words->word_num;
    word->upleft = ul; 
    word->ref.x = ul.x; word->ref.y = ul.y+wh-1;
    word->width = ww; word->height = wh;
    word->data = (char *)malloc(ww*wh*sizeof(char));
    if(!word->data) error("add_new_word: Cannot allocate memory\n");
    memset(word->data, 0, ww*wh);
    
    for(i = 0; i < letter_num; i++) {
      cur_mark = all_marks->marks + letters[i];
      posi.x = cur_mark->upleft.x-ul.x;
      posi.y = cur_mark->upleft.y-ul.y;
      write_mark_on_template(cur_mark, word->data, ww, wh, posi);
    }
  
    cur_word = all_words->words + all_words->word_num;
    cur_word->letter_num = letter_num;
    for(i = 0; i < letter_num; i++) cur_word->letters[i] = letters[i];
    cur_word->bitmap = word;
  }   
  
  all_words->word_num++;
}

/* Subroutine:  void init_cleanup_image()
   Function:	set up memory for the cleanup image
   Input:	none
   Output:	none
*/
void init_cleanup_image()
{
  cleanup->width = doc_buffer->width; 
  cleanup->height = doc_buffer->height;
  cleanup->data = (char *)malloc(sizeof(char)*cleanup->width*cleanup->height);
  if(!cleanup->data)
    error("init_cleanup_image: cannot allocate memory\n");

  reset_cleanup_image();
}

/* Subroutine:  void reset_cleanup_image()
   Function:	reset the cleanup image memory to 0
   Input:	none
   Output:	none
*/
void reset_cleanup_image()
{
  memset(cleanup->data, 0, cleanup->width*cleanup->height);
}

/* Subroutine:	void free_marks_not_needed()
   Function:	free the bitmap memory allocated to those non-dictionary marks
   Input:	none
   Output:	none
*/
void free_marks_not_needed()
{
  register int i;
  Mark *mark;
  
  for(i = 0, mark = all_marks->marks; i < all_marks->mark_num; i++, mark++) 
    if(!mark->in_dict && mark->data) {
      free((void *)mark->data);
      mark->data = NULL;
    }
}

/* Subroutine:	void encode_page_info()
   Function:	encode page information segment 
   Input:	none
   Output:	none
*/
void encode_page_info()
{
  SegHeader header;
  unsigned char flags;
  int refined;
  int def_pixel;
  int def_combop;
  int aux_buffer;
  int overriden;
  unsigned short strip_info;
    
  /* page information segment header */
  header.type = PAGE_INFO;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.seg_length = 19;
  header.page_asso = codec->cur_page+1;
  write_segment_header(&header);

  /* page size */
  output_int(codec->doc_width);
  output_int(codec->doc_height);
  
  /* page resolution: unknown */
  output_int(0);
  output_int(0);
  
  /* page segment flags */
  if(codec->lossy && codec->pms) flags = 0;
  else flags = 1;		/* page is eventually lossless */
  refined = FALSE;		/* page does not contain refinement */
  flags |= refined << 1;
  def_pixel = 0;		/* default pixel is black */
  flags |= def_pixel << 2;
  def_combop = JB2_OR;		/* default combination operator is OR */		  flags |= def_combop << 3;
  aux_buffer = FALSE;		/* page does not require auxiliary buffer */
  flags |= aux_buffer << 5;
  overriden = FALSE;		/* def_combop cannot be overriden */ 
  flags |= overriden << 6;
  output_byte(flags);

  /* page striping information */
  if(codec->split_num > 1) {
    /* reserve some redundancy in max_strip_height */
    strip_info = (unsigned short)(codec->doc_height/(codec->split_num));
    strip_info += codec->doc_height >> 3; /* 1/8 of the document height */
    strip_info |= 0x8000;
  }
  else strip_info = 0;
  output_short(strip_info);
}

/* Subroutine:	encode_end_of_page()
   Function:	encode end of page segment 
   Input:	none
   Output:	none
*/
void encode_end_of_page()
{
  SegHeader header;
  
  header.type = END_OF_PAGE;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.page_asso = codec->cur_page+1;
  header.seg_length = 0;
  write_segment_header(&header);
}

/* Subroutine:	encode_end_of_file()
   Function:	encode end of file segment 
   Input:	none
   Output:	none
*/
void encode_end_of_file()
{
  SegHeader header;
  
  header.type = END_OF_FILE;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.page_asso = 0;
  header.seg_length = 0;
  write_segment_header(&header);
}

/* Subroutine:	encode_end_of_stripe()
   Function:	encode end of stripe segment 
   Input:	the end row value
   Output:	none
*/
void encode_end_of_stripe(int end_row)
{
  SegHeader header;
  
  header.type = END_OF_STRIPE;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.page_asso = codec->cur_page+1;
  header.seg_length = 4;
  write_segment_header(&header);  
  
  output_int(end_row);
}
