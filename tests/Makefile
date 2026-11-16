CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

test: test_date_math
	./test_date_math

test_date_math: test_date_math.c ../date_math.c ../date_math.h
	$(CC) $(CFLAGS) -o $@ test_date_math.c ../date_math.c

clean:
	rm -f test_date_math

.PHONY: test clean
