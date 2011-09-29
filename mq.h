
/*
 * Status description of an arithmetic encoder
 */

typedef unsigned char ARENC_BUFFER;

typedef struct arenc_state {
  ARENC_BUFFER *st;    /* probability status for contexts, MSB = MPS */

  unsigned long c;                /* C register, base of coding intervall, *
                                   * layout as in Table 23                 */
  unsigned long a;      /* A register, normalized size of coding intervall */
  int ct;  /* bit shift counter, determines when next byte will be written */
  int buffer;                /* buffer for most recent output byte != 0xff */

  int first_byte;
  int last_byte;
  
  int nff;
  int n7f;
} ARENC_STATE;

#define INT_ARITH_CTX_BITS	9
#define BITMAP_ARITH_CTX_BITS	10

