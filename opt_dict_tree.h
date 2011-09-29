
#define MAX_EDGE_NUM 200000

typedef struct {
  int vert1, vert2;
  float mm_score;
} Edge;

typedef struct {
  int total_edge_num;
  Edge edges[MAX_EDGE_NUM];
} MarkMatchGraph;

#define MAX_TREE_NUM 2000
#define MAX_TREE_NODE_NUM 2000

typedef struct {
  int total_node_num;
  int nodes[MAX_TREE_NODE_NUM];
} TreeNodeSet;

#define MAX_NODE_DEGREE 200

typedef struct {
  int edge;
  int node;
} NodeEdgePair;
 
typedef struct {
  int degree;
  NodeEdgePair pairs[MAX_NODE_DEGREE];
} TreeNode;

#define MAX_CHILD_NUM 100

typedef struct {
  int parent;
  int child_num;
  int child[MAX_CHILD_NUM];
  float mm_score;   /* mismatch with its parent */
} MarkMatchTree;

typedef struct {
  int root_num;
  int leaf_num;
  int single_root_num;
  int one_child_node_num;
  int two_child_node_num;
  int mult_child_node_num;
  int biggest_node;
  float total_mm;
} TreeInfo;

#define MAX_MATCH_PER_MARK 800

typedef struct {
  Edge *edge;
  int parent_offspring;
} MatchInfo;

typedef struct {
  int match_num;
  MatchInfo match_info[MAX_MATCH_PER_MARK];
} MarkMatchBuf;

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
  int ref_group;
  int ref_mark;
  float mm_score;
} GroupMatchInfo;

#define BIGGEST_GROUP 	 1000

typedef struct {
  int total_entry_num;
  int entries[BIGGEST_GROUP];
  int leader;
} Group;

typedef struct {
  Group *groups;
  int group_num;
} GroupInfo;

#define LONGEST_CHAIN 500

typedef struct {
  int chain[LONGEST_CHAIN];
  int chain_size; 
  int loop_start, loop_end;
  int break_point;
} Chain;


