
#include "doc_coder.h"
#include "dictionary.h"
#include "encode.h"
#include "entropy.h"
#include "mq.h"

void encode_cleanup_image(void);
void refine_encode_original_image(void);

extern void bin_encode_direct(char *, int, int, ARITH_CODED_BITSTREAM *);
extern void bin_encode_refine(char *, int, int, char *, int, int, int, int,
	ARITH_CODED_BITSTREAM *);
extern void reset_arith_bitmap_coders(void);
extern void arith_encode_init(void);
extern void arith_encode_flush(ARITH_CODED_BITSTREAM *);

extern int  write_coded_bitstream(char *, int);
extern void write_segment_header(SegHeader *);
extern void error(char *);

extern Codec		*codec;
extern PixelMap 	*cleanup;
extern PixelMap		*doc_buffer;
extern PixelMap 	*ori_buffer;

/* Subroutine:	void encode_cleanup_image()
   Function:	encode the cleanup image
   Input:	none
   Output:	none
*/
void encode_cleanup_image()
{
  SegHeader header;
  GenRegionDataHeader data_header;
    
  ARITH_CODED_BITSTREAM bitstr;
  int out_bits;

  int ori_file;	/* file size before this segment is sent */

  extern void write_gen_reg_seg_header(GenRegionDataHeader *);
  
  bitstr.max_buffer_size = ((cleanup->width*cleanup->height)>>3)>>3;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data)
    error("encode_cleanup_image: cannot allocate memory\n");
  
  ori_file = ftell(codec->fp_out);
  
  reset_arith_bitmap_coders();
  arith_encode_init();
  
  bin_encode_direct(cleanup->data, cleanup->width, cleanup->height, &bitstr);
  codec->report.uncoded_cleanup_bits = bitstr.uncoded_size;
  
  arith_encode_flush(&bitstr);
  
  /* write generic region segment header */
  header.type = IM_GEN_REG;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.page_asso = codec->cur_page+1;
    
  /* we know the generic region segment data header is 20 bytes long:
     1. 17 bytes for region segment information field, and
     2. 1 byte for generic region segment flags, and
     3. 2 bytes for adaptive template pixel info when template = 2 */
  header.seg_length = 20; 
  header.seg_length += bitstr.coded_size >> 3;
  write_segment_header(&header);

  /* write generic region segment data header */
  data_header.reg_info.width = doc_buffer->width;
  data_header.reg_info.height = doc_buffer->height;
  data_header.reg_info.locx = 0;
  data_header.reg_info.locy = doc_buffer->top_y;
  data_header.reg_info.excombop = JB2_OR;

  data_header.tpdon = FALSE; data_header.rtemplate = 2;
  data_header.mmr = FALSE;
  data_header.atx[0] = 2; data_header.aty[0] = -1;
  write_gen_reg_seg_header(&data_header);
  
  /* write generic region segment data */
  out_bits = write_coded_bitstream(bitstr.data, bitstr.coded_size);
  codec->report.cleanup_bits = out_bits;
  free((void *)bitstr.data);

  codec->report.gen_region_size = ftell(codec->fp_out) - ori_file;
}

/* Subroutine:	void refine_encode_original_image()
   Function:	encode the original image with respect to a blank image, for 
   		test purposes ONLY
   Input:	none
   Output:	none
*/
void refine_encode_original_image()
{
  SegHeader header;
  GenRefRegionDataHeader data_header;
  
  ARITH_CODED_BITSTREAM bitstr;
  int out_bits;

  int ori_file;	/* file size before this segment is sent */

  extern void write_gen_ref_reg_seg_header(GenRefRegionDataHeader *);
    
  bitstr.max_buffer_size = ((cleanup->width*cleanup->height)>>3)>>3;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data)
    error("refine_encode_original_image: cannot allocate memory\n");

  ori_file = ftell(codec->fp_out);
  
  reset_arith_bitmap_coders();
  arith_encode_init();
  
  bin_encode_refine(cleanup->data, cleanup->width, cleanup->height,
  		    ori_buffer->data, ori_buffer->width, ori_buffer->height, 
		    0, 0, &bitstr);
  codec->report.uncoded_residue_bits = bitstr.uncoded_size;

  arith_encode_flush(&bitstr);
    
  /* write generic refinement segment header */
  header.type = IM_GEN_REF_REG;
  header.retain_this = FALSE;
  header.ref_seg_count = 0;
  header.page_asso = codec->cur_page+1;

  /* the generic refinement region segment data header is 18 bytes long:
     1. 17 bytes for region segment information field, and
     2. 1 byte for generic region segment flags, and
     3. 0 bytes for adaptive template pixel info when template = 1 */
  header.seg_length = 18; 
  header.seg_length += bitstr.coded_size >> 3;
  write_segment_header(&header);

  /* write generic refinement region segment data header */
  data_header.reg_info.width = doc_buffer->width;
  data_header.reg_info.height = doc_buffer->height;
  data_header.reg_info.locx = 0;
  data_header.reg_info.locy = doc_buffer->top_y;
  data_header.reg_info.excombop = JB2_OR;

  data_header.tpdon = FALSE; data_header.rtemplate = 1;
  write_gen_ref_reg_seg_header(&data_header);
  
  /* write generic refinement region segment data */
  out_bits = write_coded_bitstream(bitstr.data, bitstr.coded_size);
  codec->report.residue_bits = out_bits;
  free((void *)bitstr.data);

  codec->report.gen_ref_region_size = ftell(codec->fp_out) - ori_file;
}

