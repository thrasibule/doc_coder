typedef struct {
  Mark *marks[MAX_MARK_NUM];
  int mark_num;
} MarkPointerList;

#define BIGGEST_SUPERCLASS 	 1000

#define MAX_MATCH_PER_MARK 50

typedef struct {
  int match_num;
  MatchInfo match_info[MAX_MATCH_PER_MARK];
} MarkMatchBuf;

typedef struct {
  int ref_scl;
  int ref_mark;
  float mm_score;
} SuperclassMatchInfo;

typedef struct {
  int total_entry_num;
  int entries[BIGGEST_SUPERCLASS];
  int leader;
} Superclass;

typedef struct {
  Superclass *scl;
  int scl_num;
} SuperclassInfo;

#define TOO_BIG 20000.00

#define LONGEST_CHAIN 500

typedef struct {
  int chain[LONGEST_CHAIN];
  int chain_size; 
  int loop_start, loop_end;
  int break_point;
} Chain;

#define MAX_CHILD_NUM 100

typedef struct {
  int parent;
  int child_num;
  int child[MAX_CHILD_NUM];
  float mm_score;   /* mismatch with its parent */
} MarkMatchTree;


