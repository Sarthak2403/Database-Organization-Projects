#include "storage_mgr.h"
#include "buffer_mgr_stat.h"
#include "buffer_mgr.h"
#include "dberror.h"
#include "test_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// var to store the current test's name
char *testName;

// check whether two the content of a buffer pool is the same as an expected content
// (given in the format produced by sprintPoolContent)
#define ASSERT_EQUALS_POOL(expected,bm,message)                    \
do {                                    \
char *real;                                \
char *_exp = (char *) (expected);                                   \
real = sprintPoolContent(bm);                    \
if (strcmp((_exp),real) != 0)                    \
{                                    \
printf("[%s-%s-L%i-%s] FAILED: expected <%s> but was <%s>: %s\n",TEST_INFO, _exp, real, message); \
free(real);                            \
exit(1);                            \
}                                    \
printf("[%s-%s-L%i-%s] OK: expected <%s> and was <%s>: %s\n",TEST_INFO, _exp, real, message); \
free(real);                                \
} while(0)

// test and helper methods
static void createDummyPages(BM_BufferPool *bm, int num);

static void testClock (void);

static void testCreatingAndReadingDummyPages (void);
static void createDummyPages(BM_BufferPool *bm, int num);
static void checkDummyPages(BM_BufferPool *bm, int num);

static void testReadPage (void);

// main method
int
main (void)
{
    initStorageManager();
    testName = "";
    testCreatingAndReadingDummyPages();
    testReadPage();
    testClock();
    return 0;
}

void
testCreatingAndReadingDummyPages (void)
{
  BM_BufferPool *bm = MAKE_POOL();
  testName = "Creating and Reading Back Dummy Pages";

  CHECK(createPageFile("testbuffer.bin"));

  createDummyPages(bm, 22);
  checkDummyPages(bm, 20);

  createDummyPages(bm, 10);
  checkDummyPages(bm, 10);

  CHECK(destroyPageFile("testbuffer.bin"));

  free(bm);
  TEST_DONE();
}


void 
createDummyPages(BM_BufferPool *bm, int num)
{
  int i;
  BM_PageHandle *h = MAKE_PAGE_HANDLE();

  CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));
  
  for (i = 0; i < num; i++)
    {
      CHECK(pinPage(bm, h, i));
      sprintf(h->data, "%s-%i", "Page", h->pageNum);
      CHECK(markDirty(bm, h));
      CHECK(unpinPage(bm,h));
    }

  CHECK(shutdownBufferPool(bm));

  free(h);
}

void 
checkDummyPages(BM_BufferPool *bm, int num)
{
  int i;
  BM_PageHandle *h = MAKE_PAGE_HANDLE();
  char *expected = malloc(sizeof(char) * 512);

  CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));

  for (i = 0; i < num; i++)
    {
      CHECK(pinPage(bm, h, i));

      sprintf(expected, "%s-%i", "Page", h->pageNum);
      ASSERT_EQUALS_STRING(expected, h->data, "reading back dummy page content");

      CHECK(unpinPage(bm,h));
    }

  CHECK(shutdownBufferPool(bm));

  free(expected);
  free(h);
}

void
testReadPage ()
{
  BM_BufferPool *bm = MAKE_POOL();
  BM_PageHandle *h = MAKE_PAGE_HANDLE();
  testName = "Reading a page";

  CHECK(createPageFile("testbuffer.bin"));
  CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));
  
  CHECK(pinPage(bm, h, 0));
  CHECK(pinPage(bm, h, 0));

  CHECK(markDirty(bm, h));

  CHECK(unpinPage(bm,h));
  CHECK(unpinPage(bm,h));

  CHECK(forcePage(bm, h));

  CHECK(shutdownBufferPool(bm));
  CHECK(destroyPageFile("testbuffer.bin"));

  free(bm);
  free(h);

  TEST_DONE();
}

// test clock replacement strategy
void
testClock(void)
{
    const int orderRequests[]= {3,2,0,8,4,2,5,0,9,8,3,2};
    int j = 0;
    BM_BufferPool *bm = MAKE_POOL();
    BM_PageHandle *h = MAKE_PAGE_HANDLE();

    // Initializing pool contents
    const char *poolContents[]= {
    "[3x0],[-1 0],[-1 0],[-1 0]", "[3x0],[2 0],[-1 0],[-1 0]", "[3x0],[2 0],[0 0],[-1 0]", "[3x0],[2 0],[0 0],[8 0]", "[4 0],[2 0],[0 0],[8 0]", "[4 0],[2 0],[0 0],[8 0]", "[4 0],[2 0],[5 0],[8 0]", "[4 0],[2 0],[5 0],[0 0]", "[9 0],[2 0],[5 0],[0 0]", "[9 0],[8 0],[5 0],[0 0]", "[9 0],[8 0],[3x0],[0 0]", "[9 0],[8 0],[3x0],[2 0]" };
    testName = "Testing CLOCK page replacement";

    // Creating page file
    CHECK(createPageFile("testbuffer.bin"));

    // Creating dummy pages
    createDummyPages(bm, 1000);

    // initiating buffer pool
    CHECK(initBufferPool(bm, "testbuffer.bin", 4, RS_CLOCK, NULL));
    
    for (int i=0;i<11;i++)
    {
        pinPage(bm,h,orderRequests[i]);
	    if(orderRequests[i] == 3) {
		    markDirty(bm,h);
        }
        unpinPage(bm,h);
        ASSERT_EQUALS_POOL(poolContents[j++], bm, "check pool content using pages");
    }
    
    forceFlushPool(bm);
    ASSERT_EQUALS_INT(2, getNumWriteIO(bm), "Write I/Os");
    ASSERT_EQUALS_INT(10, getNumReadIO(bm), "Read I/Os");
    
    free(bm);
    free(h);
    TEST_DONE();
}

// test error cases
void
testError (void)
{
    BM_BufferPool *bm = MAKE_POOL();
    BM_PageHandle *h = MAKE_PAGE_HANDLE();
    testName = "ERROR TEST";
    
    CHECK(createPageFile("testbuffer.bin"));
    
    // pinpage until buffer pool is full and then request additional page.
    CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));
    CHECK(pinPage(bm, h, 0));
    CHECK(pinPage(bm, h, 1));
    CHECK(pinPage(bm, h, 2));
    
    ASSERT_ERROR(pinPage(bm, h, 3), "try to pin page when pool is full of pinned pages with fix-count > 0");
    
    CHECK(shutdownBufferPool(bm));
    
    // try to pin page with negative page number.
    CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));
    ASSERT_ERROR(pinPage(bm, h, -10), "try to pin page with negative page number");
    CHECK(shutdownBufferPool(bm));
    
    
    // try to use uninitialized buffer pool
    ASSERT_ERROR(initBufferPool(bm, "unavailable.bin", 3, RS_FIFO, NULL), "try to init buffer pool for non existing page file");
    ASSERT_ERROR(shutdownBufferPool(bm), "shutdown buffer pool that is not open");
    ASSERT_ERROR(forceFlushPool(bm), "flush buffer pool that is not open");
    ASSERT_ERROR(pinPage(bm, h, 1), "pin page in buffer pool that is not open");
    
    
    // try to unpin, mark, or force page that is not in pool
    CHECK(initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL));
    ASSERT_ERROR(unpinPage(bm, h), "Try to unpin a page which is not available in framelist.");
    ASSERT_ERROR(forcePage(bm, h), "Try to forceflush a page which is not available in framelist.");
    ASSERT_ERROR(markDirty(bm, h), "Try to markdirty a page which is not available in framelist.");
    CHECK(shutdownBufferPool(bm));
    
    // done remove page file
    CHECK(destroyPageFile("testbuffer.bin"));
    
    free(bm);
    free(h);
    TEST_DONE();
}
