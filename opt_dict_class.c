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
#include <math.h>

#define BIGGEST_SUPERCLASS       1000

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

void opt_mark_dictionary_class(void);
extern void form_equivalence_classes(void);
void merge_equivalence_classes(void);
void form_equivalence_supclasses(void);
extern void tree_to_dictionary(void);
void get_mark_match_tree(void);
extern void modify_dictionary_marks(void);
extern void delete_perfect_dict_marks(void);
extern void exclude_singletons(void);

extern void error(char *);

extern Dictionary 	*dictionary;
extern int		symbol_ID_in_bits;
extern MarkList 	*all_marks;
extern Codec		*codec;
extern EquiClass	*equi_class;
extern int 		equi_class_num;
extern MarkPointerList 	*mark_pointer_list;
extern MarkMatchTree 	*mark_match_tree;

/* Subroutine:	void opt_mark_dictionary_class()
   Function:	optimize the mark dictionary for each page, using the 
   		class-based design 
   Input:	none
   Output:	none
*/
void opt_mark_dictionary_class(void)
{
  double a;

  form_equivalence_classes();
  merge_equivalence_classes();
  form_equivalence_supclasses();
  get_mark_match_tree();
  tree_to_dictionary();
  
  free((void *)equi_class);
  free((void *)mark_match_tree);

  if(codec->lossy) modify_dictionary_marks();
  
  delete_perfect_dict_marks();  
  exclude_singletons();
  
  /* decide # bits needed to fixed length code the symbol IDs */
  a = ceil(log((double)dictionary->total_mark_num)/log(2.));
  symbol_ID_in_bits = (int)a;
  
  /* this is unnecessary because currently class-based dictionary *
   * cannot work on multipage documents */ 
  #ifdef NEVER
  update_prev_dict_flags(); 
  #endif
}

float *merge_cost;
int *new_repre;

void calc_merge_cost();
void find_classes_to_merge(int *, int *);
void merge_two_classes(int, int);
void update_merge_cost(int, int);

void merge_equivalence_classes(void)
{
  register int i;
  int cur_size, tar_size;
  int cl1, cl2;
  int *empty_classes, empty_class_num;
  int single_class_num;
  
  extern void delete_equi_classes(int *, int);
  
  cur_size = equi_class_num;
  merge_cost = (float *)malloc(sizeof(float)*(cur_size*cur_size));
  new_repre = (int *)malloc(sizeof(int)*cur_size*cur_size);
  if(!merge_cost || !new_repre)
    error("merge_equi_class: cannot allocate memory\n");
  
  for(i = 0, single_class_num = 0; i < equi_class_num; i++)
    if(equi_class[i].total_entry_num == 1)
      single_class_num++;
      
  printf("current dictionary size is %d, enter target size:", 
  	cur_size-single_class_num);
  scanf("%d", &tar_size);
  tar_size += single_class_num;
  
  if(tar_size < cur_size) {
    calc_merge_cost();
    while(tar_size < cur_size) {
      find_classes_to_merge(&cl1, &cl2);
      if(cl1 != -1 && cl2 != -1) {
        merge_two_classes(cl1, cl2);
        update_merge_cost(cl1, cl2);
        cur_size--;
      }
      else break;
    }
    free((void *)merge_cost);
    free((void *)new_repre);
    
    if(cur_size > tar_size) 
      printf("Cannot reduce to target size, smallest size attained is %d\n", 
    	cur_size-single_class_num);

    /* now delete the empty classes after merging step is finished */
    empty_classes = (int *)malloc(sizeof(int)*equi_class_num);
    if(!empty_classes)
      error("merge_equivalence_classes: Cannot allocate memory\n");
    
    for(i = 0, empty_class_num = 0; i < equi_class_num; i++)
      if(equi_class[i].total_entry_num == 0) 
        empty_classes[empty_class_num++] = i;
    
    delete_equi_classes(empty_classes, empty_class_num);
    
    free((void *)empty_classes);
  }
}

float cost_between_classes(int, int);

void calc_merge_cost()
{
  register int i, j;

  for(i = 0; i < equi_class_num; i++) 
    for(j = 0; j < i; j++) 
      merge_cost[i*equi_class_num+j] = cost_between_classes(i, j);
}

float cost_between_classes(int cl1, int cl2)
{
  register int i;
  int entry_num1, entry_num2;
  EquiClass *clptr1, *clptr2;
  EquiClass tmp_cl;
  Mark *rmark1, *rmark2;
  float cost;
  extern float get_equi_class_repre(EquiClass *);
    
  clptr1 = equi_class + cl1; entry_num1 = clptr1->total_entry_num;
  clptr2 = equi_class + cl2; entry_num2 = clptr2->total_entry_num;
  if(clptr1->repre == -1 || clptr2->repre == -1)
    return TOO_BIG;
  else {
    rmark1 = all_marks->marks + clptr1->repre;
    rmark2 = all_marks->marks + clptr2->repre;
  
    if(abs(rmark1->width  - rmark2->width ) > 2 ||
       abs(rmark1->height - rmark2->height) > 2) {
      new_repre[cl1*equi_class_num+cl2] = -1; 
      cost = TOO_BIG;
    }
    else if(codec->match_two_marks(rmark1, rmark2) > codec->mismatch_thres) {
      new_repre[cl1*equi_class_num+cl2] = -1;
      cost = TOO_BIG;
    }
    else {
      tmp_cl.total_entry_num = entry_num1 + entry_num2;
      if(tmp_cl.total_entry_num > MAX_EQUI_CLASS)
        error("cost_between_classes: class entry buffer is full!\n");
      
      for(i = 0; i < entry_num1; i++)
        tmp_cl.entries[i] = equi_class[cl1].entries[i];
      for(i = 0; i < entry_num2; i++)
        tmp_cl.entries[i+entry_num1] = equi_class[cl2].entries[i];
      
      cost = get_equi_class_repre(&tmp_cl);
      new_repre[cl1*equi_class_num+cl2] = tmp_cl.repre;
    }
  
    return cost;
  }
}

void find_classes_to_merge(int *cl1, int *cl2)
{
  register int i, j;
  float lowest_cost;
  int size;
  
  size = equi_class_num; 
  lowest_cost = TOO_BIG; 
  (*cl1) = -1; (*cl2) = -1; 
  for(i = 1; i < size; i++) 
    for(j = 0; j < i; j++) { 
      if(merge_cost[i*size+j] < lowest_cost) {
        lowest_cost = merge_cost[i*size+j];
        *cl1 = i; *cl2 = j;
      }
    }
}

void merge_two_classes(int cl1, int cl2)
{
  register int i;
  int num1, num2;
  
  num1 = equi_class[cl1].total_entry_num;
  num2 = equi_class[cl2].total_entry_num;
  
  /* move all entries in cl1 into cl2: cl2 is always smaller than cl1 */
  for(i = 0; i < num1; i++)
    equi_class[cl2].entries[num2+i] = equi_class[cl1].entries[i];
  equi_class[cl2].total_entry_num += num1;
  equi_class[cl2].repre = new_repre[cl1*equi_class_num+cl2];

  /* remove class cl1 */
  equi_class[cl1].repre = -1;
  equi_class[cl1].total_entry_num = 0;
}

/* cl1 is the class being removed, cl2 is the new bigger class */
void update_merge_cost(int cl1, int cl2)
{
  register int i;
  
  for(i = 0; i < cl1; i++) 
    merge_cost[cl1*equi_class_num+i] = TOO_BIG;
  for(i = cl1+1; i < equi_class_num; i++)
    merge_cost[i*equi_class_num+cl1] = TOO_BIG;
      
  for(i = 0; i < cl2; i++)
    merge_cost[cl2*equi_class_num+i] = cost_between_classes(cl2, i);
}

MarkPointerList *mark_pointer_list;
SuperclassInfo *old_scl, *new_scl;   /* old/new super-class information */
MatchInfo *mark_match_info;         /* ref mark and mm_score for all marks */
MarkMatchBuf *mark_match_buf;       /* list of most similar marks for all marks */
int *mark_scl_index;              /* which superclass each mark belongs to */
SuperclassMatchInfo *scl_match_info;   

extern void init_mark_pointer_list();
void match_all_repre_marks();
void init_scl(), init_mark_match_info();
void match_old_scl(), generate_new_scl();
void update_mark_scl_index(), update_scl_info(), update_mark_match_info();

/* form equivalence super-classes from the equivalence class representatives */
void form_equivalence_supclasses()
{ 
  #ifdef DEBUG
  register int i;
  int direct_single;
  #endif
  
  init_mark_pointer_list(); 
  init_mark_match_info();
  match_all_repre_marks();

  init_scl();
  do {
    match_old_scl();
    generate_new_scl();
    if(new_scl->scl_num == old_scl->scl_num) break;
    update_mark_scl_index();
    update_mark_match_info();
    update_scl_info();
  } while(1);

  free((void *)new_scl->scl);
  free((void *)old_scl);
  free((void *)new_scl);
  free((void *)mark_scl_index);
  free((void *)mark_match_buf);
  free((void *)scl_match_info);
  
  #ifdef DEBUG
  direct_single = 0;
  for(i = 0; i < new_scl->scl_num; i++) {
    if(new_scl->scl[i].total_entry_num == 1 &&
       equi_class[new_scl->scl[i].entries[0]].total_entry_num == 1)
       direct_single++;
  }
  
  printf("%d superclasses built for %d classes, with %d direct singletons\n",
    new_scl->scl_num, equi_class_num, direct_single); 
  #endif
}

/* initialize mark_match_info buffer, all marks have no match now and 
   all marks are direct symbols */
void init_mark_match_info()
{
  register int i;

  mark_match_info = (MatchInfo *)
    malloc(sizeof(MatchInfo)*mark_pointer_list->mark_num);
  mark_match_buf = (MarkMatchBuf *)
    malloc(sizeof(MarkMatchBuf)*mark_pointer_list->mark_num);
  mark_scl_index = (int *)
    malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!mark_match_info || !mark_match_buf || !mark_scl_index)
    error("init_mark_match_info: Cannot allocate memory\n");

  /* initialize 3 items */
  for(i = 0; i < mark_pointer_list->mark_num; i++) {
    mark_match_info[i].ref_mark = -1; /* all repres have no match yet */
    mark_match_info[i].mm_score = 1.;
    mark_match_buf[i].match_num = 0; 
    mark_scl_index[i] = i;       /* all repres belong to its own superclass */
  }
}

void add_to_match_buf(int, int, float);

/* match all the representative marks once and for all, write the 
   matching information into mark_match_buf, every mark can have up
   to MAX_MATCH_PER_MARK matching marks, all of which have a mismatch
   score below the mismatch threshold */
void match_all_repre_marks()
{
  register int i, j;
  int total_mark_num;
  Mark *cur, *ref;
  float mm_score;

  total_mark_num = mark_pointer_list->mark_num;
  for(i = 0; i < total_mark_num; i++) {
    cur = mark_pointer_list->marks[i];
    for(j = 0; j < i; j++) {
      ref = mark_pointer_list->marks[j];
      if(codec->prescreen_two_marks(cur, ref)) {
        mm_score = codec->match_two_marks(cur, ref);
	if(mm_score < codec->mismatch_thres) { 
	  add_to_match_buf(i, j, mm_score);
	  add_to_match_buf(j, i, mm_score);
	}
      }
    }
  }
}

/* add for a mark a reference mark, make sure all reference marks of this
   mark are always sorted by their mismatch scores */ 
void add_to_match_buf(int cur, int ref, float score)
{
  register int i;
  int posi;
  MarkMatchBuf *buf;

  buf = mark_match_buf + cur;
  /* find where this current ref mark should be inserted */
  for(i = buf->match_num-1; i >= 0; i--) 
    if(buf->match_info[i].mm_score < score) break;
  posi = i+1;
  /* if the ref mark buffer has been filled up, and if the current ref's
     mismatch score is higher than the last one, then we need not change 
     the current mark's reference buffer */
  if(posi < MAX_MATCH_PER_MARK) {
    if(buf->match_num < MAX_MATCH_PER_MARK) buf->match_num++;
    /* move the reference information backward by 1 position */
    for(i = buf->match_num-1; i > posi; i--) 
      buf->match_info[i] = buf->match_info[i-1];
    /* add the current reference into buffer */
    buf->match_info[posi].ref_mark = ref;
    buf->match_info[posi].mm_score = score;
  }
}

/* initialize old_scl, put each mark in dictionary into one scl */
void init_scl()
{
  register int i;
  Superclass *scl;
  
  old_scl = (SuperclassInfo *)malloc(sizeof(SuperclassInfo)*1);
  if(!old_scl) 
    error("init_scl: cannot allocate memory\n");
    
  scl = (Superclass *)malloc(sizeof(Superclass)*mark_pointer_list->mark_num);
  if(!scl) error("init_scl: cannot allocate memory\n");
  old_scl->scl = scl;
  old_scl->scl_num = mark_pointer_list->mark_num;
  
  for(i = 0; i < old_scl->scl_num; i++) {
    scl[i].total_entry_num = 1;
    scl[i].entries[0] = scl[i].leader = i;
  }
  
  new_scl = (SuperclassInfo *)NULL;
  scl_match_info = (SuperclassMatchInfo *)NULL;
}

void init_scl_match_info();
void find_scl_match(int, MatchInfo *);

/* for each existing super-class in old_scl buffer, find its best matching
   superclass, i.e., find the best matching mark for its super-class leader */
void match_old_scl()
{
  register int i;
  MatchInfo info;

  init_scl_match_info();

  for(i = 0; i < old_scl->scl_num; i++) {
    find_scl_match(i, &info);
    if(info.ref_mark != -1) {
      scl_match_info[i].ref_scl = mark_scl_index[info.ref_mark];
      scl_match_info[i].ref_mark = info.ref_mark;
      scl_match_info[i].mm_score = info.mm_score;
    }
  }
}

/* initialize scl_match_info buffer every time before matching 
   current superclasses */
void init_scl_match_info()
{
  register int i;
  
  if(scl_match_info) free((void *)scl_match_info);
  
  scl_match_info = (SuperclassMatchInfo *)
    malloc(sizeof(SuperclassMatchInfo)*old_scl->scl_num);
  if(!scl_match_info) 
    error("init_scl_match_info: Cannot allocate memory\n");

  for(i = 0; i < old_scl->scl_num; i++) {
    scl_match_info[i].ref_scl = scl_match_info[i].ref_mark = -1;
    scl_match_info[i].mm_score = 1.;
  }
}

/* find the best reference mark for the current superclass (leader) */
void find_scl_match(int cg, MatchInfo *info) 
{
  register int i;
  MarkMatchBuf *buf;

  buf = mark_match_buf + old_scl->scl[cg].leader;
  
  /* find the first matching mark that doesn't belong to the current 
     superclass */
  for(i = 0; i < buf->match_num; i++) 
    if(mark_scl_index[buf->match_info[i].ref_mark] != cg) break;

  if(i < buf->match_num) *info = buf->match_info[i];
  else {
    info->mm_score = 1.0;
    info->ref_mark = -1;
  }
}

void identify_chains(Chain *, int *);
void break_loops(Chain *, int), merge_old_scl(Chain *, int);

void generate_new_scl()
{
  Chain *chains;
  int chain_num;
  
  chains = (Chain *)malloc(sizeof(Chain)*old_scl->scl_num);
  if(!chains)
    error("generate_new_scl: cannot allocate memory\n");
  
  /* identify the underlying loops from scl_match_info */
  identify_chains(chains, &chain_num);
  
  /* break the loops found	*/
  break_loops(chains, chain_num);

  merge_old_scl(chains, chain_num);
  
  free((void *)chains);
}

int every_scl_marked(int *, int, int *);
int closed_loop_in_chain(int *, int, int *);
void copy_chain(Chain *, int *, int, int, int);
void add_to_chain(Chain *, int, int *, int);

void identify_chains(Chain *chains, int *chain_num)
{
  register int i;
  Chain *chain_ptr;
  int temp_chain[10000];
  int marker[10000];
  int temp_chain_size;
  int first_unmarked;
  int loop_head, old_chain;
  int scl_num;
  int no_loop;
  
  scl_num = old_scl->scl_num;
  for(i = 0; i < scl_num; i++) marker[i] = FALSE;
  
  *chain_num = 0; chain_ptr = chains;
  while(!every_scl_marked(marker, scl_num, &first_unmarked)) {
    temp_chain[0] = first_unmarked; temp_chain_size = 1;
    old_chain = FALSE; no_loop = FALSE;
    while(!closed_loop_in_chain(temp_chain, temp_chain_size, &loop_head) 
       && !no_loop && !old_chain) {
      temp_chain[temp_chain_size] = 
      	  scl_match_info[temp_chain[temp_chain_size-1]].ref_scl;
      /* if this new element is already marked, then the procedure will *
       * find an old loop, so break now*/
      if(temp_chain[temp_chain_size] == -1) no_loop= TRUE;
      else if(marker[temp_chain[temp_chain_size]]) old_chain = TRUE; 
      temp_chain_size++;
    }
    if(!old_chain) {
      copy_chain(chain_ptr, temp_chain, temp_chain_size, no_loop, loop_head);
      (*chain_num)++; chain_ptr++;
    }
    else  add_to_chain(chains, *chain_num, temp_chain, temp_chain_size);

    for(i = 0; i < temp_chain_size-1; i++) marker[temp_chain[i]] = TRUE;
  }
}

int every_scl_marked(int *marker, int size, int *first_unmarked)
{
  register int i;
  
  for(i = 0; i < size; i++) { 
    if(!marker[i]) {
      *first_unmarked = i;
      return FALSE;
    }
  }
  
  *first_unmarked = -1;
  return TRUE;
}

int closed_loop_in_chain(int *chain, int chain_size, int *loop_head)
{
  register int i;
  int last;
  
  last = chain[chain_size-1];
  for(i = 0; i < chain_size-1; i++) 
    if(last == chain[i]) {
      *loop_head = i;
      return TRUE;
    }
    
  return FALSE;
}

void copy_chain(Chain *chain, int *temp_chain, int temp_chain_size, 
  int no_loop, int loop_head)
{
  register int i;
  
  if(temp_chain_size-1 > LONGEST_CHAIN) 
      error("copy_chain: pre-defined buffer cannot hold the chain\n");
  
  for(i = 0; i < temp_chain_size-1; i++) 
    chain->chain[i] = temp_chain[i];
    
  chain->chain_size = temp_chain_size-1;
  if(!no_loop) {
    chain->loop_start = loop_head; 
    chain->loop_end = chain->chain_size-1;
  }
  else 
    chain->loop_start = chain->loop_end = temp_chain_size-2;
}

void add_to_chain(Chain *chains, int chain_num, 
  int *temp_chain, int temp_chain_size)
{
  register int i, j;
  int last, found;
  Chain *chain_ptr;
  int chain_index;

  /* find to which chain the temp buffer should be joined */
  found = FALSE;
  last = temp_chain[temp_chain_size-1]; 
  chain_ptr = chains;
  for(i = 0; i < chain_num && !found; i++) {
    for(j = 0; j < chain_ptr->chain_size && !found; j++) 
      if(last == chain_ptr->chain[j]) {
        found = TRUE;
        chain_index = i; 
	break;
      }
    chain_ptr++;
  }
  
  if(!found) 
    error("add_to_chain: illogical error, check code!\n");
  
  chain_ptr = chains + chain_index;  
  if(chain_ptr->chain_size+temp_chain_size-1 > LONGEST_CHAIN) 
    error("add_to_chain: pre-defined buffer is not long enough\n");
    
  for(j = 0, i = chain_ptr->chain_size; j < temp_chain_size-1; i++, j++) 
    chain_ptr->chain[i] = temp_chain[j];
    
  chain_ptr->chain_size += temp_chain_size-1;
}

void break_loops(Chain *chains, int chain_num)
{
  register int i, j;
  Chain *chain;
  float highest_mismatch, mismatch;
  
  for(i = 0; i < chain_num; i++) {
    chain = chains + i;
    if(chain->chain_size > 1) {
     chain->break_point = chain->chain[chain->loop_start];
     highest_mismatch =	scl_match_info[chain->break_point].mm_score;
     for(j = chain->loop_start + 1; j <= chain->loop_end; j++) {
      mismatch =  scl_match_info[chain->chain[j]].mm_score;
      if(mismatch > highest_mismatch) {
    	chain->break_point = chain->chain[j];
    	highest_mismatch = mismatch;
      }
     }
    }
    else chain->break_point = chain->chain[0];
    scl_match_info[chain->break_point].ref_mark = -1;
    scl_match_info[chain->break_point].ref_scl = -1;
    scl_match_info[chain->break_point].mm_score = 1.;
  }
}

void merge_scl_in_chain(Chain *, Superclass *);

void merge_old_scl(Chain *chains, int chain_num)
{
  register int i;
  Superclass *scl;
  Chain *chain;
  
  new_scl = (SuperclassInfo *)malloc(sizeof(SuperclassInfo)*1);
  if(!new_scl) 
    error("merge_old_scl: cannot allocate memory\n");
  scl = (Superclass *)malloc(sizeof(Superclass)*chain_num);
  if(!scl)
    error("merge_old_scl: cannot allocate memory\n");
    
  new_scl->scl = scl;
  new_scl->scl_num = chain_num;

  for(i = 0; i < chain_num; i++) {
    chain = chains + i;
    merge_scl_in_chain(chain, scl + i);
  }
}

void merge_scl_in_chain(Chain *chain, Superclass *ng)
{
  register int i, j, k;
  int total_entry_num;
  Superclass *og;
  
  total_entry_num = 0;
  for(i = 0; i < chain->chain_size; i++) 
    total_entry_num += old_scl->scl[chain->chain[i]].total_entry_num;
  
  if(total_entry_num > BIGGEST_SUPERCLASS) 
    error("merge_scl_in_chain: superclass is too big to be held in buffer\n");

  for(i = 0, j = 0; i < chain->chain_size; i++) {
    og = old_scl->scl + chain->chain[i];
    for(k = 0; k < og->total_entry_num; k++) 
      ng->entries[j++] = og->entries[k];
  }
  ng->total_entry_num = total_entry_num;
  ng->leader = old_scl->scl[chain->break_point].leader;
}

void update_mark_scl_index()
{
  register int i, j;
  Superclass *cg;

  cg = new_scl->scl;
  for(i = 0; i < new_scl->scl_num; i++) {
    for(j = 0; j < cg->total_entry_num; j++)
      mark_scl_index[cg->entries[j]] = i;
    cg++;
  }
}

/* after new superclasses have been generated, buffer mark_match_info needs 
   to be updated accordingly because more symbols(leaders) have found their
   match */ 
void update_mark_match_info()
{
  register int i;
  int leader;
  
  for(i = 0; i < old_scl->scl_num; i++) {
    leader = old_scl->scl[i].leader;
    mark_match_info[leader].ref_mark = scl_match_info[i].ref_mark;
    mark_match_info[leader].mm_score = scl_match_info[i].mm_score;
  } 
}

void update_scl_info()
{
  free((void *)old_scl->scl);
  free((void *)old_scl);
  
  old_scl = new_scl;
  new_scl = (SuperclassInfo *)NULL;
}

void get_mark_match_tree()
{
  register int i;
  int mark_num;
  int ref;
  
  mark_num = mark_pointer_list->mark_num;
  mark_match_tree = (MarkMatchTree *)malloc(sizeof(MarkMatchTree)*mark_num);
  if(!mark_match_tree)
    error("get_mark_match_tree: cannot allocate memory\n");
  
  for(i = 0; i < mark_num; i++) mark_match_tree[i].child_num = 0;
  
  for(i = 0; i < mark_num; i++) {
    ref = mark_match_info[i].ref_mark;
    mark_match_tree[i].parent = ref;
    mark_match_tree[i].mm_score = mark_match_info[i].mm_score;
    if(ref != -1) 
      mark_match_tree[ref].child[mark_match_tree[ref].child_num++] = i;
  }
}

void find_in_class(int);

void find_in_class(int mark)
{
  register int i, k;
  int class_entry_num;

  for(i = 0; i < equi_class_num; i++) {
    class_entry_num = equi_class[i].total_entry_num; 
    for(k = 0; k < class_entry_num; k++)
       if(equi_class[i].entries[k] == mark) {
	 printf("dict index %d lies in class %d\n", mark, i);
	 return;
       }
  }
}

void find_in_supclass(int);

void find_in_supclass(int repre)
{
  register int i, j;
  Superclass *scl;
  
  for(i = 0, scl = new_scl->scl; i < new_scl->scl_num; i++, scl++) {
    for(j = 0; j < scl->total_entry_num; j++) {
      if(repre == scl->entries[j]) {
        printf("representative #%d belongs to superclass #%d, ", repre, i);
	if(scl->total_entry_num == 1) 
	  printf("which has only ONE member\n");
	else 
	  printf("which has %d members\n", scl->total_entry_num);
	return;
      }
    }
  }
  
  printf("representative #%d doesn't belong to any superclass\n", repre);
}
