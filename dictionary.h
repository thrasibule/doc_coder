
/* Dictionary definitions internal only to dictionary.c */

#define DIRECT_DICT		0
#define REFINE_DICT		1

#define MAX_DICT_ENTRY		MAX_MARK_NUM

typedef struct {
	int height;
	int entry_num;
} HeightClassInfo;

typedef struct {
	int index, ref_index;/* index information		*/
	float mm;	     /* mismatch between new symbol and its reference */
	int singleton;	     /* if this mark is a singleton	*/
	int perfect;         /* if this mark is perfect after shape unifying */
	Mark *mark;	     /* MarkList entry pointer	*/
	Mark *ref_mark;	     /* MarkList ref entry pointer 	*/
	int rdx, rdy;
} DictionaryEntry;

typedef struct {
	int total_mark_num;	/* total dictionary size	*/
	int direct_mark_num;	/* direct dictionary size	*/
	int refine_mark_num;	/* refinement dictionary size	*/
	/* necessary height class info in direct dict	*/
	DictionaryEntry entries[MAX_DICT_ENTRY];
} Dictionary;
