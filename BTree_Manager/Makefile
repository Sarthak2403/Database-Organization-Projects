
OFILES = btree_mgr.o rm_serializer.o record_mgr.o dberror.o storage_mgr.o buffer_mgr.o expr.o

all: output1

run: run_output1

output1: test_assign4_1.o $(OFILES) 
	gcc -o output1 test_assign4_1.o $(OFILES) -lm -I.

test_expr.o: test_expr.c
	gcc -c test_expr.c

test_assign4_1.o: test_assign4_1.c
	gcc -c test_assign4_1.c

rm_serializer.o: rm_serializer.c
	gcc -c rm_serializer.c

record_mgr.o: record_mgr.c
	gcc -c record_mgr.c

expr.o: expr.c
	gcc -c expr.c

storage_mgr.o: storage_mgr.c
	gcc -c storage_mgr.c

buffer_mgr_stat.o: buffer_mgr_stat.c
	gcc -c buffer_mgr_stat.c

buffer_mgr.o: buffer_mgr.c
	gcc -c buffer_mgr.c

btree_mgr.o: btree_mgr.c
	gcc -c btree_mgr.c

dberror.o: dberror.c
	gcc -c dberror.c

clean:	
	rm -rf *.o output1

run_output1:
	./output1