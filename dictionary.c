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
#include "opt_dict.h"
#include "entropy.h"
#include "mq.h"
#include <math.h>

Dictionary	 	*dictionary;
MarkMatchTree 		*mark_match_tree;
static DirectDict	*direct_dict;
static RefineDict	*refine_dict;
int			symbol_ID_in_bits;
int			prev_dict_size;

void init_dictionary(void);
void free_dictionary(void);
void make_mark_dictionary(void);
void make_mark_dictionary_pms(void);
void update_mark_dictionary(void);
void exclude_singletons(void);
void delete_perfect_dict_marks(void);
void add_mark_to_dictionary(int, int, float);
void modify_dictionary_marks(void);
void tree_to_dictionary(void);
void encode_dictionary(void);
void split_dictionary(void);
void modify_dict_indices(void);
void encode_direct_dict(void);
void encode_refine_dict(void);

extern void modify_refine_mark(Mark *, Mark *, int *, int *, int *);
extern void match_mark_with_dict(Mark *, int *, float *);
extern void error(char *);

extern void bin_encode_refine(char *, int, int, char *, int, int, int, int, 
	ARITH_CODED_BITSTREAM *);
extern void bin_encode_direct(char *, int, int, ARITH_CODED_BITSTREAM *);
extern void int_encode(int, int, ARITH_CODED_BITSTREAM *);
extern void symID_encode(int, ARITH_CODED_BITSTREAM *);
extern void reset_arith_int_coders(void);
extern void reset_arith_bitmap_coders(void);
extern void arith_encode_init(void);
extern void arith_encode_flush(ARITH_CODED_BITSTREAM *);  

extern int  write_coded_bitstream(char *, int);
extern void buffer_coded_bitstream(ARITH_CODED_BITSTREAM *,
			  	   CODED_SEGMENT_DATA *);
				   
extern Codec *codec;
extern MarkList *all_marks;
extern MarkPointerList *mark_pointer_list;

/* Subroutine: 	void init_dictionary()
   Function:	initilize the dictionary
   Input:	none
   Output:	none
*/
void init_dictionary()
{
  dictionary = (Dictionary *)malloc(sizeof(Dictionary)*1);
  if(!dictionary) error("Cannot allocate memory for dictionary!\n");
    
  dictionary->total_mark_num = 0;
}

/* Subroutine:	void free_dictionary()
   Function:	free the dictionary memory
   Input:	none
   Output:	none
*/
void free_dictionary()
{
  free((void *)dictionary);
}

/* Subroutine:	void make_mark_dictionary()
   Function:	match the extracted marks and decide dictionary
   Input:	none
   Output:	none
*/
void make_mark_dictionary()
{
  register int i;
  int ref_index;
  float mm;
  Mark *mark;

  prev_dict_size = dictionary->total_mark_num;
//  #ifdef DEBUG
  printf("prev_dict_size = %d\n", prev_dict_size);
//  #endif
  for(i = 0; i < prev_dict_size; i++) 
    dictionary->entries[i].singleton = TRUE; 
  
  mark = all_marks->marks;
  for(i = 0; i < all_marks->mark_num; i++, mark++) {
    match_mark_with_dict(mark, &ref_index, &mm);
    add_mark_to_dictionary(i, ref_index, mm);
  }

  if(codec->dict_type == OP) /* pretend there's no singletons */ {
    for(i = 0; i < dictionary->total_mark_num; i++)
      dictionary->entries[i].singleton = FALSE;
  }
  
  if(codec->lossy)  modify_dictionary_marks();
  delete_perfect_dict_marks();  

  exclude_singletons();
}

/* Subroutine:	void make_mark_dictionary_pms()
   Function: 	form the dictionary in PM&S mode
   Input:	none
   Output:	none
*/
void make_mark_dictionary_pms()
{
  register int i;
  int ref_index;
  float mm;
  Mark *mark;

  prev_dict_size = dictionary->total_mark_num;
  #ifdef DEBUG
  printf("prev_dict_size = %d\n", prev_dict_size);
  #endif
  for(i = 0; i < prev_dict_size; i++) 
    dictionary->entries[i].singleton = TRUE; 
  
  mark = all_marks->marks;
  for(i = 0; i < all_marks->mark_num; i++, mark++) {
    match_mark_with_dict(mark, &ref_index, &mm);
    if(ref_index == -1) add_mark_to_dictionary(i, ref_index, mm);
  }

  for(i = 0; i < dictionary->total_mark_num; i++)
    dictionary->entries[i].singleton = FALSE;
}

/* Subroutine:	void add_mark_to_dictionary()
   Function:	add new mark into (temporary) dictionary 
   Input:	new mark and its matching index
   Output:	none
*/
void add_mark_to_dictionary(int mark_index, int ref_index, float mm)
{
  DictionaryEntry *cur_entry;
  int new_index;
  
  new_index = dictionary->total_mark_num;
  cur_entry = dictionary->entries + new_index;
  cur_entry->index = new_index;
  cur_entry->ref_index = ref_index;
  cur_entry->mm = mm;
  cur_entry->singleton = TRUE;		/* not yet referenced by anyone else */
  cur_entry->mark = all_marks->marks + mark_index;
  if(ref_index != -1) {
    cur_entry->ref_mark = dictionary->entries[ref_index].mark;
    dictionary->entries[ref_index].singleton = FALSE;
  }
  else cur_entry->ref_mark = NULL;
  if(mm == 0.) cur_entry->perfect = TRUE;
  else cur_entry->perfect = FALSE;
  cur_entry->rdx = cur_entry->rdy = 0;
  
  dictionary->total_mark_num++;
}

void delete_singletons(int *, int);

/* Subroutine:	exclude_singletons()
   Function:	examine the new dictionary entries and remove all the 
   		singletons 
   Input:	none
   Output:	none
*/
void exclude_singletons()
{
  register int i;
  int total_mark_num;
  int single_tbl[MAX_DICT_ENTRY], single_num;
  int new_index_tbl[MAX_DICT_ENTRY], counter;
  DictionaryEntry *entry;
  
  total_mark_num = dictionary->total_mark_num;
  single_num = 0; counter = prev_dict_size;
  for(i = prev_dict_size; i < total_mark_num; i++) {
    if(dictionary->entries[i].singleton) {
      new_index_tbl[i] = -1;
      single_tbl[single_num++] = i;
    }
    else new_index_tbl[i] = counter++;
  }
 
  delete_singletons(single_tbl, single_num);

  /* change the dict indices after singletons are removed */
  entry = dictionary->entries + prev_dict_size;
  for(i = prev_dict_size; i < dictionary->total_mark_num; i++, entry++) {
    entry->index = new_index_tbl[entry->index];
    if(entry->index == -1)
      error("exclude_singletons: illogical error 1, check your code!\n");
    
    if(entry->ref_index != -1) {
      if(entry->ref_index >= prev_dict_size) 
        entry->ref_index = new_index_tbl[entry->ref_index];
      if(entry->ref_index == -1)
        error("exclude_singletons: illogical error 2, check your code!\n");    
    } 
  }
}

/* Subroutine:	delete_singletons()
   Function:	remove all the singletons from dictionary
   Input:	the index table and number of singletons 
   Output:	none
*/
void delete_singletons(int *single_tbl, int single_num)
{
  register int i, j;
  
  for(i = 0; i < single_num-1; i++) {
    for(j = single_tbl[i]+1; j < single_tbl[i+1]; j++) 
      dictionary->entries[j-i-1] = dictionary->entries[j];
  }
  
  for(j = single_tbl[single_num-1]+1; j < dictionary->total_mark_num; j++) 
    dictionary->entries[j-single_num] = dictionary->entries[j];
  
  dictionary->total_mark_num -= single_num;
}

void change_to_new_reference(int old, int new);

/* Subroutine:  delete_perfect_dict_marks()
   Function:    delete perfect symbols from the dictionary. This is done 
   		by treating them as singletons which will be removed later on
   Input:       none
   Output:      none
*/
void delete_perfect_dict_marks()
{
  register int i;
  int total_mark_num;
  DictionaryEntry *entry;
  int ref_index;

  total_mark_num = dictionary->total_mark_num;
  entry = dictionary->entries + prev_dict_size;
  for(i = prev_dict_size; i < total_mark_num; i++, entry++) {
    if(entry->perfect && !entry->singleton) {
      ref_index = entry->ref_index;
      while(dictionary->entries[ref_index].perfect)
        ref_index = dictionary->entries[ref_index].ref_index;
      if(ref_index == -1)
        error("delete_perfect_dict_marks: illogical error, check code\n");
      entry->ref_index = ref_index;
      entry->singleton = TRUE;
      change_to_new_reference(i, ref_index);
    }
  }
}

/* Subroutine:	void change_to_new_reference()
   Function:	scan all dictionary entries, if ref_index == old, change it
   		to new. This is a lower-level call of delete_perfect_dict_marks
   Input:	old and new ref_index
   Output:	none 
*/
void change_to_new_reference(int old, int new)
{
  register int i;
  int total_mark_num;
  DictionaryEntry *entry;

  total_mark_num = dictionary->total_mark_num;
  entry = dictionary->entries + prev_dict_size;
  for(i = prev_dict_size; i < total_mark_num; i++, entry++)
    if(entry->ref_index == old) entry->ref_index = new;
}

void copy_tree_into_dict(int);
static int *swap_tbl;

/* Subroutine:	void tree_to_dictionary()
   Function:	convert mark_match_tree into new dictionary. We make sure a 
   		symbol comes after its reference by copying each tree in the
		descending order
   Input:	none
   Output:	none
*/
void tree_to_dictionary()
{
  register int i;
  int *roots, root_num;
  
  swap_tbl = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!swap_tbl) 
    error("tree_to_dictionary: cannot allocate memory\n");

  for(i = 0; i < prev_dict_size; i++) swap_tbl[i] = i;
    
  roots = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!roots) error("tree_to_dictionary: cannot allocate memory\n");
  for(i = 0, root_num = 0; i < mark_pointer_list->mark_num; i++) 
  {
    if(mark_match_tree[i].parent == -1) 
      roots[root_num++] = i;
  }

  /* for each tree, traverse it depth first, copying each node into
     the new dictionary along the way */
  for(i = 0; i < root_num; i++) 
  {
    copy_tree_into_dict(roots[i]);
  }

  free((void *)roots);
  free((void *)swap_tbl);
  
  free((void *)mark_pointer_list);
}

/* Subroutine:	void copy_tree_into_dict()
   Function:	copy the tree into the dictionary starting from "node", 
   		depth first 
   Input:	starting node
   Output:	none
*/
void copy_tree_into_dict(int node)
{
  register int i;
  DictionaryEntry *entry;
  MarkMatchTree *tnode;
  int index;

  if(dictionary->total_mark_num == MAX_DICT_ENTRY)
    error("copy_tree_into_dict: dictionary entry buffer is full\n");
    
  tnode = mark_match_tree + node;
  
  /* if this node represents a new mark from the current page, 
     copy itself into the dictionary */
  if(node >= prev_dict_size) {
    index = dictionary->total_mark_num;
    entry = dictionary->entries + index;
  
    entry->index = index;
    entry->mark = mark_pointer_list->marks[node];
    if(tnode->parent == -1) {
      entry->ref_index = -1;
      entry->ref_mark = NULL;
    }
    else { 
      entry->ref_index = swap_tbl[tnode->parent];
      entry->ref_mark = mark_pointer_list->marks[tnode->parent];
    }
  
    entry->mm = tnode->mm_score;
  
    if(tnode->mm_score == 0.) entry->perfect = TRUE;
    else entry->perfect = FALSE;
 
    entry->singleton = FALSE;
  
    swap_tbl[node] = index;
    dictionary->total_mark_num++;
  }
  
  /* copy this node's children */
  if(tnode->child_num) {
    for(i = 0; i < tnode->child_num; i++)
      copy_tree_into_dict(tnode->child[i]);
  }
}

/* Subroutine:	void update_mark_dictionary()
   Function:	for the new page, update the mark dictionary by throwing out
   		the unused dictionary entries
   Input:	void
   Output:	void
*/
void update_mark_dictionary()
{
  register int i, j;
  DictionaryEntry *entry;
  int ent_tbl[MAX_MARK_NUM], ind_tbl[MAX_MARK_NUM], work[MAX_MARK_NUM];
  int total_mark_num, single_num, count;
  
  extern void sort_empty_marks();
  
  total_mark_num = dictionary->total_mark_num;
  
  /* examine the old part of the symbol dictionary and decide which entries
     are not used any more, delete such entries */
  single_num = 0; entry = dictionary->entries;
  for(i = 0; i < prev_dict_size; i++, entry++) 
    if(entry->singleton) {
      ent_tbl[single_num] = i;	/* ent_tbl stores the entry numbers */
      ind_tbl[single_num] = entry->index; /* ind_tbl stores the index numbers */
      single_num++;
      
      /* free memory of the unexported dictionary mark bitmaps  */
      free((void *)entry->mark->data);  
    }
  
  if(single_num > 0) {
    delete_singletons(ent_tbl, single_num);
  
    /* after all unexported marks (singletons) are deleted, the "index" 
       field and the "ref_index" field in each entry needs to be re-examined 
       to ensure consecutive values. For this purpose, we store in the array 
       "ind_tbl" and "ent_tbl" to store the number that needs to be subtracted
       from "index" and "ref_index", respectively. */
  
    /* 1. the "ind_tbl" for "index" field */
    memcpy(work, ind_tbl, single_num*sizeof(int));
    sort_empty_marks(work, single_num);
    memset(ind_tbl, 0, work[0]*sizeof(int));
    for(i = 0, count = 1; i < single_num-1; i++, count++)
      for(j = work[i]; j < work[i+1]; j++) 
        ind_tbl[j] = count;
    for(j = work[single_num-1]; j < total_mark_num; j++) 
      ind_tbl[j] = count;

    /* 2. the "ent_tbl" for "ref_index" field */
    memcpy(work, ent_tbl, single_num*sizeof(int));
    memset(ent_tbl, 0, work[0]*sizeof(int));
    for(i = 0, count = 1; i < single_num-1; i++, count++)
      for(j = work[i]; j < work[i+1]; j++) 
        ent_tbl[j] = count;
    for(j = work[single_num-1]; j < total_mark_num; j++) 
      ent_tbl[j] = count;

    entry = dictionary->entries;
    for(i = 0; i < total_mark_num; i++, entry++) {
      entry->index -= ind_tbl[entry->index];
      if(entry->ref_index != -1) 
        entry->ref_index -= ent_tbl[entry->ref_index];
    }
  }
}

/* Subroutine: 	void modify_dictionary_marks()
   Function:	in lossy compression case, refinement marks are shape-unified 
   Input:	none
   Output:	none
*/
void modify_dictionary_marks()
{
  register int i, j;
  int total_mark_num;
  int total_refine_dict_mark, perfect_refine_dict_mark;
  int perfect;
  DictionaryEntry *cur_entry, *ref_entry;
  int modified[MAX_DICT_ENTRY];
  int ref_chain[MAX_DICT_ENTRY], ref_chain_size;
  int ref_index;
  Mark *ref_mark, *cur_mark;
  
  total_mark_num = dictionary->total_mark_num;
  for(i = 0; i < prev_dict_size; i++) modified[i] = TRUE; 
  memset(modified+prev_dict_size, 0,
   	 sizeof(int)*(total_mark_num-prev_dict_size));
  
  cur_entry = dictionary->entries;
  for(i = 0; i < total_mark_num; i++, cur_entry++)
    if(cur_entry->singleton) modified[i] = TRUE;
    
  total_refine_dict_mark = perfect_refine_dict_mark = 0;
  for(i = prev_dict_size; i < total_mark_num; i++) {
    cur_entry = dictionary->entries + i;
    if(cur_entry->ref_index != -1 && !modified[i]) {
      ref_chain[0] = i;
      ref_chain_size = 1;
      ref_index = cur_entry->ref_index;
      while(ref_index != -1) {
        ref_chain[ref_chain_size++] = ref_index;
        if(modified[ref_index]) break;
	else ref_index = dictionary->entries[ref_index].ref_index;
      }
      for(j = ref_chain_size-1; j > 0; j--) {
        ref_entry = dictionary->entries + ref_chain[j];
	cur_entry = dictionary->entries + ref_chain[j-1];
        ref_mark = ref_entry->mark;
	cur_mark = cur_entry->mark;
	modify_refine_mark(ref_mark, cur_mark, 
		&perfect, &cur_entry->rdx, &cur_entry->rdy);
	modified[ref_chain[j-1]] = TRUE;
        total_refine_dict_mark++; 
	if(perfect) {
	  perfect_refine_dict_mark++;
	  cur_entry->perfect = TRUE;
        }
      }
    }
  }
  
//  printf("modified %d pixels in total\n", total_modified);

  printf("%d perfect dictionary symbols generated "
         "from %d refinement dictionary symbols\n", 
	 perfect_refine_dict_mark, total_refine_dict_mark);
}

/* Subroutine:	void encode_dictionary()
   Function:	split the dictionary into direct and refinement dictionaries, 
   		and encode them seperately 
   Input:	none
   Output:	none
*/
void encode_dictionary()
{
  /* initialize direct and refinement dictionaries */
  direct_dict = (DirectDict *)malloc(sizeof(DirectDict)*1);
  refine_dict = (RefineDict *)malloc(sizeof(RefineDict)*1);
  if(!direct_dict || !refine_dict) 
    error("encode_dictionary: no memory for direct or refinement dictionary\n");
  
  /* split the dictionary into direct and refinement dictionaries */
  split_dictionary();

  /* since dictionary entries are shuffled during the splitting process, 
     modify the indices to the new correct values */
  modify_dict_indices();
    
  /* code the direct dictionary */
  encode_direct_dict();
  
  /* code the refinement dictionary */
  encode_refine_dict();

  /* free the memories for direct and refinement dictionaries */
  free((void *)direct_dict);
  free((void *)refine_dict);
}

void copy_to_direct_dict(DictionaryEntry *);
void copy_to_refine_dict(DictionaryEntry *);

/* Subroutine: 	void split_dictionary()
   Function:	split the dictionary into 2 parts, one directly coded and the
   		other refinement coded
   Input:	none
   Output:	none
*/
void split_dictionary()
{
  register int i;
  int total_mark_num;
  DictionaryEntry *cur_entry;
  
  /* start splitting */
  total_mark_num = dictionary->total_mark_num;
  dictionary->direct_mark_num = dictionary->refine_mark_num = 0;
  refine_dict->total_mark_num = direct_dict->total_mark_num = 0;
  refine_dict->height_class_num = direct_dict->height_class_num = 0;
  cur_entry = dictionary->entries + prev_dict_size;
  for(i = prev_dict_size; i < total_mark_num; i++, cur_entry++) {
    if(cur_entry->ref_index == -1) {
      dictionary->direct_mark_num++;
      copy_to_direct_dict(cur_entry);
    }
    else {
      dictionary->refine_mark_num++;
      copy_to_refine_dict(cur_entry);
    }
  }

  codec->report.total_dict_mark = dictionary->total_mark_num-prev_dict_size;
  codec->report.direct_dict_mark = dictionary->direct_mark_num;
  codec->report.refine_dict_mark = dictionary->refine_mark_num;
}

void insert_new_height_class(HeightClass *, int *, int, int);
void add_to_height_class(HeightClass *, DictionaryEntry *, int);
void find_in_height_class(HeightClass *, int, int, int *, int *);
void copy_height_class(HeightClass *, HeightClass *);

/* Subroutine: 	void copy_to_direct_dict();
   Function:	copy an entry from the temporary dictionary into the 
   		direct dictionary
   Input:	the entry to be copied
   Output:	none
*/
void copy_to_direct_dict(DictionaryEntry *entry)
{  
  register int j;
  int new_class;
  HeightClass *dh;
  
  /* find a correct height class to place the new mark 	*/
  dh = direct_dict->height_classes;
  for(j = direct_dict->height_class_num-1, new_class = TRUE; j >= 0; j--) 
  {
    if(entry->mark->height >= dh[j].height) {
      if(entry->mark->height == dh[j].height) new_class = FALSE; 
      else new_class = TRUE;
      break; 
    }
  }

  if(new_class) {
    insert_new_height_class(dh, &direct_dict->height_class_num,
      entry->mark->height, j+1);
    add_to_height_class(dh+j+1, entry, 0);
  }    
  else add_to_height_class(dh+j, entry, 0);
  
  direct_dict->total_mark_num++;
}

/* Subroutine: 	void copy_to_refine_dict()
   Function:	copy an entry from temporary dictionary to refinement
   		dictionary. This procedure is more complicated because a symbol
		has to come after its reference, though it may have smaller
		height value.
   Input:	the entry to be copied
   Output:	none
*/	
void copy_to_refine_dict(DictionaryEntry *entry)
{
  register int j;
  int hc_posi, ent_posi;
  HeightClass *rh;
  int rh_num;
  int new_class;
  
  rh = refine_dict->height_classes;
  rh_num = refine_dict->height_class_num;
  
  /* find which height class the current mark's reference belong to */
  find_in_height_class(rh, rh_num, entry->ref_index, &hc_posi, &ent_posi);
  
  /* if the reference mark isn't in the refinement dictionary, then we can 
     follow the same procedure as in the case of direct dictionary to insert
     the current mark */
  if(hc_posi == -1) {
    for(j = rh_num-1, new_class = TRUE; j >= 0; j--) 
    {
      if(entry->mark->height >= rh[j].height) 
      {
        if(entry->mark->height == rh[j].height) 
          new_class = FALSE; 
        else 
          new_class = TRUE;
        break; 
      }
    }

    if(new_class) 
    {
      insert_new_height_class(rh, &refine_dict->height_class_num,
        entry->mark->height, j+1);
      add_to_height_class(rh+j+1, entry, 0);
    }    
    else add_to_height_class(rh+j, entry, 0);  
  }
  
  /* otherwise, we make sure the current mark comes after its reference */
  else {
    if(rh[hc_posi].height == entry->mark->height) 
    {
      add_to_height_class(rh+hc_posi, entry, ent_posi+1);
    }
    else 
    {
      for(j = rh_num-1, new_class = TRUE; j > hc_posi; j--) 
      {
        if(entry->mark->height >= rh[j].height) {
          if(entry->mark->height == rh[j].height) new_class = FALSE; 
          else new_class = TRUE;
          break; 
        }
      }

      if(new_class) {
        insert_new_height_class(rh, &refine_dict->height_class_num,
          entry->mark->height, j+1);
        add_to_height_class(rh+j+1, entry, 0);
      }    
      else add_to_height_class(rh+j, entry, 0);  
    }
  } 
   
  refine_dict->total_mark_num++;
}

/* Subroutine:	void add_to_height_class()
   Function:	insert an entry into the input height class. For direct 
   		dictionary, class entries are fully sorted by their width. For
		refinement dictionary, entries are sorted as much as possible
		so that every symbol comes after its reference (even though its 
		width is bigger). For this purpose, we put the new entry in 
		a sorted way after the input "start" (always 0 for direct 
		dictionary). 
   Input:	entry and height class pointers, the starting position
   Output:	none
*/
void add_to_height_class(HeightClass *height_class, DictionaryEntry *entry, int start)
{
  int i, posi;
  
  if(height_class->cur_entry_num == MAX_HEIGHT_CLASS_ENTRY) 
    error("add_to_height_class: height class entry buffer is full\n");

  /* find the right position to insert the new mark 	*/
  for(posi = start; posi < height_class->cur_entry_num; posi++) 
    if(height_class->entries[posi].mark->width > entry->mark->width) break;
  /* move the existing marks forward			*/
  for(i = height_class->cur_entry_num; i > posi; i--) 
    height_class->entries[i] = height_class->entries[i-1];
  
  /* store the mark information		*/
  height_class->entries[posi].index = entry->index;
  height_class->entries[posi].mark = entry->mark;
  height_class->entries[posi].ref_index = entry->ref_index;
  height_class->entries[posi].ref_mark = entry->ref_mark;
  height_class->entries[posi].rdx = entry->rdx;
  height_class->entries[posi].rdy = entry->rdy;
  height_class->entries[posi].rdx = 0;
  height_class->entries[posi].rdy = 0;
  
  height_class->cur_entry_num++;
}

/* Subroutine:	void insert_new_height_class()
   Function:	insert a new height class at the designated position
   Input:	height class buffer and size, the new height and 
   		its position
   Output:	none
*/
void insert_new_height_class(HeightClass *hc, int *hc_num, int nh, int posi)
{
  register int i;
  int num;
  
  num = *hc_num;
  
  if(num == MAX_HEIGHT_CLASS)
    error("insert_new_hc: height buffer is full!\n");

  for(i = num; i > posi; i--) 
    copy_height_class(hc + i, hc + i-1);

  hc[posi].height = nh;
  hc[posi].cur_entry_num = 0;
  
  (*hc_num)++;
}

/* Subroutine: 	void copy_height_class()
   Function:	copy height class information from "src" to "dest"
   Input:	source and destination height class pointers
   Output:	none
*/
void copy_height_class(HeightClass *dest, HeightClass *src)
{
  register int i;
  
  dest->cur_entry_num = src->cur_entry_num;
  dest->height = src->height;
  for(i = 0; i < src->cur_entry_num; i++) 
    dest->entries[i] = src->entries[i];
}

/* Subroutine:	void find_in_height_class()
   Function:	scan the existing height classes, find the input index and
   		return its position
   Input:	height class buffer and its size, the input index
   Output:	the input's position in height class buffer
*/
void find_in_height_class(HeightClass *hc, int hc_num, int index, 
	int *hc_posi, int *ent_posi)
{
  register int i, j;
  HeightClass *cur_hc;
  
  for(i = 0, cur_hc = hc; i < hc_num; i++, cur_hc++) 
  {
    for(j = 0; j < cur_hc->cur_entry_num; j++) 
    {
      if(cur_hc->entries[j].index == index) {
        *hc_posi = i; *ent_posi = j; return;
      }
    }
  }
  
  /* this means the reference isn't in the refinement dictionary, so it must 
     be a direct dictionary symbol */
  *hc_posi = -1; *ent_posi = -1; return;
}

void get_new_index_table(int *);

/* Subroutine:	void modify_dict_indices()
   Function:	modify the dictionary indices. After the original dict is
   		splitted into direct and refinement dictionaries which are
		sorted by height, the index field needs to be modified 
		accordingly
   Input:	none
   Output:	none
*/
void modify_dict_indices()
{
  int index_tbl[MAX_DICT_ENTRY];
  register int i, j;
  HeightClass *class_ptr;
  HeightClassEntry *entry_ptr;
  DictionaryEntry *cur_entry;
  
  get_new_index_table(index_tbl);

  /* modify the "index" field in "dictionary", only modify the new part */
  cur_entry = dictionary->entries+prev_dict_size;
  for(i = prev_dict_size; i < dictionary->total_mark_num; i++, cur_entry++) 
    cur_entry->index = index_tbl[cur_entry->index];
    
  /* modify the "ref_index" field in "refine_dict" */
  class_ptr = refine_dict->height_classes;
  for(i = 0; i < refine_dict->height_class_num; i++, class_ptr++) { 
    entry_ptr = class_ptr->entries;
    for(j = 0; j < class_ptr->cur_entry_num; j++, entry_ptr++) 
      entry_ptr->ref_index = dictionary->entries[entry_ptr->ref_index].index;
  }
}

/* Subroutine:	void get_new_index_table()
   Function:	re-visit the two dictionaries to gather new index order after
   		splitting
   Input:	none
   Output:	the new index table
*/
void get_new_index_table(int *index_tbl)
{
  register int i, j, counter;
  HeightClass *class_ptr;
  
  counter = prev_dict_size; 
  
  /* scan direct dictionary */
  class_ptr = direct_dict->height_classes;
  for(i = 0; i < direct_dict->height_class_num; i++, class_ptr++) {
    for(j = 0; j < class_ptr->cur_entry_num; j++)
    	index_tbl[class_ptr->entries[j].index] = counter++;
  }

  /* scan refinement dictionary */
  class_ptr = refine_dict->height_classes;
  for(i = 0; i < refine_dict->height_class_num; i++, class_ptr++) {
    for(j = 0; j < class_ptr->cur_entry_num; j++)
    	index_tbl[class_ptr->entries[j].index] = counter++;
  }
}

void encode_refine_bitmap(HeightClassEntry *, CODED_SEGMENT_DATA *);
void get_export_flags(int *, int *);
void encode_export_flags(int *, int, CODED_SEGMENT_DATA *);

extern void write_segment_header(SegHeader *);
extern void write_sym_dict_seg_header(SymDictDataHeader *);  

/* Subroutine:	void encode_direct_dict()
   Function:	For the direct dictionary, 
   		1. send it height class by height class
		2. for each height class, Huffman code delta_height;
		3. for each symbol in the height class, Huffman code 
		   delta_width, arithmetic code its bitmap
   Input:	none
   Output:	none
*/
void encode_direct_dict()
{
  register int i, j;
  HeightClass *class_ptr;
  int pre_height, pre_width;
  Mark *cur_mark;
  int export_flags[MAX_DICT_ENTRY];
  double a;
    
  SegHeader header;
  SymDictDataHeader data_header;
  
  ARITH_CODED_BITSTREAM bitstr;
  CODED_SEGMENT_DATA coded_data;

  int ori_file;	/* file size before this segment is transmitted */
    
  /* decide # bits needed to code the symbol IDs */
  a = ceil(log((double)direct_dict->total_mark_num)/log(2.));
  if((symbol_ID_in_bits = (int)a) > MAX_SYMBOL_ID_LEN)
    error("encode_direct_dict: code length for symbol IDs is too long\n");
  
  /* encode segment data */
  ori_file = ftell(codec->fp_out);
  bitstr.max_buffer_size = 1024;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data)
    error("encode_direct_dict: cannot allocate memory\n");
  coded_data.size = 0;
  
  reset_arith_int_coders();
  reset_arith_bitmap_coders();
  arith_encode_init();
  
  class_ptr = direct_dict->height_classes;
  for(i = 0, pre_height = 0; i < direct_dict->height_class_num; i++) {
    int_encode(class_ptr->height-pre_height, IADH, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.size_bits += bitstr.coded_size;

    pre_width = 0;
    for(j = 0; j < class_ptr->cur_entry_num; j++) {
      cur_mark = class_ptr->entries[j].mark;
      int_encode(cur_mark->width-pre_width, IADW, &bitstr);
      buffer_coded_bitstream(&bitstr, &coded_data);
      codec->report.size_bits += bitstr.coded_size;

      bin_encode_direct(cur_mark->data, cur_mark->width, cur_mark->height, 
			&bitstr);
      buffer_coded_bitstream(&bitstr, &coded_data);
      codec->report.direct_bits += bitstr.coded_size;
      codec->report.uncoded_direct_bits += bitstr.uncoded_size;
      
      pre_width = cur_mark->width;
    }

    int_encode(OOB, IADW, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.size_bits += bitstr.coded_size;
    
    pre_height = class_ptr->height;
    class_ptr++;
  }
  
  /* encode export flags */
  for(i = 0; i < direct_dict->total_mark_num; i++) 
    export_flags[i] = TRUE;
  encode_export_flags(export_flags, direct_dict->total_mark_num, &coded_data);
  
  bitstr.coded_size = 0;
  arith_encode_flush(&bitstr);
  buffer_coded_bitstream(&bitstr, &coded_data);
  
  free((void *)bitstr.data);
  
  /* segment header */
  header.type = SYM_DICT;
  header.retain_this = TRUE;	/* always retain a dictionary */
  header.ref_seg_count = 0;	/* direct dict doesn't refer to anyone else */
  header.page_asso = 0;  
  
  header.seg_length = 2 +	/* sym dict flags */
  		      2 +	/* sym dict AT flags */
		      4 +	/* SDNUMEXSYMS */
		      4;	/* SDNUMNEWSYMS */
  header.seg_length += coded_data.size;
  write_segment_header(&header);
  
  /* segment data header */
  data_header.huff = FALSE;
  data_header.refagg = FALSE;
  data_header.ctx_used = FALSE;
  data_header.ctx_retained = FALSE;
  data_header.dtemplate = 2;
  data_header.atx[0] = 2; data_header.aty[0] = -1;
  data_header.rtemplate = 0;
    
  data_header.numexsyms = direct_dict->total_mark_num;
  data_header.numnewsyms = direct_dict->total_mark_num;
  write_sym_dict_seg_header(&data_header);
  
  /* segment data */
  write_coded_bitstream(coded_data.data, coded_data.size<<3);

  codec->report.direct_dict_size = ftell(codec->fp_out) - ori_file;
}

/* Subroutine:	void encode_refine_dict()
   Function:	For the refinement dictionary,
		1. send it symbol by symbol
		2. for each symbol, run-length code its reference symbol_ID,
		   Huffman code its delta_width and delta_height w.r.t. its
		   reference, arithmetic code its refinement bitmap
   Input:	none
   Output:	none
*/
void encode_refine_dict()
{
  register int i, j;
  HeightClass *class_ptr;
  HeightClassEntry *entry_ptr;
  register int pre_height, pre_width;
  Mark *cur_mark, *ref_mark;
  int export_flags[MAX_DICT_ENTRY], export_mark_num;
  double a;
 
  SegHeader header;
  SymDictDataHeader data_header;
  
  ARITH_CODED_BITSTREAM bitstr;
  CODED_SEGMENT_DATA coded_data;
  
  int ori_file;	/* file size before this segment is transmitted */
    
  /* decide # bits needed to fixed length code the symbol IDs */
  a = ceil(log((double)dictionary->total_mark_num)/log(2.));
  if((symbol_ID_in_bits = (int)a) > MAX_SYMBOL_ID_LEN)
    error("encode_direct_dict: code length for symbol IDs is too long\n");

  ori_file = ftell(codec->fp_out);
  
  bitstr.max_buffer_size = 1024;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data)
    error("encode_refine_dict: cannot allocate memory\n");
  coded_data.size = 0;
  
  reset_arith_int_coders();
  reset_arith_bitmap_coders();
  arith_encode_init();
  
  class_ptr = refine_dict->height_classes;
  pre_height = 0;
  for(i = 0; i < refine_dict->height_class_num; i++) {
    int_encode(class_ptr->height-pre_height, IADH, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.size_bits += bitstr.coded_size;

    pre_width = 0; entry_ptr = class_ptr->entries;
    for(j = 0; j < class_ptr->cur_entry_num; j++) {
      cur_mark = entry_ptr->mark;
      ref_mark = entry_ptr->ref_mark;
      int_encode(cur_mark->width-pre_width, IADW, &bitstr);
      buffer_coded_bitstream(&bitstr, &coded_data);
      codec->report.size_bits += bitstr.coded_size;
 
      int_encode(1, IAAI, &bitstr);
      buffer_coded_bitstream(&bitstr, &coded_data);
      codec->report.misc_bits += bitstr.coded_size;
      
      encode_refine_bitmap(entry_ptr, &coded_data);

      pre_width = cur_mark->width;
      entry_ptr++;
    }
    int_encode(OOB, IADW, &bitstr);
    buffer_coded_bitstream(&bitstr, &coded_data);
    codec->report.size_bits += bitstr.coded_size;
    
    pre_height = class_ptr->height;
    class_ptr++;
  }
  #ifdef DEBUG
  fprintf(stdout, "refinement dictionary CR is: %d/%d = %.2f\n",
	  codec->report.uncoded_refine_bits, codec->report.refine_bits, 
	  (float)codec->report.uncoded_refine_bits/(float)codec->report.refine_bits); 
  #endif
  
  get_export_flags(export_flags, &export_mark_num);
  encode_export_flags(export_flags, dictionary->total_mark_num, &coded_data);

  bitstr.coded_size = 0;
  arith_encode_flush(&bitstr);
  buffer_coded_bitstream(&bitstr, &coded_data);
  free((void *)bitstr.data);  
  
  /* segment header */
  header.type = SYM_DICT;
  header.retain_this = TRUE;	/* always retain a dictionary */
  if(codec->cur_seg == 2) {
    header.ref_seg_count = 1;
    header.ref_seg[0] = codec->cur_seg-1;
    header.retain_ref[0] = FALSE;
  }
  else {
    header.ref_seg_count = 2;
    header.ref_seg[0] = codec->cur_seg-6;
    header.ref_seg[1] = codec->cur_seg-1;
    if(codec->split_num > 1 && codec->cur_split > 0)
      header.ref_seg[0]++;
    if(codec->residue_coding) header.ref_seg[0]--;
    header.retain_ref[0] = header.retain_ref[1] = FALSE;
  }
  header.page_asso = 0;
  
  header.seg_length = 2 +	/* sym dict flags */
  		      2 +	/* sym dict AT flags */
		      4 +	/* SDNUMEXSYMS */
		      4;	/* SDNUMNEWSYMS */
  header.seg_length += coded_data.size;
  write_segment_header(&header);
  
  /* segment data header */
  data_header.huff = FALSE;
  data_header.refagg = TRUE;
  data_header.ctx_used = FALSE;
  data_header.ctx_retained = FALSE;
  data_header.dtemplate = 2;
  data_header.atx[0] = 2; data_header.aty[0] = -1;
  data_header.rtemplate = 1;
  
  data_header.numexsyms = export_mark_num;
  data_header.numnewsyms = refine_dict->total_mark_num;
  write_sym_dict_seg_header(&data_header);
  
  /* segment data */
  write_coded_bitstream(coded_data.data, coded_data.size<<3);

  codec->report.refine_dict_size = ftell(codec->fp_out) - ori_file;
}

/* Subroutine:	void encode_refine_bitmap()
   Function:	transmit a bitmap using refinement coding, as specified in 
   		JBIG2 CD, this resembles a symbol region coding procedure
   Input:	current dictionary entry
   Output:	coded bitstream
*/
void encode_refine_bitmap(HeightClassEntry *entry, 
	CODED_SEGMENT_DATA *coded_data)
{
  Mark *ref_mark, *cur_mark;
  int index;
  int dw, dh, dx, dy;
  PixelCoord rc, cc;
  ARITH_CODED_BITSTREAM bitstr;
  
  ref_mark = entry->ref_mark;
  cur_mark = entry->mark;
  index = entry->ref_index;
  
  bitstr.max_buffer_size = 1024;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data) 
    error("encode_export_flags: Cannot allocate memory\n");
  
  /*
  int_encode(0, IADT, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;
  
  int_encode(0, IADT, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;
  
  int_encode(0, IAFS, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;
  
  #ifdef NEVER
  int_encode(0, IAIT, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;
  #endif
  */
  symID_encode(index, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.index_bits += bitstr.coded_size;
  /*
  int_encode(1, IARI, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;

  dw = cur_mark->width - ref_mark->width;
  dh = cur_mark->height- ref_mark->height;

  int_encode(dw, IARDW, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.size_bits += bitstr.coded_size;
  
  int_encode(dh, IARDH, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.size_bits += bitstr.coded_size;
  */

  cc.x = cur_mark->c.x + entry->rdx; 
  cc.y = cur_mark->c.y + entry->rdy;
  rc.x = ref_mark->c.x; rc.y = ref_mark->c.y;  
  
  dx = (cc.x-rc.x); dx -= (cur_mark->width-ref_mark->width)/2;
  dy = (cc.y-rc.y); dy -= (cur_mark->height-ref_mark->height)/2;
      
  int_encode(dx, IARDX, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.refine_offset_bits += bitstr.coded_size;
  
  int_encode(dy, IARDY, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.refine_offset_bits += bitstr.coded_size;

  bin_encode_refine(ref_mark->data, ref_mark->width, ref_mark->height,
                    cur_mark->data, cur_mark->width, cur_mark->height,
                    cc.x-rc.x, cc.y-rc.y, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.refine_bits += bitstr.coded_size;      
  codec->report.uncoded_refine_bits += bitstr.uncoded_size;
  
  /*
  int_encode(OOB, IADS, &bitstr);
  buffer_coded_bitstream(&bitstr, coded_data);
  codec->report.misc_bits += bitstr.coded_size;
  */
  free((void *)bitstr.data);
}

/* Subroutine:	void encode_export_flags()
   Function:	transmit the export flags (whether to retain this symbol of
   		not) as specified in JBIG2 CD
   Input:	buffer of flags and its size
   Output:	coded bitstream
*/
void encode_export_flags(int *flags, int size, CODED_SEGMENT_DATA *coded_data)
{
  register int i;
  int cur_ex, count;
  ARITH_CODED_BITSTREAM bitstr;
  
  bitstr.max_buffer_size = 1024;
  bitstr.data = (char *)malloc(sizeof(char)*bitstr.max_buffer_size);
  if(!bitstr.data) 
    error("encode_export_flags: Cannot allocate memory\n");
  
  cur_ex = FALSE; 
  for(i = 0; i < size; ) {
    count = 0;
    while(i < size && flags[i] == cur_ex) {
      count++; i++;
    }
    
    int_encode(count, IAEX, &bitstr);
    buffer_coded_bitstream(&bitstr, coded_data);
    codec->report.export_bits += bitstr.coded_size;
    cur_ex = !cur_ex;
  }
  
  free((void *)bitstr.data);
}

/* Subroutine:	void get_export_flags()
   Function:	decide the export flags for all the dictionary symbols
   Input:	buffer of flags
   Output:	none
*/
void get_export_flags(int *export, int *export_num)
{
  register int i;
  int total_mark_num;
  DictionaryEntry *entry;
  
  total_mark_num = dictionary->total_mark_num;
  
  memset(export, 0xff, sizeof(int)*total_mark_num);
  
  *export_num = 0;
  for(i = 0, entry = dictionary->entries; i < total_mark_num; entry++, i++) {
    if(!entry->singleton) {
      export[entry->index] = TRUE; (*export_num)++;
    }
    else export[entry->index] = FALSE;
  }
}
