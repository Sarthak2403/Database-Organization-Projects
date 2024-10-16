#ifndef DBERROR_H
#define DBERROR_H

#include "stdio.h"

/* module wide constants */
#define PAGE_SIZE 4096

/* return code definitions */
typedef int RC;

#define RC_OK 0
#define RC_FILE_NOT_FOUND 1
#define RC_FILE_HANDLE_NOT_INIT 2
#define RC_WRITE_FAILED 3
#define RC_READ_NON_EXISTING_PAGE 4
#define RC_ERROR 5
#define RC_READ_FAILED 6
#define RC_NULL_POINTER 7
#define RC_BUFFER_POOL_HAS_PINNED_PAGES 8

#define RC_RM_COMPARE_VALUE_OF_DIFFERENT_DATATYPE 200
#define RC_RM_EXPR_RESULT_IS_NOT_BOOLEAN 201
#define RC_RM_BOOLEAN_EXPR_ARG_IS_NOT_BOOLEAN 202
#define RC_RM_NO_MORE_TUPLES 203
#define RC_RM_NO_PRINT_FOR_DATATYPE 204
#define RC_RM_UNKOWN_DATATYPE 205


#define RC_INVALID_RECORD_SIZE 206
#define RC_INVALID_RECORD_MARKER 207
#define RC_SCAN_CONDITION_NOT_FOUND 208
#define RC_INVALID_INPUT 209
#define RC_ALLOCATION_FAILED 210
#define RC_INVALID_DATATYPE 211
#define RC_PINNED_PAGES_IN_BUFFER 212
#define RC_BUFFER_ERROR 213

#define RC_TM_ABORT_TRANSACTION 300
#define RC_TM_NO_ACTIVE_TRANSACTION 301
#define RC_TM_TRANSACTION_NOT_ACTIVE 302
#define RC_TM_INCORRECT_TRAILER_CHECKSUM 303
#define RC_FILE_CLOSE_ERROR 304
#define RC_INVALID_BLOCK_POSITION 305

#define RC_IM_KEY_NOT_FOUND 306
#define RC_IM_KEY_ALREADY_EXISTS 307
#define RC_IM_N_TO_LAGE 308
#define RC_IM_NO_MORE_ENTRIES 309
#define RC_MEMORY_ALLOCATION_FAIL 310

/* holder for error messages */
extern char *RC_message;

/* print a message to standard out describing the error */
extern void printError (RC error);
extern char *errorMessage (RC error);

#define THROW(rc,message) \
		do {			  \
			RC_message=message;	  \
			return rc;		  \
		} while (0)		  \

// check the return code and exit if it is an error
#define CHECK(code)							\
		do {									\
			int rc_internal = (code);						\
			if (rc_internal != RC_OK)						\
			{									\
				char *message = errorMessage(rc_internal);			\
				printf("[%s-L%i-%s] ERROR: Operation returned error: %s\n",__FILE__, __LINE__, __TIME__, message); \
				free(message);							\
				exit(1);							\
			}									\
		} while(0);


#endif
