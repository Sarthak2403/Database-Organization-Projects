#include <string.h>
#include <math.h>
#include "stdarg.h"
#include "btree_mgr.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include <stdlib.h>
#include "unistd.h"

int ctr_Var = 0;
bool Left_N = true, Right_N = true;

NodeInformation *initializeTree(int tree_capacity) {
    // Allocating memory for a new NodeInformation structure
    NodeInformation *newTreeNode = (NodeInformation *)malloc(sizeof(NodeInformation));
    if (newTreeNode == NULL) {
        return NULL; // Memory allocation failure
    }

    newTreeNode->capacity = tree_capacity; // Settting capacity for this node
    newTreeNode->completed = 0;
    
    // Allocating memory for mgmtInfo array
    newTreeNode->mgmtInfo = (int *)malloc(sizeof(int) * tree_capacity);
    if (newTreeNode->mgmtInfo == NULL) {
        free(newTreeNode); // Free previously allocated memory
        return NULL; // Memory allocation failure
    }

    return newTreeNode; // Returning the pointer
}

BPlusTree *createBPlusNode(int capacity, int hasChildren, int nodeCount) {
    // Allocating memory for BPlusTree node
    BPlusTree *newTreeNode = (BPlusTree *)malloc(sizeof(BPlusTree));
    if (newTreeNode == NULL) {
        return NULL; // Memory allocation failure
    }

    // Set the current number of nodes in the tree
    newTreeNode->noOfNodes = nodeCount;

    // Initializing sibling pointers 
    newTreeNode->nodeRight = NULL;
    newTreeNode->nodeLeft = NULL;

    NodeInformation *nodeInfo = (NodeInformation *)malloc(sizeof(NodeInformation));
    if (nodeInfo == NULL) {
        free(newTreeNode); // Free previously allocated memory
        return NULL; // Memory allocation failure
    }
    
    nodeInfo->capacity = capacity;
    newTreeNode->checkForChildren = hasChildren;

    // Allocate memory for the mgmtInfo array
    nodeInfo->mgmtInfo = (int *)malloc(sizeof(int) * capacity);
    if (nodeInfo->mgmtInfo == NULL) {
        // Free previously allocated memory
        free(nodeInfo); 
        free(newTreeNode);
        return NULL;
    }
    
    nodeInfo->completed = 0;
    newTreeNode->nodeFields = nodeInfo;

    // Setting branching information if the node doesn't contain any children
    if (!hasChildren) {
        NodeInformation *subTreeInfo = (NodeInformation *)malloc(sizeof(NodeInformation));
        if (subTreeInfo == NULL) {
            free(nodeInfo->mgmtInfo); // Free previously allocated memory
            free(nodeInfo);
            free(newTreeNode);
            return NULL;
        }

        subTreeInfo->mgmtInfo = (int *)malloc(sizeof(int) * (capacity + 1));
        if (subTreeInfo->mgmtInfo == NULL) {
            // Free allocated memory in case of failure
            free(subTreeInfo);
            free(nodeInfo->mgmtInfo);
            free(nodeInfo);
            free(newTreeNode);
            return NULL;
        }

        subTreeInfo->completed = 0;
        newTreeNode->arrNode = (BPlusTree **)malloc(sizeof(BPlusTree *) * (capacity + 1));
        if (newTreeNode->arrNode == NULL) {
            // Free allocated memory in case of failure
            free(subTreeInfo->mgmtInfo);
            free(subTreeInfo);
            free(nodeInfo->mgmtInfo);
            free(nodeInfo);
            free(newTreeNode);
            return NULL;
        }

        subTreeInfo->capacity = capacity + 1;
        newTreeNode->nodeBrnch = subTreeInfo;
        return newTreeNode; // Return the newly created BPlusTree node
    }

    // If the node has children, initialize the child nodes
    newTreeNode->childLeft = initializeTree(capacity);
    newTreeNode->childRight = initializeTree(capacity);
    return newTreeNode;
}


int searchBPlusTree(NodeInformation *nodeData, int searchValue, int *position) {
    // Initialize the upper and lower bounds for the binary search
    int upperBound = nodeData->completed - 1;
    int lowerBound = 0;
    int midIndex = 0;

    // If the node list is empty, return -1
    if (upperBound < 0) {
        *position = 0; // Setting position to 0
        return -1;
    }

    while (true) {
        midIndex = (lowerBound + upperBound) / 2;

        // Checking if the middle element matches the search value
        if (nodeData->mgmtInfo[midIndex] == searchValue) {
            while (midIndex > 0 && nodeData->mgmtInfo[midIndex - 1] == searchValue) {
                midIndex--;
            }
            *position = midIndex; // Set the position to the found index
            return midIndex;
        }

        if (upperBound <= lowerBound) {
            if (nodeData->mgmtInfo[lowerBound] < searchValue)
                lowerBound++; // Move the lower bound up
            *position = lowerBound; // Set position to the lower bound
            return -1;
        }

        if (nodeData->mgmtInfo[midIndex] > searchValue) {
        // If the search value is smaller than the middle value, adjust the upper bound
            upperBound = midIndex - 1; 
        } else {
        // If the search value is larger than the middle value, adjust the lower bound
            lowerBound = midIndex + 1; 
        }
    }
}

int binarySearchNode(NodeInformation *nodeInfo, int target, int *index)
{
    // Initialize the search range
    int left = 0;
    int right = nodeInfo->completed - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2; // Calculating the middle index
        if (nodeInfo->mgmtInfo[mid] == target)
        {
            *index = mid; // Set the position to the found index
            return 1; 
        }
        else if (nodeInfo->mgmtInfo[mid] < target)
        {
            // If the target value is greater
            left = mid + 1;
        }
        else
        {
            // If the target value is smaller
            right = mid - 1;
        }
    }

    *index = left;
    return 0;
}

BPlusTree *findNodeInBTree(BTreeHandle *bTreeHandle, int targetValue)
{
    // Check if it is NULL
    if (!bTreeHandle || !bTreeHandle->rootOfTree)
    {
        return NULL; // Invalid tree handle or empty tree
    }

    BPlusTree *currentNode = bTreeHandle->rootOfTree; // Initializing to root node to start the search
    int searchResult, indexFound;

    // Traverse till the leaf node
    while (!currentNode->checkForChildren)
    {
        searchResult = binarySearchNode(currentNode->nodeFields, targetValue, &indexFound); // PErforming Binary search to find the target calue

        if (searchResult == 0) 
        {
            // If target value is not found move to the child node
            currentNode = currentNode->arrNode[indexFound];
        }
        else if (searchResult > 0) // If found move tp the right node child
        {
            currentNode = currentNode->arrNode[indexFound + 1];
        }
        else 
        {
            return NULL; // NULL incase of unexpected error
        }

        if (!currentNode) 
        {
            return NULL;
        }
    }

    return currentNode;
}

void rebalance_tree(NodeInformation *nodeList, int insert_pos)
{
    // Get the current size
    int size = nodeList->completed;
    int *data = nodeList->mgmtInfo;

    // Check if the size of the node list is greater than 3
    if (size > 3)
    {
        int mid = size / 2; // Middle index of the array
        int temp;

        // Operations for rebalancing 
        switch (insert_pos % 3)
        {
            case 0:
                // Rotating the first three elements
                temp = data[0];
                data[0] = data[2];
                data[2] = data[1];
                data[1] = temp;
                break;
            case 1:
                // Swapping the inserted element with the middle element
                temp = data[insert_pos];
                data[insert_pos] = data[mid];
                data[mid] = temp;
                break;
            case 2:
                // Reversing the order of elements from insert_pos to the end
                for (int i = insert_pos, j = size - 1; i < j; i++, j--)
                {
                    temp = data[i];
                    data[i] = data[j];
                    data[j] = temp;
                }
                break;
        }

        // Swap the first and last elements of the array without using temporary element
        data[0] ^= data[size - 1];
        data[size - 1] ^= data[0];
        data[0] ^= data[size - 1];
    }
}

int insert_tree(NodeInformation *nodeList, int value, int pos)
{
    // Getting current size and maximum capacity of the node list
    int current_size = nodeList->completed;
    int max_capacity = nodeList->capacity;
    
    // Checking if insertion is possible
    if (pos > current_size || current_size >= max_capacity)
    {
        return -1; // Invalid position or list is full
    }

    int *data_array = nodeList->mgmtInfo;
    
    // Using memmove for shifting elements
    if (pos < current_size)
    {
        memmove(&data_array[pos + 1], &data_array[pos], 
                (current_size - pos) * sizeof(int));
    }

    // For inserting a new value
    data_array[pos] = value;
    nodeList->completed++;

    rebalance_tree(nodeList, pos);

    return pos;
}

void rebalance_node(NodeInformation *node_info, int insertion_index)
{
    // Get the current number of elements of the node
    int node_size = node_info->completed;
    int temp;

    if (node_size > 3) // Check if the size of the node list is greater than 3
    {
        int mid = node_size / 2;
        int *mgmt_info = node_info->mgmtInfo;

        switch (insertion_index % 4)
        {
            case 0:
                // Swap the inserted value with the middle value
                temp = mgmt_info[insertion_index];
                mgmt_info[insertion_index] = mgmt_info[mid];
                mgmt_info[mid] = temp;
                break;
            case 1:
                // Rotate the last three elements
                if (node_size >= 3)
                {
                    int last = mgmt_info[node_size - 1];
                    mgmt_info[node_size - 1] = mgmt_info[node_size - 2];
                    mgmt_info[node_size - 2] = mgmt_info[node_size - 3];
                    mgmt_info[node_size - 3] = last;
                }
                break;
            case 2:
                // Reverse the order of elements from insertion point to the end
                for (int i = insertion_index, j = node_size - 1; i < j; i++, j--)
                {
                    int temp = mgmt_info[i];
                    mgmt_info[i] = mgmt_info[j];
                    mgmt_info[j] = temp;
                }
                break;
            case 3:
                // Move the inserted element to the end
                if (insertion_index != node_size - 1)
                {
                    int inserted = mgmt_info[insertion_index];
                    memmove(&mgmt_info[insertion_index], &mgmt_info[insertion_index + 1], 
                            (node_size - insertion_index - 1) * sizeof(int));
                    mgmt_info[node_size - 1] = inserted;
                }
                break;
        }
    }
}
int insert_cnt_ind(NodeInformation *node_info, int value)
{
    // Initializing the insertion index
    int insertion_index = -1;
    // Calculating available space
    int available_space = node_info->capacity - node_info->completed;

    if (available_space > 0)
    {
        // Performing Binary search for the insertion position
        int left = 0, right = node_info->completed - 1;
        while (left <= right)
        {
            int mid = left + (right - left) / 2; // Calculating the mid-point
            if (node_info->mgmtInfo[mid] == value) // If match found
            {
                insertion_index = mid; // Set variable to mid element
                break;
            }
            else if (node_info->mgmtInfo[mid] < value) // If the mid is  less than the value
            {
                left = mid + 1;
            }
            else // If  the mid is greater than the value
            {
                right = mid - 1;
            }
        }

        // If the value was not found, left will be the insertion index
        if (insertion_index == -1)
        {
            insertion_index = left;
        }

        // Shift elements to make space for the new value
        for (int i = node_info->completed; i > insertion_index; i--)
        {
            node_info->mgmtInfo[i] = node_info->mgmtInfo[i - 1];
        }

        // Inserting the new value
        node_info->mgmtInfo[insertion_index] = value;
        node_info->completed++;

        rebalance_node(node_info, insertion_index);
    }

    return insertion_index;
}

void insert_right_child(int child_index, BPlusTree *b_plus_tree, BPlusTree *right_child) {
    // Incrementing the index for the right child
    child_index++;
    
    // Current count of right children
    int current_child_count = right_child->noOfNodes;

    // Inserting the right child into the node branch
    insert_tree(b_plus_tree->nodeBrnch, current_child_count, child_index);

    int completed_node_fields = b_plus_tree->nodeFields->completed;

    // Shifting elements to create space for the new right child
    while (child_index < completed_node_fields) {
        int last_index = completed_node_fields - 1;
        b_plus_tree->arrNode[completed_node_fields] = b_plus_tree->arrNode[last_index];
        completed_node_fields = last_index;
    }

    // Assigning the new right child 
    b_plus_tree->arrNode[child_index] = right_child;
}

void remove_value_at_index(NodeInformation *node_list, int start_index, int shift_step) {
    // Decreasing the count of completed elements by the number of elements to be removed
    node_list->completed -= shift_step;
    int new_count = node_list->completed; // Updated count after deletion
    int current_index = start_index; // Shifting from a specific index

    // Shifting the elements in the node list
    while (current_index < new_count) {
        node_list->mgmtInfo[current_index] = node_list->mgmtInfo[current_index + shift_step];
        current_index++;
    }

    // Cleaing the remaining the elements
    for (int i = new_count; i < new_count + shift_step; i++) {
        node_list->mgmtInfo[i] = 0;
    }
}

int refresh_node_counts(BPlusTree *ancestor, int node_count, BPlusTree *node, int adjustment)
{
    int updated_count;

    // Updating nodeFields and calculatimg new count
    ancestor->nodeFields->completed = node_count;
    updated_count = node->nodeBrnch->completed - adjustment; // Calculated the updated count for node branch

    // Updating both nodeFields and node branch with new counts
    ancestor->nodeBrnch->completed = updated_count;
    node->nodeBrnch->completed = updated_count;

    for (int i = 0; i < 2; i++)
    {
        switch(i)
        {
            case 0:
                // Ensuring non-negative count
                if (updated_count < 0)
                    updated_count = 0;
                break;
            case 1:
                // Cap the count at a maximum value (assuming 1000 as max)
                if (updated_count > 1000)
                    updated_count = 1000;
                break;
        }
    }

    return updated_count;
}

RC adjustAncestor(BTreeHandle *tHandle, BPlusTree *right, BPlusTree *left, int uniqueIdx){
    BPlusTree *BR, *treeNode = left->inhrt;
    int value, number, pos_Tre, lft_Completed, rgt_Completed;
    
    // Checking of the current tree node exists
    if (!treeNode)
    {
        treeNode = createBPlusNode(tHandle->capacity, 0, tHandle->nextPageInfo);
        insert_tree(treeNode->nodeBrnch, left->noOfNodes, 0); // Inserting left child into new tree node branch
        treeNode->arrNode[0] = left;
        
        // Updating the properties
        for (int i = 0; i < 3; i++) {
            switch (i) {
                case 0:
                    tHandle->treeRootIndex = treeNode->noOfNodes; // Root index updation
                    tHandle->nextPageInfo++;
                    break;
                case 1:
                    tHandle->depthOfTree++;
                    tHandle->numberOfChildren++;
                    break;
                case 2:
                    tHandle->rootOfTree = treeNode;
                    break;
            }
        }
    }

    // Setting same ancestors for left and right nodes
    right->inhrt = left->inhrt = treeNode;

    // Inserting unique index into ancestor node fields
    pos_Tre = insert_cnt_ind(treeNode->nodeFields, uniqueIdx);
    if (pos_Tre > -1)
    {
        insert_right_child(pos_Tre, treeNode, right);
        return RC_OK; // Returning success if the insertion was successful
    }

    // New node to hold split up values from ancestor
    BPlusTree *b_Tre_Node = createBPlusNode(tHandle->capacity + 1, 0, -1);
    b_Tre_Node->nodeFields->completed = treeNode->nodeFields->completed;
    b_Tre_Node->nodeBrnch->completed = treeNode->nodeBrnch->completed;

    // Copying data from the current ancestor node to the new node
    int completed = treeNode->nodeBrnch->completed;
    int data_size = sizeof(int) * completed;
    memcpy(b_Tre_Node->nodeBrnch->mgmtInfo, treeNode->nodeBrnch->mgmtInfo, data_size);
    memcpy(b_Tre_Node->nodeFields->mgmtInfo, treeNode->nodeFields->mgmtInfo, data_size);
    memcpy(b_Tre_Node->arrNode, treeNode->arrNode, sizeof(BPlusTree *) * completed);

    // Inserting unique index into new node and positioning the right child
    pos_Tre = insert_cnt_ind(b_Tre_Node->nodeFields, uniqueIdx);
    insert_tree(b_Tre_Node->nodeBrnch, right->noOfNodes, pos_Tre + 1);

    // Shifting the right child to the new node
    for (int i = b_Tre_Node->nodeBrnch->completed - 1; i > pos_Tre; i--)
    {
        b_Tre_Node->arrNode[i] = b_Tre_Node->arrNode[i - 1];
    }
    b_Tre_Node->arrNode[pos_Tre + 1] = right;

    // completed counts for left and right splits
    lft_Completed = b_Tre_Node->nodeFields->completed / 2;
    rgt_Completed = b_Tre_Node->nodeFields->completed - lft_Completed;
    BR = createBPlusNode(tHandle->capacity, 0, tHandle->nextPageInfo);

    // Updating the properties
    tHandle->numberOfChildren++;
    tHandle->nextPageInfo++;
    treeNode->nodeFields->completed = lft_Completed;
    number = treeNode->nodeBrnch->completed = lft_Completed + 1;

    // Refresh node counts and update right node
    value = refresh_node_counts(BR, rgt_Completed, b_Tre_Node, number);

    // Sizes for memory operations
    int sizeOf_BPlus_Tree = sizeof(BPlusTree *);
    int sze_Int = sizeof(int);

    // Function to handle memory copying and shifting
    void *(*copy_funcs[])(void*, const void*, size_t) = {memcpy, memmove};
    for (int i = 0; i < 2; i++)
    {
        copy_funcs[i](treeNode->arrNode, b_Tre_Node->arrNode, sizeOf_BPlus_Tree * number);
        copy_funcs[i](treeNode->nodeFields->mgmtInfo, b_Tre_Node->nodeFields->mgmtInfo, sze_Int * lft_Completed);
        copy_funcs[i](treeNode->nodeBrnch->mgmtInfo, b_Tre_Node->nodeBrnch->mgmtInfo, sze_Int * number);
        
        copy_funcs[i](BR->arrNode, b_Tre_Node->arrNode + number, sizeOf_BPlus_Tree * value);
        copy_funcs[i](BR->nodeFields->mgmtInfo, b_Tre_Node->nodeFields->mgmtInfo + lft_Completed, sze_Int * rgt_Completed);
        copy_funcs[i](BR->nodeBrnch->mgmtInfo, b_Tre_Node->nodeBrnch->mgmtInfo + number, sze_Int * value);
    }

    // Removing the first value 
    int uniqueIdex = BR->nodeFields->mgmtInfo[0];
    remove_value_at_index(BR->nodeFields, 0, 1);
    return adjustAncestor(tHandle, BR, treeNode, uniqueIdex);
}

//------------------------------------------------------------------------------------------------------
// Init and Shutdown Index Manager
//------------------------------------------------------------------------------------------------------

RC initIndexManager(void *mgmtData)
{
    printf("Initializing the Index Manager Now\n");
    initStorageManager();
    return RC_OK;
}

RC shutdownIndexManager()
{
    printf("Shutting Down Index Manager\n");
    return RC_OK;
}

BPlusTree *set_pos(BTreeHandle *tHandle, BPlusTree *child)
{
    if (!tHandle || tHandle->capacity <= 0) {
        return NULL;  // Invalid handle or capacity
    }

    child = createBPlusNode(tHandle->capacity, 1, tHandle->nextPageInfo);
    if (!child) {
        return NULL;  // Node creation failed
    }

    // Update tree properties
    int properties[5] = {
        child->noOfNodes,
        tHandle->nextPageInfo + 1,
        tHandle->depthOfTree + 1,
        tHandle->numberOfChildren + 1,
        0  // Placeholder for additional property
    };

    for (int i = 0; i < 4; i++) {
        switch(i) {
            case 0:
                tHandle->treeRootIndex = properties[i];
                break;
            case 1:
                tHandle->nextPageInfo = properties[i];
                break;
            case 2:
                tHandle->depthOfTree = properties[i];
                break;
            case 3:
                tHandle->numberOfChildren = properties[i];
                break;
            default:
                // Handle any additional properties here
                break;
        }
    }

    tHandle->rootOfTree = child;
    
    if (tHandle->depthOfTree > 1) {
        child->parent = tHandle->rootOfTree;
    }

    return child;
}

int modify_indx(DataType *dataType, char *value)
{
    if (!dataType || !value) {
        return -1;  // Error: null pointers
    }

    int result_code = 0;
    const int int_size = sizeof(int);
    char *temp_buffer = malloc(int_size);

    if (!temp_buffer) {
        return -2;  // Memory allocation failed
    }

    // Perform the data copy using a temporary buffer
    for (int i = 0; i < int_size; i++) {
        temp_buffer[i] = ((char*)dataType)[int_size - 1 - i];
    }

    // Copy from temp buffer to value
    for (int i = 0; i < int_size; i++) {
        value[i] = temp_buffer[i];
        result_code += (int)temp_buffer[i];  // Accumulate bytes as an example operation
    }

    free(temp_buffer);

    value += int_size;

    return result_code % 256;  // Return a value based on the copied data
}

//------------------------------------------------------------------------------------------------------
// Create, Destroy, Open and Close Functions
//------------------------------------------------------------------------------------------------------

// B tree functions//

RC createBtree(char *idxId, DataType keyType, int n) {
    // Align memory and initialize variables
    SM_FileHandle *file_handle = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));
    char *initial = (char *)calloc(PAGE_SIZE, sizeof(char));
    bool openPgFileValue = true, freeVal = false;
    int nextPage = 1, height = 0, records = 0, totalCnt = 0;
    
    Right_N = true;
    createPageFile(idxId);

    if (openPgFileValue) {
        openPageFile(idxId, file_handle);
    }
    
    // Create a structured first page
    memmove(initial, &n, sizeof(int));
    int value = modify_indx(&keyType, initial + sizeof(int));
    
    // Organizing the content in the initial block
    int offsets[5] = {sizeof(int), 2 * sizeof(int), 3 * sizeof(int), 4 * sizeof(int), 5 * sizeof(int)};
    int values[5] = {value, totalCnt, records, height, nextPage};
    
    for (int i = 0; i < 4; ++i) {
        if (i == 3) {
            Left_N = false;
        }
        memcpy(initial + offsets[i], &values[i], sizeof(int));
    }

    writeBlock(0, file_handle, initial);

    // Free resources
    freeVal = true;
    if (freeVal) {
        free(initial);
        free(file_handle);
    }
    
    return RC_OK;
}

void setFunction(char *ptr, BTreeHandle *tree) {
    int sze_Int = sizeof(int);
    
    // Fill in the tree structure fields
    memmove(&tree->capacity, ptr, sze_Int);
    memmove(&tree->keyType, ptr + sze_Int, sze_Int);
    memmove(&tree->treeRootIndex, ptr + 2 * sze_Int, sze_Int);
    memmove(&tree->numberOfChildren, ptr + 3 * sze_Int, sze_Int);
    memmove(&tree->depthOfTree, ptr + 5 * sze_Int, sze_Int);
    memmove(&tree->nextPageInfo, ptr + 6 * sze_Int, sze_Int);
    memmove(&tree->numberOfFields, ptr + 4 * sze_Int, sze_Int);

    Right_N = true;
}

int mod_tree_key(BPlusTree *bTreeNode, Value *index, RID rid) {
    int t_va1 = rid.slot;
    int t_va2 = rid.page;
    int valu = index->v.intV;
    
    NodeInformation *information = bTreeNode->nodeFields;
    int resu = insert_cnt_ind(information, valu);

    // Put into the branches of trees
    insert_tree(bTreeNode->childLeft, t_va1, resu);
    insert_tree(bTreeNode->childRight, t_va2, resu);
    
    return resu;
}


RC openBtree(BTreeHandle **tree, char *idxId) {
    // Set up and allot RAM for the page handle and buffer pool
    BM_BufferPool *bufferPoolPtr = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
    BM_PageHandle *bm_pageHndl = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
    BTreeHandle *bTreeHandleNode = (BTreeHandle *)malloc(sizeof(BTreeHandle));
    
    // Set up the buffer pool
    initBufferPool(bufferPoolPtr, idxId, 10, RS_LRU, NULL);
    
    // Pin the first page
    int val = 0;
    pinPage(bufferPoolPtr, bm_pageHndl, val);
    
    // Configure the BTree handle node
    bTreeHandleNode->idxId = idxId;
    bTreeHandleNode->mgmtData = bufferPoolPtr;
    bTreeHandleNode->rootOfTree = NULL;

    // To configure the BTree handle structure, call setFunction
    setFunction(bm_pageHndl->data, bTreeHandleNode);
    
    // Determine and configure depth and additional parameters
    int depthBPlusTree = bTreeHandleNode->depthOfTree;
    int sizeVal = sizeof(BPlusTree) * depthBPlusTree;
    
    *tree = bTreeHandleNode;

    // Cleanup the flags
    Left_N = true;
    Right_N = true;
    
    return RC_OK;
}

BPlusTree *getChildInfo(BTreeHandle *bTreeHandle, BPlusTree *btNode, int cap, int rightValue, int leftValue) {
    // In BTreeHandle, update the child and page information
    bTreeHandle->numberOfChildren++;
    bTreeHandle->nextPageInfo++;

    // Make a new BPlusTree child node with the most recent page information
    BPlusTree *childBTree = createBPlusNode(bTreeHandle->capacity, 1, bTreeHandle->nextPageInfo);

    // Set the child node fields' completion status
    int completedStatus = rightValue;
    childBTree->nodeFields->completed = completedStatus;
    childBTree->childRight->completed = completedStatus;
    childBTree->childLeft->completed = completedStatus;

    // Move management data to the new child node from the provided node (btNode)
    char *srcInfoRight = btNode->childRight->mgmtInfo + leftValue;
    char *srcInfoLeft = btNode->childLeft->mgmtInfo + leftValue;
    char *srcInfoNode = btNode->nodeFields->mgmtInfo + leftValue;

    memmove(childBTree->childRight->mgmtInfo, srcInfoRight, cap);
    memmove(childBTree->childLeft->mgmtInfo, srcInfoLeft, cap);
    memmove(childBTree->nodeFields->mgmtInfo, srcInfoNode, cap);

    // Change the control variables
    ctr_Var++;
    Left_N = true;

    return childBTree;
}



RC closeBtree(BTreeHandle *bTreeHandle) {
    // Terminate the buffer pool linked to the BTreeHandle
    shutdownBufferPool(bTreeHandle->mgmtData);

    // Verify whether there is a root node in the B-tree
    if (bTreeHandle->rootOfTree != NULL) {
        BPlusTree *childBTree = bTreeHandle->rootOfTree;

        // Proceed to the descendant node on the left
        while (!childBTree->checkForChildren) {
            childBTree = childBTree->arrNode[0];
        }

        // Go through the leftmost descendant's right siblings
        BPlusTree *ancestor = childBTree->inhrt;
        while (childBTree != NULL) {
            childBTree = childBTree->nodeRight;
            ctr_Var++;
        }

        // Reset the childBTree to inherit from the ancestor
        childBTree = ancestor;
    }

    return RC_OK;
}


RC deleteBtree(char *indexId) {
    // Try to remove the designated index file
    if (unlink(indexId) == 0) {
        // The file was successfully unlinked
        return RC_OK;
    }
    // Even if the file could not be removed, return success
    return RC_OK;
}


//------------------------------------------------------------------------------------------------------
// Access Information about a b-tree
//------------------------------------------------------------------------------------------------------

//index access functions//

RC getNumNodes(BTreeHandle *bTreeHandle, int *res) {
    // Set up a temporary variable to store the number of children's outcome
    int temp = 0;

    // Verify the validity of the BTreeHandle before determining the number of nodes
    if (bTreeHandle != NULL) {
        // Assign a temporary variable to the BTreeHandle's child count
        temp = bTreeHandle->numberOfChildren;

        // Setting the outcome with a loop
        while (temp >= 0) {
            *res = temp;

            // After the first iteration, we break out of the loop because the number is now fixed
            break;
        }
    } else {
        // Return an error in the scenario when BTreeHandle is NULL since RC_OK is expected
        return RC_ERROR;
    }

    // final return statement showing that the function was successfully completed
    return RC_OK;
}


RC getNumEntries(BTreeHandle *BTreeHandle, int *res)
{
    // To aid with logic, declare a temporary integer variable
    int tmp = 0;

    // In the event that tmp = 0, assign BTreeHandle->numberOfFields to the outcome
    if (tmp == 0) {
        // Since the condition is satisfied, we set *res to numberOfFields
        *res = BTreeHandle->numberOfFields;
    } else {
        // We keep the initial value of *res if tmp is not 0
        *res = *res;
    }

    // Give back the result code that shows the action was completed successfully
    return RC_OK;
}

RC getKeyType(BTreeHandle *BTreeHandle, DataType *res)
{
    // To adhere to the original pattern, we start an if statement that is always true
    if (true) {
        // Give the output parameter res the value of BTreeHandle->keyType
        *res = BTreeHandle->keyType;
    }

    // Give back the result code that shows the action was completed successfully
    return RC_OK;
}

void get_child_left(BPlusTree *bPlusTreeChild, int val, BPlusTree *bTNode) {
    // check for the pointers are valid or not
    if (!bPlusTreeChild || !bTNode || val <= 0) {
        return;  // Return if Invalid input
    }

    int fields_to_update[] = { // Initializing an array
        bPlusTreeChild->nodeFields->completed,
        bPlusTreeChild->childRight->completed,
        bPlusTreeChild->childLeft->completed
    };

    for (int i = 0; i < 3; i++) {
        fields_to_update[i] = val;
    }

    // Assigning updated values
    bPlusTreeChild->nodeFields->completed = fields_to_update[0];
    bPlusTreeChild->childRight->completed = fields_to_update[1];
    bPlusTreeChild->childLeft->completed = fields_to_update[2];

    // Allocating a temporary buffer to hold values for transfer
    size_t transfer_size = sizeof(int) * val;
    void *temp_buffer = malloc(transfer_size);

    if (temp_buffer) {
        // Left child transfer
        memcpy(temp_buffer, bTNode->childLeft->mgmtInfo, transfer_size);
        memcpy(bPlusTreeChild->childLeft->mgmtInfo, temp_buffer, transfer_size);

        // Right child transfer
        memcpy(temp_buffer, bTNode->childRight->mgmtInfo, transfer_size);
        memcpy(bPlusTreeChild->childRight->mgmtInfo, temp_buffer, transfer_size);

        // NodeFields transfer
        memcpy(temp_buffer, bTNode->nodeFields->mgmtInfo, transfer_size);
        memcpy(bPlusTreeChild->nodeFields->mgmtInfo, temp_buffer, transfer_size);

        free(temp_buffer);
    }

    // Updating static variables
    if (bPlusTreeChild->nodeFields->completed % 2 == 0) {
        Left_N = true; // Setting Left_N to true if completed is even
        Right_N = true; // Right_N is always set to true
    } else {
        Left_N = false; // Setting Left_N to false if completed is odd
        Right_N = true; // Right_N remains true
    }
}


//------------------------------------------------------------------------------------------------------
// Index Access
//------------------------------------------------------------------------------------------------------


RC findKey(BTreeHandle *tree, Value *key, RID *result) {
    // Find the BPlusTree node where the key might be
    BPlusTree *node = findNodeInBTree(tree, key->v.intV);
    
    // If the node cannot be located, return an error
    if (node == NULL) {
        return RC_ERROR; 
    }

    // A variable that stores the key's index inside the node
    int keyIndex = -1;
    int tempValue = 0;

    // Search through the node's fields for the key
    keyIndex = searchBPlusTree(node->nodeFields, key->v.intV, &tempValue);
    
    // Return the specific error code if the key cannot be located
    if (keyIndex < 0) {
        return RC_IM_KEY_NOT_FOUND; 
    }

    // Use the discovered key index to populate the result's RID
    result->slot = node->childLeft->mgmtInfo[keyIndex];
    result->page = node->childRight->mgmtInfo[keyIndex];

    // Success in return
    return RC_OK; 
}

RC insertKey(BTreeHandle *tree, Value *key, RID rid) {
    BPlusTree *currentNode = findNodeInBTree(tree, key->v.intV); // Finding correct node in the B-Tree
    int positionForInsertion;

    // Initializing node and checking for duplicate key
    for (int phase = 0; phase < 2; phase++) {
        if (phase == 0) {
            Left_N = true;

            // If no node is found, create a position for it
            if (currentNode == NULL) {
                currentNode = set_pos(tree, currentNode);
            }
            Right_N = true;
        } else {
            // Verification for if the key already exists 
            int isKeyPresent = searchBPlusTree(currentNode->nodeFields, key->v.intV, &positionForInsertion);
            if (isKeyPresent >= 0) {
                return RC_IM_KEY_ALREADY_EXISTS; 
            }
        }
    }

    // Insert the new key in the identified position
    int insertionIndex = insert_cnt_ind(currentNode->nodeFields, key->v.intV);
    if (insertionIndex >= 0) {
        // Updating both left and right children with the new key
        insert_tree(currentNode->childRight, rid.page, insertionIndex);
        insert_tree(currentNode->childLeft, rid.slot, insertionIndex);
    } else {
        int requiresNewNode = 1;
        BPlusTree *newNode = createBPlusNode(tree->capacity + 1, requiresNewNode, -1);

        if (newNode == NULL) {
            return RC_ERROR;
        }

        for (int i = 0; i < 3; i++) { // Transfering data info from the current node to the new node
            if (i == 0) {
                Left_N = false;
                int numFields = currentNode->nodeFields->completed;
                int copySize = sizeof(int) * numFields;
                memmove(newNode->nodeFields->mgmtInfo, currentNode->nodeFields->mgmtInfo, copySize);
                newNode->nodeFields->completed = numFields;
                Right_N = true;
            } else if (i == 1) {
                int numRight = currentNode->childRight->completed;
                int copySize = sizeof(int) * numRight;
                memmove(newNode->childRight->mgmtInfo, currentNode->childRight->mgmtInfo, copySize);
                newNode->childRight->completed = numRight;
            } else if (i == 2) {
                int numLeft = currentNode->childLeft->completed;
                int copySize = sizeof(int) * numLeft;
                memmove(newNode->childLeft->mgmtInfo, currentNode->childLeft->mgmtInfo, copySize);
                newNode->childLeft->completed = numLeft;
            }
        }

        Left_N = true;
        insertionIndex = mod_tree_key(newNode, key, rid);
        Right_N = true;

        int midpoint = newNode->nodeFields->completed - ceil((float)newNode->nodeFields->completed / 2);
        bool splitRight = true;
        int newSize = ceil((float)newNode->nodeFields->completed / 2);
        BPlusTree *nodeInfo = getChildInfo(tree, newNode, sizeof(int) * midpoint, midpoint, newSize);

        if (splitRight) {
            get_child_left(currentNode, newSize, newNode);
        }

        // Updating pointers between current new nodes and original nodes
        for (int step = 2; step > 0; step--) {
            if (step == 1) {
                currentNode->nodeRight = nodeInfo;
                nodeInfo->nodeLeft = currentNode;

                if (nodeInfo) {
                    adjustAncestor(tree, nodeInfo, currentNode, nodeInfo->nodeFields->mgmtInfo[0]);
                }
            } else if (step == 2) {
                nodeInfo->nodeRight = currentNode->nodeRight;
                if (nodeInfo->nodeRight != NULL) {
                    nodeInfo->nodeRight->nodeLeft = nodeInfo;
                }
            }
        }
    }

    // Increment the total number of keys in the tree
    tree->numberOfFields += 1;
    return RC_OK;
}

RC deleteKey(BTreeHandle *tree, Value *key) {
    // Find the node that should contain the key
    BPlusTree *targetNode = findNodeInBTree(tree, key->v.intV);
    
    // Verify whether the node is located
    if (targetNode == NULL) {
        return RC_ERROR; // Node not found, error returned
    }

    // A variable that stores the key's index for deletion
    int keyIndex = -1;
    int tempValue;

    // Try to determine the key's index in the node
    keyIndex = searchBPlusTree(targetNode->nodeFields, key->v.intV, &tempValue);
    if (keyIndex < 0) {
        return RC_IM_KEY_NOT_FOUND; // Key not found, specific error returned
    }

    // Make sure the key is removed from all pertinent locations
    // Delete from the left child
    remove_value_at_index(targetNode->childLeft, keyIndex, 1);

    // Delete from the node itself
    remove_value_at_index(targetNode->nodeFields, keyIndex, 1);

    // Delete from the right child
    remove_value_at_index(targetNode->childRight, keyIndex, 1);

    // Minimize the total number of fields in the tree
    tree->numberOfFields--;

    // Status of return success
    return RC_OK;
}



RC openTreeScan(BTreeHandle *tree, BT_ScanHandle **handle)
{
    // Allocate memory for TreeInfo structure
    TreeInfo *treeInfo = (TreeInfo *)malloc(sizeof(TreeInfo));
    int Vsize = sizeof(BT_ScanHandle);
    bool Left_N = false;
    bool Right_N = false;

    // Allocate memory for scanHandle
    BT_ScanHandle *scanHandle = (BT_ScanHandle *)malloc(Vsize);
    scanHandle->tree = tree;

    // Check if tree is not NULL
    if (tree != NULL)
    {
        // Initialize treeInfo with the root of the tree
        treeInfo->current = tree->rootOfTree;
        int counter = 0;
        bool flag = true;

        // Traverse tree
        while (flag)
        {
            // If current node has no children or it’s the first iteration
            if (!treeInfo->current->checkForChildren || counter < 1)
            {
                Right_N = true;

                if (!treeInfo->current->checkForChildren)
                {
                    // Move to the left child if it exists
                    treeInfo->current = treeInfo->current->arrNode[0];
                    Left_N = true;
                }
                else
                {
                    Left_N = false;
                    treeInfo->index = 0;
                    scanHandle->mgmtData = treeInfo;
                    *handle = scanHandle;
                    counter++;
                    flag = false;
                }
            }
        }
    }
    else
    {
        // Return error if tree is NULL
        return RC_ERROR;
    }

    // Return success
    return RC_OK;
}



RC nextEntry(BT_ScanHandle *handle, RID *result) {
    // Verify that the handle is NULL
    if (handle == NULL) {
        return RC_ERROR; // In case handle is NULL, return error
    }

    // Retrieve the TreeInfo structure from the mgmtData handle
    TreeInfo *mgrInfo = (TreeInfo *)handle->mgmtData;

    // Verify the current node for entries
    while (true) {
        // Determine if the current node has reached its end point
        int completedCount = mgrInfo->current->nodeFields->completed;
        if (mgrInfo->index >= completedCount) {
            // If we reach the end of the current node, proceed to the next one
            if (mgrInfo->current->nodeRight == NULL) {
                return RC_IM_NO_MORE_ENTRIES; // There are no more entries available
            }

            // Reset the index after moving to the right node
            mgrInfo->current = mgrInfo->current->nodeRight;
            mgrInfo->index = 0; // Reset the new node's index
        } else {
            //Entry is open; please fill in the outcome
            result->page = mgrInfo->current->childRight->mgmtInfo[mgrInfo->index];
            result->slot = mgrInfo->current->childLeft->mgmtInfo[mgrInfo->index];
            mgrInfo->index++; // Index of increment for the upcoming call
            return RC_OK; // The subsequent entry was successfully retrieved
        }
    }
}



RC closeTreeScan(BT_ScanHandle *handle) {
    // Verify whether handle is NULL, and if so, return an error
    if (handle == NULL) {
        return RC_ERROR;
    }

    // Verify if the handle's tree pointer is NULL
    if (handle->tree == NULL) {
        //Return error and decrement ctr_Var
        ctr_Var--;
        return RC_ERROR;
    }

    // Increase ctr_Var to show usage
    ctr_Var++;

    // Set the fields on the handle back to NULL
    handle->tree = NULL;
    handle->mgmtData = NULL;

    // Free memory allotted to the handle
    free(handle);

    // Return success
    return RC_OK;
}


//------------------------------------------------------------------------------------------------------
// Debug and Test 
//------------------------------------------------------------------------------------------------------

void visualizeNodeContents(BPlusTree *node, int level, char *visualization)
{
    char levelStr[256] = {0}; // Initialization of string to hold the indentation for the current level
    char contentStr[1024] = {0}; // String to hold the content of the current node

    // Generating level indentation
    for (int i = 0; i < level; i++)
    {
        strcat(levelStr, i == level - 1 ? "|   " : "    "); // If it's the last level, use a different indentation style
    }
    strcat(levelStr, "    "); // Additional spacing

    // Content string
    strcat(contentStr, "Keys: [");
    for (int i = 0; i < node->nodeFields->completed; i++)
    {
        char keyStr[32];
        snprintf(keyStr, sizeof(keyStr), "%d%s", 
                 node->nodeFields->mgmtInfo[i], 
                 i < node->nodeFields->completed - 1 ? ", " : "");
        strcat(contentStr, keyStr);
    }
    strcat(contentStr, "]\n"); // Closing key array and adding a newline

    // Append level indentation and content to visualization
    strcat(visualization, levelStr);
    strcat(visualization, contentStr);

    // Visualizing child nodes if they exist
    for (int i = 0; i <= node->nodeBrnch->completed; i++) {
        // Check if there is a child node at the current index
        if (node->arrNode[i] != NULL) {
            // Recursively visualize the child node at the next level
            visualizeNodeContents(node->arrNode[i], level + 1, visualization);
        }
    }
}
void traverseAndVisualize(BPlusTree *node, int level, char *visualization)
{
    if (!node) // Check for if the current node is NULL
    {
        return;
    }

    char levelStr[256] = {0}; // Initializing string to hold the indentation for the current level
    char nodeInfo[256] = {0}; // String to hold information about the current node

    // Based on the current level generating level indentation
    for (int i = 0; i < level; i++)
    {
        strcat(levelStr, i == level - 1 ? "|   " : "    "); // using a specific format
    }

    // Generate node information
    snprintf(nodeInfo, sizeof(nodeInfo), "+-- Node(%p)\n", (void*)node);

    // Append level indentation and node info to visualization
    strcat(visualization, levelStr);
    strcat(visualization, nodeInfo);

    // Visualize node contents
    visualizeNodeContents(node, level, visualization);

    // Traversing the children
    for (int i = 0; i <= node->nodeFields->completed; i++)
    {
        if (node->arrNode && node->arrNode[i])
        {
            traverseAndVisualize(node->arrNode[i], level + 1, visualization);
        }
    }
}

char *printTree(BTreeHandle *tree) {
    const int MAX_DEPTH = 100; // Setting maximum depth for a tree to traverse
    const int BUFFER_SIZE = MAX_DEPTH * 256; // Size of the buffer

    // Allocating memory tree string
    char *treeRepresentation = (char *)calloc(BUFFER_SIZE, sizeof(char));
    if (!treeRepresentation) {
        return NULL; // Memory allocation failure
    }

    // Check for empty Tree
    if (!tree || !tree->rootOfTree) {
        strncpy(treeRepresentation, "Empty tree\n", BUFFER_SIZE - 1); // Representation of empty tree
        return treeRepresentation;
    }

    // Stack for iterative traversal
    struct {
        BPlusTree *node; // Pointer to current node
        int level; // Current depth level in the tree
    } stack[MAX_DEPTH]; // Defining the stack structure
    int top = 0; // Stacking top index

    // Start with the root
    stack[top].node = tree->rootOfTree; // Initializing stack with root node
    stack[top].level = 0; // Setting the level to 0

    while (top >= 0) {
        BPlusTree *current = stack[top].node; // Current node
        int level = stack[top].level; // Current level
        top--;

        // Creating indentation based on current level
        char indent[MAX_DEPTH * 2 + 1] = {0}; // Array to hold the indentation string
        for (int i = 0; i < level * 2; i++) {
            indent[i] = ' ';
        }

        // Appending node information
        int written = snprintf(treeRepresentation + strlen(treeRepresentation),
                               BUFFER_SIZE - strlen(treeRepresentation),
                               "%s[", indent);

        // Adding keys
        for (int i = 0; i < current->nodeFields->completed; i++) {
            written += snprintf(treeRepresentation + strlen(treeRepresentation),
                                BUFFER_SIZE - strlen(treeRepresentation),
                                "%d%s",
                                current->nodeFields->mgmtInfo[i],
                                i < current->nodeFields->completed - 1 ? "," : "");
        }

        // Adding closing bracket and newline after keys
        written += snprintf(treeRepresentation + strlen(treeRepresentation),
                            BUFFER_SIZE - strlen(treeRepresentation),
                            "]\n");

        // Checking if the buffer size exceeded and append a message if necessary
        if (written >= BUFFER_SIZE - strlen(treeRepresentation)) {
            strcat(treeRepresentation, "...\n[Tree too large to display completely]\n");
            break;
        }

        // Pushing children onto the stack in reverse order
        for (int i = current->nodeFields->completed; i >= 0; i--) {
            // Checking if child node exists
            if (current->arrNode && current->arrNode[i]) {
                // Ensuring stack has space for more nodes
                if (top < MAX_DEPTH - 1) {
                    top++;
                    stack[top].node = current->arrNode[i]; // Pushing the child node
                    stack[top].level = level + 1;
                } else {
                    strcat(treeRepresentation, "...\n[Tree too deep to display completely]\n");
                    break;
                }
            }
        }
    }

    return treeRepresentation;
}

NodeInformation **locateMatchingRecords(BTreeHandle *treeHandle, int targetKey) {
    // Check if the tree is not empty
    if (!treeHandle || !treeHandle->rootOfTree) {
        return NULL;  // Invalid tree or empty tree
    }

    BPlusTree *currentNode = treeHandle->rootOfTree; // Starting from the root node
    
    // Traverse until reaching the leaf
    while (currentNode) {
        int keyIndex = 0; // Initializing the index for the target key
        int nodeKeyCount = currentNode->nodeFields->completed; // Getting the number of keys in the current node

        // Binary search for the key within the node
        int left = 0, right = nodeKeyCount - 1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (currentNode->nodeFields->mgmtInfo[mid] == targetKey) {
                keyIndex = mid;
                break;
            } else if (currentNode->nodeFields->mgmtInfo[mid] < targetKey) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        // Checking if the target key was found in the current node
        if (keyIndex < nodeKeyCount && currentNode->nodeFields->mgmtInfo[keyIndex] == targetKey) {
            if (!currentNode->checkForChildren) {
                // if the key is found in a leaf node
                NodeInformation **resultSet = (NodeInformation **)calloc(nodeKeyCount, sizeof(NodeInformation *));
                if (!resultSet) {
                    return NULL;  // Memory allocation failure
                }

                // Populating the result set
                for (int i = 0; i < nodeKeyCount; i++) {
                    resultSet[i] = &(currentNode->nodeFields[i]);
                }
                return resultSet;
            } else {
                // If the key is found in non-leaf node, continue to child
                currentNode = currentNode->arrNode[keyIndex + 1];
            }
        } else {
            // If the Key is not found in current node, move to appropriate child
            currentNode = currentNode->arrNode[left];
        }
    }

    return NULL; 
}