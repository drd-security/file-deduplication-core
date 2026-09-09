CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L

all: test_filededup

test_filededup: test_filededup.c ../src/filededup.c ../src/filededup.h
	$(CC) $(CFLAGS) -I../src test_filededup.c ../src/filededup.c -o $@

run: test_filededup
	./test_filededup

clean:
	rm -f test_filededup