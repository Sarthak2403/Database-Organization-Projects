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
    printf("Creating page file: %s\n", fileName);
    
    if (fileName == NULL)
    {
        RC_message = "Invalid file name.";
        return RC_ERROR;
    }

    FILE *filePointer = fopen(fileName, "w+");
    if (filePointer == NULL)
    {
        RC_message = "File creation failed.";
        return RC_ERROR;
    }

    SM_PageHandle pageHandle = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (pageHandle == NULL)
    {
        fclose(filePointer);
        RC_message = "Memory allocation failed.";
        return RC_ERROR;
    }

    size_t bytesWritten = fwrite(pageHandle, sizeof(char), PAGE_SIZE, filePointer);
    if (bytesWritten < PAGE_SIZE)
    {
        free(pageHandle);
        fclose(filePointer);
        RC_message = "Failed to write data to the file.";
        return RC_WRITE_FAILED;
    }

    free(pageHandle);
    fclose(filePointer);

    RC_message = "File created and initialized successfully.";
    printf("File creation successful: %s\n", fileName);
    return RC_OK;
}

extern RC openPageFile(char *fileName, SM_FileHandle *fileHandle)
{
    printf("Opening page file: %s\n", fileName);

    if (fileName == NULL || fileHandle == NULL)
    {
        RC_message = "Invalid file name or file handle.";
        return RC_ERROR;
    }

    FILE *filePointer = fopen(fileName, "r+");
    if (filePointer == NULL)
    {
        RC_message = "File not found.";
        return RC_FILE_NOT_FOUND;
    }

    if (fseek(filePointer, 0, SEEK_END) != 0)
    {
        fclose(filePointer);
        RC_message = "Error while seeking the end of the file.";
        return RC_ERROR;
    }

    long fileSize = ftell(filePointer);
    if (fileSize == -1)
    {
        fclose(filePointer);
        RC_message = "Error while determining file size.";
        return RC_ERROR;
    }

    int totalPages = (int)(fileSize / PAGE_SIZE);
    printf("Total number of pages: %d\n", totalPages);

    fileHandle->fileName = fileName;
    fileHandle->mgmtInfo = filePointer;
    fileHandle->totalNumPages = totalPages;
    fileHandle->curPagePos = 0;

    rewind(filePointer);

    RC_message = "File opened successfully.";
    printf("File opened successfully: %s\n", fileName);
    return RC_OK;
}

extern RC closePageFile(SM_FileHandle *fileHandle)
{
    printf("Closing page file: %s\n", fileHandle->fileName);

    if (fileHandle == NULL || fileHandle->mgmtInfo == NULL)
    {
        RC_message = "Invalid file handle.";
        return RC_ERROR;
    }

    int closeResult = fclose(fileHandle->mgmtInfo);
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
    printf("Destroying page file: %s\n", fileName);

    int remove_result = remove(fileName);
    if (remove_result != 0)
    {
        RC_message = "Destroying file has failed.";
        return RC_ERROR;
    }

    RC_message = "Destroyed the file successfully.";
    return RC_OK;
}

/* ............Block Operations............... */

extern RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    printf("Reading block %d from file: %s\n", pageNum, fHandle->fileName);

    if (pageNum >= fHandle->totalNumPages || pageNum < 0)
    {
        printf("Error: Trying to read a non-existing page %d\n", pageNum);
        return RC_READ_NON_EXISTING_PAGE;
    }

    fileptr = fopen(fHandle->fileName, "r");
    if (fileptr == NULL)
    {
        printf("Error: File %s not found.\n", fHandle->fileName);
        return RC_FILE_NOT_FOUND;
    }

    if (fseek(fileptr, pageNum * PAGE_SIZE, SEEK_SET) != 0)
    {
        printf("Error: Failed to seek to the correct page %d\n", pageNum);
        fclose(fileptr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    size_t readBytes = fread(memPage, sizeof(char), PAGE_SIZE, fileptr);
    if (readBytes < PAGE_SIZE)
    {
        printf("Error: Failed to read page %d\n", pageNum);
        fclose(fileptr);
        return RC_READ_FAILED;
    }

    fHandle->curPagePos = pageNum;  // Update the current page position
    fclose(fileptr);

    printf("Successfully read block %d from file: %s\n", pageNum, fHandle->fileName);
    return RC_OK;
}

extern int getBlockPos(SM_FileHandle *fHandle)
{
    if (fHandle == NULL || fHandle->fileName == NULL)
    {
        printf("Error: Invalid file handle.\n");
        return RC_FILE_HANDLE_NOT_INIT;
    }

    if (fopen(fHandle->fileName, "r") != NULL)
    {
        printf("Current block position: %d\n", fHandle->curPagePos);
        return fHandle->curPagePos;
    }
    else
    {
        printf("Error: File %s not found.\n", fHandle->fileName);
        return RC_FILE_NOT_FOUND;
    }
}

extern RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    return readBlock(0, fHandle, memPage);
}

extern RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle->curPagePos <= 0)
        return RC_READ_NON_EXISTING_PAGE;

    return readBlock(fHandle->curPagePos - 1, fHandle, memPage);
}

extern RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle->curPagePos < 0 || fHandle->curPagePos >= fHandle->totalNumPages)
        return RC_READ_NON_EXISTING_PAGE;

    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

extern RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle->curPagePos >= fHandle->totalNumPages - 1)
        return RC_READ_NON_EXISTING_PAGE;

    return readBlock(fHandle->curPagePos + 1, fHandle, memPage);
}

extern RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

/* ............Operations for writing the Blocks............... */

extern RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    printf("Writing block %d to file: %s\n", pageNum, fHandle->fileName);

    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
    {
        printf("Error: Invalid file handle or file not open.\n");
        return RC_FILE_HANDLE_NOT_INIT;
    }

    FILE *fileptr = (FILE *)fHandle->mgmtInfo;

    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
    {
        printf("Error: Invalid page number %d\n", pageNum);
        return RC_WRITE_FAILED;
    }

    long int write_offset = pageNum * PAGE_SIZE;

    if (fseek(fileptr, write_offset, SEEK_SET) != 0)
    {
        printf("Error: Failed to seek to the correct page for writing.\n");
        return RC_WRITE_FAILED;
    }

    size_t write_result = fwrite(memPage, sizeof(char), PAGE_SIZE, fileptr);

    if (write_result != PAGE_SIZE)
    {
        printf("Error: Failed to write block %d\n", pageNum);
        return RC_WRITE_FAILED;
    }

    printf("Write operation successful on block %d\n", pageNum);
    return RC_OK;
}

extern RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

RC appendEmptyBlock(SM_FileHandle *fHandle)
{
    if (fHandle == NULL || fHandle->mgmtInfo == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }
    
    FILE *fileptr = (FILE *)fHandle->mgmtInfo;
    fseek(fileptr, 0, SEEK_END);
    
    char emptyPage[PAGE_SIZE] = {0};
    size_t bytesWritten = fwrite(emptyPage, sizeof(char), PAGE_SIZE, fileptr);
    
    if (bytesWritten == PAGE_SIZE)
    {
        fHandle->totalNumPages++;
        return RC_OK;
    }
    else
    {
        return RC_WRITE_FAILED;
    }
}
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }
    
    while (fHandle->totalNumPages < numberOfPages)
    {
        RC rc = appendEmptyBlock(fHandle);
        if (rc != RC_OK)
        {
            return rc;
        }
    }
    
    return RC_OK;
}

