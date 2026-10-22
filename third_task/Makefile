CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread

.PHONY: all clean benchmark
all: list_pthread list_custom

list_pthread: list_benchmark.c my_rand.c my_rand.h timer.h
	$(CC) $(CFLAGS) list_benchmark.c my_rand.c -o $@

list_custom: list_benchmark.c my_rwlock.c my_rwlock.h my_rand.c my_rand.h timer.h
	$(CC) $(CFLAGS) -DUSE_MY_RWLOCK list_benchmark.c my_rwlock.c my_rand.c -o $@

benchmark: all
	python3 benchmark.py

clean:
	rm -f list_pthread list_custom results.csv
