#include <stdio.h>
#include <stdlib.h>
#include "storage_mgr.h"
#include <sys/stat.h>
#include "dberror.h"
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


FILE *fileptr;
char *RC_message;

extern void initStorageManager()
{
    printf("\n <-----Initializing Storage Manager----->\n ");
}


/* ............Page Files modifying............... */


extern RC createPageFile(char *fileName)
{
    // verifying the fileName input
    if (fileName == NULL)
    {
        RC_message = "Invalid file name.";
        return RC_ERROR;
    }

    // Use the "w+" mode for opening the file 
    FILE *filePointer = fopen(fileName, "w+");

    // Verify that the file has been made successfully.
    if (filePointer == NULL)
    {
        RC_message = "File creation failed.";
        return RC_ERROR;
    }

    // For a page, use calloc to set up memory 
    SM_PageHandle pageHandle = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (pageHandle == NULL)
    {
        fclose(filePointer);
        RC_message = "Memory allocation failed.";
        return RC_ERROR;
    }

    // Add the blank page to this file.
    size_t bytesWritten = fwrite(pageHandle, sizeof(char), PAGE_SIZE, filePointer);

    // Verify if the task to write was accomplished.
    if (bytesWritten < PAGE_SIZE)
    {
        free(pageHandle); // Allotted memory free
        fclose(filePointer); // file closing
        RC_message = "Failed to write data to the file.";
        return RC_WRITE_FAILED;
    }

    // shut the file and release any memory.
    free(pageHandle);
    fclose(filePointer);

    RC_message = "File created and initialized successfully.";
    return RC_OK;
}



extern RC openPageFile(char *fileName, SM_FileHandle *fileHandle)
{
    // Verify the validity of Name and Handle of the file.
    if (fileName == NULL || fileHandle == NULL)
    {
        RC_message = "Invalid file name or file handle.";
        return RC_ERROR;
    }

    //In "r+" mode , opening the file.
    FILE *filePointer = fopen(fileName, "r+");

    // Verify that the file has been opened properly.
    if (filePointer == NULL)
    {
        RC_message = "File not found.";
        return RC_FILE_NOT_FOUND;
    }

    // To find the size of the file, shift pointer of the file to the final part of the file.
    if (fseek(filePointer, 0, SEEK_END) != 0)
    {
        fclose(filePointer);  // Close the file to finish cleaning up.
        RC_message = "Error while seeking the end of the file.";
        return RC_ERROR;
    }

    // Use ftell to find the size of the file.
    long fileSize = ftell(filePointer);
    if (fileSize == -1)
    {
        fclose(filePointer);  // Close the file to finish cleaning up.
        RC_message = "Error while determining file size.";
        return RC_ERROR;
    }

    // Determine how many pages there are in the entire file.
    int totalPages = (int)(fileSize / PAGE_SIZE);
    printf("Total number of pages: %d\n", totalPages);

    //Fill in the layout of the file handle
    fileHandle->fileName = fileName;
    fileHandle->mgmtInfo = filePointer;
    fileHandle->totalNumPages = totalPages;
    fileHandle->curPagePos = 0;

    // To the file's beginning, reset the pointer.
    rewind(filePointer);

    RC_message = "File opened successfully.";
    return RC_OK;
}


extern RC closePageFile(SM_FileHandle *fileHandle)
{
    // To continue, check that the fileHandle is correct.
    if (fileHandle == NULL || fileHandle->mgmtInfo == NULL)
    {
        RC_message = "Invalid file handle.";
        return RC_ERROR;
    }

    // Try shutting the file and see what happens.
    int closeResult = fclose(fileHandle->mgmtInfo);

    // According on the result, alter the RC_message and return the proper status code.
    if (closeResult == 0)
    {
        RC_message = "File closed successfully.";
        return RC_OK;
    }
    else
    {
        RC_message = "Failed to close the file.";
        return RC_FILE_CLOSE_ERROR;
    }
}


extern RC destroyPageFile(char *fileName)
{
    bool file_status = true;
    // deleting the file and observing the result
    int remove_result = remove(fileName);
    // We are now validating the results, thus remove function returns 0 if successful.
    if (remove_result != 0)
    {
        RC_message = "Destroying File has failed.";
        return RC_ERROR;
    }
    RC_message = "Destroyed the file successfully";
    return RC_OK;

    while (file_status != true) {
        return RC_ERROR;
    }
}


/* ............Block Operations............... */


extern RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
	// Checking for page numbers to be less than Total number of oages and 0
	if (pageNum > fHandle->totalNumPages || pageNum < 0)
        	return RC_READ_NON_EXISTING_PAGE;

	// Opening the file in read mode
	fileptr = fopen(fHandle->fileName, "r");

	if(fileptr == NULL)
		return RC_FILE_NOT_FOUND;
	
	// Setting the pointer position of the file stream.
	if(fseek(fileptr, (pageNum * PAGE_SIZE), SEEK_SET) == 0) {
		// Reading the content and storing it.
		fread(memPage, sizeof(char), PAGE_SIZE, fileptr);
	} else {
		return RC_READ_NON_EXISTING_PAGE; 
	}
    	
	// Setting the current page position
	fHandle->curPagePos = ftell(fileptr); 
	
	// Closing of file stream.     	
	fclose(fileptr);
    return RC_OK;
}


extern int getBlockPos(SM_FileHandle *fHandle){
    if (fHandle == NULL || fHandle->fileName == NULL) {
        // Error handling for file handle if it is invalid
        RC_message="File dosen't exists.";
		return RC_FILE_HANDLE_NOT_INIT;
    }
	else
	{
		if((fopen(fHandle->fileName,"r")) != NULL)// check whether file exist of not.
		{
			return fHandle->curPagePos;
		}
		else
		{
			RC_message="File Not Found";
			return RC_FILE_NOT_FOUND;
		}
	} 

}

// Reading of First Block
extern RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    if (fHandle == NULL || fHandle->fileName == NULL) {
        // Error handling for file handle if it is invalid
        RC_message="File dosen't exists.";
		return RC_FILE_HANDLE_NOT_INIT;
    }
	else{
        // Reading of the Block and storing its position in memPage
        return  readBlock(0,fHandle,memPage);
    }
}

// Reading of Block before the current block
extern RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int currentPage = fHandle->curPagePos-1;
    if(fHandle==NULL||fHandle->fileName == NULL){
        RC_message="File dosen't exists."; // Error handling for file handle if it is invalid
		return RC_FILE_HANDLE_NOT_INIT;
    }
    else{
        // Checking for the current page is the first page or invalid and returning 'Non- Existing page Error'
        if(fHandle->curPagePos <=0){
            return RC_READ_NON_EXISTING_PAGE;
        }
        else{
            // return (currentPage,memPage);
            return RC_OK;
        }
    }
}

// Reading of current block 
extern RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    if(fHandle==NULL||fHandle->fileName == NULL){
        // Error handling for file handle if it is invalid
        RC_message="File dosen't exists.";
		return RC_FILE_HANDLE_NOT_INIT;
    }
    else{
        // Checking for current page to be in between first and the last one
        if (fHandle->curPagePos < 0 || fHandle->curPagePos >= fHandle->totalNumPages) {
            return RC_READ_NON_EXISTING_PAGE; 
        }
        else{
            // return(fHandle->curPagePos,memPage);
            return RC_OK;
        }
    }
}

// Reading for next Block after the current block
extern RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    if(fHandle==NULL||fHandle->fileName == NULL){
        // Error handling for file handle if it is invalid
        RC_message="File dosen't exists.";
		return RC_FILE_HANDLE_NOT_INIT;
    }
    else{
        if (fHandle->curPagePos >= fHandle->totalNumPages-1) {
            return RC_READ_NON_EXISTING_PAGE; 
        }
        else{
            // return(fHandle->curPagePos+1,memPage);
            return RC_READ_FAILED;
        }
    }
}

extern RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    if(fHandle==NULL||fHandle->fileName == NULL){
        RC_message="File dosen't exists.";
		return RC_FILE_HANDLE_NOT_INIT;
    }
    else{
        // Checking if the current page is last
        if (fHandle->totalNumPages <= 0) {
            return RC_READ_NON_EXISTING_PAGE; 
        }
        else{
            // return(fHandle->totalNumPages-1,memPage);
            return RC_OK;
        }
    }
}


/* ............Operations for writing the Blocks............... */


extern RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    // Validate the file handle and its management info (file pointer)
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
    {
        RC_message = "File handle or management info not initialized properly";
        return RC_FILE_HANDLE_NOT_INIT;
    }

    FILE *fileptr = (FILE *)fHandle->mgmtInfo;

    // Validate the page number
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
    {
        RC_message = "Writing to a non-existing page. Please recheck the page number.";
        return RC_WRITE_FAILED;
    }

    // Calculate the offset to find the position of the pointer in the file for the requested pageNum
    long int write_offset = pageNum * PAGE_SIZE;

    // Seek to the correct position in the file
    if (fseek(fileptr, write_offset, SEEK_SET) != 0)
    {
        RC_message = "Invalid seek position. Please recheck the file position.";
        return RC_WRITE_FAILED;
    }

    // Perform the write operation
    size_t write_result = fwrite(memPage, sizeof(char), PAGE_SIZE, fileptr);

    // Verify whether the data has been written properly
    if (write_result != PAGE_SIZE)
    {
        RC_message = "Write operation failed. Please recheck.";
        return RC_WRITE_FAILED;
    }

    // If the write operation is successful
    RC_message = "Write operation was successful";
    return RC_OK;
}


extern RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    // Check if the file handle or memory page is null
    if (fHandle == NULL || memPage == NULL)
        return RC_NULL_POINTER;

    // Get the current block position
    int currentBlockPos = getBlockPos(fHandle);

    // Ensure the current block position is valid
    if (currentBlockPos < 0)
        return RC_INVALID_BLOCK_POSITION;

    // Write the content to the disk at the current block position
    RC writeStatus = writeBlock(currentBlockPos, fHandle, memPage);

    // Return the status of the write operation
    return writeStatus;
}
extern RC appendEmptyBlock(SM_FileHandle *fHandle)
{
    // Check if the file handle is null
    if (fHandle == NULL) 
    {
        RC_message = "File handle not initialized properly";
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Get the file pointer from the management info in fHandle
    FILE *fileptr = (FILE *)fHandle->mgmtInfo;

    // Check if the file pointer is null
    if (fileptr == NULL)
    {
        RC_message = "File not found";
        return RC_FILE_NOT_FOUND;
    }

    // Seek to the end of the file
    if (fseek(fileptr, 0, SEEK_END) != 0)
    {
        RC_message = "Seeking position failed";
        return RC_WRITE_FAILED;
    }

    // Allocate memory for the new block and initialize it with zeros
    SM_PageHandle emptyBlock = (SM_PageHandle)calloc(PAGE_SIZE, 1);
    if (emptyBlock == NULL)
    {
        RC_message = "Memory allocation for new block failed";
        return RC_MEMORY_ALLOCATION_FAIL;
    }

    // Write the empty block to the file
    size_t written = fwrite(emptyBlock, 1, PAGE_SIZE, fileptr);
    if (written < PAGE_SIZE)
    {
        free(emptyBlock);  // Free allocated memory on error
        RC_message = "Writing empty block to file failed";
        return RC_WRITE_FAILED;
    }

    // Update the total number of pages in the file handle
    fHandle->totalNumPages++;

    // Free the allocated memory for the empty block
    free(emptyBlock);

    // Return success
    return RC_OK;
}

extern RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
    // Validate the file handle
    if (fHandle == NULL)
    {
        RC_message = "File handle not initialized properly";
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // If the file already has enough capacity, return success
    if (numberOfPages <= fHandle->totalNumPages)
    {
        return RC_OK;
    }

    // Calculate the number of pages to add
    int requiredPages = numberOfPages - fHandle->totalNumPages;

    // Append the required number of empty blocks
    for (int i = 0; i < requiredPages; i++)
    {
        RC status = appendEmptyBlock(fHandle);
        if (status != RC_OK)
        {
            RC_message = "Failed to append empty block";
            return status; // Return the error code from appendEmptyBlock
        }
    }

    return RC_OK;
}
