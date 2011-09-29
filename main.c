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
#include "entropy.h"
#include "mq.h"

Codec *codec;
PixelMap *doc_buffer;
PixelMap *ori_buffer;
Page pages[MAX_PAGE_NUM];
MarkList *all_marks;
WordList *all_words;
Mark *all_word_marks;
PixelMap *cleanup;

ARENC_STATE	*mq_coder;

extern void parse_command_line(int, char **);
extern void read_pbm_file_header(void);
extern void read_in_doc_buffer(void);
extern int  doc_buffer_blank(void);
extern void free_buffer_memory(PixelMap *);
extern void process_buffer(void);
extern void init_cleanup_image(void);
extern void reset_cleanup_image(void);
extern void init_dictionary(void);
extern void free_dictionary(void);
extern void make_mark_dictionary(void);
extern void update_mark_dictionary(void);
extern void opt_mark_dictionary_mixed(void);
extern void opt_mark_dictionary_tree(void);
extern void opt_mark_dictionary_class(void);
extern void make_mark_dictionary_pms(void);
extern void make_coding_strips(void);
extern void encode_dictionary(void);
extern void encode_coding_strips(void);
extern void encode_cleanup_image(void);
extern void free_coding_strips(void);
extern void free_marks_not_needed(void);

extern void save_doc_buffer(void);
extern void reconstruct_image(void);
extern void refine_encode_original_image(void);

extern void create_arith_coders(void);
extern void destroy_arith_coders(void);
extern void write_JBIG2_header(void);
extern void print_compression_param(void);
extern void print_compression_report(int, int);
extern void print_overall_compression_report(void);
extern void print_detailed_compression_report(int, int);
extern void reset_codec_report(void);
extern void error(char *);

extern void write_dictionary(void);
extern void write_all_marks(void);
extern void write_regions(void);
extern void write_cleanup_image(void);

extern void encode_page_info(void);
extern void encode_end_of_page(void);
extern void encode_end_of_file(void);
extern void encode_end_of_stripe(int);

extern void edge_smoothing();

int main(int argc, char **argv)
{  
  register int i, j;
  char fn[1000], first[500], second[500], count[100];
  int new_dict;
  
  parse_command_line(argc, argv);

  print_compression_param();
  
  /* if multi-page, cut the file name into parts */
  if(codec->page_num > 1) {
    strcpy(fn, codec->report.file_header);
    for(i = 0; i < strlen(fn); i++) 
      if(fn[i] >= '0' && fn[i] <= '9') break;
    strncpy(first, fn, i);
    first[i] = '\0';
    for(j = i; j < strlen(fn) && fn[j] >= '0' && fn[j] <= '9'; j++);
    strncpy(count, fn+i, j-i);
    count[j-i] = '\0';
    strcpy(second, fn+j);
  }
  /* if single-page, just copy the file name */
  else strcpy(first, codec->report.file_header);
  
  /* input image buffer */
  doc_buffer = (PixelMap *)malloc(sizeof(PixelMap));
  if(!doc_buffer)
    error("main: Cannot allocate memory\n");
  doc_buffer->top_y = 0;

  /* cleanup image buffer */
  cleanup = (PixelMap *)malloc(sizeof(PixelMap));
  if(!cleanup)
    error("main: Cannot allocate memory\n");
  
  /* one extra buffer is needed if residue_coding is turned ON */
  if(codec->residue_coding) {
    ori_buffer = (PixelMap *)malloc(sizeof(PixelMap));
    if(!ori_buffer)
      error("main: Cannot allocate memory\n");
  }
    
  /* compressed file */
  sprintf(fn, "%s%s.jb2", codec->report.data_path, first);
  codec->fp_out = fopen(fn, "wb");
  if(!codec->fp_out) error("cannot open binary file for writing\n");
  
  /* coding structures */
  create_arith_coders();
  init_dictionary();
  
  write_JBIG2_header();
  codec->cur_seg = 0;
  
  new_dict = TRUE; codec->report.overall_bits = 0;
  for(i = 0; i < codec->page_num; i++) {
    codec->cur_page = i;
    
    /* read in and process the current page */
    if(codec->page_num > 1) 
      sprintf(fn, "%s%s%d%s.pbm", codec->report.data_path, 
    	first, atoi(count)+i, second);
    else sprintf(fn, "%s%s.pbm", codec->report.data_path, first);
      
    codec->fp_in = fopen(fn, "rb");
    if(!codec->fp_in) error("Cannot open document file for reading\n");

    read_pbm_file_header();
    encode_page_info();
    
    doc_buffer->top_y = 0;
    for(j = 0; j < codec->split_num; j++) {
      codec->cur_split = j;
      
      read_in_doc_buffer();
      if(codec->residue_coding) save_doc_buffer();
      if(codec->lossy && !codec->pms) edge_smoothing();
            
      all_marks = (MarkList *)malloc(sizeof(MarkList));
      if(!all_marks)
        error("main: Cannot allocate memory for all_marks\n");
      all_marks->mark_num = 0;
      pages[i*codec->split_num+j].all_marks = all_marks;

      init_cleanup_image();
      process_buffer();
      free_buffer_memory(doc_buffer);
   
      reset_codec_report();
      codec->report.total_marks = all_marks->mark_num;
      
      if(!codec->no_update) {
        switch(codec->dict_type) {
	        case SE:
	        case OP:
	          make_mark_dictionary();
                  break;
	        case MIXED:
	          opt_mark_dictionary_mixed();
                  break;
	        case CLASS:
	          opt_mark_dictionary_class();
	          break;
	        case TREE:
	          opt_mark_dictionary_tree();
	          break;
	        case PMS_DICT:
	          make_mark_dictionary_pms();
	          break;
	        default:
	          error("Unknown dictionary type, aborting...\n");
	          break;
	      }
        encode_dictionary();
        if(!new_dict) update_mark_dictionary();
        new_dict = FALSE;
      }
      else {
        if(new_dict) {
          switch(codec->dict_type) {
	  case SE:
	  case OP:
	    make_mark_dictionary();
            break;
	  case MIXED:
	    opt_mark_dictionary_mixed();
            break;
  	  case CLASS:
	    opt_mark_dictionary_class();
	    break;
	  case TREE:
	    opt_mark_dictionary_tree();
	    break;
	  case PMS_DICT:
	    make_mark_dictionary_pms();
	    break;
	  default:
	    error("Unknown dictionary type, aborting...\n");
	    break;
	  }
          encode_dictionary();
          new_dict = FALSE;
	}
      }

      make_coding_strips();
      encode_coding_strips();

      encode_cleanup_image();
      
      if(codec->residue_coding) {
        reconstruct_image();
	refine_encode_original_image();
        free((void *)ori_buffer->data);
      }
      
      free_coding_strips();
      free_marks_not_needed();
      free((void *)cleanup->data);
	
      if(j < codec->split_num-1) 
        encode_end_of_stripe(doc_buffer->height+doc_buffer->top_y-1);
      
      if(!codec->silent) {
        print_compression_report(i, j);
        if(codec->detail) print_detailed_compression_report(i, j);
      }
      
      doc_buffer->top_y += doc_buffer->height;
    }
    encode_end_of_page();
    
    /* close the input file */
    fclose(codec->fp_in);
  }
  
  encode_end_of_file();

  print_overall_compression_report();
  
  destroy_arith_coders();
  
  free((void *)doc_buffer);
  free((void *)cleanup);
  fclose(codec->fp_out);

  return 0;
}

