#define MAX_EQUI_CLASS_NUM 	2000
#define MAX_EQUI_CLASS 		200
//#define MAX_EQUI_CLASS 	500

typedef struct {
  int entries[MAX_EQUI_CLASS];
  int total_entry_num;    /* # of entry in this class */
  int repre;
} EquiClass;

typedef struct {
  int ref_mark;
  float mm_score;
} MatchInfo;

#define MAX_EDGE_NUM 20000

typedef struct {
  int vert1, vert2;
  float mm_score;
} Edge;

typedef struct {
  int total_edge_num;
  Edge *edges;
} MarkMatchGraph;

#define MAX_MATCH_PER_MARK 50 

typedef struct {
  int match_num;
  MatchInfo match_info[MAX_MATCH_PER_MARK];
} MarkMatchBuf;

#define MAX_CHILD_NUM 100
//#define MAX_CHILD_NUM 400

typedef struct {
  int parent;
  int child_num;
  int child[MAX_CHILD_NUM];
  float mm_score;   /* mismatch with its parent */
} MarkMatchTree;

#define MAX_TREE_NUM 	  500
#define MAX_TREE_NODE_NUM 400 
//#define MAX_TREE_NODE_NUM 1000

typedef struct {
  int total_node_num;
  int nodes[MAX_TREE_NODE_NUM];
} TreeNodeSet;

#define MAX_NODE_DEGREE 50

typedef struct {
  int edge;
  int node;
} NodeEdgePair;
 
typedef struct {
  int degree;
  NodeEdgePair pairs[MAX_NODE_DEGREE];
} TreeNode;

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

typedef struct {
  Mark *marks[MAX_MARK_NUM];
  int mark_num;
} MarkPointerList;

#define LONGEST_CHAIN 500

typedef struct {
  int chain[LONGEST_CHAIN];
  int chain_size;
  int loop_start, loop_end;
  int break_point;
} Chain;
