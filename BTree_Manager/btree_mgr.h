#ifndef BTREE_MGR_H
#define BTREE_MGR_H

#include "dberror.h"
#include "tables.h"

// structure for accessing btrees
typedef struct NodeInformation{
  int *mgmtInfo;
  int completed;
  int capacity;
  int *keys;
  int *values;
  int numKeys;
  int isInitialized;  // Initialization flag to track setup status
  int *nodeArray; 
  int maxNodes; 
}NodeInformation;

typedef struct BPlusTree{
  int noOfNodes;
  struct BPlusTree *inhrt;
  int checkForChildren;
  NodeInformation *childRight;
  struct BPlusTree **arrNode;
  NodeInformation *nodeFields;
  NodeInformation *nodeBrnch;
  struct BPlusTree *nodeRight;
  NodeInformation *childLeft;
  struct BPlusTree *nodeLeft;
  struct BPlusTree *parent;
}BPlusTree;

typedef struct BTreeHandle {
  DataType keyType;
  char *idxId;
  void *mgmtData;
  int numberOfChildren;
  int treeRootIndex;
  int nextPageInfo;
  BPlusTree *rootOfTree;
  int numberOfFields;
  int depthOfTree;
  int capacity;
} BTreeHandle;

typedef struct TreeInfo{
  BPlusTree *current;
  int index;
}TreeInfo;

typedef struct BT_ScanHandle {
  BTreeHandle *tree;
  void *mgmtData;
} BT_ScanHandle;


// init and shutdown index manager
extern RC initIndexManager (void *mgmtData);
extern RC shutdownIndexManager ();

// create, destroy, open, and close an btree index
extern RC createBtree (char *idxId, DataType keyType, int n);
extern RC openBtree (BTreeHandle **tree, char *idxId);
extern RC closeBtree (BTreeHandle *tree);
extern RC deleteBtree (char *idxId);

// access information about a b-tree
extern RC getNumNodes (BTreeHandle *tree, int *result);
extern RC getNumEntries (BTreeHandle *tree, int *result);
extern RC getKeyType (BTreeHandle *tree, DataType *result);

// index access
extern RC findKey (BTreeHandle *tree, Value *key, RID *result);
extern RC insertKey (BTreeHandle *tree, Value *key, RID rid);
extern RC deleteKey (BTreeHandle *tree, Value *key);
extern RC openTreeScan (BTreeHandle *tree, BT_ScanHandle **handle);
extern RC nextEntry (BT_ScanHandle *handle, RID *result);
extern RC closeTreeScan (BT_ScanHandle *handle);

// debug and test functions
extern char *printTree (BTreeHandle *tree);

#endif // BTREE_MGR_H
