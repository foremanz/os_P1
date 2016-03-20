all: mem_mgr

mem_mgr: main.c mem_pool.c mem_pool.h test_suite.c test_suite.h cmocka.h
	gcc -g -std=c11 *.c -L/usr/lib -lcmocka -o mem_mgr 
