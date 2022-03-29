CC = clang
CFLAGS = -O0 -g -Wall -Wextra -std=c99 -pedantic-errors -isystem /usr/local/llvm-10/lib/clang/10.0.1/include/

all: test

.PHONY: test
test: mini.c
	$(CC) $(CFLAGS) -o $@ mini.c

clean:
	rm -f mini *.o *.ll


BITCODE_C = /usr/local/llvm-10/bin/clang -emit-llvm -S -g -DHAVE_CONFIG_H
llvm_link = /usr/local/llvm-10/bin/llvm-link


BITCODES=mini.ll

%.ll: %.c
	$(BITCODE_C) $(DEFAULT_INCLUDES) $< -o $@

bitcode: $(BITCODES)
	$(llvm_link) $(BITCODES) -o mini_linked.ll -S
	sed 's/\ optnone\>//g' mini_linked.ll > mini_opt.ll
	opt -mem2reg -S -o opt.ll mini_opt.ll
