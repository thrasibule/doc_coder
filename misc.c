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
#include <math.h>

/* list of allowable command line options 	*/
#define PAGE_NUM	1
#define HELP		2
#define STRIP_HEIGHT	3
#define NO_UPDATE	4
#define DICT_TYPE	5
#define IMAGE_PATH	6
#define CONNECTIVITY	7
#define BUFFER_HEIGHT	8
#define SPLIT_NUM	9
#define MISMATCH_THRES	10
#define MATCHING_METHOD 11
#define LOSSY		12
#define PMS             13
#define LLOYD 		14
#define SILENT		15
#define RESIDUE		16
#define ALIGNMENT	17
#define DETAIL		18
#define NEARLOSSLESS	19
#define CLUSTER		20
#define FEATURE		21
#define UNKNOWN_COMMAND	100

void	parse_command_line(int, char **);
void 	help_menu(void), error(char *);
int 	get_option(char *);
void 	copy_data_with_margin(char *, int, int, int, int, int, int, char *);
char 	*copy_mark_data(char *, int);
float   ent(float);
void    reset_codec_report(void);
void 	print_compression_param(void);
void 	print_compression_report(int, int);
void	print_overall_compression_report(void);
void    print_detailed_compression_report(int, int);
int 	twos_complement(int, int);

extern Codec *codec;
extern PixelMap *doc_buffer;

/* Subroutine:	void parse_command_line()
   Function:	parse the command line and store the information given
   Input:	command line parameters
   Output:	none
*/
void parse_command_line(int argc, char *argv[])
{
  int 	i;
  int 	option;
  char 	fn[100];
  int 	file_specified = FALSE;
    
  extern float match_two_marks_XOR(Mark *, Mark *);
  extern float match_two_marks_WXOR(Mark *, Mark *);
  extern float match_two_marks_WAN(Mark *, Mark *);
  extern float match_two_marks_ENT(Mark *, Mark *);
  extern float match_two_marks_CLUSTER(Mark *, Mark *);
      
  extern int prescreen_two_marks(Mark *, Mark *);
  extern int prescreen_two_marks_feature(Mark *, Mark *);
  
  if(argc == 1) help_menu();
  
  codec = (Codec *)malloc(sizeof(Codec)*1);
  if(!codec) error("Cannot allocate memory for codec structure\n");
  
  /* default values */
  codec->page_num = 1;
  codec->dict_type = MIXED;
  codec->lloyd = FALSE;
  codec->no_update = FALSE;
  codec->lossy = FALSE;
  codec->pms = FALSE;
  codec->strip_height = DEFAULT_STRIP_HEIGHT;
  codec->split_num = 1;
  codec->connectivity = FOUR_CONNECT;
  codec->mismatch_thres = DEFAULT_MISMATCH_THRES;
  codec->lth = codec->hth = 0; 
  codec->match_two_marks = match_two_marks_XOR;
  codec->prescreen_two_marks = prescreen_two_marks;
  codec->align = CENTER;
  codec->residue_coding = FALSE;
  codec->nearlossless = FALSE;
  codec->silent = FALSE;
  codec->detail = FALSE;
  strcpy(codec->report.data_path, DEFAULT_PATH);
  
  /* parse command line	*/
  for(i = 1; i < argc; i++) {
    if(argv[i][0] == '-') {
	option = get_option( &argv[i][1] );
	switch(option) {
	case PAGE_NUM:
	  codec->page_num = atoi(argv[++i]);
	  if(codec->page_num > MAX_PAGE_NUM) {
	    codec->page_num = MAX_PAGE_NUM;
	    printf("Warning: document contains too many pages, only %d pages "
	    	   "are processed\n", codec->page_num);
	  } 
	  break;
	case NO_UPDATE:
	  codec->no_update = TRUE;
	  break;
	case DICT_TYPE:
	  i++;
	  if(!strcmp(argv[i], "SE"))
	    codec->dict_type = SE;
	  else if(!strcmp(argv[i], "CLASS"))
	    codec->dict_type = CLASS;
	  else if(!strcmp(argv[i], "TREE"))
	    codec->dict_type = TREE;
	  else if(!strcmp(argv[i], "MIXED"))
	    codec->dict_type = MIXED;
	  else if(!strcmp(argv[i], "OP"))
	    codec->dict_type = OP;
	  else 
	    error("Unknown dictionary type! Use -h for help menu\n");
	  break;
	case LLOYD:
	  codec->lloyd = TRUE;
	  break;
	case STRIP_HEIGHT:
	  codec->strip_height = atoi(argv[++i]);
	  break;
	case SPLIT_NUM:
	  codec->split_num = atoi(argv[++i]);
	  break;
	case IMAGE_PATH:
	  strcpy(codec->report.data_path, argv[++i]);
	  break;
	case CONNECTIVITY:
	  codec->connectivity = atoi(argv[++i]);
	  if(codec->connectivity != EIGHT_CONNECT && 
	     codec->connectivity != FOUR_CONNECT) 
	    error("Invalid value for connectivity rule\n");
	  break;
	case LOSSY:
	  codec->lossy = TRUE;
	  break;
	case PMS:
	  codec->pms = TRUE;
	  codec->lossy = TRUE;
	  break;
	case MISMATCH_THRES:
	  codec->mismatch_thres = (float)atof(argv[++i]);
	  break;
	case CLUSTER:
	  sscanf(argv[++i], "%d,%d", &codec->lth, &codec->hth);
	  break;
	case MATCHING_METHOD:
	  i++;
	  if(!strcmp(argv[i], "XOR"))
	    codec->match_two_marks = match_two_marks_XOR;
	  else if(!strcmp(argv[i], "WXOR"))
	    codec->match_two_marks = match_two_marks_WXOR;
	  else if(!strcmp(argv[i], "WAN"))
	    codec->match_two_marks = match_two_marks_WAN;
	  else if(!strcmp(argv[i], "ENT"))
	    codec->match_two_marks = match_two_marks_ENT;
	  else if(!strcmp(argv[i], "FAST"))
	    codec->match_two_marks = match_two_marks_CLUSTER;
	  else 
	    error("Unknown pattern matching method! Use -h for help menu\n");
	  break;
	case FEATURE:
	  codec->prescreen_two_marks = prescreen_two_marks_feature;
	  break;
	case ALIGNMENT:
	  i++;
	  if(!strcmp(argv[i], "CENTROID"))
 	    codec->align = CENTROID;
	  else if(!strcmp(argv[i], "CENTER"))
	    codec->align = CENTER;
	  else 
	    error("Unknown alignment parameter! Use -h for help menu\n");
	  break;
	case SILENT:
	  codec->silent = TRUE;
	  break;
	case DETAIL:
	  codec->detail = TRUE;
	  break;
	case RESIDUE:
	  codec->residue_coding = TRUE;
	  break;
	case HELP:
	  help_menu();
	  break;
	case UNKNOWN_COMMAND:
	  error("Unknown command line option!!! Use -h for help menu\n");
	  break;
	}
    }
    else if(!file_specified) {
      strcpy(fn, argv[i]);
      file_specified = TRUE;
    }
    else {
    	fprintf(stdout, "Too many file names specified\n");
  	help_menu();
    }
  }
  
  if(!file_specified)
    error("parse_command_line: this encoder version needs an input file\n");

  if(codec->page_num > 1) {
    if(codec->dict_type == OP || codec->dict_type == CLASS || 
       codec->dict_type == TREE)
      error("ONE-PASS or CLASS or TREE dictionary can NOT be used in multipage mode\n");
  }       

  /* in PM&S mode, override specified dictionary type */ 
  if(codec->pms) {
    if(codec->page_num > 1) 
      error("PM&S mode works for only single-page document\n");
    codec->dict_type = PMS_DICT;
  }

  /* residue coding is only meaningful in lossy case */
  if(codec->residue_coding && !codec->lossy) {
    codec->residue_coding = FALSE;
    printf("Warning: in lossless compression, residue coding is not "
           "meaningful and will be overridden.\n");
  }

  /* LLOYD algorithm can only be used with CLASS and MIXED dictionaries */
  if(codec->lloyd) 
    if((codec->dict_type != CLASS) && (codec->dict_type != MIXED)) 
      printf("Warning: the LLOYD mode will be ignored when the dictionary "
      	     "used is not CLASS or MIXED\n");
 
  /* LLOYD algorithm can only be used in single-page coding for now */
  if(codec->lloyd && codec->page_num > 1)
    error("parse_command_line: LLOYD mode is not yet available for multi-page coding");
   
  /* CENTROID alignment is only available in Hamming distance based pattern
     matching now */
  if(codec->match_two_marks != match_two_marks_XOR && 
     codec->align == CENTROID) {
    printf("Warning: CENTROID alignment is only available to "
           "XOR pattern matching now, ignored!\n");
    codec->align = CENTER;
  }

  /* if FAST pattern matching is called for and no explicit thresholds 
     were given, set the thresholds to default */ 
  if(codec->match_two_marks == match_two_marks_CLUSTER &&
     (codec->lth == 0 && codec->hth == 0)) {
    codec->lth = DEFAULT_LOW_THRES; codec->hth = DEFAULT_HIGH_THRES;
  }
  
  /* store the file header information */
  strcpy(codec->report.file_header, fn);
}

/* Subroutine: 	int get_option()
   Function:	translate the command line option 
   Input:	command line option in the form of a string
   Output:	command line option in the form of an integer
*/
int get_option(char *str)
{
  if(!strcmp(str, "pn") || !strcmp(str, "pnum")) return PAGE_NUM;
  else if(!strcmp(str, "dict")) return DICT_TYPE;
  else if(!strcmp(str, "nu") || !strcmp(str, "noupdate")) return NO_UPDATE;
  else if(!strcmp(str, "strip") || !strcmp(str, "s")) return STRIP_HEIGHT;
  else if(!strcmp(str, "split") || !strcmp(str, "sp")) return SPLIT_NUM;
  else if(!strcmp(str, "connect") || !strcmp(str, "c")) return CONNECTIVITY;
  else if(!strcmp(str, "path") || !strcmp(str, "p")) return IMAGE_PATH;
  else if(!strcmp(str, "help") || !strcmp(str, "h")) return HELP;
  else if(!strcmp(str, "mismatch") || !strcmp(str, "mm")) return MISMATCH_THRES;
  else if(!strcmp(str, "pm")) return MATCHING_METHOD;
  else if(!strcmp(str, "lossy") || !strcmp(str, "LOSSY")) return LOSSY;
  else if(!strcmp(str, "pms") || !strcmp(str, "PMS")) return PMS;
  else if(!strcmp(str, "silent")) return SILENT;
  else if(!strcmp(str, "res") || !strcmp(str, "residue")) return RESIDUE;
  else if(!strcmp(str, "lloyd")) return LLOYD;
  else if(!strcmp(str, "align")) return ALIGNMENT;
  else if(!strcmp(str, "detail")) return DETAIL;
  else if(!strcmp(str, "cl") || !strcmp(str, "cluster")) return CLUSTER;            
  else if(!strcmp(str, "feature")) return FEATURE;

  else return UNKNOWN_COMMAND;
}

/* Subroutine: 	void help_menu();
   Function:	print out the help menu;
   Input:	none;
   Output:	none;
*/
void help_menu()
{

  fprintf(stdout, 
	  "  doc_coder [option(s)] [document file] [compressed file]\n");
  fprintf(stdout, "\n  Allowable command line options:\n");
  fprintf(stdout, "\t-path/-p          <image path>\n");
  fprintf(stdout, "\t-connect/-c       <mark connectivity(4 or 8)>\n");
  fprintf(stdout, "\t-strip/-s         <buffer strip height>\n");
  fprintf(stdout, "\t-mismatch/-mm     <mismatch threshold>\n");
  fprintf(stdout, "\t-pm               <pattern matching(XOR/WXOR/WAN/ENT/FAST)>\n");
  fprintf(stdout, "\t-cl/-cluster      <(low, high) error cluster size>\n");
  fprintf(stdout, "\t-feature          <add features to pre-screening>\n");
  fprintf(stdout, "\t-align            <alignment(CENTER/CENROID)>\n");
  fprintf(stdout, "\t-split/-sp        <# splits for each page>\n");
  fprintf(stdout, "\t-pnum/-pn         number of pages\n");
  fprintf(stdout, "\t-dict             <dict type(OP/SE/CLASS/TREE/MIXED)\n");
  fprintf(stdout, "\t-lloyd            run LLOYD algorithm on CLASS/MIXED dict\n");
  fprintf(stdout, "\t-noupdate/-nu     turn OFF UPDATE_DICT\n"); 
  fprintf(stdout, "\t-lossy/-LOSSY     lossy mode\n");
  fprintf(stdout, "\t-pms/-PMS         PMS lossy mode\n");
  fprintf(stdout, "\t-res/-residue     turn ON residue coding\n");
  fprintf(stdout, "\t-silent           turn ON silent mode, print no report\n");
  fprintf(stdout, "\t-detail           turn ON detail mode, print detailed report\n");
  fprintf(stdout, "\t-help/-h          this menu\n");
  
  fprintf(stdout, "\n  Default values if not specified above\n");
  fprintf(stdout, "\tImage path:       ../data\n");
  fprintf(stdout, "\tConnectivity:     4\n");
  fprintf(stdout, "\tStrip size:       8\n");
  fprintf(stdout, "\tMismatch thres:   0.15\n");
  fprintf(stdout, "\tPattern matching: XOR(Hamming distance)\n");
  fprintf(stdout, "\tError cluster:    2,4\n");
  fprintf(stdout, "\tFeature screening:OFF\n");
  fprintf(stdout, "\tAlignment:        CENTER(geometric)\n");
  fprintf(stdout, "\tNumber of splits: 1\n");
  fprintf(stdout, "\tNumber of pages:  1\n");
  fprintf(stdout, "\tDictionary:       MIXED\n");
  fprintf(stdout, "\tLloyd mode:       OFF\n");
  fprintf(stdout, "\tUPDATE_DICT:      ON\n");
  fprintf(stdout, "\tLossy mode:       OFF\n");
  fprintf(stdout, "\tPMS mode:         OFF\n");
  fprintf(stdout, "\tResidue coding:   OFF\n");
  fprintf(stdout, "\tSilent mode:      OFF\n");
  fprintf(stdout, "\tDetail mode:      OFF\n");
  
  exit(0);
}

/* Subroutine:	void error()
   Function:	print error information and quit
   Input:	error information
   Output:	none
*/
void error(char *str)
{
  fprintf(stdout, str);
  exit(-1);
}

/* Subroutine:	void copy_mark_data()
   Function:	copy mark data
   Input:	source data buffer pointer and its size
   Output:	destination data buffer pointer
*/
char *copy_mark_data(char *src, int size)
{
  register int i;
  char *dest;
  
  dest = (char *)malloc(sizeof(char)*size);
  if(!dest) error("copy_mark_data: cannot allocate memory\n");
  
  for(i = 0; i < size; i++) dest[i] = src[i];
 
  return dest;
}

/* Subroutine:	copy_data_with_margin()
   Function:	copy data from source to destination, adding certain amount
   		of margins or cutting some off, decided by the values and signs 
		of lm, rm, tm and bm
   Input:	source bitmap, its size, and four margins
   Output:	destination bitmap
*/
void copy_data_with_margin(char *src, int w, int h, 
	int lm, int rm, int tm, int bm, char *dest) /* margin values */
{
  register int i, j;
  register char *sptr, *dptr;
  int dw, dh, cw, ch;
  
  dw = w+lm+rm; dh = h+tm+bm; 
  dptr = dest; sptr = src;
  
  /* if tm is non-negative, add white margin on top */
  /* or else, cut off some from top */
  if(tm >= 0) {
    for(i = 0; i < tm; i++)  
      for(j = 0; j < dw; j++) 
    	*dptr++ = 0;
  }
  else  sptr = src + (-tm)*w;
  
  /* for each line, if lm/rm non-negative, fill left/right margin with white */
  /* or if lm/rm negative, skip some left/right margin */
  ch = dh < h ? dh:h; cw = dw < w? dw:w;
  for(i = 0; i < ch; i++) {
    /* fill left margin with white or skip some left margin */
    if(lm >= 0) for(j = 0; j < lm; j++) *dptr++ = 0;
    else sptr += (-lm);
    
    /* copy data from source */
    for(j = 0; j < cw; j++) *dptr++ = *sptr++;

    /* fill right margin with white or skip some right margin */
    if(rm >= 0) for(j = 0; j < rm; j++) *dptr++ = 0;
    else sptr += (-rm);
  }

  /* if bm is non-negative, add white margin to bottom */
  /* or else, skip some bottom lines */
  if(bm >= 0) {
    for(i = 0; i < bm; i++)
      for(j = 0; j < dw; j++) 
        *dptr++ = 0;
  }
}

/* calculate -p*log(p)-(1-p)*log(1-p) */
float ent(float p)
{
  if(p == 0.0 || p == 1.0) return 0.;
  else return -(p*log(p)+(1.-p)*log(1.-p));
}

/* Subroutine:	void reset_codec_report()
   Function:	reset all the fields in the codec report
   Input:	none
   Output:	none
*/ 
void reset_codec_report(void)
{
  /* segment sizes */
  codec->report.direct_dict_size = 0;
  codec->report.refine_dict_size = 0;
  codec->report.text_region_size = 0;
  codec->report.gen_region_size = 0;
  codec->report.gen_ref_region_size = 0;
  
  /* detailed coding results */
  codec->report.index_bits = 0;
  codec->report.location_bits = 0;
  codec->report.size_bits = 0;
  codec->report.direct_bits = 0;
  codec->report.refine_bits = 0;
  codec->report.refine_offset_bits = 0;
  codec->report.export_bits = 0;
  codec->report.cleanup_bits = 0;
  codec->report.residue_bits = 0;
  codec->report.misc_bits = 0;
  codec->report.total_bits = 0;
  codec->report.flipped_pixels = 0;
  codec->report.uncoded_direct_bits = 0;
  codec->report.uncoded_refine_bits = 0;
}

/* Subroutine:	void print_compression_param()
   Function:	print out a series of encoding parameters used
   Input:	none
   Output:	none
*/
void print_compression_param(void)
{
  extern float match_two_marks_XOR(Mark *, Mark *);
  extern float match_two_marks_WXOR(Mark *, Mark *);
  extern float match_two_marks_WAN(Mark *, Mark *);
  extern float match_two_marks_ENT(Mark *, Mark *);
  extern float match_two_marks_CLUSTER(Mark *, Mark *);
    
  fprintf(stdout, "\n");

  fprintf(stdout, "\t\t\tInput Image Information\n\n");
  fprintf(stdout, "\tImage file header: \t\t%s\n", codec->report.file_header);
  fprintf(stdout, "\tTotal number of pages: \t\t%d\n", codec->page_num);
  fprintf(stdout, "\tNumber of stripes per page: \t%d\n", codec->split_num);
  fprintf(stdout, "\n\n");
  
  fprintf(stdout, "\t\t\tEncoding Parameters\n\n");
  if(codec->match_two_marks == match_two_marks_XOR)
    fprintf(stdout, "\tPattern matching method: \tXOR\n");
  else if(codec->match_two_marks == match_two_marks_WXOR)
    fprintf(stdout, "\tPattern matching method: \tWXOR\n");
  else if(codec->match_two_marks == match_two_marks_WAN)
    fprintf(stdout, "\tPattern matching method: \tWAN\n");
  else if(codec->match_two_marks == match_two_marks_ENT)
    fprintf(stdout, "\tPattern matching method: \tENT\n");
  else {
    fprintf(stdout, "\tPattern matching method: \tFAST\n");
    fprintf(stdout, "\tCluster size threshold: \t[%d,%d]\n", 
    	codec->lth, codec->hth);
  }
  
  fprintf(stdout, "\tMismatch threshold: \t\t%.3f\n", codec->mismatch_thres);
  fprintf(stdout, "\tStrip height: \t\t\t%d\n", codec->strip_height);
  fprintf(stdout, "\tConnectivity rule: \t\t%d\n", codec->connectivity);

  if(codec->pms) fprintf(stdout, "\tCoding mode: \t\t\tPM&S\n");
  else fprintf(stdout, "\tCoding mode: \t\t\tSPM\n");
  if(codec->lossy && !codec->residue_coding) 
    fprintf(stdout, "\tImage quality: \t\t\tLOSSY\n");
  else fprintf(stdout, "\tImage quality: \t\t\tLOSSLESS\n");
  if(!codec->pms) {
    fprintf(stdout, "\tSymbol dictionary: \t\t");
    switch(codec->dict_type) {
    case OP:
      fprintf(stdout, "One pass\n");  
      break;
    case SE:
      fprintf(stdout, "Singleton exclusion\n");  
      break;
    case CLASS:
      fprintf(stdout, "Class-based ");  
      if(codec->lloyd) fprintf(stdout, "w/ Lloyd\n");  
      else fprintf(stdout, "w/o Lloyd\n");
      break;
    case TREE:
      fprintf(stdout, "Tree-based\n");  
      break;
    case MIXED:
      fprintf(stdout, "Mixed ");  
      if(codec->lloyd) fprintf(stdout, "w/ Lloyd\n");  
      else fprintf(stdout, "w/o Lloyd\n");
      break;
    default:
      break;
    }
  }
  if(codec->page_num > 1) {
    if(codec->no_update) fprintf(stdout, "\tUpdate dictionary: \t\tNO\n");
    else fprintf(stdout, "\tUpdate dictionary: \t\tYES\n");
  }

  fprintf(stdout, "\n\n");
}

/* Subroutine:	void print_compression_report()
   Function:	print out # bytes used to transmit each type of segment
   Input:	page and part number
   Output:	none
*/
void print_compression_report(int page, int part)
{
  fprintf(stdout, "\t\t\tPartial Coding Results\n\n");
  
  fprintf(stdout, "\tDocument page #%d: \t\t%dx%d raw\n",
  	page+1, codec->doc_width, codec->doc_height);
  fprintf(stdout, "\tThis is part #%d: \t\tlines %d--%d\n",
        part+1, doc_buffer->top_y, doc_buffer->top_y+doc_buffer->height-1);
  fprintf(stdout, "\tTotal marks extracted: \t\t%d\n",
    	codec->report.total_marks+codec->report.speck_marks);
  fprintf(stdout, "\tSpecks in cleanup page: \t%d\n",
  	codec->report.speck_marks);
  fprintf(stdout, "\tMarks in cleanup page: \t\t%d\n",
  	codec->report.marks_in_cleanup);
  fprintf(stdout, "\tMarks in text region: \t\t%d\n",
  	codec->report.marks_in_strips);
  fprintf(stdout, "\tMarks embedded coded: \t\t%d\n",
  	codec->report.embedded_marks);  
  fprintf(stdout, "\n\n");
  
  fprintf(stdout, "\tDirect dictionary: \t\t%d\n",
        codec->report.direct_dict_size);
  fprintf(stdout, "\tRefinement dictionary: \t\t%d\n",
    	codec->report.refine_dict_size);
  fprintf(stdout, "\tText region: \t\t\t%d\n",
  	codec->report.text_region_size);
  fprintf(stdout, "\tGeneric region: \t\t%d\n",
  	codec->report.gen_region_size);
  fprintf(stdout, "\tGeneric refinement region: \t%d\n",
  	codec->report.gen_ref_region_size);
  
  fprintf(stdout, "\n\n");
}

/* Subroutine:  void print_overall_compression_report()
   Function:	print the overall code rate for coding the entire document
   Input:	none
   Output:	none
*/
void print_overall_compression_report()
{
  fprintf(stdout, "\tIN TOTAL %d BYTES WERE SENT FOR THIS IMAGE\n\n", 
  	(int)ftell(codec->fp_out));
}

/* Subroutine:	void print_detailed_compression_report()
   Function:	print a detailed report of encoding a page/part of a page,
   		including the compression ratios for each type of data
   Input:	page number and part number
   Output:	none
*/
void print_detailed_compression_report(int page, int part)
{
  fprintf(stdout, "\t\t\tDetailed Bit Allocation\n\n");
  
  fprintf(stdout, "\tIndex coding: \t\t\t%d bits\n", 
    codec->report.index_bits);
  codec->report.total_bits = codec->report.index_bits;
  fprintf(stdout, "\tLocation coding: \t\t%d bits\n", 
    codec->report.location_bits);
  codec->report.total_bits += codec->report.location_bits;    
  fprintf(stdout, "\tSize coding: \t\t\t%d bits\n", 
    codec->report.size_bits);
  codec->report.total_bits += codec->report.size_bits;
  fprintf(stdout, "\tDirect bitmap coding: \t\t%d bits\n", 
    codec->report.direct_bits);
  codec->report.total_bits += codec->report.direct_bits;
  fprintf(stdout, "\tRefinement bitmap coding: \t%d bits\n", 
    codec->report.refine_bits);
  codec->report.total_bits += codec->report.refine_bits;
  fprintf(stdout, "\tRefinement offset coding: \t%d bits\n",
    codec->report.refine_offset_bits);
  codec->report.total_bits += codec->report.refine_offset_bits;
  fprintf(stdout, "\tExport flag coding: \t\t%d bits\n",
    codec->report.export_bits);
  codec->report.total_bits += codec->report.export_bits;
  fprintf(stdout, "\tCleanup coding: \t\t%d bits\n", 
    codec->report.cleanup_bits);
  codec->report.total_bits += codec->report.cleanup_bits;
  fprintf(stdout, "\tResidue coding: \t\t%d bits\n", 
    codec->report.residue_bits);
  codec->report.total_bits += codec->report.residue_bits;
  fprintf(stdout, "\tOther information coding: \t%d bits\n\n",
    codec->report.misc_bits);  
  codec->report.total_bits += codec->report.misc_bits;
    
  codec->report.overall_bits += codec->report.total_bits;
  
  fprintf(stdout, "\t\t\tMarks in Dictionaries\n\n");
  fprintf(stdout, "\tTotal number of dictionary marks: \t%d\n",
  	codec->report.total_dict_mark);
  fprintf(stdout, "\tNumber of direct dictionary marks: \t%d\n",
  	codec->report.direct_dict_mark);
  fprintf(stdout, "\tNumber of refinement dictionary marks: \t%d\n",
  	codec->report.refine_dict_mark);
  fprintf(stdout, "\n\n");
  
  fprintf(stdout, "\t\t\tUncoded Sizes and Compression Ratios\n\n");
  fprintf(stdout, "\tDirect bitmap: \t\t%d(CR = %.2f)\n", 
  	codec->report.uncoded_direct_bits,
  	(float)codec->report.uncoded_direct_bits/(float)codec->report.direct_bits
	);
  fprintf(stdout, "\tRefinement bitmap: \t%d(CR = %.2f)\n", 
  	codec->report.uncoded_refine_bits, 
	(float)codec->report.uncoded_refine_bits/(float)codec->report.refine_bits
	);
  fprintf(stdout, "\tCleanup bitmap: \t%d(CR = %.2f)\n", 
  	codec->report.uncoded_cleanup_bits, 
	(float)codec->report.uncoded_cleanup_bits/(float)codec->report.cleanup_bits
	);
  fprintf(stdout, "\tResidue bitmap: \t%d(CR = %.2f)\n", 
  	codec->report.uncoded_residue_bits, 
	(float)codec->report.uncoded_residue_bits/(float)codec->report.residue_bits
	);
  fprintf(stdout, "\tTotal bitmap: \t\t%d(CR = %.2f)\n", 
    	codec->report.total_uncoded_bits, 
  	(float)codec->report.total_uncoded_bits/(float)codec->report.total_bits
	);

  fprintf(stdout, "\n");
}

/* Subroutine:	int twos_complement()
   Function:	calculate the input value's twos complement
   Input:	the value and its length in bits (including 1 bit for sign) 
   Output:	two's complement
*/
int twos_complement(int value, int len)
{
  int comp, mask;
  
  if(value < 0) {
    comp = -value;
    mask = (1 << len) - 1;
    comp ^= mask;
    comp++;
    comp |= 0x01 << (len-1);
  }
  else comp = value;

  return comp;
}
