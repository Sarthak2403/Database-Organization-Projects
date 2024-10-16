#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <ctype.h>

RecMgr *recManager; 
size_t intSize = sizeof(int);
size_t schemaSize = sizeof(Schema);

int flagCheck = -1, ATTRIBUTE_SIZE = 15; 
typedef char *charPointer;
bool activeFlag = true; 
typedef int *intPointer;

RC returnCode;

void memcpyFunc(void *destination, const void *source, int typeSize);
void verifyEmptyTable();
RC funcPinOrUnpinPage(RecMgr *recManager, bool unpin, int pageNum);
RC readTableInfo(int value);
RC funTwoObjPinUnpin(BM_BufferPool *bufferPool, BM_PageHandle *pageHandle, bool unpin, int pageNumber);

// Function to fine available slots in the page
int findAvailableSlot(charPointer charData, int recordSize) 
{
	int idx;  // Current index
	int tmp = -1; // variable to store available slot index
	int limit = 0; // Variable to calculate the total number of records in the page
	bool foundFlag = false; // Flag if available slot has been found

	limit = PAGE_SIZE / recordSize;
	flagCheck = 1;
	for (idx = 0; recordSize > 0 && idx < limit; idx++) 
	{
        // Calculating the starting index of current record in the page
		int indexValue = idx * recordSize;
		readTableInfo(indexValue);
		if (charData[indexValue] == '-') // If the slot is marked as unavailable (represented by '-'), skipping to the next one
		{
			continue;
		} 
		else if (charData[indexValue] == '+') // If the slot is marked as already taken (represented by '+'), reset the search
		{
			verifyEmptyTable();
			tmp = -1;
			foundFlag = false;
		} 
		else 
		{
			tmp = idx;
			foundFlag = true;
		}
		if (tmp > -1) // Returning the index of the available slot
			return tmp;
	}
    // If no available slot is found, return -1
	return tmp;
}

// Record manager initialization function
extern RC initRecordManager(void *mgmtData)
{
	activeFlag = false; // Deactivating the flag to represent record manager is not active
	RC retValue = RC_OK;
	initStorageManager(); // Initializing storage manager
	return retValue;
}

// Shutdown record manager function
extern RC shutdownRecordManager()
{
	shutdownBufferPool(&recManager->bufferPool);
    // Setting the record manager pointer to NULL and freeing memory
	recManager = NULL;
	free(recManager);

	returnCode = RC_OK;
	activeFlag = true;
	return returnCode;
}

// Create table function
extern RC createTable(charPointer tableName, Schema *schema) 
{
	int recordManagerSize = sizeof(RecMgr);
	recManager = (RecMgr *)malloc(recordManagerSize); // Allocating memory
	initBufferPool(&recManager->bufferPool, tableName, 100, RS_LRU, NULL); // Initializing the buffer pool with the specified parameters

    // Creating a buffer to hold page data
	char pageData[PAGE_SIZE];
	charPointer currentDataPointer = pageData; // Pointer used for page data

	int zeroValue = 0;
	*(intPointer)currentDataPointer = zeroValue;
	currentDataPointer += intSize;

	*(intPointer)currentDataPointer = 1; // Storing the first page number
	currentDataPointer += intSize;

	readTableInfo(1);
	*(intPointer)currentDataPointer = schema->numAttr; // Storing the number of attributes
	currentDataPointer += intSize;

	activeFlag = true;
	*(intPointer)currentDataPointer = schema->keySize; // Storing the size of the key.
	currentDataPointer += intSize;

	for (int i = 0; i < schema->numAttr; i++) 
	{
		strncpy(currentDataPointer, schema->attrNames[i], ATTRIBUTE_SIZE);
		currentDataPointer += ATTRIBUTE_SIZE;
        
        // data type of the attribute
		*(intPointer)currentDataPointer = (int)schema->dataTypes[i];
		currentDataPointer += intSize;

		verifyEmptyTable();
		*(intPointer)currentDataPointer = (int)schema->typeLength[i];
		currentDataPointer += intSize;
	}

	SM_FileHandle fileHandle;
    // Creating the page file for the table and opening it
	if (createPageFile(tableName) == RC_OK) 
	{
		verifyEmptyTable();
		openPageFile(tableName, &fileHandle);
	}
	if (writeBlock(0, &fileHandle, pageData) != RC_OK) // Writing the initial page data (schema and metadata) to block 0 of the file.
	{
		printf("Error writing block.");
	}
	return RC_OK;
}

// Open table function
extern RC openTable(RM_TableData *relation, charPointer tableName) 
{
	SM_PageHandle pageHandle; // Pointer to handle page data
	int dataTypeSize = sizeof(DataType);

	relation->mgmtData = recManager;

    // Pinning the first page in the buffer pool to read the table metadata.
	pinPage(&recManager->bufferPool, &recManager->pageHandle, 0);
	pageHandle = (charPointer)recManager->pageHandle.data;

    // Extracting the tuple count from the page.
	recManager->tupleCount = *(intPointer)pageHandle;
	pageHandle += intSize;

	recManager->freePage = *(intPointer)pageHandle; // Extracting the free page number
	activeFlag = false;
	pageHandle += intSize;

    // Extract the number of attributes (columns) in the table schema.
	int numAttr = *(intPointer)pageHandle;
	pageHandle += intSize;

    // Allocating memory for the schema structure and its components
	Schema *schema = (Schema *)malloc(schemaSize);
	schema->dataTypes = (DataType *)malloc(dataTypeSize * numAttr);
	schema->attrNames = (charPointer *)malloc(sizeof(charPointer) * numAttr);
	schema->numAttr = numAttr;

    // Allocating memory for storing the length of each attribute data type
	int *typeLenArray = (intPointer)malloc(intSize * numAttr); 
	schema->typeLength = typeLenArray;

	for (int i = 0; i < numAttr; i++) 
	{
		schema->attrNames[i] = (charPointer)malloc(ATTRIBUTE_SIZE); // Allocating memory for each attribute name.
		strncpy(schema->attrNames[i], pageHandle, ATTRIBUTE_SIZE); // Copy the attribute name from the page to the schema
		pageHandle += ATTRIBUTE_SIZE;

		schema->dataTypes[i] = *(intPointer)pageHandle;
		pageHandle += intSize;

		schema->typeLength[i] = *(intPointer)pageHandle;
		pageHandle += intSize;
	}

    // Unpin the page after reading the table metadata
	funcPinOrUnpinPage(recManager, true, 0);

	relation->schema = schema;
	return RC_OK;
}

// Close table function
extern RC closeTable(RM_TableData *relation) 
{
	activeFlag = false; // Marking the table as inactive
	RecMgr *recMgr = relation->mgmtData; // Retrieving the record manager associated with the table
	shutdownBufferPool(&recMgr->bufferPool);
	return returnCode;
}

// Delete table function
extern RC deleteTable(charPointer tableName) 
{
	activeFlag = true;
	if (activeFlag != false) // Checking if the active flag is true
	{
        // Destroy the page file associated with the table name
		return destroyPageFile(tableName);
	}
}

// Function to get number of tuples
extern int getNumTuples(RM_TableData *relation) 
{
	RecMgr *recMgr = relation->mgmtData; // Retrieve the record manager from the table data
	return recMgr->tupleCount; // Returning the number of tuples 
}

// Function to verify if table is Empty
void verifyEmptyTable() 
{
	return;
}

// Function to read table data
RC readTableInfo(int value) 
{
	return RC_OK;
}


//=================================================================================

extern RC insertRecord(RM_TableData *iReln, Record *iRec)
{
    RecMgr *record_mgr_obj = iReln->mgmtData;
    RID *recRID = &iRec->id;
    charPointer charPgHandleData, tempPgHandleData, newPgHandleData;
    int recSize = 0;

    // If the record size isn't set already, get it
    if (recSize == 0)
    {
        recSize = getRecordSize(iReln->schema);
        recRID->page = record_mgr_obj->freePage;

        // The record manager page should be pinned
        if (activeFlag)
        {
            verifyEmptyTable();
        }
        else
        {
            printf("Warning- Flag is false. Cannot check the table.");
        }

        funcPinOrUnpinPage(record_mgr_obj, false, recRID->page);
    }

    // Retrieve the page's data
    tempPgHandleData = record_mgr_obj->pageHandle.data;
    charPgHandleData = tempPgHandleData;

    // Locate the space on the page where the new record should go
    recRID->slot = findAvailableSlot(charPgHandleData, recSize);

    // Loop to locate an empty spot on the page
    while (recRID->slot == -1)
    {
        funcPinOrUnpinPage(record_mgr_obj, true, 0); // Unpinning the current page
        recRID->page++; // Incrementing the page number
        funcPinOrUnpinPage(record_mgr_obj, false, recRID->page); // Pinning the new page

        charPgHandleData = record_mgr_obj->pageHandle.data;
        recRID->slot = findAvailableSlot(charPgHandleData, recSize);
    }

    // Verify the validity of charPgHandleData
    if (charPgHandleData == NULL)
    {
        printf("Error- charPgHandleData is NULL, Inside Insert Record Function");
    }
    else
    {
        markDirty(&record_mgr_obj->bufferPool, &record_mgr_obj->pageHandle);
        newPgHandleData = charPgHandleData;

        // Verify that the table is empty
        verifyEmptyTable();

        // Move the cursor to the appropriate location
        newPgHandleData += (recRID->slot * recSize);
        *newPgHandleData = '+';
        memcpy(++newPgHandleData, iRec->data + 1, recSize - 1);

        // When processing is finished, unpin the page
        if (newPgHandleData != charPgHandleData && flagCheck == 0)
        {
            activeFlag = true;
            funcPinOrUnpinPage(record_mgr_obj, activeFlag, 0);
            record_mgr_obj->tupleCount++;
            printf("Info: The value of chk is not zero");
        }
    }

    // Pin the table's first page
    readTableInfo(flagCheck);
    funcPinOrUnpinPage(record_mgr_obj, false, 0);

    return RC_OK;
}


RC funcPinOrUnpinPage(RecMgr *objRecMgr, bool unpin, int pgno)
{
    int pinVal;

    if (unpin)
    {
        // If unpin is true, unpin the page
        pinVal = unpinPage(&objRecMgr->bufferPool, &objRecMgr->pageHandle);
        activeFlag = true;
    }
    else
    {
        // If unpin is false, pin the page
        pinVal = pinPage(&objRecMgr->bufferPool, &objRecMgr->pageHandle, pgno);
        flagCheck = 0;
    }

    return pinVal;
}


extern RC deleteRecord(RM_TableData *iRel, RID iRelID)
{
    // Verify the management data
    if (iRel->mgmtData == NULL) {
        return RC_ERROR; // Missing management data prevents the page from being pinned.
    }

    // Cast RecMgr* to mgmtData
    RecMgr *mgr = (RecMgr *)iRel->mgmtData;

    // The page with the record should be pinned
    funcPinOrUnpinPage(mgr, false, iRelID.page);

    // Retrieving the page data
    charPointer iRecData = mgr->pageHandle.data;
    if (iRecData == NULL) {
        return RC_ERROR; // Page data is NULL here
    }

    // Determine the record's location and dimensions on the page
    int recordSize = getRecordSize(iRel->schema);
    int recordStartPosition = iRelID.slot * recordSize;

    // Replace the record's content with '-' to mark it as removeds
    for (int i = 0; i < recordSize; ++i)
    {
        iRecData[recordStartPosition + i] = '-';
    }

    // Designate the page as dirty and save it to the disk
    if (markDirty(&mgr->bufferPool, &mgr->pageHandle) == RC_OK)
    {
        forcePage(&mgr->bufferPool, &mgr->pageHandle);
    }
    else
    {
        printf("Page Not Dirty");
    }

    return RC_OK; // The record was successfully deleted
}


extern RC updateRecord(RM_TableData *iRel, Record *iRecords)
{
    // Verify the management data
    if (iRel->mgmtData == NULL) {
        return RC_ERROR;
    }

    // Cast RecMgr* to mgmtData
    RecMgr *mgr = (RecMgr *)iRel->mgmtData;

    // The page with the record should be pinned
    funcPinOrUnpinPage(mgr, false, iRecords->id.page);

    // Determine the record size and confirm
    int recordSize = getRecordSize(iRel->schema);
    if (recordSize <= 0) {
        printf("Error: Invalid record size\n");
        return RC_INVALID_RECORD_SIZE;
    }

    // Obtain the page data pointer
    charPointer recData = mgr->pageHandle.data;
    if (recData == NULL) {
        return RC_ERROR;
    }

    // Determine where the record that needs to be modified is located
    RID recId = iRecords->id;
    recData += (recId.slot * recordSize);

    // Updating the record data
    *recData = '+';
    memcpy(recData + 1, iRecords->data + 1, recordSize - 1);

    // Mark the page as dirty
    RC updateStatus = markDirty(&mgr->bufferPool, &mgr->pageHandle);

    // If the update was successful, unpin the page
    if (updateStatus == RC_OK) {
        funcPinOrUnpinPage(mgr, true, 0);
        printf("Record has been updated\n");
    }

    return updateStatus;
}


extern RC getRecord(RM_TableData *iReln, RID iRecordID, Record *iRec)
{
    // Verify the management data
    RecMgr *recordMgr = (RecMgr *)iReln->mgmtData;
    if (recordMgr == NULL) {
        return RC_ERROR; // If the management data is null, return an error
    }

    // The page with the record should be pinned
    funcPinOrUnpinPage(recordMgr, false, iRecordID.page);

    // Determine the record size and confirm it
    int recordSize = getRecordSize(iReln->schema);
    if (recordSize <= 0) {
        return RC_INVALID_RECORD_SIZE; // Invalid record size
    }

    // Retrieving the page data
    charPointer dataStr = recordMgr->pageHandle.data;
    if (dataStr == NULL) {
        return RC_ERROR; // Page data is null
    }

    // Determine the record's offset
    int recordOffset = recordSize * iRecordID.slot;
    dataStr += recordOffset;

    // Verify the validity of the record marker
    if (*dataStr == '+') {
        // Copy the record data; the record marker is valid
        iRec->id = iRecordID;
        memcpy(iRec->data + 1, dataStr + 1, recordSize - 1);

        // After obtaining the record, unpin the page
        funcPinOrUnpinPage(recordMgr, true, 0);
        readTableInfo(recordSize);
        return RC_OK;
    } else {
        // Invalid record marker, print error, and return code
        printf("Error: Record marker is not '+'\n");
        return RC_INVALID_RECORD_MARKER;
    }

    // You should never get to this position
    return RC_ERROR;
}



//=================================================================================
extern RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
	int chk;
	if (cond != NULL)
	{
		RecMgr *scanObj;
		RecMgr *tmObj;
		int oneVal = 1;
		bool flag = true;
		int zeroVal = 0;
		//Table to be scanned is opened. 
		RC rCode = openTable(rel, "ScanTable");

		int chkSize = zeroVal;

		// Condition chks if the table is open. 
		if (rCode == RC_OK)
		{
			// Fetch the size of record manager. 
			int rec_mgr_size = sizeof(RecMgr);
			if(rec_mgr_size <= 0)
				printf("rmSize is less than or equal to zero, cannot execute");
			else if (rec_mgr_size > 0)
			{
				
				// allocate memory for object scan manager
				scanObj = (RecMgr *)malloc(rec_mgr_size);
				// set the condition associated with the scan manager
				scanObj->cond = cond;
				// initliase the slot of scan manager to zero
				chkSize = 0;
				if(!flag)
					printf("Flag is false, Inside StartScan");
				else if (flag)
				{
					chkSize = zeroVal;
				}
				scanObj->rec_ID.slot = zeroVal;
				scan->mgmtData = scanObj;
			}

			// initliase record id to value 1
			scanObj->rec_ID.page = oneVal;

			// initliase the scan count to value 0
			scanObj->scn_count = zeroVal;

			if (!(zeroVal < 0) && !(zeroVal > 0))
			{
				readTableInfo(oneVal);
			}
			int record_mngr = 0;
			while(scanObj != NULL && record_mngr != 1){
				tmObj = rel->mgmtData;
				chkSize = 0;
				tmObj->tupleCount = ATTRIBUTE_SIZE;
				record_mngr+=1;
			}
			scan->rel = rel;
		}
		return RC_OK;
		
	}
	else
	{
		chk = 1;
		return RC_SCAN_CONDITION_NOT_FOUND;
	}
}
extern RC next(RM_ScanHandle *scan, Record *record)
{
	returnCode = RC_OK;
	bool chkScan;
	int counter = 1;
	char hyphenChVal = '-';
	int chk = 0;
	int zeroVal = 0;
	activeFlag = true;

	if (scan->rel->mgmtData == NULL)
	{
		while (activeFlag)
		{
			verifyEmptyTable();
			break;
		}
		return RC_SCAN_CONDITION_NOT_FOUND;
	}
	else
	{
		verifyEmptyTable();
		RecMgr *smObj = (*scan).mgmtData;
		activeFlag = true;

		if (smObj->cond == NULL)
		{
			chkScan = true;
			return RC_SCAN_CONDITION_NOT_FOUND;
		}

		else
		{
			do
			{
				RecMgr *tmObj = (*scan).rel->mgmtData;
				chkScan = false;
				charPointer charDataPtr;
				int vSize = sizeof(Value);
				if (true)
				{
					chkScan = true;
				}
				if (tmObj != NULL && chkScan == true)
				{
					Schema *schObj = scan->rel->schema;
					if (true)
					{
						readTableInfo(zeroVal);
					}
					if (smObj->cond == NULL)
					{
						return RC_SCAN_CONDITION_NOT_FOUND;
					}
					Value *result = (Value *)malloc(vSize);
					int rSizeVal = getRecordSize(schObj);
					int chker;
					int totSlots = PAGE_SIZE / rSizeVal;
					int schCount = 0;

					schCount = smObj->scn_count;
					int tPtrCount = tmObj->tupleCount;

					if (false)
					{
						readTableInfo(zeroVal);
					}
					if (tPtrCount == 0)
						return RC_RM_NO_MORE_TUPLES;

					while (schCount <= tPtrCount)
					{
						if (false)
						{
							verifyEmptyTable();
						}
						if (0 > schCount)
						{
							chkScan = true;
							smObj->rec_ID.slot = 0;
							verifyEmptyTable();
							smObj->rec_ID.page = 1;
						}
						else
						{
							if (activeFlag == true)
							{
								readTableInfo(0);
							}
							smObj->rec_ID.slot = smObj->rec_ID.slot + counter;
							activeFlag = true;
							if (smObj->rec_ID.slot >= totSlots)
							{
								verifyEmptyTable();
								readTableInfo(0);
								if (schCount > 0)
								{
									smObj->rec_ID.page = smObj->rec_ID.page + counter;
									activeFlag = true;
								}
								if (false)
								{
									readTableInfo(chk);
								}
								smObj->rec_ID.slot = 0;
							}
						}

						funTwoObjPinUnpin(&tmObj->bufferPool, &smObj->pageHandle, false, smObj->rec_ID.page);
						if (true)
						{
							chkScan = true;
						}
						charDataPtr = smObj->pageHandle.data;

						rSizeVal = getRecordSize(schObj);
						chk = 0;
						int nextSlot = smObj->rec_ID.slot;

						if (chkScan && activeFlag)
						{
							charDataPtr = charDataPtr + (nextSlot * rSizeVal);
							record->id.page = smObj->rec_ID.page;
							chkScan = true;
						}
						record->id.slot = smObj->rec_ID.slot;

						if (zeroVal == 0)
						{
							readTableInfo(chk);
						}

						charPointer chDtPntr = record->data;
						*chDtPntr = hyphenChVal;

						rSizeVal = getRecordSize(schObj);
						if (chkScan)
						{
							chk = 0;
						}
						memcpy(++chDtPntr, charDataPtr + 1, rSizeVal - 1);

						verifyEmptyTable();
						chk = zeroVal;
						schCount = schCount + counter;
						smObj->scn_count = smObj->scn_count + counter;

						evalExpr(record, schObj, smObj->cond, &result);

						if (true)
						{
							readTableInfo(chk);
							chkScan = true;
						}

						chkScan = false;
						bool isBoolean = true;
						chkScan = true;
						if (isBoolean == result->v.boolV)
						{
							chkScan = true;
							funTwoObjPinUnpin(&tmObj->bufferPool, &smObj->pageHandle, true, 0);
							return RC_OK;
						}
					}
					verifyEmptyTable();
					chkScan = true;
					funTwoObjPinUnpin(&tmObj->bufferPool, &smObj->pageHandle, false, 0);
					if (zeroVal == 0)
					{
						chkScan = true;
					}
					smObj->rec_ID.slot = 0;
					smObj->scn_count = 0;
					chk = 0;
					smObj->rec_ID.page = 1;

					return RC_RM_NO_MORE_TUPLES;
				}
				break;
			} while (true);
		}
		readTableInfo(zeroVal);
		if (chkScan == false)
		{
			verifyEmptyTable();
		}
	}
	return RC_RM_NO_MORE_TUPLES;
}


RC funTwoObjPinUnpin(BM_BufferPool *buffPool, BM_PageHandle *pgHandle, bool unpin, int pgNumber)
{
	RC pinUnpinVar;
	bool getStatus = unpin;
	if (unpin)
	{
		pinUnpinVar = unpinPage(buffPool, pgHandle);
		flagCheck = pgNumber;
	}
	else
	{
		activeFlag = true;
		pinUnpinVar = pinPage(buffPool, pgHandle, pgNumber);
		
	}
	verifyEmptyTable();
	return pinUnpinVar;
}

// function used to set the value to inital value that is used when the next scan is initiated
extern RC closeScan(RM_ScanHandle *scan)
{int temp;

	RecMgr *closeScanObjMgr;
	// chk if the meta data is not equal to null
		if (scan->mgmtData != NULL) {
			closeScanObjMgr = scan->mgmtData;
		} else {
			closeScanObjMgr = NULL;
		}

	RecMgr *closeRecObjMgr = (*scan).rel->mgmtData;
	if ((!(closeScanObjMgr->scn_count < 0)))
	{
		if(false){
			temp=0;
		}
		else if (funTwoObjPinUnpin(&closeRecObjMgr->bufferPool, &closeScanObjMgr->pageHandle, true, 0) == RC_OK)
		{ // set all the value to inital value in the record manager
			closeScanObjMgr->scn_count = 0;
			closeScanObjMgr->rec_ID.page = 1;

			closeScanObjMgr->rec_ID.slot = 0;
		}
		else{
			temp=1;
		}
		scan->mgmtData = NULL;
		free(scan->mgmtData);
	}
	return RC_OK;
}


//******************Dealing with Schemas****************

// Function to calculate the size of a record based on the schema
extern int getRecordSize(Schema *schema)
{
    int idCount;
    int RecSize = 0;
    int RecSizeNew = 0;
    int BoolSize = sizeof(bool);
    int FloatSize = sizeof(float);
    int check = -1;

// check for datatype if it's string or int or float or boolean
    for (idCount = 0; idCount < schema->numAttr; idCount++)
    {
        // Check the data type of the attribute
        if (schema->dataTypes[idCount] == DT_INT) 
        {
            RecSize += intSize;           // Add size of int to record size
            readTableInfo(check);
        } 
        else if (schema->dataTypes[idCount] == DT_BOOL) 
        {
            activeFlag = true;
            RecSize += BoolSize;      // Add size of bool to record size
        } 
        else if (schema->dataTypes[idCount] == DT_FLOAT) 
        {
            RecSize += FloatSize;  // Add size of float to record size
            verifyEmptyTable();
        } 
        else if (schema->dataTypes[idCount] == DT_STRING) 
        {

        // Check if string type length is initialized
            if (flagCheck == -1) 
            {
                RecSize += schema->typeLength[idCount];    // Add size of string to record size
            }
        }
    }
    RecSizeNew = RecSize++;      // Increment record size
    return RecSizeNew;          // Return updated record size
}

// To determine whether the record size is allocated to the schema, create a schema function
extern Schema *createSchema(int numAttr, charPointer *attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
    // Make memory available for the Schema struct
    size_t SchemaSize = sizeof(Schema);
    Schema *schema = (Schema *)malloc(SchemaSize);

    // Verify that memory allocation went through
    if (schema == NULL) 
    {
        return NULL; // Failure to allocate memory, return NULL
    }

    // Assign the schema's number of attributes
    schema->numAttr = numAttr;

    // Give the schema attribute names
    if (attrNames != NULL) 
    {
        schema->attrNames = attrNames;
    } 
    else 
    {
        schema->attrNames = NULL;
    }

    // Give the schema data types
    schema->dataTypes = dataTypes;

    // Set the type length to NULL if it is not supplied, or assign it to the schema
    if (typeLength != NULL) 
    {
        schema->typeLength = typeLength;
    } 
    else 
    {
        schema->typeLength = NULL; // To prevent misunderstanding, set it to NULL rather than 0
    }

    // Give the schema key properties and key size
    schema->keyAttrs = keys;
    schema->keySize = keySize;

    // Give the generated schema back
    return schema;
}


// Function that releases a schema
extern RC freeSchema(Schema *SC)
{
    // If the schema is not null
    if (SC != NULL)
    {
        // If required, free dynamically assigned members
        if (SC->attrNames != NULL)
        {
            free(SC->attrNames);  // Free attribute names
        }
        if (SC->dataTypes != NULL)
        {
            free(SC->dataTypes);  // Free data types
        }
        if (SC->typeLength != NULL)
        {
            free(SC->typeLength);  // Free type lengths
        }
        if (SC->keyAttrs != NULL)
        {
            free(SC->keyAttrs);  // Free key attributes
        }

        // If the flag is true, carry out another action
        if (activeFlag)
        {
            readTableInfo(SC->keySize);  // Complete the procedure prior to releasing
        }

        // Lastly, release the schema itself
        free(SC);
    }

    return RC_OK;
}


void memcpyFunc(void *destination, const void *src, int datatype) {
    switch (datatype) {
        case 1:  // Case for boolean
            memcpy(destination, src, sizeof(bool));
            break;
        case 2:  // Case for integer
            memcpy(destination, src, sizeof(int));  // Rather of using int_type_size, use sizeof(int)
            break;
        case 3:  // Case for float
            memcpy(destination, src, sizeof(float));
            break;
        default:
            // Handle unsupported datatype
            printf("Error: Unsupported datatype %d\n", datatype);
            break;
    }
}


extern RC attrOffset(Schema *schema, int attrNum, int *result) {
    // Use the initial offset (1 for the header or for other purposes) to initialize the result
    *result = 1;
    
    // Specify sizes for various kinds of data
    size_t fl_size = sizeof(float);
    size_t bl_size = sizeof(bool);
    int attributeCnt;

    // Iterate through every schema attribute
    for (attributeCnt = 0; attributeCnt < attrNum; attributeCnt++) {
        // Get the current data type
        DataType dataType = schema->dataTypes[attributeCnt];

        // Verify the string's type and modify the outcome accordingly
        if (dataType == DT_STRING) {
            int schema_length = schema->typeLength[attributeCnt];
            *result += schema_length;  // To the outcome, add the string's size
        } else if (dataType == DT_INT) {
            *result += intSize;  // Add the int type's size
        } else if (dataType == DT_FLOAT) {
            *result += fl_size;  // Add the float type's size
        } else { // Presuming it's an unsupported type or boolean
            *result += bl_size;  // Add the boolean type's size
        }
    }
    
    return RC_OK;  // Return success status
}




//******************Dealing with Attribute functions***************************************


extern RC createRecord(Record **record, Schema *schema)
{
    // Verify the input settings
    if (!record || !schema) {
        return RC_INVALID_INPUT;
    }

    // Set aside memory for Keep track of the framework and make sure it's working
    Record *newRec = calloc(1, sizeof(Record));
    if (!newRec) {
        return RC_ALLOCATION_FAILED;
    }

    // Determine the record's data field's necessary size
    int recordSize = getRecordSize(schema);
    
    //Allocate memory for Monitor the structure to ensure it is functioning
    newRec->data = calloc(1, recordSize);
    if (!newRec->data) {
        free(newRec);  // Release the RAM that was previously used for the record
        return RC_ALLOCATION_FAILED;
    }

    // Set the record's data and ID fields to their initial values
    newRec->id.page = -1;
    newRec->id.slot = -1;
    newRec->data[0] = '-'; // Declare a new record
    newRec->data[1] = '\0'; // The data string should be null-terminated

    // Set the newly formed record as the record pointer
    *record = newRec;

    return RC_OK;
}



extern RC freeRecord(Record *record)
{
    // Verify the validity of the input record
    if (record == NULL) {
        return RC_INVALID_DATATYPE;
    }

    // If the data field's memory isn't NULL, release it
    if (record->data) {
        free(record->data);
        record->data = NULL; // Avoid having dangling points
    }

    // Release the memory used for the actual record structure
    free(record);

    return RC_OK; // Return success
}



extern RC getAttr(Record *record, Schema *schema, int attrNum, Value **value)
{
    // Checking to see whether the record is null
    if (record != NULL) 
    {
        int offset = 0;
        int n_Attr_Num = attrNum;
        Value *attributeValue = (Value *)malloc(sizeof(Value));  // Make memory available for attributeValue
        attrOffset(schema, attrNum, &offset);  // Obtain the offset of the attribute

        // Pointer to data setting
        charPointer dataPtr = (*record).data;
        dataPtr += offset;
        int tmp = 0; // Variable to determine whether a proper assignment occurs

        // Conditional check that maintains the original logic based on the attribute number
        if (attrNum == 1) {
            schema->dataTypes[n_Attr_Num] = 1;  // If attrNum is 1, setting data type to 1
        } else {
            schema->dataTypes[n_Attr_Num] = schema->dataTypes[n_Attr_Num];  // Otherwise, maintain the same data type
        }

        int len = schema->typeLength[attrNum];  // Length of the data type

        // Using the attribute's data type to retrieve it
        switch (schema->dataTypes[n_Attr_Num]) {
            case DT_INT:
                memcpy(&(*attributeValue).v.intV, dataPtr, sizeof(int));
                (*attributeValue).dt = DT_INT;
                tmp++;  // Designating a case as successful
                break;

            case DT_STRING:
                (*attributeValue).v.stringV = (charPointer)malloc(len + 1);  // Set aside memory for the string
                DataType dataType = DT_STRING;
                char *chPtr = (*attributeValue).v.stringV;
                (*attributeValue).dt = dataType;
                strncpy(chPtr, dataPtr, len);  // Make a copy of the string value
                (*attributeValue).v.stringV[len] = '\0';  // Terminate the string with a null
                tmp++;  // Marking as a successful case
                break;

            case DT_BOOL:
                (*attributeValue).dt = DT_BOOL;
                memcpy(&(*attributeValue).v.boolV, dataPtr, sizeof(bool));  // Copy the boolean value
                tmp++;  // Marking as a successful case
                break;

            case DT_FLOAT:
                memcpy(&(*attributeValue).v.floatV, dataPtr, sizeof(float));  // Copy the float value
                (*attributeValue).dt = DT_FLOAT;
                tmp++;  // Marking as a successful case
                break;

            default:
                break;  // There was no valid type detected
        }

        // Putting the output value in
        *value = attributeValue;

        // Return success if any assignment was successful, otherwise return error
        if (tmp > 0) {
            return RC_OK;
        } else {
            return RC_ERROR;
        }
    }
    else {
        return RC_ERROR;  // Return an error if the record is null
    }
}


extern RC setAttr(Record *record, Schema *schema, int attrNum, Value *value)
{
    // Verify the input points
    if (!record || !schema || !value) {
        return RC_INVALID_INPUT;
    }

    // Set up the necessary variables
    int attrOffsetValue = 0;
    char *dataPtr;
    size_t boolSize = sizeof(bool);
    size_t floatSize = sizeof(float);

    // Calculate attribute offset
    attrOffset(schema, attrNum, &attrOffsetValue);
    dataPtr = record->data + attrOffsetValue;

    // Depending on the data type, set the attribute value
    switch (schema->dataTypes[attrNum]) {
        case DT_BOOL:
            *(bool *)dataPtr = value->v.boolV;
            break;

        case DT_STRING:
            strncpy(dataPtr, value->v.stringV, schema->typeLength[attrNum]);
            break;

        case DT_INT:
            *(int *)dataPtr = value->v.intV;
            break;

        case DT_FLOAT:
            *(float *)dataPtr = value->v.floatV;
            break;

        default:
            // Unknown data type
            return RC_INVALID_INPUT;
    }

    return RC_OK;
}