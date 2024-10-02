CFLAGS  = -g -Wall
LDFLAGS = -pthread
 
all: test1 test2

run: run_test1 run_test2

test1: test_assign2_1.o storage_mgr.o dberror.o buffer_mgr.o buffer_mgr_stat.o
	gcc $(CFLAGS) -o test1 test_assign2_1.o storage_mgr.o dberror.o buffer_mgr.o buffer_mgr_stat.o -lm $(LDFLAGS)

test2: test_assign2_2.o storage_mgr.o dberror.o buffer_mgr.o buffer_mgr_stat.o
	gcc $(CFLAGS) -o test2 test_assign2_2.o storage_mgr.o dberror.o buffer_mgr.o buffer_mgr_stat.o -lm $(LDFLAGS)

test_assign2_1.o: test_assign2_1.c dberror.h storage_mgr.h test_helper.h buffer_mgr.h buffer_mgr_stat.h
	gcc $(CFLAGS) -c test_assign2_1.c -lm

test_assign2_2.o: test_assign2_2.c dberror.h storage_mgr.h test_helper.h buffer_mgr.h buffer_mgr_stat.h
	gcc $(CFLAGS) -c test_assign2_2.c -lm

buffer_mgr_stat.o: buffer_mgr_stat.c buffer_mgr_stat.h buffer_mgr.h
	gcc $(CFLAGS) -c buffer_mgr_stat.c

buffer_mgr.o: buffer_mgr.c buffer_mgr.h dt.h storage_mgr.h
	gcc $(CFLAGS) -c buffer_mgr.c -pthread

storage_mgr.o: storage_mgr.c storage_mgr.h 
	gcc $(CFLAGS) -c storage_mgr.c -lm

dberror.o: dberror.c dberror.h 
	gcc $(CFLAGS) -c dberror.c

clean: 
	rm test1 test2 *.o

run_test1:
	./test1

run_test2:
	./test2
