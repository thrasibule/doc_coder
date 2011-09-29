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
#include <assert.h>

void create_arith_coders(void);
void destroy_arith_coders(void);
void reset_arith_int_coders(void);
void reset_arith_bitmap_coders(void);

void bin_encode_direct(char *, int, int, ARITH_CODED_BITSTREAM *);
void bin_encode_refine(char *, int, int, char *, int, int, int, int,
	ARITH_CODED_BITSTREAM *);

void int_encode(int, int, ARITH_CODED_BITSTREAM *);
void symID_encode(int, ARITH_CODED_BITSTREAM *);

void arith_encode_init(void);
void arith_encode_flush(ARITH_CODED_BITSTREAM *bitstr);
void arith_encode_bit(int cx, int pix, ARITH_CODED_BITSTREAM *bitstr);

static void arith_encode_byte_out(ARENC_STATE *, ARITH_CODED_BITSTREAM *);
static void arith_encode_set_bits(ARENC_STATE *, ARITH_CODED_BITSTREAM *);
static void arith_encode_send_byte(ARENC_STATE *, ARITH_CODED_BITSTREAM *);
static void write_byte_to_stream(int, ARITH_CODED_BITSTREAM *);

extern int  write_coded_bitstream(char *, int);
extern void huff_code(int, HUFF_TABLE, int *, int *);
extern void copy_data_with_margin(char *, int, int, int, int, int, int, char *);
extern void error(char *);

extern int 		symbol_ID_in_bits;
extern Codec		*codec;

extern ARENC_STATE	*mq_coder;

static ARENC_BUFFER	*direct;
static ARENC_BUFFER	*refine;
static ARENC_BUFFER	*num[NUM_INTARITH_CODERS];

extern HUFF_TABLE	int_arith;

/* Subroutine:	void create_arith_coders()
   Function:	allocate memory for all integer arithmetic coders 
   		and both bitmap coders
   Input:	none
   Output:	none
*/
void create_arith_coders()
{
  register int i;
  
  /* create mq_coder */
  mq_coder = (ARENC_STATE *)malloc(sizeof(ARENC_STATE)*1);
  if(!mq_coder) 
      error("create_arith_coders: cannot allocate memory for MQ coders\n");
  
  /* allocate memory for its contexts, */
  mq_coder->st = (unsigned char *)
	malloc(sizeof(unsigned char)*(1<<TOTAL_CONTEXT_BITS));
  if(!mq_coder->st)
      error("create_arith_coders: cannot allocate memory for QM coder\n");

  /* integer arithmetic coders */
  for(i = 0; i < NUM_INTARITH_CODERS; i++) 
    num[i] = mq_coder->st + (i << MAX_SYMBOL_ID_LEN);

  /* direct bitmap coder */
  direct = mq_coder->st + (DIRECT << MAX_SYMBOL_ID_LEN);

  /* refinement bitmap coder */
  refine = mq_coder->st + (REFINE << MAX_SYMBOL_ID_LEN);

  arith_encode_init();
}

/* Subroutine:	void destroy_arith_coders()
   Function:	free memories allocated for all integer arithmetic coders
		and both bitmap coders
   Input:	none
   Output:	none
*/
void destroy_arith_coders()
{
  free((void *)mq_coder->st);
  free((void *)mq_coder);
}

/* Subroutine:	void reset_arith_int_coders()
   FUnction:	reset all the arithmetic integer coders' statistics
   Input:	none
   Output:	none
*/
void reset_arith_int_coders()
{
  register int i;
  
  for(i = 0; i < NUM_INTARITH_CODERS; i++) 
    memset(num[i], 0, sizeof(ARENC_BUFFER)*(1<<MAX_SYMBOL_ID_LEN));
}

/* Subroutine:	void reset_arith_bitmap_coders()
   Function:	reset the two arithmetic bitmap coders' statistics
   Input:	none
   Output:	none
*/
void reset_arith_bitmap_coders()
{
  memset(direct, 0, sizeof(ARENC_BUFFER)*(1<<MAX_SYMBOL_ID_LEN));
  memset(refine, 0, sizeof(ARENC_BUFFER)*(1<<MAX_SYMBOL_ID_LEN));
}

/* Subroutine:	void bin_encode_direct()
   Function:	binary encode the input bitmap
   Input:	the bitmap and its size, the arithmetic coder to be used
   Output:	coded bitstream
*/
void bin_encode_direct(char *data, int width, int height,
	ARITH_CODED_BITSTREAM *bitstr)
{
    register int i, j;
    int overall_context, cur_context;
    unsigned int pp_mask, p_mask, c_mask;
    unsigned int pp_line, p_line, c_line;
    unsigned int mask, word;
    int bits_context;	
    char *dptr;
    char *n_data;
    
    /* set up coding data buffer with proper margins	*/
    n_data = (char *)malloc(sizeof(char)*(width+4)*(height+2));
    if(!n_data)
      error("bin_encode_direct: Cannot allocate memory\n");
    copy_data_with_margin(data, width, height, 2, 2, 2, 0, n_data);

    /* 10 bits for direct bitmap coding 	*/
    bits_context = BITMAP_ARITH_CTX_BITS;
    pp_mask = 0x07; p_mask = 0x1f; c_mask = 0x03;
       
    /* initalise variables */
    mask = (1 << (bits_context)) - 1;
    overall_context = DIRECT << MAX_SYMBOL_ID_LEN;
    
    dptr = n_data+2*(width+4)+2; 
    bitstr->coded_size = 0;
    bitstr->uncoded_size = width*height;
    for(i = 0; i < height; i++) {
      /* initialize context information from previous lines*/
      p_line = (*(dptr-(width+4)-2) != 0);
      p_line = (p_line << 1) | (*(dptr-(width+4)-1) != 0);
      p_line = (p_line << 1) | (*(dptr-(width+4)) != 0);
      p_line = (p_line << 1) | (*(dptr-(width+4)+1) != 0);

      pp_line = (*(dptr-2*(width+4)-1) != 0); 
      pp_line = (pp_line << 1) | (*(dptr-2*(width+4)) != 0);

      c_line = 0;
      
      for(j = 0; j < width; j++) 
      {
      	/* context formulation */
	      p_line = (p_line << 1) | (*(dptr-(width+4)+j+2) != 0);
	      p_line &= p_mask;
	      pp_line = (pp_line << 1) | (*(dptr-2*(width+4)+j+1) != 0);
	      pp_line &= pp_mask;
	      cur_context = ((pp_line << 7)|(p_line << 2)|(c_line)) & mask;
	
	      word = dptr[j];
	      if(word == 0) {
	            arith_encode_bit(overall_context | cur_context, 0, bitstr);
	            c_line = ((c_line << 1)) & c_mask;
	      }
	      else if(word == 1) {
	            arith_encode_bit(overall_context | cur_context, 1, bitstr);
	            c_line = ((c_line << 1) | 1) & c_mask;
	      }
	      else 
          error("bin_encode_direct: illegal non-binary value!!!\n");
      }
      dptr += width+4;
    }

    free((void *)n_data);
}

/* Subroutine:	void bin_encode_refine()
   Function:	binary/ternary/quatenary encode the input bitmap using the
   		reference bitmap
   Input:	the bitmaps, their sizes, and relative location offsets
   Output:	bits used
*/
void bin_encode_refine(char *rdata, int rwidth, int rheight, 
	char *data, int width, int height, int rdx, int rdy, 
	ARITH_CODED_BITSTREAM *bitstr)
{
    register int i, j;
    int overall_context, cur_context;
    unsigned int p_mask, c_mask;
    unsigned int p_line, c_line;
    unsigned int rp_mask, rc_mask, rn_mask;
    unsigned int rp_line, rc_line, rn_line;
    unsigned int mask, word;
    int bits_context;	
    char *dptr, *rdptr;
    char *n_data, *n_rdata;
    int	cwidth, cheight;
    int lm, rm, tm, bm;
    int	rlm, rrm, rtm, rbm;
    int eff_width;
    
    /* setup coding buffers for the two bitmaps with proper margins */
    lm = rdx>0 ? 0:-rdx; lm++;
    rm = (width-rdx)>rwidth ? 0:rwidth-(width-rdx); rm++;
    tm = rdy>0 ? 0:-rdy; tm++;
    bm = (height-rdy)>rheight ? 0:rheight-(height-rdy); bm++;
    n_data = (char *)malloc(sizeof(char)*(width+lm+rm)*(height+tm+bm));
    if(!n_data)
      error("bin_encode_refine: Cannot allocate memory\n");
    copy_data_with_margin(data, width, height, lm, rm, tm, bm, n_data);
    
    rlm = rdx>0 ? rdx:0; rlm++;
    rrm = (width-rdx)>rwidth ? (width-rdx)-rwidth:0; rrm++;
    rtm = rdy>0 ? rdy:0; rtm++;
    rbm = (height-rdy)>rheight ? (height-rdy)-rheight:0; rbm++;
    n_rdata = (char *)malloc(sizeof(char)*(rwidth+rlm+rrm)*(rheight+rtm+rbm));
    if(!n_rdata)
      error("bin_encode_refine: Cannot allocate memory\n");
    copy_data_with_margin(rdata, rwidth, rheight, rlm, rrm, rtm, rbm, n_rdata);
   
    if(width+lm+rm != rwidth+rlm+rrm || height+tm+bm != rheight+rtm+rbm)
      error("bin_encode_refine: illogical error, check code\n");
    
    cwidth = width+lm+rm-2; cheight = height+bm+tm-2;
    
    /* 10 bits for refinement bitmap coding */
    bits_context = BITMAP_ARITH_CTX_BITS;
    p_mask = 0x07; c_mask = 0x01;
    rp_mask = 0x01; rc_mask = 0x07; rn_mask = 0x03; 
    
    /* initalise variables */
    mask = (1 << (bits_context)) - 1;
    overall_context = REFINE << MAX_SYMBOL_ID_LEN;
    
    eff_width = cwidth+2;
    dptr = n_data+eff_width*tm+lm; rdptr = n_rdata+eff_width*tm+lm;
    bitstr->uncoded_size = width*height;
    bitstr->coded_size = 0;
    for(i = 0; i < height; i++) {
      /* initialize context information from previous lines*/
      p_line = (*(dptr-eff_width-1) != 0);
      p_line = (p_line << 1) | (*(dptr-eff_width) != 0);
      c_line = 0;
      
      rc_line = (*(rdptr-1) != 0);
      rc_line = (rc_line << 1) | (*(rdptr) != 0);
      rn_line = (*(rdptr+eff_width) != 0);

      for(j = 0; j < width; j++) {
      	/* context formulation */
	p_line = (p_line << 1) | (*(dptr-(eff_width)+j+1) != 0);
	p_line &= p_mask;
      	rp_line = (*(rdptr-eff_width+j) != 0);
      	rp_line &= rp_mask;
	rc_line = (rc_line << 1) | (*(rdptr+j+1) != 0);
	rc_line &= rc_mask; 
      	rn_line = (rn_line << 1) | (*(rdptr+eff_width+j+1) != 0);
	rn_line &= rn_mask;
	
	cur_context = 
	  ((rp_line<<9)|(rc_line<<6)|(rn_line<<4)|(p_line<<1)|(c_line))&mask;
	
	word = dptr[j];
	if(word == 0) {
	      arith_encode_bit(overall_context | cur_context, 0, bitstr);
	      c_line = ((c_line << 1)) & c_mask;
	}
	else if(word == 1) {
	      arith_encode_bit(overall_context | cur_context, 1, bitstr);
	      c_line = ((c_line << 1) | 1) & c_mask;
	}
	else error("bin_encode_refine: illegal non-binary value\n");
      }
      dptr += eff_width;
      rdptr += eff_width;
    }
    
    free((void *)n_data);
    free((void *)n_rdata);
}

/* Subroutine:	void int_encode()
   Function:	arithmetic encode the input integar value
   Input:	the integar value and its type
   Output:	coded bitstream 
*/
void int_encode(int value, int type, ARITH_CODED_BITSTREAM *bitstr)
{
  register int i; 
  int overall_context, cur_context;
  int bit, cword, clen;

  /* get the code word that represents this integar value */    
  huff_code(value, int_arith, &cword, &clen);

  bitstr->coded_size = 0;
  bitstr->uncoded_size = clen;
  overall_context = (type << MAX_SYMBOL_ID_LEN);
  cur_context = 1;
  for(i = 0; i < clen; i++) {
    bit = (cword >> (clen-1-i)) & 1;
    arith_encode_bit(overall_context | cur_context, bit, bitstr);
	
    /* update context */
    if(cur_context < 256)
      cur_context = (cur_context<<1) | bit;
    else 
      cur_context = (((cur_context<<1) | bit) & 511) | 256;
  }
}

/* Subroutine:	void symID_encode()
   Function:	arithmetic encode the input symbol ID
   Input:	the symbol ID
   Output:	coded bitstream 
*/
void symID_encode(int ID, ARITH_CODED_BITSTREAM *bitstr)
{
  register int i;
  int overall_context, cur_context;
  int bit, cword, clen;

  /* get the code word */
  cword = ID; clen = symbol_ID_in_bits;
    
  bitstr->coded_size = 0;
  bitstr->uncoded_size = clen;
  overall_context = IAID << MAX_SYMBOL_ID_LEN;
  cur_context = 1;
  for(i = 0; i < clen; i++) {
    bit = (cword >> (clen-1-i)) & 1;
    arith_encode_bit(overall_context | cur_context, bit, bitstr);
	
    /* update context */
    cur_context = (cur_context<<1) | bit;
  }
}

/*
 * The next functions implement the arithmedic encoder and decoder
 * required for JBIG. The same algorithm is also used in the arithmetic
 * variant of JPEG.
 */

void arith_encode_init()
{
  ARENC_STATE *s;
  
  s = mq_coder;
  
  s->c = 0;
  s->a = 0x8000;
  s->ct = 12;
  s->buffer = 0;    /* empty */
  
  s->first_byte = TRUE;
  s->last_byte = FALSE;
  
  s->nff = s->n7f = 0;
}

void arith_encode_flush(ARITH_CODED_BITSTREAM *bitstr)
{
  ARENC_STATE *s;
  
  s = mq_coder;
  arith_encode_set_bits(s, bitstr);

  /* send remaining bytes to output */
  s->c <<= s->ct;
  arith_encode_byte_out(s, bitstr);
  
  s->c |= 0x7fff;
  s->c <<= s->ct;
  arith_encode_byte_out(s, bitstr);
  
  s->c |= 0x7fff;
  
  if(s->c != 0xff) {
    s->c <<= s->ct;
    arith_encode_byte_out(s, bitstr);
  }
  
  s->last_byte = TRUE;
  arith_encode_send_byte(s, bitstr);
  
  write_byte_to_stream(0xac, bitstr);
}

void arith_encode_bit(int cx, int pix, ARITH_CODED_BITSTREAM *bitstr) 
{
  ARENC_STATE *s;
  register unsigned lsz, ss;
  register ARENC_BUFFER *st;
  int renorm;

  extern short jbg_lsz[];
  extern unsigned char jbg_nmps[], jbg_nlps[], jbg_swtch[];

  s = mq_coder;  
  
  st = s->st + cx;
  ss = *st & 0x3f;
  assert(ss >= 0 && ss < 47);
  lsz = jbg_lsz[ss];

  renorm = FALSE;
  if (((pix << 7) ^ s->st[cx]) & 0x80) {
    /* encode the less probable symbol */
    if ((s->a -= lsz) < lsz) 
      s->c += lsz;
    else s->a = lsz;
    if(jbg_swtch[ss]) *st ^= 0x80;
    *st = (*st & 0x80) | (jbg_nlps[ss] & 0x7f);
    renorm = TRUE;
  } 
  else {
    /* encode the more probable symbol */
    if (((s->a -= lsz) & 0x8000) == 0) {
      if(s->a < lsz)
        s->a = lsz;
      else s->c += lsz;
      *st = (*st & 0x80) | (jbg_nmps[ss] & 0x7f);
      renorm = TRUE;
    }
    else s->c += lsz;
  }

  /* renormalization of coding interval */
  if(renorm)
    do {
      s->a <<= 1;
      s->c <<= 1;
      --s->ct;
      if (s->ct == 0) 
	arith_encode_byte_out(s, bitstr);
    } while ((s->a & 0x8000) == 0);
 
  return;
}

static void arith_encode_byte_out(ARENC_STATE *s, 
	ARITH_CODED_BITSTREAM *stream)
{
  int pad;
  
  pad = FALSE;
  if(s->buffer == 0xff) pad = TRUE;
  else
    if(s->c > 0x7ffffff) {
      s->buffer++;
      if(s->buffer == 0xff) {
        s->c &= 0x7ffffff;
	pad = TRUE;
      }
    }
  arith_encode_send_byte(s, stream);

  if(pad) {
    s->buffer = s->c >> 20;
    s->c &= 0xfffff;
    s->ct = 7;
  }
  else {
    s->buffer = s->c >> 19;
    s->c &= 0x7ffff;
    s->ct = 8;
  }
}

static void arith_encode_send_byte(ARENC_STATE *s, 
	ARITH_CODED_BITSTREAM *stream)
{
  if(s->first_byte) s->first_byte = FALSE;
  else {
    if(((s->buffer==0x7f) && (s->n7f==s->nff-1)) ||
       ((s->buffer==0xff) && (s->nff == s->n7f))) {
      if(s->buffer==0x7f) s->n7f++;
      else s->nff++;
      if(s->last_byte) 
        write_byte_to_stream(0xff, stream);
    }
    else {
      while(s->nff > 0) {
        write_byte_to_stream(0xff, stream);
	s->nff--;
	if(s->n7f > 0) {
	  write_byte_to_stream(0x7f, stream);
	  s->n7f--;
	}
      }
      write_byte_to_stream(s->buffer, stream);
    }
  }
}

static void arith_encode_set_bits(ARENC_STATE *s, ARITH_CODED_BITSTREAM *stream)
{
  unsigned long temp;
  
  temp = s->c + s->a;
  s->c = s->c | 0xffff;
  if(s->c >= temp) s->c -= 0x8000;
}

static void write_byte_to_stream(int byte, ARITH_CODED_BITSTREAM *stream)
{
  if((stream->coded_size>>3) == stream->max_buffer_size)
    error("write_byte_to_stream: pre-allocated bitstream buffer is full\n");
    
  stream->data[stream->coded_size>>3] = (char)byte;
  stream->coded_size += 8;
}

