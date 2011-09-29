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
#include "dictionary.h"

int huffman(int, HUFF_TABLE, int);
int fixlen(int, int, int);
void huff_code(int, HUFF_TABLE, int *, int *);
int fixlen_code(int, int), find_range(int, RANGE_TABLE *, int);
/*void arith_encode(char *, int, int, char *, int, int, int);*/

extern void output_bits(int, int);
/*extern void arith_encode_direct(char *, int, int);
extern void arith_encode_refine(char *, int, int, char *, int, int);*/
extern void error(char *);

/* Subroutine:	void fixlen()
   Function:	fixed-length code the input integar number with the
   		given length
   Input:	the value, its code length in bits and SIGNED/UNSIGNED 
   		coding mode 
   Output:	bits used
*/
int fixlen(int value, int length, int mode)
{
  register int cword;
  
  if(mode == UNSIGNED) {
    cword = fixlen_code(value, length);
    output_bits(cword, length);
  }
  else {
    /* fixed length code magnitude value		*/
    cword = fixlen_code(abs(value), length-1);

    /* output sign bit first then magnitude		*/
    if(value >= 0) output_bits(0, 1);
    else output_bits(1, 1);
    output_bits(cword, length-1);
  }
  
  return length;
}

/* Subroutine:	void huffman()
   Function:	Huffman code the input number using give (standard) code 
   		table
   Input:	value to be coded, code table and accompanying range table,
   		the flag whether this code is to be written into output
   Output:	none
*/
int huffman(int value, HUFF_TABLE table, int write)
{
  int cvalue, clen;

  huff_code(value, table, &cvalue, &clen);
  if(write) output_bits(cvalue, clen);

  return clen;
}

/* Subroutine: 	huff_code()
   Function:	get the huffman codeword for the input value using 
   		the input table
   Input:	the value to be coded and the code table
   Output:	the coded value and its length
*/
void huff_code(int value, HUFF_TABLE table, int *cvalue, int *clen)
{
  int range;
  int prelen, rangelen, prefix;
  int start, end;
  int tvalue;

  if(value == OOB) {
    *cvalue = table.oobcode;
    *clen = table.ooblen;
    return;
  }
    
  range = find_range(value, table.rtab, table.size);
  prefix = table.ctab[range].prefix; 
  prelen = table.ctab[range].prelen; 
  rangelen = table.ctab[range].rangelen;
  start = table.rtab[range].start;
  end = table.rtab[range].end;
  if(value < 0) tvalue = end-value;
  else tvalue = value-start;
  
  if(rangelen > 0) tvalue = fixlen_code(tvalue, rangelen);
  *cvalue = prefix << rangelen;
  *cvalue = *cvalue | tvalue;
  *clen = prelen+rangelen;
}

/* Subroutine:	fixlen_code()
   Function:	get a codeword for the input value with the designated length
   Input:	the value to be coded and its code length 
   Output:	code word(start from MSB)
*/
int fixlen_code(int value, int length)
{
  register int cword;
  register unsigned int mask;
   
  /* get a mask of the form 00..0011...11 with "length" 1's	*/
  if(length < 32) mask = (1 << length) - 1;
  else mask = 0xffffffff;
  
  /* check if the input value is too big to be coded */
  if(value > mask) 
    error("fixlen_code: attempted to code too big a value\n");

  /* keep the last "length" bits				*/
  cword = value & mask;
  return cword;
}

/* Subroutine: 	int find_range()
   Function:	find where the input value lies in the Huffman table
   Input:	the value, the range table and its size
   Output:	the range
*/
int find_range(int value, RANGE_TABLE *rtab, int tab_size)
{
  register int i;
  
  for(i = 0; i < tab_size; i++) 
    if(value >= rtab[i].start && value <= rtab[i].end) break;
  
  return i;
}

