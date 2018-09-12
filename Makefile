default: none

CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic -Werror

MD=mkdir -p
RM=rm -rf

.PHONY: directories clean test

directories: tests

tests:
	$(MD) tests

tests/rbtree_test : directories rbtree_test.c rbtree.c util.c rbtree.h util.h
	$(CC) $(CFLAGS) -o tests/rbtree_test rbtree_test.c rbtree.c util.c

test: tests/rbtree_test
	for test in tests/*; do \
		"$$test"; \
	done

clean:
	$(RM) tests
