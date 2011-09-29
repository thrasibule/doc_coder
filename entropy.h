
#define INF     0x7fffffff
#define OOB     0x7fffffff      /* Out-Of-Band value    */

#define SIGNED		1
#define UNSIGNED	0

typedef struct {
	unsigned short 	prefix;
	int 		prelen;
	int 		rangelen;
} CODE_TABLE;

typedef struct {
	int 		start, end;
} RANGE_TABLE;

typedef struct {
	int 		size;
	CODE_TABLE  	ctab[21];
	RANGE_TABLE 	rtab[21];
	unsigned short 	oobcode;
	int 		ooblen;
} HUFF_TABLE;

typedef struct {
	char *data;		/* coded bitstream data */
	int uncoded_size; 	/* raw bitmap size if uncoded */
	int coded_size;		/* coded bitstream size */
	int max_buffer_size;	/* pre-allocated data_buffer_size */ 
} ARITH_CODED_BITSTREAM;

#define MAX_CODED_DATA_SIZE	(1<<18)	/* 256K */

typedef struct {
	char data[MAX_CODED_DATA_SIZE];
	int size;
} CODED_SEGMENT_DATA;

#define NUM_INTARITH_CODERS	14

#define TOTAL_CONTEXT_BITS	20	/* 4  for distiguishing among coders
					  +16 for each coder */


enum {IAAI, IADH, IADS, IADT, IADW, IAEX, IAFS, IAID, IAIT, IARDH, IARDW, IARDX, IARDY, IARI};

enum {DIRECT=14, REFINE};

#define MAX_SYMBOL_ID_LEN	16

