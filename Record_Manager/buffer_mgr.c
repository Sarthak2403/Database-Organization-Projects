#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "pthread.h"
#include <limits.h>

static int getNewTimestamp(void);

static pthread_mutex_t mutex_initializer = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_unpinpage_initializer = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_pinpage_initializer = PTHREAD_MUTEX_INITIALIZER;

typedef struct Page
{
	SM_PageHandle data;
	PageNumber pageNumber;
	int fixCount;
	int hitLRU;
	int refLRU;
	int dirtyFlag;
    int timestamp;
} PageFrame;

int writeCnt = 0, clockPtr = 0, buffSize = 0;

int rearIndx = 0, hitCnt = 0;
int readIOCount = 0;
// Add this function to get a new timestamp
static int globalTimestamp = 0;
int getNewTimestamp() {
    return globalTimestamp++;
}

//__________________________________________________________________________________________________________
// PAGE REPLACEMENT STRATEGIES

// A Function to write a dirty page back to the disk
void writeBackPage(BM_BufferPool *const bm, PageFrame *pframe) {
    if (pframe->dirtyFlag != 1) {
        return; // return if the page is not dirty
    }

    SM_FileHandle fileHandle; // Declaring the file handle
    RC status = openPageFile(bm->pageFile, &fileHandle); // Opening the page file

    if (status == RC_OK) { // successful file open
        writeBlock(pframe->pageNumber, &fileHandle, pframe->data); // Writing the dirty page
        writeCnt++;
        closePageFile(&fileHandle); // closing file handle after writing
    }
}

// Update Function
void updatePageFrame(PageFrame *dest, PageFrame *src) {
    // Check for null pointers
    if (dest == NULL || src == NULL) { 
        return; 
    }

    // Modification of the 
    dest->dirtyFlag  = src->dirtyFlag;  // Copying the dirty flag
    dest->data       = src->data;       // Copying the data pointer
    dest->pageNumber = src->pageNumber; // Copying the page number
    dest->fixCount   = src->fixCount;   // Copying the fix count
}



void flushAllDirtyPages(BM_BufferPool *const bm, PageFrame *pageFrames, int numPages) {
    for (int i = 0; i < numPages; i++) {
        if (pageFrames[i].dirtyFlag == 1) {
            writeBackPage(bm, &pageFrames[i]); // Writing the dirty pages back to disk
            pageFrames[i].dirtyFlag = 0; // Resetting the dirty flag after flushing
        }
    }
}

// Function to unpin all pages before closing
void unpinAllPages(BM_BufferPool *const bm, PageFrame *pageFrames, int numPages) {
    for (int i = 0; i < numPages; i++) {
        pageFrames[i].fixCount = 0; // Reset the fixCount for each page
    }
}

// Function for closing pinned adn dirty pages
RC closeBufferPool(BM_BufferPool *const bm) {
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData; // getting pages from buffer pool 
    int numPages = bm->numPages;

    // Checking for any pages that are still pinned (fixCount > 0)
    for (int i = 0; i < numPages; i++) {
        if (pageFrames[i].fixCount > 0) {
            return RC_PINNED_PAGES_IN_BUFFER; // Return error if there are pinned pages
        }
    }
    // Flush all dirty pages before closing
    flushAllDirtyPages(bm, pageFrames, numPages);

    // After flushing, unpin all pages
    unpinAllPages(bm, pageFrames, numPages);

    // Releasing buffer pool management data
    free(bm->mgmtData);
    bm->mgmtData = NULL;

    return RC_OK;
}

bool isFrameReplaceable(PageFrame *frame);
void writeBackIfDirty(BM_BufferPool *const bm, PageFrame *frame);
void replaceFrame(PageFrame *destFrame, PageFrame *srcFrame);

// FIFO page replacement
void FIFO(BM_BufferPool *const bm, PageFrame *newPage) {
    PageFrame *frames = (PageFrame *)bm->mgmtData;
    int replaceIndex = rearIndx % bm->numPages;

    // Searching for first unpinned frame
    for (int i = 0; i < bm->numPages; i++) {
        if (frames[replaceIndex].fixCount == 0) {
            if (frames[replaceIndex].dirtyFlag == 1) {
                // Write back if dirty
                SM_FileHandle fHandle;
                if (openPageFile(bm->pageFile, &fHandle) == RC_OK) {
                    writeBlock(frames[replaceIndex].pageNumber, &fHandle, frames[replaceIndex].data);
                    closePageFile(&fHandle);
                    writeCnt++;
                }
            }
            // Free the old page data if it exists
            if (frames[replaceIndex].data != NULL) {
                free(frames[replaceIndex].data);
            }

            frames[replaceIndex] = *newPage; // Replace the frame
            rearIndx++;
            return;
        }
        replaceIndex = (replaceIndex + 1) % bm->numPages;
    }
}

// If a frame is replaceble use this function as a helper function.
bool isFrameReplaceable(PageFrame *frame) {
    return (frame->fixCount == 0); // If the frame is not pinned, it can be changed
}

// Helper function tto write back dirty pages
void writeBackIfDirty(BM_BufferPool *const bm, PageFrame *frame) {
    if (frame->dirtyFlag == 1) {
        writeBackPage(bm, frame); 
    }
}

// Function to replace new data
void replaceFrame(PageFrame *destFrame, PageFrame *srcFrame) {
    updatePageFrame(destFrame, srcFrame); // A helper function for determining the least-used frame's index
}


// A helper function for determining the least-used frame index
int findLeastRecentlyUsedFrame(PageFrame *frames, int size) {
    int leastHitIndex = -1;
    int minHitCount = INT_MAX;

    for (int i = 0; i < size; i++) {
        if (frames[i].fixCount == 0 && frames[i].hitLRU < minHitCount) {
            minHitCount = frames[i].hitLRU;
            leastHitIndex = i;
        }
    }
    return leastHitIndex;
}

bool isReplaceable(int lRUIndex);
void handlePageReplacement(BM_BufferPool *const bm, PageFrame *oldPage, PageFrame *newPage);  

RC LRU(BM_BufferPool *const bm, PageFrame *newPage) {
    PageFrame *frames = (PageFrame *)bm->mgmtData;
    int replaceIndex = -1;
    int leastUsedTimestamp = INT_MAX;

    // Find the least recently used frame that is not pinned
    for (int i = 0; i < bm->numPages; i++) {
        if (frames[i].fixCount == 0 && frames[i].timestamp < leastUsedTimestamp) {
            leastUsedTimestamp = frames[i].timestamp;
            replaceIndex = i;
        }
    }

    // If all frames are pinned no replacement can be performed
    if (replaceIndex == -1) {
        printf("Error: All frames are pinned. Cannot replace any frame.\n");
        return RC_ERROR;
    }

    // Write back the page if it's dirty
    if (frames[replaceIndex].dirtyFlag == 1) {
        SM_FileHandle fHandle;
        if (openPageFile(bm->pageFile, &fHandle) == RC_OK) {
            writeBlock(frames[replaceIndex].pageNumber, &fHandle, frames[replaceIndex].data);
            closePageFile(&fHandle);
            writeCnt++;
        }
    }

    if (frames[replaceIndex].data != NULL) {
        free(frames[replaceIndex].data);
    }

    frames[replaceIndex] = *newPage; // Replacing least recently used frame with the new page
    frames[replaceIndex].timestamp = getNewTimestamp();
}

bool isReplaceable(int lRUIndex) {
    return lRUIndex != -1;
}

void handlePageReplacement(BM_BufferPool *const bm, PageFrame *oldPage, PageFrame *newPage) {
    writeBackPage(bm, oldPage); // If the previous page is unclean
    // Putting new page in place of the old one
    updatePageFrame(oldPage, newPage);
}


// CLOCK page replacement policy
void CLOCK(BM_BufferPool *const bm, PageFrame *newPage) {
    PageFrame *frames = (PageFrame *)bm->mgmtData;  // collection of page frames
    int index = clockPtr; 

    while (true) {
        index %= buffSize;  // Circular indexing

        // Verifying the current frame i.e reference bit at 0 is not pinned
        if (frames[index].hitLRU == 0) {
            // Rewriting if the frame is unclean
            if (frames[index].fixCount == 0) {
                writeBackPage(bm, &frames[index]);
            }

            updatePageFrame(&frames[index], newPage); // Updating of page
    
            clockPtr = (index + 1) % buffSize;  // Move the clock pointer to the next frame
            break;  
        } else {
            // For the current frame, reset the reference bit
            frames[index].hitLRU = 0;  
        }
        index++;
    }
}

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData) {
    pthread_mutex_lock(&mutex_initializer);  // Locking mutex
    int i = 0; 
    buffSize = numPages; // Setting the bufferSize to the number of pages

    PageFrame *pageFrame = malloc(sizeof(PageFrame) * numPages); // Allocating

    for(i = 0; i < numPages; i++) {
        pageFrame[i].refLRU = 0; // Initializing LRU reference bit
        pageFrame[i].hitLRU = 0; // Initializing LRU hit counter
        pageFrame[i].data = NULL; // Initializing page data pointer
    }

    for(i = 0; i < numPages; i++) { // Initializing an extra page frame
        pageFrame[i].pageNumber = -1;
        pageFrame[i].dirtyFlag = 0; // Initializing dirty flag
        pageFrame[i].fixCount = 0;
    }

    bm->pageFile = (char *)pageFileName; // Setting Page file name
    bm->mgmtData = pageFrame; 
    bm->numPages = numPages; // Setting number of pages
    bm->strategy = strategy;

    writeCnt = 0; // Reinitializing the write count
    clockPtr = 0; // Reinitializing clock pointer
    readIOCount = 0;
    pthread_mutex_unlock(&mutex_initializer); // Unlocking mutex
    return RC_OK; 
}

RC shutdownBufferPool(BM_BufferPool *const bm) {
     //  If the Buffer pool is already shut down
    if (bm->mgmtData == NULL) {
        return RC_OK;
    }

    PageFrame *pageFrames = (PageFrame *)bm->mgmtData;
    bool hasPinnedPages = false;

    // Check for pinned pages first
    for (int i = 0; i < bm->numPages; i++) {
        if (pageFrames[i].fixCount > 0) {
            hasPinnedPages = true;
            break;
        }
    }

    if (hasPinnedPages) {
        return RC_BUFFER_POOL_HAS_PINNED_PAGES;
    }

    // If there are no pinned pages, proceed with flushing and cleanup
    RC flushStatus = forceFlushPool(bm);
    if (flushStatus != RC_OK && flushStatus != RC_FILE_CLOSE_ERROR) {
        return flushStatus;
    }

    // Free allocated memory
    for (int i = 0; i < bm->numPages; i++) {
        if (pageFrames[i].data != NULL) {
            free(pageFrames[i].data);
        }
    }

    free(pageFrames);
    bm->mgmtData = NULL;

    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return RC_ERROR;
    }

    PageFrame *pageFrames = (PageFrame *)bm->mgmtData; // Retrieve the page frames.
    SM_FileHandle fileHandle;
    // File operations status
    RC status; 

    status = openPageFile(bm->pageFile, &fileHandle);
    if (status != RC_OK) {
        if (status == RC_FILE_NOT_FOUND) {
            return RC_OK;
        }
        return status; // If open fails then return status
    }

    // Iterating over the page frames in the buffer pool
    for (int index = 0; index < buffSize; ++index) {
        // Checking if the page is clean or dirty
        if (pageFrames[index].fixCount == 0 && pageFrames[index].dirtyFlag == 1) {
            SM_PageHandle pageData = pageFrames[index].data; // Get the page information
            // Writing dirty page back to the disk
            writeBlock(pageFrames[index].pageNumber, &fileHandle, pageData);
            pageFrames[index].dirtyFlag = 0; // Designate the page as clean
            writeCnt++; // Write counter increment
        }
    }

    // Once every page has been flushed, close the page file
    closePageFile(&fileHandle); // Make sure the file handle is closed.
    return RC_OK; // Show that the flush was successful.
}

//_________________________________________________________________________________________________________
//PAGE MANAGEMENT FUNCTIONS 

// This function designates a buffer pool page as unclean
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    // Transform the management data into the appropriate framework
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData;
    int buffSize = bm->numPages; // Assume that numPages represents the buffer's size

    // Go through the buffer pool pages one by one
    for (int i = 0; i < buffSize; i++) {
        if (pageFrames[i].pageNumber == page->pageNum) {
            pageFrames[i].dirtyFlag = 1;  // Mark the page as dirty
            return RC_OK; // after the page is marked, return success
        }
    }

    // Return an error if no page matches
    printf("Error: Page %d not found in buffer pool.\n", page->pageNum);
    return RC_ERROR;
}


//The buffer pool page is unpinned using this function
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    // For thread safety, lock the mutex
    pthread_mutex_lock(&mutex_unpinpage_initializer);
    
    // Access the page frames in the buffer pool
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData;
    int buffSize = bm->numPages; // Presuming the buffer size is held by numPages

    // To locate the desired page, iterate over the page frames
    for (int i = 0; i < buffSize; i++) {
        if (pageFrames[i].pageNumber == page->pageNum) {
            pageFrames[i].fixCount--;  // Reduce the number of fixes
            pthread_mutex_unlock(&mutex_unpinpage_initializer);  // Unlock mutex
            return RC_OK; // Return success
        }
    }

    // Release the mutex and return an error if the page cannot be found
    pthread_mutex_unlock(&mutex_unpinpage_initializer);
    printf("Error: Page %d not found in buffer pool.\n", page->pageNum);
    return RC_ERROR;
}


// If a page in the buffer pool is filthy, this function compels it to be written back to the disk
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    PageFrame *forcePagePointer = (PageFrame *)bm->mgmtData;
    int buffSize = bm->numPages;  // Presuming the buffer size is held by numPages
    int i = 0;

    while (i < buffSize) {
        if (forcePagePointer[i].pageNumber == page->pageNum) {
            SM_FileHandle fHandle;

            // Open the page file and take care of any potential problems
            if (openPageFile(bm->pageFile, &fHandle) != RC_OK) {
                printf("Error: Unable to open page file %s.\n", bm->pageFile);
                return RC_FILE_NOT_FOUND;
            }

            // Eliminate the unclean flag
            forcePagePointer[i].dirtyFlag = 0;

            // Write the page frame's content to disk and deal with any problems
            if (writeBlock(forcePagePointer[i].pageNumber, &fHandle, forcePagePointer[i].data) != RC_OK) {
                printf("Error: Unable to write page %d to disk.\n", forcePagePointer[i].pageNumber);
                return RC_WRITE_FAILED;
            }

            writeCnt += 1;  // Write counter increment
            break;  // After the page is written to disk, exit the loop
        }
        i++;
    }

    // After the page is forced to disk, return a success code
    return RC_OK;
}


//This function pins a buffer pool page at the given page number
RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum) {
    pthread_mutex_lock(&mutex_pinpage_initializer);
    PageFrame *pageFrames = (PageFrame *)bm->mgmtData;
    bool pageFound = false;
    int emptyFrameIndex = -1;

    // Verify whether the page is currently in the buffer pool
    for (int i = 0; i < bm->numPages; i++) {
        if (pageFrames[i].pageNumber == pageNum) {
            pageFrames[i].fixCount++;
            pageFrames[i].timestamp = getNewTimestamp(); // Update timestamp on access
            page->pageNum = pageNum;
            page->data = pageFrames[i].data;
            pageFound = true;
            pthread_mutex_unlock(&mutex_pinpage_initializer);
            return RC_OK;
        }
        if (emptyFrameIndex == -1 && pageFrames[i].pageNumber == NO_PAGE) {
            emptyFrameIndex = i;
        }
    }
    readIOCount ++;


    // If the page is not in the buffer, we must load it
    if (!pageFound) {
        SM_FileHandle fHandle;
        if (openPageFile(bm->pageFile, &fHandle) != RC_OK) {
            pthread_mutex_unlock(&mutex_pinpage_initializer);
            return RC_FILE_NOT_FOUND;
        }

        // Make sure the file contains the page
        if (ensureCapacity(pageNum + 1, &fHandle) != RC_OK) {
            closePageFile(&fHandle);
            pthread_mutex_unlock(&mutex_pinpage_initializer);
            return RC_READ_NON_EXISTING_PAGE;
        }

        // Set aside memory for the upcoming page
        char *pageData = (char *)malloc(PAGE_SIZE);
        if (pageData == NULL) {
            closePageFile(&fHandle);
            pthread_mutex_unlock(&mutex_pinpage_initializer);
            return RC_MEMORY_ALLOCATION_FAIL;
        }

        // Read the page from disk
        if (readBlock(pageNum, &fHandle, pageData) != RC_OK) {
            free(pageData);
            closePageFile(&fHandle);
            pthread_mutex_unlock(&mutex_pinpage_initializer);
            return RC_READ_NON_EXISTING_PAGE;
        }

        closePageFile(&fHandle);

        PageFrame newPage;
        newPage.pageNumber = pageNum;
        newPage.data = pageData;
        newPage.fixCount = 1;
        newPage.dirtyFlag = 0;
        newPage.timestamp = getNewTimestamp();

        // Use the empty frame if one exists
        if (emptyFrameIndex != -1) {
            pageFrames[emptyFrameIndex] = newPage;
        } else {
            // Replace a page with LRU instead of an empty frame
            LRU(bm, &newPage);
        }

        page->pageNum = pageNum;
        page->data = pageData;
    }

    pthread_mutex_unlock(&mutex_pinpage_initializer);
    return RC_OK;
}

//_________________________________________________________________________________________________________
// STATISTICS FUNCTIONS

PageNumber *getFrameContents (BM_BufferPool *const bm) {
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    PageNumber *FC = malloc(sizeof(PageNumber) * buffSize); // Allocating memory for an array of page numbers
      for(int i = 0; i < buffSize; i++) {
    // check for no page and if not, assign the actual page number.
        FC[i] = (pageFrame[i].pageNumber == -1) ? NO_PAGE : pageFrame[i].pageNumber;
    }
    return FC;
}

bool *getDirtyFlags (BM_BufferPool *const bm) {
    bool *DFs =  malloc(sizeof(bool) * buffSize); // Allocating memory for an array of page numbers
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
     for (int i = 0; i < buffSize; i++) {
       // if the page frame is a dirtyflag it is set to 1, it should be marked as true or return false. 
        DFs[i] = (pageFrame[i].dirtyFlag == 1) ? true : false;
    }
    return DFs;
}
int *getFixCounts (BM_BufferPool *const bm) {
    int *fxcs = malloc(sizeof(int) * buffSize); // Allocating memory for an array of page numbers
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
     for(int i = 0; i < buffSize; i++) {
        fxcs[i] = (pageFrame[i].fixCount != -1) ? pageFrame[i].fixCount : 0;
    }
    return fxcs;
}

// Function to get the total number of read I/O operations performed
int getNumReadIO (BM_BufferPool *const bm) {
    return readIOCount;
}

int getNumWriteIO (BM_BufferPool *const bm) {
    return writeCnt;
}