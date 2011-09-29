
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* definitions of useful constants		*/
#define TRUE		1
#define FALSE		0

/* pattern matching method definitions */
#define XOR             0
#define WXOR            1
#define WAN             2
#define ENT		3

/* dictionary construction method */
#define SE		0
#define CLASS		1
#define TREE		2
#define MIXED		3
#define OP		4
#define PMS_DICT	5

/* alignment used for matching marks */
#define CENTER		0
#define CENTROID	1

/**********************JBIG2 segment definitions**************************/

/* define combination operators */
enum {JB2_OR, JB2_AND, JB2_XOR, JB2_XNOR, JB2_REPLACE};

/* define segment types */
#define SYM_DICT		0
#define INTER_SYM_REG		4
#define IM_SYM_REG		6
#define IM_LL_SYM_REG		7
#define HALF_DICT		16
#define INTER_HALF_REG		20
#define IM_HALF_REG		22
#define IM_LL_HALF_REG		23
#define INTER_GEN_REG		36
#define IM_GEN_REG		38
#define IM_LL_GEN_REG		39
#define INTER_GEN_REF_REG	40
#define IM_GEN_REF_REG		42
#define IM_LL_GEN_REF_REG	43
#define PAGE_INFO		48
#define END_OF_PAGE		49
#define END_OF_STRIPE		50
#define END_OF_FILE		51
#define SUPPORTED_PROF		52
#define TABLES			53
#define EXTENSION		62

#define MAX_SEG_NUM		50
#define MAX_REF_SEG_NUM		20

/* define reference corners */
enum {BOTTOMLEFT, TOPLEFT, BOTTOMRIGHT, TOPRIGHT};

typedef struct {
  	int type;			   /* segment type */
	int retain_ref[MAX_REF_SEG_NUM];   /* retain referenced segments? */ 
	int retain_this; 		   /* retain this segment? */
	int ref_seg_count;		   /* # referenced segments */
	unsigned int ref_seg[MAX_REF_SEG_NUM];  /* referenced segments */
	unsigned int page_asso;	 	   /* page association */
	int seg_length;			   /* segment data length */
} SegHeader;

typedef struct {
 	int width, height;
	int locx, locy;
	int excombop;
} RegionInfoField;

typedef struct {
	RegionInfoField reg_info;
  	int tpdon;		/* typical prediction on? */
	int rtemplate;		/* template used? 0-3 	*/
	int mmr; 		/* MMR coding used?	*/
  	char atx[4];		/* adaptive template pixel X coord */
	char aty[4];		/* adaptive template pixel Y coord */
} GenRegionDataHeader;

typedef struct {
	RegionInfoField	reg_info;
	int tpdon;		/* typical prediction on? */
	int rtemplate;		/* template used? 0-1 	*/ 
	char atx[2];		/* adaptive template pixel X coord */
	char aty[2];		/* adaptive template pixel Y coord */
} GenRefRegionDataHeader;

typedef struct {	
	RegionInfoField	reg_info;
	int huff;		/* Huffman coding */
	int refine;		/* embedded coding */
	int logsbstrips;	/* log(strip_size) */
	int refcorner;		/* reference cornder */
	int transposed;		/* transposed */
	int combop;		/* combination operator */
	int def_pixel;		/* default pixel */
	int dsoffset;		/* common DS offset */
	int rtemplate;		/* refinement coding template */
	
	int fs_tbl;		/* FS Huffman table */
	int ds_tbl;		/* DS Huffman table */
	int dt_tbl;		/* DT Huffman table */
	int rdw_tbl;		/* RDW Huffman table */
	int rdh_tbl;		/* RDH Huffman table */
	int rdx_tbl;		/* RDX Huffman table */
	int rdy_tbl;		/* RDY Huffman table */
	int rsize_tbl;		/* RSIZE Huffman table */
	
	char atx[2];		/* adaptive template pixel X coord */
	char aty[2];		/* adaptive template pixel Y coord */
	
	int numinstances;	/* total number of symbol instances */
} SymRegionDataHeader;

typedef struct {
	int huff;		/* Huffman coding */
	int refagg;		/* refinment/aggregate coding */
	int dh_tbl;		/* DH Huffman table */
	int dw_tbl;		/* DW Huffman table */
	int bmsize_tbl;		/* BMSIZE Huffman table */
	int agginst_tbl;	/* AGGINST Huffman table */
	int ctx_used;		/* bitmap coding context used */
	int ctx_retained;	/* bitmap coding context retained */
	int dtemplate;		/* direct coding template */
	int rtemplate;		/* refinement coding template */
	
	char atx[4];		/* adaptive template pixel X coord */
	char aty[4];		/* adaptive template pixel Y coord */	
	char ratx[2];		/* adaptive template pixel X coord */
	char raty[2];		/* adaptive template pixel Y coord */
	

	int numexsyms;		/* total # exported symbols */
	int numnewsyms;		/* total # new symbols defined */
} SymDictDataHeader;
 
/************************Mark information definitions***********************/
#define MAX_MARK_NUM		6000	/* max # extracted marks */
#define MAX_WORD_NUM		1000	/* max # extracted words */

typedef struct {
	int x, y;		// coordinates of a pixel (x, y)
} PixelCoord;

typedef struct {
	unsigned char *bitstream;	// data pointer to the bit stream
	unsigned char mask;	// mask used when reading out value
	int byte_posi;		// the current byte being read 
	int bit_posi_in_byte;	// position of the bit to be read 
	int total_bytes;	// file size in bytes
} BitMap;

typedef struct {
	char *data;		// data stream in the buffer
	int width, height;	// size of the data map 
	int top_y;
} PixelMap;

typedef struct {
	char *data;
	PixelCoord upleft, ref, c;
	int width, height; 
	int hole_num;
	int in_dict;
	int dict_entry;
} Mark;

typedef struct {
	Mark marks[MAX_MARK_NUM];
	int mark_num;
} MarkList;

#define LONGEST_WORD 50

typedef struct {
	int letter_num;
	int letters[LONGEST_WORD];
	Mark *bitmap;
} Word;

typedef struct {
 	Word words[MAX_WORD_NUM];
	int word_num;
} WordList;

/************************Codec structure definition*************************/
#define FOUR_CONNECT		4
#define EIGHT_CONNECT		8

#define DEFAULT_PATH		"../data/"
#define DEFAULT_STRIP_HEIGHT	8
#define DEFAULT_DOC_WIDTH	400
#define DEFAULT_DOC_HEIGHT	400
#define DEFAULT_CONNECTIVITY	FOUR_CONNECT
#define DEFAULT_MISMATCH_THRES	0.15
#define DEFAULT_LOW_THRES	2
#define DEFAULT_HIGH_THRES	4

typedef struct {
	char file_header[100];
	char data_path[100];

	int total_marks;	/* total # marks extracted */
	int speck_marks;	/* # specks in page */
	int marks_in_cleanup;	/* # marks put back into cleanup coding */
	int marks_in_strips;	/* # marks in coding strips */
	int embedded_marks;	/* # marks embedded coded */
	
	int direct_dict_size;   /* size of direct dict segment */
	int refine_dict_size;	/* size of refine dict segment */
	int text_region_size;	/* size of text region segment */
	int gen_region_size;	/* size of generic region segment */
	int gen_ref_region_size;/* size of generic refinement region segment */
       
	int total_dict_mark;    /* total # dictionary marks */
        int direct_dict_mark;   /* # directly coded marks */
        int refine_dict_mark;   /* # refinement coded marks */

        int index_bits;         /* # bits spent on index coding */
        int location_bits;      /* # bits spent on location coding */
        int size_bits;          /* # bits spent on size coding */
        int direct_bits;        /* # bits spent on direct bitmap coding */
        int refine_bits;        /* # bits spent on refinement bitmap coding */
        int refine_offset_bits; /* # bits spent on refinement bitmap offset */
        int export_bits;	/* # bits spent on coding export flags */
	int cleanup_bits;       /* # bits spent on cleanup coding */
        int residue_bits;       /* # bits spent on residue coding */
        int misc_bits;          /* # bits spent on everything else */
        int total_bits;         /* total # bits written for this buffer */

        int overall_bits;	/* overall # bits sent up to now */
	
        #ifdef OPT_HUFF
        char seg_x_tbl[10];
        char seg_y_tbl[10];
        char symbol_x_tbl[10];
        char h_tbl[10];
        char w_tbl[10];
        #endif

        int uncoded_direct_bits; /* # bits for uncoded direct bitmaps */
        int uncoded_refine_bits; /* # bits for uncoded refined bitmaps */
        int uncoded_cleanup_bits;
        int uncoded_residue_bits;
        int total_uncoded_bits;

        int flipped_pixels;
} CodecReport;

typedef struct {
	FILE *fp_in, *fp_out;		/* file handles */
	int doc_width, doc_height;	/* document page size */
	float mismatch_thres;		/* mismatch threshold */
	int lth, hth;			/* low/high error cluster threshold */
	int connectivity;		/* 4 or 8 connectivity rule? */
	int strip_height;		/* coding strip height */
	int buffer_height;		/* buffer height */
	int cleanup_coding;		/* need cleanup coding? */
	
	int page_num;			/* # pages in document */
	int no_update;			/* update dict from page to page? */
	int dict_type;			/* dictionary construction method */
	int lloyd;			/* run Lloyd for CLASS/MIXED dict? */
	int lossy;			/* lossy compression? */
	int pms;			/* use PM&S mode? */
	int split_num;			/* # splits per page */
	int silent;			/* print coding results? */
	int detail;			/* print detailed results? */
	int nearlossless;		/* near-lossless coding mode */
	int residue_coding;		/* force residue coding? */
	int align;			/* alignment by center or centroid? */
	
	int (*prescreen_two_marks)(Mark *, Mark *);
	float (*match_two_marks)(Mark *, Mark *);
  
	int cur_seg;
	int cur_page;
	int cur_split;
	
	CodecReport report;
} Codec;

/*******************Page structure definitions******************************/

#define MAX_PAGE_NUM 15

typedef struct {
 	WordList *all_words;
	MarkList *all_marks;
	Mark all_word_marks[MAX_WORD_NUM];
} Page;

/*******************Dictionary segment definitions**************************/
#define MAX_HEIGHT_CLASS_ENTRY	300	/* buffer size of each height class  */
#define MAX_HEIGHT_CLASS	400	/* buffer size of dictionary	 */

typedef struct {
	int index;		/* index 		*/
	Mark *mark;		/* mark info pointer 	*/
	int ref_index;		/* reference index 	*/
        Mark *ref_mark;         /* ref mark pointer     */
        int rdx, rdy;
} HeightClassEntry;

typedef struct {
	int cur_entry_num;	/* counter of current entry number	 */ 
	int height;		/* height value pertaining to this class */
	HeightClassEntry entries[MAX_HEIGHT_CLASS_ENTRY];
} HeightClass;

typedef struct {
	int total_mark_num;
	int height_class_num;
	HeightClass height_classes[MAX_HEIGHT_CLASS];
} DirectDict;

typedef struct {
        int total_mark_num;
        int height_class_num;
        HeightClass height_classes[MAX_HEIGHT_CLASS];
} RefineDict;

/**************************Region segment definitions************************/
typedef struct {
	int flag;		/* 1 if embedded bitmap follows */
	Mark *mark;		/* MarkList pointer 		*/
	Mark *ref_mark;		/* reference MarkList pointer 	*/
        int rdx, rdy;           /* location offset w.r.t. reference */
} EmbedInfo;

typedef struct {
	int dict_index;		/* dictionary entry this mark indexes into */
	int list_entry;		/* MarkList Entry */
	EmbedInfo embedded;
} MarkRegInfo;

typedef struct {
	int num_marks;		/* total # marks in this coding strip 	  */
	int strip_x_posi;	/* x position of the first mark		  */
	int strip_top_y;	/* topline position of the strip	  */ 
	MarkRegInfo *marks;
} CodingStrip;
