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

#define VERY_BIG 2000000

typedef struct {
  float mm_diff;
  int new_ref;
} RelocInfo;

typedef struct {
  float total_cost;
  int child_num;
  RelocInfo reloc_info[MAX_CHILD_NUM];
} CostAsLeaf;

typedef struct {
  Edge *edge;
  int parent_offspring;
} OPInfo;

 
//#define MAX_MATCH_PER_MARK 200	/* this overrides the previous value 50 */
#define MAX_MATCH_PER_MARK 800

typedef struct {
  int match_num;
  OPInfo op_info[MAX_MATCH_PER_MARK];
} MarkOPBuf;

static MarkOPBuf *mark_op_buf;   /* list of connected marks for each mark */
static CostAsLeaf *cost_as_leaf;
static int cur_dict_size;   /* current dictionary size */
static int tar_dict_size;   /* target dictionary size  */

void opt_mark_dictionary_tree(void);
void build_trees(void);
extern void get_tree_info(void);
void get_dict_size(void);
void write_mark_op_buf(void);
void modify_mark_op_buf(void);
void get_cost_as_leaf(void);
void modify_mark_match_tree(void);
extern void tree_to_dictionary(void);
void mark_singletons(void);
extern void modify_dictionary_marks(void);
extern void delete_perfect_dict_marks(void);
extern void exclude_singletons(void);
extern void error(char *);

extern Dictionary 	*dictionary;
extern int		symbol_ID_in_bits;
extern MarkList 	*all_marks;
extern Codec		*codec;
extern MarkPointerList 	*mark_pointer_list;
extern MarkMatchTree 	*mark_match_tree;
extern MarkMatchGraph 	*mark_match_graph;   /* mark match graph(s) */
extern TreeNodeSet 	*tree_node_set;         /* set of current nodes */ 
extern int 		total_set_num;
extern TreeNode 	*tree_node;
extern TreeInfo 	tree_info;

/* Subroutine:	void opt_mark_dictionary_tree()
   Function:	optimize the mark dictionary for each page, using the 
   		tree-based design 
   Input:	none
   Output:	none
*/
void opt_mark_dictionary_tree(void)
{
  double a;
  build_trees();

  write_mark_op_buf();
  
  get_tree_info();
  get_dict_size();
  printf("Current dictionary size is %d, input your target size:", 
  	cur_dict_size);
  scanf("%d", &tar_dict_size);

  modify_mark_op_buf();
  get_cost_as_leaf();
  
  printf("Current dictionary size is %d, target size is %d, "
         "modifying trees...\n", cur_dict_size, tar_dict_size);
  modify_mark_match_tree();  
  free((void *)mark_match_graph->edges);
  free((void *)mark_match_graph);

  get_tree_info();

  tree_to_dictionary();
  
  mark_singletons();
  
  if(codec->lossy) modify_dictionary_marks();
    
  delete_perfect_dict_marks();  
  exclude_singletons();
  
  /* decide # bits needed to fixed length code the symbol IDs */
  a = ceil(log((double)dictionary->total_mark_num)/log(2.));
  symbol_ID_in_bits = (int)a;

  #ifdef NEVER
  /* not necessary since TREE dictionary can only be used in single-page mode */
  update_prev_dict_flags();
  #endif
  
  /* free previously allocated memory */
  free((void *)mark_match_tree);
}

extern void init_mark_structures();
void init_mark_op_buf();
extern void match_all_marks();
void read_mark_match_graph(), write_mark_match_graph();
extern void build_min_span_trees();
void print_single_node_list();
extern void decide_tree_root();
extern void build_tree_from_node(int, int);

/* Subroutine:	void build_trees()
   Function:	match all marks from this page and build minimum spanning 
   		trees for them
   Input:	none
   Output:	none
*/
void build_trees()
{
  char ans[100];
  register int i;
  int root;
  
  init_mark_structures();
  init_mark_op_buf();
  
  printf("Read mark match graph from a file?(y/n)");
  scanf("%s", ans);
  if(!strcmp(ans, "y") || !strcmp(ans, "yes")) 
    read_mark_match_graph();

  else {
    match_all_marks();
    printf("Write mark match graph into a file?(y/n)");
    scanf("%s", ans);
  
    if(!strcmp(ans, "y") || !strcmp(ans, "yes")) 
      write_mark_match_graph();
  }
    
  build_min_span_trees();
  
//  print_single_node_list();
  
  for(i = 0; i < total_set_num; i++) { 
    decide_tree_root(i, &root);
    if(root == -1)
      error("build_trees: illogical error, check code\n");
    root = tree_node_set[i].nodes[root];
    build_tree_from_node(root, -1);
  }
  
  free((void *)tree_node);  
  free((void *)tree_node_set);
}

/* initialize mark_op_buf, the offspring-parent relationship buffer 
   for each mark */
void init_mark_op_buf()
{
  register int i;

  mark_op_buf = (MarkOPBuf *)
    malloc(sizeof(MarkOPBuf)*mark_pointer_list->mark_num);
  if(!mark_op_buf)
    error("init_mark_op_buf: Cannot allocate memory\n");

  for(i = 0; i < mark_pointer_list->mark_num; i++) 
    mark_op_buf[i].match_num = 0;
}

/* since the process of matching all marks is relatively expensive, we often
   save the output mark_match_graph into a file for future use */
void write_mark_match_graph()
{
  FILE *fp;
  register int i;
  char fn[1000], ans[100];
  Edge *edge;

  printf("Input graph file suffix:");
  scanf("%s", ans);
  sprintf(fn, "%s/%s.%s", 
    	codec->report.data_path, codec->report.file_header, ans);

  fp = fopen(fn, "w");
  if(!fp) 
    error("write_mark_match_graph: cannot open mark match graph file\n");
   
  fwrite(&codec->mismatch_thres, sizeof(float), 1, fp);
  fwrite(&(mark_match_graph->total_edge_num), sizeof(int), 1, fp);
  
  edge = mark_match_graph->edges;
  for(i = 0; i < mark_match_graph->total_edge_num; i++) {
    fwrite(&(edge->vert1), sizeof(int), 1, fp);
    fwrite(&(edge->vert2), sizeof(int), 1, fp);
    fwrite(&(edge->mm_score), sizeof(float), 1, fp);
    edge++;
  }

  fclose(fp);
}

/* if the mark_match_graph has been formed and saved before, we can read it 
   directly from a file */
void read_mark_match_graph()
{
  FILE *fp;
  register int i;
  char fn[1000], ans[100];
  Edge *edge;
  float tmp_mm;
  
  printf("Input graph file suffix:");
  scanf("%s", ans);
  sprintf(fn, "%s/%s.%s", 
  	codec->report.data_path, codec->report.file_header, ans);

  fp = fopen(fn, "r");
  if(!fp) 
    error("read_mark_match_graph: cannot open mark match graph file\n");
   
  fread(&tmp_mm, sizeof(float), 1, fp);
  if(tmp_mm != codec->mismatch_thres)
    error("read_mark_match_graph: wrong mark match graph file\n");

  fread(&(mark_match_graph->total_edge_num), sizeof(int), 1, fp);
  edge = mark_match_graph->edges;
  for(i = 0; i < mark_match_graph->total_edge_num; i++) {
    fread(&(edge->vert1), sizeof(int), 1, fp);
    fread(&(edge->vert2), sizeof(int), 1, fp);
    fread(&(edge->mm_score), sizeof(float), 1, fp);
    edge++;
  }
  fclose(fp);
}

/* after the mark_match_graph is calculated or read, we write relevant
   information into mark_op_buf for later use */
void write_mark_op_buf()
{
  register int i;
  Edge *edge;
  MarkOPBuf *buf1, *buf2;
  
  edge = mark_match_graph->edges;
  for(i = 0; i < mark_match_graph->total_edge_num; i++) {
    buf1 = mark_op_buf + edge->vert1;
    buf2 = mark_op_buf + edge->vert2;
    if(buf1->match_num == MAX_MATCH_PER_MARK || 
       buf2->match_num == MAX_MATCH_PER_MARK)
      error("write_mark_op_buf: mark match buffer is full\n");
    buf1->op_info[buf1->match_num++].edge = edge;
    buf2->op_info[buf2->match_num++].edge = edge;
    edge++;
  }
}

int in_tree(int);

/* print out a list of single nodes (nodes that are not included in a tree) */
void print_single_node_list()
{
  register int i;
  int total_node_num;
  
  printf("Single nodes are:\n");
  total_node_num = mark_pointer_list->mark_num;
  
  for(i = 0; i < total_node_num; i++) 
    if(!in_tree(i)) printf("   %d", i);

  printf("\n");
}

/* check if a node belongs to any tree */
int in_tree(int node)
{
  register int i, j;
  TreeNodeSet *set;
  
  for(i = 0, set = tree_node_set; i < total_set_num; i++, set++) {
    for(j = 0; j < set->total_node_num; j++) 
      if(node == set->nodes[j]) return TRUE;
  }
  
  return FALSE;
}

/* calculate the current dictionary size given by the trees */
void get_dict_size()
{
  cur_dict_size = mark_pointer_list->mark_num;
  cur_dict_size -= tree_info.single_root_num;/* direct singletons don't count */
  cur_dict_size -= tree_info.leaf_num; /* refinement singletons don't count */
}

void get_offsprings(int, int *, int *);
void set_offsprings_in_buf(MarkOPBuf *, int *, int);
void reset_offsprings_in_buf(MarkOPBuf *, int *, int);
void set_parent_in_buf(MarkOPBuf *, int);
void reset_parent_in_buf(MarkOPBuf *, int);

/* modify the mark_op_buf, for each mark_match_tree node, set the offspring
   flags for all its offsprings in its mark_match_buf */
void modify_mark_op_buf()
{
  register int i;
  int *offsprings, offspring_num;
 
  offsprings = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!offsprings)
    error("modify_mark_op_buf: cannot allocate memory\n");

  for(i = 0; i < mark_pointer_list->mark_num; i++) {
    offspring_num = 0;
    get_offsprings(i, offsprings, &offspring_num);
    if(offspring_num)
      set_offsprings_in_buf(mark_op_buf+i, offsprings, offspring_num);
    set_parent_in_buf(mark_op_buf+i, mark_match_tree[i].parent);
  }

  free((void *)offsprings);
}

/* get all the offsprings of the input tree node */
void get_offsprings(int cur, int *offsprings, int *offspring_num)
{
  register int i;
  MarkMatchTree *node;
  
  node = mark_match_tree + cur;
  for(i = 0; i < node->child_num; i++) {
    offsprings[*offspring_num] = node->child[i];
    (*offspring_num)++;
    get_offsprings(node->child[i], offsprings, offspring_num);
  }
}

int is_offspring(int, int *, int);

/* mark all the offsprings of a certain mark in its mark_op_buf
   to avoid them being used when modifying the tree */
void set_offsprings_in_buf(MarkOPBuf *buf, int *offsprings, 
			  int offspring_num)
{
  register int i;
  OPInfo *op;
  int cur, ref;
  
  cur = buf-mark_op_buf;
  op = buf->op_info;
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1==cur ? op->edge->vert2:op->edge->vert1;
    if(is_offspring(ref, offsprings, offspring_num))
      op->parent_offspring |= 0x01;
  }
}

void reset_offsprings_in_buf(MarkOPBuf *buf, int *offsprings, 
	int offspring_num)
{
  register int i;
  OPInfo *op;
  int cur, ref;
  
  cur = buf-mark_op_buf;
  op = buf->op_info;
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1==cur ? op->edge->vert2:op->edge->vert1;
    if(is_offspring(ref, offsprings, offspring_num))
      op->parent_offspring &= 0xfe;
  }
}

int is_offspring(int ref, int *offsprings, int offspring_num)
{
  register int i;

  for(i = 0; i < offspring_num; i++)
    if(offsprings[i] == ref) return TRUE;

  return FALSE;
}

void set_parent_in_buf(MarkOPBuf *buf, int parent)
{
  register int i;
  OPInfo *op;
  int cur, ref;
  
  cur = buf-mark_op_buf;
  op = buf->op_info;
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1==cur ? op->edge->vert2:op->edge->vert1;
    if(ref == parent) {
      op->parent_offspring |= 0x02;
      break;
    }
  }
}

void reset_parent_in_buf(MarkOPBuf *buf, int parent)
{
  register int i;
  OPInfo *op;
  int cur, ref;
  
  cur = buf-mark_op_buf;
  op = buf->op_info;
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1==cur ? op->edge->vert2:op->edge->vert1;
    if(ref == parent) {
      op->parent_offspring &= 0xfd; /* 0x11111101 */
      break;
    }
  }
}

void init_cost_as_leaf();
void get_node_cost_as_leaf(int);

/* get the cost if each node is to be made a leaf, this cost is VERY_BIG if it is already a leaf */
void get_cost_as_leaf()
{
  register int i;
  
  init_cost_as_leaf();
  
  for(i = 0; i < mark_pointer_list->mark_num; i++) 
    get_node_cost_as_leaf(i);
}

void init_cost_as_leaf()
{
  register int i;
  
  cost_as_leaf = (CostAsLeaf *)
    malloc(sizeof(CostAsLeaf)*mark_pointer_list->mark_num);
  if(!cost_as_leaf)
    error("init_cost_as_leaf: Cannot allocate memory\n");
    
  for(i = 0; i < mark_pointer_list->mark_num; i++) 
    cost_as_leaf[i].child_num = mark_match_tree[i].child_num;
}

void get_next_best_match(int, RelocInfo *);
void check_elim_potential_loop(int);

/* this subroutine finds the next best match for each child of the input node,
   and therefore calculate the input's cost_as_leaf. Notice that we want 
   to avoid the problem of potential loops, i.e., if one child finds its
   sibling or its sibling's offspring as the next best match, and that sibling
   does the same thing, the later on when we try to make the input node a leaf,
   we will have a problem */
void get_node_cost_as_leaf(int cur)
{
  register int j;
  CostAsLeaf *cost_ptr;
  MarkMatchTree *node;

  cost_ptr = cost_as_leaf + cur;
  node = mark_match_tree + cur;
  
  /* if the node is a leaf, then its cost_as_leaf is VERY BIG */
  if(cost_ptr->child_num == 0) 
    cost_ptr->total_cost = VERY_BIG;

  /* if the node has only one child, then we won't have any potential loop */
  else if (cost_ptr->child_num == 1) {
    get_next_best_match(node->child[0], &(cost_ptr->reloc_info[0]));
    if(cost_ptr->reloc_info[0].new_ref == -1)
      cost_ptr->total_cost = VERY_BIG;
    else cost_ptr->total_cost = cost_ptr->reloc_info[0].mm_diff;
  }

  /* when the node has more than one child, after finding for each child the
     next best match, we check for potential loop(s) and eliminate it (them) */
  else {
    cost_ptr->total_cost = 0.;
    for(j = 0; j < cost_ptr->child_num; j++) {
      get_next_best_match(node->child[j], &(cost_ptr->reloc_info[j]));
      if(cost_ptr->reloc_info[j].new_ref == -1) {
        cost_ptr->total_cost = VERY_BIG;
        break;
      }
      else cost_ptr->total_cost += cost_ptr->reloc_info[j].mm_diff;
    }
    /* if the total_cost is already VERY_BIG, this node will never be picked
       as leaf candidate, and we are safe */
    if(cost_ptr->total_cost != VERY_BIG) 
      check_elim_potential_loop(cur);
  }
}

void is_an_offspring(int, int *offsprings[MAX_CHILD_NUM], int *, int, int *);
void get_excluded_nodes(int *, int *, int *, int);
int check_for_loop(int *, int, int *, int *, int *);
void get_next_best_match_excluded(int, int, RelocInfo *, int *, int);

/* the cost_as_leaf of a multiple-child node is more complicated to calculate,
   because its children could potentially find each other (or each other's
   offspring) as the next best match, which cause loop(s) in the future. We
   solve this problem by checking for any loops that exist and eliminating 
   all of them */
void check_elim_potential_loop(int cur)
{
  register int i;
  MarkMatchTree *cur_node;
  CostAsLeaf *cur_cost;
  RelocInfo temp_reloc, best_reloc;
  float penalty, smallest_penalty;
  int best_to_break;
  int *new_ref;
  int *excluded, excluded_num;
  int *offsprings[MAX_CHILD_NUM], offspring_num[MAX_CHILD_NUM];
  int *loop, ls, loop_size, loop_found;
  int chain[LONGEST_CHAIN];
  
  cur_node = mark_match_tree+cur;
  cur_cost = cost_as_leaf+cur;
  
  new_ref = (int *)malloc(sizeof(int)*cur_node->child_num);
  excluded = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);  
  if(!new_ref || !excluded)
    error("check_elim_potential_loop: Cannot allocate memory\n");
    
  /* get all the offsprings for each child */
  for(i = 0; i < cur_node->child_num; i++) {
    offsprings[i] = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
    if(!offsprings[i]) 
      error("check_elim_potential_loop: Cannot allocate memory\n");
    offspring_num[i] = 0;
    get_offsprings(cur_node->child[i], offsprings[i], &offspring_num[i]);
    offsprings[i][offspring_num[i]++] = cur_node->child[i];
  }
  
  do {
    for(i = 0; i < cur_node->child_num; i++) {
      is_an_offspring(cur_cost->reloc_info[i].new_ref, offsprings, 
      	offspring_num, cur_node->child_num, &new_ref[i]);
    }
    
    loop_found = check_for_loop(new_ref, cur_node->child_num, 
    	chain, &ls, &loop_size);
    
    if(loop_found) {
      loop = chain + ls;
      /* first gather the nodes that should be excluded from further
         consideration to avoid any loop */
      excluded_num = 0;
      for(i = 0; i < loop_size; i++) 
        get_excluded_nodes(excluded+excluded_num, &excluded_num, 
		offsprings[loop[i]], offspring_num[loop[i]]);

      /* break the loop, for each node in the loop, find something not 
         in the loop and compare the penalty incurred, pick one with 
	 the smallest penalty */
      smallest_penalty = VERY_BIG;
      for(i = 0; i < loop_size; i++) {
        get_next_best_match_excluded(cur_node->child[loop[i]], 
		cur_cost->reloc_info[loop[i]].new_ref,
		&temp_reloc, excluded, excluded_num);
	
	if(temp_reloc.new_ref != -1) {
	  penalty = temp_reloc.mm_diff-cur_cost->reloc_info[loop[i]].mm_diff;
	  if(penalty < 0) 
	    error("check_elim_potential_loop: illogical error, check code!\n");
	 
	  if(penalty < smallest_penalty) {
	    best_to_break = loop[i];
	    smallest_penalty = penalty;
	    best_reloc = temp_reloc;
	  }
        }
      }
      
      /* if a best breaking point is found, modify corresponding 
      	 new_ref information */
      if(smallest_penalty != VERY_BIG) {
        cur_cost->reloc_info[best_to_break] = best_reloc;
        cur_cost->total_cost += smallest_penalty;
      }
      /* if no alternative reference can be found for any node in loop, then
         we declare the loop-elimination attempt has failed and this node will
	 have VERY BIG total cost and cannot be considered for leaf */
      else {
        cur_cost->total_cost = VERY_BIG;
	return;
      }
    }
  } while(loop_found);
  
  free((void *)new_ref);
  free((void *)excluded);
  for(i = 0; i < cur_node->child_num; i++)
    free((void *)offsprings[i]);
}

void is_an_offspring(int ref, int *offsprings[MAX_CHILD_NUM], 
	int offspring_num[MAX_CHILD_NUM], int child_num, int *which)
{
  register int i;
  
  for(i = 0; i < child_num; i++)
    if(is_offspring(ref, offsprings[i], offspring_num[i])) {
      *which = i;
      return;
    }
  
  *which = -1;
}

void get_excluded_nodes(int *excl, int *excl_num, 
	int *offsprings, int offspring_num)
{
  register int i;
  
  for(i = 0; i < offspring_num; i++)
    excl[i] = offsprings[i];
    
  (*excl_num) += offspring_num;
}

int in_chain(int, int *, int, int *);

int check_for_loop(int *ref, int ref_num, int *chain, 
	int *ls, int *loop_size)
{
  register int i;
  int chain_size;
  
  for(i = 0; i < ref_num; i++)
    if(ref[i] != -1) {
      chain[0] = ref[i]; chain_size = 1;
      while(chain[chain_size-1] != -1 && 
            !in_chain(chain[chain_size-1], chain, chain_size-1, ls)) { 
        chain[chain_size] = ref[chain[chain_size-1]];
	chain_size++;
      }
      if(chain[chain_size-1] != -1) {
       *loop_size = chain_size-(*ls)-1;
        return TRUE;
      }
    }
  
  return FALSE;
}

int in_chain(int node, int *chain, int chain_size, int *ls) 
{
  register int i;

  for(i = 0; i < chain_size; i++)
    if(chain[i] == node) {
      *ls = i;
      return TRUE;
    }
    
  return FALSE;
}

void get_next_best_match(int cur, RelocInfo *info)
{
  register int i;
  MarkOPBuf *buf;
  float new_mm, old_mm;
  OPInfo *op;
  int ref;
  
  /* search the mark match buffer for the best usable reference */
  buf = mark_op_buf + cur;
  op = buf->op_info;
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1 == cur ? op->edge->vert2:op->edge->vert1;
    if(!op->parent_offspring && mark_match_tree[ref].child_num) break;
  }
  
  if(i < buf->match_num) { /* if such a reference is found */
    new_mm = op->edge->mm_score;
    old_mm = mark_match_tree[cur].mm_score;
    info->mm_diff = new_mm-old_mm;
    info->new_ref = ref;
  }
  else {
    info->mm_diff = VERY_BIG;
    info->new_ref = -1;
  }
}

int is_excluded(int, int *, int);

void get_next_best_match_excluded(int cur, int parent, RelocInfo *info, 
	int *excl, int excl_num)
{
  register int i;
  MarkOPBuf *buf;
  float new_mm, old_mm;
  OPInfo *op;
  int ref;
  
  /* search the mark match buffer for the best usuble reference */
  buf = mark_op_buf + cur;
  op = buf->op_info;
  
  /* we need not (in fact can not) consider anything before the mark's 
     current new parent, because all those must have been looked at already,
     and by looking at them again we may get into endless loop */
  for(i = 0; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1 == cur ? op->edge->vert2:op->edge->vert1;
    if(ref == parent) break;
  }
   
  i++; op++;
  for(; i < buf->match_num; i++, op++) {
    ref = op->edge->vert1 == cur ? op->edge->vert2:op->edge->vert1;
    if(ref != parent && !op->parent_offspring &&
       mark_match_tree[ref].child_num &&
       !is_excluded(ref, excl, excl_num)) break;
  }
  
  if(i < buf->match_num) { /* if such a reference is found */
    new_mm = op->edge->mm_score;
    old_mm = mark_match_tree[cur].mm_score;
    info->mm_diff = new_mm-old_mm;
    info->new_ref = ref;
  }
  else {
    info->mm_diff = VERY_BIG;
    info->new_ref = -1;
  }
}

int is_excluded(int ref, int *excl, int excl_num)
{
  register int i;

  for(i = 0; i < excl_num; i++)
    if(excl[i] == ref) return TRUE;

  return FALSE;
}

void get_min_cost_as_leaf(int *);
void get_next_leaf_cand(int *);
void find_affected_ascendents(int, int, int *, int *);
void update_old_parent_ascendent_cost(int *, int, int *, int);
void update_old_parent_ascendent_buf(int *, int, int *, int);
void update_new_parent_ascendent_cost(int *, int, int *, int);
void update_new_parent_ascendent_buf(int *, int, int *, int);
void new_leaf_generated(int);
void add_child(int, int);
void delete_child(int, int);

/* modify the mark_match_tree to produce a dictionary with the desired size,
   by relocating some of the matching branches, note it is possible that the
   desired size cannot be reached, i.e. no more branches can be moved, in that
   case a dictionary with the smallest possible size will be generated */
void modify_mark_match_tree()
{
  register int i;
  int next, new_parent;
  RelocInfo *reloc_info;
  int *child, child_num;
  int *offsprings, offspring_num;
  int *af_asc, af_asc_num;
  
  offsprings = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  af_asc = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!offsprings || !af_asc) 
    error("modify_mark_match_tree: cannot allocate memory\n");
  
  while(cur_dict_size > tar_dict_size) {
    get_min_cost_as_leaf(&next);
//    get_next_leaf_cand(&next);
    if(next == -1) {/* this means no more nodes can be relocated */
      printf("Unable to reduce the dictionary to target size, "
             "smallest size is %d\n", cur_dict_size);
      break;
    }
    
    /* if we can find the best leaf candidate, we move its children 
       to their best new parents, doing so involves multiple issues,
       for all children and their new parents, this means the following:
       1. change all children's parent node pointers
       2. update all children's mark_op_buf(set and reset its parent field)
       3. add new children to the new parents
       4. since the new parents' offspring relationship has changed, we need
          to modify its own mark_op_buf. Besides, some of its 
	  ascendents' have new offsprings too, therefore whose mark_op_buf
	  needs to be updated too
       5. the new parents' and some of its descendents' cost_as_leaf needs to 
          be changed
       
       for the old parent, it means the following:
       1. its child_num is 0
       2. its new cost_as_leaf is VERY_BIG
       3. its own and some of its ascendents' offspring relationship is
          changed, they have a bigger range of choice for the next best
	  match
       4. it now becomes a new leaf, so if any mark that has previously
          identified this one as next best reference has to find something
	  else 
    */
    child_num = cost_as_leaf[next].child_num;
    child = mark_match_tree[next].child;
    reloc_info = cost_as_leaf[next].reloc_info;
    
    new_leaf_generated(next);
    
    /* move all children to their new parents */
    for(i = 0; i < child_num; i++) {
      new_parent = reloc_info[i].new_ref;
      mark_match_tree[child[i]].parent = new_parent;
      mark_match_tree[child[i]].mm_score += reloc_info[i].mm_diff;
      reset_parent_in_buf(mark_op_buf+child[i], next);
      set_parent_in_buf(mark_op_buf+child[i], new_parent);
      
      add_child(new_parent, child[i]);
      cost_as_leaf[new_parent].child_num++;
    }
    
    for(i = 0; i < child_num; i++) {
      new_parent = reloc_info[i].new_ref;
      
      /* get this child's offsprings */
      offspring_num = 0;
      get_offsprings(child[i], offsprings, &offspring_num);
      offsprings[offspring_num++] = child[i];

      /* change its old parent's and some of its ascendents' cost and buffer 
         information */
      find_affected_ascendents(new_parent, next, af_asc, &af_asc_num);      
      update_old_parent_ascendent_buf(af_asc, af_asc_num, 
    	offsprings, offspring_num);
      update_old_parent_ascendent_cost(af_asc, af_asc_num, 
    	offsprings, offspring_num);

      /* change its new parent's and some of its ascendents' cost and buffer 
         information */
      find_affected_ascendents(next, new_parent, af_asc, &af_asc_num);
      update_new_parent_ascendent_buf(af_asc, af_asc_num, 
	offsprings, offspring_num);
      update_new_parent_ascendent_cost(af_asc, af_asc_num,
      	offsprings, offspring_num);
    }

    /* all these steps having been successfully carried out, we can now
       decrease the current dictionary size */
    cur_dict_size--;  
  }
  
  free((void *)offsprings);
  free((void *)af_asc);
  free((void *)cost_as_leaf);
}

/* from the cost_as_leaf buffer, find the node whose children can be moved 
   with the minimal cost */
void get_min_cost_as_leaf(int *next)
{
  register int i;
  float min_cost;
  
  min_cost = VERY_BIG; *next = -1;
  for(i = 0; i < mark_pointer_list->mark_num; i++) 
    if(cost_as_leaf[i].total_cost < min_cost) {
      *next = i;
      min_cost = cost_as_leaf[i].total_cost;
    }
}

/* randomly choose a node to be made leaf next, provided that this node CAN
   be made a leaf */
void get_next_leaf_cand(int *next)
{
  int cand;
  float tmp;
  
  while(1) {
    tmp = rand()/(RAND_MAX+1.);
    cand = (int)((float)mark_pointer_list->mark_num*tmp);
    if(cost_as_leaf[cand].total_cost != VERY_BIG) break;
  }
  
  *next = cand;
}

/* now we've generated one new leaf, therefore any node as a potential leaf 
   whose any child has previously chosen this new leaf as its next reference 
   has to find something else now */
void new_leaf_generated(int leaf)
{
  register int i, j;
  CostAsLeaf *cost;
  int leaf_referred;
  
  mark_match_tree[leaf].child_num = 0;
  cost_as_leaf[leaf].child_num = 0;
  cost_as_leaf[leaf].total_cost = VERY_BIG;

  cost = cost_as_leaf;
  for(i = 0; i < mark_pointer_list->mark_num; i++, cost++) {
    leaf_referred = FALSE;
    if(cost->total_cost != VERY_BIG) { 
      for(j = 0; j < cost->child_num; j++) 
        if(cost->reloc_info[j].new_ref == leaf) {
	  leaf_referred = TRUE; 
	  break;
	}
    }
    if(leaf_referred) get_node_cost_as_leaf(i);
  }
}

/* since all the children and their offsprings will be removed from the
   old parent, the old parent's own and possibly some of its ascendents' 
   cost_as_leaf will change, this means that the parent-child relationship 
   for some of the old parent's ascendents has changed, and they now have 
   a broader range of choice as their next best reference, to simplify 
   programming, we just recalculate all the costs affected.
*/
void update_old_parent_ascendent_cost(int *af_asc, int af_asc_num,
 		   		  int *offsprings, int offspring_num)
{
  register int i;
  
  /* af_asc[0] is the old parent, we don't consider it, it is considered
     in the call routine */
  
  /* for each remaining affected ascendent but the last one, we recalculate 
     their parent's cost_as_leaf */
  for(i = 1; i < af_asc_num-1; i++)
    get_node_cost_as_leaf(mark_match_tree[af_asc[i]].parent);

  /* for the last one, more caution should be taken */
  if(mark_match_tree[af_asc[af_asc_num-1]].parent != -1) 
    get_node_cost_as_leaf(mark_match_tree[af_asc[af_asc_num-1]].parent);    
}

/* since the child and its offsprings are added to the new parent, 
   the new parent's and some of its ascendents' mark_op_buf entries 
   need to be updated */ 
void update_old_parent_ascendent_buf(int *af_asc, int af_asc_num,
				 int *offsprings, int offspring_num)
{
  register int i;
  
  for(i = 0; i < af_asc_num; i++) 
    reset_offsprings_in_buf(mark_op_buf+af_asc[i], 
          offsprings, offspring_num);
}

/* since a child has been added to the new parent, the new parent's own and 
   possibly some of its ascendents' cost_as_leaf will change, this has two 
   meanings:
   1. the new parent has one more child so its cost_as_leaf will increase
   2. the parent-child relationship for the new parent and some of its
      ascendents has changed, so if some of them had chosen any new
      offspring as their next best reference before, they cannot do it now 
*/
void update_new_parent_ascendent_cost(int *af_asc, int af_asc_num,
 		   		  int *offsprings, int offspring_num)
{
  register int i;
  
  /* we know the new parent and its ascendents are not leaf, therefore 
     if their cost_as_leaf is already VERY BIG, then adding some new 
     child/offspring doesn't change anything */
  for(i = 0; i < af_asc_num; i++)
    if(cost_as_leaf[af_asc[i]].total_cost != VERY_BIG) 
      get_node_cost_as_leaf(af_asc[i]);

  if(mark_match_tree[af_asc[af_asc_num-1]].parent != -1)    
    get_node_cost_as_leaf(mark_match_tree[af_asc[af_asc_num-1]].parent);
}

/* since the child and its offsprings are added to the new parent, 
   the new parent's and some of its ascendents' mark_op_buf entries 
   need to be updated */ 
void update_new_parent_ascendent_buf(int *af_asc, int af_asc_num,
				 int *offsprings, int offspring_num)
{
  register int i;
  
  for(i = 0; i < af_asc_num; i++) 
    set_offsprings_in_buf(mark_op_buf+af_asc[i], 
          offsprings, offspring_num);
}

int is_p1asc(int, int *, int);

/* this subroutine finds a series of tree nodes starting from p2 and going
   up to, but not including, the common ascendent as the other node p1 */ 
void find_affected_ascendents(int p1, int p2, int *af_asc, int *af_asc_num)
{
  int *p1_asc, p1_asc_num;
  int cur_asc;
  
  p1_asc = (int *)malloc(sizeof(int)*mark_pointer_list->mark_num);
  if(!p1_asc)
    error("find_affected_ascendents: cannot allocate memory\n");
  
  cur_asc = p1;
  p1_asc[0] = cur_asc; p1_asc_num = 1; 
  while(cur_asc != -1) 
    p1_asc[p1_asc_num++] = cur_asc = mark_match_tree[cur_asc].parent;
  if(cur_asc == -1) p1_asc_num--;
  
  af_asc[0] = cur_asc = p2; *af_asc_num = 1;
  while(!is_p1asc(cur_asc, p1_asc, p1_asc_num) && (cur_asc != -1)) 
    af_asc[(*af_asc_num)++] = cur_asc = mark_match_tree[cur_asc].parent;
  
  /* if p2 is p1's ascendent, then af_asc_num is 1, in which case af_asc buffer       should have at least one node, i.e., p2 itself */
  if((*af_asc_num) > 1) (*af_asc_num)--;
  
  free((void *)p1_asc);
}

int is_p1asc(int cur_asc, int *p1asc, int p1asc_num)
{
  register int i;
  
  for(i = 0; i < p1asc_num; i++) 
    if(cur_asc == p1asc[i]) return TRUE;
    
  return FALSE;
}

/* add a child to a parent */
void add_child(int parent, int child)
{
  MarkMatchTree *node;

  node = mark_match_tree + parent;
  if(node->child_num == MAX_CHILD_NUM)
    error("add_child: children buffer is full\n");
  node->child[node->child_num++] = child;
}

/* delete a child from a parent */
void delete_child(int parent, int child)
{
  register int i;
  MarkMatchTree *node;

  node = mark_match_tree + parent;
  /* find the child in the parent's children buffer */
  for(i = 0; i < node->child_num; i++) 
    if(node->child[i] == child) break;
  if(i == node->child_num)
    error("delete_child: illogical error, check code\n");

  /* remove child from parent's children buffer */
  for(; i < node->child_num-1; i++)
    node->child[i] = node->child[i+1];

  /* decrease the parent's child number */
  node->child_num--;
}

/* Subroutine:	void mark_singletons()
   Function:	mark out the singleton entries (tree leaves)
   Input:	none
   Output:	none
*/
void mark_singletons()
{
  register int i;
  DictionaryEntry *entry;
  
  entry = dictionary->entries;
  for(i = 0; i < dictionary->total_mark_num; i++, entry++) 
    entry->singleton = TRUE;
  
  entry = dictionary->entries;
  for(i = 0; i < dictionary->total_mark_num; i++, entry++)
    if(entry->ref_index != -1) 
      dictionary->entries[entry->ref_index].singleton = FALSE;
}
