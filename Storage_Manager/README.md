## CS 525 ADO Assignment - 1 - Storage Manager - Group 36

Team Members and their Contribution:

Bhavana  Polakala - A20539792, bpolakala@hawk.iit.edu, 33.33 %

Sarthak Sonpatki -  A20579456, ssonpatki@hawk.iit.edu, 33.33 %	

Vishwas Ramakrishna - A20552892, vramakrishna@hawk.iit.edu, 33.33 %		

# Storage Manager:-

This assignment uses file manipulation techniques to read, write and retrieve data/pages. The pages are stored in the disk in memory and required to be pulled through to perform above mentioned operations.

# Folder Contents:-:

 dberror.h: RC code errors with their definitions are mentioned in this file.

 dberror.c: Functions such as errorMessage and printError are mentioned in this file.

storage_mgr.h: This file defines SM_PageHandle and provides a structure and attributes for SM_FileHandle. It declares functions for reading, writing, and manipulating files.

storage_mgr.c: This file contains the methods of the functions mentioned in the storage_mgr.h file. 

test_helper.h: A set of macros needed for testing is defined in this file.

test_assign1_1.c: The methods mentioned in this file are  testSinglePageContent and testCreateOpenClose which are used test the program.

 Makefile: This file gets created on running ‘make’ command which is used to produce the binary test_assign1 from test_assign1_1.c.

 Readme.txt: Detailed description of the program is mentioned 
  
# Functions

1) extern void initStorageManager (void):
This function initiates up the application for the storage manager.

2) extern RC createPageFile (char *fileName):
This function initializes a newly created page file called with a blank page. If everything goes well, it returns RC_OK; if not, it returns an error code, as RC_ERROR in the case of a failed file creation or RC_WRITE_FAILED in the case of a problem writing data into the file. 

3) extern RC openPageFile (char *fileName, SM_FileHandle *fHandle):
This function loads an existing page file, obtains its contents, including its current location and number of pages, and adds it into our 'fHandle' structure. It returns RC_FILE_NOT_FOUND if the file cannot be located. An effective opening yields RC_OK.

4) extern RC closePageFile (SM_FileHandle *fHandle):
With the file handle 'fHandle' offered, this method shuts an open page file connected with it. If the file is properly closed, it returns RC_OK; if not, it returns RC_ERROR.

5) extern RC destroyPageFile (char *fileName):
Using 'remove', this function removes (wipes out) the page file with the name 'fileName'. If it is successful, RC_OK is returned. If there are any problems with the deletion, RC_ERROR is returned.

6) extern RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage):
The function is used to read the block and store the data in memPage and also store the current position of the page in the fileptr pointer.

7) extern int getBlockPos (SM_FileHandle *fHandle):
The function is used to get the current position of the block. 

8) extern RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
The function is used to read the first block in the disk. 

9) extern RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
The function is used to read the previous block based on the current page position.

10) extern RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
This function is used to read the contents of the current page based on the pointer.

11) extern RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
The function is used to read the next block based on the current page position.

12) extern RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
The function is used to read the last block in the disk.

13) extern RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage):
The writeBlock function handles validations and error checking for seek positions and write operations, writing the contents of a memory page to a given page number in a file. 


14) extern RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage):
The goal of this method is to write and save the contents of the memPage on the current page. If there are any problems finding the necessary position or writing the contents, it returns the relevant error indicators.

15) extern RC appendEmptyBlock (SM_FileHandle *fHandle)
The goal of this strategy is to produce more pages. Here, the file is sought to its conclusion, an empty block is allocated and written, and the number of pages is increased by one.

16) extern RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)
This technique adds empty blocks for the necessary additional number of pages after determining whether there is enough space to hold the given number of pages.

### Procedures for Implementation:
Process 1:

1) Navigate to the "our project" directory in Visual Studio Code by opening a new terminal and typing "cd filename."
2) We make use of WSL (Ubuntu) to execute the program.
3) Press Enter after typing "gcc -o test_assign1 storage_mgr.c dberror.c test_assign1_1.c". A test_assign1 executable file will result from this.
4)To launch the file that's executable and test the program, type "./test_assig1".
5) After testing, use "clear" to eliminate all unwanted files.
6) Enter "valgrind --leak-check=full./test_assign1" and hit Enter to check for memory leaks. 
7) Use the command rm test_assign1 to delete the executable file.

Process 2:
1) A makefile has been made in order to run our program.
2) Type "make" in the newly opened terminal to produce the executable file test_assign1.
3)Next, in order to validate our program, we must run the file using the make run command.
4) Then, to remove all executable files and rerun, we may utilize "make clean" in a Linux environment and "make clean" in a Windows environment. 
5) Enter "valgrind --leak-check=full./test_assign1" and hit Enter to check for memory leaks.

