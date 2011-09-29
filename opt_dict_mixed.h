
#define MAX_EDGE_NUM 20000

typedef struct {
  int vert1, vert2;
  float mm_score;
} Edge;

typedef struct {
  int total_edge_num;
  Edge *edges;
} MarkMatchGraph;

#define MAX_CHILD_NUM 50

typedef struct {
  int parent;
  int child_num;
  int child[MAX_CHILD_NUM];
  float mm_score;   /* mismatch with its parent */
} MarkMatchTree;

#define MAX_TREE_NUM 	  500
#define MAX_TREE_NODE_NUM 100

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

