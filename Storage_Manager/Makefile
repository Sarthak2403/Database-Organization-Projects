# Example Makefile
CC = gcc
CFLAGS = -g

all: test_assign1_1

test_assign1_1: dberror.o storage_mgr.o test_assign1_1.o
	$(CC) $(CFLAGS) -o test_assign1_1 dberror.o storage_mgr.o test_assign1_1.o

dberror.o: dberror.c dberror.h
	$(CC) $(CFLAGS) -c dberror.c

storage_mgr.o: storage_mgr.c storage_mgr.h
	$(CC) $(CFLAGS) -c storage_mgr.c

test_assign1_1.o: test_assign1_1.c storage_mgr.h
	$(CC) $(CFLAGS) -c test_assign1_1.c

clean:
	rm -f *.o test_assign1_1