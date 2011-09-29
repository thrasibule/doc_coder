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
#include <math.h>

char JBIG2_ID_STRING[] = {0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a};

static int  out_buffer;
static int  bits_written;

void output_bits(int, int);
void write_a_bit(int);
void output_byte(int);
void output_short(int);
void output_int(int);
void start_writing(void), finish_writing(void);

void read_in_doc_buffer(void);
void init_doc_buffer(int);
void set_up_buffer_memory(PixelMap *);
void adjust_doc_buffer(void);
int  doc_buffer_blank(void);
void save_doc_buffer(void);

int write_raw_bitmap(char *, int);
int write_coded_bitstream(char *, int);

void write_mark_to_buffer(Mark *, PixelCoord);
void write_pixel_to_buffer(int, int, char);
char read_pixel_from_buffer(int, int);
void write_mark_on_template(Mark *, char *, int, int, PixelCoord);

void write_pgm_image(char *, int, int, char *);
void write_pbm_image(char *, int, int, char *);
void write_dictionary();
void write_all_marks();
void write_regions();
void write_cleanup_image(void);
void write_mark_image(Mark *);

void read_dictionary(void);
void read_all_marks(void);
void read_regions(void);
void read_pbm_file_header(void);
void buffer_coded_bitstream(ARITH_CODED_BITSTREAM *,
                            CODED_SEGMENT_DATA *);

void write_JBIG2_header(void);
void write_segment_header(SegHeader *);
void write_region_seg_info_field(RegionInfoField *);
void write_gen_reg_seg_header(GenRegionDataHeader *);
void write_gen_ref_reg_seg_header(GenRefRegionDataHeader *);
void write_sym_reg_seg_header(SymRegionDataHeader *);
void write_sym_dict_seg_header(SymDictDataHeader *);

extern Codec *codec;
extern PixelMap *doc_buffer;
extern PixelMap *ori_buffer;
extern PixelMap *cleanup;
extern Dictionary *dictionary;
extern MarkList *all_marks;
extern int cur_coding_strip;
extern CodingStrip *coding_strips;

extern void copy_data_with_margin(char *, int, int, int, int, int, int, char *);
extern void error(char *);

/* Subroutine:	int read_in_doc_buffer();
   Function: 	read the image from file to doc_buffer
   Input:	none
   Output:	none
*/
void read_in_doc_buffer()
{
  register int x, y;
  char *data_ptr;
  int bits_to_go;
  int buffer;
  int bytes_per_line;
  
  /* move the file pointer to the correct start of data */  
  bytes_per_line = codec->doc_width>>3;
  if(codec->doc_width % 8) bytes_per_line++;

  init_doc_buffer(codec->cur_split == codec->split_num-1);

  fseek(codec->fp_in, -bytes_per_line*codec->doc_height, SEEK_END);
  fseek(codec->fp_in, bytes_per_line*doc_buffer->top_y, SEEK_CUR);
  /* read in pixel values from document image	*/
  data_ptr = doc_buffer->data; bits_to_go = 0;
  for(y = 0; y < doc_buffer->height; y++) {
    for(x = 0; x < doc_buffer->width; x++) {
      if(!bits_to_go) {
        buffer = fgetc(codec->fp_in);
        bits_to_go = 8;
      }
      if(buffer & 0x80) data_ptr[x] = 1;
      buffer <<= 1; bits_to_go--;
    }
    bits_to_go = 0;
    data_ptr += doc_buffer->width+2;
  }
}

void set_doc_buffer_size(int);

/* Subroutine:	void init_doc_buffer()
   Function:	allocate memory for doc_buffer pointer and set up internal data
   		buffer for it
   Input:	if this is the last part of one page
   Output:	none
*/
void init_doc_buffer(int last)
{
  set_doc_buffer_size(last);
  set_up_buffer_memory(doc_buffer);
}

int  find_buffer_height(void);

/* Subroutine:	void set_doc_buffer_size()
   Function:	decide size of doc_buffer
   Input:	if this's the last stripe
   Output:	none
*/
void set_doc_buffer_size(last)
{
  doc_buffer->width = codec->doc_width;

  if(codec->split_num == 1)
    doc_buffer->height = codec->doc_height;
  else if(!last) doc_buffer->height = find_buffer_height();
  else doc_buffer->height = codec->doc_height-doc_buffer->top_y;
}

int black_pixels_in_line(char *, int);

/* Subroutine:	int find_buffer_height()
   Function:	search within a certain range of the nominal buffer height
   		for a white line. If a white line exists, return its position;
		otherwise, return the position of a line which has the least 
		amount of black. The returned height value is to be used
		as the true buffer height 
   Input:	none
   Output:	buffer height
*/
int find_buffer_height()
{
  register int i;
  char *line_buffer;
  int bytes_per_line;
  int range;
  int cur, min, posi;
  int nom_height;	/* nominal height */
  
  bytes_per_line = codec->doc_width>>3;
  if(codec->doc_width % 8) bytes_per_line++;
  
  nom_height = codec->doc_height/codec->split_num;
  range = nom_height>>4; /* 1/16(6.25%)*doc_buffer->height */

  line_buffer = (char *)malloc(sizeof(char)*bytes_per_line);
  if(!line_buffer)
    error("find_buffer_height: cannot allocate memory\n");
  
  /* search downward fist */
  fseek(codec->fp_in, -bytes_per_line*codec->doc_height, SEEK_END);
  fseek(codec->fp_in, bytes_per_line*doc_buffer->top_y, SEEK_CUR);  
  fseek(codec->fp_in, bytes_per_line*nom_height, SEEK_CUR);
  min = doc_buffer->width;
  for(i = 0; i <= range; i++) {
    fread(line_buffer, sizeof(char), bytes_per_line, codec->fp_in);
    cur = black_pixels_in_line(line_buffer, bytes_per_line);
    if(cur == 0) {
      min = cur; posi = i; break;
    }
    else if(cur < min) {
      min = cur; posi = i;
    }
  }
  
  /* if necessary, search upward now */
  if(min > 0) {
    fseek(codec->fp_in, -bytes_per_line*codec->doc_height, SEEK_END);
    fseek(codec->fp_in, bytes_per_line*doc_buffer->top_y, SEEK_CUR);    
    fseek(codec->fp_in, bytes_per_line*(nom_height-1), SEEK_CUR);
    for(i = -1; i >= -range; i--) {
      fseek(codec->fp_in, bytes_per_line*(-2), SEEK_CUR);
      fread(line_buffer, sizeof(char), bytes_per_line, codec->fp_in);
      cur = black_pixels_in_line(line_buffer, bytes_per_line);
      if(cur == 0) {
        min = cur; posi = i; break;
      }
      else if(cur < min) {
        min = cur; posi = i;
      }
    }
  }
  
  free((void *)line_buffer);
  return nom_height+posi;
}

int black_pixels_in_byte(char);

/* Subroutine:	int black_pixels_in_line()
   Function:	count number of black pixels in the current text line
   Input:	line pointer and bytes in line
   Output:	number of black pixels
*/
int black_pixels_in_line(char *line, int bytes_in_line)
{
  register int i;
  int count;
  
  for(count = 0, i = 0; i < bytes_in_line; i++) {
    if(line[i]) count += black_pixels_in_byte(line[i]);
  }
  
  return count;
}

/* Subroutine:	int black_pixels_in_byte()
   Function:	count number of black pixels in the input byte
   Input:	byte value
   Output:	number of black pixels
*/
int black_pixels_in_byte(char byte)
{
  register int i;
  int count;
  
  for(i = 0, count = 0; i < 8; i++) 
    if((byte >> i) & 0x01) count++;
  
  return count;
}

/* Subroutine:	void set_up_buffer_memory()
   Function:	set up memory for encoding buffer, which is in 8 bits/pixel 
		format, the format that can be handled more easily;
   Input:	buffer pointer 
   Output:	none;
*/
void set_up_buffer_memory(PixelMap *buffer)
{
  char *data_ptr;
  int ew, eh;
  
  ew = buffer->width+2; eh = buffer->height+2;
  
  /* need four more lines(two horizontal and two vertical) to store 	*
   * the image, because the encoding buffer needs border		*/
  data_ptr = (char *)malloc(sizeof(char)*ew*eh);
  if(!data_ptr) error("set_up_buffer: Cannot allocate memory\n");
  memset(data_ptr, 0, ew*eh*sizeof(char));
    
  /* point the buffer to correct starting position			*/
  buffer->data = data_ptr+(buffer->width+2)+1;
}

#ifdef NEVER
int is_white_line(char *, int);

/* Subroutine:	void adjust_doc_buffer()
   Function:	adjust the doc_buffer height just read. Search from bottom up
   		for a white line, when reached, use this line as the doc_buffer
		true height
   Input:	none
   Output:	none
*/
void adjust_doc_buffer()
{
  register char *line;
  register int y;
  
  /* find the first white line from the bottom up */
  line = doc_buffer->data+(doc_buffer->width+2)*(doc_buffer->height-1);
  for(y = doc_buffer->height-1; y >= 0; y--, line -= (doc_buffer->width+2)) 
    if(is_white_line(line, doc_buffer->width)) break;
  
  if(y != -1) {
    /* set the line below the last white white to allow for necessary margin */
    if(y+1 < doc_buffer->height) 
      memset(line+doc_buffer->width+2, 0, doc_buffer->width);
  
    /* adjust the doc_buffer height to that empty line */
    doc_buffer->height = y+1;
  }
  else
    printf("adjust_doc_buffer: cannot adjust buffer size because no white line is found\n");
/*    error("adjust_doc_buffer: no white line in buffer, don't know how to proceed\n"); */
}
#endif

/* Subroutine:	void save_doc_buffer()
   Function:	save doc_buffer content into ori_buffer
   Input:	none
   Output:	none
*/
void save_doc_buffer()
{
  register int i;
  int width, height;
  
  width = ori_buffer->width = doc_buffer->width; 
  height = ori_buffer->height = doc_buffer->height;
  ori_buffer->data = (char *)malloc(sizeof(char)*width*height);
  if(!ori_buffer->data) 
    error("save_doc_buffer: Cannot allocate memory\n");
  
  for(i = 0; i < height; i++) 
    memcpy(ori_buffer->data+i*width, doc_buffer->data+i*(width+2), 
	   width*sizeof(char));
}

/* Subroutine:	int is_white_line()
   Function:	decide if the input line of pixels are all white
   Input:	line pointer and its width
   Output:	binary decision 
*/
int is_white_line(char *line, int width)
{
  register int x;
  
  for(x = 0; x < width; x++)
    if(line[x]) return FALSE;
    
  return TRUE;
}

/* Subroutine:	int doc_buffer_blank()
   Function:	decide if doc_buffer is entire blank (white)
   Input:	none
   Output: 	binary decision
*/
int doc_buffer_blank()
{
  register int i, j;
  register char *ptr;
  
  ptr = doc_buffer->data;
  for(i = 0; i < doc_buffer->height; i++, ptr += (doc_buffer->width+2))
    for(j = 0; j < doc_buffer->width; j++)
      if(ptr[j]) return FALSE;
      
  return TRUE;
}

/* Subroutine: 	void write_raw_bitmap()
   Function:	output to the file the raw bitmap rather than arithmetic coded
   		bitmap when arithmetic coding doesn't compress
   Input:	bitmap in char format and its size
   Output:	bits used 
*/
int write_raw_bitmap(char *data, int size)
{
  register int i, bits_used;
  
  bits_used = 0;
  for(i = 0; i < size; i++) {
    if(data[i]) write_a_bit(1);
    else write_a_bit(0);
    bits_used++;
  }

  return bits_used;
}

/* Subroutine: 	void write_coded_bitstream()
   Function:	output to the file the arithmetic coded bitstream
   Input:	binary bitstream with coded data, and total # bits
   Output:	bits used 
*/
int write_coded_bitstream(char *data, int size)
{
  int last_byte, cleanup;
  
  last_byte = size >> 3; cleanup = size%8;
  
  fwrite(data, sizeof(char), last_byte, codec->fp_out);
  if(cleanup) 
    output_bits((int)(data[last_byte]>>(8-cleanup)), cleanup);

  return size;
}

/* Subroutine:	void start_writing()
   Function:	initialize variables before start outputting bits
   Input:	none
   Output:	none
*/
void start_writing()
{
  bits_written = 0;
  out_buffer = 0;
}

/* Subroutine:	void output_byte()
   Function:	write a byte
   Input:	byte value
   Output:	none
*/
void output_byte(int byte)
{
  fputc(byte, codec->fp_out);
}

/* Subroutine:	void output_byte()
   Function:	write two bytes
   Input:	short value
   Output:	none
*/
void output_short(int shrt)
{
  fputc(((shrt >> 8) & 0xff), codec->fp_out);
  fputc((shrt & 0xff), codec->fp_out);
}

/* Subroutine:	void output_byte()
   Function:	write 4 bytes
   Input:	integer value
   Output:	none
*/
void output_int(int inte)
{
  fputc(((inte >> 24) & 0xff), codec->fp_out);
  fputc(((inte >> 16) & 0xff), codec->fp_out);
  fputc(((inte >>  8) & 0xff), codec->fp_out);
  fputc((inte & 0xff), codec->fp_out);
}

/* Subroutine:	void output_bits()
   Function:	output the designated number of bits from the given value
   Input:	the value and the number of bits to be output
   Output:	none
*/
void output_bits(int value, int length)
{
  register int i;
  register int mask;
  
  value <<= (32-length);
  
  mask = 0x80000000;
  for(i = 0; i < length; i++) {
    if(value & mask) write_a_bit(1);
    else write_a_bit(0);
    value <<= 1;
  }
}

/* Subroutine: 	void write_a_bit();
   Function:	writes an output bit to output buffer, and if buffer is full,
   		write it to file
   Input:	the bit to be output
   Output:	none
*/
void write_a_bit(int bit)
{
  out_buffer <<= 1;
  if(bit) out_buffer |= 0x01;
  bits_written++;

  if(bits_written == 8) {
    fputc(out_buffer, codec->fp_out);
    out_buffer = 0;
    bits_written = 0;
  }
}

/* Subroutine:	void finish_writing()
   Function:	finish outputting by writing bits left in the out_buffer
   		that haven't been written yet
   Input:	none
   Output:	none
*/
void finish_writing()
{
  if(bits_written > 0) {
    out_buffer <<= (8-bits_written);
    fputc(out_buffer, codec->fp_out);
  }
}

/* Subroutine:	void write_cleanup_image()
   Function;	this function writes the current image content in 
   		doc_buffer into an output file
   Input:	none
   Output:	none
*/
void write_cleanup_image(void)
{
  char fn[200], suffix[100];

  printf("Input residual image file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);

  write_pbm_image(cleanup->data, cleanup->width, cleanup->height, fn);
}

/* Subroutine:	void read_dictionary()
   Function:	read dictionary from an external file(after dict. opt.)
   Input:	none
   Output:	none
*/
void read_dictionary()
{
  FILE *fp;
  register int i;
  char suffix[100], fn[100];
  DictionaryEntry *cur_entry;
  int list_entry, total_mark_num;
  float mismatch_thres;
  
  printf("Input dictionary file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
     
  fp = fopen(fn, "r");
  if(!fp) error("read_dictionary: cannot open dictionary file\n");
   
  fread(&mismatch_thres, sizeof(float), 1, fp);
  if(mismatch_thres != codec->mismatch_thres) 
    error("wrong external dictionary, mismatch thres is not correct\n");
  fread(&total_mark_num, sizeof(int), 1, fp);

  if(!codec->lossy && total_mark_num != dictionary->total_mark_num)
    error("wrong external dictionary, number of marks is not correct\n");
   
  dictionary->total_mark_num = total_mark_num;
  for(i = 0; i < total_mark_num; i++) {
    cur_entry = dictionary->entries + i;
    fread(&(cur_entry->index), sizeof(int), 1, fp);
    fread(&(cur_entry->ref_index), sizeof(int), 1, fp);
    fread(&(cur_entry->singleton), sizeof(int), 1, fp);
    if(codec->lossy) cur_entry->perfect = FALSE;
    
    fread(&list_entry, sizeof(int), 1, fp);
    cur_entry->mark = all_marks->marks + list_entry;
    if(cur_entry->ref_index != -1) {
      fread(&list_entry, sizeof(int), 1, fp);
      cur_entry->ref_mark = all_marks->marks + list_entry;
    }
  }
  fclose(fp);
}

/* Subroutine: 	void read_all_marks()
   Function:	read MarkList from an external file(after dict. opt.)
   Input:	void
   Output:	void
*/
void read_all_marks()
{
  register int i;
  FILE *fp;
  Mark *cur_mark;
  char suffix[100], fn[1000];
  int total_mark_num;
  PixelCoord ref;
  int width, height;
  
  printf("Input mark file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
 
  fp = fopen(fn, "r");  
  if(!fp) error("read_all_marks: cannot open mark file\n");
 
  fread(&total_mark_num, sizeof(int), 1, fp);

  if(!codec->lossy && total_mark_num != all_marks->mark_num)
    error("read_all_marks: wrong mark file, number of marks doesn't agree\n");
   
  all_marks->mark_num = total_mark_num;
  for(i = 0; i < total_mark_num; i++) {
    cur_mark = all_marks->marks + i;
    fread(&(ref.x), sizeof(int), 1, fp);
    fread(&(ref.y), sizeof(int), 1, fp);
    fread(&(width), sizeof(int), 1, fp);
    fread(&(height), sizeof(int), 1, fp);
    if(!codec->lossy) {
      if(ref.x != cur_mark->ref.x || ref.y != cur_mark->ref.y ||
         width != cur_mark->width || height != cur_mark->height)
         error("read_all_marks: wrong mark file\n");
    }
    else {
      if(width != cur_mark->width || height != cur_mark->height) {
        free((void *)cur_mark->data);
        cur_mark->data = (char *)malloc(sizeof(char)*width*height);
        if(!cur_mark->data)
          error("read_all_marks: cannot allocate memory\n");
        cur_mark->width = width; cur_mark->height = height;
      }
    }
    fread(cur_mark->data, sizeof(char), width*height, fp);
  }
  fclose(fp);
}

/* Subroutine:	void read_regions()
   Function:	read the coding strips from a file 
   Input:	none
   Output:	none
*/ 
void read_regions()
{
  register int i, j;
  FILE *fp;
  char fn[100], suffix[100];
  MarkRegInfo *cur_mark;
  CodingStrip *cur_strip;

  printf("Input region file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
  
  fp = fopen(fn, "r");
  if(!fp) error("read_regions: cannot open region file\n");
  
  fread(&(codec->strip_height), sizeof(int), 1, fp);
  fread(&(cur_coding_strip), sizeof(int), 1, fp);

  cur_strip = coding_strips;
  for(i = 0; i < cur_coding_strip; i++) {
    fread(&(cur_strip->strip_x_posi), sizeof(int), 1, fp);
    fread(&(cur_strip->strip_top_y), sizeof(int), 1, fp);
    fread(&(cur_strip->num_marks), sizeof(int), 1, fp);
    
    cur_mark = cur_strip->marks;
    for(j = 0; j < cur_strip->num_marks; j++) {
      fread(&(cur_mark->dict_index), sizeof(int), 1, fp);
      fread(&(cur_mark->list_entry), sizeof(int), 1, fp);
      cur_mark++;
    }
    
    cur_strip++;
  }
  
  fclose(fp);
}

/* Subroutine:	void write_dictionary()
   Function:	write the complete dictionary information(before splitting it)
   		for other programs' use
   Input:	none
   Output:	none
*/
void write_dictionary()
{
  register int i;
  FILE *fp;
  char fn[1000], suffix[100];
  DictionaryEntry *cur_entry;
  int list_entry;
  
  printf("Input dictionary file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
  
  fp = fopen(fn, "w");
  if(!fp) error("write_dictionary: cannot open dictionary file\n");
  
  fwrite(&(codec->mismatch_thres), sizeof(float), 1, fp);
  fwrite(&(dictionary->total_mark_num), sizeof(int), 1, fp);
  for(i = 0; i < dictionary->total_mark_num; i++) {
    cur_entry = dictionary->entries + i;
    fwrite(&(cur_entry->index), sizeof(int), 1, fp);
    fwrite(&(cur_entry->ref_index), sizeof(int), 1, fp);
    fwrite(&(cur_entry->singleton), sizeof(int), 1, fp);
    list_entry = cur_entry->mark-all_marks->marks;
    fwrite(&list_entry, sizeof(int), 1, fp);
    if(cur_entry->ref_index != -1) {
      list_entry = cur_entry->ref_mark-all_marks->marks;
      fwrite(&list_entry, sizeof(int), 1, fp);
    }
  }
  
  fclose(fp);
}

/* Subroutine:	void write_all_marks()
   Function:	write the complete information of extracted marks
   		for other programs' use
   Input:	none
   Output:	none
*/
void write_all_marks()
{
  register int i;
  FILE *fp;
  char fn[1000], suffix[100];
  Mark *cur_mark;
  
  printf("Input mark file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
  
  fp = fopen(fn, "w");
  if(!fp) error("write_all_marks: cannot open mark file\n");
  
  fwrite(&(all_marks->mark_num), sizeof(int), 1, fp);
  for(i = 0; i < all_marks->mark_num; i++) {
    cur_mark = all_marks->marks + i;
    fwrite(&(cur_mark->ref.x), sizeof(int), 1, fp);
    fwrite(&(cur_mark->ref.y), sizeof(int), 1, fp);
    fwrite(&(cur_mark->width), sizeof(int), 1, fp);
    fwrite(&(cur_mark->height), sizeof(int), 1, fp);
    fwrite(cur_mark->data, sizeof(char), cur_mark->width*cur_mark->height, fp);
  }
  
  fclose(fp);  
}

/* Subroutine:	void write_regions()
   Function:	write the coding strips into a file for later use
   Input:	none
   Output:	none
*/ 
void write_regions()
{
  register int i, j;
  FILE *fp;
  char fn[1000], suffix[100];
  MarkRegInfo *cur_mark;
  CodingStrip *cur_strip;
  
  printf("Input region file suffix:");
  scanf("%s", suffix);
  sprintf(fn, "%s%s/%s.%s", codec->report.data_path, 
    codec->report.file_header, codec->report.file_header, suffix);
  
  fp = fopen(fn, "w");
  if(!fp) error("write_regions: cannot open mark file\n");
  
  fwrite(&(codec->strip_height), sizeof(int), 1, fp);
  fwrite(&(cur_coding_strip), sizeof(int), 1, fp);

  cur_strip = coding_strips;
  for(i = 0; i < cur_coding_strip; i++) {
    fwrite(&(cur_strip->strip_x_posi), sizeof(int), 1, fp);
    fwrite(&(cur_strip->strip_top_y), sizeof(int), 1, fp);
    fwrite(&(cur_strip->num_marks), sizeof(int), 1, fp);
    
    cur_mark = cur_strip->marks;
    for(j = 0; j < cur_strip->num_marks; j++) {
      fwrite(&(cur_mark->dict_index), sizeof(int), 1, fp);
      fwrite(&(cur_mark->list_entry), sizeof(int), 1, fp);
      cur_mark++;
    }
    
    cur_strip++;
  }
  
  fclose(fp);    
}

/* Subroutine:	void write_pgm_image()
   Function:	write the image stored in "data" into file specified by "fn"
   		in PGM format
   Input:	image data, its size, and PGM filename
   Output:	none
*/
void write_pgm_image(char *data, int width, int height, char *fn)
{
  FILE *fp;
  char buffer[1000];
  
  fp = fopen(fn, "w");
  if(!fp) 
    error("write_pgm_image: cannot open pgm file\n");
  
  sprintf(buffer, "P5\n");
  fprintf(fp, buffer);
  sprintf(buffer, "%d %d\n", width, height);
  fprintf(fp, buffer);
  sprintf(buffer, "255\n");
  fprintf(fp, buffer);
  
  fwrite(data, sizeof(unsigned char), width*height, fp);
  
  fclose(fp);
}

/* Subroutine:	void write_pbm_image()
   Function:	write the image stored in "data" into file specified by "fn"
   		in PBM format
   Input:	image data, its size, and PBM filename
   Output:	none
*/
void write_pbm_image(char *data, int width, int height, char *fn)
{
  FILE *fp;
  register int x, y;
  int buffer;
  int bits_to_go;
  char *bptr;
  
  fp = fopen(fn, "w");
  if(!fp)
    error("write_pbm_image: Cannot open PBM image");

  /* PBM image identifier */
  fprintf(fp, "P4\n");
  
  /* image size */
  fprintf(fp, "%d %d\n", width, height);
  
  /* image content */
  bptr = data;
  for(y = 0; y < height; y++) {
    bits_to_go = 8; buffer = 0;
    for(x = 0; x < width; x++) {
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
    bptr += width;
  }
  
  fclose(fp);
}

/* Subroutine:	void write_matlab_file()
   Function:	write the input histogram into a MATLAB .mat file
   Input:	histogram and MATLAB file name
   Output:	none
*/
void write_matlab_file(int *hist, int row, int col, char *filename)
{
  FILE *fp;
  char dataname[1000];

  typedef struct {
    long type;
    long row;
    long col;
    long imaginary;
    long namelen;
  } MatFile;
  MatFile mat_file;

  strcpy(dataname, "hist");

  mat_file.type = 20;
  mat_file.row = row;
  mat_file.col = col;
  mat_file.imaginary = 0;
  mat_file.namelen = strlen(dataname)+1;

  fp = fopen(filename, "wb");
  if(!fp) error("write_matlab_file: cannot open *.mat file\n");

  fwrite(&mat_file, sizeof(MatFile), 1, fp);
  fwrite(dataname, sizeof(char), mat_file.namelen, fp);
  fwrite(hist, sizeof(int), row*col, fp);
  fclose(fp);
}

/* Subroutine: 	void write_mark_to_buffer()
   Function: 	writes the given mark to "doc_buffer"
   Input:	the mark and its position(upleft corner) in buffer
   Output:	none
*/
void write_mark_to_buffer(Mark *mark, PixelCoord upleft)
{
  register int i, j;
  char *dptr;
  int width, height;
  
  dptr = mark->data;
  width = mark->width; height = mark->height;
  for(i = 0; i < height; i++) {
    for(j = 0; j < width; j++) 
      if(dptr[j]) write_pixel_to_buffer(upleft.x+j, upleft.y+i, dptr[j]);
    dptr += width;
  }
}

/* Subroutine:  void write_pixel_to_buffer()
   Function:	writes a pixel value into "doc_buffer"
   Input:	the position and the new value
   Output:	none
*/
void write_pixel_to_buffer(int x, int y, char value)
{
  doc_buffer->data[y*(doc_buffer->width+2) + x] = value;
}

/* Subroutine: 	char read_pixel_from_buffer()
   Function:	read a pixel from doc_buffer, returns its value
   Input:	(x, y) coordinates in doc_buffer
   Output:	the pixel value 
*/
char read_pixel_from_buffer(int x, int y)
{
  return (char)(doc_buffer->data[y*(doc_buffer->width+2) + x]);
}

/* Subroutine:	void write_mark_on_template()
   Function:	write the bitmap of a mark onto a template buffer at the given
   		position
   Input:	input mark, the template buffer and its size, the starting 
   		position
   Output:	none
*/
void write_mark_on_template(Mark *mark, char *template_buffer, int tw, int th, 
  	PixelCoord posi)
{
  register int x, y;
  char *tptr, *mptr;
  int x1, y1, x2, y2;
  int lm, rm, tm, bm;
  int mw, mh;
  char *ndata;
  
  mw = mark->width; mh = mark->height;
  
  x1 = posi.x; x2 = posi.x+mw;
  y1 = posi.y; y2 = posi.y+mh;
  if(x1 < 0 || y1 < 0 || x2 > tw ||  y2 > th) {
    printf("write_mark_on_template: part of the mark will be cut\n");
    
    lm = x1 < 0 ? x1:0; rm = x2 < tw ? 0:tw-x2;
    tm = y1 < 0 ? y1:0; bm = y2 < th ? 0:th-y2;
    ndata = (char *)malloc(sizeof(char)*(mw+lm+rm)*(mh+tm+bm));
    if(!ndata) error("write_mark_on_template: Cannot allocate memory\n");
    copy_data_with_margin(mark->data, mw, mh, lm, rm, tm, bm, ndata);
    
    x1 = x1 > 0  ? x1:0;  y1 = y1 > 0  ? y1:0;
    x2 = x2 < tw ? x2:tw; y2 = y2 < th ? y2:th;
    mptr = ndata; tptr = template_buffer + y1*tw;
    for(y = y1; y < y2; y++) {
      for(x = x1; x < x2; x++)
        if(mptr[x-x1]) tptr[x] = mptr[x-x1]; 
      mptr += mw+lm+rm; tptr += tw;
    } 
  
    free((void *)ndata);
  }
  else {
    tptr = template_buffer + y1*tw; mptr = mark->data;
    for(y = y1; y < y2; y++) {
      for(x = x1; x < x2; x++) 
        if(mptr[x-x1]) tptr[x] = mptr[x-x1];
      tptr += tw; mptr += mw;
    }
  }
}

/* Subroutine:	void write_mark_image()
   Function:	write the input mark image into "temp.pbm", only for debugging
   		purpose
   Input:	the mark
   Output:	none
*/
void write_mark_image(Mark *mark)
{
  write_pbm_image(mark->data, mark->width, mark->height, "temp.pbm");
}

/* Subroutine:	void read_pbm_file_header()
   Function:	read the PBM file header
   Input:	none
   Output:	none
*/
void read_pbm_file_header(void)
{
  char line[2000];
  
  /* PBM file identifier */
  fgets(line, 2000, codec->fp_in);
  if(strncmp(line, "P4", strlen("P4"))) {
    fprintf(stderr, "Input file is not in PBM format\n");
    exit(1);
  }
  
  /* skip possible comments */
  fgets(line, 2000, codec->fp_in);
  while(line[0] == '#' || line[0] == '\n') 
    fgets(line, 2000, codec->fp_in);
  
  /* image size */
  sscanf(line, "%d %d", &codec->doc_width, &codec->doc_height); 

  /* skip possible comments */
  fgets(line, 2000, codec->fp_in);
  while(line[0] == '#' || line[0] == '\n') 
    fgets(line, 2000, codec->fp_in);
}

/* Subroutine:	void buffer_coded_bitstream()
   Function:	write the coded bitstream into the input buffer. This is to
   		buffer the coded segment data before writing it to a file
   Input:	coded bitstream and the segment data buffer
   Output:	none
*/
void buffer_coded_bitstream(ARITH_CODED_BITSTREAM *bitstr, 
			    CODED_SEGMENT_DATA *buffer)
{
  if(bitstr->coded_size == 0) /* nothing new to buffer */
    return;
  
  if(buffer->size+(bitstr->coded_size>>3) > MAX_CODED_DATA_SIZE)
    error("buffer_coded_bitstream: pre-allocated buffer size is too small\n");
    
  memcpy(buffer->data+buffer->size, bitstr->data, 
   	sizeof(char)*(bitstr->coded_size>>3));

  buffer->size += bitstr->coded_size>>3;
}

/* Subroutine:	void write_JBIG2_header()
   Function:	write the JBIG2 file header as specified in CD
   Input:	none
   Output:	none
*/
void write_JBIG2_header()
{
  int flags;
  
  int temp; 

  /* JBIG2 ID STRING */
  fwrite(JBIG2_ID_STRING, sizeof(char), 8, codec->fp_out);

  /* File header flags: sequential file structure, page number known */
  flags = 0x01;
  output_byte(flags);
  
  /* Number of pages */
  output_int(codec->page_num);
}

/* Subroutine:	void write_segment_header()
   Function:	write segment header as specified in CD
   Input:	segment type, data length
   Output:	none
*/
void write_segment_header(SegHeader *header)
{
  register int i;
  int flags;
  int ref_seg_count_and_ret;
  
  output_int(codec->cur_seg);
  
  /* deferred-non-retain = 0, page_asso_size = 0 */
  flags = (header->type & 0x3f);
  output_byte(flags);
  
  if(header->ref_seg_count <= 4) {
    ref_seg_count_and_ret = header->retain_this;
    for(i = 0; i < header->ref_seg_count; i++) 
       ref_seg_count_and_ret |= header->retain_ref[i] << (i+1);
    ref_seg_count_and_ret |= (header->ref_seg_count & 0x07) << 5;
    output_byte(ref_seg_count_and_ret);
  }
  else error("write_segment_header: too many referenced segments\n");
  
  if(codec->cur_seg <= 256) {
    for(i = 0; i < header->ref_seg_count; i++) 
      output_byte(header->ref_seg[i]);
  }
  else if(codec->cur_seg <= 65536) {
    for(i = 0; i < header->ref_seg_count; i++) 
      output_short(header->ref_seg[i]); 
  }
  else {
    for(i = 0; i < header->ref_seg_count; i++) 
      output_int(header->ref_seg[i]);
  }
  
  /* note page_asso_size = 0 is always true */
  output_byte(header->page_asso);
  output_int(header->seg_length); 
  
  codec->cur_seg++;
}

/* Subroutine:	void write_region_seg_info_field()
   Function:	write the region segment information field as specified in CD
   Input:	region size and location, external combination operator
   Output:	none
*/
void write_region_seg_info_field(RegionInfoField *info)
{
  int flags;
  
  output_int(info->width);
  output_int(info->height);
  output_int(info->locx);
  output_int(info->locy);
  
  if(info->excombop < 0 || info->excombop > 4) 
    error("write_region_seg_info_field: illegal excombop value, check code\n");
    
  flags = info->excombop;
  output_byte(flags);
}

/* Subroutine:	void write_gen_reg_seg_header()
   Function:	write generic region segment header as specified in CD
   Input:	typical prediction on? which template? mmr coding?
   Output:	none 
*/
void write_gen_reg_seg_header(GenRegionDataHeader *header)
{
  int flags;
  
  write_region_seg_info_field(&header->reg_info);

  flags = (header->tpdon & 0x01) << 3;
  flags |= (header->rtemplate & 0x03) << 1;
  flags |= header->mmr & 0x01;
  output_byte(flags);
  
  if(!header->mmr) {
    switch(header->rtemplate) {
    case 0:
      output_byte(header->atx[0]);
      output_byte(header->aty[0]);
      output_byte(header->atx[1]);
      output_byte(header->aty[1]);
      output_byte(header->atx[2]);
      output_byte(header->aty[2]);
      output_byte(header->atx[3]);
      output_byte(header->aty[3]);
      break;
    case 1:
    case 2:
    case 3:
      output_byte(header->atx[0]);
      output_byte(header->aty[0]);
      break;
    default:
      break;      
    }
  }
}

/* Subroutine:	void write_gen_ref_reg_seg_header()
   Function:	write generic refinement region segment header as 
   		specified in CD
   Input:	typical prediction on? which rtemplate? 
   Output:	none 
*/
void write_gen_ref_reg_seg_header(GenRefRegionDataHeader *header)
{
  int flags;
  
  write_region_seg_info_field(&header->reg_info);

  flags = (header->tpdon & 0x01) << 1;
  flags |= (header->rtemplate & 0x01);
  output_byte(flags);
  
  if(header->rtemplate == 0) {
    output_byte(header->atx[0]);
    output_byte(header->aty[0]);
    output_byte(header->atx[1]);
    output_byte(header->aty[1]);
  }
}

/* Subroutine:	void write_sym_reg_seg_header()
   Function:	write symbol region segment header as specified in CD
   Input:	symbol region segment data header
   Output:	none 
*/
void write_sym_reg_seg_header(SymRegionDataHeader *header)
{
  int flags;
  
  extern int twos_complement(int, int);
  
  write_region_seg_info_field(&header->reg_info);
  
  flags = (header->rtemplate & 0x01) << 15;
  flags |= (twos_complement(header->dsoffset, 5) & 0x1f) << 10;
  flags |= (header->def_pixel & 0x01) << 9;
  flags |= (header->combop & 0x03) << 7;
  flags |= (header->transposed & 0x01) << 6;
  flags |= (header->refcorner & 0x03) << 4;
  flags |= (header->logsbstrips & 0x03) << 2;
  flags |= (header->refine & 0x01) << 1;
  flags |= header->huff & 0x01;
  
  output_short(flags);
  
  if(header->huff) {
    flags = (header->rsize_tbl & 0x01) << 14;
    flags |= (header->rdy_tbl & 0x03) << 12;
    flags |= (header->rdx_tbl & 0x03) << 10;
    flags |= (header->rdh_tbl & 0x03) << 8;
    flags |= (header->rdw_tbl & 0x03) << 6;
    flags |= (header->dt_tbl & 0x03) << 4;
    flags |= (header->ds_tbl & 0x03) << 2;
    flags |= header->fs_tbl & 0x03;
  
    output_short(flags);    
  }
  
  if(header->refine && (header->rtemplate == 0)) {
    output_byte(header->atx[0]);
    output_byte(header->aty[0]);
    output_byte(header->atx[1]);
    output_byte(header->aty[1]);
  }
  
  output_int(header->numinstances);
  
  /* no symbol ID Huffman decoding table */
}


/* Subroutine:	void write_sym_dict_seg_header()
   Function:	write symbol dictionary segment header as specified in CD
   Input:	symbol dictionary segment data header
   Output:	none 
*/
void write_sym_dict_seg_header(SymDictDataHeader *header)
{
  int flags;
  
  flags = (header->rtemplate & 0x01) << 12;
  flags |= (header->dtemplate & 0x03) << 10;
  flags |= (header->ctx_retained & 0x01) << 9;
  flags |= (header->ctx_used & 0x01) << 8;
  if(header->huff) {
    flags |= (header->agginst_tbl & 0x01) << 7;
    flags |= (header->bmsize_tbl & 0x01) << 6;
    flags |= (header->dw_tbl & 0x03) << 4;
    flags |= (header->dh_tbl & 0x03) << 2;
  }
  flags |= (header->refagg & 0x01) << 1;
  flags |= header->huff & 0x01;
  
  output_short(flags);
    
  if(!header->huff) {
    switch(header->dtemplate) {
    case 0:
      output_byte(header->atx[0]);
      output_byte(header->aty[0]);
      output_byte(header->atx[1]);
      output_byte(header->aty[1]);
      output_byte(header->atx[2]);
      output_byte(header->aty[2]);
      output_byte(header->atx[3]);
      output_byte(header->aty[3]);
      break;
    case 1:
    case 2:
    case 3:
      output_byte(header->atx[0]);
      output_byte(header->aty[0]);
      break;
    default:
      break;      
    }
  }

  if(header->refagg && (header->rtemplate == 0)) {
    output_byte(header->ratx[0]);
    output_byte(header->raty[0]);
    output_byte(header->ratx[1]);
    output_byte(header->raty[1]);
  }
    
  output_int(header->numexsyms);
  output_int(header->numnewsyms);
}

