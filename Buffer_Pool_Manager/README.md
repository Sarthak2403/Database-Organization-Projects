## CS 525 ADO Assignment - 2 - Buffer Pool Manager - Group 36

Team Members and their Contribution:

Bhavana  Polakala - A20539792, bpolakala@hawk.iit.edu, 33.33 %

Sarthak Sonpatki -  A20579456, ssonpatki@hawk.iit.edu, 33.33 %	

Vishwas Ramakrishna - A20552892, vramakrishna@hawk.iit.edu, 33.33 %		

# Buffer Pool Manager:-

Implementing a buffer manager that controls a set number of memory pages that correspond to pages from a page file that is maintained by the storage manager set up in assignment 1 is the aim of this assignment. Page frames, or simply frames, are the memory pages that the buffer manager is in charge of managing. A buffer pool is what we refer to as the collection of a page file and the page frames that store pages from that file. Multiple open buffer pools can be managed concurrently by the buffer manager. For every page file, there is only one buffer pool, though. One page replacement approach, chosen during buffer pool initialization, is used by each buffer pool. The two substitute approaches that are used are FIFO, LRU and CLOCK.

# Folder Contents:-:

 dberror.h: RC code errors with their definitions are mentioned in this file.

 dberror.c: Functions such as errorMessage and printError are mentioned in this file.

storage_mgr.h: This file defines SM_PageHandle and provides a structure and attributes for SM_FileHandle. It declares functions for reading, writing, and manipulating files.

storage_mgr.c: This file contains the methods of the functions mentioned in the storage_mgr.h file. 

buffer_mgr.c : This file contains the methods of the functions mentioned in the buffer_mgr.h file.

buffer_mgr.h : This file defines the structure of the buffer pool and declares functions for buffer pool management.

test_helper.h: A set of macros needed for testing is defined in this file.

test_assign2_1.c: Implements and tests a buffer manager using FIFO page replacement strategy, allowing for creating, reading, and managing pages from a file, while simulating basic buffer pool operations.

test_assign2_2.c: Similar to test_assign2_1 this file is used for testcases such as testCreatingAndReadingDummyPages, testReadPage, testClock

Makefile: This file gets created on running ‘make’ command which is used to produce the binary test_assign1 from test_assign1_1.c.

Readme.txt: Detailed description of the program is mentioned 
  
## Functions

## Page Replacement Algorithm Strategies:

- FIFO : First In First Out (FIFO), functions like a queue where the page that appears first in the buffer pool is at the front and is replaced first if the buffer pool fills up.

- LRU : Least Recently Used (LRU) evicts the page frame that has gone unused the longest, compared to the other page frames in the buffer pool.

## Optional Extensions that we implemented:

- CLOCK : This algorithm keeps a track of the last added page frame in the buffer pool.

## Functions

1) Buffer Pool Functions

	1.1. RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData):__

	1.2. RC shutdownBufferPool(BM_BufferPool *const bm)__

	1.3. RC forceFlushPool(BM_BufferPool *const bm)__

2) Page Management Functions

	2.1. RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page): This function sets the dirtyFlag to indicate that the page has been modified, marking the page frame as dirty (setting the dirty bit to 1) so that changes can be reflected on disk later.

    2.2. RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page): This function writes the content of the specified page frame to the corresponding page on disk. It checks all pages in the buffer pool to find the target page and uses Storage Manager functions to write the page’s content to the disk.

    2.3. RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum): This function safely pins the page with the specified pageNum. The buffer manager ensures sufficient capacity, then loads the page from memory based on the pageNum. If the buffer pool is full, it references a new page and the page's data field points to the page frame where its content is stored.

    2.4. RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page): This function unpins the specified page by locating it using its pageNum and decreases the fixCount of that page, indicating it is no longer in use.

3) Statistic Functions:

	3.1. PageNumber *getFrameContents (BM_BufferPool *const bm): This function returns an array of PageNumbers, representing the page numbers stored in each page frame. If a frame is empty, it returns NO_PAGE for that frame.

    3.2. Bool *getDirtyFlags (BM_BufferPool *const bm): This function returns an array of boolean values, indicating whether each page in the buffer pool is dirty (modified) or clean (unmodified).

    3.3. int *getFixCounts (BM_BufferPool *const bm): This function returns an array of integers representing the fix count of each page frame, indicating how many clients are currently using each page.

    3.4. int getNumReadIO (BM_BufferPool *const bm): This function returns the total number of pages that have been read from the disk since the buffer pool was initialized.

    3.5. int getNumWriteIO (BM_BufferPool *const bm): This function returns the total number of pages that have been written to the page file since the buffer pool was initialized.

### Procedures for Implementation:
Process 1:

11) Navigate to the project folder.
2) Open the terminal in the project folder / VS code or any other IDE.
3) Run the below command to clear any pre-existin executable run files
```
make clean
```
4) Then run the below code to create fresh executable files
```
make
```
5) To test the program run the below command in your terminal
```
./test1

./test2  
```
