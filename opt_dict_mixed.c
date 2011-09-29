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

extern Dictionary 	*dictionary;
extern MarkList		*all_marks;
extern int		symbol_ID_in_bits;
extern int		prev_dict_size;
extern EquiClass 	*equi_class;
extern int 		equi_class_num;
extern MarkPointerList 	*mark_pointer_list; 
extern MarkMatchTree 	*mark_match_tree; 

static int max_edge_num;

MarkMatchGraph *mark_match_graph;   /* mark match graph(s) */
TreeNodeSet *tree_node_set;         /* set of current nodes */ 
int total_set_num;
TreeNode *tree_node;
TreeInfo tree_info;

void opt_mark_dictionary_mixed(void);
void match_class_repre(void);
void get_tree_info(void);
void update_prev_dict_flags(void);

void find_speck_in_dict(void);
void find_in_equi_class(int);
void find_in_match_graph(int);
void find_in_node_set(int);
void calc_tree_weight(void);

extern void tree_to_dictionary(void);
extern void form_equivalence_classes(void);
extern void modify_dictionary_marks(void);
extern void delete_perfect_dict_marks(void);
extern void exclude_singletons(void);
extern void add_word_to_dictionary(int, int, int);
extern void add_mark_to_dictionary(int, int, int);
extern void error(char *);

extern Codec *codec;
extern MarkList *all_marks;
extern WordList *all_words;

/* Subroutine:	void opt_mark_dictionary_mixed()
   Function:	optimize the mark dictionary for each page, using the combined 
   		class-based and tree-based design 
   Input:	none
   Output:	none
*/
void opt_mark_dictionary_mixed()
{
  double a;
  
  form_equivalence_classes();
  
  match_class_repre();
  
  get_tree_info();
  
  tree_to_dictionary();

//  find_speck_in_dict();

  if(codec->lossy) modify_dictionary_marks();
    
  delete_perfect_dict_marks();  
  exclude_singletons();
  
  /* decide # bits needed to fixed length code the symbol IDs */
  a = ceil(log((double)dictionary->total_mark_num)/log(2.));
  symbol_ID_in_bits = (int)a;

  update_prev_dict_flags();
  
  /* free previously allocated memory */
  free((void *)equi_class);
  free((void *)mark_match_tree);
}

void init_mark_structures();
void match_all_marks();
void build_min_span_trees();
void decide_tree_root(int, int *);
void build_tree_from_node(int, int);

/* Subroutine:	void match_class_repre()
   Function:	decide the matching relationship between class representatives,
   		i.e., dictionary symbols, using the modified Kruscal's 
		algorithm
   Input:	none
   Output:	none
*/
void match_class_repre()
{
  register int i;
  int root;
  int prev_mark_as_root;
  
  init_mark_structures();

  match_all_marks();
    
  build_min_span_trees();
  
  prev_mark_as_root = 0;
  for(i = 0; i < total_set_num; i++) { 
    decide_tree_root(i, &root);
    if(root == -1)
      error("match_class_repre: illogical error, check code\n");
    root = tree_node_set[i].nodes[root];
    if(root < prev_dict_size) prev_mark_as_root++; 
    build_tree_from_node(root, -1);
  }

  free((void *)tree_node_set);  
  free((void *)tree_node);  

  free((void *)mark_match_graph->edges);
  free((void *)mark_match_graph);
}

void init_mark_pointer_list();
void init_mark_match_graph();
void init_mark_match_tree();

/* Subroutine:	void init_mark_structures()
   Function:	initialize all structure buffers needed for the tree 
   		construction process 
   Input:	none
   Output:	none
*/
void init_mark_structures()
{
  init_mark_pointer_list();
  init_mark_match_graph();
  init_mark_match_tree();
}

/* Subroutine:	void init_mark_pointer_list()
   Function:	initialize the mark_pointer_list structure
   Input:	none
   Output:	none
*/
void init_mark_pointer_list()
{
  register int i;
  int repre;
  EquiClass *cur_class;
  int direct_singleton;
  
  mark_pointer_list = (MarkPointerList *)malloc(sizeof(MarkPointerList)*1);
  if(!mark_pointer_list)
    error("init_mark_pointer_list: Cannot allocate memory\n");
  
  /* for MIXED or CLASS dictionary, copy only equi class representatives */
  if(codec->dict_type == MIXED || codec->dict_type == CLASS) {
    /* first add all pre-existing dictionary symbols */
    for(i = 0; i < prev_dict_size; i++) 
      mark_pointer_list->marks[i] = dictionary->entries[i].mark;
  
    /* now add class representatives that are from the new page */ 
    mark_pointer_list->mark_num = prev_dict_size;
    direct_singleton = 0;
    for(i = 0, cur_class = equi_class; i < equi_class_num; i++, cur_class++) {
      if(cur_class->total_entry_num > 1) {
        repre = cur_class->repre;
        if(repre >= prev_dict_size) {
          if(mark_pointer_list->mark_num == MAX_MARK_NUM) 
            error("init_mark_pointer_list: Pointer buffer is full\n");

          mark_pointer_list->marks[mark_pointer_list->mark_num++] =
      	    all_marks->marks + (repre-prev_dict_size);
        }
      }
      else direct_singleton++;
    }
  
    #ifdef DEBUG
    printf("Number of direct singletons in this page is about %d\n", 
      direct_singleton);
    #endif
  }
  
  /* for TREE dictionary, copy all extracted marks */
  else if(codec->dict_type == TREE) {
    mark_pointer_list->mark_num = all_marks->mark_num;
    for(i = 0; i < all_marks->mark_num; i++)
      mark_pointer_list->marks[i] = all_marks->marks + i;
  }
}

/* Subroutine:	void init_mark_match_graph()
   Function:	initialize the mark_match_graph
   Input:	none
   Output:	none
*/
void init_mark_match_graph()
{
  mark_match_graph =(MarkMatchGraph *)malloc(sizeof(MarkMatchGraph)*1);
  if(!mark_match_graph)
    error("init_mark_match_graph: Cannot allocate memory\n");
  
  /* We roughly estimate the maximum size of the edge buffer this way: 
     for English or other Western language text, suppose there are 40 
     sets of symbols (for English alone 26 letters + puctuation marks). 
     Inside each set each symbol is matched with all others, and there is
     no match between sets. Each set has roughly the same size */
  max_edge_num = (mark_pointer_list->mark_num/40);
  max_edge_num = max_edge_num*max_edge_num;
  max_edge_num *= 40;
  
  mark_match_graph->edges = (Edge *)malloc(sizeof(Edge)*max_edge_num);
  if(!mark_match_graph)
    error("init_mark_match_graph: Cannot allocate memory\n");

  mark_match_graph->total_edge_num = 0;
}

/* Subroutine:	void init_mark_match_tree()
   Function:	initialize the mark_match_tree
   Input:	none
   Output:	none
*/
void init_mark_match_tree()
{
  register int i;
  
  mark_match_tree = (MarkMatchTree *)
    malloc(sizeof(MarkMatchTree)*mark_pointer_list->mark_num);
  if(!mark_match_tree)
    error("init_mark_match_tree: Cannot allocate memory\n");
  
  for(i = 0; i < mark_pointer_list->mark_num; i++) {
    mark_match_tree[i].parent = -1;
    mark_match_tree[i].child_num = 0;
    mark_match_tree[i].mm_score = 1.;
  }
}

void add_an_edge(int, int, float);
void sort_all_edges(void);
void find_biggest_edge(void);
static int biggest_edge = -1;

/* Subroutine:	void match_all_marks()
   Function:	match all the dictionary marks, including marks from the 
   		previous dictionary and new representatives from the current
		page. The matching graphs are written into mark_match_graph, 
		which contains at most "max_edge_num" connecting edges. All 
		these edges have a weight lower than the preset threshold. 
		They are sorted by weights into ascending order.
   Input:	none
   Output:	none
*/
void match_all_marks()
{
  register int i, j;
  Mark *cur, *ref;
  float mm_score;

  /* match only new marks from this page with all other marks, existing
     dictionary marks need not to be matched */
  for(i = prev_dict_size; i < mark_pointer_list->mark_num; i++) {
    cur = mark_pointer_list->marks[i];
    for(j = 0; j < i; j++) {
      ref = mark_pointer_list->marks[j];
      if(codec->prescreen_two_marks(cur, ref)) {
        mm_score = codec->match_two_marks(cur, ref);
        if(mm_score < codec->mismatch_thres) 
	  add_an_edge(i, j, mm_score);
      }
    }
  }
  
  sort_all_edges();
}

/* Subroutine:	void add_an_edge()
   Function:	add a new edge. If the edge buffer in mark_match_graph has
    		not been filled up yet, just add the new edge at the end;
		otherwise, compare the current biggest edge with the new edge 
		and replace it with the new one if the new one carry a smaller
		weight
   Input:	parameters specifying the new edge
   Output:	none
*/ 
void add_an_edge(int v1, int v2, float score)
{
  Edge *edge;
  
  if(mark_match_graph->total_edge_num == max_edge_num) {
     if(biggest_edge == -1)
       printf("Warning: edge buffer is already full, "
       	      "will have to sacrifice existing edges\n");
        
     find_biggest_edge();
     edge = mark_match_graph->edges + biggest_edge;
     if(edge->mm_score > score) {
       edge->vert1 = v1; edge->vert2 = v2; edge->mm_score = score;
     }
  }
  
  else {
    edge = mark_match_graph->edges + mark_match_graph->total_edge_num;
    edge->vert1 = v1; edge->vert2 = v2; edge->mm_score = score;

    mark_match_graph->total_edge_num++;
  }
}

/* Subroutine:	void sort_all_edges()
   Function:	sort all the edges in mark_match_graph by their weights into
   		increasing order
   Input:	none
   Output:	none
*/
void sort_all_edges()
{
  register int i, j;
  Edge temp;
  int total_edge_num;
  Edge *edges;
  
  total_edge_num = mark_match_graph->total_edge_num;
  edges = mark_match_graph->edges;
  for(i = 0; i < total_edge_num-1; i++)
    for(j = total_edge_num-1; j >= i+1; j--)
      if(edges[j].mm_score < edges[j-1].mm_score) {
	temp = edges[j];
	edges[j] = edges[j-1];
	edges[j-1] = temp;
     }  
}

/* Subroutine:	void find_biggest_edge()
   Function:	when the edge buffer in mark_match_graph is filled up, 
   		a pointer is used to record the position of the edge with
		the biggest weight. This way next time the new edge is 
		computed, it is compared against this biggest_edge. If
		the new edge is even bigger, we do nothing; otherwise it
		replaces the current biggest edge. 
   Input:	none
   Output:	none
*/
void find_biggest_edge()
{
  register int i;
  int total_edge_num;
  Edge *edge;
  int biggest_weight;
  
  total_edge_num = mark_match_graph->total_edge_num;
  edge = mark_match_graph->edges;
  biggest_weight = edge->mm_score;
  biggest_edge = 0; edge++;
  for(i = 1; i < total_edge_num; i++, edge++) 
    if(edge->mm_score >= biggest_weight) {
      biggest_weight = edge->mm_score;
      biggest_edge = i;
    }
}

void init_tree_node_sets();
void init_tree_nodes();
int edge_introduce_loop(Edge *, int *, int *);
void add_node_edge_pair(int, int, int);
void merge_tree_node_set(TreeNodeSet *, TreeNodeSet *);
void move_tree_node_set(TreeNodeSet *, TreeNodeSet *);

/* Subroutine:	void build_min_span_trees()
   Function:	build minimum spanning tree(s) from the matching graph(s), 
   		using the modified Kruscal's algorithm
   Input:	none
   Output:	none
*/
void build_min_span_trees()
{
  register int i;
  int v1set, v2set;
  int usable_set[MAX_TREE_NUM];
  int usable_set_num;
  Edge *edge;
  TreeNodeSet *set1, *set2;
  int total_mark_num, biggest_tree;
  
  init_tree_node_sets();
  init_tree_nodes();
  
  usable_set_num = 0;
  edge = mark_match_graph->edges;
  for(i = 0; i < mark_match_graph->total_edge_num; i++) {
    if(!edge_introduce_loop(edge, &v1set, &v2set)) {
      if(v1set == v2set) 
      {/* note v1set = v2set = -1 must be TRUE */
        /* we know a new set has been established */
	      if(usable_set_num) 
          set1 = tree_node_set + usable_set[--usable_set_num];	
	      else  {
  	      if(total_set_num == MAX_TREE_NUM)
	          error("build_min_span_tree: tree_node_set buffer is full\n");
	        set1 = tree_node_set + total_set_num;
	      total_set_num++;
	      }
	
	set1->total_node_num = 2;
	set1->nodes[0] = edge->vert1;
	set1->nodes[1] = edge->vert2;
      }
      else if(v1set != -1 && v2set != -1) {
        /* these two vertices lie in different sets. need to merge them */
	set1 = tree_node_set + (v1set < v2set ? v1set:v2set);
	set2 = tree_node_set + (v1set > v2set ? v1set:v2set);
	
	merge_tree_node_set(set1, set2);
	usable_set[usable_set_num++] = set2-tree_node_set;
      }
      else {
	/* one of the vertices doesn't belong to a set yet, add it to the set
	   the other vertex belongs to */ 
	set1 = tree_node_set + (v2set == -1 ? v1set:v2set);
	if(set1->total_node_num == MAX_TREE_NODE_NUM) 
	  error("build_min_span_tree: node buffer is full\n");
	
	set1->nodes[set1->total_node_num++] = 
	  v1set == -1 ? edge->vert1:edge->vert2;
      }
      
      add_node_edge_pair(edge->vert1, i, edge->vert2);
      add_node_edge_pair(edge->vert2, i, edge->vert1);
    }
    
    edge++;
  }
  
  while(usable_set_num > 0) {
    if(usable_set[--usable_set_num] < total_set_num)	{
      set1 = tree_node_set + usable_set[usable_set_num];
      if(set1->total_node_num)
        error("build_min_span_trees: illogical error, check code\n");
      set2 = tree_node_set + (--total_set_num);
      while(set2->total_node_num == 0) 
      	set2 = tree_node_set + (--total_set_num);
      move_tree_node_set(set1, set2);
    }
  }
  
  total_mark_num = 0; biggest_tree = 0;
  for(i = 0; i < total_set_num; i++) {
    if(tree_node_set[i].total_node_num == 0) 
      error("build_min_span_trees: illogical error, check code\n");
    total_mark_num += tree_node_set[i].total_node_num;
    if(tree_node_set[i].total_node_num > biggest_tree)
      biggest_tree = tree_node_set[i].total_node_num;
  }
  
  #ifdef DEBUG
  printf("%d symbols included in %d tree, total symbol number is %d\n",
    total_mark_num, total_set_num, mark_pointer_list->mark_num);
  printf("biggest tree has %d nodes\n", biggest_tree);
  #endif
}

/* Subroutine:	void init_tree_node_sets()
   Function:	initialize the tree_node_set structure
   Input:	none
   Output:	none
*/
void init_tree_node_sets()
{
  register int i;

  tree_node_set = (TreeNodeSet *)malloc(sizeof(TreeNodeSet)*MAX_TREE_NUM);
  if(!tree_node_set) 
    error("init_tree_node_sets: Cannot allocate memory\n");
      
  for(i = 0; i < MAX_TREE_NUM; i++) 
    tree_node_set[i].total_node_num = 0;
  
  total_set_num = 0;
}

/* Subroutine:	void init_tree_nodes()
   Function:	initialize the tree_node structure 
   Input:	none
   Output:	none
*/
void init_tree_nodes()
{
  register int i;
  
  tree_node = (TreeNode *)malloc(sizeof(TreeNode)*mark_pointer_list->mark_num);
  if(!tree_node)
    error("init_tree_nodes: Cannot allocate memory\n");
    
  for(i = 0; i < mark_pointer_list->mark_num; i++) 
    tree_node[i].degree = 0;
}

int vert_in_set(int, TreeNodeSet *);
int set_has_existing_mark(int);

/* Subroutine:	int edge_introduce_loop()
   Function:	decide if the input edge will introduce a loop into an 
   		existing tree, i.e., if its two vertices lie in the same tree. 
		If not, return which set(s) the two vertices lie in
   Input:	the edge
   Output:	binary decision, and the sets the two vertices lie in (or -1)
*/
int edge_introduce_loop(Edge *edge, int *v1set, int *v2set) 
{
  register int i;
  int s1, s2;
  
  /* search for vert1 */
  for(i = 0; i < total_set_num; i++) 
    if(vert_in_set(edge->vert1, tree_node_set+i)) break;
  if(i == total_set_num) s1 = -1;
  else s1 = i;
  
  /* search for vert2 */
  for(i = 0; i < total_set_num; i++) 
    if(vert_in_set(edge->vert2, tree_node_set+i)) break;
  if(i == total_set_num) s2 = -1;
  else s2 = i;
  
  if(s1 == s2 && s1 == -1) 
  {
    *v1set = s1; *v2set = s2;
    return FALSE;
  }

  if(s1 == s2 && s1 != -1) return TRUE;
#if 0
  else if((s1 == -1) && (edge->vert1 < prev_dict_size) &&
          set_has_existing_mark(s2)) return TRUE;
  else if((s2 == -1) && (edge->vert2 < prev_dict_size) &&
          set_has_existing_mark(s1)) return TRUE;
  else if(set_has_existing_mark(s1) && 
          set_has_existing_mark(s2)) return TRUE;
  else {
    *v1set = s1; *v2set = s2;
    return FALSE;
  }
#endif
  else if((s1 == -1))
  {
    if((edge->vert1 < prev_dict_size) && set_has_existing_mark(s2)) 
    {
      return TRUE;
    }
    else {
      *v1set = s1; *v2set = s2;
      return FALSE;
    }
  }
  else if((s2 == -1))
  {
    if((edge->vert2 < prev_dict_size) && set_has_existing_mark(s1)) 
    {
      return TRUE;
    }
    else {
      *v1set = s1; *v2set = s2;
      return FALSE;
    }
  }
  else if(set_has_existing_mark(s1) && 
          set_has_existing_mark(s2)) return TRUE;
  else {
    *v1set = s1; *v2set = s2;
    return FALSE;
  }
}

/* Subroutine:	int vert_in_set()
   Function:	decide if the input node already exists in the input set
   Input:	the node and the set to be examined
   Output:	binary decision
*/
int vert_in_set(int v, TreeNodeSet *set)
{
  register int i;
  
  for(i = 0; i < set->total_node_num; i++) 
    if(set->nodes[i] == v) return TRUE;
    
  return FALSE;
}

/* Subroutine:	int set_has_existing_mark()
   Function:	decide if the input set contains a node representing
   		an existing dictionary mark 
   Input:	the node set to be examined
   Output:	binary decision
*/
int set_has_existing_mark(int S)
{
  register int i;
  TreeNodeSet *s;
  
  s = tree_node_set + S;
  for(i = 0; i < s->total_node_num; i++) 
    if(s->nodes[i] < prev_dict_size) return TRUE;
  
  return FALSE;
}

/* Subroutine:	void add_node_edge_pair()
   Function:	add a node-edge pair to the input node and increase its degree
   Input:	the current node, the node to be connected and the edge 
   		connecting them
   Output:	none
*/
void add_node_edge_pair(int cur, int edge, int node)
{
  NodeEdgePair *pair;
  
  if(tree_node[cur].degree == MAX_NODE_DEGREE) 
    error("add_node_edge_pair: node-edge pair buffer is full\n");
    
  pair = tree_node[cur].pairs + tree_node[cur].degree;
  
  pair->node = node; pair->edge = edge;
  tree_node[cur].degree++;
}

/* Subroutine:	void merge_tree_node_set()
   Function:	merge the nodes in set2 into set1. Used when two vertices 
   		belong to two different existing node sets.
   Input:	the 2 node sets to be merged
   Output:	none
*/
void merge_tree_node_set(TreeNodeSet *set1, TreeNodeSet *set2)
{
  register int i;
  
  if(set1->total_node_num+set2->total_node_num > MAX_TREE_NODE_NUM)
    error("merge_tree_node_set: node buffer MAX_TREE_NODE_NUM is full\n");
  
  for(i = 0; i < set2->total_node_num; i++) 
    set1->nodes[set1->total_node_num++] = set2->nodes[i];
  
  set2->total_node_num = 0;
}

/* Subroutine:	void move_tree_node_set()
   Function:	move the nodes in set2 into set1
   Input:	the source and dest node sets
   Output:	none
*/
void move_tree_node_set(TreeNodeSet *set1, TreeNodeSet *set2)
{
  register int i;
  
  for(i = 0; i < set2->total_node_num; i++) 
    set1->nodes[i] = set2->nodes[i];
  set1->total_node_num = set2->total_node_num;
  set2->total_node_num = 0;
}

/* Subroutine:  void decide_tree_root()
   Function:	after the minimum spanning trees are built, for each tree 
		we choose a root for it. For those trees that contain one
		(and only one) previous dictionary mark, this mark serves 
		as the root; for those trees that don't have previous marks,
		we choose arbitrarily any node with degree > 1
   Input:	the tree whose root is to be chosen
   Output:	the root chosen
*/
void decide_tree_root(int t, int *root)
{
  register int i;
  TreeNodeSet *set;
  int root_found;
  
  set = tree_node_set + t; root_found = FALSE;
  for(i = 0; i < set->total_node_num; i++) 
    if(set->nodes[i] < prev_dict_size) {
      if(!root_found) { root_found = TRUE; *root = i;}
      else error("decide_tree_root: illogical error, a tree can't have 2 previous dict marks\n");
    }
    
  if(root_found) return;
  else {
    if(set->total_node_num == 2) {
      *root = 0;
      return;
    }
    for(i = 0; i < set->total_node_num; i++) 
      if(tree_node[set->nodes[i]].degree > 1) {
        *root = i;
        return;
      }
  }
}

/* Subroutine:  void build_tree_from_node()
   Function: 	build tree recursively, going down from root downward, depth
   		first. Write parent/children/mismatch information along the way 
   Input:	the current node, and the current edge leading downwards
   Output:	none
*/
void build_tree_from_node(int node, int edge)
{
  register int i, j;
  MarkMatchTree *cur_node;
  TreeNode *t_node;
  
  cur_node = mark_match_tree + node;
  t_node = tree_node + node;
  
  /* for root node */
  if(edge == -1) {
    cur_node->parent = -1;
    cur_node->child_num = t_node->degree;
    cur_node->mm_score = 1.0;
  }
  /* for other nodes */
  else {
    if(mark_match_graph->edges[edge].vert1 == node)
      cur_node->parent = mark_match_graph->edges[edge].vert2;
    else cur_node->parent = mark_match_graph->edges[edge].vert1;
    cur_node->child_num = t_node->degree-1;
    if(cur_node->child_num > MAX_CHILD_NUM) 
      error("build_tree_from_node: Child buffer is full\n");
    cur_node->mm_score = mark_match_graph->edges[edge].mm_score;
  }
  
  for(i = 0, j = 0; i < t_node->degree; i++) {
    if(t_node->pairs[i].edge != edge) {
      cur_node->child[j] = t_node->pairs[i].node;
      build_tree_from_node(t_node->pairs[i].node, t_node->pairs[i].edge);
      j++;
    }
  }
}

/* Subroutine:	void get_tree_info()
   Function:	traverse the tree built and print out tree information 
   Input:	none
   Output:	none
*/
void get_tree_info()
{
  register int i;
  MarkMatchTree *node;
  
  tree_info.leaf_num = tree_info.root_num = tree_info.single_root_num = 0;
  tree_info.one_child_node_num = tree_info.two_child_node_num = 
    tree_info.mult_child_node_num = 0;
  tree_info.biggest_node = 0;
  node = mark_match_tree; tree_info.total_mm = 0.;
  for(i = 0; i < mark_pointer_list->mark_num; i++) {
    if(node->parent == -1) {
      tree_info.root_num++;
      if(node->child_num == 0) 
        tree_info.single_root_num++;
    }
    else {
      if(node->child_num == 0) tree_info.leaf_num++;
      else if(node->child_num == 1) tree_info.one_child_node_num++;
      else if(node->child_num == 2) tree_info.two_child_node_num++;
      else tree_info.mult_child_node_num++;
      if(node->child_num > tree_info.biggest_node) 
      	tree_info.biggest_node = node->child_num;
      tree_info.total_mm += node->mm_score;
    }
    node++;
  }
  
  #ifdef DEBUG
  printf("Altogether %d trees were built, %d of which are single-node trees\n",
    tree_info.root_num, tree_info.single_root_num);
  printf("Different node types: \n"
	 "\t %d leaf nodes\n"
	 "\t %d nodes with only 1 child\n"
	 "\t %d nodes with 2 children\n"
	 "\t %d nodes with more children\n",
	 tree_info.leaf_num, tree_info.one_child_node_num, 
	 tree_info.two_child_node_num, tree_info.mult_child_node_num);
  printf("Total mismatch cost in all trees is %.2f\n", tree_info.total_mm);
  printf("Biggest node has %d children\n", tree_info.biggest_node);
  #endif
}

/* Subroutine:	void update_prev_dict_flags()
   Function:	update the "singleton" flag in all previous dictionary entries,
   		those who don't serve as a root or as a class representative
		are declared singletons
   Input:	none
   Output:	none
*/
void update_prev_dict_flags()
{
  register int i;
  int repre;
  
  for(i = 0; i < prev_dict_size; i++)
    dictionary->entries[i].singleton = TRUE;
  
  for(i = 0; i < equi_class_num; i++) {
    if((repre = equi_class[i].repre) < prev_dict_size) 
      dictionary->entries[repre].singleton = FALSE;
  }
  
  for(i = 0; i < prev_dict_size; i++) {
    if(mark_match_tree[i].child_num) 
      dictionary->entries[i].singleton = FALSE;
  } 
}

#ifdef NEVER
int in_tree(int);

/* print out a list of single nodes (nodes that are not included in a tree) */
void print_single_node_list()
{
  register int i;
  int total_node_num;
  
  printf("Single nodes are:\n");
  total_node_num = dictionary->total_mark_num;
  
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
#endif

extern int is_speck(Mark *);

/* Subroutine:	void find_speck_in_dict(void)
   Function:	search the dictionary entries and print out those that are
   		specks
   Input:	none
   Output:	none
*/
void find_speck_in_dict(void)
{
  register int i;
  int total_speck_num;
    
  for(i = 0, total_speck_num = 0; i < dictionary->total_mark_num; i++) 
    if(is_speck(dictionary->entries[i].mark))
       total_speck_num++;
  
  printf("found %d specks in the dictionary\n", total_speck_num);
}

/* Subroutine:	void find_in_equi_class()
   Function:	find which equi_class a particular mark belongs to 
   Input:	input mark
   Output:	none
*/
void find_in_equi_class(int mark)
{
  register int i, j;
  EquiClass *cur_class;
  
  for(i = 0, cur_class = equi_class; i < equi_class_num; i++, cur_class++) {
    for(j = 0; j < cur_class->total_entry_num; j++) 
      if(mark == cur_class->entries[j]) break; 
    if(j < cur_class->total_entry_num) break;
  }
  
  printf("input mark %d is in class %d, entry %d\n", mark, i, j); 
}

/* Subroutine:	void find_in_match_graph()
   Function:	find in which edges the input mark shows up
   Input:	input mark
   Output:	none
*/
void find_in_match_graph(int mark)
{
  register int i;
  Edge *edge;
  int total_edge_num;
  
  printf("Input mark %d belongs to these edges: ", mark);
  total_edge_num = mark_match_graph->total_edge_num;
  edge = mark_match_graph->edges; 
  for(i = 0; i < total_edge_num; i++, edge++) {
    if(mark == edge->vert1 || mark == edge->vert2) 
      printf("%d   ", i); 
  }
  
  printf("\n"); 
}

/* Subroutine:	void find_in_node_set()
   Function:	find to which tree_node_set a particular mark belongs 
   Input:	input mark
   Output:	none
*/
void find_in_node_set(int mark)
{
  register int i, j;
  TreeNodeSet *set;
  
  for(i = 0, set = tree_node_set; i < total_set_num; i++, set++) {
    for(j = 0; j < set->total_node_num; j++) 
      if(mark == set->nodes[j]) break; 
    if(j < set->total_node_num) break;
  }
  
  printf("Input mark %d belongs to tree_node_set %d\n", mark, i); 
}

/* Subroutine:	void calc_tree_weight()
   Function:	parse mark_match_tree and calculate its overall weigth 
   		(mismatch)
   Input:	none
   Output:	none
*/
void calc_tree_weight()
{
  register int i;
  MarkMatchTree *node;
  float weight;
  
  weight = 0.; node = mark_match_tree;
  for(i = 0; i < mark_pointer_list->mark_num; i++, node++) 
    if(node->parent != -1) weight += node->mm_score;
    
  printf("The overall weight mark_match_tree carries is %.2f\n", weight);
}
