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
#include "entropy.h"
#include "mq.h"
#include <math.h>

void init_coding_strips(void);
void make_coding_strips(void);
void encode_coding_strips(void);
void free_coding_strips(void);

void decide_marks_in_dict(void);
void substitute_mark(Mark *, Mark *);
int  is_speck(Mark *);
void write_into_cleanup(Mark *);
void find_marks_in_strip(int, int, int *, int *, int *);
int  mark_in_strip(Mark *, int, int);
void horiz_sort_marks(int *, int);

CodingStrip	*coding_strips;
int 		cur_coding_strip;
int 		max_coding_strip_num;

extern void modify_refine_mark(Mark *, Mark *, int *, int *, int *);
extern void match_mark_with_dict(Mark *, int *, float *);
extern void write_mark_on_template(Mark *, char *, int, int, PixelCoord);
extern int  write_coded_bitstream(char *, int);
extern void buffer_coded_bitstream(ARITH_CODED_BITSTREAM *, 
	CODED_SEGMENT_DATA *);
extern void error(char *);

extern void bin_encode_refine(char *, int, int, char *, int, int, int, int, 
	ARITH_CODED_BITSTREAM *);

extern void int_encode(int, int, ARITH_CODED_BITSTREAM *);
extern void symID_encode(int, ARITH_CODED_BITSTREAM *);

extern void write_segment_header(SegHeader *);
extern void write_sym_reg_seg_header(SymRegionDataHeader *);

extern Codec *codec;
extern MarkList *all_marks;
extern WordList *all_words;
extern Mark *all_word_marks;
extern PixelMap *doc_buffer;
extern int symbol_ID_in_bits;
extern Dictionary *dictionary;
extern PixelMap *cleanup;
extern ARENC_STATE *snum[NUM_INTARITH_CODERS];
extern ARENC_STATE *sbt_refine;

/* Subroutine:	void init_coding_strips()
   Function:	get memory to hold coding strips, estimate # of strips
   Input:	none
   Output:	none
*/
void init_coding_strips(void)
{
  max_coding_strip_num = codec->doc_height/codec->strip_height;
  
  coding_strips = (CodingStrip *)
  	malloc(sizeof(CodingStrip)*max_coding_strip_num); 
  if(!coding_strips) error("init_coding_strips: Cannot allocate memory\n");

  cur_coding_strip = 0;
}

/* Subroutine:	void free_coding_strips()
   Function:	free the memory allocated to the coding strips
   Input:	none
   Output:	none
*/
void free_coding_strips()
{
  register int i;
  CodingStrip *strip;
  
  strip = coding_strips;
  for(i = 0; i < cur_coding_strip; i++, strip++) 
    free((void *)strip->marks);

  free((void *)coding_strips);
}

/* Subroutine:  make_coding_strips()
   Function:    set up coding strips, put all the extracted symbols (in
                all_marks) into coding strips. In the lossy mode, things are
		more complicated because changing the symbol bitmaps may
		change their location, therefore shape-unifying has to be
		done before the coding strips are formed
   Input:       none
   Output:      none
*/
void make_coding_strips()
{
  register int i;
  register int top, bottom;
  CodingStrip *strip_ptr;
  int mark_index[MAX_MARK_NUM], mark_num;
  int processed[MAX_MARK_NUM];
  int ref_index;
  float mm;
  Mark *cur;
  MarkRegInfo *mark_reg;
  int perfect;
  int rdx[MAX_MARK_NUM], rdy[MAX_MARK_NUM];
  int perfect_sym_num;

  decide_marks_in_dict();
  
  perfect_sym_num = 0;
  for(i = 0, cur = all_marks->marks; i < all_marks->mark_num; i++, cur++) {
    if(!cur->in_dict) {
      match_mark_with_dict(cur, &ref_index, &mm);
      if(codec->pms) {
       if(ref_index != -1) {
         cur->in_dict = TRUE; cur->dict_entry = ref_index;
	 substitute_mark(cur, dictionary->entries[ref_index].mark);
       }
      }
      else if(codec->lossy) { /* lossy SPM compression */
       if(mm > 0. && ref_index != -1) {
	modify_refine_mark(dictionary->entries[ref_index].mark, 
			   cur, &perfect, &rdx[i], &rdy[i]);
	perfect_sym_num += perfect;
	if(perfect) cur->in_dict = TRUE;
	else cur->in_dict = FALSE; 
        cur->dict_entry = ref_index;
       }
       else if(ref_index == -1) {
        cur->in_dict = FALSE; cur->dict_entry = -1;
       }
       else { /* if mm == 0.) */
        cur->in_dict = TRUE; cur->dict_entry = ref_index;
       }
      }
      else { /* lossless SPM compression */
       rdx[i] = rdy[i] = 0;
       if(mm > 0.) cur->in_dict = FALSE; 
       else cur->in_dict = TRUE;
       cur->dict_entry = ref_index; 
      }
    }
  }
  
  if(codec->lossy && !codec->pms)
    printf("generated %d perfect symbols\n", perfect_sym_num);
  
  memset(processed, FALSE, sizeof(int)*all_marks->mark_num);

  #ifdef NEVER /* specks are not extracted from the page */ 
  /* first of all, put all specks back into the cleanup image */
  codec->report.speck_marks = 0;
  for(i = 0; i < all_marks->mark_num; i++)
    if(is_speck(all_marks->marks + i)) {
      write_into_cleanup(all_marks->marks+i);
      processed[i] = TRUE;
      codec->report.speck_marks++;
    }
  #endif 
  
  init_coding_strips();
  strip_ptr = coding_strips; 
  codec->report.marks_in_strips = 0;
  codec->report.marks_in_cleanup = 0;
  codec->report.embedded_marks = 0;

  for(top = 0; top < doc_buffer->height; top += codec->strip_height) {
    if(top+codec->strip_height < doc_buffer->height)
      bottom = top+codec->strip_height;
    else bottom = codec->doc_height;

    find_marks_in_strip(top, bottom, processed, mark_index, &mark_num);

    /* if there're marks lying in the current strip, add them */
    if(mark_num) {
      horiz_sort_marks(mark_index, mark_num);

      strip_ptr->marks = (MarkRegInfo *)malloc(sizeof(MarkRegInfo)*mark_num);
      if(!strip_ptr->marks)
        error("make_coding_strips: Cannot allocate memory for new marks\n");

      strip_ptr->num_marks = 0; mark_reg = strip_ptr->marks;
      for(i = 0; i < mark_num; i++) {
        cur = all_marks->marks+mark_index[i];
	if(cur->in_dict) {
	  codec->report.marks_in_strips++;
	  mark_reg->dict_index = dictionary->entries[cur->dict_entry].index;
	  mark_reg->list_entry = mark_index[i];
	  mark_reg->embedded.flag = FALSE;
	  mark_reg->embedded.mark = NULL;
	  mark_reg->embedded.ref_mark = NULL;
	  
	  mark_reg++;
	  strip_ptr->num_marks++;
	}
	else {
	  /* if this is a direct singleton, write it back to cleanup */
	  if(cur->dict_entry == -1) {
	    write_into_cleanup(cur);
	    codec->report.marks_in_cleanup++;
	  }
	  else {
            codec->report.marks_in_strips++;
	    mark_reg->dict_index = dictionary->entries[cur->dict_entry].index;
            mark_reg->list_entry = mark_index[i];  
	    codec->report.embedded_marks++;
            mark_reg->embedded.flag = TRUE;
            mark_reg->embedded.mark = cur;
            mark_reg->embedded.ref_mark = 
	      dictionary->entries[cur->dict_entry].mark;
      	    mark_reg->embedded.rdx = rdx[mark_index[i]];
	    mark_reg->embedded.rdy = rdy[mark_index[i]];
	  
	    mark_reg++;
	    strip_ptr->num_marks++;
	  }
        }
      }
    
      if(strip_ptr->num_marks) {
        strip_ptr->strip_x_posi =
	  all_marks->marks[strip_ptr->marks[0].list_entry].ref.x;
        strip_ptr->strip_top_y = top;
        cur_coding_strip++; strip_ptr++;
      } 
      else free((void *)strip_ptr->marks);
    }
  }
}

/* Subroutine:	void substitute_mark()
   Function:	in PM&S mode, substitute the dictionary mark for the
   		current one, change the current mark's position if 
		necessary (due to size difference)
   Input:	the current and the reference marks
   Output:	none
*/
void substitute_mark(Mark *cur, Mark *ref)
{
  int dx, dy;
  
  dx = ref->c.x-cur->c.x; dy = ref->c.y-cur->c.y;
  
  cur->upleft.x -= dx; cur->upleft.y -= dy;
  cur->width = ref->width; cur->height = ref->height;
  cur->ref.x = cur->upleft.x; cur->ref.y = cur->upleft.y+cur->height-1;
  free((void *)cur->data);
  cur->data = ref->data;
}
 
/* Subroutine:	void decide_marks_in_dict()
   Function:	decide which marks are in the dictionary 
   Input:	none
   Output:	none
*/
void decide_marks_in_dict()
{
  register int i;
  Mark *mark;
  
  for(i = 0, mark = all_marks->marks; i < all_marks->mark_num; i++, mark++)
    mark->in_dict = FALSE;
    
  for(i = 0; i < dictionary->total_mark_num; i++) {
    mark = dictionary->entries[i].mark;
    mark->in_dict = TRUE;
    mark->dict_entry = i;
  }
}

/* Subroutine:	int is_speck()
   Function:	decide if the input mark is speck noise
   Input:	mark to be tested
   Output:	binary decision
*/
int is_speck(Mark *mark)
{
  if((mark->width <= 2) && (mark->height <= 2)) return TRUE;
  else return FALSE;
}

/* Subroutine:	void write_into_cleanup()
   Function:	write the input mark into the cleanup image
   Input:	mark to be written
   Output:	none
*/
void write_into_cleanup(Mark *mark)
{
  write_mark_on_template(mark, cleanup->data, cleanup->width, cleanup->height,
      mark->upleft);
}

/* Subroutine:  void find_marks_in_strip()
   Function:    search all the marks not yet added into the coding strips and
                see if any lie in the current coding strip
   Input:       coding strip top and bottom scan lines, the buffer "processed"
   Output:      the buffer and size of the marks in the current strip
*/
void find_marks_in_strip(int top, int bottom, int *processed,
        int *mark_index, int *mark_num)
{
  register int i;
 
  *mark_num = 0;
  for(i = 0; i < all_marks->mark_num; i++) {
    if(!processed[i] && mark_in_strip(all_marks->marks+i, top, bottom)) {
      mark_index[(*mark_num)++] = i;
      processed[i] = TRUE;
    }
  }
}

/* Subroutine:  mark_in_strip()
   Function:    decide if the input mark's reference corner lies inside the
                current strip
   Input:       current mark and the strip's top and bottom line
   Output:      the binary output
*/
int mark_in_strip(Mark *mark, int top, int bottom)
{
  if(mark->ref.y < bottom && mark->ref.y >= top) return TRUE;
  else return FALSE;
}

/* Subroutine:	void horiz_sort_marks()
   Function:	horizontally sort the marks in buffer
   Input:	mark buffer and its size
   Output:	none
*/
void horiz_sort_marks(int *sorted_index, int mark_num)
{
  register int i, j;
  int temp;

  /* Sort the marks horizontally according to their leftmost pixel 	*/
  for(i = 0; i < mark_num-1; i++)
    for(j = mark_num-1; j >= i+1; j--)
	  if(all_marks->marks[sorted_index[j]].ref.x < 
	     all_marks->marks[sorted_index[j-1]].ref.x ) {
		temp = sorted_index[j];
		sorted_index[j] = sorted_index[j-1];
		sorted_index[j-1] = temp;
	  }
}

/* Subroutine:	void encode_coding_strips()
   Function:	encode all the coding strips, first encode the typical 
   		horizontal offset for this page, then for each strip
		1. encode its relative strip_top_y and x_posi;
		2. for each symbol in the strip, encode its dictionary index,
		   its y_posi_in_strip and relative x_offset, and its refinement
		   bitmap if any
   Input:	none
   Output:	none
*/
void encode_coding_strips()
{
  register int i, j;
  int strip_height_in_bits;
  double a;
  int ave_horiz_offset;
  int *horiz_offset_prob;

  int sbrefine;
  int horiz_offset, vert_posi;
  int strip_x_off, strip_y_off;
  CodingStrip *strip;
  MarkRegInfo *mark_region;
  Mark *cur_mark, *pre_mark, *ref_mark;
  int dw, dh, dx, dy;
  PixelCoord rc, cc;
  
  ARITH_CODED_BITSTREAM bitstr;
  CODED_SEGMENT_DATA coded_data;
    
  SegHeader header;
  SymRegionDataHeader data_header;

  int ori_file;	/* file size before this segment is sent */
  
  extern void reset_arith_int_coders(void);
  extern void reset_arith_bitmap_coders(void);
  extern void arith_encode_init(void);
  extern void arith_encode_flush(ARITH_CODED_BITSTREAM *);
  extern void write_sym_reg_seg_header(SymRegionDataHeader *);
    
  switch(codec->strip_height) {
    case 1:
      strip_height_in_bits = 0;
      break;
    case 2:
      strip_height_in_bits = 1;
      break;
    case 4:
      strip_height_in_bits = 2;
      break;
    case 8:
      strip_height_in_bits = 3;
      break;
    default:
      error("encode_coding_strips: illegal coding strip height value\n");
      break;
  }

  a = ceil(log((double)dictionary->total_mark_num)/log(2.));
  symbol_ID_in_bits = (int)a;

  ori_file = ftell(codec->fp_out);
  
  /* find common horizontal offset DSOFFSET for this segment */
  horiz_offset_prob = (int *)malloc(sizeof(int)*codec->doc_width);
  if(!horiz_offset_prob) 
    error("encode_coding_strips: Cannot allocate memory\n");
  memset(horiz_offset_prob, 0, sizeof(int)*codec->doc_width);
  for(i = 0, strip = coding_strips; i < cur_coding_strip; i++, strip++) {
    for(j = 1, mark_region = strip->marks+1; 
        j < coding_strips[i].num_marks; j++, mark_region++) {
	cur_mark = all_marks->marks + mark_region->list_entry;
	pre_mark = all_marks->marks + mark_region[-1].list_entry;
	horiz_offset = cur_mark->ref.x-(pre_mark->ref.x+pre_mark->width-1);
	/* intuitively positive horiz_offset values dominate, count them only */
	if(horiz_offset >= 0) horiz_offset_prob[horiz_offset]++;
    }
  }
  
  ave_horiz_offset = 0;
  for(i = 1; i < codec->doc_width; i++) {
    if(horiz_offset_prob[i] > horiz_offset_prob[ave_horiz_offset])
	ave_horiz_offset = i;
  }
  free((void *)horiz_offset_prob);

  /* make sure DSOFFSET lies inside the interval [-16, 15] */
  if(ave_horiz_offset > 15) ave_horiz_offset = 15;
  else if(ave_horiz_offset < -16) ave_horiz_offset = -16;

  if(codec->report.embedded_marks) sbrefine = TRUE;
  else sbrefine = FALSE;

  /* encode symbol region data */
  bitstr.max_buffer_size = 1024;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data)
    error("encode_coding_strips: Cannot allocate memory\n");
  coded_data.size = 0;
  reset_arith_int_coders();
  reset_arith_bitmap_coders();
  arith_encode_init();
  
  /* code the initial STRIPT */
  int_encode(0, IADT, &bitstr);
  buffer_coded_bitstream(&bitstr, &coded_data);
  codec->report.location_bits += bitstr.coded_size;  

  for(i = 0, strip = coding_strips; i < cur_coding_strip; i++, strip++) {
    /* encode segment location information */
    if(i == 0) { 
    	strip_y_off = strip->strip_top_y;
    	strip_x_off = strip->strip_x_posi;
    }
    else {
     	strip_y_off = strip->strip_top_y-strip[-1].strip_top_y;
    	strip_x_off = strip->strip_x_posi-strip[-1].strip_x_posi;
    }
    strip_y_off >>= strip_height_in_bits; /* strip_y_off /= codec->strip_height */

    int_encode(strip_y_off, IADT, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.location_bits += bitstr.coded_size;
    
    /* encode symbol region segment data */
    for(j = 0, mark_region = strip->marks; 
    	j < coding_strips[i].num_marks; j++, mark_region++) {
	cur_mark = all_marks->marks + mark_region->list_entry;
	if(j == 0) {
	  int_encode(strip_x_off, IAFS, &bitstr);
	  buffer_coded_bitstream(&bitstr, &coded_data);
	  codec->report.location_bits += bitstr.coded_size;
	}
	else {
	  pre_mark = all_marks->marks + mark_region[-1].list_entry;
	  horiz_offset = cur_mark->upleft.x-
	  		 (pre_mark->upleft.x+pre_mark->width-1)-
			 ave_horiz_offset;	
      	  int_encode(horiz_offset, IADS, &bitstr);
    	  buffer_coded_bitstream(&bitstr, &coded_data);
    	  codec->report.location_bits += bitstr.coded_size;
	}
	
	/* only code vert_posi when strip_height > 1 */
	if(strip_height_in_bits) {
	  vert_posi = cur_mark->ref.y-strip->strip_top_y;
	  int_encode(vert_posi, IAIT, &bitstr);
    	  buffer_coded_bitstream(&bitstr, &coded_data);
    	  codec->report.location_bits += bitstr.coded_size;
	}
	
	/* encode dictionary index */
    	symID_encode(mark_region->dict_index, &bitstr);
    	buffer_coded_bitstream(&bitstr, &coded_data);
    	codec->report.index_bits += bitstr.coded_size;
	
	/* 1 bit to indicate if embedded coding follows */
        if(sbrefine) {
  	  int_encode(mark_region->embedded.flag, IARI, &bitstr);
	  buffer_coded_bitstream(&bitstr, &coded_data);
	  codec->report.misc_bits += bitstr.coded_size;

	  if(mark_region->embedded.flag) {/* embedded coded bitmap follows */	
	    ref_mark = mark_region->embedded.ref_mark;
	  
	    dw = cur_mark->width - ref_mark->width;
	    dh = cur_mark->height- ref_mark->height;
	  
	    int_encode(dw, IARDW, &bitstr);
	    buffer_coded_bitstream(&bitstr, &coded_data);
	    codec->report.size_bits += bitstr.coded_size;
	  
	    int_encode(dh, IARDH, &bitstr);
	    buffer_coded_bitstream(&bitstr, &coded_data);
	    codec->report.size_bits += bitstr.coded_size;
    
     	    rc.x = ref_mark->c.x; rc.y = ref_mark->c.y;
            cc.x = cur_mark->c.x + mark_region->embedded.rdx;
	    cc.y = cur_mark->c.y + mark_region->embedded.rdy;
	  
	    dx = cc.x-rc.x; dx -= dw/2;
	    dy = cc.y-rc.y; dy -= dh/2;
	    int_encode(dx, IARDX, &bitstr);
	    buffer_coded_bitstream(&bitstr, &coded_data);
  	    codec->report.refine_offset_bits += bitstr.coded_size;
         
	    int_encode(dy, IARDY, &bitstr);
	    buffer_coded_bitstream(&bitstr, &coded_data);
	    codec->report.refine_offset_bits += bitstr.coded_size;   

	    bin_encode_refine(ref_mark->data, 
	    		      ref_mark->width, ref_mark->height, 
			      cur_mark->data, 
			      cur_mark->width, cur_mark->height, 
			      cc.x-rc.x, cc.y-rc.y, &bitstr);
	    buffer_coded_bitstream(&bitstr, &coded_data);
	    codec->report.refine_bits += bitstr.coded_size;

	    /* bitmaps of non-dictionary marks are no longer needed */
	    /* free((void *)cur_mark->data); */
	    codec->report.uncoded_refine_bits += bitstr.uncoded_size;
          } 
        }
    }

    int_encode(OOB, IADS, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.location_bits += bitstr.coded_size;
  }
  
  bitstr.coded_size = 0;
  arith_encode_flush(&bitstr);
  buffer_coded_bitstream(&bitstr, &coded_data);
  
  free((void *)bitstr.data);
  
  /* symbol region segment header. We assume that the order of segments
     for each page is fixed: page info, direct dict, refinement dict, 
     symbol region, cleanup page, residue page (if applicable), end of page */
  header.type = IM_SYM_REG;
  header.retain_this = FALSE;
  header.ref_seg_count = 1;
  header.ref_seg[0] = codec->cur_seg-1;
  if(codec->cur_page == codec->page_num-1) {
    /* if last page, then the dict referred-to need not to be retained */
    header.retain_ref[0] = FALSE;
  }
  else header.retain_ref[0] = TRUE;
  header.page_asso = codec->cur_page+1;
  
  header.seg_length = 17 +	/* region segment information field */
  		      2 +	/* symbol region segment flags */
		      4;	/* SBNUMINSTANCES */
  header.seg_length += coded_data.size;
  write_segment_header(&header);
  
  /* symbol region segment data header */
  data_header.reg_info.width = doc_buffer->width;
  data_header.reg_info.height = doc_buffer->height;
  data_header.reg_info.locx = 0;
  data_header.reg_info.locy = doc_buffer->top_y;
  data_header.reg_info.excombop = JB2_OR;
  
  data_header.huff = FALSE;
  data_header.refine = sbrefine;
  data_header.logsbstrips = strip_height_in_bits;
  data_header.refcorner = BOTTOMLEFT;
  data_header.transposed = FALSE;
  data_header.combop = JB2_OR;
  data_header.def_pixel = 0;
  data_header.dsoffset = ave_horiz_offset;
  data_header.rtemplate = 1;
  data_header.numinstances = codec->report.marks_in_strips;

  write_sym_reg_seg_header(&data_header);

  /* write coded segment data part */
  write_coded_bitstream(coded_data.data, coded_data.size << 3);

  codec->report.text_region_size = ftell(codec->fp_out) - ori_file;
}

